# InEar Teensy Source Plugin for Open Ephys

A DataThread plugin that reads EEG data from a Teensy microcontroller with ADS1299 analog front-end using the InEar Teensy fixed-length packet protocol.

> **Note:** For bandwidth-optimized acquisition (~49% savings), see the [InEar Teensy Optimized](../inear-teensy-source-optimized/) plugin.

## Protocol: InEar Teensy (Fixed-Length)

56-byte fixed-length packets transmitted at 1 kHz over USB at 2 Mbaud.

- **Bandwidth**: 54.7 KB/s
- **Sample Rate**: 1000 Hz (all channels)
- **Packet Size**: 56 bytes (fixed)

### Packet Structure

| Offset | Size | Field       | Description                                    |
|--------|------|-------------|------------------------------------------------|
| 0      | 2B   | Header      | Sync bytes: `0xA5 0x5A`                        |
| 2      | 4B   | Timestamp   | Microsecond timer (Big Endian, wraps ~71 min)  |
| 6      | 1B   | Marker      | Hardware event triggers (button presses)       |
| 7      | 15B  | EEG         | 5 channels × 24-bit Big Endian                 |
| 22     | 6B   | Accel       | 3 channels × 16-bit Big Endian (X, Y, Z)       |
| 28     | 18B  | PPG         | 3 channels × 48-bit Big Endian (Red, IR, Green)|
| 46     | 2B   | Temperature | 16-bit Big Endian (0.01°C units)               |
| 48     | 2B   | Battery     | 16-bit Big Endian (mV)                         |
| 50     | 2B   | Sync        | 16-bit Big Endian sync signal                  |
| 52     | 1B   | Counter     | Packet sequence 0-255                          |
| 53     | 1B   | Checksum    | XOR of bytes 0-52                              |
| 54     | 2B   | Footer      | Termination bytes: `0xC0 0xC0`                 |

## Data Streams

The plugin creates two data streams in Open Ephys:

1. **EEG Stream** (1000 Hz)
   - 5 channels of EEG data
   - 24-bit resolution, scaled to µV
   - Event channel for hardware markers

2. **Aux Stream** (1000 Hz)
   - 9 channels: AccelX, AccelY, AccelZ, PPG_Red, PPG_IR, PPG_Green, Temperature, Battery, Sync
   - All channels at 1000 Hz (oversampled from native rates)

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
Copy-Item "Release\inear-teensy-source.dll" "C:\Program Files\Open Ephys\plugins\" -Force
```

## Usage

1. **Flash Teensy** with `inear_teensy_firmware.ino` from `teensy_firmwares/inear_teensy_firmware/`
2. **Load Plugin** in Open Ephys: Processors → Sources → InEar Teensy
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
  Current: 54.7 KB/s (280000 bytes in 5s)
  Average: 54.7 KB/s (273.4 KB total)
  Packets: 5000 received, 0 dropped
  Packet sizes: min=56, max=56, avg=56 bytes
  Types: Fixed(56B)=5000
================================================
```

## Signal Chain Example

```
[InEar Teensy] → [Bandpass Filter] → [LSL Outlet] → [LFP Viewer]
```

## Hardware Requirements

- Teensy 4.0/4.1 microcontroller
- ADS1299 EEG analog front-end
- ADXL345 accelerometer (optional)
- MAX30102 PPG sensor (optional)
- USB connection to PC (2 Mbaud)

## Comparison with Optimized Version

| Feature | This Plugin | Optimized Plugin |
|---------|-------------|------------------|
| Packet Size | Fixed 56B | Variable 26-55B |
| Bandwidth | 54.7 KB/s | ~28.5 KB/s |
| Aux Sampling | 1000 Hz (oversampled) | Native rates |
| Firmware | `inear_teensy_firmware` | `inear_teensy_firmware_optimized` |

## License

MIT License - See Open Ephys plugin guidelines.

