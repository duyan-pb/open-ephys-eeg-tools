# InEar Teensy Source Plugin for Open Ephys

A DataThread plugin that reads EEG data from a Teensy microcontroller with ADS1299 analog front-end using the InEar Teensy fixed-length packet protocol.

## Protocol: InEar Teensy

56-byte fixed-length packets transmitted at 1 kHz over USB at 2 Mbaud.

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
   - All channels at native sample rate

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

1. **Connect Teensy** running `inear_teensy_firmware.ino`
2. **Load Plugin** in Open Ephys: Processors → Sources → InEar Teensy
3. **Select COM Port** from dropdown
4. **Click Connect**
5. **Start Acquisition** (play button)

### Simulation Mode

Enable "Simulate" checkbox to test without hardware. Generates sine waves on all EEG channels (3, 7, 11, 15, 19 Hz) and simulated PPG heartbeat at 72 BPM.

## Signal Chain Example

```
[InEar Teensy Source] → [LSL Outlet] → [LFP Viewer]
```

The LSL Outlet broadcasts timestamped data to the network for use in external applications (Python, MATLAB, etc.).

## Hardware Requirements

- Teensy 4.0/4.1 microcontroller
- ADS1299 EEG analog front-end
- ADXL345 accelerometer (optional)
- MAX30102 PPG sensor (optional)
- USB connection to PC (2 Mbaud)

## License

MIT License - See Open Ephys plugin guidelines.

