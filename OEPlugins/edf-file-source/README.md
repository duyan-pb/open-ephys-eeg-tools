# EDF/CSV File Source Plugin for Open Ephys

A FileSource plugin that enables loading and playback of EDF, BDF, CSV, and XZ-compressed files in Open Ephys.

## Supported Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| **EDF** | `.edf` | European Data Format (standard biosignal format) |
| **BDF** | `.bdf` | BioSemi Data Format (24-bit extension of EDF) |
| **EDF+** | `.edf` | EDF with annotations |
| **CSV** | `.csv` | Comma-separated values with header |
| **XZ** | `.csv.xz` | XZ-compressed CSV files |

## Features

- **Multi-channel support** - Load files with any number of channels
- **Automatic scaling** - Physical units are correctly scaled from digital values
- **Annotation support** - EDF+ annotations are preserved as events
- **Compression** - XZ-compressed files are decompressed on-the-fly
- **Looping** - Optionally loop file playback

## Building

### Prerequisites

- Open Ephys GUI source code (plugin-GUI)
- CMake 3.15+
- Visual Studio 2022 (Windows) or GCC/Clang (Linux/macOS)
- liblzma (for XZ support, included in `libs/`)

### Build Steps

```bash
# Windows
cd Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Deploy (requires admin)
Copy-Item "Release\edf-file-source.dll" "C:\Program Files\Open Ephys\plugins\" -Force
```

## Usage

1. **Load Plugin** - Processors → Sources → File Reader
2. **Select File** - Click browse and choose an EDF, BDF, or CSV file
3. **Configure** - Set playback options (loop, speed)
4. **Start Playback** - Click Play

### CSV Format Requirements

CSV files must have:
- Header row with channel names
- Numeric data in columns
- Consistent sample rate (no timestamps required)

Example:
```csv
EEG1,EEG2,EEG3,Accel_X,Accel_Y,Accel_Z
-12.5,14.2,-8.7,0.01,-0.02,0.98
-11.8,13.9,-9.1,0.02,-0.01,0.99
...
```

## Signal Chain Example

```
[File Reader (EDF)] → [Bandpass Filter] → [LFP Viewer]
```

## EDF/BDF File Structure

The plugin parses standard EDF/BDF headers:

| Field | Size | Description |
|-------|------|-------------|
| Version | 8B | "0       " (EDF) or "\xFFBIOSEMI" (BDF) |
| Patient ID | 80B | Patient information |
| Recording ID | 80B | Recording information |
| Start Date | 8B | dd.mm.yy |
| Start Time | 8B | hh.mm.ss |
| Header Bytes | 8B | Total header size |
| Reserved | 44B | "EDF+C" for continuous EDF+ |
| Num Records | 8B | Number of data records |
| Duration | 8B | Record duration in seconds |
| Num Signals | 4B | Number of channels |

## License

GPL-3.0 (same as Open Ephys)
