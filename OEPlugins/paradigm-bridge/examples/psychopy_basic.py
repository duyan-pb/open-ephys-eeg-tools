"""
Example: Basic PsychoPy paradigm with Open Ephys recording + triggers
=====================================================================

This is the enhanced version of Alexey's original script, now using the
ParadigmBridge plugin for cleaner recording control and trigger annotations.

Requirements:
    pip install psychopy
    pip install paradigm-bridge   (or run from OEPlugins/paradigm-bridge)

Open Ephys Setup:
    Signal chain must include the **Paradigm Bridge** C++ plugin:
    [Source] → [Paradigm Bridge] → [Bandpass Filter] → [Record Node] → [LFP Viewer]

    The Python script talks to the Paradigm Bridge plugin via TCP (port 5557).
    No extra dependencies (ZMQ / pyzmq) are needed.

Trigger line assignments (0-based, 0–7):
    Line 0 = Stimulus onset / offset
    Line 5 = Experiment start / end
"""

from psychopy import visual, event, core
from paradigm_bridge import ParadigmBridge

# ===========================================================================
# 1. Connect to Open Ephys
# ===========================================================================

bridge = ParadigmBridge(verbose=True)

# Optional: verify connection before starting
report = bridge.check_signal_chain()
print(f"Connected: {report['connected']}")
print(f"Mode: {report['mode']}")
print(f"Trigger backend: {report['trigger_backend']}")
print(f"Triggers available: {report['triggers_available']}")
print(f"Processors: {report['processors']}")

# ===========================================================================
# 2. PsychoPy setup
# ===========================================================================

# `checkTiming=False` avoids PsychoPy's startup frame-rate calibration screen,
# which can appear to "freeze" on busy systems while Open Ephys is rendering.
win = visual.Window(
    size=(800, 600),
    monitor='testMonitor',
    units='pix',
    checkTiming=False,
)
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
bridge.experiment_start()  # TTL marker on line 5

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

    bridge.stimulus_on(line=0)           # TTL ON line 0 = stimulus onset
    core.wait(STIM_DURATION, hogCPUperiod=0.0)
    bridge.stimulus_off(line=0)          # TTL OFF line 0 = stimulus offset

    # --- ITI (blank screen) ---
    win.flip()
    core.wait(ITI, hogCPUperiod=0.0)

# ===========================================================================
# 5. Stop recording
# ===========================================================================

bridge.experiment_end()  # TTL marker on line 5
bridge.stop_recording()

text_stim.setText('Done! Recording saved.')
text_stim.draw()
win.flip()
core.wait(2.0, hogCPUperiod=0.0)

# ===========================================================================
# 6. Cleanup
# ===========================================================================

win.close()
bridge.close()
core.quit()
