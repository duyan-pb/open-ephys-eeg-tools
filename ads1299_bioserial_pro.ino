/*
 * =============================================================================
 * BioSerial-Pro EEG Acquisition Firmware for Teensy + ADS1299
 * =============================================================================
 * 
 * Protocol: BioSerial-Pro (Fixed 32-byte packets @ 1kHz)
 * 
 * Packet Structure (32 bytes):
 *   Header    (2B): 0xA5 0x5A (Sync sequence)
 *   Timestamp (4B): Microsecond timer (uint32_t, wraps every ~71 minutes)
 *   Marker    (1B): Hardware event triggers (button presses, etc.)
 *   EEG       (15B): 5 channels × 24-bit Big Endian
 *   Aux       (6B): 3 channels × 16-bit (Multiplexed based on counter)
 *   Counter   (1B): 0-255 (Controls Aux multiplexing & packet sequencing)
 *   Checksum  (1B): XOR of bytes 0-29
 *   Footer    (2B): 0xC0 0xC0
 * 
 * Aux Multiplexing Schedule:
 *   Counter 0-199 (mod 5 == 0): Accelerometer XYZ @ 200Hz
 *   Counter 0 (mod 256 == 0):   Health data (battery, temp, etc.) @ ~4Hz
 * 
 * Architecture:
 *   - ISR: Generates data → Writes to Ring Buffer (decoupled)
 *   - Loop: Reads Ring Buffer → Sends USB packets asynchronously
 * 
 * =============================================================================
 */

#include <SPI.h>

extern "C" uint32_t set_arm_clock(uint32_t frequency);

// =============================================================================
// BioSerial-Pro Protocol Constants
// =============================================================================

#define BIOSERIAL_HEADER_1      0xA5
#define BIOSERIAL_HEADER_2      0x5A
#define BIOSERIAL_FOOTER_1      0xC0
#define BIOSERIAL_FOOTER_2      0xC0
#define BIOSERIAL_PACKET_SIZE   32

// EEG Configuration
#define NUM_EEG_CHANNELS        5
#define NUM_AUX_CHANNELS        3
#define BYTES_PER_EEG_SAMPLE    3   // 24-bit
#define BYTES_PER_AUX_SAMPLE    2   // 16-bit

// Sampling rates
#define EEG_SAMPLE_RATE_HZ      1000
#define ACCEL_SAMPLE_RATE_HZ    200
#define HEALTH_SAMPLE_RATE_HZ   4

// Derived constants
#define ACCEL_DIVIDER           (EEG_SAMPLE_RATE_HZ / ACCEL_SAMPLE_RATE_HZ)  // 5

// =============================================================================
// Ring Buffer for Decoupled ISR/Transmission
// =============================================================================

#define RING_BUFFER_PACKETS     64    // ~64ms of data at 1kHz
#define RING_BUFFER_SIZE        (RING_BUFFER_PACKETS * BIOSERIAL_PACKET_SIZE)

class PacketRingBuffer {
public:
    volatile uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head = 0;  // Write position
    volatile uint16_t tail = 0;  // Read position
    volatile uint16_t count = 0; // Number of packets in buffer
    
    bool write(const uint8_t* packet) {
        if (count >= RING_BUFFER_PACKETS) {
            return false;  // Buffer full, packet dropped
        }
        
        // Copy packet to buffer
        uint16_t writePos = head * BIOSERIAL_PACKET_SIZE;
        for (int i = 0; i < BIOSERIAL_PACKET_SIZE; i++) {
            buffer[writePos + i] = packet[i];
        }
        
        // Advance head
        head = (head + 1) % RING_BUFFER_PACKETS;
        __disable_irq();
        count++;
        __enable_irq();
        
        return true;
    }
    
    bool read(uint8_t* packet) {
        if (count == 0) {
            return false;  // Buffer empty
        }
        
        // Copy packet from buffer
        uint16_t readPos = tail * BIOSERIAL_PACKET_SIZE;
        for (int i = 0; i < BIOSERIAL_PACKET_SIZE; i++) {
            packet[i] = buffer[readPos + i];
        }
        
        // Advance tail
        tail = (tail + 1) % RING_BUFFER_PACKETS;
        __disable_irq();
        count--;
        __enable_irq();
        
        return true;
    }
    
    uint16_t available() const {
        return count;
    }
    
    bool isFull() const {
        return count >= RING_BUFFER_PACKETS;
    }
    
    void clear() {
        head = tail = count = 0;
    }
};

PacketRingBuffer packetBuffer;

// =============================================================================
// Hardware Pin Definitions
// =============================================================================

