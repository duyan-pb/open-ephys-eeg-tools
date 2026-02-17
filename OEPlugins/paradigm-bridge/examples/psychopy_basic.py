"""
Example: Basic PsychoPy paradigm with Open Ephys recording + triggers
=====================================================================

This is the enhanced version of Alexey's original script, now using the
ParadigmBridge plugin for cleaner recording control and trigger annotations.

Requirements:
    pip install psychopy
    pip install paradigm-bridge   (or run from OEPlugins/paradigm-bridge)

Open Ephys Setup:
    Signal chain must include:
    [Source] → [Network Events] → [Bandpass Filter] → [Record Node] → [LFP Viewer]
"""

from psychopy import visual, event, core
from paradigm_bridge import ParadigmBridge
import time

# ===========================================================================
# 1. Connect to Open Ephys
# ===========================================================================

bridge = ParadigmBridge(
    enable_triggers=True,  # requires Network Events plugin in signal chain
    verbose=True,
)

# Optional: verify connection before starting
report = bridge.check_signal_chain()
print(f"Connected: {report['connected']}")
print(f"Mode: {report['mode']}")
print(f"Network Events available: {report['has_network_events']}")
print(f"Processors: {report['processors']}")

# ===========================================================================
# 2. PsychoPy setup
# ===========================================================================

win = visual.Window(size=(800, 600), monitor='testMonitor', units='pix')
text_stim = visual.TextStim(win, text='Press SPACE to begin recording')
text_stim.draw()
win.flip()

# Wait for spacebar
keys = event.waitKeys(keyList=['space', 'escape'])

if 'escape' in keys:
    win.close()
    bridge.close()
    core.quit()

# ===========================================================================
# 3. Start recording
# ===========================================================================

bridge.start_recording("psychopy_basic_demo")
bridge.experiment_start()  # TTL marker on line 6

text_stim.setText('Recording... presenting stimuli')
text_stim.draw()
win.flip()

# ===========================================================================
# 4. Run a simple trial loop
# ===========================================================================

NUM_TRIALS = 5
STIM_DURATION = 0.5   # seconds
ITI = 1.0             # inter-trial interval

for trial in range(NUM_TRIALS):
    # --- Stimulus onset ---
    text_stim.setText(f'Trial {trial + 1} / {NUM_TRIALS}')
    text_stim.draw()
    win.flip()

    bridge.stimulus_on(line=1)           # TTL ON line 1 = stimulus onset
    time.sleep(STIM_DURATION)
    bridge.stimulus_off(line=1)          # TTL OFF line 1 = stimulus offset

    # --- ITI (blank screen) ---
    win.flip()
    time.sleep(ITI)

# ===========================================================================
# 5. Stop recording
# ===========================================================================

bridge.experiment_end()  # TTL marker on line 6
bridge.stop_recording()

text_stim.setText('Done! Recording saved.')
text_stim.draw()
win.flip()
time.sleep(2)

# ===========================================================================
# 6. Cleanup
# ===========================================================================

win.close()
bridge.close()
core.quit()
