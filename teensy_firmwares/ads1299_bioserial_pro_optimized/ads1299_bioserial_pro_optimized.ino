/*
 * =============================================================================
 * BioSerial-Pro EEG OPTIMIZED Firmware for Teensy 4.1
 * =============================================================================
 * 
 * UPGRADED VERSION of ads1299_bioserial_pro.ino
 * 
 * Improvements over original:
 * - Variable-length packets (26-55 bytes vs fixed 38 bytes)
 * - Multi-rate sampling (Accel@250Hz, PPG@100Hz, Health@10Hz vs all@1kHz)
 * - Enumerated packet types for efficient parsing
 * - Sequence numbers for gap detection
 * - Same simulation mode for testing without hardware
 * 
 * Aux Channels (same as original, different transmission rate):
 *   - AccelX, AccelY, AccelZ    @ 250 Hz
 *   - PPG Red, PPG IR, PPG Green @ 100 Hz  
 *   - Temperature, Battery       @ 10 Hz
 * 
 * =============================================================================
 */

extern "C" uint32_t set_arm_clock(uint32_t frequency);

#include "OptimizedProtocol.h"

// =============================================================================
// Hardware Pins
// =============================================================================

#define MARKER_PIN      2
#define LED_STATUS      13

// =============================================================================
// Ring Buffer for Decoupled ISR/Transmission
// =============================================================================

#define RING_BUFFER_PACKETS     64

class PacketRingBuffer {
public:
    uint8_t buffer[RING_BUFFER_PACKETS][MAX_PACKET_SIZE];
    uint8_t packetSizes[RING_BUFFER_PACKETS];
    volatile uint16_t head = 0;
    volatile uint16_t tail = 0;
    volatile uint16_t count = 0;
    
    bool write(const uint8_t* packet, uint8_t size) {
        if (count >= RING_BUFFER_PACKETS) {
            return false;
        }
        
        for (int i = 0; i < size; i++) {
            buffer[head][i] = packet[i];
        }
        packetSizes[head] = size;
        
        head = (head + 1) % RING_BUFFER_PACKETS;
        __disable_irq();
        count++;
        __enable_irq();
        
        return true;
    }
    
    bool read(uint8_t* packet, uint8_t* size) {
        if (count == 0) {
            return false;
        }
        
        *size = packetSizes[tail];
        for (int i = 0; i < *size; i++) {
            packet[i] = buffer[tail][i];
        }
        
        tail = (tail + 1) % RING_BUFFER_PACKETS;
        __disable_irq();
        count--;
        __enable_irq();
        
        return true;
    }
    
    uint16_t available() const { return count; }
    void clear() { head = tail = count = 0; }
};

PacketRingBuffer packetBuffer;

// =============================================================================
// Global State Variables
// =============================================================================

// Sensor data storage
SensorData sensorData;

// Counters
volatile uint32_t sampleNumber = 0;
volatile uint8_t sequenceNumber = 0;
volatile uint32_t droppedPackets = 0;

// Timing
volatile uint32_t lastSampleTimestamp = 0;

// Marker state
volatile bool markerPending = false;

// Simulation counter
volatile uint32_t simSampleCount = 0;

// Timer for 1kHz sampling
IntervalTimer sampleTimer;

// =============================================================================
// Forward Declarations
// =============================================================================

void sampleISR();
void generateSimulatedData();
int buildPacket(uint8_t* packet, PacketType pktType, uint32_t timestamp);
uint8_t computeChecksum(const uint8_t* data, int len);

// =============================================================================
// Interrupt Service Routine - 1kHz Sample Timer
// =============================================================================

void sampleISR() {
    // Capture timestamp
    lastSampleTimestamp = micros();
    
    // Generate simulated data
    generateSimulatedData();
    
    // Read marker button
    bool hasMarkerEvent = !digitalRead(MARKER_PIN);  // Active low
    if (hasMarkerEvent) {
        sensorData.marker = 0x01;
        markerPending = true;
    }
    
    // Determine packet type based on sample number
    PacketType pktType = getPacketType(sampleNumber, markerPending);
    
    // Build and queue packet
    static uint8_t packet[MAX_PACKET_SIZE];
    int packetSize = buildPacket(packet, pktType, lastSampleTimestamp);
    
    if (packetSize > 0) {
        if (!packetBuffer.write(packet, packetSize)) {
            droppedPackets++;
        }
    }
    
    // Clear marker after sending
    if (markerPending) {
        sensorData.marker = 0;
        markerPending = false;
    }
    
    // Increment counters
    sampleNumber++;
    simSampleCount++;
    sequenceNumber++;
}

