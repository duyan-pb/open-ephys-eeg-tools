# Open Ephys EEG Tools & Plugins

A collection of custom Open Ephys plugins and Python tools for EEG data processing.

## Components

### Custom Open Ephys Plugins (`OEPlugins/`)

| Plugin | Type | Description |
|--------|------|-------------|
| **edf-file-source** | FileSource | Load EDF, BDF, CSV, and XZ-compressed files |
| **lab-streaming-layer-io** | DataThread + Processor | LSL Inlet (source) and LSL Outlet (sink) |
| **lsl-outlet** | Processor | Standalone LSL Outlet sink plugin |
| **custom-ic-source** | DataThread | Native serial IC hardware integration |
| **custom_python_ic_connector** | Python | Python bridge for hardware via LSL |
| **lsl_investigation** | Python | LSL testing tools and documentation |

### Python Scripts (Root)

- `edf_to_lsl_streamer.py` - Stream EDF files via LSL
- `lsl_eeg_streamer.py` - Generic LSL EEG streamer
- `decompress_csv.py` - XZ decompression utility

### Submodules

- `plugin-GUI/` - Open Ephys GUI (v1.0.1)
- `open-ephys-python-tools/` - Open Ephys Python analysis tools

## Building Plugins (Windows)

```powershell
# Prerequisites: Visual Studio 2022, CMake

# Build a plugin
cd OEPlugins/<plugin-name>/Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release --target install
```

Plugins are installed to `plugin-GUI/Build/Release/plugins/`.

## Requirements

- Open Ephys GUI v1.0.1+
- Visual Studio 2022 (Windows)
- Python 3.10+ with pylsl, mne, numpy

## License

GPL-3.0 (same as Open Ephys)
