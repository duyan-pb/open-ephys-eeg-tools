/*
 * =============================================================================
 * BioSerial-Pro EEG Simulation Firmware for Teensy 4.1
 * =============================================================================
 * 
 * SIMULATION MODE ONLY - No hardware required
 * Generates synthetic EEG-like signals for testing Open Ephys plugin
 * 
 * Protocol: BioSerial-Pro (Fixed 38-byte packets @ 1kHz)
 * 
 * Packet Structure (38 bytes):
 *   Header    (2B): 0xA5 0x5A (Sync sequence)
 *   Timestamp (4B): Microsecond timer (uint32_t, wraps every ~71 minutes)
 *   Marker    (1B): Hardware event triggers (button presses, etc.)
 *   EEG       (15B): 5 channels × 24-bit Big Endian
 *   Aux       (12B): 6 channels × 16-bit:
 *                    [0] AccelX, [1] AccelY, [2] AccelZ,
 *                    [3] PPG, [4] Temperature, [5] Battery
 *   Counter   (1B): Packet sequence number (0-255)
 *   Checksum  (1B): XOR of bytes 0-35
 *   Footer    (2B): 0xC0 0xC0
 * 
 * =============================================================================
 */

extern "C" uint32_t set_arm_clock(uint32_t frequency);

// =============================================================================
// BioSerial-Pro Protocol Constants
// =============================================================================

#define BIOSERIAL_HEADER_1      0xA5
#define BIOSERIAL_HEADER_2      0x5A
#define BIOSERIAL_FOOTER_1      0xC0
#define BIOSERIAL_FOOTER_2      0xC0
#define BIOSERIAL_PACKET_SIZE   38   // 2+4+1+15+12+1+1+2 = 38 bytes

// EEG Configuration
#define NUM_EEG_CHANNELS        5
#define NUM_AUX_CHANNELS        6    // AccelX, AccelY, AccelZ, PPG, Temp, Battery
#define BYTES_PER_EEG_SAMPLE    3    // 24-bit
#define BYTES_PER_AUX_SAMPLE    2    // 16-bit

// Sampling rate
#define EEG_SAMPLE_RATE_HZ      1000

// =============================================================================
// Ring Buffer for Decoupled ISR/Transmission
// =============================================================================

#define RING_BUFFER_PACKETS     64    // ~64ms of data at 1kHz
#define RING_BUFFER_SIZE        (RING_BUFFER_PACKETS * BIOSERIAL_PACKET_SIZE)

class PacketRingBuffer {
public:
    volatile uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head = 0;
    volatile uint16_t tail = 0;
    volatile uint16_t count = 0;
    
    bool write(const uint8_t* packet) {
        if (count >= RING_BUFFER_PACKETS) {
            return false;
        }
        
        uint16_t writePos = head * BIOSERIAL_PACKET_SIZE;
        for (int i = 0; i < BIOSERIAL_PACKET_SIZE; i++) {
            buffer[writePos + i] = packet[i];
        }
        
        head = (head + 1) % RING_BUFFER_PACKETS;
        __disable_irq();
        count++;
        __enable_irq();
        
        return true;
    }
    
