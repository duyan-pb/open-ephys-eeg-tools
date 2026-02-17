# Paradigm Bridge for Open Ephys

A Python plugin that bridges experiment paradigm software (PsychoPy, Expyriment, custom scripts) with the Open Ephys GUI. Provides unified recording control and trigger annotation capabilities.

## Features

| Feature | Method | Description |
|---------|--------|-------------|
| **Start/Stop Recording** | HTTP Server (port 37497) | Built into Open Ephys GUI, no plugin needed |
| **Trigger Annotations** | Network Events (ZMQ port 5556) | TTL events visible in LFP Viewer + saved in recordings |
| **Standalone GUI** | tkinter | User input window for manual control |
| **Device-Agnostic** | — | Works with InEar Teensy, OpenBCI Cyton, or any source |

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Your Paradigm Script                   │
│              (PsychoPy / Expyriment / custom)             │
└───────────────────────┬──────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│                   ParadigmBridge                          │
│  ┌────────────────────┐  ┌─────────────────────────────┐ │
│  │ RecordingController│  │    TriggerManager            │ │
│  │  (HTTP → port 37497│  │  (ZMQ → port 5556)          │ │
│  │  built-in server)  │  │  Network Events plugin)     │ │
│  └─────────┬──────────┘  └──────────────┬──────────────┘ │
└────────────┼────────────────────────────┼────────────────┘
             │                            │
             ▼                            ▼
┌──────────────────────────────────────────────────────────┐
│                     Open Ephys GUI                        │
│                                                           │
│  [Source] → [Network Events] → [Filter] → [Record Node]  │
│     ▲                                         │           │
│     │         InEar Teensy / OpenBCI Cyton     │           │
│     └─ hardware ───────────────────────────────┘           │
└──────────────────────────────────────────────────────────┘
```

## Quick Start

### 1. Install

```bash
cd OEPlugins/paradigm-bridge
pip install -e .
```

Or install dependencies directly:
```bash
pip install -r requirements.txt
```

### 2. Set Up Open Ephys Signal Chain

For trigger annotations to work, your signal chain must include the **Network Events** plugin:

```
[Source] → [Network Events] → [Bandpass Filter] → [Record Node] → [LFP Viewer]
```

Install Network Events via the Plugin Installer (`Ctrl+P` in Open Ephys GUI).

> **Note:** If you only need recording control (start/stop), Network Events is NOT required — the HTTP Server is built into the GUI.

### 3. Use in Your Paradigm

```python
from paradigm_bridge import ParadigmBridge

bridge = ParadigmBridge()
bridge.start_recording("my_experiment")

# Your paradigm logic here...
bridge.send_trigger(line=1, state=1)   # stimulus ON → visible in GUI
bridge.send_trigger(line=1, state=0)   # stimulus OFF

bridge.stop_recording()
bridge.close()
```

### 4. Or Use the Standalone GUI

```bash
# Launch the GUI window
python -m paradigm_bridge

# Or via entry point
paradigm-bridge-gui
```

## API Reference

### ParadigmBridge (Main Class)

```python
from paradigm_bridge import ParadigmBridge

bridge = ParadigmBridge(
    http_address="127.0.0.1",   # Open Ephys GUI address
    http_port=37497,             # HTTP Server port
    zmq_address="127.0.0.1",    # Network Events address
    zmq_port=5556,               # Network Events ZMQ port
    enable_triggers=True,        # Set False for recording-only mode
    verbose=False,               # Enable console logging
)
```

#### Recording Control

| Method | Description |
|--------|-------------|
| `bridge.start_recording(name)` | Start recording with optional directory name |
| `bridge.stop_recording()` | Stop recording, keep acquisition alive |
| `bridge.start_acquisition()` | Start data acquisition (IDLE → ACQUIRE) |
| `bridge.stop_acquisition()` | Stop everything (→ IDLE) |
| `bridge.status()` | Get current mode: `'IDLE'`, `'ACQUIRE'`, or `'RECORD'` |
| `bridge.set_recording_name(name)` | Set recording directory prepend text |
| `bridge.set_recording_directory(path)` | Set parent recording directory |
| `bridge.is_connected()` | Check HTTP Server connectivity |
| `bridge.wait_for_gui(timeout)` | Block until GUI is reachable |
| `bridge.check_signal_chain()` | Verify connection + list processors |

#### Trigger Annotations

| Method | TTL Line | Description |
|--------|----------|-------------|
| `bridge.send_trigger(line, state)` | Any (1–256) | Send raw TTL ON/OFF |
| `bridge.trigger_pulse(line, duration_ms)` | Any | Brief ON/OFF pulse |
| `bridge.stimulus_on(line)` | 1 (default) | Mark stimulus onset |
| `bridge.stimulus_off(line)` | 1 (default) | Mark stimulus offset |
| `bridge.response(line)` | 3 (default) | Mark participant response |
| `bridge.block_start(line)` | 5 (default) | Mark block start |
| `bridge.block_end(line)` | 5 (default) | Mark block end |
| `bridge.experiment_start(line)` | 6 (default) | Mark experiment start |
| `bridge.experiment_end(line)` | 6 (default) | Mark experiment end |

#### Paradigm Helpers

```python
# Automatic trigger bracketing around a trial function
bridge.run_trial(
    trial_func=my_stimulus_function,
    trial_number=1,
    trigger_line=1,
)
```

### RecordingController (Low-Level)

```python
from paradigm_bridge import RecordingController

