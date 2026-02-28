"""
PsychoPy + Open Ephys via Paradigm Bridge plugin
=================================================

Requirements:
  - Open Ephys GUI with paradigm-bridge-cpp.dll in the plugins folder
  - Signal chain: [Source] → [Paradigm Bridge] → [Record Node] → [LFP Viewer]
  - PsychoPy installed
  - No extra pip packages for Open Ephys control (socket only)

How to use:
  1. Copy paradigm-bridge-cpp.dll into your Open Ephys plugins folder
  2. Open Ephys GUI → build signal chain above
  3. Run this script from PsychoPy
"""

import socket


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
    def point(self, line=0):
        """Single-command point marker (new DLL), fallback to ON/OFF."""
        resp = self._send(f"PULSE {line}")
        if resp.startswith("OK"):
            return resp
        if "unknown command" in resp:
            self.trigger_on(line)
            self.trigger_off(line)
            return "OK TRIGGER fallback"
        return resp

    def trigger_pulse(self, line=0, ms=5):
        # Prefer server-side point marker when a very short pulse is requested.
        if ms <= 0:
            return self.point(line)
        self.trigger_on(line)
        core.wait(ms / 1000.0, hogCPUperiod=0.0)
        self.trigger_off(line)

    # --- Annotation helpers (ANT Neuro style) ---
    def marker(self, line=0):
        """Single time-point annotation (point marker)."""
        return self.point(line)

    def interval_start(self, line=1):
        """Start interval annotation (line goes HIGH)."""
        return self.trigger_on(line)

    def interval_stop(self, line=1):
        """End interval annotation (line goes LOW)."""
        return self.trigger_off(line)

    def close(self):
        self.sock.close()


# ===========================================================================
# Example PsychoPy paradigm
# ===========================================================================

from psychopy import visual, event, core

# Connect to plugin
bridge = ParadigmBridge()

# PsychoPy window
# checkTiming=False avoids the "Attempting to measure frame rate" freeze
win = visual.Window(size=(800, 600), units='pix', checkTiming=False)
text = visual.TextStim(win, text='Press SPACE to start recording')
text.draw()
win.flip()
event.waitKeys(keyList=['space', 'escape'])

# Start recording
bridge.record_name("my_experiment")
bridge.record_start()

# Simple trial loop with both annotation types
for trial in range(5):
    text.setText(f'Trial {trial + 1} / 5')
    text.draw()
    win.flip()

    # Point marker: marks stimulus onset (line 0)
    bridge.marker(0)

    # Interval: marks the stimulus-on period (line 1)
    bridge.interval_start(1)      # stimulus ON
    core.wait(0.5, hogCPUperiod=0.0)
    bridge.interval_stop(1)       # stimulus OFF

    win.flip()
    core.wait(1.0, hogCPUperiod=0.0)

# Stop recording
bridge.record_stop()
bridge.close()

win.close()
core.quit()
