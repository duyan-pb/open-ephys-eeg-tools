# LSL Investigation Tools

This folder contains Python scripts for investigating Lab Streaming Layer (LSL) streams, reading EDF files, and analyzing EEG annotation data.

## Setup

Install required dependencies:

```bash
pip install pylsl mne numpy pandas matplotlib
```

## Scripts

### 1. `investigate_lsl_streams.py`
Discover and examine LSL streams on the network.

```bash
# Discover all streams
python investigate_lsl_streams.py

# Create a test EEG stream (run in separate terminal)
python investigate_lsl_streams.py --create-test

# Create a test marker stream
python investigate_lsl_streams.py --create-markers

# Show full XML descriptions
python investigate_lsl_streams.py --xml

# Read more samples
python investigate_lsl_streams.py --read 100
```

**LSL Stream Properties:**
| Property | Description |
|----------|-------------|
| `name` | Human-readable stream name |
| `type` | Content type (EEG, Markers, Gaze, etc.) |
| `channel_count` | Number of channels per sample |
| `nominal_srate` | Sampling rate (0 = irregular/marker stream) |
| `channel_format` | Data type (float32, double64, string, int32, etc.) |
| `source_id` | Unique device/source identifier |
| `uid` | Unique stream instance ID |
| `hostname` | Machine providing the stream |
| `desc` | Extended XML metadata |

---

### 2. `read_edf_example.py`
Read EDF files, stream them via LSL, or convert to OpenEphys Binary format.

```bash
# Show EDF file information
python read_edf_example.py ../eeg_data/eeg1.edf

# List all EDF files in eeg_data folder
python read_edf_example.py --list

# Stream EDF via LSL (for testing)
python read_edf_example.py ../eeg_data/eeg1.edf --stream

# Stream at 2x speed
python read_edf_example.py ../eeg_data/eeg1.edf --stream --speed 2.0

# Convert to OpenEphys Binary format
python read_edf_example.py ../eeg_data/eeg1.edf --convert

# Plot a segment
python read_edf_example.py ../eeg_data/eeg1.edf --plot --start 10 --duration 5
```

---

### 3. `read_annotations.py`
Analyze EEG annotation CSV files and clinical information.

```bash
# Show all available data
python read_annotations.py

# List available files
python read_annotations.py --list

# Analyze annotation files
python read_annotations.py --annotations

# Show clinical information
python read_annotations.py --clinical

# Show summary statistics
python read_annotations.py --summary

# Show clinical data as table
python read_annotations.py --table
```

---

## Data Flow Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        DATA SOURCES                                 │
├─────────────────────────────────────────────────────────────────────┤
│  EDF Files          │  LSL Streams        │  OpenEphys Recording    │
│  (eeg_data/*.edf)   │  (Network)          │  (Binary format)        │
└─────────┬───────────┴─────────┬───────────┴───────────┬─────────────┘
          │                     │                       │
          ▼                     ▼                       ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        PYTHON TOOLS                                 │
├─────────────────────────────────────────────────────────────────────┤
│  read_edf_example.py    investigate_lsl_streams.py    open_ephys    │
│  - Read EDF info        - Discover streams            - Load Binary │
│  - Stream via LSL       - Read properties             - Get samples │
│  - Convert to Binary    - Read samples                - Get events  │
└─────────────────────────────────────────────────────────────────────┘
```

## LSL Data Handling in OpenEphys

The OpenEphys LSL plugin (`lab-streaming-layer-io`) handles data as follows:

1. **Discovery**: `lsl::resolve_streams()` finds all available streams
2. **Classification**: 
   - Streams with `nominal_srate > 0` → Data streams
   - Streams with `nominal_srate == 0` → Marker/event streams
3. **Data Reading**: `pull_chunk_multiplexed()` reads samples in chunks
4. **Format Conversion**: Multiplexed → Channel-major format
5. **Buffering**: Data goes to `DataBuffer` with timestamps

## OpenEphys Binary Format

```
recording_directory/
├── structure.oebin          # JSON metadata
├── continuous/
│   └── stream_name/
│       ├── continuous.dat   # Raw int16 [samples × channels]
│       ├── sample_numbers.npy
│       └── timestamps.npy
├── events/
│   └── processor_name/
│       └── TTL/
│           ├── states.npy
│           ├── sample_numbers.npy
│           └── timestamps.npy
└── spikes/
    └── electrode_name/
        ├── waveforms.npy
        └── sample_numbers.npy
```

## Channel Formats (LSL)

| Format | Description | Use Case |
|--------|-------------|----------|
| `cf_float32` | 32-bit float | Typical EEG (microvolts) |
| `cf_double64` | 64-bit double | High precision data |
| `cf_string` | Variable length | Event markers |
| `cf_int32` | 32-bit integer | Event codes |
| `cf_int16` | 16-bit integer | High-rate audio |
| `cf_int8` | 8-bit integer | Binary signals |

## Useful Links

- [LSL Documentation](https://labstreaminglayer.readthedocs.io/)
- [OpenEphys Documentation](https://open-ephys.github.io/gui-docs/)
- [OpenEphys Python Tools](https://github.com/open-ephys/open-ephys-python-tools)
- [MNE-Python (EDF reading)](https://mne.tools/)
