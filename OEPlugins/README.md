# Open Ephys Plugins

Custom plugins for Open Ephys GUI providing data sources, paradigm integration, LSL streaming, and file I/O.

> **Last build:** February 23, 2026 — all 8 C++ plugins built and installed to `plugin-GUI/Build/Release/plugins/`.

## Plugin Overview

### C++ Plugins (DLLs)

| Plugin | Type | Description | Status |
|--------|------|-------------|--------|
| **[inear-teensy-source](inear-teensy-source/)** | DataThread | Teensy EEG source (56-byte fixed @ 1 kHz) | ✅ Production |
| **[inear-teensy-source-optimized](inear-teensy-source-optimized/)** | DataThread | Teensy EEG source (variable 26–55 bytes, 49% savings) | ✅ Production |
| **[openbci-cyton](openbci-cyton/)** | DataThread | OpenBCI Cyton board (8/16 ch, 250 Hz) | ✅ Production |
| **[custom-ic-source](custom-ic-source/)** | DataThread | Generic serial IC hardware integration | ✅ Production |
| **[edf-file-source](edf-file-source/)** | FileSource | EDF/BDF/CSV/XZ file loader | ✅ Production |
| **[lab-streaming-layer-io](lab-streaming-layer-io/)** | DataThread + Processor | LSL Inlet (source) + LSL Outlet (sink) | ✅ Production |
| **[lsl-outlet](lsl-outlet/)** | Processor | LSL streaming sink | ✅ Production |
| **[paradigm-bridge-cpp](paradigm-bridge-cpp/)** | Processor | Native C++ paradigm bridge — TCP triggers + recording control | ✅ Production |

### Python Packages

| Package | Version | Description | Status |
|---------|---------|-------------|--------|
| **[paradigm-bridge](paradigm-bridge/)** | 2.0.0 | PsychoPy/Expyriment ↔ Open Ephys (HTTP recording + TCP triggers) | ✅ Production |
| **[custom-python-ic-connector](custom_python_ic_connector/)** | 1.0.0 | Streams custom IC data to Open Ephys via LSL | ✅ Production |

### Investigation/Utility

| Tool | Description |
|------|-------------|
| **[lsl_investigation](lsl_investigation/)** | Python scripts for investigating LSL streams, reading EDF files |

### Releases

| Release | Description |
|---------|-------------|
| **[paradigm-bridge-v2.0.0](releases/paradigm-bridge-v2.0.0/)** | Standalone release bundle (DLL + PsychoPy scripts, no pip install) |

## Build All Plugins (Windows)

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
    Push-Location "$p\Build"
    cmake --build . --config Release
    cmake --install . --config Release
    Pop-Location
}
```

DLLs are installed to `plugin-GUI/Build/Release/plugins/` automatically.

## Build a Single Plugin

```powershell
cd <plugin-folder>\Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
cmake --install . --config Release
```

## Install Python Packages

```powershell
# Create a virtual environment (from workspace root)
python -m venv .venv
.venv\Scripts\Activate.ps1

# Install paradigm-bridge with dev tools
pip install -e paradigm-bridge[dev]

# Install custom IC connector
pip install -e custom_python_ic_connector[dev]
```

## Run Tests

```powershell
# Paradigm Bridge (4 tests)
cd paradigm-bridge
python -m pytest tests/ -v

# Live protocol test against Open Ephys
python test_paradigm_bridge_demo.py --live
```

## Plugin Categories

### Data Sources (DataThread)
- Read data from hardware (Teensy, OpenBCI, custom ICs) or virtual sources (LSL)
- Provide continuous sample streams to the Open Ephys signal chain

### File Sources (FileSource)
- Load recorded data from EDF, BDF, CSV, and XZ-compressed files
- Enable offline analysis and signal chain testing

### Processors (Filter)
- Process or augment data in the signal chain
- Paradigm Bridge: inject TTL events from external scripts
- LSL Outlet: stream data out to external applications

### Python Bridges
- Connect external Python software (PsychoPy, Expyriment) to Open Ephys
- Control recording, send TTL triggers, read status

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

### OpenBCI with Recording
```
[OpenBCI Cyton] → [Bandpass Filter] → [Record Node] → [LFP Viewer]
```

## Requirements

- Open Ephys GUI v1.0.1+ (Plugin API v10)
- CMake 3.15+
- Visual Studio 2022 (Windows) / GCC (Linux) / Clang (macOS)
- Python 3.8+ (for Python packages)

## License

Plugins are licensed under GPL-3.0 (same as Open Ephys).
