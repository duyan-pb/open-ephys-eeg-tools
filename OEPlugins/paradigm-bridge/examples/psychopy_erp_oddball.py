"""
Example: ERP Oddball paradigm with Open Ephys
==============================================

A classic auditory oddball paradigm using PsychoPy for stimulus presentation
and ParadigmBridge for Open Ephys integration. Demonstrates:
  - Different trigger lines for standard vs deviant stimuli
  - Response triggers on button press
  - Block start/end markers
  - Proper experiment bracketing

Trigger line assignments (convention):
    Line 1 = Standard stimulus
    Line 2 = Deviant stimulus
    Line 3 = Participant response (button press)
    Line 5 = Block start / end
    Line 6 = Experiment start / end

Open Ephys Setup:
    [Source] → [Network Events] → [Bandpass Filter] → [Record Node] → [LFP Viewer]
"""

import random
import time
from psychopy import visual, event, core, sound
from paradigm_bridge import ParadigmBridge

# ===========================================================================
# Configuration
# ===========================================================================

NUM_BLOCKS = 2
TRIALS_PER_BLOCK = 20
DEVIANT_PROBABILITY = 0.2
STIM_DURATION = 0.075       # 75 ms tone
RESPONSE_WINDOW = 0.8       # seconds to respond
ITI_RANGE = (0.8, 1.2)      # random inter-trial interval

# Trigger line assignments
TRIG_STANDARD = 1
TRIG_DEVIANT = 2
TRIG_RESPONSE = 3
TRIG_BLOCK = 5
TRIG_EXPERIMENT = 6

# ===========================================================================
# Setup
# ===========================================================================

bridge = ParadigmBridge(verbose=True)
bridge.wait_for_gui(timeout=15)

win = visual.Window(size=(1024, 768), fullscr=False, monitor='testMonitor', units='pix')
fixation = visual.TextStim(win, text='+', height=40)
instruction = visual.TextStim(win, text='', height=24, wrapWidth=700)

# Create tones (PsychoPy sound)
standard_tone = sound.Sound(value=1000, secs=STIM_DURATION)  # 1000 Hz
deviant_tone = sound.Sound(value=1500, secs=STIM_DURATION)   # 1500 Hz

# ===========================================================================
# Instructions
# ===========================================================================

instruction.setText(
    "Oddball Paradigm\n\n"
    "You will hear a series of tones.\n"
    "Most tones are STANDARD (low pitch).\n"
    "Occasionally a DEVIANT tone (high pitch) will occur.\n\n"
    "Press SPACE as quickly as possible when you hear a DEVIANT tone.\n\n"
    "Press SPACE to begin."
)
instruction.draw()
win.flip()
event.waitKeys(keyList=['space'])

# ===========================================================================
# Start recording
# ===========================================================================

bridge.start_recording("ERP_oddball")
bridge.experiment_start(line=TRIG_EXPERIMENT)
time.sleep(0.5)  # small settle time

# ===========================================================================
# Main paradigm loop
# ===========================================================================

all_results = []

for block in range(NUM_BLOCKS):
    # Block start
    bridge.block_start(line=TRIG_BLOCK)

    instruction.setText(f"Block {block + 1} / {NUM_BLOCKS}\n\nPress SPACE to start")
    instruction.draw()
    win.flip()
    event.waitKeys(keyList=['space'])

    fixation.draw()
    win.flip()
    time.sleep(1.0)

    # Generate trial sequence
    trials = []
    for t in range(TRIALS_PER_BLOCK):
        is_deviant = random.random() < DEVIANT_PROBABILITY
        trials.append({"type": "deviant" if is_deviant else "standard"})

    # Run trials
    for t_idx, trial in enumerate(trials):
        is_deviant = trial["type"] == "deviant"
        trigger_line = TRIG_DEVIANT if is_deviant else TRIG_STANDARD

        # Present stimulus
        fixation.draw()
        win.flip()

        if is_deviant:
            deviant_tone.play()
        else:
            standard_tone.play()

        # Trigger: stimulus onset
        bridge.send_trigger(line=trigger_line, state=1)

        # Response window
        response_clock = core.Clock()
        response_made = False
        rt = None

        while response_clock.getTime() < RESPONSE_WINDOW:
            keys = event.getKeys(keyList=['space'], timeStamped=response_clock)
            if keys and not response_made:
                response_made = True
                rt = keys[0][1]
                bridge.response(line=TRIG_RESPONSE)  # trigger: response

        # Trigger: stimulus offset
        bridge.send_trigger(line=trigger_line, state=0)

        # Log result
        all_results.append({
            "block": block + 1,
            "trial": t_idx + 1,
            "type": trial["type"],
            "response": response_made,
            "rt": rt,
        })

        # Inter-trial interval
        iti = random.uniform(*ITI_RANGE)
        time.sleep(iti)

    # Block end
    bridge.block_end(line=TRIG_BLOCK)

# ===========================================================================
# Finish
# ===========================================================================

bridge.experiment_end(line=TRIG_EXPERIMENT)
bridge.stop_recording()

# Show summary
n_correct = sum(1 for r in all_results if r["type"] == "deviant" and r["response"])
n_deviant = sum(1 for r in all_results if r["type"] == "deviant")
n_false_alarm = sum(1 for r in all_results if r["type"] == "standard" and r["response"])
avg_rt = None
rts = [r["rt"] for r in all_results if r["type"] == "deviant" and r["rt"] is not None]
if rts:
    avg_rt = sum(rts) / len(rts)

summary = (
    f"Experiment Complete!\n\n"
    f"Deviant detection: {n_correct}/{n_deviant}\n"
    f"False alarms: {n_false_alarm}\n"
    f"Mean RT: {avg_rt * 1000:.0f} ms\n\n" if avg_rt else
    f"Experiment Complete!\n\n"
    f"Deviant detection: {n_correct}/{n_deviant}\n"
    f"False alarms: {n_false_alarm}\n"
    f"Mean RT: N/A\n\n"
)
summary += "Press any key to exit."

instruction.setText(summary)
instruction.draw()
win.flip()
event.waitKeys()

# Cleanup
win.close()
bridge.close()
core.quit()
