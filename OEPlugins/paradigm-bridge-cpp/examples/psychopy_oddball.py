"""
PsychoPy + Paradigm Bridge Integration Example

Demonstrates a simple ERP oddball paradigm with triggers sent
via the Paradigm Bridge C++ plugin's TCP interface.

TTL Line Mapping:
    Line 0: Stimulus onset (1=onset, 0=offset)
    Line 1: Stimulus type  (0=standard, 1=deviant/target)
    Line 2: Response        (1=button press)
    Line 3: Block markers   (1=block start, 0=block end)

Requirements:
    pip install psychopy
"""

import socket
import time
import random


class ParadigmBridge:
    """Minimal TCP client for the Paradigm Bridge C++ plugin."""

    def __init__(self, host="localhost", port=5557):
        self.sock = socket.create_connection((host, port), timeout=5.0)
        # Verify connection
        self.sock.sendall(b"PING\n")
        resp = self.sock.recv(256).decode().strip()
        if not resp.startswith("OK"):
            raise ConnectionError(f"Unexpected response: {resp}")

    def send(self, cmd):
        self.sock.sendall((cmd + "\n").encode())
        return self.sock.recv(1024).decode().strip()

    def trigger(self, line, state):
        self.send(f"TRIGGER {line} {state}")

    def start_recording(self):
        self.send("RECORD START")

    def stop_recording(self):
        self.send("RECORD STOP")

    def set_recording_dir(self, path):
        self.send(f"RECORD DIR {path}")

    def set_recording_name(self, name):
        self.send(f"RECORD NAME {name}")

    def message(self, text):
        self.send(f"MESSAGE {text}")

    def close(self):
        self.sock.close()


def run_oddball_paradigm():
    """Simple auditory oddball paradigm with Open Ephys triggers."""

    # --- Optional: import PsychoPy if available ---
    try:
        from psychopy import visual, event, core, sound
        USE_PSYCHOPY = True
    except ImportError:
        print("PsychoPy not installed. Running in simulation mode.")
        USE_PSYCHOPY = False

    # --- Connect to Open Ephys ---
    print("Connecting to Paradigm Bridge...")
    bridge = ParadigmBridge(host="localhost", port=5557)
    print("Connected!")

    # --- Experiment parameters ---
    N_BLOCKS = 2
    TRIALS_PER_BLOCK = 20
    DEVIANT_PROB = 0.2
    STIM_DURATION = 0.1       # seconds
    ISI_MIN = 0.8             # inter-stimulus interval
    ISI_MAX = 1.2
    SUBJECT_ID = "sub_001"

    # --- Setup recording ---
    bridge.set_recording_name(SUBJECT_ID + "_oddball")
    bridge.message(f"Oddball paradigm starting: {N_BLOCKS} blocks, {TRIALS_PER_BLOCK} trials/block")

    # --- Setup PsychoPy (if available) ---
    if USE_PSYCHOPY:
        win = visual.Window([800, 600], color="black")
        fixation = visual.TextStim(win, text="+", color="white", height=0.2)
        standard_sound = sound.Sound(value=1000, secs=STIM_DURATION)  # 1000 Hz standard
        deviant_sound = sound.Sound(value=1500, secs=STIM_DURATION)   # 1500 Hz deviant

    # --- Start recording ---
    bridge.start_recording()
    time.sleep(0.5)  # Wait for recording to start

    try:
        for block in range(N_BLOCKS):
            bridge.message(f"Block {block + 1}/{N_BLOCKS} starting")
            bridge.trigger(3, 1)  # Block start marker

            if USE_PSYCHOPY:
                fixation.draw()
                win.flip()

            time.sleep(1.0)  # Pre-block pause

            for trial in range(TRIALS_PER_BLOCK):
                # Determine stimulus type
                is_deviant = random.random() < DEVIANT_PROB

                # Set stimulus type trigger BEFORE onset
                bridge.trigger(1, 1 if is_deviant else 0)

                # Stimulus onset
                bridge.trigger(0, 1)

                if USE_PSYCHOPY:
                    if is_deviant:
                        deviant_sound.play()
                    else:
                        standard_sound.play()

                stim_type = "DEVIANT" if is_deviant else "standard"
                print(f"  Block {block+1}, Trial {trial+1}: {stim_type}")

                time.sleep(STIM_DURATION)

                # Stimulus offset
                bridge.trigger(0, 0)

                # Check for response (PsychoPy) or simulate
                if USE_PSYCHOPY:
                    keys = event.getKeys(keyList=["space"], timeStamped=True)
                    if keys:
                        bridge.trigger(2, 1)  # Response marker
                        time.sleep(0.01)
                        bridge.trigger(2, 0)

                # Inter-stimulus interval
                isi = random.uniform(ISI_MIN, ISI_MAX)
                time.sleep(isi)

            bridge.trigger(3, 0)  # Block end marker
            bridge.message(f"Block {block + 1}/{N_BLOCKS} completed")

            # Inter-block pause
            if block < N_BLOCKS - 1:
                print(f"  Rest between blocks...")
                time.sleep(3.0)

    finally:
        # Always stop recording, even on error
        bridge.stop_recording()
        bridge.message("Oddball paradigm completed")
        bridge.close()

        if USE_PSYCHOPY:
            win.close()

    print("\nExperiment complete!")
    print(f"Status: {bridge.send('STATUS') if bridge.sock else 'disconnected'}")


if __name__ == "__main__":
    run_oddball_paradigm()
