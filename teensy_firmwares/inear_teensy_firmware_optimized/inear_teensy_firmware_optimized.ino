/*
 * InEar Teensy OPTIMIZED Firmware for Teensy 4.1
 * 
 * SIMULATION MODE: Generates synthetic data for all sensors
 * Streams VARIABLE-LENGTH packets over USB at 1kHz to Open Ephys
 * 
 * Optimized Protocol Features:
 * - Variable packet sizes (26-55 bytes) based on data content
 * - Multi-rate sensor sampling (EEG 1kHz, Accel 250Hz, PPG 100Hz, Health 10Hz)
 * - Enumerated packet types for efficient parsing
 * - Sequence numbers for gap detection
 * 
 * Must be used with "InEar Teensy Opt" plugin in Open Ephys!
 */

#include <SPI.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

#define SIMULATION_MODE true   // Set to false for real hardware
#define SAMPLE_RATE_HZ 1000
#define USB_BAUD 2000000

// ============================================================================
// PROTOCOL CONSTANTS (must match plugin OptimizedProtocol.h!)
// ============================================================================

#define SYNC_BYTE_1     0xA5
#define SYNC_BYTE_2     0x5A
#define FOOTER_BYTE_1   0xC0
#define FOOTER_BYTE_2   0xC0

#define NUM_EEG_CHANNELS   5
#define NUM_ACCEL_CHANNELS 3
#define NUM_PPG_CHANNELS   3

#define SIZE_EEG       15    // 5ch × 24-bit = 15 bytes
#define SIZE_ACCEL     6     // 3ch × 16-bit = 6 bytes
#define SIZE_PPG       18    // 3ch × 48-bit = 18 bytes
#define SIZE_HEALTH    4     // Temp(2) + Battery(2) = 4 bytes
#define SIZE_MARKER    1     // 1 byte
#define SIZE_HEADER    8     // Sync(2) + Type(1) + Seq(1) + Timestamp(4)
#define SIZE_FOOTER    3     // Checksum(1) + Footer(2)

#define MAX_PACKET_SIZE 55   // Full sync with marker

// Packet types
#define PKT_EEG_ONLY           0x00
#define PKT_EEG_ACCEL          0x01
#define PKT_EEG_PPG            0x02
#define PKT_EEG_ACCEL_PPG      0x03
#define PKT_EEG_HEALTH         0x04
#define PKT_EEG_ACCEL_HEALTH   0x05
#define PKT_EEG_FULL_SYNC      0x06

#define PKT_MARKER_FLAG        0x10  // OR with base type to add marker

// Math constants
#ifndef TWO_PI
#define TWO_PI 6.283185307179586
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

uint8_t packetBuffer[MAX_PACKET_SIZE];
uint8_t packetSequence = 0;
uint32_t sampleCount = 0;

// Simulation variables
float simTime = 0.0f;
const float simDeltaT = 1.0f / SAMPLE_RATE_HZ;

// Timing
IntervalTimer sampleTimer;
volatile bool dataReady = false;

// EEG simulation frequencies (Hz) for each channel
const float eegFreqs[NUM_EEG_CHANNELS] = {3.0f, 6.0f, 10.0f, 20.0f, 40.0f};

// Current sensor values (for sample-and-hold on multi-rate sensors)
int16_t accelValues[3] = {0, 0, 0};
int64_t ppgValues[3] = {0, 0, 0};
int16_t temperature = 3650;  // 36.50°C in centi-degrees
int16_t battery = 4200;      // 4200 mV
uint8_t markerByte = 0;

// ============================================================================
// PACKET TYPE DETERMINATION
// ============================================================================

uint8_t getPacketType(uint32_t sampleNum, bool hasMarker)
{
    uint8_t baseType;
    
    if (sampleNum % 1000 == 0)
    {
        // Full sync every 1000 samples (1 Hz)
        baseType = PKT_EEG_FULL_SYNC;
    }
    else if (sampleNum % 100 == 0)
    {
        // Health data every 100 samples (10 Hz)
        if (sampleNum % 4 == 0)
            baseType = PKT_EEG_ACCEL_HEALTH;  // Also include accel if aligned
        else
            baseType = PKT_EEG_HEALTH;
    }
    else if (sampleNum % 20 == 0)
    {
        // Both accel (250Hz) and PPG (100Hz) aligned
        baseType = PKT_EEG_ACCEL_PPG;
    }
    else if (sampleNum % 10 == 0)
    {
        // PPG every 10 samples (100 Hz)
        baseType = PKT_EEG_PPG;
    }
    else if (sampleNum % 4 == 0)
    {
        // Accel every 4 samples (250 Hz)
        baseType = PKT_EEG_ACCEL;
    }
    else
    {
        // EEG only
        baseType = PKT_EEG_ONLY;
    }
    
    if (hasMarker)
    {
        baseType |= PKT_MARKER_FLAG;
    }
    
    return baseType;
}