// ADXL345 Accelerometer
const int ADXL_CS = 10;

// ADS1299 EEG AFE
const int ADS1299_CS1 = 7;
const int ADS1299_CS2 = 6;
const int ADS1299_CS3 = 5;
const int START = 15;
const int N_DRDY = 22;
const int N_PWDN = 14;

// Marker input (button or external trigger)
const int MARKER_PIN = 2;

// LED indicators
const int LED_STATUS = 13;
const int LED_SYNC = 0;

// =============================================================================
// SPI Settings
// =============================================================================

SPISettings ADXL_settings(5E6, MSBFIRST, SPI_MODE3);
SPISettings ADS1299_SPI_settings(20E6, MSBFIRST, SPI_MODE1);

// =============================================================================
// ADXL345 Register Definitions
// =============================================================================

const uint8_t ADXL_BW_RATE     = 0x2C;
const uint8_t ADXL_POWER_CTL   = 0x2D;
const uint8_t ADXL_DATA_FORMAT = 0x31;
const uint8_t ADXL_DATAX0      = 0x32;

const uint8_t ADXL_WRITE     = 0x00;
const uint8_t ADXL_READ      = 0x80;
const uint8_t ADXL_READ_MULT = 0xC0;

// =============================================================================
// ADS1299 Register and Command Definitions
// =============================================================================

const int GAIN_1x  = 0x00;
const int GAIN_2x  = 0x10;
const int GAIN_4x  = 0x20;
const int GAIN_6x  = 0x30;
const int GAIN_8x  = 0x40;
const int GAIN_12x = 0x50;
const int GAIN_24x = 0x60;

const int FS_250  = 0x06;
const int FS_500  = 0x05;
const int FS_1k   = 0x04;
const int FS_2k   = 0x03;
const int FS_4k   = 0x02;
const int FS_8k   = 0x01;
const int FS_16k  = 0x00;

const int CHSET_INPUT = 0x00;
const int SHORT = 0x01;
const int TEST  = 0x05;

// =============================================================================
// Global State Variables
// =============================================================================

// Data storage
volatile int32_t eegSamples[8];        // Raw 24-bit EEG from ADS1299
volatile int16_t accelSamples[3];      // Accelerometer XYZ
volatile uint8_t markerState = 0;      // Hardware marker
volatile uint8_t packetCounter = 0;    // 0-255 rolling counter

// Timing
volatile uint32_t lastSampleTimestamp = 0;
volatile uint32_t droppedPackets = 0;

// Health monitoring
volatile int16_t batteryVoltage = 0;
volatile int16_t temperature = 0;
volatile int16_t impedanceCheck = 0;

// Accelerometer divider
volatile uint8_t accelDivider = 0;

// Simulation mode flag (set to true if no hardware attached)
bool simulationMode = false;
double simPhase = 0.0;

// =============================================================================
// Forward Declarations
// =============================================================================

void ADXL_init();
void ADXL_read(int16_t* x, int16_t* y, int16_t* z);
void ADS1299_init();
void ADS1299_RESET(int CS);
void ADS1299_SDATAC(int CS);
void ADS1299_RDATAC(int CS);
void ADS1299_WREG(int CS, uint8_t addr, uint8_t data);
uint8_t ADS1299_RREG(int CS, uint8_t addr);
void readEEGData();
void generateSimulatedData();
void buildPacket(uint8_t* packet);
uint8_t calculateChecksum(const uint8_t* data, int len);

// =============================================================================
// Interrupt Service Routine - Data Ready from ADS1299
// =============================================================================

void ADS1299_dataReady_ISR() {
    // Capture timestamp immediately
    lastSampleTimestamp = micros();
    
    // Read EEG data from ADS1299 (or generate simulated data)
    if (simulationMode) {
        generateSimulatedData();
    } else {
        readEEGData();
    }
    
    // Read accelerometer at reduced rate (200Hz)
    if (accelDivider == 0) {
        if (!simulationMode) {
            ADXL_read(&accelSamples[0], &accelSamples[1], &accelSamples[2]);
        } else {
            // Simulated accelerometer (gravity + small vibration)
            accelSamples[0] = 10 + (int16_t)(5.0 * sin(simPhase * 0.3));
            accelSamples[1] = 15 + (int16_t)(3.0 * cos(simPhase * 0.2));
            accelSamples[2] = 250 + (int16_t)(2.0 * sin(simPhase * 0.1)); // ~1g
        }
        accelDivider = ACCEL_DIVIDER - 1;
    } else {
        accelDivider--;
    }
    
    // Read marker pin
    markerState = digitalReadFast(MARKER_PIN) ? 0x01 : 0x00;
    
    // Update health metrics at 1Hz (counter == 0)
    if (packetCounter == 0) {
        // Read battery voltage from ADC (placeholder)
        batteryVoltage = analogRead(A0);  // Scale appropriately
        // Temperature (placeholder)
        temperature = 250;  // 25.0°C * 10
        // Impedance check (placeholder)
        impedanceCheck = 1000;
    }
    
    // Build and queue the packet
    uint8_t packet[BIOSERIAL_PACKET_SIZE];
    buildPacket(packet);
    
    if (!packetBuffer.write(packet)) {
        droppedPackets++;
    }
    
    // Increment counter (wraps 0-255)
    packetCounter++;
}

