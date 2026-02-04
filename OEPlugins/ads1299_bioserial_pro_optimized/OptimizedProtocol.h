/*
    ===========================================================================
    
    ADS1299 BioSerial Pro - Optimized Variable-Length Protocol
    
    Teensy 4.0 firmware for InEar EEG device with optimized transmission.
    
    Features:
    - Variable-length packets based on data availability
    - Multi-rate sampling (EEG@1kHz, Accel@250Hz, PPG@100Hz, Health@10Hz)
    - Enumerated packet types for efficient parsing
    - Sequence numbers for gap detection
    - XOR checksum validation
    
    Protocol:
    - EEG:    Every packet (1000 Hz)
    - Accel:  Every 4th packet (250 Hz)
    - PPG:    Every 10th packet (100 Hz)  
    - Health: Every 100th packet (10 Hz)
    - Sync:   Every 1000th packet (1 Hz) - full data
    
    ===========================================================================
*/

#ifndef OPTIMIZED_PROTOCOL_H
#define OPTIMIZED_PROTOCOL_H

#include <Arduino.h>

// =============================================================================
// Protocol Constants
// =============================================================================

#define SYNC_BYTE_1         0xA5
#define SYNC_BYTE_2         0x5A
#define FOOTER_BYTE_1       0xC0
#define FOOTER_BYTE_2       0xC0

#define SERIAL_BAUD_RATE    2000000  // 2 Mbps

// Channel counts
#define NUM_EEG_CHANNELS    5
#define NUM_ACCEL_CHANNELS  3
#define NUM_PPG_CHANNELS    3

// Data sizes in bytes
#define SIZE_EEG            15   // 5ch × 24-bit = 15 bytes
#define SIZE_ACCEL          6    // 3ch × 16-bit = 6 bytes
#define SIZE_PPG            18   // 3ch × 48-bit = 18 bytes
#define SIZE_HEALTH         4    // Temp(2) + Battery(2) = 4 bytes
#define SIZE_MARKER         1
#define SIZE_TIMESTAMP      4

// Header: Sync(2) + Type(1) + Seq(1) + Timestamp(4) = 8 bytes
#define SIZE_HEADER         8
// Footer: Checksum(1) + Footer(2) = 3 bytes
#define SIZE_FOOTER         3

// Maximum packet size
#define MAX_PACKET_SIZE     (SIZE_HEADER + SIZE_EEG + SIZE_MARKER + SIZE_ACCEL + SIZE_PPG + SIZE_HEALTH + SIZE_FOOTER)

// Sampling intervals (in samples at 1kHz base rate)
#define ACCEL_INTERVAL      4     // 250 Hz
#define PPG_INTERVAL        10    // 100 Hz
#define HEALTH_INTERVAL     100   // 10 Hz
#define SYNC_INTERVAL       1000  // 1 Hz full sync

// =============================================================================
// Packet Types
// =============================================================================

typedef enum {
    // Base types (no marker)
    PKT_EEG_ONLY           = 0x00,  // EEG data only (most common)
    PKT_EEG_ACCEL          = 0x01,  // EEG + Accelerometer
    PKT_EEG_PPG            = 0x02,  // EEG + PPG
    PKT_EEG_ACCEL_PPG      = 0x03,  // EEG + Accel + PPG
    PKT_EEG_HEALTH         = 0x04,  // EEG + Temp + Battery
    PKT_EEG_ACCEL_HEALTH   = 0x05,  // EEG + Accel + Health
    PKT_EEG_FULL_SYNC      = 0x06,  // Full packet with ALL data
    
    // With marker flag (0x10 | base type)
    PKT_EEG_MARKER         = 0x10,
    PKT_EEG_ACCEL_MARKER   = 0x11,
    PKT_EEG_PPG_MARKER     = 0x12,
    PKT_EEG_ACCEL_PPG_MARKER = 0x13,
    PKT_EEG_HEALTH_MARKER  = 0x14,
    PKT_EEG_ACCEL_HEALTH_MARKER = 0x15,
    PKT_EEG_FULL_SYNC_MARKER = 0x16,
    
    // Special
    PKT_HEARTBEAT          = 0xFE,
    PKT_INVALID            = 0xFF
} PacketType;

