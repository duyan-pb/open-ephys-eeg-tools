/*
 * InEar Teensy Firmware for Teensy 4.1
 * 
 * SIMULATION MODE: Generates synthetic data for all sensors
 * Streams 56-byte packets over USB at 1kHz to Open Ephys
 * 
 * Protocol: 5 EEG (24-bit) + 3 Accel (16-bit) + 3 PPG (48-bit) + Temp + Battery + Sync
 */

#include <SPI.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

#define SIMULATION_MODE true   // Set to false for real hardware
#define SAMPLE_RATE_HZ 1000
#define USB_BAUD 2000000

// InEar Teensy Protocol Constants (must match plugin!)
#define IET_HEADER_1     0xA5
#define IET_HEADER_2     0x5A
#define IET_FOOTER_1     0xC0
#define IET_FOOTER_2     0xC0
#define IET_PACKET_SIZE  56
#define IET_NUM_EEG_CH   5
#define IET_NUM_ACCEL_CH 3
#define IET_NUM_PPG_CH   3

// Math constant - avoid conflict with Arduino's wiring.h
#ifndef TWO_PI
#define TWO_PI 6.283185307179586
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// PACKET STRUCTURE (56 bytes total)
// ============================================================================
// Offset  Size  Description
// ------  ----  -----------
// 0       2     Header (0xA5 0x5A)
// 2       4     Timestamp (microseconds, uint32, BIG ENDIAN)
// 6       1     Marker byte (event triggers)
// 7       15    EEG data (5 channels × 24-bit = 15 bytes, BIG ENDIAN)
// 22      6     Accelerometer (3 × int16 = 6 bytes, BIG ENDIAN)
// 28      18    PPG data (3 channels × 48-bit = 18 bytes, BIG ENDIAN)
// 46      2     Temperature (int16, 0.01°C units, BIG ENDIAN)
// 48      2     Battery (uint16, mV, BIG ENDIAN)
// 50      2     Sync signal (uint16, BIG ENDIAN)
// 52      1     Packet counter (uint8, 0-255)
// 53      1     Checksum (XOR of bytes 0-52)
// 54      2     Footer (0xC0 0xC0)

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

uint8_t packetBuffer[IET_PACKET_SIZE];
uint8_t packetCounter = 0;
uint32_t sampleCount = 0;

// Simulation variables
float simTime = 0.0f;
const float simDeltaT = 1.0f / SAMPLE_RATE_HZ;

// Timing
IntervalTimer sampleTimer;
volatile bool dataReady = false;

// EEG simulation frequencies (Hz) for each channel
const float eegFreqs[IET_NUM_EEG_CH] = {3.0f, 6.0f, 10.0f, 20.0f, 40.0f};

// ============================================================================
// SIMULATION DATA GENERATION
// ============================================================================