// =============================================================================
// Generate Simulated Data - Same as Original but with PPG expansion
// =============================================================================

void generateSimulatedData() {
    const float sampleRate = 1000.0f;
    float t = (float)simSampleCount;
    
    // =========================================================================
    // EEG Channels (5 channels) - Same as original
    // =========================================================================
    
    sensorData.eeg[0] = (int32_t)(100000.0f * sinf(2.0f * PI * 3.0f * t / sampleRate));
    sensorData.eeg[1] = (int32_t)(100000.0f * sinf(2.0f * PI * 5.0f * t / sampleRate));
    sensorData.eeg[2] = (int32_t)(100000.0f * sinf(2.0f * PI * 10.0f * t / sampleRate));
    sensorData.eeg[3] = (int32_t)(100000.0f * sinf(2.0f * PI * 15.0f * t / sampleRate));
    sensorData.eeg[4] = (int32_t)(100000.0f * sinf(2.0f * PI * 20.0f * t / sampleRate));
    
    // =========================================================================
    // Accelerometer (3 channels) - Same as original AccelX/Y/Z
    // =========================================================================
    
    sensorData.accel[0] = (int16_t)(1000.0f * sinf(2.0f * PI * 0.5f * t / sampleRate));
    sensorData.accel[1] = (int16_t)(1000.0f * sinf(2.0f * PI * 0.7f * t / sampleRate));
    sensorData.accel[2] = (int16_t)(256 + 500.0f * sinf(2.0f * PI * 0.3f * t / sampleRate));
    
    // =========================================================================
    // PPG (3 channels) - Expanded from original single PPG
    // Simulated heartbeat ~72 BPM
    // =========================================================================
    
    float heartPhase = 2.0f * PI * 1.2f * t / sampleRate;
    float pulse = powf(sinf(heartPhase), 4);  // Sharper pulse shape
    
    sensorData.ppg[0] = (int64_t)(100000 + pulse * 5000);   // Red
    sensorData.ppg[1] = (int64_t)(120000 + pulse * 6000);   // IR
    sensorData.ppg[2] = (int64_t)(80000 + pulse * 4000);    // Green
    
    // =========================================================================
    // Health Data - Same as original Temp/Battery
    // =========================================================================
    
    // Temperature: 36.5°C with slow drift
    sensorData.temperature = (int16_t)(3650 + 50.0f * sinf(2.0f * PI * 0.05f * t / sampleRate));
    
    // Battery: ~4.0V with slow variation
    sensorData.battery = (int16_t)(4000 + 200.0f * sinf(2.0f * PI * 0.02f * t / sampleRate));
}

// =============================================================================
// Build Variable-Length Packet
// =============================================================================

