# LSL Investigation Toolkit - Comprehensive Documentation

## Table of Contents

1. [Overview](#overview)
2. [LSL Stream Properties - Complete Reference](#lsl-stream-properties---complete-reference)
3. [Python Scripts Reference](#python-scripts-reference)
4. [Data Formats](#data-formats)
5. [OpenEphys Integration](#openephys-integration)
6. [Usage Examples](#usage-examples)
7. [Troubleshooting](#troubleshooting)

---

## Overview

This toolkit provides Python scripts for investigating Lab Streaming Layer (LSL) streams, reading EEG data files (EDF format), and understanding how data flows through OpenEphys.

### Installation

```bash
pip install -r requirements.txt
```

Required packages:
- `pylsl` - Lab Streaming Layer Python bindings
- `mne` - EEG/MEG data processing (for EDF reading)
- `numpy` - Numerical computing
- `pandas` - Data manipulation
- `matplotlib` - Visualization

---

## LSL Stream Properties - Complete Reference

### StreamInfo Constructor

```python
import pylsl

info = pylsl.StreamInfo(
    name='MyStream',           # Human-readable name
    type='EEG',                # Content type
    channel_count=8,           # Number of channels
    nominal_srate=256.0,       # Sampling rate (Hz), 0 for irregular
    channel_format=pylsl.cf_float32,  # Data type
    source_id='device-001'     # Unique device identifier
)
```

### Core Properties (Required)

| Property | Method | Type | Description |
|----------|--------|------|-------------|
| **Name** | `name()` | string | Human-readable stream name (e.g., "OpenEphys EEG") |
| **Type** | `type()` | string | Content type identifier |
| **Channel Count** | `channel_count()` | int | Number of channels per sample |
| **Sampling Rate** | `nominal_srate()` | float | Hz; 0 = irregular/event stream |
| **Channel Format** | `channel_format()` | enum | Data type (see table below) |
| **Source ID** | `source_id()` | string | Unique device/source identifier |

### Channel Format Types

| Constant | Value | Description | Bytes/sample |
|----------|-------|-------------|--------------|
| `cf_float32` | 1 | 32-bit floating point | 4 |
| `cf_double64` | 2 | 64-bit double precision | 8 |
| `cf_string` | 3 | Variable-length string | varies |
| `cf_int32` | 4 | 32-bit signed integer | 4 |
| `cf_int16` | 5 | 16-bit signed integer | 2 |
| `cf_int8` | 6 | 8-bit signed integer | 1 |
| `cf_int64` | 7 | 64-bit signed integer | 8 |
| `cf_undefined` | 0 | Undefined format | - |

### Stream Type Categories

Common stream types (conventions, not enforced):

| Type | Description | Typical Format | Typical Rate |
|------|-------------|----------------|--------------|
| `EEG` | Electroencephalography | float32 | 256-2048 Hz |
| `EMG` | Electromyography | float32 | 1000-4000 Hz |
| `ECG` | Electrocardiography | float32 | 250-1000 Hz |
| `EOG` | Electrooculography | float32 | 250-500 Hz |
| `Gaze` | Eye tracking | float32 | 60-1000 Hz |
| `Audio` | Audio signals | float32/int16 | 44100 Hz |
| `Markers` | Event markers | string | 0 (irregular) |
| `VideoRaw` | Raw video frames | int8 | 30-60 Hz |
| `MoCap` | Motion capture | float32 | 100-200 Hz |

### Network/Session Properties (Auto-assigned)

| Property | Method | Type | Description |
|----------|--------|------|-------------|
| **UID** | `uid()` | string | Auto-generated unique stream instance ID |
| **Hostname** | `hostname()` | string | Machine name hosting the stream |
| **Session ID** | `session_id()` | string | Optional session grouping identifier |
| **Version** | `version()` | int | LSL protocol version (e.g., 110 = 1.10) |
| **Created At** | `created_at()` | float | Stream creation timestamp |

### Channel Metadata Methods

```python
# Setting metadata (on StreamInfo before creating outlet)
info.set_channel_labels(['Fp1', 'Fp2', 'C3', 'C4', 'O1', 'O2', 'T3', 'T4'])
info.set_channel_types(['EEG', 'EEG', 'EEG', 'EEG', 'EEG', 'EEG', 'EEG', 'EEG'])
info.set_channel_units(['uV', 'uV', 'uV', 'uV', 'uV', 'uV', 'uV', 'uV'])

# Getting metadata (from StreamInfo after resolving)
labels = info.get_channel_labels()  # Returns list of strings
types = info.get_channel_types()    # Returns list of strings
units = info.get_channel_units()    # Returns list of strings
```

### XML Description Structure

The `desc()` method returns an XML element for custom metadata:

```python
desc = info.desc()

# Add simple values
desc.append_child_value('manufacturer', 'OpenEphys')
desc.append_child_value('model', 'Acquisition Board')

# Add nested structure
acquisition = desc.append_child('acquisition')
acquisition.append_child_value('compensated_lag', '0.005')

# Add channel metadata (alternative to convenience methods)
channels = desc.append_child('channels')
for i, name in enumerate(['Fp1', 'Fp2', 'C3', 'C4']):
    ch = channels.append_child('channel')
    ch.append_child_value('label', name)
    ch.append_child_value('type', 'EEG')
    ch.append_child_value('unit', 'uV')
    ch.append_child_value('index', str(i))
```

### Full XML Structure

```xml
<?xml version="1.0"?>
<info>
    <!-- Core properties -->
    <name>MyStream</name>
    <type>EEG</type>
    <channel_count>4</channel_count>
    <channel_format>float32</channel_format>
    <source_id>device-001</source_id>
    <nominal_srate>256.0</nominal_srate>
    
    <!-- Network/session (auto-assigned when stream goes live) -->
    <version>1.10</version>
    <created_at>1234567890.123</created_at>
    <uid>abc123-def456-ghi789</uid>
    <session_id></session_id>
    <hostname>MyComputer</hostname>
    
    <!-- Network endpoints (assigned by LSL) -->
    <v4address>192.168.1.100</v4address>
    <v4data_port>16573</v4data_port>
    <v4service_port>16572</v4service_port>
    <v6address>::1</v6address>
    <v6data_port>16575</v6data_port>
    <v6service_port>16574</v6service_port>
    
    <!-- Custom description (user-defined) -->
    <desc>
        <channels>
            <channel>
                <label>Fp1</label>
                <type>EEG</type>
                <unit>uV</unit>
            </channel>
            <!-- ... more channels ... -->
        </channels>
        <manufacturer>OpenEphys</manufacturer>
        <model>Acquisition Board</model>
        <acquisition>
            <compensated_lag>0.005</compensated_lag>
        </acquisition>
    </desc>
</info>
```

---

## Python Scripts Reference

### 1. investigate_lsl_streams.py

Discover and investigate LSL streams on the network.

```bash
# Discover all streams
python investigate_lsl_streams.py

# With verbose network info
python investigate_lsl_streams.py --verbose

# Show full XML descriptions
python investigate_lsl_streams.py --xml

# Read samples from discovered streams
python investigate_lsl_streams.py --read 20

# Create a test EEG stream (for testing)
python investigate_lsl_streams.py --create-test

# Create a test marker stream
python investigate_lsl_streams.py --create-markers

# Adjust discovery wait time
python investigate_lsl_streams.py --wait 5.0
```

**Key Functions:**

| Function | Description |
|----------|-------------|
| `discover_streams(wait_time)` | Find all LSL streams on network |
| `print_stream_info(info, verbose)` | Display stream properties |
| `print_stream_xml(info)` | Pretty-print XML description |
| `read_channel_metadata(inlet)` | Extract channel labels/types/units |
| `read_samples_from_stream(info, n)` | Read n samples from a stream |
| `create_test_outlet()` | Create test EEG stream |
| `create_marker_outlet()` | Create test marker stream |

### 2. read_edf_example.py

Read EDF files, stream via LSL, and convert to OpenEphys Binary format.

```bash
# List all EDF files from both data directories
python read_edf_example.py --list

# Show info about an EDF file
python read_edf_example.py eeg1.edf

# Stream EDF file via LSL
python read_edf_example.py eeg1.edf --stream

# Stream in loop mode
python read_edf_example.py eeg1.edf --stream --loop

# Convert EDF to OpenEphys Binary format
python read_edf_example.py eeg1.edf --convert

# Specify output directory
python read_edf_example.py eeg1.edf --convert --output ./output_data
```

**Key Functions:**

| Function | Description |
|----------|-------------|
| `list_all_edf_files()` | List EDF files from all data directories |
| `find_edf_file(filename)` | Search for EDF file in all directories |
| `read_edf_info(filepath)` | Read EDF file and display info |
| `stream_edf_via_lsl(filepath, loop)` | Create LSL outlet and stream data |
| `convert_edf_to_binary(filepath, output)` | Convert to OpenEphys format |

**Supported Data Directories:**
- `eeg_data/` - Neonatal seizure EEG dataset (79 files)
- `eeg_data_2/EDF_format/EDF_format/` - HIE graded dataset (169 files)

### 3. read_annotations.py

Analyze EEG annotation and clinical information files.

```bash
# Overview of all data files
python read_annotations.py

# List files from all directories
python read_annotations.py --list

# Show clinical information (eeg_data)
python read_annotations.py --clinical

# Show metadata (eeg_data_2)
python read_annotations.py --metadata

# Show EEG grades (eeg_data_2)
python read_annotations.py --grades

# Show summary statistics
python read_annotations.py --summary

# Show clinical data as table
python read_annotations.py --table

# Analyze specific file
python read_annotations.py --file annotations_2017_A_fixed.csv
```

**Key Functions:**

| Function | Description |
|----------|-------------|
| `list_available_files()` | List all data files from both directories |
| `load_annotations(filepath)` | Load and analyze annotation CSV |
| `load_clinical_info(filepath)` | Load clinical_information.csv |
| `load_metadata_eeg_data_2(filepath)` | Load metadata.csv (eeg_data_2) |
| `load_eeg_grades(filepath)` | Load eeg_grades.csv |
| `show_clinical_table(df)` | Display formatted clinical table |

---

## Data Formats

### EDF (European Data Format)

Standard format for biosignal recordings.

**Structure:**
```
[Header - 256 bytes fixed]
  - Version (8 bytes)
  - Patient ID (80 bytes)
  - Recording ID (80 bytes)
  - Start date/time (16 bytes)
  - Number of data records
  - Duration of data record
  - Number of signals

[Signal Headers - 256 bytes per signal]
  - Label (16 bytes)
  - Transducer type (80 bytes)
  - Physical dimension/unit (8 bytes)
  - Physical min/max (16 bytes)
  - Digital min/max (16 bytes)
  - Prefiltering (80 bytes)
  - Number of samples per record

[Data Records]
  - Interleaved signal data (int16)
```

**Reading with MNE:**
```python
import mne

raw = mne.io.read_raw_edf('eeg1.edf', preload=True)
print(f"Channels: {raw.ch_names}")
print(f"Sample rate: {raw.info['sfreq']} Hz")
print(f"Duration: {raw.times[-1]} seconds")
data = raw.get_data()  # Shape: (n_channels, n_samples)
```

### OpenEphys Binary Format

Used by OpenEphys GUI for continuous recordings.

**Directory Structure:**
```
recording_directory/
├── structure.oebin              # JSON metadata
└── continuous/
    └── stream_name/
        ├── continuous.dat       # Raw data (int16, interleaved)
        ├── sample_numbers.npy   # Sample indices
        └── timestamps.npy       # Timestamps in seconds
```

**structure.oebin Format:**
```json
{
  "GUI version": "0.6.0",
  "continuous": [
    {
      "folder_name": "Rhythm_FPGA-100.0",
      "sample_rate": 30000.0,
      "num_channels": 16,
      "source_processor_name": "Rhythm FPGA",
      "channels": [
        {"channel_name": "CH1", "units": "uV", "bit_volts": 0.195}
      ]
    }
  ],
  "events": [...],
  "spikes": [...]
}
```

**Reading OpenEphys Binary:**
```python
import numpy as np
import json

# Read metadata
with open('structure.oebin', 'r') as f:
    metadata = json.load(f)

# Read continuous data
n_channels = metadata['continuous'][0]['num_channels']
data = np.memmap('continuous.dat', dtype='int16', mode='r')
data = data.reshape(-1, n_channels).T

# Apply scaling
bit_volts = metadata['continuous'][0]['channels'][0]['bit_volts']
data_uv = data.astype(np.float32) * bit_volts

# Read timestamps
timestamps = np.load('timestamps.npy')
```

### Annotation CSV Format (eeg_data)

Binary seizure annotations per sample.

```csv
A,B,C
0,0,0
0,0,0
1,1,0    # Annotators A and B marked seizure
1,1,1    # All three annotators marked seizure
```

- Columns A, B, C = different expert annotators
- 0 = no seizure, 1 = seizure at this sample
- Sample rate matches EEG recording (typically 256 Hz)

### Metadata CSV Format (eeg_data_2)

```csv
file_ID,baby_ID,epoch_number,grade,sampling_freq,reference,quality_comments,seizures_YN,seizure_comments
ID01_epoch1,ID01,1,2,256,Cz,good quality,N,
ID01_epoch2,ID01,2,2,256,Cz,some artifact,N,
```

| Column | Description |
|--------|-------------|
| `file_ID` | EDF filename (without extension) |
| `baby_ID` | Patient identifier |
| `epoch_number` | 1-hour epoch number |
| `grade` | HIE severity (1=normal, 2=moderate, 3=severe, 4=inactive) |
| `sampling_freq` | 200 or 256 Hz |
| `reference` | EEG reference electrode |
| `quality_comments` | Recording quality notes |
| `seizures_YN` | Y/N for seizure presence |

---

## OpenEphys Integration

### LSL Plugin (lab-streaming-layer-io)

The OpenEphys LSL plugin receives LSL streams and integrates them into the signal chain.

**Plugin Location:** `lab-streaming-layer-io/Source/`

**Key Components:**

1. **LSLThread** - Network thread for receiving LSL data
   - Resolves streams by name/type
   - Creates StreamInlet for data reception
   - Handles timestamps and synchronization

2. **LSLProcessor** - Signal chain processor
   - Buffers incoming samples
   - Converts to OpenEphys data format
   - Manages channel metadata

**Data Flow:**
```
LSL Outlet → Network → LSL Inlet → Buffer → OpenEphys Signal Chain
```

### Streaming EDF to OpenEphys

1. Start the EDF streamer:
```bash
python read_edf_example.py eeg1.edf --stream --loop
```

2. In OpenEphys:
   - Add "LSL Inlet" source
   - Configure to find stream by name
   - Connect to Record Node or other processors

### Converting EDF to OpenEphys Format

```bash
python read_edf_example.py eeg1.edf --convert --output ./converted
```

This creates:
- `structure.oebin` - Metadata JSON
- `continuous/EDF_Stream-XXX/continuous.dat` - Raw data
- `continuous/EDF_Stream-XXX/timestamps.npy` - Timestamps

---

## Usage Examples

### Example 1: Discover and Inspect Streams

```python
import pylsl

# Find all streams
streams = pylsl.resolve_streams(wait_time=2.0)

for stream in streams:
    print(f"Found: {stream.name()} ({stream.type()})")
    print(f"  Channels: {stream.channel_count()}")
    print(f"  Rate: {stream.nominal_srate()} Hz")
    print(f"  Format: {stream.channel_format()}")
    print(f"  Labels: {stream.get_channel_labels()}")
```

### Example 2: Create an EEG Outlet

```python
import pylsl
import numpy as np
import time

# Create stream info
info = pylsl.StreamInfo(
    name='MyEEG',
    type='EEG',
    channel_count=8,
    nominal_srate=256,
    channel_format=pylsl.cf_float32,
    source_id='my-eeg-device'
)

# Add channel metadata
info.set_channel_labels(['Fp1', 'Fp2', 'C3', 'C4', 'P3', 'P4', 'O1', 'O2'])
info.set_channel_units(['uV'] * 8)

# Create outlet
outlet = pylsl.StreamOutlet(info)

# Stream data
while True:
    sample = np.random.randn(8).astype(np.float32) * 50  # Fake EEG
    outlet.push_sample(sample)
    time.sleep(1/256)  # Maintain sampling rate
```

### Example 3: Read from a Stream

```python
import pylsl
import numpy as np

# Find EEG streams
streams = pylsl.resolve_byprop('type', 'EEG', timeout=5.0)
if not streams:
    raise RuntimeError("No EEG stream found")

# Create inlet
inlet = pylsl.StreamInlet(streams[0])

# Get stream info
info = inlet.info()
n_channels = info.channel_count()
srate = info.nominal_srate()

# Read data
print(f"Reading from {info.name()}...")
samples = []
timestamps = []

for _ in range(1000):
    sample, ts = inlet.pull_sample(timeout=1.0)
    if sample:
        samples.append(sample)
        timestamps.append(ts)

data = np.array(samples)
print(f"Received {data.shape[0]} samples")
```

### Example 4: Stream EDF File

```python
import mne
import pylsl
import time
import numpy as np

# Load EDF
raw = mne.io.read_raw_edf('eeg1.edf', preload=True)
data = raw.get_data()
srate = raw.info['sfreq']

# Create LSL outlet
info = pylsl.StreamInfo(
    name='EDF_Stream',
    type='EEG',
    channel_count=len(raw.ch_names),
    nominal_srate=srate,
    channel_format=pylsl.cf_float32,
    source_id='edf-file'
)
info.set_channel_labels(raw.ch_names)

outlet = pylsl.StreamOutlet(info)

# Stream samples
for i in range(data.shape[1]):
    sample = data[:, i].astype(np.float32)
    outlet.push_sample(sample)
    time.sleep(1/srate)
```

---

## Troubleshooting

### No Streams Found

1. **Check firewall settings** - LSL uses UDP multicast
2. **Verify network** - Streams only visible on same subnet
3. **Increase wait time** - `pylsl.resolve_streams(wait_time=10.0)`
4. **Check if outlet exists** - Run `--create-test` in another terminal

### Stream Data Issues

1. **Buffer overflow** - Increase inlet buffer: `StreamInlet(info, max_buflen=360)`
2. **Timestamp issues** - Use `inlet.time_correction()` for sync
3. **Wrong data type** - Verify `channel_format` matches expected type

### EDF Reading Errors

1. **Install MNE** - `pip install mne`
2. **Memory issues** - Use `preload=False` for large files
3. **Channel issues** - Check `raw.ch_names` matches expected

### OpenEphys Connection

1. **Plugin not finding stream** - Verify stream name matches exactly
2. **No data** - Check if outlet is actively pushing samples
3. **Wrong sample rate** - Verify `nominal_srate` is set correctly

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────────┐
│                    LSL StreamInfo Quick Reference                   │
├─────────────────────────────────────────────────────────────────────┤
│ CORE PROPERTIES                                                     │
│   name()           → Stream name (string)                           │
│   type()           → Content type (EEG, Markers, etc.)              │
│   channel_count()  → Number of channels (int)                       │
│   nominal_srate()  → Sampling rate Hz, 0=irregular (float)          │
│   channel_format() → Data type enum (cf_float32, etc.)              │
│   source_id()      → Device identifier (string)                     │
├─────────────────────────────────────────────────────────────────────┤
│ NETWORK PROPERTIES (auto-assigned)                                  │
│   uid()            → Unique stream ID                               │
│   hostname()       → Host machine name                              │
│   session_id()     → Session grouping ID                            │
│   version()        → LSL protocol version                           │
│   created_at()     → Creation timestamp                             │
├─────────────────────────────────────────────────────────────────────┤
│ CHANNEL METADATA                                                    │
│   get_channel_labels() / set_channel_labels([...])                  │
│   get_channel_types()  / set_channel_types([...])                   │
│   get_channel_units()  / set_channel_units([...])                   │
├─────────────────────────────────────────────────────────────────────┤
│ XML METHODS                                                         │
│   as_xml()         → Full XML as string                             │
│   desc()           → XML description element (for custom metadata)  │
├─────────────────────────────────────────────────────────────────────┤
│ CHANNEL FORMATS                                                     │
│   cf_float32 (1)   cf_double64 (2)   cf_string (3)                  │
│   cf_int32 (4)     cf_int16 (5)      cf_int8 (6)     cf_int64 (7)   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Version History

- **v1.0** (2026-01-28) - Initial release with full LSL property documentation
- Scripts: `investigate_lsl_streams.py`, `read_edf_example.py`, `read_annotations.py`
- Support for `eeg_data` and `eeg_data_2` directories
