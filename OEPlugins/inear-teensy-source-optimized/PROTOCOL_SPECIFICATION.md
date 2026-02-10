# InEar Teensy Optimized Protocol Specification

Technical specification for the InEar Teensy **Optimized** variable-length packet protocol used between Teensy 4.1 firmware and Open Ephys plugin.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Protocol Comparison](#2-protocol-comparison)
3. [Packet Types](#3-packet-types)
4. [Packet Structure](#4-packet-structure)
5. [Data Encoding](#5-data-encoding)
6. [Multi-Rate Sampling](#6-multi-rate-sampling)
7. [Bandwidth Analysis](#7-bandwidth-analysis)
8. [Error Detection](#8-error-detection)
9. [Implementation Notes](#9-implementation-notes)

---

## 1. Overview

The Optimized Protocol is a variable-length binary streaming protocol designed for efficient real-time EEG acquisition. It reduces bandwidth by ~49% compared to the original fixed-length protocol by only transmitting auxiliary sensor data at their native sample rates.

| Parameter | Value |
|-----------|-------|
| **Sample Rate** | 1000 Hz (EEG base rate) |
| **Packet Size** | 26-55 bytes (variable) |
| **Baud Rate** | 2,000,000 (2 Mbaud) |
| **Avg Bandwidth** | ~28.5 KB/s (measured) |
| **Original Bandwidth** | 56.0 KB/s (56 bytes × 1000 Hz) |
| **Savings** | ~49% |
| **Latency** | < 1 ms (single packet) |

---

## 2. Protocol Comparison

### Original vs Optimized

| Feature | Original Protocol | Optimized Protocol |
|---------|-------------------|-------------------|
| Packet Size | Fixed 56 bytes | Variable 26-55 bytes |
| Auxiliary Data | Every packet (1000 Hz) | Native rates (10-250 Hz) |
| Packet Type Field | None | Enumerated byte |
| Sequence Number | Counter at end | In header |
| Timestamp | 4 bytes | 4 bytes (microseconds) |
| Average Bandwidth | 56,000 bytes/sec | ~28,500 bytes/sec |
| Bandwidth Savings | - | ~49% |

### Why Variable Length?

Auxiliary sensors don't need 1 kHz sampling:

| Sensor | Nyquist Limit | Original Rate | Optimized Rate | Savings |
|--------|---------------|---------------|----------------|---------|
| EEG | 500 Hz | 1000 Hz | 1000 Hz | None |
| Accel | 125 Hz | 1000 Hz | 250 Hz | 75% |
| PPG | 50 Hz | 1000 Hz | 100 Hz | 90% |
| Temp | 0.5 Hz | 1000 Hz | 10 Hz | 99% |
| Battery | 0.1 Hz | 1000 Hz | 10 Hz | 99% |

---

## 3. Packet Types

### 3.1 Type Enumeration

The packet type byte determines what data is included:

```
Bit 7-5: Reserved (0)
Bit 4:   Marker flag (1 = marker byte present)
Bit 3-0: Base type (0x00-0x06)
```

### 3.2 Base Types

| Code | Name | Size | Contents |
|------|------|------|----------|
| 0x00 | EEG_ONLY | 26B | Header + EEG + Footer |
| 0x01 | EEG_ACCEL | 32B | + Accelerometer (6B) |
| 0x02 | EEG_PPG | 44B | + PPG sensors (18B) |
| 0x03 | EEG_ACCEL_PPG | 50B | + Accel + PPG |
| 0x04 | EEG_HEALTH | 30B | + Temp + Battery (4B) |
| 0x05 | EEG_ACCEL_HEALTH | 36B | + Accel + Health |
| 0x06 | EEG_FULL_SYNC | 54B | All sensor data |

### 3.3 Marker Flag

Add 0x10 to any base type to include a 1-byte marker:

| Code | Name | Size |
|------|------|------|
| 0x10 | EEG_MARKER | 27B |
| 0x11 | EEG_ACCEL_MARKER | 33B |
| 0x12 | EEG_PPG_MARKER | 45B |
| 0x13 | EEG_ACCEL_PPG_MARKER | 51B |
| 0x14 | EEG_HEALTH_MARKER | 31B |
| 0x15 | EEG_ACCEL_HEALTH_MARKER | 37B |
| 0x16 | EEG_FULL_SYNC_MARKER | 55B |

### 3.4 Special Types

| Code | Name | Description |
|------|------|-------------|
| 0xFE | HEARTBEAT | Keep-alive (no data) |
| 0xFF | INVALID | Error marker |

---

## 4. Packet Structure

### 4.1 General Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│ HEADER (8B) │ EEG (15B) │ [MARKER 1B] │ [ACCEL 6B] │ [PPG 18B] │ [HEALTH 4B] │ FOOTER (3B) │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Header (8 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1B | Sync1 | 0xA5 |
| 1 | 1B | Sync2 | 0x5A |
| 2 | 1B | PacketType | Type enum (see above) |
| 3 | 1B | Sequence | 0-255 wrapping counter |
| 4 | 4B | Timestamp | Microseconds since boot (Big Endian) |

### 4.3 EEG Data (15 bytes) - Always Present

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 8 | 3B | EEG1 | Channel 1, 24-bit signed BE |
| 11 | 3B | EEG2 | Channel 2, 24-bit signed BE |
| 14 | 3B | EEG3 | Channel 3, 24-bit signed BE |
| 17 | 3B | EEG4 | Channel 4, 24-bit signed BE |
| 20 | 3B | EEG5 | Channel 5, 24-bit signed BE |

### 4.4 Marker (1 byte) - Optional

Present only when PacketType has bit 4 set (0x10 flag).

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 23* | 1B | Marker | Event trigger byte |

*Offset varies based on packet type

### 4.5 Accelerometer (6 bytes) - Optional

Present in types: EEG_ACCEL, EEG_ACCEL_PPG, EEG_ACCEL_HEALTH, EEG_FULL_SYNC

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| +0 | 2B | AccelX | X-axis, 16-bit signed BE |
| +2 | 2B | AccelY | Y-axis, 16-bit signed BE |
| +4 | 2B | AccelZ | Z-axis, 16-bit signed BE |

### 4.6 PPG Data (18 bytes) - Optional

Present in types: EEG_PPG, EEG_ACCEL_PPG, EEG_FULL_SYNC

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| +0 | 6B | PPG_Red | Red LED, 48-bit unsigned BE |
| +6 | 6B | PPG_IR | IR LED, 48-bit unsigned BE |
| +12 | 6B | PPG_Green | Green LED, 48-bit unsigned BE |

### 4.7 Health Data (4 bytes) - Optional

Present in types: EEG_HEALTH, EEG_ACCEL_HEALTH, EEG_FULL_SYNC

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| +0 | 2B | Temperature | 16-bit signed BE (0.01°C units) |
| +2 | 2B | Battery | 16-bit unsigned BE (mV) |

### 4.8 Footer (3 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| -3 | 1B | Checksum | XOR of all preceding bytes |
| -2 | 1B | Footer1 | 0xC0 |
| -1 | 1B | Footer2 | 0xC0 |

---

## 5. Data Encoding

### 5.1 Byte Order

All multi-byte values are **Big Endian** (network byte order).

### 5.2 EEG Channel Encoding (24-bit signed, Big-Endian)

Each EEG channel uses 3 bytes in two's complement format:

```
Byte 0: MSB (includes sign bit)
Byte 1: Middle byte
Byte 2: LSB

Value = (Byte0 << 16) | (Byte1 << 8) | Byte2

If Byte0 & 0x80:  # Negative number
    Value = Value - 0x1000000  # Convert from unsigned to signed
```

**Conversion to microvolts:**
```
voltage_µV = raw_24bit × 0.0223517
```

### 5.3 Accelerometer Encoding (16-bit signed, Big-Endian)

```
Value = (Byte0 << 8) | Byte1
If Byte0 & 0x80:
    Value = Value - 0x10000  # Two's complement
```

**Conversion to G:**
```
accel_G = raw_16bit × 0.000244  # At ±8g range
```

### 5.4 PPG Encoding (48-bit unsigned, Big-Endian)

Each PPG channel uses 6 bytes:

```
Value = (B0 << 40) | (B1 << 32) | (B2 << 24) | (B3 << 16) | (B4 << 8) | B5
```

### 5.5 Temperature Encoding (16-bit signed, Big-Endian)

```
temperature_C = raw_16bit / 100.0  # Stored as centi-degrees
```

### 5.6 Battery Encoding (16-bit unsigned, Big-Endian)

```
battery_V = raw_16bit / 1000.0  # Stored as millivolts
```

---

## 6. Multi-Rate Sampling

### 6.1 Sample Rate Schedule

The packet type varies based on the sample number:

```cpp
uint8_t getPacketType(uint32_t sampleNum, bool hasMarker)
{
    uint8_t baseType;
    
    if (sampleNum % 1000 == 0)
        baseType = EEG_FULL_SYNC;      // 1 Hz - all data
    else if (sampleNum % 100 == 0)
        baseType = EEG_ACCEL_HEALTH;   // 10 Hz - health + accel
    else if (sampleNum % 20 == 0)
        baseType = EEG_ACCEL_PPG;      // 50 Hz - both aligned
    else if (sampleNum % 10 == 0)
        baseType = EEG_PPG;            // 100 Hz - PPG
    else if (sampleNum % 4 == 0)
        baseType = EEG_ACCEL;          // 250 Hz - accel
    else
        baseType = EEG_ONLY;           // 1000 Hz - EEG only
    
    return hasMarker ? (baseType | 0x10) : baseType;
}
```

### 6.2 Packet Distribution (per second)

| Type | Count/sec | Bytes/packet | Bytes/sec |
|------|-----------|--------------|-----------|
| EEG_ONLY | 749 | 26 | 19,474 |
| EEG_ACCEL | 200 | 32 | 6,400 |
| EEG_PPG | 40 | 44 | 1,760 |
| EEG_ACCEL_PPG | 9 | 50 | 450 |
| EEG_ACCEL_HEALTH | 1 | 36 | 36 |
| EEG_FULL_SYNC | 1 | 54 | 54 |
| **Total** | **1000** | - | **~28,174** |

*Actual rates vary slightly due to alignment*

---

## 7. Bandwidth Analysis

### 7.1 Comparison

| Protocol | Packets/sec | Bytes/sec | Efficiency |
|----------|-------------|-----------|------------|
| Original (fixed 56B) | 1000 | 56,000 | 100% (baseline) |
| Optimized (variable) | 1000 | ~33,000 | 59% (~41% savings) |

### 7.2 Serial Throughput

At 2 Mbaud (2,000,000 bits/sec) with 8-N-1 encoding:
- Serial overhead: 10 bits per byte (1 start + 8 data + 1 stop)
- Maximum throughput: 2,000,000 / 10 = 200,000 bytes/sec
- Optimized usage: ~28,500 bytes/sec (14.3%)
- Original usage: 56,000 bytes/sec (28.0%)

Both protocols have significant headroom for reliability.

---

## 8. Error Detection

### 8.1 Sync Bytes

Receiver should scan for 0xA5 0x5A pattern. False positives are rare:
- Probability of random occurrence: 1/65536

### 8.2 Packet Type Validation

Valid packet types are 0x00-0x06 and 0x10-0x16. Invalid types should trigger resync.

### 8.3 Checksum

XOR of all bytes from offset 0 to (packet_size - 4):

```cpp
uint8_t computeChecksum(const uint8_t* packet, int size)
{
    uint8_t checksum = 0;
    for (int i = 0; i < size - 3; i++)  // Exclude checksum and footer
        checksum ^= packet[i];
    return checksum;
}
```

### 8.4 Footer Validation

Packet must end with 0xC0 0xC0.

### 8.5 Sequence Gap Detection

If `(current_seq - last_seq) != 1 (mod 256)`, packets were lost.

---

## 9. Implementation Notes

### 9.1 Receiver State Machine

```
┌─────────┐   0xA5   ┌─────────┐   0x5A   ┌─────────┐
│  IDLE   │ ──────► │ SYNC_1  │ ──────► │ HEADER  │
└─────────┘         └─────────┘         └────┬────┘
     ▲                   │                   │
     │    not 0x5A       │    read type      │
     └───────────────────┘                   ▼
                                       ┌─────────┐
     ┌───────────────────┐             │  DATA   │
     │     VALIDATE      │◄────────────┴─────────┘
     └────────┬──────────┘    read packet_size
              │
              │ checksum OK
              ▼
        ┌───────────┐
        │  PROCESS  │
        └───────────┘
```

### 9.2 Sample-and-Hold for Aux Channels

Since aux data arrives at lower rates, the receiver should hold the last value:

```cpp
// Update accel only when present in packet
if (hasAccel(packetType))
{
    accelState.x = parseAccelX(packet);
    accelState.y = parseAccelY(packet);
    accelState.z = parseAccelZ(packet);
}
// Otherwise use last known values

// Output all channels at 1 kHz
auxBuffer[0] = accelState.x;
auxBuffer[1] = accelState.y;
auxBuffer[2] = accelState.z;
```

### 9.3 C++ Header Reference

See `OptimizedProtocol.h` for complete implementation including:
- Packet type enum
- Size lookup functions
- Content flag helpers
- Scale factors
