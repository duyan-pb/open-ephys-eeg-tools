# InEar Teensy Optimized Source Plugin for Open Ephys

A DataThread plugin that reads EEG data from a Teensy microcontroller with ADS1299 analog front-end using the InEar Teensy **variable-length optimized packet protocol**.

## Protocol: InEar Teensy Optimized

Variable-length packets (26-55 bytes) transmitted at 1 kHz over USB at 2 Mbaud. This optimized protocol reduces bandwidth by ~40% compared to the fixed 56-byte protocol by only sending auxiliary sensor data at their native sample rates.

### Multi-Rate Sampling

| Data Type | Sample Rate | Bandwidth Savings |
|-----------|-------------|-------------------|
| EEG       | 1000 Hz     | Every packet      |
| Accel     | 250 Hz      | Every 4th packet  |
| PPG       | 100 Hz      | Every 10th packet |
| Health    | 10 Hz       | Every 100th packet|
| Full Sync | 1 Hz        | Every 1000th packet|

### Packet Types

| Type | Code | Size | Contents |
|------|------|------|----------|
| EEG_ONLY | 0x00 | 26B | EEG only (most common) |
| EEG_ACCEL | 0x01 | 32B | EEG + Accelerometer |
| EEG_PPG | 0x02 | 44B | EEG + PPG sensors |
| EEG_ACCEL_PPG | 0x03 | 50B | EEG + Accel + PPG |
| EEG_HEALTH | 0x04 | 30B | EEG + Temp + Battery |
| EEG_ACCEL_HEALTH | 0x05 | 36B | EEG + Accel + Health |
| EEG_FULL_SYNC | 0x06 | 54B | All sensor data |

Add 0x10 to any type for marker flag (+1 byte).

### Packet Structure

```
┌────────────────────────────────────────────────────────────────────┐
│ HEADER (8B) │ EEG (15B) │ [MARKER] │ [ACCEL] │ [PPG] │ [HEALTH] │ FOOTER (3B) │
└────────────────────────────────────────────────────────────────────┘
```

| Field | Size | Description |
|-------|------|-------------|
| Sync | 2B | `0xA5 0x5A` |
| Type | 1B | PacketType enum |
| Sequence | 1B | 0-255 wrapping counter |
| Timestamp | 4B | Microseconds (Big Endian) |
| EEG | 15B | 5 channels × 24-bit BE |
| Marker | 1B | Optional event trigger |
| Accel | 6B | Optional: 3 × 16-bit BE |
| PPG | 18B | Optional: 3 × 48-bit BE |
| Health | 4B | Optional: Temp + Battery |
| Checksum | 1B | XOR of all preceding bytes |
| Footer | 2B | `0xC0 0xC0` |

## Data Streams

The plugin creates two data streams in Open Ephys:

1. **EEG Stream** (1000 Hz)
   - 5 channels of EEG data
   - 24-bit resolution, scaled to µV
   - Event channel for hardware markers

2. **Aux Stream** (1000 Hz, sample-and-hold)
   - 9 channels: AccelX, AccelY, AccelZ, PPG_Red, PPG_IR, PPG_Green, Temperature, Battery, Sync
   - Lower-rate sensors use sample-and-hold interpolation

## Building

### Prerequisites

- Open Ephys GUI source code
- CMake 3.15+
- Visual Studio 2019+ (Windows) or GCC/Clang (Linux/macOS)

### Build Steps

```bash
# Set GUI_BASE_DIR environment variable
export GUI_BASE_DIR=/path/to/plugin-GUI

# Create build directory
mkdir Build && cd Build

# Configure
cmake -G "Visual Studio 17 2022" -A x64 ..

# Build
cmake --build . --config Release

# Install (copies to Open Ephys plugins folder)
cmake --install . --config Release
```

## Usage

1. **Flash Teensy** with `inear_teensy_firmware_optimized.ino` from `teensy_firmwares/`
2. **Load Plugin** in Open Ephys: Processors → Sources → InEar Teensy Opt
3. **Select COM Port** from dropdown
4. **Click Connect**
5. **Start Acquisition** (play button)

### Simulation Mode

Enable "Simulate" checkbox to test without hardware. Generates sine waves on all EEG channels (3, 7, 11, 15, 19 Hz) and simulated PPG heartbeat at 72 BPM.

## Signal Chain Example

```
[InEar Teensy Opt Source] → [Bandpass Filter] → [LSL Outlet] → [LFP Viewer]
```

The LSL Outlet broadcasts timestamped data to the network for use in external applications (Python, MATLAB, etc.).

## Comparison: Original vs Optimized

| Feature | Original Protocol | Optimized Protocol |
|---------|-------------------|-------------------|
| Packet Size | Fixed 56B | Variable 26-55B |
| Avg Bandwidth | 56 KB/s | ~33 KB/s (~40% savings) |
| Aux Data Rate | 1000 Hz (oversampled) | Native rates |
| Gap Detection | Counter only | Sequence + Type |
| Plugin Name | InEar Teensy | InEar Teensy Opt |
| Firmware | `inear_teensy_firmware.ino` | `inear_teensy_firmware_optimized.ino` |

## Hardware Requirements

- Teensy 4.0/4.1 microcontroller
- ADS1299 EEG analog front-end
- ADXL345 accelerometer (optional)
- MAX30102 PPG sensor (optional)
- USB connection to PC (2 Mbaud)

## License

MIT License - See Open Ephys plugin guidelines.
