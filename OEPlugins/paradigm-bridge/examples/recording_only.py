"""
Example: Recording control only (no triggers, no PsychoPy)
===========================================================

Minimal script that demonstrates controlling Open Ephys recording
from pure Python — no paradigm software needed. This is the simplest
possible usage, equivalent to Alexey's original attached script but
using the ParadigmBridge plugin.

Requirements:
    pip install paradigm-bridge   (or run from OEPlugins/paradigm-bridge)

Open Ephys Setup:
    Any signal chain with a source and Record Node.
    Network Events plugin is NOT required for this example.
"""

from paradigm_bridge import ParadigmBridge
import time

# Connect without triggers (only recording control)
bridge = ParadigmBridge(enable_triggers=False)

# Check connection
if not bridge.is_connected():
    print("ERROR: Open Ephys GUI is not running or HTTP Server is not enabled.")
    print("       Start Open Ephys and ensure the HTTP Server is active (port 37497).")
    exit(1)

print(f"Connected! Current mode: {bridge.status()}")

# Set recording name and start
bridge.start_recording("simple_recording_test")
print("Recording started...")

# Record for 10 seconds
time.sleep(10)

# Stop
bridge.stop_recording()
print("Recording stopped.")

# Optionally stop acquisition entirely
# bridge.stop_acquisition()

bridge.close()
print("Done.")