// =============================================================================
// Read EEG Data from ADS1299
// =============================================================================

void readEEGData() {
    SPI.beginTransaction(ADS1299_SPI_settings);
    delayNanoseconds(10);
    digitalWriteFast(ADS1299_CS1, LOW);
    delayNanoseconds(6);
    
    // Skip status registers (3 bytes)
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    
    // Read 8 channels (only use first 5 for protocol)
    for (int i = 0; i < 8; i++) {
        int32_t sample = 0;
        sample = SPI.transfer(0x00);
        sample = (sample << 8) | SPI.transfer(0x00);
        sample = (sample << 8) | SPI.transfer(0x00);
        // Sign extend 24-bit to 32-bit
        if (sample & 0x800000) {
            sample |= 0xFF000000;
        }
        eegSamples[i] = sample;
    }
    
    delayMicroseconds(2);
    digitalWriteFast(ADS1299_CS1, HIGH);
    SPI.endTransaction();
}

// =============================================================================
// Generate Simulated EEG Data (10Hz sine wave)
// =============================================================================

void generateSimulatedData() {
    // Generate 10Hz sine wave for each channel with different phases
    const double freqHz = 10.0;
    const double sampleRate = 1000.0;
    const double amplitude = 100000.0;  // ~100µV at 24-bit resolution
    
    for (int ch = 0; ch < 8; ch++) {
        double phase = simPhase + (ch * M_PI / 8.0);  // Phase offset per channel
        double value = amplitude * sin(2.0 * M_PI * freqHz * phase / sampleRate);
        
        // Add some channel-specific variation
        value *= (1.0 + ch * 0.1);
        
        // Add small noise
        value += (random(-1000, 1000));
        
        eegSamples[ch] = (int32_t)value;
    }
    
    simPhase += 1.0;  // Increment phase
}

// =============================================================================
// Build BioSerial-Pro Packet
// =============================================================================