int buildPacket(uint8_t* packet, PacketType pktType, uint32_t timestamp) {
    int pos = 0;
    
    // Header
    packet[pos++] = SYNC_BYTE_1;
    packet[pos++] = SYNC_BYTE_2;
    packet[pos++] = (uint8_t)pktType;
    packet[pos++] = sequenceNumber;
    
    // Timestamp (big-endian)
    packet[pos++] = (timestamp >> 24) & 0xFF;
    packet[pos++] = (timestamp >> 16) & 0xFF;
    packet[pos++] = (timestamp >> 8) & 0xFF;
    packet[pos++] = timestamp & 0xFF;
    
    // EEG data (always present) - 5 channels × 3 bytes
    for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++) {
        int32_t val = sensorData.eeg[ch];
        packet[pos++] = (val >> 16) & 0xFF;
        packet[pos++] = (val >> 8) & 0xFF;
        packet[pos++] = val & 0xFF;
    }
    
    // Marker (if flag set)
    if (hasMarker(pktType)) {
        packet[pos++] = sensorData.marker;
    }
    
    // Accelerometer (if included)
    if (hasAccel(pktType)) {
        for (int ch = 0; ch < NUM_ACCEL_CHANNELS; ch++) {
            int16_t val = sensorData.accel[ch];
            packet[pos++] = (val >> 8) & 0xFF;
            packet[pos++] = val & 0xFF;
        }
    }
    
    // PPG (if included) - 3 channels × 6 bytes (48-bit)
    if (hasPPG(pktType)) {
        for (int ch = 0; ch < NUM_PPG_CHANNELS; ch++) {
            int64_t val = sensorData.ppg[ch];
            packet[pos++] = (val >> 40) & 0xFF;
            packet[pos++] = (val >> 32) & 0xFF;
            packet[pos++] = (val >> 24) & 0xFF;
            packet[pos++] = (val >> 16) & 0xFF;
            packet[pos++] = (val >> 8) & 0xFF;
            packet[pos++] = val & 0xFF;
        }
    }
    
    // Health data (if included)
    if (hasHealth(pktType)) {
        packet[pos++] = (sensorData.temperature >> 8) & 0xFF;
        packet[pos++] = sensorData.temperature & 0xFF;
        packet[pos++] = (sensorData.battery >> 8) & 0xFF;
        packet[pos++] = sensorData.battery & 0xFF;
    }
    
    // Checksum (XOR of all bytes so far)
    packet[pos] = computeChecksum(packet, pos);
    pos++;
    
    // Footer
    packet[pos++] = FOOTER_BYTE_1;
    packet[pos++] = FOOTER_BYTE_2;
    
    return pos;
}

uint8_t computeChecksum(const uint8_t* data, int len) {
    uint8_t checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    // Set CPU clock for low jitter
    set_arm_clock(151200000);  // 151.2 MHz
    
    // Initialize Serial (USB) at optimized baud rate
    Serial.begin(SERIAL_BAUD_RATE);
    
    // Wait for USB connection (with timeout)
    uint32_t startWait = millis();
    while (!Serial && (millis() - startWait < 3000)) {
        // Wait up to 3 seconds for USB
    }
    
    // Configure pins
    pinMode(MARKER_PIN, INPUT_PULLUP);
    pinMode(LED_STATUS, OUTPUT);
    
    // LED on during init
    digitalWrite(LED_STATUS, HIGH);
    
    // Print startup message
    Serial.println("BioSerial-Pro OPTIMIZED Firmware");
    Serial.println("=================================");
    Serial.println("Variable-length packets with multi-rate sampling");
    Serial.print("EEG: 1000 Hz | Accel: 250 Hz | PPG: 100 Hz | Health: 10 Hz");
    Serial.println();
    Serial.print("Baud rate: ");
    Serial.println(SERIAL_BAUD_RATE);
    Serial.println("Starting in 1 second...");
    
    delay(1000);
    
    // Start 1kHz sample timer
    sampleTimer.begin(sampleISR, 1000);  // 1000µs = 1kHz
    
    Serial.println("Streaming started!");
    
    // LED off - will blink in loop
    digitalWrite(LED_STATUS, LOW);
}

// =============================================================================
// Main Loop - Asynchronous USB Transmission
// =============================================================================

void loop() {
    static uint8_t txPacket[MAX_PACKET_SIZE];
    static uint8_t txSize;
    static uint32_t lastStatusTime = 0;
    static uint32_t txCount = 0;
    
    // Send all available packets from ring buffer
    while (packetBuffer.available() > 0) {
        if (packetBuffer.read(txPacket, &txSize)) {
            // Write packet to USB
            Serial.write(txPacket, txSize);
            Serial.send_now();  // Force immediate transmission
            txCount++;
        }
    }
    
    // Status LED blink every second
    uint32_t now = millis();
    if (now - lastStatusTime >= 1000) {
        lastStatusTime = now;
        digitalToggle(LED_STATUS);
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

void digitalToggle(int pin) {
    digitalWrite(pin, !digitalRead(pin));
}
