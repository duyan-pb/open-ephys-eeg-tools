"""
PsychoPy + Open Ephys via Paradigm Bridge plugin
=================================================

Requirements:
  - Open Ephys GUI with paradigm-bridge-cpp.dll in the plugins folder
  - Signal chain: [Source] → [Paradigm Bridge] → [Record Node] → [LFP Viewer]
  - No pip install needed. This script uses only Python stdlib.

How to use:
  1. Copy paradigm-bridge-cpp.dll into your Open Ephys plugins folder
  2. Open Ephys GUI → build signal chain above
  3. Run this script from PsychoPy
"""

import socket
import time


# ===========================================================================
# Paradigm Bridge helper — copy this into any PsychoPy script
# ===========================================================================

class ParadigmBridge:
    """Talks to the Paradigm Bridge plugin over TCP. No dependencies."""

    def __init__(self, host="127.0.0.1", port=5557):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(3.0)
        self.sock.connect((host, port))

    def _send(self, cmd):
        self.sock.sendall((cmd + "\n").encode())
        return self.sock.recv(4096).decode().strip()

    # --- Recording ---
    def record_start(self):          return self._send("RECORD START")
    def record_stop(self):           return self._send("RECORD STOP")
    def record_name(self, name):     return self._send(f"RECORD NAME {name}")
    def record_dir(self, path):      return self._send(f"RECORD DIR {path}")

    # --- Triggers (lines 0-7) ---
    def trigger_on(self, line=0):    return self._send(f"TRIGGER {line} 1")
    def trigger_off(self, line=0):   return self._send(f"TRIGGER {line} 0")

    def trigger_pulse(self, line=0, ms=5):
        self.trigger_on(line)
        time.sleep(ms / 1000)
        self.trigger_off(line)

    def close(self):
        self.sock.close()


# ===========================================================================
# Example PsychoPy paradigm
# ===========================================================================

from psychopy import visual, event, core

# Connect to plugin
bridge = ParadigmBridge()

# PsychoPy window
win = visual.Window(size=(800, 600), units='pix')
text = visual.TextStim(win, text='Press SPACE to start recording')
text.draw()
win.flip()
event.waitKeys(keyList=['space', 'escape'])

# Start recording
bridge.record_name("my_experiment")
bridge.record_start()

# Simple trial loop
for trial in range(5):
    text.setText(f'Trial {trial + 1} / 5')
    text.draw()
    win.flip()

    bridge.trigger_on(0)          # stimulus ON  (line 0)
    time.sleep(0.5)
    bridge.trigger_off(0)         # stimulus OFF (line 0)

    win.flip()
    time.sleep(1.0)

# Stop recording
bridge.record_stop()
bridge.close()

win.close()
core.quit()
