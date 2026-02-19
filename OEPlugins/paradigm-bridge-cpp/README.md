# Paradigm Bridge (C++ Plugin)

A **native C++ Open Ephys processor plugin** that bridges external paradigm software (PsychoPy, MATLAB, custom Python/C++ scripts) to the Open Ephys GUI signal chain. Compiles to a `.dll` (Windows), `.so` (Linux), or `.bundle` (macOS) that loads directly into Open Ephys.

## Features

| Feature | Description |
|---------|-------------|
| **TTL Event Injection** | Receive trigger commands via TCP and inject TTL events (8 lines) into the signal chain |
| **Recording Control** | Start/stop recording, set directory and name from external scripts |
| **Status Messages** | Forward messages to the Open Ephys GUI console |
| **Auto-Start Server** | Optionally start TCP server automatically on acquisition |
| **Manual Triggers** | Editor UI with manual ON/OFF trigger buttons per line |
| **Zero Dependencies** | Uses only JUCE sockets and Open Ephys API — no external libraries |

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  External Software (PsychoPy, MATLAB, Python scripts)        │
│                                                              │
│  import socket                                               │
│  s = socket.create_connection(("localhost", 5557))           │
│  s.sendall(b"TRIGGER 0 1\n")   # TTL line 0 → HIGH         │
│  s.sendall(b"RECORD START\n")  # Start recording            │
└──────────────────┬───────────────────────────────────────────┘
                   │ TCP (port 5557)
                   ▼
┌──────────────────────────────────────────────────────────────┐
│  Paradigm Bridge (C++ Open Ephys Plugin)                     │
│                                                              │
│  ┌─────────────────────┐     ┌──────────────────────────┐   │
│  │  TcpCommandServer   │────▶│  ParadigmBridge          │   │
│  │  (background thread)│     │  (GenericProcessor)       │   │
│  │                     │     │                           │   │
│  │  • Accepts TCP conn │     │  • Injects TTL events     │   │
│  │  • Parses commands  │     │  • Controls recording     │   │
│  │  • Sends responses  │     │  • Passes data through    │   │
│  └─────────────────────┘     └──────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  ParadigmBridgeEditor (JUCE GUI panel)               │   │
│  │  • Port config  • Auto-start toggle                  │   │
│  │  • Server status • Client connection indicator       │   │
│  │  • Trigger count • Last command display              │   │
│  │  • Manual trigger buttons (ON/OFF per line)          │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
                   │
                   ▼ Signal Chain
┌──────────────────────────────────────────────────────────────┐
│  Source → [Paradigm Bridge] → Bandpass Filter → Record Node  │
│                                                              │
│  Continuous data passes through unchanged.                   │
│  TTL events are injected and visible to downstream nodes.    │
└──────────────────────────────────────────────────────────────┘
```

## Plugin Type

- **Type**: Processor (FILTER)
- **Behavior**: Passes all continuous data through unchanged, injects TTL events
- **Placement**: Between any source and the Record Node

## TCP Protocol

### Connection

```
Host: localhost by default (remote clients are blocked unless explicitly enabled)
Port: 5557 (default, configurable)
Format: Newline-terminated UTF-8 text commands
```

Security defaults:

- Remote clients are **disabled by default** (loopback-only).
- Optional command authentication via `AUTH <token>`.

Runtime environment variables:

- `PARADIGM_BRIDGE_ALLOW_REMOTE=1` to allow non-localhost clients.
- `PARADIGM_BRIDGE_TOKEN=<secret>` to require authentication.

### Commands

| Command | Description | Example |
|---------|-------------|---------|
| `AUTH <token>` | Authenticate current TCP session (required if token is configured) | `AUTH my-secret` |
| `TRIGGER <line> <state>` | Send TTL event (line 0-7, state 0 or 1) | `TRIGGER 0 1` |
| `RECORD START` | Start recording (acquisition must be active) | `RECORD START` |
| `RECORD STOP` | Stop recording | `RECORD STOP` |
| `RECORD DIR <path>` | Set recording parent directory | `RECORD DIR C:\Data\exp1` |
| `RECORD NAME <name>` | Set recording directory base name | `RECORD NAME subject_01` |
| `RECORD NEWDIR` | Create new recording directory | `RECORD NEWDIR` |
| `MESSAGE <text>` | Send message to Open Ephys GUI console | `MESSAGE Trial 5 started` |
| `PING` | Test connection (responds with `OK PONG`) | `PING` |
| `STATUS` | Get current acquisition/recording status | `STATUS` |

### Responses

All commands receive a newline-terminated response:

```
OK [data]           — Command accepted
ERROR <message>     — Command failed
```

For recording and directory commands, `OK ... ACCEPTED` means the command was accepted
and dispatched to the GUI thread.

**Examples:**
```
→ PING
← OK PONG

→ TRIGGER 0 1
← OK TRIGGER 0 1

→ AUTH my-secret
← OK AUTH

→ STATUS
← OK ACQUISITION=ON RECORDING=OFF TRIGGERS=42 DROPPED=0 REMOTE=OFF