void buildPacket(uint8_t* packet) {
    int idx = 0;
    
    // Header (2 bytes)
    packet[idx++] = BIOSERIAL_HEADER_1;
    packet[idx++] = BIOSERIAL_HEADER_2;
    
    // Timestamp (4 bytes, Little Endian for ARM efficiency, sent as Big Endian)
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
    
    // Aux Data (6 bytes = 3 channels × 2 bytes, Big Endian)
    // Multiplexing based on counter:
    //   - Default: Accelerometer XYZ (when updated)
    //   - Counter 0: Health data (battery, temp, impedance)
    
    if (packetCounter == 0) {
        // Health packet: Battery, Temperature, Impedance
        packet[idx++] = (batteryVoltage >> 8) & 0xFF;
        packet[idx++] = batteryVoltage & 0xFF;
        packet[idx++] = (temperature >> 8) & 0xFF;
        packet[idx++] = temperature & 0xFF;
        packet[idx++] = (impedanceCheck >> 8) & 0xFF;
        packet[idx++] = impedanceCheck & 0xFF;
    } else {
        // Accelerometer XYZ
        packet[idx++] = (accelSamples[0] >> 8) & 0xFF;
        packet[idx++] = accelSamples[0] & 0xFF;
        packet[idx++] = (accelSamples[1] >> 8) & 0xFF;
        packet[idx++] = accelSamples[1] & 0xFF;
        packet[idx++] = (accelSamples[2] >> 8) & 0xFF;
        packet[idx++] = accelSamples[2] & 0xFF;
    }
    
    // Counter (1 byte)
    packet[idx++] = packetCounter;
    
    // Checksum (1 byte) - XOR of bytes 0-29
    packet[idx++] = calculateChecksum(packet, idx);
    
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
// ADXL345 Accelerometer Functions
// =============================================================================

void ADXL_init() {
    SPI.beginTransaction(ADXL_settings);
    
    // Data format: Full resolution, +/-16g
    digitalWriteFast(ADXL_CS, LOW);
    delayNanoseconds(5);
    SPI.transfer(ADXL_DATA_FORMAT | ADXL_WRITE);
    SPI.transfer(0x0B);
    delayNanoseconds(10);
    digitalWriteFast(ADXL_CS, HIGH);
    
    delayNanoseconds(150);
    
    // Sampling rate: 200Hz
    digitalWriteFast(ADXL_CS, LOW);
    delayNanoseconds(5);
    SPI.transfer(ADXL_BW_RATE | ADXL_WRITE);
    SPI.transfer(0x0B);
    delayNanoseconds(10);
    digitalWriteFast(ADXL_CS, HIGH);
    
    delayNanoseconds(150);
    
    // Power control: Measurement mode
    digitalWriteFast(ADXL_CS, LOW);
    delayNanoseconds(5);
    SPI.transfer(ADXL_POWER_CTL | ADXL_WRITE);
    SPI.transfer(0x08);
    delayNanoseconds(10);
    digitalWriteFast(ADXL_CS, HIGH);
    
    SPI.endTransaction();
}

void ADXL_read(int16_t* x, int16_t* y, int16_t* z) {
    SPI.beginTransaction(ADXL_settings);
    digitalWriteFast(ADXL_CS, LOW);
    delayNanoseconds(5);
    SPI.transfer(ADXL_DATAX0 | ADXL_READ_MULT);
    *x = (int16_t)SPI.transfer(0) | ((int16_t)SPI.transfer(0) << 8);
    *y = (int16_t)SPI.transfer(0) | ((int16_t)SPI.transfer(0) << 8);
    *z = (int16_t)SPI.transfer(0) | ((int16_t)SPI.transfer(0) << 8);
    delayNanoseconds(10);
    digitalWriteFast(ADXL_CS, HIGH);
    SPI.endTransaction();
}

// =============================================================================
// ADS1299 Register Access Functions
// =============================================================================

void ADS1299_RESET(int CS) {
    SPI.beginTransaction(ADS1299_SPI_settings);
    digitalWrite(CS, LOW);
    delayNanoseconds(6);
    SPI.transfer(0x06);  // RESET command
    delayMicroseconds(2);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

void ADS1299_SDATAC(int CS) {
    SPI.beginTransaction(ADS1299_SPI_settings);
    digitalWrite(CS, LOW);
    delayNanoseconds(6);
    SPI.transfer(0x11);  // SDATAC command
    delayMicroseconds(2);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

void ADS1299_RDATAC(int CS) {
    SPI.beginTransaction(ADS1299_SPI_settings);
    digitalWrite(CS, LOW);
    delayNanoseconds(6);
    SPI.transfer(0x10);  // RDATAC command
    delayMicroseconds(2);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

void ADS1299_WREG(int CS, uint8_t addr, uint8_t data) {
    SPI.beginTransaction(ADS1299_SPI_settings);
    digitalWrite(CS, LOW);
    delayNanoseconds(6);
    SPI.transfer(addr + 0x40);
    delayMicroseconds(2);
    SPI.transfer(0x00);  // Write 1 register
    delayMicroseconds(2);
    SPI.transfer(data);
    delayMicroseconds(2);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
}

uint8_t ADS1299_RREG(int CS, uint8_t addr) {
    uint8_t val = 0;
    SPI.beginTransaction(ADS1299_SPI_settings);
    digitalWrite(CS, LOW);
    delayNanoseconds(6);
    SPI.transfer(addr + 0x20);
    delayMicroseconds(2);
    SPI.transfer(0x00);  // Read 1 register
    delayMicroseconds(2);
    val = SPI.transfer(0x00);
    delayMicroseconds(2);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
    return val;
}

void ADS1299_init() {
    // Power up
    digitalWrite(N_PWDN, HIGH);
    delay(128);  // Wait for power-on reset
    
    // Reset
    ADS1299_RESET(ADS1299_CS1);
    delayMicroseconds(9);
    
    // Stop continuous data mode
    ADS1299_SDATAC(ADS1299_CS1);
    
    // Configuration
    ADS1299_WREG(ADS1299_CS1, 0x01, 0x90 | 0x20 | FS_1k);  // CONFIG1: Clock out, 1kHz
    ADS1299_WREG(ADS1299_CS1, 0x03, 0xE0);  // CONFIG3: Internal reference
    
    delay(10);  // Reference settle
    
    // Channel settings (Gain 24x, Normal input)
    for (int ch = 0; ch < 8; ch++) {
        ADS1299_WREG(ADS1299_CS1, 0x05 + ch, GAIN_24x | CHSET_INPUT);
    }
    
    // Verify device ID
    uint8_t id = ADS1299_RREG(ADS1299_CS1, 0x00);
    if (id != 0x3E) {
        Serial.print("Warning: Unexpected ADS1299 ID: 0x");
        Serial.println(id, HEX);
        simulationMode = true;
    }
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    // Set CPU clock for low jitter
    set_arm_clock(151200000);  // 151.2 MHz
    
    // Initialize Serial (USB)
    Serial.begin(12000000);  // High baud rate for USB
    
    // Initialize SPI
    SPI.begin();
    
    // Configure pins
    pinMode(MARKER_PIN, INPUT_PULLUP);
    pinMode(LED_STATUS, OUTPUT);
    pinMode(LED_SYNC, OUTPUT);
    
    pinMode(ADXL_CS, OUTPUT);
    digitalWriteFast(ADXL_CS, HIGH);
    
    pinMode(ADS1299_CS1, OUTPUT);
    pinMode(ADS1299_CS2, OUTPUT);
    pinMode(ADS1299_CS3, OUTPUT);
    digitalWriteFast(ADS1299_CS1, HIGH);
    digitalWriteFast(ADS1299_CS2, HIGH);
    digitalWriteFast(ADS1299_CS3, HIGH);
    
    pinMode(START, OUTPUT);
    digitalWrite(START, LOW);
    pinMode(N_PWDN, OUTPUT);
    digitalWrite(N_PWDN, LOW);
    pinMode(N_DRDY, INPUT);
    
    // Initialize LED
    digitalWriteFast(LED_STATUS, HIGH);
    
    // Try to initialize hardware
    Serial.println("BioSerial-Pro Initializing...");
    
    // Initialize accelerometer
    ADXL_init();
    
    // Initialize ADS1299 (will set simulationMode if not found)
    ADS1299_init();
    
    if (simulationMode) {
        Serial.println("Hardware not detected - Running in SIMULATION mode");
        Serial.println("Generating 10Hz sine wave test signal");
        
        // Use timer for simulation mode (1kHz interrupt)
        // For Teensy 4.x, use IntervalTimer
        static IntervalTimer simTimer;
        simTimer.begin(ADS1299_dataReady_ISR, 1000);  // 1000µs = 1kHz
    } else {
        Serial.println("ADS1299 detected - Running in HARDWARE mode");
        
        // Start conversion
        digitalWrite(START, HIGH);
        
        // Enable continuous data mode
        ADS1299_RDATAC(ADS1299_CS1);
        
        // Attach interrupt for DRDY
        attachInterrupt(digitalPinToInterrupt(N_DRDY), ADS1299_dataReady_ISR, FALLING);
    }
    
    Serial.println("BioSerial-Pro Ready");
    Serial.print("Packet size: ");
    Serial.print(BIOSERIAL_PACKET_SIZE);
    Serial.println(" bytes");
    Serial.print("Sample rate: ");
    Serial.print(EEG_SAMPLE_RATE_HZ);
    Serial.println(" Hz");
    
    // Wait before starting transmission
    delay(1000);
    
    digitalWriteFast(LED_STATUS, LOW);
}

// =============================================================================
// Main Loop - Asynchronous USB Transmission
// =============================================================================

void loop() {
    static uint8_t txPacket[BIOSERIAL_PACKET_SIZE];
    static uint32_t lastStatusTime = 0;
    static uint32_t txCount = 0;
    
    // Check for packets in the ring buffer
    while (packetBuffer.available() > 0) {
        // Read packet from ring buffer
        if (packetBuffer.read(txPacket)) {
            // Write to USB Serial
            Serial.write(txPacket, BIOSERIAL_PACKET_SIZE);
            
            // CRITICAL: Force USB packet transmission immediately
            // This ensures ~1ms latency instead of waiting for USB buffer fill
            Serial.send_now();
            
            txCount++;
        }
    }
    
    // Toggle status LED every second
    uint32_t now = millis();
    if (now - lastStatusTime >= 1000) {
        lastStatusTime = now;
        digitalToggle(LED_STATUS);
        
        // Print stats every 10 seconds
        static uint8_t statCounter = 0;
        if (++statCounter >= 10) {
            statCounter = 0;
            Serial.print("Stats: TX=");
            Serial.print(txCount);
            Serial.print(" Dropped=");
            Serial.print(droppedPackets);
            Serial.print(" BufferUsed=");
            Serial.println(packetBuffer.available());
        }
    }
}

// =============================================================================
// Digital Toggle Helper
// =============================================================================

void digitalToggle(int pin) {
    digitalWrite(pin, !digitalRead(pin));
}
