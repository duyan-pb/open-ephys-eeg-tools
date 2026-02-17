"""
Example: Adapt for OpenBCI Cyton amplifier
==========================================

The ParadigmBridge is device-agnostic — it controls the Open Ephys GUI,
not the hardware directly. This example shows the recommended signal chain
and usage when using an OpenBCI Cyton board.

The key insight: the paradigm script is IDENTICAL regardless of which EEG
amplifier is used. Only the Open Ephys signal chain changes.

Open Ephys Signal Chain for OpenBCI Cyton:
    [OpenBCI Cyton] → [Network Events] → [Bandpass Filter 1-50Hz] → [Record Node] → [LFP Viewer]

Open Ephys Signal Chain for InEar Teensy:
    [InEar Teensy Opt] → [Network Events] → [Bandpass Filter 1-50Hz] → [Record Node] → [LFP Viewer]

Requirements:
    pip install paradigm-bridge psychopy
"""

from psychopy import visual, event, core
from paradigm_bridge import ParadigmBridge
import time

# ===========================================================================
# The bridge does NOT care which amplifier you use
# It communicates with Open Ephys GUI, not with the hardware
# ===========================================================================

bridge = ParadigmBridge(verbose=True)

# Verify signal chain
report = bridge.check_signal_chain()
print("=" * 50)
print("Signal Chain Check")
print("=" * 50)
print(f"  Connected:        {report['connected']}")
print(f"  Mode:             {report['mode']}")
print(f"  Network Events:   {report['has_network_events']}")
print(f"  Triggers ready:   {report['triggers_available']}")
print(f"  Processors:       {report['processors']}")
print("=" * 50)

if not report['connected']:
    print("\nERROR: Cannot connect to Open Ephys GUI.")
    print("Make sure:")
    print("  1. Open Ephys is running")
    print("  2. Your signal chain includes a source (Cyton or Teensy)")
    print("  3. HTTP Server is enabled")
    exit(1)

if not report['has_network_events']:
    print("\nWARNING: Network Events plugin not found in signal chain.")
    print("Triggers will not be visible in the GUI.")
    print("Add Network Events to your signal chain for trigger support.")

# ===========================================================================
# Rest of the paradigm is identical for any amplifier
# ===========================================================================

win = visual.Window(size=(800, 600), monitor='testMonitor', units='pix')
text = visual.TextStim(win, text='', height=24)

# Instruction
text.setText(
    "This paradigm works with:\n"
    "  - OpenBCI Cyton (250 Hz)\n"
    "  - InEar Teensy (1000 Hz)\n"
    "  - Any Open Ephys source\n\n"
    "Press SPACE to start recording"
)
text.draw()
win.flip()
event.waitKeys(keyList=['space'])

# Record
bridge.start_recording("amplifier_agnostic_demo")
bridge.experiment_start()

for trial in range(3):
    text.setText(f"Trial {trial + 1}")
    text.draw()
    win.flip()

    bridge.stimulus_on(line=1)
    time.sleep(0.5)
    bridge.stimulus_off(line=1)

    win.flip()
    time.sleep(1.0)

bridge.experiment_end()
bridge.stop_recording()

text.setText("Done!")
text.draw()
win.flip()
time.sleep(2)

win.close()
bridge.close()
core.quit()
