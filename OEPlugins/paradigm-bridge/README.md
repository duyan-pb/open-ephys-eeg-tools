# Paradigm Bridge for Open Ephys

A Python package that bridges experiment paradigm software (PsychoPy, Expyriment, custom scripts) with the Open Ephys GUI. Provides unified recording control and TTL trigger annotation via the **Paradigm Bridge** C++ plugin.

## Features

| Feature | Method | Description |
|---------|--------|-------------|
| **Start/Stop Recording** | HTTP Server (port 37497) | Built into Open Ephys GUI, no plugin needed |
| **TTL Triggers** | TCP (port 5557) | Sent to Paradigm Bridge C++ plugin, visible in LFP Viewer |
| **Standalone GUI** | tkinter | User input window for manual control |
| **Device-Agnostic** | — | Works with InEar Teensy, OpenBCI Cyton, or any source |
| **Legacy ZMQ** | ZMQ (port 5556, optional) | Fallback via Network Events plugin (requires pyzmq) |

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
│  │ RecordingController│  │    TcpTriggerClient          │ │
│  │  (HTTP → port 37497│  │  (TCP → port 5557)          │ │
│  │  built-in server)  │  │  paradigm-bridge-cpp plugin) │ │
│  └─────────┬──────────┘  └──────────────┬──────────────┘ │
└────────────┼────────────────────────────┼────────────────┘
             │                            │
             ▼                            ▼
┌──────────────────────────────────────────────────────────┐
│                     Open Ephys GUI                        │
│                                                           │
│  [Source] → [Paradigm Bridge] → [Filter] → [Record Node] │
│     ▲              │                          │           │
│     │         TTL events via TCP              │           │
│     └─ hardware ──────────────────────────────┘           │
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

> **Note:** The default TCP backend uses only Python stdlib (`socket`). No extra packages needed beyond `requests` (for HTTP recording control).

### 2. Set Up Open Ephys Signal Chain

Your signal chain must include the **Paradigm Bridge** C++ plugin:

```
[Source] → [Paradigm Bridge] → [Bandpass Filter] → [Record Node] → [LFP Viewer]
```

Build and install the `paradigm-bridge-cpp` plugin from `OEPlugins/paradigm-bridge-cpp/`.

> **Recording-only mode:** If you only need start/stop recording, no plugin is needed — the HTTP Server is built into the GUI. Use `ParadigmBridge(trigger_backend=None)`.

### 3. Use in Your Paradigm

```python
from paradigm_bridge import ParadigmBridge

bridge = ParadigmBridge()                      # TCP by default
bridge.start_recording("my_experiment")

# Your paradigm logic here...
bridge.send_trigger(line=0, state=1)           # stimulus ON → TTL event in GUI
bridge.send_trigger(line=0, state=0)           # stimulus OFF

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
    address="127.0.0.1",         # Open Ephys GUI address
    http_port=37497,             # HTTP Server port
    tcp_port=5557,               # Paradigm Bridge TCP port
    trigger_backend="tcp",       # "tcp" (default), "zmq", or None
    zmq_port=5556,               # Network Events ZMQ port (only if backend="zmq")
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

#### TTL Triggers (TCP → paradigm-bridge-cpp)

| Method | TTL Line | Description |
|--------|----------|-------------|
| `bridge.send_trigger(line, state)` | Any (0–7) | Send raw TTL ON/OFF |
| `bridge.trigger_pulse(line, duration_ms)` | Any | Brief ON/OFF pulse |
| `bridge.stimulus_on(line)` | 0 (default) | Mark stimulus onset |
| `bridge.stimulus_off(line)` | 0 (default) | Mark stimulus offset |
| `bridge.response(line)` | 2 (default) | Mark participant response |
| `bridge.block_start(line)` | 4 (default) | Mark block start |
| `bridge.block_end(line)` | 4 (default) | Mark block end |
| `bridge.experiment_start(line)` | 5 (default) | Mark experiment start |
| `bridge.experiment_end(line)` | 5 (default) | Mark experiment end |

#### Paradigm Helpers

```python
# Automatic trigger bracketing around a trial function
bridge.run_trial(
    trial_func=my_stimulus_function,
    trial_number=1,
    trigger_line=0,
)
```

### TcpTriggerClient (Low-Level TCP)

```python
from paradigm_bridge import TcpTriggerClient

tcp = TcpTriggerClient(address="127.0.0.1", port=5557)
tcp.send_trigger(line=0, state=1)
tcp.pulse(line=1, duration_ms=5.0)
tcp.ping()
tcp.get_status()
tcp.close()
```

### RecordingController (Low-Level HTTP)

```python
from paradigm_bridge import RecordingController

rc = RecordingController(address="127.0.0.1", port=37497)
rc.start_recording("session_name")
rc.stop_recording()
```

### TriggerManager (Legacy ZMQ — requires pyzmq)

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

### Standard (Paradigm Bridge plugin)
```
[Source] → [Paradigm Bridge] → [Bandpass Filter 1-50Hz] → [Record Node] → [LFP Viewer]
```

### InEar Teensy
```
[InEar Teensy Opt] → [Paradigm Bridge] → [Bandpass Filter 1-50Hz] → [Record Node] → [LFP Viewer]
```

### OpenBCI Cyton
```
[OpenBCI Cyton] → [Paradigm Bridge] → [Bandpass Filter 1-50Hz] → [Record Node] → [LFP Viewer]
```

> **Note on InEar Teensy:** The Teensy board has hardware TTL lines (`TTL_IN` / `TTL_OUT` in the Compact Protocol). Once the firmware supports hardware triggers, those will appear as native TTL events — independent of the software triggers sent by this plugin. You can use both simultaneously.

## Trigger Line Convention

Recommended TTL line assignments for EEG paradigms (0-based, lines 0–7):

| Line | Purpose | Example |
|------|---------|---------|
| 0 | Standard stimulus onset/offset | Tone, visual stimulus |
| 1 | Deviant / target stimulus | Oddball target |
| 2 | Participant response | Button press |
| 3 | Feedback | Correct/incorrect indicator |
| 4 | Block start / end | Block boundaries |
| 5 | Experiment start / end | Session boundaries |
| 6–7 | User-defined | Condition codes, etc. |

## TCP Protocol Reference

The Paradigm Bridge C++ plugin accepts newline-terminated text commands on TCP port 5557:

| Command | Description | Example |
|---------|-------------|---------|
| `TRIGGER <line> <state>` | Set TTL line ON (1) or OFF (0) | `TRIGGER 0 1\n` |
| `RECORD START` | Start recording | `RECORD START\n` |
| `RECORD STOP` | Stop recording | `RECORD STOP\n` |
| `RECORD DIR <path>` | Set recording directory | `RECORD DIR C:/data\n` |
| `RECORD NAME <name>` | Set recording prepend text | `RECORD NAME exp1\n` |
| `MESSAGE <text>` | Send text annotation | `MESSAGE trial_start\n` |
| `PING` | Connection check | `PING\n` → `PONG` |
| `STATUS` | Get plugin status | `STATUS\n` → JSON |

## Dependencies

| Package | Version | Required | Purpose |
|---------|---------|----------|---------|
| `requests` | ≥2.25.0 | ✅ | HTTP communication with Open Ephys |
| `pyzmq` | ≥22.0.0 | ❌ optional | ZMQ backend for Network Events plugin |
| `psychopy` | — | ❌ optional | Stimulus presentation |

Install ZMQ backend extras: `pip install paradigm-bridge[zmq]`

## License

MIT License — Free to use and modify.