rc = RecordingController(address="127.0.0.1", port=37497)
rc.start_recording("session_name")
rc.stop_recording()
```

### TriggerManager (Low-Level)

```python
from paradigm_bridge import TriggerManager

tm = TriggerManager(address="127.0.0.1", port=5556)
tm.send(line=1, state=1)
tm.pulse(line=2, duration_ms=5.0)
tm.close()
```

## Examples

| Example | File | Description |
|---------|------|-------------|
| Basic PsychoPy | [`examples/psychopy_basic.py`](examples/psychopy_basic.py) | Simple stimulus loop with triggers |
| ERP Oddball | [`examples/psychopy_erp_oddball.py`](examples/psychopy_erp_oddball.py) | Full oddball paradigm with blocks |
| Recording Only | [`examples/recording_only.py`](examples/recording_only.py) | No triggers, no PsychoPy — just record |
| OpenBCI Cyton | [`examples/openbci_cyton_paradigm.py`](examples/openbci_cyton_paradigm.py) | Device-agnostic demo |

## Signal Chain Examples

### OpenBCI Cyton (Alexey's first target)
```
[OpenBCI Cyton] → [Network Events] → [Bandpass Filter 1-50Hz] → [Record Node] → [LFP Viewer]
```

### InEar Teensy (once hardware triggers are ready)
```
[InEar Teensy Opt] → [Network Events] → [Bandpass Filter 1-50Hz] → [Record Node] → [LFP Viewer]
```

> **Note on InEar Teensy:** The Teensy board has hardware TTL lines (`TTL_IN` / `TTL_OUT` in the Compact Protocol TYPE+TTL byte). Once the Teensy firmware supports hardware triggers, those will appear as native TTL events in Open Ephys — independent of the software triggers sent by this plugin. You can use both simultaneously.

### Multi-modal
```
[OpenBCI Cyton (EEG)] ──→ [Merger] → [Network Events] → [Record Node]
[InEar Teensy (PPG)]  ──→ [Merger]
```

## Trigger Line Convention

Recommended TTL line assignments for EEG paradigms:

| Line | Purpose | Example |
|------|---------|---------|
| 1 | Standard stimulus onset/offset | Tone, visual stimulus |
| 2 | Deviant / target stimulus | Oddball target |
| 3 | Participant response | Button press |
| 4 | Feedback | Correct/incorrect indicator |
| 5 | Block start / end | Block boundaries |
| 6 | Experiment start / end | Session boundaries |
| 7–256 | User-defined | Condition codes, etc. |

## Comparison with ANT Neuro

Alexey mentioned replicating ANT Neuro's trigger annotation feature. Here's how the ParadigmBridge compares:

| Feature | ANT Neuro | ParadigmBridge |
|---------|-----------|----------------|
| Start/stop recording | eego SDK | HTTP Server API |
| Trigger annotations | Hardware trigger port | Network Events (ZMQ) |
| Visible in GUI | ✅ | ✅ (LFP Viewer shows TTL events) |
| Saved in recording | ✅ | ✅ (Binary/NWB format) |
| Software triggers | Via eego SDK | Via `send_trigger()` |
| Hardware triggers | BNC connector | Teensy TTL_IN/TTL_OUT (future) |

## Dependencies

| Package | Version | Purpose |
|---------|---------|---------|
| `requests` | ≥2.25.0 | HTTP communication with Open Ephys |
| `pyzmq` | ≥22.0.0 | ZMQ communication with Network Events |
| `psychopy` | (optional) | Stimulus presentation |

## License

MIT License — Free to use and modify.
