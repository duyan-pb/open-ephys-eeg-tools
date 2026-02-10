# InEar Teensy Protocol Specification

Technical specification for the InEar Teensy EEG streaming protocol used between Teensy 4.1 firmware and Open Ephys plugin.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Channel Configuration](#2-channel-configuration)
3. [Packet Structure](#3-packet-structure)
4. [Data Encoding](#4-data-encoding)
5. [Protocol Design Rationale](#5-protocol-design-rationale)
6. [Timing & Bandwidth](#6-timing--bandwidth)
7. [Error Detection](#7-error-detection)
8. [Implementation Notes](#8-implementation-notes)

---

## 1. Overview

InEar Teensy is a binary streaming protocol designed for real-time EEG acquisition from an ADS1299-based biosignal amplifier connected to a Teensy 4.1 microcontroller.

| Parameter | Value |
|-----------|-------|
| **Sample Rate** | 1000 Hz |
| **Packet Size** | 56 bytes |
| **Baud Rate** | 2,000,000 (2 Mbaud) |
| **Data Throughput** | 56,000 bytes/sec |
| **Latency** | < 1 ms (single packet) |

---

## 2. Channel Configuration

### 2.1 EEG Channels (5 channels)

High-resolution electrophysiology channels from the ADS1299 analog front-end.

| Channel | Name | Description | Resolution | Range | Units |
|---------|------|-------------|------------|-------|-------|
| 0 | EEG1 | Frontal Left (Fp1) | 24-bit signed | ±187.5 mV | µV |
| 1 | EEG2 | Frontal Right (Fp2) | 24-bit signed | ±187.5 mV | µV |
| 2 | EEG3 | Central (Cz) | 24-bit signed | ±187.5 mV | µV |
| 3 | EEG4 | Parietal Left (P3) | 24-bit signed | ±187.5 mV | µV |
| 4 | EEG5 | Parietal Right (P4) | 24-bit signed | ±187.5 mV | µV |

**ADS1299 Specifications:**
- ADC Resolution: 24 bits (16,777,216 levels)
- Input-referred noise: 1 µVpp
- CMRR: -110 dB
- Gain: 24x (default)
- LSB size: 0.02235 µV/count at gain=24

**Conversion formula:**
```
voltage_µV = raw_24bit * (4.5V / 2^23) / gain * 1e6
           = raw_24bit * 0.02235  (at gain=24)
```

### 2.2 Auxiliary Channels (9 channels)

Supplementary sensor data sampled synchronously with EEG.

| Channel | Name | Description | Resolution | Range | Units |
|---------|------|-------------|------------|-------|-------|
| 0 | AccelX | Accelerometer X-axis | 16-bit signed | ±2g | mg |
| 1 | AccelY | Accelerometer Y-axis | 16-bit signed | ±2g | mg |
| 2 | AccelZ | Accelerometer Z-axis | 16-bit signed | ±2g | mg |
| 3 | PPG_Red | PPG Red LED | 48-bit unsigned | 0-281474976710655 | counts |
| 4 | PPG_IR | PPG Infrared LED | 48-bit unsigned | 0-281474976710655 | counts |
| 5 | PPG_Green | PPG Green LED | 48-bit unsigned | 0-281474976710655 | counts |
| 6 | Temp | Temperature sensor | 16-bit unsigned | 0-100 | °C × 100 |
| 7 | Battery | Battery voltage | 16-bit unsigned | 0-5000 | mV |
| 8 | Sync | Synchronization marker | 16-bit unsigned | 0-65535 | - |

**Sensor Details:**

- **Accelerometer (ICM-20948 or similar)**
  - Range: ±2g, ±4g, ±8g, ±16g (configured at ±2g)
  - LSB: 16384 counts/g at ±2g
  - Bandwidth: 500 Hz

- **PPG (MAX30102 or similar)**
  - 3-wavelength optical sensor
  - Red: 660 nm, IR: 880 nm, Green: 530 nm
  - 18-bit ADC, packed as 48-bit big-endian
  - Used for heart rate and SpO2

- **Temperature**
  - Internal sensor or external thermistor
  - Resolution: 0.01°C
  - Value = raw / 100.0 to get °C

- **Battery**
  - Direct ADC reading of battery voltage divider
  - Value in millivolts

- **Sync**
  - External trigger/marker input
  - Used for event synchronization

---

## 3. Packet Structure

Each packet is exactly **56 bytes** containing one sample from all channels.

```
Offset  Size  Field           Description
------  ----  --------------  ----------------------------------
0       1     Header[0]       0xA5 (sync byte 1)
1       1     Header[1]       0x5A (sync byte 2)
2       4     Timestamp       32-bit big-endian microsecond timestamp
6       1     Marker          Event marker byte
7       15    EEG Data        5 channels × 3 bytes (24-bit BE each)
22      6     Accel Data      3 channels × 2 bytes (16-bit BE each)
28      18    PPG Data        3 channels × 6 bytes (48-bit BE each)
46      2     Temperature     16-bit signed BE (0.01°C units)
48      2     Battery         16-bit unsigned BE (mV)
50      2     Sync            16-bit unsigned BE
52      1     Counter         Packet counter (0-255)
53      1     Checksum        XOR of bytes 0-52
54      1     Footer[0]       0xC0 (end byte 1)
55      1     Footer[1]       0xC0 (end byte 2)
------  ----  --------------  ----------------------------------
Total: 56 bytes
```

### 3.1 Visual Representation

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│ A5 5A │ TS[4] │ MKR │ EEG[15] │ ACC[6] │ PPG[18] │ TMP[2] │ BAT[2] │ SYN[2] │ CTR │ CHK │ C0 C0 │
└─────────────────────────────────────────────────────────────────────────────────────┘
   2       4      1       15        6        18        2        2        2       1     1      2    = 56 bytes
```

---

## 4. Data Encoding

### 4.1 Byte Order

| Field | Byte Order | Reason |
|-------|-----------|--------|
| Timestamp | Big-endian | Consistent with sensor data |
| EEG Data | Big-endian | Native to ADS1299 SPI output |
| Accel Data | Big-endian | Consistent with EEG, network standard |
| PPG Data | Big-endian | Consistent with EEG, network standard |

### 4.2 EEG Channel Encoding (24-bit signed, Big-Endian)

Each EEG channel uses 3 bytes in two's complement format:

```
Byte 0: MSB (includes sign bit)
Byte 1: Middle byte
Byte 2: LSB

Value = (Byte0 << 16) | (Byte1 << 8) | Byte2

If Byte0 & 0x80:  # Negative number
    Value = Value - 0x1000000  # Convert from unsigned to signed
```

**Example:**
```
Raw bytes: [0xFF, 0xFE, 0x00]
Unsigned:  0xFFFE00 = 16776704
Signed:    16776704 - 16777216 = -512
Voltage:   -512 × 0.02235 = -11.44 µV
```

### 4.3 Accelerometer Encoding (16-bit signed, Big-Endian)

Each accelerometer channel uses 2 bytes:

```
Byte 0: MSB
Byte 1: LSB

Value = (Byte0 << 8) | Byte1
```

Signed (two's complement).

### 4.4 PPG Channel Encoding (48-bit unsigned, Big-Endian)

Each PPG channel uses 6 bytes:

```
Byte 0: MSB (most significant)
Byte 1: ...
Byte 2: ...
Byte 3: ...
Byte 4: ...
Byte 5: LSB (least significant)

Value = (Byte0 << 40) | (Byte1 << 32) | (Byte2 << 24) | (Byte3 << 16) | (Byte4 << 8) | Byte5
```

Unsigned. The MAX30102 ADC is 18-bit, so upper bytes are typically zero.

### 4.5 Temp/Battery/Sync Encoding (16-bit, Big-Endian)

Each field uses 2 bytes:

```
Byte 0: MSB
Byte 1: LSB

Value = (Byte0 << 8) | Byte1
```

- Temperature: Signed (two's complement)
- Battery, Sync: Unsigned

### 4.6 Sample Count

32-bit unsigned counter, little-endian, wraps at 2³² (4,294,967,296).

At 1000 Hz, wraps every ~49.7 days.

---

## 5. Protocol Design Rationale

### 5.1 Why 56 Bytes?

| Component | Bytes | Justification |
|-----------|-------|---------------|
| Header | 2 | Reliable sync detection with unique pattern |
| Timestamp | 4 | 32-bit microsecond timestamp |
| Marker | 1 | Event tagging for experiments |
| EEG (5 × 3B) | 15 | Full 24-bit resolution from ADS1299 |
| Accel (3 × 2B) | 6 | 16-bit sufficient for accelerometer |
| PPG (3 × 6B) | 18 | 48-bit packed from MAX30102 |
| Temp | 2 | 16-bit temperature |
| Battery | 2 | 16-bit battery voltage |
| Sync | 2 | 16-bit sync marker |
| Counter | 1 | Packet counter (0-255) |
| Checksum | 1 | Error detection |
| Footer | 2 | Frame boundary detection |
| **Total** | **56** | All bytes carry useful data |

### 5.2 Why 0xA5 0x5A Header?

```
0xA5 = 10100101 (binary)
0x5A = 01011010 (binary)
```

- **Alternating pattern**: Easy to detect, unlikely in random data
- **Bit-complementary**: 0xA5 XOR 0xFF = 0x5A
- **Industry standard**: Used in many embedded protocols (NMEA, Modbus variants)
- **Self-clocking**: Transitions help UART recover from errors

### 5.3 Why 0xC0 0xC0 Footer?

- **Distinct from header**: No confusion between start/end
- **SLIP compatibility**: 0xC0 is the SLIP END character
- **Double byte**: More reliable frame detection
- **Not in typical data**: Rare in 24-bit EEG samples

### 5.4 Why Big-Endian for Sensor Data?

1. **ADS1299 native format**: SPI outputs MSB first (Big-Endian)
2. **No conversion needed**: Firmware can DMA directly from SPI
3. **Network standard**: Most protocols use Big-Endian
4. **Human readable**: Hex dumps show intuitive order

### 5.5 Why 1000 Hz Sample Rate?

| Consideration | Impact |
|---------------|--------|
| EEG bandwidth | Clinical EEG: 0.5-70 Hz, needs ≥140 Hz (Nyquist) |
| EMG/EOG artifacts | Up to 300 Hz, needs ≥600 Hz |
| 50/60 Hz noise | Easier to filter at higher rates |
| USB bandwidth | 56 KB/s << 12 Mbps USB Full Speed |
| Processing load | 1000 samples/sec easily handled |
| Standard rate | Matches most EEG systems |

### 5.6 Why 2 Mbaud?

```
Required bandwidth = 56 bytes × 1000 Hz × 10 bits/byte = 560,000 bps
Safety margin = 2,000,000 / 560,000 = 3.57×
```

- **3.5× headroom**: Absorbs timing jitter and USB latency
- **Teensy 4.1 capable**: Hardware UART supports up to 6 Mbaud
- **Standard rate**: Common in high-speed serial applications
- **Low latency**: Smaller FIFO fill time

---

## 6. Timing & Bandwidth

### 6.1 Data Rate Calculations

```
Packet size:        56 bytes
Sample rate:        1000 Hz
Data rate:          56,000 bytes/sec

Serial (8-N-1):     10 bits per byte (1 start + 8 data + 1 stop)
Wire bit rate:      56,000 × 10 = 560,000 bits/sec

Serial bandwidth:   2,000,000 baud
Max throughput:     2,000,000 / 10 = 200,000 bytes/sec
Utilization:        56,000 / 200,000 = 28.0%

USB transfer:       ~500 µs per bulk transfer (typical)
Packets per USB:    ~50-100 packets batched
```

### 6.2 Latency Budget

| Stage | Latency |
|-------|---------|
| ADC conversion | 1 ms (1000 Hz) |
| SPI transfer | ~50 µs |
| Packet assembly | ~10 µs |
| USB transfer | ~500 µs |
| Host processing | ~100 µs |
| **Total** | **~2 ms** |

### 6.3 Buffer Sizing

| Component | Size | Duration |
|-----------|------|----------|
| Teensy TX buffer | 4 KB | ~71 packets (71 ms) |
| USB bulk buffer | 512 B | ~9 packets |
| Host read buffer | 32 KB | ~571 packets (571 ms) |
| Ring buffer | 2048 samples | 2048 ms |

---

## 7. Error Detection

### 7.1 Checksum Algorithm

**XOR checksum of bytes 0-52 (53 bytes):**

```c
uint8_t checksum = 0;
for (int i = 0; i < 53; i++) {
    checksum ^= packet[i];
}
// checksum stored at packet[53]
```

**Properties:**
- Detects single-bit errors: 100%
- Detects odd number of bit errors: 100%
- Detects burst errors: ~50%
- Computational cost: Minimal (no division/multiply)

### 7.2 Packet Validation Steps

1. **Find header**: Scan for 0xA5 0x5A
2. **Check footer**: Verify bytes 54-55 are 0xC0 0xC0
3. **Verify checksum**: XOR bytes 0-52, compare to byte 53
4. **Check sample count**: Should increment by 1 (detect drops)

### 7.3 Error Recovery

```
On checksum error:
  1. Discard packet
  2. Scan for next header
  3. Increment error counter

On missing packet (gap in sample count):
  1. Insert zero samples or interpolate
  2. Increment drop counter
  3. Log warning
```

---

## 8. Implementation Notes

### 8.1 Firmware (Teensy 4.1)

```cpp
// Packet constants
const uint8_t HEADER[] = {0xA5, 0x5A};
const uint8_t FOOTER[] = {0xC0, 0xC0};
const int PACKET_SIZE = 56;

// Build packet
void buildPacket(uint8_t* packet, uint32_t timestamp, 
                 int32_t* eeg, int16_t* accel, int64_t* ppg,
                 int16_t temp, uint16_t battery, uint16_t sync,
                 uint8_t marker, uint8_t counter) {
    // Header
    packet[0] = 0xA5;
    packet[1] = 0x5A;
    
    // Timestamp (big-endian)
    packet[2] = (timestamp >> 24) & 0xFF;
    packet[3] = (timestamp >> 16) & 0xFF;
    packet[4] = (timestamp >> 8) & 0xFF;
    packet[5] = timestamp & 0xFF;
    
    // Marker
    packet[6] = marker;
    
    // EEG data (big-endian, 24-bit)
    for (int i = 0; i < 5; i++) {
        packet[7 + i*3 + 0] = (eeg[i] >> 16) & 0xFF;
        packet[7 + i*3 + 1] = (eeg[i] >> 8) & 0xFF;
        packet[7 + i*3 + 2] = eeg[i] & 0xFF;
    }
    
    // Accelerometer (big-endian, 16-bit)
    for (int i = 0; i < 3; i++) {
        packet[22 + i*2 + 0] = (accel[i] >> 8) & 0xFF;
        packet[22 + i*2 + 1] = accel[i] & 0xFF;
    }
    
    // PPG data (big-endian, 48-bit)
    for (int i = 0; i < 3; i++) {
        packet[28 + i*6 + 0] = (ppg[i] >> 40) & 0xFF;
        packet[28 + i*6 + 1] = (ppg[i] >> 32) & 0xFF;
        packet[28 + i*6 + 2] = (ppg[i] >> 24) & 0xFF;
        packet[28 + i*6 + 3] = (ppg[i] >> 16) & 0xFF;
        packet[28 + i*6 + 4] = (ppg[i] >> 8) & 0xFF;
        packet[28 + i*6 + 5] = ppg[i] & 0xFF;
    }
    
    // Temperature (big-endian, 16-bit)
    packet[46] = (temp >> 8) & 0xFF;
    packet[47] = temp & 0xFF;
    
    // Battery (big-endian, 16-bit)
    packet[48] = (battery >> 8) & 0xFF;
    packet[49] = battery & 0xFF;
    
    // Sync (big-endian, 16-bit)
    packet[50] = (sync >> 8) & 0xFF;
    packet[51] = sync & 0xFF;
    
    // Counter
    packet[52] = counter;
    
    // Checksum
    uint8_t checksum = 0;
    for (int i = 0; i < 53; i++) checksum ^= packet[i];
    packet[53] = checksum;
    
    // Footer
    packet[54] = 0xC0;
    packet[55] = 0xC0;
}
```

### 8.2 Plugin (C++)

```cpp
// Parse single packet
bool parsePacket(const uint8_t* data, Sample& sample) {
    // Verify header
    if (data[0] != 0xA5 || data[1] != 0x5A) return false;
    
    // Verify footer
    if (data[54] != 0xC0 || data[55] != 0xC0) return false;
    
    // Verify checksum
    uint8_t checksum = 0;
    for (int i = 0; i < 53; i++) checksum ^= data[i];
    if (checksum != data[53]) return false;
    
    // Parse timestamp (big-endian)
    sample.timestamp = (data[2] << 24) | (data[3] << 16) | 
                       (data[4] << 8) | data[5];
    
    // Parse marker
    sample.marker = data[6];
    
    // Parse EEG (big-endian, 24-bit signed)
    for (int i = 0; i < 5; i++) {
        int32_t raw = (data[7 + i*3] << 16) | 
                      (data[8 + i*3] << 8) | 
                      data[9 + i*3];
        if (raw & 0x800000) raw |= 0xFF000000; // Sign extend
        sample.eeg[i] = raw * 0.02235f; // Convert to µV
    }
    
    // Parse Accelerometer (big-endian, 16-bit signed)
    for (int i = 0; i < 3; i++) {
        sample.accel[i] = (int16_t)((data[22 + i*2] << 8) | data[23 + i*2]);
    }
    
    // Parse PPG (big-endian, 48-bit unsigned)
    for (int i = 0; i < 3; i++) {
        sample.ppg[i] = ((int64_t)data[28 + i*6] << 40) |
                        ((int64_t)data[29 + i*6] << 32) |
                        ((int64_t)data[30 + i*6] << 24) |
                        ((int64_t)data[31 + i*6] << 16) |
                        ((int64_t)data[32 + i*6] << 8) |
                        data[33 + i*6];
    }
    
    // Parse Temp, Battery, Sync (big-endian, 16-bit)
    sample.temp = (int16_t)((data[46] << 8) | data[47]);
    sample.battery = (data[48] << 8) | data[49];
    sample.sync = (data[50] << 8) | data[51];
    
    // Parse counter
    sample.counter = data[52];
    
    return true;
}
```

### 8.3 Python

```python
import struct

HEADER = bytes([0xA5, 0x5A])
FOOTER = bytes([0xC0, 0xC0])
PACKET_SIZE = 56

def parse_packet(data: bytes) -> dict:
    """Parse a 56-byte InEar Teensy packet."""
    if len(data) != 56:
        raise ValueError(f"Expected 56 bytes, got {len(data)}")
    
    if data[0:2] != HEADER:
        raise ValueError("Invalid header")
    
    if data[54:56] != FOOTER:
        raise ValueError("Invalid footer")
    
    # Verify checksum
    checksum = 0
    for b in data[0:53]:
        checksum ^= b
    if checksum != data[53]:
        raise ValueError(f"Checksum mismatch: {checksum} != {data[53]}")
    
    # Parse timestamp (big-endian)
    timestamp = struct.unpack('>I', data[2:6])[0]
    
    # Marker
    marker = data[6]
    
    # Parse EEG (big-endian, 24-bit signed)
    eeg = []
    for i in range(5):
        offset = 7 + i * 3
        raw = (data[offset] << 16) | (data[offset+1] << 8) | data[offset+2]
        if raw & 0x800000:
            raw -= 0x1000000
        eeg.append(raw * 0.02235)  # µV
    
    # Parse accelerometer (big-endian, 16-bit signed)
    accel = []
    for i in range(3):
        offset = 22 + i * 2
        val = (data[offset] << 8) | data[offset+1]
        if val & 0x8000:
            val -= 0x10000
        accel.append(val)
    
    # Parse PPG (big-endian, 48-bit unsigned)
    ppg = []
    for i in range(3):
        offset = 28 + i * 6
        val = (data[offset] << 40) | (data[offset+1] << 32) | \
              (data[offset+2] << 24) | (data[offset+3] << 16) | \
              (data[offset+4] << 8) | data[offset+5]
        ppg.append(val)
    
    # Parse temp, battery, sync (big-endian, 16-bit)
    temp = struct.unpack('>h', data[46:48])[0]
    battery = struct.unpack('>H', data[48:50])[0]
    sync = struct.unpack('>H', data[50:52])[0]
    
    # Counter
    counter = data[52]
    
    return {
        'timestamp': timestamp,
        'marker': marker,
        'eeg': eeg,
        'accel': accel,
        'ppg': ppg,
        'temp': temp,
        'battery': battery,
        'sync': sync,
        'counter': counter
    }
```

---

## Appendix A: Protocol Constants

```cpp
// C/C++ Header
namespace InEarTeensy {
    // Packet structure
    constexpr uint8_t  HEADER_BYTE_1    = 0xA5;
    constexpr uint8_t  HEADER_BYTE_2    = 0x5A;
    constexpr uint8_t  FOOTER_BYTE_1    = 0xC0;
    constexpr uint8_t  FOOTER_BYTE_2    = 0xC0;
    constexpr int      PACKET_SIZE      = 56;
    
    // Channel counts
    constexpr int      NUM_EEG_CHANNELS = 5;
    constexpr int      NUM_ACCEL_CHANNELS = 3;
    constexpr int      NUM_PPG_CHANNELS = 3;
    
    // Timing
    constexpr int      SAMPLE_RATE      = 1000;  // Hz
    constexpr int      BAUD_RATE        = 2000000;
    
    // Byte offsets
    constexpr int      OFFSET_HEADER    = 0;
    constexpr int      OFFSET_TIMESTAMP = 2;
    constexpr int      OFFSET_MARKER    = 6;
    constexpr int      OFFSET_EEG       = 7;
    constexpr int      OFFSET_ACCEL     = 22;
    constexpr int      OFFSET_PPG       = 28;
    constexpr int      OFFSET_TEMP      = 46;
    constexpr int      OFFSET_BATTERY   = 48;
    constexpr int      OFFSET_SYNC      = 50;
    constexpr int      OFFSET_COUNTER   = 52;
    constexpr int      OFFSET_CHECKSUM  = 53;
    constexpr int      OFFSET_FOOTER    = 54;
    
    // Conversion
    constexpr float    EEG_SCALE        = 0.02235f;  // µV per count
}
```

---

## Appendix B: Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-02-03 | Initial protocol specification |

---

## Appendix C: References

1. **ADS1299 Datasheet** - Texas Instruments SBAS499
2. **Teensy 4.1 Documentation** - PJRC
3. **Open Ephys Plugin API** - open-ephys.org
4. **USB 2.0 Specification** - usb.org

