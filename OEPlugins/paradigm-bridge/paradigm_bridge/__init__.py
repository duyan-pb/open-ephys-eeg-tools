"""
Paradigm Bridge for Open Ephys
==============================

A Python plugin that bridges experiment paradigm software (PsychoPy, Expyriment,
custom scripts) with the Open Ephys GUI. Provides unified recording control and
trigger annotation via the built-in HTTP Server and the Network Events plugin.

Features
--------
- Start / stop recording from any Python script
- Send TTL trigger annotations visible in Open Ephys GUI
- Automatic connection management with retry logic
- Device-agnostic: works with InEar Teensy, OpenBCI Cyton, or any source
- Optional standalone GUI for manual control (no PsychoPy required)

Quick Start
-----------
>>> from paradigm_bridge import ParadigmBridge
>>> bridge = ParadigmBridge()
>>> bridge.start_recording("my_experiment")
>>> bridge.send_trigger(line=1, state=1)   # stimulus ON
>>> bridge.send_trigger(line=1, state=0)   # stimulus OFF
>>> bridge.stop_recording()

Requirements
------------
- Open Ephys GUI running with HTTP Server enabled (port 37497)
- Network Events plugin in the signal chain (for trigger annotations)
"""

__version__ = "1.0.0"

from .bridge import ParadigmBridge
from .trigger import TriggerManager
from .recorder import RecordingController

__all__ = [
    "ParadigmBridge",
    "TriggerManager",
    "RecordingController",
]