int getPacketSize(uint8_t packetType)
{
    int size = SIZE_HEADER + SIZE_EEG + SIZE_FOOTER;
    
    // Marker flag adds 1 byte
    if (packetType & PKT_MARKER_FLAG)
    {
        size += SIZE_MARKER;
    }
    
    // Get base type
    uint8_t baseType = packetType & 0x0F;
    
    switch (baseType)
    {
        case PKT_EEG_ONLY:
            break;
        case PKT_EEG_ACCEL:
            size += SIZE_ACCEL;
            break;
        case PKT_EEG_PPG:
            size += SIZE_PPG;
            break;
        case PKT_EEG_ACCEL_PPG:
            size += SIZE_ACCEL + SIZE_PPG;
            break;
        case PKT_EEG_HEALTH:
            size += SIZE_HEALTH;
            break;
        case PKT_EEG_ACCEL_HEALTH:
            size += SIZE_ACCEL + SIZE_HEALTH;
            break;
        case PKT_EEG_FULL_SYNC:
            size += SIZE_ACCEL + SIZE_PPG + SIZE_HEALTH;
            break;
    }
    
    return size;
}

// ============================================================================
// DATA GENERATION (SIMULATION)
// ============================================================================

void generateSimulatedData(uint8_t packetType)
{
    uint8_t baseType = packetType & 0x0F;
    bool includeAccel = (baseType == PKT_EEG_ACCEL || baseType == PKT_EEG_ACCEL_PPG || 
                         baseType == PKT_EEG_ACCEL_HEALTH || baseType == PKT_EEG_FULL_SYNC);
    bool includePPG = (baseType == PKT_EEG_PPG || baseType == PKT_EEG_ACCEL_PPG || 
                       baseType == PKT_EEG_FULL_SYNC);
    bool includeHealth = (baseType == PKT_EEG_HEALTH || baseType == PKT_EEG_ACCEL_HEALTH || 
                          baseType == PKT_EEG_FULL_SYNC);
    
    // Update accelerometer (250 Hz)
    if (includeAccel)
    {
        accelValues[0] = (int16_t)(100 * sin(TWO_PI * 0.2f * simTime));  // X
        accelValues[1] = (int16_t)(50 * sin(TWO_PI * 0.15f * simTime));   // Y
        accelValues[2] = (int16_t)(16384 + 30 * sin(TWO_PI * 0.25f * simTime)); // Z (gravity)
    }
    
    // Update PPG (100 Hz)
    if (includePPG)
    {
        float heartRate = 1.2f;  // ~72 BPM
        float heartPhase = fmod(simTime * heartRate, 1.0f);
        
        float ppgPulse = 0;
        if (heartPhase < 0.15f)
        {
            ppgPulse = sin(M_PI * heartPhase / 0.15f);
        }
        else if (heartPhase < 0.4f)
        {
            float decay = (heartPhase - 0.15f) / 0.25f;
            ppgPulse = cos(M_PI * decay * 0.5f) * 0.8f;
            if (heartPhase > 0.25f && heartPhase < 0.32f)
            {
                ppgPulse += 0.15f * sin(M_PI * (heartPhase - 0.25f) / 0.07f);
            }
        }
        
        ppgValues[0] = (int64_t)(100000000000LL + ppgPulse * 5000000000LL);   // Red
        ppgValues[1] = (int64_t)(120000000000LL + ppgPulse * 6000000000LL);   // IR
        ppgValues[2] = (int64_t)(80000000000LL + ppgPulse * 4000000000LL);    // Green
    }
    
    // Update health (10 Hz)
    if (includeHealth)
    {
        temperature = (int16_t)(3650 + 10 * sin(TWO_PI * 0.001f * simTime));  // ~36.5°C
        battery = (int16_t)(4200 - (simTime * 0.01f));  // Slow discharge
        if (battery < 3700) battery = 3700;
    }
    
    simTime += simDeltaT;
}

// ============================================================================
// PACKET BUILDING
// ============================================================================

void packInt24BE(uint8_t* dest, int32_t value)
{
    dest[0] = (value >> 16) & 0xFF;
    dest[1] = (value >> 8) & 0xFF;
    dest[2] = value & 0xFF;
}

void packInt16BE(uint8_t* dest, int16_t value)
{
    dest[0] = (value >> 8) & 0xFF;
    dest[1] = value & 0xFF;
}

void packInt48BE(uint8_t* dest, int64_t value)
{
    dest[0] = (value >> 40) & 0xFF;
    dest[1] = (value >> 32) & 0xFF;
    dest[2] = (value >> 24) & 0xFF;
    dest[3] = (value >> 16) & 0xFF;
    dest[4] = (value >> 8) & 0xFF;
    dest[5] = value & 0xFF;
}