// =============================================================================
// Data Structures
// =============================================================================

typedef struct {
    int32_t eeg[NUM_EEG_CHANNELS];       // Raw 24-bit values
    int16_t accel[NUM_ACCEL_CHANNELS];   // Raw 16-bit values
    int64_t ppg[NUM_PPG_CHANNELS];       // Raw 48-bit values
    int16_t temperature;                  // Raw 16-bit value
    int16_t battery;                      // Raw 16-bit value
    uint8_t marker;                       // Event marker (0 = no event)
} SensorData;

// =============================================================================
// Helper Functions
// =============================================================================

// Determine packet type based on sample number and marker
inline PacketType getPacketType(uint32_t sampleNum, bool hasMarker) {
    uint8_t baseType;
    
    if (sampleNum % SYNC_INTERVAL == 0) {
        // Full sync every 1000 samples
        baseType = PKT_EEG_FULL_SYNC;
    }
    else if (sampleNum % HEALTH_INTERVAL == 0) {
        // Health data every 100 samples
        if (sampleNum % ACCEL_INTERVAL == 0)
            baseType = PKT_EEG_ACCEL_HEALTH;
        else
            baseType = PKT_EEG_HEALTH;
    }
    else if (sampleNum % ACCEL_INTERVAL == 0 && sampleNum % PPG_INTERVAL == 0) {
        // Both accel and PPG aligned (every 20 samples)
        baseType = PKT_EEG_ACCEL_PPG;
    }
    else if (sampleNum % PPG_INTERVAL == 0) {
        // PPG every 10 samples
        baseType = PKT_EEG_PPG;
    }
    else if (sampleNum % ACCEL_INTERVAL == 0) {
        // Accel every 4 samples
        baseType = PKT_EEG_ACCEL;
    }
    else {
        // EEG only
        baseType = PKT_EEG_ONLY;
    }
    
    // Add marker flag if needed
    if (hasMarker) {
        baseType |= 0x10;
    }
    
    return (PacketType)baseType;
}

// Get expected packet size for a type
inline int getPacketSize(PacketType type) {
    int size = SIZE_HEADER + SIZE_EEG + SIZE_FOOTER;
    
    // Check marker flag
    if ((type & 0x10) != 0) {
        size += SIZE_MARKER;
    }
    
    // Get base type
    uint8_t baseType = type & 0x0F;
    
    switch (baseType) {
        case 0x00: // EEG_ONLY
            break;
        case 0x01: // EEG_ACCEL
            size += SIZE_ACCEL;
            break;
        case 0x02: // EEG_PPG
            size += SIZE_PPG;
            break;
        case 0x03: // EEG_ACCEL_PPG
            size += SIZE_ACCEL + SIZE_PPG;
            break;
        case 0x04: // EEG_HEALTH
            size += SIZE_HEALTH;
            break;
        case 0x05: // EEG_ACCEL_HEALTH
            size += SIZE_ACCEL + SIZE_HEALTH;
            break;
        case 0x06: // EEG_FULL_SYNC
            size += SIZE_ACCEL + SIZE_PPG + SIZE_HEALTH;
            break;
        default:
            return -1;
    }
    
    return size;
}

// Check if packet type includes accelerometer
inline bool hasAccel(PacketType type) {
    uint8_t base = type & 0x0F;
    return base == 0x01 || base == 0x03 || base == 0x05 || base == 0x06;
}

// Check if packet type includes PPG
inline bool hasPPG(PacketType type) {
    uint8_t base = type & 0x0F;
    return base == 0x02 || base == 0x03 || base == 0x06;
}

// Check if packet type includes health data
inline bool hasHealth(PacketType type) {
    uint8_t base = type & 0x0F;
    return base == 0x04 || base == 0x05 || base == 0x06;
}

// Check if packet type includes marker
inline bool hasMarker(PacketType type) {
    return (type & 0x10) != 0;
}

#endif // OPTIMIZED_PROTOCOL_H