→ TRIGGER 8 1
← ERROR line must be 0-7
```

## Build Instructions

### Prerequisites

- **Visual Studio 2022** (Windows) or GCC/Clang (Linux/macOS)
- **CMake 3.15+**
- **Open Ephys GUI** source code (built, with `open-ephys.lib` available)

The `GUI_BASE_DIR` should point to the `plugin-GUI` directory (auto-detected as `../../plugin-GUI`).

### Windows (Visual Studio 2022)

```powershell
cd OEPlugins\paradigm-bridge-cpp
mkdir Build
cd Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

The DLL will be output to: `plugin-GUI\Build\Release\plugins\paradigm-bridge-cpp.dll`

### Linux

```bash
cd OEPlugins/paradigm-bridge-cpp
mkdir -p Build && cd Build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### macOS

```bash
cd OEPlugins/paradigm-bridge-cpp
mkdir -p Build && cd Build
cmake -G Xcode ..
xcodebuild -configuration Release
```

### Install

After building, the plugin DLL/SO/bundle is automatically installed into the Open Ephys plugins directory. Restart Open Ephys GUI to load the new plugin.

## Usage in Open Ephys

1. **Add to signal chain**: Drag "Paradigm Bridge" from the processor list into your signal chain, between the source and record node.

2. **Configure port**: Set the TCP port in the editor panel (default: 5557).

3. **Start server**: Click "Start Server" or enable "Auto" to auto-start when acquisition begins.

4. **Connect client**: From your paradigm script, connect via TCP to `localhost:5557`.

5. **Send commands**: Use newline-terminated text commands.

## Integration Tests

An optional pytest integration suite is provided in `tests/test_tcp_protocol.py`.
It validates command/response behavior against a running plugin instance.

```bash
pip install pytest
export PARADIGM_BRIDGE_TEST_HOST=127.0.0.1
export PARADIGM_BRIDGE_TEST_PORT=5557
# Optional if authentication is enabled:
export PARADIGM_BRIDGE_TEST_TOKEN=your-token
pytest tests/test_tcp_protocol.py -q
```

## Python Client Examples

### Minimal Example

```python
import socket

def send_command(sock, command):
    sock.sendall((command + "\n").encode())
    return sock.recv(1024).decode().strip()

# Connect to Paradigm Bridge
s = socket.create_connection(("localhost", 5557))

# Test connection
print(send_command(s, "PING"))  # OK PONG

# Set up recording
send_command(s, "RECORD DIR C:\\Data\\experiment")
send_command(s, "RECORD NAME subject_01")
send_command(s, "RECORD START")

# Send triggers during experiment
send_command(s, "TRIGGER 0 1")  # Stimulus onset
send_command(s, "TRIGGER 0 0")  # Reset
send_command(s, "TRIGGER 1 1")  # Response marker

# Stop recording
send_command(s, "RECORD STOP")
s.close()
```

### PsychoPy Integration

```python
import socket
from psychopy import visual, event, core

class ParadigmBridge:
    def __init__(self, host="localhost", port=5557):
        self.sock = socket.create_connection((host, port))
    
    def send(self, cmd):
        self.sock.sendall((cmd + "\n").encode())
        return self.sock.recv(1024).decode().strip()
    
    def trigger(self, line, state):
        return self.send(f"TRIGGER {line} {state}")
    
    def start_recording(self):
        return self.send("RECORD START")
    
    def stop_recording(self):
        return self.send("RECORD STOP")
    
    def close(self):
        self.sock.close()

# Setup
bridge = ParadigmBridge()
win = visual.Window([800, 600])
stim = visual.TextStim(win, text="+")

bridge.start_recording()

for trial in range(10):
    stim.draw()
    win.flip()
    bridge.trigger(0, 1)  # Mark stimulus onset
    core.wait(1.0)
    bridge.trigger(0, 0)  # Reset trigger

bridge.stop_recording()
bridge.close()
win.close()
```

## Source Files

| File | Description |
|------|-------------|
| `Source/OpenEphysLib.cpp` | Plugin registration (FILTER processor type) |
| `Source/ParadigmBridge.h/cpp` | Main processor — TTL injection, recording control, command routing |
| `Source/ParadigmBridgeEditor.h/cpp` | JUCE editor panel — server controls, status display, manual triggers |
| `Source/TcpCommandServer.h/cpp` | Background TCP server thread — accepts connections, parses commands |
| `CMakeLists.txt` | Build configuration (standard Open Ephys plugin pattern) |

## Signal Chain Examples

### Basic EEG with Paradigm Triggers
```
InEar Teensy Source → Paradigm Bridge → Bandpass Filter → Record Node → LFP Viewer
```

### OpenBCI with PsychoPy
```
OpenBCI Cyton → Paradigm Bridge → Record Node
```

### LSL Source with Triggers + Streaming
```
LSL Source → Paradigm Bridge → LSL Outlet
                             → Record Node
```

## Comparison with Network Events Plugin

| Feature | Network Events | Paradigm Bridge |
|---------|---------------|-----------------|
| Protocol | ZMQ (REQ/REP) | TCP (plain sockets) |
| Port | 5556 | 5557 (configurable) |
| TTL Events | ✅ | ✅ |
| Recording Control | ❌ | ✅ |
| Status Messages | ❌ | ✅ |
| Dependencies | libzmq | None (JUCE sockets) |
| Manual Triggers | ❌ | ✅ |
| Auto-start | ❌ | ✅ |

The Paradigm Bridge is a superset of Network Events, purpose-built for paradigm software integration.

## License

GPL v3 (same as Open Ephys GUI)
