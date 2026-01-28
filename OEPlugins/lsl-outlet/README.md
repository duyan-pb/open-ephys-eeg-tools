# LSL Outlet Plugin (Standalone)

A standalone Open Ephys plugin that streams data via Lab Streaming Layer (LSL) to external applications.

## Features

- **Stream continuous data** - All channels from your signal chain are streamed via LSL
- **TTL markers** - TTL events are optionally streamed as string markers
- **Channel metadata** - Channel names and units are included in the LSL stream info
- **Configurable** - Set custom stream name and type
- **Multi-stream support** - Creates separate LSL outlets for each data stream

## Installation

### Prerequisites
- Open Ephys GUI (built from source or with development headers)
- CMake 3.15+
- Visual Studio 2022 (Windows) or GCC/Clang (Linux/macOS)

### Build

```bash
cd lsl-outlet/Build

# Windows
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release --target install

# Linux/macOS
cmake ..
make -j4
make install
```

## Usage

1. Add **LSL Outlet** to your signal chain (found under Sinks)
2. Configure:
   - **Stream Name**: Base name for LSL streams (default: "OpenEphys")
   - **Stream Type**: Content type, e.g., "EEG", "EMG" (default: "EEG")
   - **TTL Markers**: Toggle whether to stream TTL events
3. Press Play to start streaming
4. Connect from external applications using LSL

### Receiving Data

The plugin creates LSL outlets that can be discovered by any LSL-compatible application:

```python
import pylsl

# Find Open Ephys streams
streams = pylsl.resolve_byprop('type', 'EEG', timeout=5.0)
for stream in streams:
    print(f"Found: {stream.name()}")

# Create inlet and receive data
inlet = pylsl.StreamInlet(streams[0])
while True:
    sample, timestamp = inlet.pull_sample()
    print(sample)
```

### LSL Stream Details

**Data Streams:**
- Name: `{StreamName}_{DataStreamName}` (e.g., "OpenEphys_Rhythm_FPGA")
- Type: As configured (default: "EEG")
- Format: float32, interleaved
- Metadata: Channel labels, units, acquisition info

**Marker Stream:**
- Name: `{StreamName}_Markers` (e.g., "OpenEphys_Markers")
- Type: "Markers"
- Format: String, irregular rate
- Values: `TTL_Line{N}_State{0|1}` (e.g., "TTL_Line1_State1")

## Configuration Persistence

Settings are automatically saved/loaded with your Open Ephys signal chain configuration.

## Version History

- **v1.0.0** - Initial standalone release
  - Continuous data streaming
  - TTL marker support
  - Parameter persistence