void generateSimulatedData() {
    // --- EEG Data (5 channels, 24-bit each, BIG ENDIAN) ---
    // Generate sine waves at different frequencies with some noise
    for (int ch = 0; ch < IET_NUM_EEG_CH; ch++) {
        float amplitude = 100000.0f;  // ~100µV in 24-bit scale
        float signal = amplitude * sin(TWO_PI * eegFreqs[ch] * simTime);
        
        // Add some noise
        signal += (random(-1000, 1000));
        
        int32_t eegValue = (int32_t)signal;
        
        // Pack 24-bit value (BIG ENDIAN) at offset 7 + (ch * 3)
        int offset = 7 + (ch * 3);
        packetBuffer[offset + 0] = (eegValue >> 16) & 0xFF;  // MSB first
        packetBuffer[offset + 1] = (eegValue >> 8) & 0xFF;
        packetBuffer[offset + 2] = eegValue & 0xFF;          // LSB last
    }
    
    // --- Accelerometer Data (3 channels, 16-bit each, BIG ENDIAN) ---
    // Simulate gentle motion/breathing artifact
    float accelX = 100 * sin(TWO_PI * 0.2f * simTime);  // 0.2 Hz sway
    float accelY = 50 * sin(TWO_PI * 0.15f * simTime);  // 0.15 Hz
    float accelZ = 16384 + 30 * sin(TWO_PI * 0.25f * simTime);  // Gravity + breathing
    
    int16_t ax = (int16_t)accelX;
    int16_t ay = (int16_t)accelY;
    int16_t az = (int16_t)accelZ;
    
    // Pack at offset 22 (BIG ENDIAN - MSB first)
    packetBuffer[22] = (ax >> 8) & 0xFF;
    packetBuffer[23] = ax & 0xFF;
    packetBuffer[24] = (ay >> 8) & 0xFF;
    packetBuffer[25] = ay & 0xFF;
    packetBuffer[26] = (az >> 8) & 0xFF;
    packetBuffer[27] = az & 0xFF;
    
    // --- PPG Data (3 channels, 48-bit each, BIG ENDIAN) ---
    // Simulate heartbeat at ~72 BPM (1.2 Hz)
    float heartRate = 1.2f;  // Hz
    float heartPhase = fmod(simTime * heartRate, 1.0f);
    
    // Create realistic PPG waveform shape
    float ppgPulse = 0;
    if (heartPhase < 0.15f) {
        // Systolic rise
        ppgPulse = sin(M_PI * heartPhase / 0.15f);
    } else if (heartPhase < 0.4f) {
        // Diastolic decay with dicrotic notch
        float decay = (heartPhase - 0.15f) / 0.25f;
        ppgPulse = cos(M_PI * decay * 0.5f) * 0.8f;
        // Dicrotic notch
        if (heartPhase > 0.25f && heartPhase < 0.32f) {
            ppgPulse += 0.15f * sin(M_PI * (heartPhase - 0.25f) / 0.07f);
        }
    }
    
    // PPG channels: Red, IR, Green with different DC offsets and AC amplitudes
    int64_t ppgRed   = (int64_t)(100000000000LL + ppgPulse * 5000000000LL);
    int64_t ppgIR    = (int64_t)(120000000000LL + ppgPulse * 6000000000LL);
    int64_t ppgGreen = (int64_t)(80000000000LL + ppgPulse * 4000000000LL);
    
    // Pack 48-bit values at offset 28 (6 bytes each, BIG ENDIAN)
    packInt48BE(packetBuffer + 28, ppgRed);
    packInt48BE(packetBuffer + 34, ppgIR);
    packInt48BE(packetBuffer + 40, ppgGreen);
    
    // --- Temperature (16-bit, 0.01°C units, BIG ENDIAN) ---
    // Simulate slow temperature drift around 36.5°C
    float tempC = 36.5f + 0.1f * sin(TWO_PI * 0.001f * simTime);
    int16_t tempValue = (int16_t)(tempC * 100);  // 3650 = 36.50°C
    packetBuffer[46] = (tempValue >> 8) & 0xFF;
    packetBuffer[47] = tempValue & 0xFF;
    
    // --- Battery (16-bit, mV, BIG ENDIAN) ---
    // Simulate slow battery discharge from 4200mV to 3700mV over time
    float batteryMV = 4200.0f - (simTime * 0.01f);
    if (batteryMV < 3700.0f) batteryMV = 3700.0f;
    uint16_t battValue = (uint16_t)batteryMV;
    packetBuffer[48] = (battValue >> 8) & 0xFF;
    packetBuffer[49] = battValue & 0xFF;
    
    // --- Sync Signal (16-bit, BIG ENDIAN) ---
    // Generate periodic sync pulses
    static uint32_t nextSyncTime = 0;
    static uint16_t syncState = 0;
    
    if (sampleCount >= nextSyncTime) {
        if (syncState == 0) {
            syncState = 1;
            nextSyncTime = sampleCount + 100;  // 100ms pulse
        } else {
            syncState = 0;
            nextSyncTime = sampleCount + random(500, 2000);  // Random interval
        }
    }
    packetBuffer[50] = (syncState >> 8) & 0xFF;
    packetBuffer[51] = syncState & 0xFF;
    
    simTime += simDeltaT;
}

