# Open Ephys Plugins

Custom plugins for Open Ephys GUI providing various data sources and processing capabilities.

## Plugin Overview

| Plugin | Type | Description | Status |
|--------|------|-------------|--------|
| **[inear-teensy-source](inear-teensy-source/)** | DataThread | Teensy EEG source (56-byte fixed) | ✅ Production |
| **[inear-teensy-source-optimized](inear-teensy-source-optimized/)** | DataThread | Teensy EEG source (variable, 49% savings) | ✅ Production |
| **[edf-file-source](edf-file-source/)** | FileSource | EDF/BDF/CSV file loader | ✅ Production |
| **[lsl-outlet](lsl-outlet/)** | Processor | LSL streaming sink | ✅ Production |
| **[lab-streaming-layer-io](lab-streaming-layer-io/)** | Mixed | LSL inlet + outlet | ✅ Production |
| **[custom-ic-source](custom-ic-source/)** | DataThread | IC hardware integration | 🔧 Development |
| **[openbci-cyton](openbci-cyton/)** | DataThread | OpenBCI Cyton support | 🔧 Development |

## Quick Build (Windows)

```powershell
# Build a single plugin
cd <plugin-folder>\Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Deploy (requires admin or close Open Ephys first)
Copy-Item "Release\<plugin-name>.dll" "C:\Program Files\Open Ephys\plugins\" -Force
```

## Build All Plugins

```powershell
# Build all main plugins
$plugins = @("inear-teensy-source", "inear-teensy-source-optimized", "edf-file-source", "lsl-outlet")
foreach ($p in $plugins) {
    Push-Location "$p\Build"
    cmake --build . --config Release
    Pop-Location
}

# Deploy all (requires admin)
Start-Process powershell -Verb RunAs -ArgumentList "-Command", @"
Copy-Item 'inear-teensy-source\Build\Release\*.dll' 'C:\Program Files\Open Ephys\plugins\' -Force
Copy-Item 'inear-teensy-source-optimized\Build\Release\*.dll' 'C:\Program Files\Open Ephys\plugins\' -Force
Copy-Item 'edf-file-source\Build\Release\*.dll' 'C:\Program Files\Open Ephys\plugins\' -Force
Copy-Item 'lsl-outlet\Build\Release\*.dll' 'C:\Program Files\Open Ephys\plugins\' -Force
"@
```

## Plugin Categories

### Data Sources (DataThread)
- Read data from hardware or virtual sources
- Provide continuous sample streams to Open Ephys

### File Sources (FileSource)
- Load recorded data from files
- Support various formats (EDF, BDF, CSV)

### Processors
- Process data in the signal chain
- Output to external systems (LSL)

## Recommended Signal Chains

### Basic EEG Recording
```
[InEar Teensy Opt] → [Bandpass Filter] → [Record Node]
```

### EEG with LSL Streaming
```
[InEar Teensy Opt] → [Bandpass Filter] → [LSL Outlet] → [LFP Viewer]
```

### File Playback
```
[File Reader (EDF)] → [Bandpass Filter] → [LFP Viewer]
```

## Requirements

- Open Ephys GUI v1.0.1+
- CMake 3.15+
- Visual Studio 2022 (Windows) / GCC (Linux) / Clang (macOS)
- Plugin API v10

## License

Plugins are licensed under MIT or GPL-3.0 (same as Open Ephys).
