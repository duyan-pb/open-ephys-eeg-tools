/*
    ------------------------------------------------------------------

    InEar Teensy Optimized Protocol Definition
    
    Variable-length packet protocol with enumerated packet types
    for efficient bandwidth usage and reliable delivery.

    Protocol Features:
    - Enumerated packet types for predictable parsing
    - Sequence numbers for gap detection
    - Variable length packets based on data content
    - Checksum validation
    
    Timing (at 1kHz base rate):
    - EEG:    Every packet (1000 Hz)
    - Accel:  Every 4th packet (250 Hz)
    - PPG:    Every 10th packet (100 Hz)
    - Health: Every 100th packet (10 Hz)
    - Sync:   Every 1000th packet (1 Hz) - full data

    ------------------------------------------------------------------
*/

#ifndef OPTIMIZED_PROTOCOL_H
#define OPTIMIZED_PROTOCOL_H

#include <cstdint>

namespace OptimizedProtocol
{

// =============================================================================
// Protocol Constants
// =============================================================================

constexpr uint8_t SYNC_BYTE_1 = 0xA5;
constexpr uint8_t SYNC_BYTE_2 = 0x5A;
constexpr uint8_t FOOTER_BYTE_1 = 0xC0;
constexpr uint8_t FOOTER_BYTE_2 = 0xC0;

constexpr int BAUD_RATE = 2000000;  // 2 Mbps
constexpr int EEG_SAMPLE_RATE = 1000;

// Channel counts
constexpr int NUM_EEG_CHANNELS = 5;
constexpr int NUM_ACCEL_CHANNELS = 3;
constexpr int NUM_PPG_CHANNELS = 3;

// Data sizes in bytes
constexpr int SIZE_EEG = 15;      // 5ch × 24-bit = 15 bytes
constexpr int SIZE_ACCEL = 6;     // 3ch × 16-bit = 6 bytes
constexpr int SIZE_PPG = 18;      // 3ch × 48-bit = 18 bytes
constexpr int SIZE_HEALTH = 4;    // Temp(2) + Battery(2) = 4 bytes
constexpr int SIZE_MARKER = 1;    // 1 byte
constexpr int SIZE_TIMESTAMP = 4; // 4 bytes (microseconds)

// Header: Sync(2) + Type(1) + Seq(1) + Timestamp(4) = 8 bytes
constexpr int SIZE_HEADER = 8;
// Footer: Checksum(1) + Footer(2) = 3 bytes
constexpr int SIZE_FOOTER = 3;

// Scale factors
constexpr float EEG_SCALE_UV = 0.0223517f;    // ADS1299 @ 24x gain: µV per count
constexpr float ACCEL_SCALE_G = 0.000244f;    // ±8g range: G per count
constexpr float PPG_SCALE = 1.0f;             // Raw counts
constexpr float TEMP_SCALE = 0.01f;           // °C per count (firmware sends centi-degrees)
constexpr float BATTERY_SCALE = 1.0f;         // mV per count (firmware sends mV)

// =============================================================================
// Packet Types (Enumerated)
// =============================================================================

// Base packet types (low nibble: 0x00-0x0F)
// High nibble flags: 0x10 = Marker present
enum class PacketType : uint8_t
{
    // Base types (no marker)
    EEG_ONLY           = 0x00,  // EEG data only (most common)
    EEG_ACCEL          = 0x01,  // EEG + Accelerometer (every 4th)
    EEG_PPG            = 0x02,  // EEG + PPG (every 10th, not aligned with accel)
    EEG_ACCEL_PPG      = 0x03,  // EEG + Accel + PPG (every 20th)
    EEG_HEALTH         = 0x04,  // EEG + Temp + Battery (every 100th)
    EEG_ACCEL_HEALTH   = 0x05,  // EEG + Accel + Health (every 100th, aligned with accel)
    EEG_FULL_SYNC      = 0x06,  // Full packet with ALL data (every 1000th)
    
    // With marker flag (0x10 | base type)
    EEG_MARKER         = 0x10,
    EEG_ACCEL_MARKER   = 0x11,
    EEG_PPG_MARKER     = 0x12,
    EEG_ACCEL_PPG_MARKER = 0x13,
    EEG_HEALTH_MARKER  = 0x14,
    EEG_ACCEL_HEALTH_MARKER = 0x15,
    EEG_FULL_SYNC_MARKER = 0x16,
    