void packInt48BE(uint8_t* dest, int64_t value) {
    // Pack 48-bit value in BIG ENDIAN format (MSB first)
    dest[0] = (value >> 40) & 0xFF;
    dest[1] = (value >> 32) & 0xFF;
    dest[2] = (value >> 24) & 0xFF;
    dest[3] = (value >> 16) & 0xFF;
    dest[4] = (value >> 8) & 0xFF;
    dest[5] = value & 0xFF;
}

// ============================================================================
// PACKET BUILDING AND TRANSMISSION
// ============================================================================

void buildPacket() {
    // Header (bytes 0-1): 0xA5 0x5A
    packetBuffer[0] = IET_HEADER_1;
    packetBuffer[1] = IET_HEADER_2;
    
    // Timestamp (bytes 2-5, BIG ENDIAN microseconds)
    uint32_t timestamp = micros();
    packetBuffer[2] = (timestamp >> 24) & 0xFF;  // MSB first
    packetBuffer[3] = (timestamp >> 16) & 0xFF;
    packetBuffer[4] = (timestamp >> 8) & 0xFF;
    packetBuffer[5] = timestamp & 0xFF;          // LSB last
    
    // Marker byte (byte 6) - event triggers
    packetBuffer[6] = 0x00;  // No event marker
    
    // Generate sensor data (fills bytes 7-51)
    if (SIMULATION_MODE) {
        generateSimulatedData();
    } else {
        // TODO: Read from real hardware
        readRealSensorData();
    }
    
    // Packet counter (byte 52)
    packetBuffer[52] = packetCounter++;
    
    // Checksum (byte 53) - XOR of bytes 0-52 (header through counter)
    uint8_t checksum = 0;
    for (int i = 0; i < 53; i++) {
        checksum ^= packetBuffer[i];
    }
    packetBuffer[53] = checksum;
    
    // Footer (bytes 54-55): 0xC0 0xC0
    packetBuffer[54] = IET_FOOTER_1;
    packetBuffer[55] = IET_FOOTER_2;
    
    // Increment sample count (used for sync timing in both modes)
    sampleCount++;
}

void readRealSensorData() {
    // Placeholder for real hardware reading
    // This would interface with ADS1299, ADXL, MAX30102, etc.
    memset(packetBuffer + 7, 0, 45);  // Zero out data portion
}

void sendPacket() {
    Serial.write(packetBuffer, IET_PACKET_SIZE);
}

// ============================================================================
// TIMER INTERRUPT
// ============================================================================

void sampleTimerISR() {
    dataReady = true;
}

// ============================================================================
// SETUP AND LOOP
// ============================================================================

void setup() {
    // Initialize USB Serial
    Serial.begin(USB_BAUD);
    
    // Wait for serial connection (with timeout)
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime < 5000)) {
        // Wait up to 5 seconds for serial
    }
    
    // Initialize packet buffer
    memset(packetBuffer, 0, IET_PACKET_SIZE);
    
    // Initialize random seed
    randomSeed(analogRead(0));
    
    // Setup LED for status
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    
    // Start sample timer at 1kHz
    sampleTimer.begin(sampleTimerISR, 1000000 / SAMPLE_RATE_HZ);
    
    // Small delay before starting
    delay(100);
}

void loop() {
    if (dataReady) {
        dataReady = false;
        
        // Build and send packet
        buildPacket();
        sendPacket();
        
        // Toggle LED every 500 samples (2Hz blink)
        static uint16_t ledCounter = 0;
        if (++ledCounter >= 500) {
            ledCounter = 0;
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        }
    }
}

