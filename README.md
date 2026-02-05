# Open Ephys EEG Tools & Plugins

A collection of custom Open Ephys plugins, Teensy firmware, and Python tools for EEG data acquisition and processing.

## Components

### Custom Open Ephys Plugins (`OEPlugins/`)

| Plugin | Type | Description |
|--------|------|-------------|
| **inear-teensy-source** | DataThread | Teensy EEG source (56-byte fixed packets @ 1kHz) |
| **inear-teensy-source-optimized** | DataThread | Teensy EEG source (26-55 byte variable packets, ~49% bandwidth savings) |
| **edf-file-source** | FileSource | Load EDF, BDF, CSV, and XZ-compressed files |
| **lsl-outlet** | Processor | LSL Outlet sink for streaming to external apps |
| **lab-streaming-layer-io** | DataThread + Processor | LSL Inlet (source) and LSL Outlet (sink) |
| **custom-ic-source** | DataThread | Native serial IC hardware integration |

### Teensy Firmware (`teensy_firmwares/`)

| Firmware | Protocol | Description |
|----------|----------|-------------|
| **inear_teensy_firmware** | Fixed 56-byte | Original protocol, 56 KB/s bandwidth |
| **inear_teensy_firmware_optimized** | Variable 26-55 byte | Optimized protocol, ~28 KB/s bandwidth |
| **ads1299_bioserial_pro** | Fixed 56-byte | BioSerial Pro variant |
| **ads1299_bioserial_pro_optimized** | Variable | BioSerial Pro optimized variant |

### Python Scripts (Root)

- `edf_to_lsl_streamer.py` - Stream EDF files via LSL
- `lsl_eeg_streamer.py` - Generic LSL EEG streamer
- `decompress_csv.py` - XZ decompression utility

### Submodules

- `plugin-GUI/` - Open Ephys GUI (v1.0.1)
- `open-ephys-python-tools/` - Open Ephys Python analysis tools

## Quick Start

### Building Plugins (Windows)

```powershell
# Prerequisites: Visual Studio 2022, CMake

# Build a plugin
cd OEPlugins/<plugin-name>/Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Deploy to Open Ephys (requires admin)
Copy-Item "Build\Release\<plugin-name>.dll" "C:\Program Files\Open Ephys\plugins\" -Force
```

### Flashing Teensy Firmware

1. Open Arduino IDE
2. Open firmware from `teensy_firmwares/<firmware-name>/<firmware-name>.ino`
3. Select Tools → Board → Teensy 4.1
4. Click Upload

### Testing with Open Ephys

1. Launch Open Ephys
2. Add source plugin: Processors → Sources → InEar Teensy (or InEar Teensy Opt)
3. Select COM port and click Connect
4. Add visualization: LFP Viewer
5. Click Play to start acquisition

## Protocol Comparison

| Feature | Original Protocol | Optimized Protocol |
|---------|-------------------|-------------------|
| Packet Size | Fixed 56 bytes | Variable 26-55 bytes |
| Bandwidth | 54.7 KB/s | ~28.5 KB/s (~49% savings) |
| Aux Data Rate | 1000 Hz (oversampled) | Native rates (10-250 Hz) |
| Plugin | InEar Teensy | InEar Teensy Opt |
| Firmware | `inear_teensy_firmware` | `inear_teensy_firmware_optimized` |

## Requirements

- Open Ephys GUI v1.0.1+
- Visual Studio 2022 (Windows) / GCC (Linux/macOS)
- CMake 3.15+
- Teensy 4.1 with Teensyduino
- Python 3.10+ with pylsl, mne, numpy (for Python tools)

## License

GPL-3.0 (same as Open Ephys)
