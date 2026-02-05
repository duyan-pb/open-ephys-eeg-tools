# InEar Teensy Optimized Source Plugin for Open Ephys

A DataThread plugin that reads EEG data from a Teensy microcontroller with ADS1299 analog front-end using the InEar Teensy **variable-length optimized packet protocol**.

> **Performance**: This optimized protocol reduces bandwidth by ~49% compared to the fixed 56-byte protocol (28.5 KB/s vs 54.7 KB/s).

## Protocol: InEar Teensy Optimized

Variable-length packets (26-55 bytes) transmitted at 1 kHz over USB at 2 Mbaud. Auxiliary sensors transmit at their native rates instead of being oversampled.

- **Bandwidth**: ~28.5 KB/s (measured)
- **Savings**: ~49% vs original protocol
- **Sample Rate**: 1000 Hz (EEG), multi-rate aux

### Multi-Rate Sampling

| Data Type | Sample Rate | Packets |
|-----------|-------------|---------|
| EEG       | 1000 Hz     | Every packet |
| Accel     | 250 Hz      | Every 4th packet |
| PPG       | 100 Hz      | Every 10th packet |
| Health    | 10 Hz       | Every 100th packet |

### Packet Types

| Type | Code | Size | Contents |
|------|------|------|----------|
| EEG_ONLY | 0x00 | 26B | EEG only (most common, ~64%) |
| EEG_ACCEL | 0x01 | 32B | EEG + Accelerometer (~25%) |
| EEG_ACCEL_PPG | 0x03 | 50B | EEG + Accel + PPG (~10%) |
| EEG_FULL_SYNC | 0x06 | 55B | All sensor data (~1%) |

Add 0x10 to any type for marker flag (+1 byte).

### Packet Structure

```
┌─────────────────────────────────────────────────────────────────────────┐
│ HEADER (8B) │ EEG (15B) │ [MARKER] │ [ACCEL] │ [PPG] │ [HEALTH] │ FOOTER (3B) │
└─────────────────────────────────────────────────────────────────────────┘
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

- Open Ephys GUI source code (plugin-GUI)
- CMake 3.15+
- Visual Studio 2022 (Windows) or GCC/Clang (Linux/macOS)

### Build Steps

```bash
# Windows
cd Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Deploy (requires admin)
Copy-Item "Release\inear-teensy-source-optimized.dll" "C:\Program Files\Open Ephys\plugins\" -Force
```

## Usage

1. **Flash Teensy** with `inear_teensy_firmware_optimized.ino` from `teensy_firmwares/inear_teensy_firmware_optimized/`
2. **Load Plugin** in Open Ephys: Processors → Sources → InEar Teensy Opt
3. **Select COM Port** from dropdown
4. **Click Connect**
5. **Start Acquisition** (play button)

### Simulation Mode

Enable "Simulate" checkbox to test without hardware. Generates:
- Sine waves on EEG channels (3, 7, 11, 15, 19 Hz)
- Simulated PPG heartbeat at 72 BPM

## Bandwidth Monitoring

The plugin logs bandwidth statistics every 5 seconds:

```
======== BANDWIDTH REPORT (t=5s) ========
  Current: 28.5 KB/s (145920 bytes in 5s)
  Average: 28.5 KB/s (142.5 KB total)
  Packets: 5000 received, 0 dropped
  Packet sizes: min=26, max=55, avg=29.2 bytes
  Types: EEG=3250, +Accel=1250, +PPG=450, Full=50
================================================
```

## Signal Chain Example

```
[InEar Teensy Opt] → [Bandpass Filter] → [LSL Outlet] → [LFP Viewer]
```

## Comparison: Original vs Optimized

| Feature | Original Protocol | Optimized Protocol |
|---------|-------------------|-------------------|
| Packet Size | Fixed 56B | Variable 26-55B |
| Avg Bandwidth | 54.7 KB/s | ~28.5 KB/s |
| Bandwidth Savings | - | ~49% |
| Aux Data Rate | 1000 Hz (oversampled) | Native rates |
| Plugin Name | InEar Teensy | InEar Teensy Opt |
| Firmware | `inear_teensy_firmware` | `inear_teensy_firmware_optimized` |

## Hardware Requirements

- Teensy 4.0/4.1 microcontroller
- ADS1299 EEG analog front-end
- ADXL345 accelerometer (optional)
- MAX30102 PPG sensor (optional)
- USB connection to PC (2 Mbaud)

## License

MIT License - See Open Ephys plugin guidelines.
