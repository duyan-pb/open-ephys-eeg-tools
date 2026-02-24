# Open Ephys EEG Tools & Plugins

A collection of custom Open Ephys plugins, Teensy firmware, and Python tools for EEG data acquisition, experiment paradigm integration, and real-time processing.

> **Last verified:** February 23, 2026 — all 8 C++ plugins built, all 3 Python packages installed, 20/20 tests passing.

## Components

### Custom Open Ephys Plugins (`OEPlugins/`)

| Plugin | Type | Description | Status |
|--------|------|-------------|--------|
| **[inear-teensy-source](OEPlugins/inear-teensy-source/)** | DataThread | Teensy EEG source (56-byte fixed packets @ 1 kHz) | ✅ Production |
| **[inear-teensy-source-optimized](OEPlugins/inear-teensy-source-optimized/)** | DataThread | Teensy EEG source (26–55 byte variable packets, ~49% bandwidth savings) | ✅ Production |
| **[openbci-cyton](OEPlugins/openbci-cyton/)** | DataThread | OpenBCI Cyton board (8/16 ch, 250 Hz, RFDuino) | ✅ Production |
| **[custom-ic-source](OEPlugins/custom-ic-source/)** | DataThread | Generic serial IC hardware integration | ✅ Production |
| **[edf-file-source](OEPlugins/edf-file-source/)** | FileSource | Load EDF, BDF, CSV, and XZ-compressed files | ✅ Production |
| **[lab-streaming-layer-io](OEPlugins/lab-streaming-layer-io/)** | DataThread + Processor | LSL Inlet (source) and LSL Outlet (sink) | ✅ Production |
| **[lsl-outlet](OEPlugins/lsl-outlet/)** | Processor | LSL Outlet sink for streaming to external apps | ✅ Production |
| **[paradigm-bridge-cpp](OEPlugins/paradigm-bridge-cpp/)** | Processor (DLL) | Native C++ paradigm bridge — TCP triggers + recording control | ✅ Production |

### Python Packages (`OEPlugins/`)

| Package | Version | Description |
|---------|---------|-------------|
| **[paradigm-bridge](OEPlugins/paradigm-bridge/)** | 2.0.0 | PsychoPy/Expyriment ↔ Open Ephys (recording + TTL triggers via TCP) |
| **[custom-python-ic-connector](OEPlugins/custom_python_ic_connector/)** | 1.0.0 | Streams custom IC data to Open Ephys via LSL |
| **[open-ephys-python-tools](open-ephys-python-tools/)** | 1.0.1 | Analysis tools for Open Ephys recordings (Binary, NWB, OpenEphys formats) |

### Teensy Firmware (`teensy_firmwares/`)

| Firmware | Protocol | Bandwidth | Plugin |
|----------|----------|-----------|--------|
| **[inear_teensy_firmware](teensy_firmwares/inear_teensy_firmware/)** | Fixed 56-byte | 54.7 KB/s | `inear-teensy-source` |
| **[inear_teensy_firmware_optimized](teensy_firmwares/inear_teensy_firmware_optimized/)** | Variable 26–55 byte | ~28.5 KB/s | `inear-teensy-source-optimized` |
| **[ads1299_bioserial_pro](teensy_firmwares/ads1299_bioserial_pro/)** | BioSerial Pro fixed | 54.7 KB/s | — |
| **[ads1299_bioserial_pro_optimized](teensy_firmwares/ads1299_bioserial_pro_optimized/)** | BioSerial Pro optimized | ~28.5 KB/s | — |

### Utility Scripts (Root)

| Script | Description |
|--------|-------------|
| `edf_to_lsl_streamer.py` | Stream EDF files via LSL for real-time playback |
| `lsl_eeg_streamer.py` | Generic synthetic/file-based LSL EEG streamer |
| `decompress_csv.py` | XZ → CSV decompression for the EDF/CSV File Reader |
| `inear_teensy_test.py` | InEar Teensy protocol test & simulation utility |

### Submodules

- `plugin-GUI/` — Open Ephys GUI (v1.0.1)
- `open-ephys-python-tools/` — Python analysis tools for Open Ephys recordings

## Quick Start

### 1. Set Up Python Environment

```powershell
# Create virtual environment
python -m venv .venv
.venv\Scripts\Activate.ps1

# Install Python packages
pip install -e OEPlugins/paradigm-bridge[dev]
pip install -e open-ephys-python-tools[dev]
pip install -e OEPlugins/custom_python_ic_connector[dev]
```

### 2. Build All C++ Plugins (Windows)

```powershell
# Prerequisites: Visual Studio 2022, CMake 3.15+

$plugins = @(
    "inear-teensy-source",
    "inear-teensy-source-optimized",
    "openbci-cyton",
    "custom-ic-source",
    "edf-file-source",
    "lab-streaming-layer-io",
    "lsl-outlet",
    "paradigm-bridge-cpp"
)

foreach ($p in $plugins) {
    Push-Location "OEPlugins\$p\Build"
    cmake --build . --config Release
    cmake --install . --config Release
    Pop-Location
}
```

All 8 DLLs are automatically installed to `plugin-GUI/Build/Release/plugins/`.

### 3. Flash Teensy Firmware

1. Install [Teensyduino](https://www.pjrc.com/teensy/teensyduino.html)
2. Open `teensy_firmwares/<firmware-name>/<firmware-name>.ino`
3. Select Tools → Board → Teensy 4.1
4. Click Upload

### 4. Run Open Ephys

```powershell
plugin-GUI\Build\Release\open-ephys.exe
```

### 5. Run Tests

```powershell
# Paradigm Bridge tests (4 tests)
cd OEPlugins/paradigm-bridge
python -m pytest tests/ -v

# Open Ephys Python Tools tests (16 tests)
cd open-ephys-python-tools
python -m pytest tests/ -v

# Live protocol test (requires Open Ephys running)
python OEPlugins/paradigm-bridge/test_paradigm_bridge_demo.py --live
```

## Recommended Signal Chains

### Basic EEG Recording
```
[InEar Teensy Opt] → [Bandpass Filter] → [Record Node] → [LFP Viewer]
```

### EEG with Paradigm Triggers
```
[Source] → [Paradigm Bridge] → [Bandpass Filter] → [Record Node] → [LFP Viewer]
         ↑ TCP port 5557 (PsychoPy / MATLAB / Python scripts)
```

### EEG with LSL Streaming
```
[InEar Teensy Opt] → [Bandpass Filter] → [LSL Outlet] → [LFP Viewer]
```

### File Playback
```
[File Reader / EDF Source] → [Bandpass Filter] → [LFP Viewer]
```

## Protocol Comparison

| Feature | Original Protocol | Optimized Protocol |
|---------|-------------------|-------------------|
| Packet Size | Fixed 56 bytes | Variable 26–55 bytes |
| Bandwidth | 54.7 KB/s | ~28.5 KB/s (~49% savings) |
| Aux Data Rate | 1000 Hz (oversampled) | Native rates (10–250 Hz) |
| Plugin | InEar Teensy | InEar Teensy Opt |
| Firmware | `inear_teensy_firmware` | `inear_teensy_firmware_optimized` |

## Requirements

- Open Ephys GUI v1.0.1+ (Plugin API v10)
- Visual Studio 2022 (Windows) / GCC (Linux) / Clang (macOS)
- CMake 3.15+
- Python 3.8+ (3.10+ recommended; PsychoPy requires <3.12)
- Teensy 4.1 with Teensyduino (for hardware)

## License

GPL-3.0 (same as Open Ephys)
