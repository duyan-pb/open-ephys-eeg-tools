"""
OpenEphys Recording + Trigger Annotations via Paradigm Bridge Plugin
====================================================================

This replaces the HTTP-based openephys_user_input.py script.
Everything goes through the Paradigm Bridge DLL — no pip install needed.

Setup:
  1. Copy paradigm-bridge-cpp.dll into Open Ephys plugins folder
  2. Signal chain: [Source] → [Paradigm Bridge] → [Record Node] → [LFP Viewer]
  3. Press Play (acquire) in Open Ephys
  4. Run this script
"""

import socket
import time
from psychopy import visual, event, core


# ===========================================================================
# Paradigm Bridge — talks to the DLL over TCP (port 5557)
# No pip install, no extra libraries, just Python stdlib.
# ===========================================================================

class ParadigmBridge:
    def __init__(self, host="127.0.0.1", port=5557):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((host, port))

    def _cmd(self, command):
        self.sock.sendall((command + "\n").encode())
        return self.sock.recv(4096).decode().strip()

    # Recording control (replaces the HTTP requests in Alexey's script)
    def start_recording(self):           return self._cmd("RECORD START")
    def stop_recording(self):            return self._cmd("RECORD STOP")
    def set_recording_name(self, name):  return self._cmd(f"RECORD NAME {name}")
    def set_recording_dir(self, path):   return self._cmd(f"RECORD DIR {path}")

    # Trigger annotations (visible as TTL events in LFP Viewer)
    # Lines 0-7 available
    def trigger_on(self, line=0):        return self._cmd(f"TRIGGER {line} 1")
    def trigger_off(self, line=0):       return self._cmd(f"TRIGGER {line} 0")
    def trigger_pulse(self, line=0, ms=5):
        self.trigger_on(line)
        time.sleep(ms / 1000.0)
        self.trigger_off(line)

    # Status message (appears in Open Ephys console)
    def message(self, text):             return self._cmd(f"MESSAGE {text}")

    def close(self):
        try:
            self.sock.close()
        except:
            pass


# ===========================================================================
# PsychoPy user input window (same concept as Alexey's original script)
# ===========================================================================

win = visual.Window(size=(800, 600), monitor='testMonitor', units='pix')

# --- Connect to the plugin ---
try:
    bridge = ParadigmBridge()
    text_stim = visual.TextStim(win, text='Connected to Open Ephys!\n\nPress SPACE to start recording\nPress ESCAPE to quit')
except ConnectionRefusedError:
    text_stim = visual.TextStim(win, text='ERROR: Cannot connect to Open Ephys.\n\nMake sure:\n1. Open Ephys is running\n2. Paradigm Bridge plugin is in the signal chain\n3. Press Play (acquire) first\n\nPress ESCAPE to quit')
    text_stim.draw()
    win.flip()
    event.waitKeys(keyList=['escape'])
    win.close()
    core.quit()

text_stim.draw()
win.flip()

# --- Wait for user input ---
keys = event.waitKeys(keyList=['space', 'escape'])

if 'escape' in keys:
    bridge.close()
    win.close()
    core.quit()

# --- Start recording ---
bridge.set_recording_name("my_experiment")
bridge.start_recording()

text_stim.setText('Recording...\n\nTrigger annotations are being sent.\nPress SPACE to stop.')
text_stim.draw()
win.flip()

# --- Example: send some trigger annotations while recording ---
# These will appear as TTL events in the LFP Viewer
for i in range(3):
    bridge.trigger_pulse(line=0)  # visible as a tick mark in LFP Viewer
    time.sleep(1.0)

# --- Wait for user to stop ---
event.waitKeys(keyList=['space'])

# --- Stop recording ---
bridge.stop_recording()

text_stim.setText('Recording stopped.\n\nPress any key to exit.')
text_stim.draw()
win.flip()
event.waitKeys()

# --- Cleanup ---
bridge.close()
win.close()
core.quit()