    // Special types
    HEARTBEAT          = 0xFE,  // Keep-alive (no data, just seq)
    INVALID            = 0xFF
};

// =============================================================================
// Packet Size Lookup
// =============================================================================

inline int getPacketSize(PacketType type)
{
    // Base size: Header(8) + EEG(15) + Footer(3) = 26 bytes
    int size = SIZE_HEADER + SIZE_EEG + SIZE_FOOTER;
    
    // Check for marker flag
    if ((static_cast<uint8_t>(type) & 0x10) != 0)
    {
        size += SIZE_MARKER;
    }
    
    // Get base type (strip marker flag)
    uint8_t baseType = static_cast<uint8_t>(type) & 0x0F;
    
    switch (baseType)
    {
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
            return -1; // Invalid type
    }
    
    return size;
}

// Maximum packet size (full sync with marker)
constexpr int MAX_PACKET_SIZE = SIZE_HEADER + SIZE_EEG + SIZE_MARKER + 
                                 SIZE_ACCEL + SIZE_PPG + SIZE_HEALTH + SIZE_FOOTER;
// = 8 + 15 + 1 + 6 + 18 + 4 + 3 = 55 bytes

// Minimum packet size (EEG only)
constexpr int MIN_PACKET_SIZE = SIZE_HEADER + SIZE_EEG + SIZE_FOOTER;
// = 8 + 15 + 3 = 26 bytes

// =============================================================================
// Packet Structure
// =============================================================================

#pragma pack(push, 1)

struct PacketHeader
{
    uint8_t sync1;          // 0xA5
    uint8_t sync2;          // 0x5A
    uint8_t packetType;     // PacketType enum
    uint8_t sequence;       // 0-255, wrapping counter
    uint32_t timestamp_us;  // Microseconds since boot (big-endian)
};

struct PacketFooter
{
    uint8_t checksum;       // XOR of all bytes (header to before checksum)
    uint8_t footer1;        // 0xC0
    uint8_t footer2;        // 0xC0
};

#pragma pack(pop)

// =============================================================================
// Data Content Flags (for receiver state)
// =============================================================================

inline bool hasAccel(PacketType type)
{
    uint8_t base = static_cast<uint8_t>(type) & 0x0F;
    return base == 0x01 || base == 0x03 || base == 0x05 || base == 0x06;
}

inline bool hasPPG(PacketType type)
{
    uint8_t base = static_cast<uint8_t>(type) & 0x0F;
    return base == 0x02 || base == 0x03 || base == 0x06;
}

inline bool hasHealth(PacketType type)
{
    uint8_t base = static_cast<uint8_t>(type) & 0x0F;
    return base == 0x04 || base == 0x05 || base == 0x06;
}

inline bool hasMarker(PacketType type)
{
    return (static_cast<uint8_t>(type) & 0x10) != 0;
}

inline bool isFullSync(PacketType type)
{
    uint8_t base = static_cast<uint8_t>(type) & 0x0F;
    return base == 0x06;
}

// =============================================================================
// Checksum Calculation
// =============================================================================

inline uint8_t computeChecksum(const uint8_t* data, int length)
{
    uint8_t checksum = 0;
    for (int i = 0; i < length; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

// =============================================================================
// Timing Helpers
// =============================================================================

// Determine packet type based on sample number
inline PacketType getPacketTypeForSample(uint32_t sampleNum, bool hasMarkerEvent)
{
    uint8_t baseType;
    
    if (sampleNum % 1000 == 0)
    {
        // Full sync every 1000 samples (1 Hz)
        baseType = 0x06;
    }
    else if (sampleNum % 100 == 0)
    {
        // Health data every 100 samples (10 Hz)
        // Also include accel if aligned
        if (sampleNum % 4 == 0)
            baseType = 0x05; // EEG_ACCEL_HEALTH
        else
            baseType = 0x04; // EEG_HEALTH
    }
    else if (sampleNum % 20 == 0)
    {
        // Both accel (250Hz) and PPG (100Hz) aligned
        baseType = 0x03; // EEG_ACCEL_PPG
    }
    else if (sampleNum % 10 == 0)
    {
        // PPG every 10 samples (100 Hz)
        baseType = 0x02; // EEG_PPG
    }
    else if (sampleNum % 4 == 0)
    {
        // Accel every 4 samples (250 Hz)
        baseType = 0x01; // EEG_ACCEL
    }
    else
    {
        // EEG only
        baseType = 0x00;
    }
    
    // Add marker flag if event present
    if (hasMarkerEvent)
    {
        baseType |= 0x10;
    }
    
    return static_cast<PacketType>(baseType);
}

} // namespace OptimizedProtocol

#endif // OPTIMIZED_PROTOCOL_H
