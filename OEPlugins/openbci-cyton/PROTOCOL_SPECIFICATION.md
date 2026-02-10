# OpenBCI Cyton Protocol Specification

Technical specification for the OpenBCI Cyton EEG streaming protocol used between the Cyton board and the Open Ephys plugin via RFDuino wireless link.

> **Source**: Based on official [OpenBCI Cyton Data Format](https://docs.openbci.com/Cyton/CytonDataFormat/) and [SDK](https://docs.openbci.com/Cyton/CytonSDK/) documentation.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Hardware Architecture](#2-hardware-architecture)
3. [Channel Configuration](#3-channel-configuration)
4. [Packet Structure](#4-packet-structure)
5. [Packet Types](#5-packet-types)
6. [Data Encoding](#6-data-encoding)
7. [Command Protocol](#7-command-protocol)
8. [Daisy Mode (16 Channels)](#8-daisy-mode-16-channels)
9. [Timing & Bandwidth](#9-timing--bandwidth)
10. [Implementation Notes](#10-implementation-notes)

---

## 1. Overview

OpenBCI Cyton is a wireless EEG streaming protocol using RFDuino radio modules. The Cyton board transmits data to a USB dongle which presents as a virtual serial port to the host computer.

| Parameter | Value |
|-----------|-------|
| **Sample Rate** | 250 Hz (hardware limited by radio) |
| **Packet Size** | 33 bytes |
| **Baud Rate** | 115200 (default), up to 921600 |
| **Data Throughput** | ~8.25 KB/s |
| **Latency** | ~4-8 ms (radio + USB) |
| **Channels** | 8 (Cyton) or 16 (Cyton + Daisy) |

---

## 2. Hardware Architecture

### 2.1 System Components

```
┌─────────────┐     Radio      ┌─────────────┐     USB      ┌──────────┐
│ Cyton Board │ ─────────────> │ RFDuino     │ ──────────> │ Host PC  │
│ (Device)    │   2.4 GHz      │ Dongle      │  115200     │          │
│             │                │ (Host)      │   baud      │          │
└─────────────┘                └─────────────┘              └──────────┘
```

### 2.2 Cyton Board

| Component | Part Number | Function |
|-----------|-------------|----------|
| MCU | PIC32MX250F128B | Main processor, protocol handling |
| ADC | ADS1299 | 8-channel 24-bit EEG analog front-end |
| Accelerometer | LIS3DH | 3-axis accelerometer, ±4g range |
| Radio | RFduino (nRF51822) | 2.4 GHz wireless (Nordic Gazell) |
| SD Card | MicroSD (optional) | Local data logging |

### 2.3 Radio Protocol

The RFduino modules use the Nordic Gazell protocol stack:

- **Frequency**: 2.4 GHz ISM band
- **Channels**: 1-25 (configurable)
- **Max Payload**: 32 bytes over-the-air
- **Host adds**: Header byte (0xA0) and footer byte (0xCX)

The over-the-air packet is 31 bytes; the dongle adds framing to create 33-byte serial packets.

---

## 3. Channel Configuration

### 3.1 EEG Channels (8 channels per ADS1299)

High-resolution electrophysiology channels from the ADS1299 analog front-end.

| Channel | Pin | Default Function | Resolution | Range | Units |
|---------|-----|------------------|------------|-------|-------|
| 1 | N1P/SRB2 | EEG Channel 1 | 24-bit signed | ±187.5 mV | µV |
| 2 | N2P/SRB2 | EEG Channel 2 | 24-bit signed | ±187.5 mV | µV |
| 3 | N3P/SRB2 | EEG Channel 3 | 24-bit signed | ±187.5 mV | µV |
| 4 | N4P/SRB2 | EEG Channel 4 | 24-bit signed | ±187.5 mV | µV |
| 5 | N5P/SRB2 | EEG Channel 5 | 24-bit signed | ±187.5 mV | µV |
| 6 | N6P/SRB2 | EEG Channel 6 | 24-bit signed | ±187.5 mV | µV |
| 7 | N7P/SRB2 | EEG Channel 7 | 24-bit signed | ±187.5 mV | µV |
| 8 | N8P/SRB2 | EEG Channel 8 | 24-bit signed | ±187.5 mV | µV |

**ADS1299 Specifications:**
- ADC Resolution: 24 bits (16,777,216 levels)
- Input-referred noise: < 1 µVpp
- CMRR: -110 dB
- Programmable gain: 1x, 2x, 4x, 6x, 8x, 12x, 24x
- Default gain: 24x

**Scale Factor Calculation:**
```
Scale Factor (V/count) = 4.5V / gain / (2^23 - 1)

Gain    Scale Factor (µV/count)    Range (mV)
----    -----------------------    ----------
1x      0.5364                     ±4500
2x      0.2682                     ±2250
4x      0.1341                     ±1125
6x      0.0894                     ±750
8x      0.0671                     ±562.5
12x     0.0447                     ±375
24x     0.02235                    ±187.5
```

### 3.2 Accelerometer Channels (LIS3DH)

| Axis | Description | Resolution | Range | Units |
|------|-------------|------------|-------|-------|
| X | Lateral acceleration | 16-bit signed | ±4g | g × 0.000125 |
| Y | Longitudinal acceleration | 16-bit signed | ±4g | g × 0.000125 |
| Z | Vertical acceleration | 16-bit signed | ±4g | g × 0.000125 |

**Accelerometer Specifications:**
- Sample rate: 25 Hz (interleaved with EEG at 250 Hz)
- Scale factor: 0.002 / 16 = 0.000125 g/count
- Used for motion artifact detection and head position

---

## 4. Packet Structure

Each packet is exactly **33 bytes** containing one sample from all 8 EEG channels plus auxiliary data.

```
Offset  Size  Field           Description
------  ----  --------------  ----------------------------------
0       1     Header          0xA0 (sync byte)
1       1     Sample Number   Counter 0-255 (wraps)
2-4     3     EEG Channel 1   24-bit signed, Big-Endian
5-7     3     EEG Channel 2   24-bit signed, Big-Endian
8-10    3     EEG Channel 3   24-bit signed, Big-Endian
11-13   3     EEG Channel 4   24-bit signed, Big-Endian
14-16   3     EEG Channel 5   24-bit signed, Big-Endian
17-19   3     EEG Channel 6   24-bit signed, Big-Endian
20-22   3     EEG Channel 7   24-bit signed, Big-Endian
23-25   3     EEG Channel 8   24-bit signed, Big-Endian
26-31   6     Aux Data        Format depends on footer byte
32      1     Footer          0xCX (packet type indicator)
------  ----  --------------  ----------------------------------
Total: 33 bytes
```

### 4.1 Visual Representation

```
┌──────┬─────┬─────────────────────────────────────────┬──────────┬──────┐
│ 0xA0 │ Seq │ Ch1 │ Ch2 │ Ch3 │ Ch4 │ Ch5 │ Ch6 │ Ch7 │ Ch8 │ Aux[6] │ 0xCX │
└──────┴─────┴─────────────────────────────────────────┴──────────┴──────┘
   1      1     3     3     3     3     3     3     3     3      6      1   = 33 bytes
```

---

## 5. Packet Types

The footer byte (byte 32) indicates the packet type and how to interpret the aux bytes (26-31).

### 5.1 Firmware v1.0.0 (Legacy)

| Footer | Aux Bytes | Description |
|--------|-----------|-------------|
| `0xC0` | AX1,AX0,AY1,AY0,AZ1,AZ0 | Accelerometer data (3 × 16-bit) |

### 5.2 Firmware v2.0.0+ (Current)

| Footer | Byte 26 | Byte 27 | Bytes 28-31 | Description |
|--------|---------|---------|-------------|-------------|
| `0xC0` | AX1 | AX0 | AY1,AY0,AZ1,AZ0 | Standard with accelerometer |
| `0xC1` | UDF | UDF | UDF,UDF,UDF,UDF | Standard with raw aux |
| `0xC2` | UDF | UDF | UDF,UDF,UDF,UDF | User defined |
| `0xC3` | AC | AV | T3,T2,T1,T0 | Timestamp SET + interleaved accel |
| `0xC4` | AC | AV | T3,T2,T1,T0 | Timestamp + interleaved accel |
| `0xC5` | UDF | UDF | T3,T2,T1,T0 | Timestamp SET + raw aux |
| `0xC6` | UDF | UDF | T3,T2,T1,T0 | Timestamp + raw aux |

**Legend:**
- `AX`, `AY`, `AZ`: Accelerometer axes (16-bit Big-Endian)
- `AC`: Accelerometer code ('X', 'x', 'Y', 'y', 'Z', 'z')
- `AV`: Accelerometer value (high or low byte)
- `T3-T0`: 32-bit timestamp (ms since board start)
- `UDF`: User-defined

### 5.3 Interleaved Accelerometer (0xC3, 0xC4)

To fit timestamps, accelerometer data is sent across multiple packets:

| AC Code | AV Contains | Action |
|---------|-------------|--------|
| 'X' | AX high byte | Store, wait for 'x' |
| 'x' | AX low byte | Combine with stored high byte |
| 'Y' | AY high byte | Store, wait for 'y' |
| 'y' | AY low byte | Combine with stored high byte |
| 'Z' | AZ high byte | Store, wait for 'z' |
| 'z' | AZ low byte | Combine, accelerometer sample complete |

**Timing**: Complete accelerometer sample every 6 packets (~24 ms at 250 Hz).

### 5.4 Timestamp SET vs Regular

- `0xC3` / `0xC5` (SET): Sent in response to `<` sync command
- `0xC4` / `0xC6` (Regular): Normal timestamped packets

Use SET packets to calculate round-trip latency.

---

## 6. Data Encoding

### 6.1 24-bit Signed Integer (EEG Data)

EEG channels are stored as 24-bit two's complement, Big-Endian:

```c
int32_t interpret24BitAsInt32(uint8_t* bytes) {
    int32_t value = ((0xFF & bytes[0]) << 16) |
                    ((0xFF & bytes[1]) << 8) |
                    (0xFF & bytes[2]);
    
    // Sign extend if bit 23 is set
    if (value & 0x00800000) {
        value |= 0xFF000000;
    }
    
    return value;
}
```

**Example:**
```
Raw bytes: [0x7F, 0xFF, 0xFF]  = +8,388,607 (maximum positive)
Raw bytes: [0x80, 0x00, 0x00]  = -8,388,608 (maximum negative)
Raw bytes: [0xFF, 0xFF, 0xFF]  = -1
Raw bytes: [0x00, 0x00, 0x01]  = +1
```

### 6.2 16-bit Signed Integer (Accelerometer)

Accelerometer axes are 16-bit signed, Big-Endian:

```c
int16_t interpret16BitAsInt16(uint8_t* bytes) {
    return (int16_t)((bytes[0] << 8) | bytes[1]);
}
```

### 6.3 32-bit Unsigned Integer (Timestamp)

Timestamp is milliseconds since board power-on, Big-Endian:

```c
uint32_t interpretTimestamp(uint8_t* bytes) {
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}
```

At 250 Hz, wraps every ~49.7 days.

### 6.4 Sample Number

8-bit unsigned counter (0-255), wraps every 256 samples (~1.024 seconds).

Used to detect dropped packets:
```c
uint8_t expected = (lastSampleNumber + 1) & 0xFF;
if (currentSampleNumber != expected) {
    int dropped = (currentSampleNumber - expected + 256) % 256;
    LOGW("Dropped %d packets", dropped);
}
```

---

## 7. Command Protocol

### 7.1 Command Format

Commands are single ASCII characters or multi-byte sequences. The board responds with ASCII text ending in `$$$`.

### 7.2 Basic Commands

| Command | Description | Response |
|---------|-------------|----------|
| `b` | Start streaming | None (starts binary data) |
| `s` | Stop streaming | None |
| `v` | Soft reset | Firmware info + `$$$` |
| `?` | Query registers | Register dump + `$$$` |
| `d` | Restore defaults | `updating channel settings to default$$$` |
| `D` | Report defaults | 6 ASCII chars + `$$$` |

### 7.3 Channel Control

**Turn channels OFF (1-8):**
```
1 2 3 4 5 6 7 8
```

**Turn channels ON (1-8):**
```
! @ # $ % ^ & *
```

### 7.4 Channel Configuration

**Format:** `x(CHANNEL)(POWER)(GAIN)(INPUT)(BIAS)(SRB2)(SRB1)X`

| Position | Values | Description |
|----------|--------|-------------|
| CHANNEL | 1-8, Q-I | Channel number (Q-I for Daisy 9-16) |
| POWER | 0/1 | 0=ON, 1=OFF |
| GAIN | 0-6 | 0=1x, 1=2x, 2=4x, 3=6x, 4=8x, 5=12x, 6=24x |
| INPUT | 0-7 | Input type (0=normal, 5=test signal) |
| BIAS | 0/1 | Include in bias (0=no, 1=yes) |
| SRB2 | 0/1 | Connect to SRB2 (0=no, 1=yes) |
| SRB1 | 0/1 | Connect to SRB1 (0=no, 1=yes) |

**Example:**
```
x3060110X  // Channel 3, ON, 24x gain, normal input, bias yes, SRB2 yes, SRB1 no
```

### 7.5 Test Signals

| Command | Signal |
|---------|--------|
| `0` | Connect inputs to internal GND |
| `-` | Test signal 1x amplitude, slow pulse |
| `=` | Test signal 1x amplitude, fast pulse |
| `p` | Connect to DC signal |
| `[` | Test signal 2x amplitude, slow pulse |
| `]` | Test signal 2x amplitude, fast pulse |

### 7.6 Daisy Commands

| Command | Description | Response |
|---------|-------------|----------|
| `C` | Attach Daisy (16ch) | `daisy attached16$$$` or `16$$$` |
| `c` | Detach Daisy (8ch) | `daisy removed$$$` |

### 7.7 Timestamp Commands

| Command | Description | Response |
|---------|-------------|----------|
| `<` | Enable timestamp sync | `Time stamp ON$$$` or 0xC3/0xC5 in stream |
| `>` | Disable timestamp | `Time stamp OFF$$$` |

### 7.8 Radio Configuration (v2.0.0+)

Prefix: `0xF0` followed by command byte.

| Sequence | Description |
|----------|-------------|
| `0xF0 0x00` | Get radio channel |
| `0xF0 0x01 0xNN` | Set radio channel (1-25) |
| `0xF0 0x05` | Set baud rate to 115200 |
| `0xF0 0x06` | Set baud rate to 230400 |
| `0xF0 0x0A` | Set baud rate to 921600 |
| `0xF0 0x07` | Get radio system status |

### 7.9 Sample Rate Commands (v3.0.0+)

| Command | Sample Rate |
|---------|-------------|
| `~0` | 16000 Hz* |
| `~1` | 8000 Hz* |
| `~2` | 4000 Hz* |
| `~3` | 2000 Hz* |
| `~4` | 1000 Hz* |
| `~5` | 500 Hz* |
| `~6` | 250 Hz (default) |
| `~~` | Query current rate |

*Note: Sample rates above 250 Hz are not supported over radio. Use SD card logging.

---

## 8. Daisy Mode (16 Channels)

### 8.1 Overview

The Daisy expansion module adds a second ADS1299 for 16 total channels. Due to radio bandwidth limits, samples are interleaved and averaged.

### 8.2 Interleaving Pattern

| Sample # | Packet Contains | Maps To |
|----------|-----------------|---------|
| 0 | board(0), daisy(0) | Invalid (skip) |
| 1 | avg(board0, board1) | Channels 1-8 |
| 2 | avg(daisy1, daisy2) | Channels 9-16 |
| 3 | avg(board2, board3) | Channels 1-8 |
| 4 | avg(daisy3, daisy4) | Channels 9-16 |
| ... | ... | ... |

### 8.3 Effective Sample Rate

- **Transmitted rate**: 250 Hz per packet
- **Per-channel rate**: 125 Hz (every other packet)
- **Averaging**: 2-sample averaging done on-board

### 8.4 Channel Mapping

| Sample Number | Odd (1,3,5...) | Even (2,4,6...) |
|---------------|----------------|-----------------|
| Channels | 1-8 (Cyton) | 9-16 (Daisy) |
| Data | Board ADS1299 | Daisy ADS1299 |

### 8.5 Reconstruction

To reconstruct full 250 Hz for all channels:

```c
// Odd samples: Board channels 1-8
if (sampleNumber % 2 == 1) {
    channels[0..7] = parseEEG(packet);
    channels[8..15] = interpolate(previous_daisy, next_daisy);
}
// Even samples: Daisy channels 9-16
else {
    channels[8..15] = parseEEG(packet);
    channels[0..7] = interpolate(previous_board, next_board);
}
```

**Note**: For accurate timing, use SD card logging which stores raw unaveraged samples.

---

## 9. Timing & Bandwidth

### 9.1 Data Rate Calculations

```
Packet size:        33 bytes
Sample rate:        250 Hz
Data rate:          8,250 bytes/sec

Serial (8-N-1):     10 bits per byte (1 start + 8 data + 1 stop)
Wire bit rate:      8,250 × 10 = 82,500 bits/sec

Serial bandwidth:   115,200 baud
Max throughput:     115,200 / 10 = 11,520 bytes/sec
Utilization:        8,250 / 11,520 = 71.6%

With 921600 baud:
Max throughput:     921,600 / 10 = 92,160 bytes/sec
Utilization:        8,250 / 92,160 = 9.0%
```

### 9.2 Radio Constraints

The RFDuino radio is the bandwidth bottleneck:

| Parameter | Value |
|-----------|-------|
| Max OTA payload | 32 bytes |
| Air data rate | ~2 Mbps |
| Effective throughput | ~31 × 250 = 7,750 bytes/sec |
| Protocol overhead | ~500 bytes/sec |
| **Available for data** | ~8,000 bytes/sec |

This is why sample rates > 250 Hz are not supported wirelessly.

### 9.3 Latency Budget

| Stage | Latency |
|-------|---------|
| ADC conversion | 4 ms (250 Hz) |
| SPI transfer | ~100 µs |
| Packet assembly | ~50 µs |
| Radio TX/RX | ~2 ms |
| USB transfer | ~1 ms |
| Host processing | ~100 µs |
| **Total** | **~7 ms** |

### 9.4 Dropped Packet Detection

```c
void checkForDroppedPackets(uint8_t currentSample) {
    uint8_t expected = (lastSample + 1) & 0xFF;
    if (currentSample != expected) {
        int dropped = (currentSample - expected + 256) % 256;
        totalDropped += dropped;
        LOGW("Dropped %d packets (total: %d)", dropped, totalDropped);
    }
    lastSample = currentSample;
}
```

---

## 10. Implementation Notes

### 10.1 Packet Validation

```cpp
bool validatePacket(const uint8_t* packet) {
    // Check header
    if (packet[0] != 0xA0) {
        return false;
    }
    
    // Check footer (must be 0xC0-0xCF)
    if ((packet[32] & 0xF0) != 0xC0) {
        return false;
    }
    
    return true;
}
```

### 10.2 Packet Synchronization

```cpp
size_t findPacketStart(const std::vector<uint8_t>& buffer) {
    for (size_t i = 0; i < buffer.size(); i++) {
        if (buffer[i] == 0xA0) {
            // Verify we have enough data
            if (i + 33 <= buffer.size()) {
                // Check footer
                if ((buffer[i + 32] & 0xF0) == 0xC0) {
                    return i;
                }
            }
        }
    }
    return std::string::npos;
}
```

### 10.3 Complete Packet Parser

```cpp
bool parsePacket(const uint8_t* packet, 
                 float* eegData, 
                 float* accelData,
                 uint8_t& sampleNumber) {
    // Validate
    if (packet[0] != 0xA0) return false;
    uint8_t footer = packet[32];
    if ((footer & 0xF0) != 0xC0) return false;
    
    // Sample number
    sampleNumber = packet[1];
    
    // Parse 8 EEG channels
    for (int ch = 0; ch < 8; ch++) {
        int offset = 2 + (ch * 3);
        int32_t raw = interpret24BitAsInt32(&packet[offset]);
        eegData[ch] = raw * 0.02235f;  // µV at 24x gain
    }
    
    // Parse aux data based on packet type
    PacketType type = static_cast<PacketType>(footer);
    
    if (type == PacketType::STANDARD_ACCEL) {
        accelData[0] = interpret16BitAsInt16(&packet[26]) * 0.000125f;
        accelData[1] = interpret16BitAsInt16(&packet[28]) * 0.000125f;
        accelData[2] = interpret16BitAsInt16(&packet[30]) * 0.000125f;
    }
    
    return true;
}
```

### 10.4 Serial Port Configuration

```cpp
// Windows
DCB dcb = {0};
dcb.BaudRate = 115200;
dcb.ByteSize = 8;
dcb.Parity = NOPARITY;
dcb.StopBits = ONESTOPBIT;
dcb.fDtrControl = DTR_CONTROL_ENABLE;  // Required for dongle
dcb.fRtsControl = RTS_CONTROL_ENABLE;

// Linux
struct termios tty;
cfsetospeed(&tty, B115200);
cfsetispeed(&tty, B115200);
tty.c_cflag = CS8 | CLOCAL | CREAD;
tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
```

### 10.5 Initialization Sequence

```cpp
bool initializeCyton() {
    // 1. Open serial port
    if (!openSerial(portName, 115200)) return false;
    
    // 2. Wait for connection to stabilize
    Thread::sleep(500);
    
    // 3. Send soft reset
    write("v");
    
    // 4. Wait for ready response (ends with $$$)
    std::string response = readUntil("$$$", 5000);
    if (response.empty()) return false;
    
    // 5. Parse firmware version
    parseFirmwareVersion(response);
    
    // 6. Configure Daisy if needed
    if (useDaisy) {
        write("C");
        Thread::sleep(100);
        readUntil("$$$", 1000);
    }
    
    return true;
}
```

### 10.6 Start/Stop Streaming

```cpp
bool startStreaming() {
    // Clear any pending data
    flushInput();
    
    // Send start command
    write("b");  // No response expected
    
    isStreaming = true;
    startThread();  // Start data acquisition thread
    
    return true;
}

bool stopStreaming() {
    stopThread(1000);  // Wait up to 1s for thread to stop
    
    // Send stop command
    write("s");  // No response expected
    
    isStreaming = false;
    
    // Clear remaining data
    Thread::sleep(100);
    flushInput();
    
    return true;
}
```

---

## Appendix A: Quick Reference

### A.1 Header/Footer Bytes

| Byte | Value | Description |
|------|-------|-------------|
| Header | `0xA0` | Packet start |
| Footer | `0xC0-0xCF` | Packet type + end |

### A.2 Scale Factors

| Data Type | Scale Factor | Units |
|-----------|--------------|-------|
| EEG (24x gain) | 0.02235 | µV/count |
| Accelerometer | 0.000125 | g/count |

### A.3 Timing

| Parameter | Value |
|-----------|-------|
| Sample rate | 250 Hz |
| Packet interval | 4 ms |
| Baud rate | 115200 (default) |

---

## Appendix B: Comparison with InEar Teensy

| Feature | OpenBCI Cyton | InEar Teensy |
|---------|---------------|--------------|
| Connection | Wireless (RFDuino) | USB Direct |
| Sample Rate | 250 Hz | 1000 Hz |
| Channels | 8 (16 w/Daisy) | 5 EEG + 9 aux |
| Packet Size | 33 bytes | 56 bytes |
| Bandwidth | ~8.25 KB/s | ~54.7 KB/s |
| Latency | ~7 ms | ~2 ms |
| ADC | ADS1299 | ADS1299 |
| Checksum | None (radio CRC) | XOR |
| Error Recovery | Sample counter | Sample counter + checksum |

---

## References

1. [OpenBCI Cyton Data Format](https://docs.openbci.com/Cyton/CytonDataFormat/)
2. [OpenBCI Cyton SDK](https://docs.openbci.com/Cyton/CytonSDK/)
3. [ADS1299 Datasheet](https://www.ti.com/product/ADS1299)
4. [LIS3DH Datasheet](https://www.st.com/resource/en/datasheet/lis3dh.pdf)
5. [Nordic Gazell Protocol](https://infocenter.nordicsemi.com/topic/sdk_nrf5_v17.1.0/lib_gazelle.html)
