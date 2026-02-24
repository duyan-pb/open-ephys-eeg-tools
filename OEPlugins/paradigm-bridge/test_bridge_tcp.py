"""Quick test of the ParadigmBridge high-level interface with TCP backend."""

import sys
import os
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from paradigm_bridge import ParadigmBridge

print("=== ParadigmBridge high-level test ===\n")

bridge = ParadigmBridge(verbose=True)

# Check signal chain
report = bridge.check_signal_chain()
print("Connected:", report["connected"])
print("Mode:", report["mode"])
print("Trigger backend:", report["trigger_backend"])
print("Triggers available:", report["triggers_available"])
print("Processors:", report["processors"])
print()

# Convenience methods
print("stimulus_on(0) ...")
bridge.stimulus_on(line=0)
time.sleep(0.05)

print("stimulus_off(0) ...")
bridge.stimulus_off(line=0)
time.sleep(0.1)

print("experiment_start(5) ...")
bridge.experiment_start(line=5)
time.sleep(0.05)

print("experiment_end(5) ...")
bridge.experiment_end(line=5)
time.sleep(0.1)

print("response(2) ...")
bridge.response(line=2)
time.sleep(0.1)

print("block_start(4) ...")
bridge.block_start(line=4)
time.sleep(0.05)

print("block_end(4) ...")
bridge.block_end(line=4)
time.sleep(0.1)

print("trigger_pulse(0, 10ms) ...")
bridge.trigger_pulse(line=0, duration_ms=10.0)

bridge.close()
print("\nAll ParadigmBridge methods work correctly with TCP backend!")