    bool read(uint8_t* packet) {
        if (count == 0) {
            return false;
        }
        
        uint16_t readPos = tail * BIOSERIAL_PACKET_SIZE;
        for (int i = 0; i < BIOSERIAL_PACKET_SIZE; i++) {
            packet[i] = buffer[readPos + i];
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

// Data storage
volatile int32_t eegSamples[8];
volatile int16_t auxSamples[6];  // AccelX, AccelY, AccelZ, PPG, Temp, Battery
volatile uint8_t markerState = 0;
volatile uint8_t packetCounter = 0;

// Timing
volatile uint32_t lastSampleTimestamp = 0;
volatile uint32_t droppedPackets = 0;

// Simulation phase counter
volatile uint32_t simSampleCount = 0;

// LED
const int LED_STATUS = 13;

// Marker button (optional)
const int MARKER_PIN = 2;

// Timer for 1kHz sampling
IntervalTimer sampleTimer;

// =============================================================================
// Forward Declarations
// =============================================================================

void sampleISR();
void generateSimulatedData();
void buildPacket(uint8_t* packet);
uint8_t calculateChecksum(const uint8_t* data, int len);

// =============================================================================
// Interrupt Service Routine - 1kHz Sample Timer
// =============================================================================

void sampleISR() {
    // Capture timestamp
    lastSampleTimestamp = micros();
    
    // Generate ALL simulated data (EEG + Accel)
    generateSimulatedData();
    
    // Read marker button
    markerState = !digitalRead(MARKER_PIN);  // Active low
    
    // Build packet
    static uint8_t packet[BIOSERIAL_PACKET_SIZE];
    buildPacket(packet);
    
    // Write to ring buffer
    if (!packetBuffer.write(packet)) {
        droppedPackets++;
    }
    
    // Increment counter
    packetCounter++;
    simSampleCount++;
}

// =============================================================================
// Generate Simulated Data - SIMPLE SINE WAVES FOR ALL CHANNELS
// =============================================================================

void generateSimulatedData() {
    // Simple approach: Just sine waves at different frequencies
    // Large amplitudes to ensure visibility in Open Ephys LFP Viewer
    
    const float sampleRate = 1000.0f;
    float t = (float)simSampleCount;  // Time in samples
    
    // =========================================================================
    // EEG Channels (5 channels) - Different frequency sine waves
    // Amplitude: 100,000 counts = ~2235 µV after scaling (very visible!)
    // =========================================================================
    
    // Channel 0: 3 Hz sine wave
    eegSamples[0] = (int32_t)(100000.0f * sinf(2.0f * PI * 3.0f * t / sampleRate));
    
    // Channel 1: 5 Hz sine wave  
    eegSamples[1] = (int32_t)(100000.0f * sinf(2.0f * PI * 5.0f * t / sampleRate));
    
    // Channel 2: 10 Hz sine wave (alpha rhythm)
    eegSamples[2] = (int32_t)(100000.0f * sinf(2.0f * PI * 10.0f * t / sampleRate));
    
    // Channel 3: 15 Hz sine wave
    eegSamples[3] = (int32_t)(100000.0f * sinf(2.0f * PI * 15.0f * t / sampleRate));
    
    // Channel 4: 20 Hz sine wave
    eegSamples[4] = (int32_t)(100000.0f * sinf(2.0f * PI * 20.0f * t / sampleRate));
    
    // Unused EEG channels
    eegSamples[5] = (int32_t)(100000.0f * sinf(2.0f * PI * 8.0f * t / sampleRate));
    eegSamples[6] = (int32_t)(100000.0f * sinf(2.0f * PI * 12.0f * t / sampleRate));
    eegSamples[7] = (int32_t)(100000.0f * sinf(2.0f * PI * 18.0f * t / sampleRate));
    
    // =========================================================================
    // Aux Channels (6 channels) - All simple sine waves
    // =========================================================================
    
    // [0] AccelX: 0.5 Hz sine wave
    auxSamples[0] = (int16_t)(1000.0f * sinf(2.0f * PI * 0.5f * t / sampleRate));
    
    // [1] AccelY: 0.7 Hz sine wave
    auxSamples[1] = (int16_t)(1000.0f * sinf(2.0f * PI * 0.7f * t / sampleRate));
    
    // [2] AccelZ: 0.3 Hz sine wave + gravity offset
    auxSamples[2] = (int16_t)(256 + 500.0f * sinf(2.0f * PI * 0.3f * t / sampleRate));
    
    // [3] PPG: 1.2 Hz sine wave (simulated heartbeat ~72 BPM)
    auxSamples[3] = (int16_t)(10000.0f * sinf(2.0f * PI * 1.2f * t / sampleRate));
    
    // [4] Temperature: 0.05 Hz very slow drift around 3650 (36.5°C)
    auxSamples[4] = (int16_t)(3650 + 50.0f * sinf(2.0f * PI * 0.05f * t / sampleRate));
    
    // [5] Battery: 0.02 Hz very slow wave around 4000mV
    auxSamples[5] = (int16_t)(4000 + 200.0f * sinf(2.0f * PI * 0.02f * t / sampleRate));
}

// =============================================================================
// Build BioSerial-Pro Packet
// =============================================================================

void buildPacket(uint8_t* packet) {
    int idx = 0;
    
    // Header (2 bytes)
    packet[idx++] = BIOSERIAL_HEADER_1;
    packet[idx++] = BIOSERIAL_HEADER_2;
    
    // Timestamp (4 bytes, Big Endian)
    uint32_t ts = lastSampleTimestamp;
    packet[idx++] = (ts >> 24) & 0xFF;
    packet[idx++] = (ts >> 16) & 0xFF;
    packet[idx++] = (ts >> 8) & 0xFF;
    packet[idx++] = ts & 0xFF;
    
    // Marker (1 byte)
    packet[idx++] = markerState;
    
    // EEG Data (15 bytes = 5 channels × 3 bytes, Big Endian)
    for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++) {
        int32_t sample = eegSamples[ch];
        packet[idx++] = (sample >> 16) & 0xFF;
        packet[idx++] = (sample >> 8) & 0xFF;
        packet[idx++] = sample & 0xFF;
    }
    
    // Aux Data (12 bytes = 6 channels × 2 bytes, Big Endian)
    // [0] AccelX, [1] AccelY, [2] AccelZ, [3] PPG, [4] Temp, [5] Battery
    for (int aux = 0; aux < NUM_AUX_CHANNELS; aux++) {
        packet[idx++] = (auxSamples[aux] >> 8) & 0xFF;
        packet[idx++] = auxSamples[aux] & 0xFF;
    }
    
    // Counter (1 byte)
    packet[idx++] = packetCounter;
    
    // Checksum (1 byte) - XOR of bytes 0-29
    packet[idx] = calculateChecksum(packet, idx);
    idx++;
    
    // Footer (2 bytes)
    packet[idx++] = BIOSERIAL_FOOTER_1;
    packet[idx++] = BIOSERIAL_FOOTER_2;
}

// =============================================================================
// Calculate XOR Checksum
// =============================================================================

uint8_t calculateChecksum(const uint8_t* data, int len) {
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
    
    // Initialize Serial (USB) - high speed
    Serial.begin(12000000);
    
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
    Serial.println("BioSerial-Pro Simulation Firmware");
    Serial.println("==================================");
    Serial.print("Packet size: ");
    Serial.print(BIOSERIAL_PACKET_SIZE);
    Serial.println(" bytes");
    Serial.print("Sample rate: ");
    Serial.print(EEG_SAMPLE_RATE_HZ);
    Serial.println(" Hz");
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
    static uint8_t txPacket[BIOSERIAL_PACKET_SIZE];
    static uint32_t lastStatusTime = 0;
    static uint32_t txCount = 0;
    
    // Send all available packets from ring buffer
    while (packetBuffer.available() > 0) {
        if (packetBuffer.read(txPacket)) {
            // Write packet to USB
            Serial.write(txPacket, BIOSERIAL_PACKET_SIZE);
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
