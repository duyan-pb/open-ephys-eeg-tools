"""
Paradigm Bridge for Open Ephys
==============================

A Python package that bridges experiment paradigm software (PsychoPy, Expyriment,
custom scripts) with the Open Ephys GUI. Provides unified recording control and
trigger annotation via the built-in HTTP Server and the **Paradigm Bridge** C++
plugin (TCP, port 5557).

Features
--------
- Start / stop recording from any Python script
- Send TTL trigger events via TCP to the paradigm-bridge-cpp plugin
- Automatic connection management with retry logic
- Device-agnostic: works with InEar Teensy, OpenBCI Cyton, or any source
- Optional standalone GUI for manual control (no PsychoPy required)
- Legacy ZMQ backend available for the Network Events plugin

Quick Start
-----------
>>> from paradigm_bridge import ParadigmBridge
>>> bridge = ParadigmBridge()                   # TCP by default
>>> bridge.start_recording("my_experiment")
>>> bridge.send_trigger(line=0, state=1)        # stimulus ON
>>> bridge.send_trigger(line=0, state=0)        # stimulus OFF
>>> bridge.stop_recording()
>>> bridge.close()

Open Ephys Signal Chain
-----------------------
[Source] → [Paradigm Bridge] → [Record Node] → [LFP Viewer]

Requirements
------------
- Open Ephys GUI running with HTTP Server enabled (port 37497)
- Paradigm Bridge C++ plugin loaded in the signal chain (TCP server on 5557)
"""

__version__ = "2.0.0"

from .bridge import ParadigmBridge
from .tcp_trigger import TcpTriggerClient
from .trigger import TriggerManager
from .recorder import RecordingController

__all__ = [
    "ParadigmBridge",
    "TcpTriggerClient",
    "TriggerManager",
    "RecordingController",
]