void packUint32BE(uint8_t* dest, uint32_t value)
{
    dest[0] = (value >> 24) & 0xFF;
    dest[1] = (value >> 16) & 0xFF;
    dest[2] = (value >> 8) & 0xFF;
    dest[3] = value & 0xFF;
}

int buildPacket(uint8_t packetType)
{
    uint8_t baseType = packetType & 0x0F;
    bool hasMarker = (packetType & PKT_MARKER_FLAG) != 0;
    
    bool includeAccel = (baseType == PKT_EEG_ACCEL || baseType == PKT_EEG_ACCEL_PPG || 
                         baseType == PKT_EEG_ACCEL_HEALTH || baseType == PKT_EEG_FULL_SYNC);
    bool includePPG = (baseType == PKT_EEG_PPG || baseType == PKT_EEG_ACCEL_PPG || 
                       baseType == PKT_EEG_FULL_SYNC);
    bool includeHealth = (baseType == PKT_EEG_HEALTH || baseType == PKT_EEG_ACCEL_HEALTH || 
                          baseType == PKT_EEG_FULL_SYNC);
    
    int offset = 0;
    
    // Header: Sync(2) + Type(1) + Seq(1) + Timestamp(4) = 8 bytes
    packetBuffer[offset++] = SYNC_BYTE_1;
    packetBuffer[offset++] = SYNC_BYTE_2;
    packetBuffer[offset++] = packetType;
    packetBuffer[offset++] = packetSequence++;
    
    uint32_t timestamp = micros();
    packUint32BE(&packetBuffer[offset], timestamp);
    offset += 4;
    
    // EEG data (always present): 5ch × 24-bit = 15 bytes
    for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++)
    {
        float amplitude = 100000.0f;
        float signal = amplitude * sin(TWO_PI * eegFreqs[ch] * simTime);
        signal += (random(-1000, 1000));
        int32_t eegValue = (int32_t)signal;
        packInt24BE(&packetBuffer[offset], eegValue);
        offset += 3;
    }
    
    // Marker (if present): 1 byte
    if (hasMarker)
    {
        packetBuffer[offset++] = markerByte;
    }
    
    // Accel (if present): 3ch × 16-bit = 6 bytes
    if (includeAccel)
    {
        for (int ch = 0; ch < NUM_ACCEL_CHANNELS; ch++)
        {
            packInt16BE(&packetBuffer[offset], accelValues[ch]);
            offset += 2;
        }
    }
    
    // PPG (if present): 3ch × 48-bit = 18 bytes
    if (includePPG)
    {
        for (int ch = 0; ch < NUM_PPG_CHANNELS; ch++)
        {
            packInt48BE(&packetBuffer[offset], ppgValues[ch]);
            offset += 6;
        }
    }
    
    // Health (if present): Temp(2) + Battery(2) = 4 bytes
    if (includeHealth)
    {
        packInt16BE(&packetBuffer[offset], temperature);
        offset += 2;
        packInt16BE(&packetBuffer[offset], battery);
        offset += 2;
    }
    
    // Checksum: XOR of all bytes from start to here
    uint8_t checksum = 0;
    for (int i = 0; i < offset; i++)
    {
        checksum ^= packetBuffer[i];
    }
    packetBuffer[offset++] = checksum;
    
    // Footer
    packetBuffer[offset++] = FOOTER_BYTE_1;
    packetBuffer[offset++] = FOOTER_BYTE_2;
    
    return offset;
}

// ============================================================================
// TIMER INTERRUPT
// ============================================================================

void sampleTimerISR()
{
    dataReady = true;
}

// ============================================================================
// SETUP AND LOOP
// ============================================================================

void setup()
{
    // Initialize USB Serial
    Serial.begin(USB_BAUD);
    
    // Wait for serial connection (with timeout)
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime < 5000))
    {
        // Wait up to 5 seconds
    }
    
    // Initialize random seed
    randomSeed(analogRead(0));
    
    // Setup LED for status
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    
    // Start sample timer at 1kHz
    sampleTimer.begin(sampleTimerISR, 1000000 / SAMPLE_RATE_HZ);
    
    delay(100);
}

void loop()
{
    if (dataReady)
    {
        dataReady = false;
        
        // Determine packet type based on sample number
        bool hasMarkerEvent = false;  // Could be from hardware trigger
        uint8_t packetType = getPacketType(sampleCount, hasMarkerEvent);
        
        // Generate sensor data based on packet type
        if (SIMULATION_MODE)
        {
            generateSimulatedData(packetType);
        }
        else
        {
            // TODO: Read from real hardware
        }
        
        // Build and send packet
        int packetSize = buildPacket(packetType);
        Serial.write(packetBuffer, packetSize);
        
        sampleCount++;
        
        // Toggle LED every 500 samples (2Hz blink)
        static uint16_t ledCounter = 0;
        if (++ledCounter >= 500)
        {
            ledCounter = 0;
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        }
    }
}
