"""
OpenEphys Recording + Trigger Annotations via Paradigm Bridge Plugin
====================================================================

A drop-in replacement for the HTTP-based ``openephys_user_input.py`` script.
Instead of making REST calls to the Open Ephys HTTP server, all communication
goes through the **Paradigm Bridge** C++ plugin DLL over a single TCP socket
on port 5557.  No ``pip install`` is needed — only the Python standard library
(``socket``, ``time``) is used for the bridge; **PsychoPy** is the only
external dependency (for the visual stimulus window).

Architecture
------------
::

    ┌──────────────┐  TCP :5557   ┌──────────────────┐      ┌─────────────┐
    │  This script │ ──────────►  │  Paradigm Bridge  │ ───► │ Record Node │
    │  (PsychoPy)  │  commands    │  (C++ DLL plugin) │ TTL  │ / LFP View  │
    └──────────────┘  ◄────────── └──────────────────┘      └─────────────┘
                       responses

Signal chain inside Open Ephys::

    [Source] → [Paradigm Bridge] → [Record Node] → [LFP Viewer]

Prerequisites
-------------
1. Copy ``paradigm-bridge-cpp.dll`` into the Open Ephys ``plugins/`` folder.
2. Build the signal chain shown above in the Open Ephys GUI.
3. Press **Play** (acquire) so the TCP server starts listening.
4. Run this script: ``python openephys_paradigm_bridge.py``

TCP Protocol (newline-terminated UTF-8)
---------------------------------------
=========================== ========================== ===========================
Command                     Response                   Description
=========================== ========================== ===========================
``PING``                    ``OK PONG``                Connection test
``TRIGGER <line> <0|1>``    ``OK TRIGGER <line> <s>``  Set TTL line 0-7 high/low
``PULSE <line>``            ``OK PULSE <line>``        Point marker (immediate ON/OFF)
``RECORD START``            ``OK RECORD START ACCEPTED`` Start recording
``RECORD STOP``             ``OK RECORD STOP ACCEPTED``  Stop recording
``RECORD NAME <name>``      ``OK RECORD NAME ACCEPTED``  Set recording dir name
``RECORD DIR <path>``       ``OK RECORD DIR ACCEPTED``   Set recording base path
``MESSAGE <text>``          ``OK MESSAGE``             Print to OE console
``STATUS``                  ``OK STATUS …``            Query server state
=========================== ========================== ===========================

Differences from the original HTTP approach
--------------------------------------------
+-------------------------------+--------------------------------------------+
| Before (HTTP)                 | Now (Paradigm Bridge TCP)                  |
+===============================+============================================+
| ``pip install requests``      | No install — uses ``socket`` (stdlib)      |
+-------------------------------+--------------------------------------------+
| ``requests.put(…/api/status)``| ``bridge.start_recording()``               |
+-------------------------------+--------------------------------------------+
| No trigger annotations        | ``bridge.trigger_pulse(line=0)`` — visible |
|                               | as TTL events in LFP Viewer                |
+-------------------------------+--------------------------------------------+

Usage
-----
Run the script after Open Ephys is acquiring::

    python openephys_paradigm_bridge.py

A PsychoPy window will open.  Press **SPACE** to start recording, which also
sends three example trigger pulses (visible as tick marks in LFP Viewer).
Press **SPACE** again to stop, then any key to exit.

Author
------
Generated for the duyan-pb/open-ephys-eeg-tools workspace.
"""

import socket
from psychopy import visual, event, core


# ===========================================================================
# Paradigm Bridge — talks to the DLL over TCP (port 5557)
# No pip install, no extra libraries, just Python stdlib.
# ===========================================================================

class ParadigmBridge:
    """Client for the Paradigm Bridge C++ plugin running inside Open Ephys.

    Communicates over a persistent TCP connection using a simple
    newline-delimited text protocol.  Each command is sent as a single
    UTF-8 line; the server replies with one line starting with ``OK``
        (or ``ERROR`` on failure).

    Parameters
    ----------
    host : str, optional
        IP address of the machine running Open Ephys (default ``"127.0.0.1"``).
    port : int, optional
        TCP port the Paradigm Bridge plugin listens on (default ``5557``).

    Examples
    --------
    >>> bridge = ParadigmBridge()
    >>> bridge.start_recording()
    'OK RECORD STARTED'
    >>> bridge.trigger_pulse(line=0, ms=10)
    >>> bridge.stop_recording()
    'OK RECORD STOPPED'
    >>> bridge.close()
    """

    def __init__(self, host="127.0.0.1", port=5557):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((host, port))

    def _cmd(self, command):
        """Send a single command string and return the server's reply.

        Parameters
        ----------
        command : str
            Raw command text **without** a trailing newline (it is added
            automatically).

        Returns
        -------
        str
            The server response, stripped of the trailing newline.
        """
        self.sock.sendall((command + "\n").encode())
        return self.sock.recv(4096).decode().strip()

    # ---- Recording control --------------------------------------------------
    # These replace the HTTP ``requests.put(…)`` calls in the original script.

    def start_recording(self):
        """Begin recording.  Equivalent to pressing the Record button in OE.

        Returns
        -------
        str
            ``'OK RECORD STARTED'`` on success.
        """
        return self._cmd("RECORD START")

    def stop_recording(self):
        """Stop an active recording.

        Returns
        -------
        str
            ``'OK RECORD STOPPED'`` on success.
        """
        return self._cmd("RECORD STOP")

    def set_recording_name(self, name):
        """Set the recording directory name (the folder under the base path).

        Parameters
        ----------
        name : str
            Directory name, e.g. ``"my_experiment"``.

        Returns
        -------
        str
            ``'OK RECORD NAME <name>'`` on success.
        """
        return self._cmd(f"RECORD NAME {name}")

    def set_recording_dir(self, path):
        """Set the recording base directory path.

        Parameters
        ----------
        path : str
            Absolute filesystem path, e.g. ``"C:/data/eeg"``.

        Returns
        -------
        str
            ``'OK RECORD DIR <path>'`` on success.
        """
        return self._cmd(f"RECORD DIR {path}")

    # ---- Trigger annotations ------------------------------------------------
    # Triggers are injected as TTL events on one of 8 digital lines (0-7).
    # They appear as vertical tick marks in LFP Viewer and are saved by the
    # Record Node into the events file.

    def trigger_on(self, line=0):
        """Set a TTL line HIGH.

        Parameters
        ----------
        line : int, optional
            Digital line index (0-7, default ``0``).

        Returns
        -------
        str
            ``'OK TRIGGER <line> 1'`` on success.
        """
        return self._cmd(f"TRIGGER {line} 1")

    def trigger_off(self, line=0):
        """Set a TTL line LOW.

        Parameters
        ----------
        line : int, optional
            Digital line index (0-7, default ``0``).

        Returns
        -------
        str
            ``'OK TRIGGER <line> 0'`` on success.
        """
        return self._cmd(f"TRIGGER {line} 0")

    def trigger_pulse(self, line=0, ms=5):
        """Send a brief HIGH→LOW pulse on the specified TTL line.

        This is the most common way to mark a stimulus event.  The pulse
        width defaults to 5 ms which is long enough to be captured at
        typical EEG sample rates (250-1000 Hz).

        Parameters
        ----------
        line : int, optional
            Digital line index (0-7, default ``0``).
        ms : int or float, optional
            Pulse duration in milliseconds (default ``5``).
        """
        if ms <= 0:
            return self.point(line)

        self.trigger_on(line)
        core.wait(ms / 1000.0, hogCPUperiod=0.0)
        self.trigger_off(line)

    def point(self, line=0):
        """Single time-point marker using the DLL's PULSE command.

        Falls back to back-to-back TRIGGER ON/OFF if an older DLL is used.
        """
        response = self._cmd(f"PULSE {line}")
        if response.startswith("OK"):
            return response
        if "unknown command" in response:
            self.trigger_on(line)
            self.trigger_off(line)
            return "OK TRIGGER fallback"
        return response

    # ---- Annotation helpers -------------------------------------------------
    # ANT Neuro style: single time-point markers AND interval annotations.

    def marker(self, line=0):
        """Single time-point annotation (instantaneous event marker).

        Sends a 1 ms pulse — the shortest practical pulse.  Use this to
        mark discrete events like stimulus onset, button press, etc.
        Equivalent to what ANT Neuro calls a 'marker'.

        Parameters
        ----------
        line : int, optional
            Digital line index (0-7, default ``0``).
        """
        return self.point(line)

    def interval_start(self, line=1):
        """Start an interval annotation by setting a TTL line HIGH.

        Call :meth:`interval_stop` with the same line to end the interval.
        Use a different line than your point markers (e.g. line 1 for
        intervals, line 0 for point markers) to distinguish them in
        the recording.  Equivalent to ANT Neuro 'interval annotation start'.

        Parameters
        ----------
        line : int, optional
            Digital line index (0-7, default ``1``).
        """
        return self.trigger_on(line)

    def interval_stop(self, line=1):
        """End an interval annotation by setting the TTL line LOW.

        Parameters
        ----------
        line : int, optional
            Digital line index (0-7, default ``1``).
        """
        return self.trigger_off(line)

    # ---- Miscellaneous ------------------------------------------------------

    def message(self, text):
        """Send a text message to the Open Ephys console/log.

        Parameters
        ----------
        text : str
            Freeform message text.

        Returns
        -------
        str
            ``'OK MESSAGE'`` on success.
        """
        return self._cmd(f"MESSAGE {text}")

    def close(self):
        """Close the TCP connection.  Safe to call multiple times."""
        try:
            self.sock.close()
        except Exception:
            pass


# ===========================================================================
# PsychoPy user input window (same concept as the original script)
#
# Flow:
#   1. Open a PsychoPy window and attempt TCP connection to the plugin.
#   2. On success → prompt user to press SPACE (start recording) or ESC (quit).
#   3. Set the recording directory name, start recording, display status.
#   4. Send 3 example trigger pulses (1 s apart) as TTL events on line 0.
#   5. Wait for SPACE → stop recording → wait for any key → exit.
# ===========================================================================

# checkTiming=False prevents PsychoPy from hanging at
# "Attempting to measure frame rate of screen, please wait..."
# which freezes on many laptops (hybrid GPU, high CPU load, etc.).
win = visual.Window(size=(800, 600), monitor='testMonitor', units='pix',
                    checkTiming=False)

# --- Step 1: Connect to the Paradigm Bridge TCP server ---
try:
    bridge = ParadigmBridge()
    text_stim = visual.TextStim(
        win,
        text='Connected to Open Ephys!\n\n'
             'Press SPACE to start recording\n'
             'Press ESCAPE to quit',
    )
except ConnectionRefusedError:
    # Connection failed — show troubleshooting steps and exit gracefully.
    text_stim = visual.TextStim(
        win,
        text='ERROR: Cannot connect to Open Ephys.\n\n'
             'Make sure:\n'
             '1. Open Ephys is running\n'
             '2. Paradigm Bridge plugin is in the signal chain\n'
             '3. Press Play (acquire) first\n\n'
             'Press ESCAPE to quit',
    )
    text_stim.draw()
    win.flip()
    event.waitKeys(keyList=['escape'])
    win.close()
    core.quit()

# --- Step 2: Wait for user to press SPACE or ESCAPE ---
keys = event.waitKeys(keyList=['space', 'escape'])

if 'escape' in keys:
    bridge.close()
    win.close()
    core.quit()

# --- Step 3: Start recording with a named directory ---
bridge.set_recording_name("my_experiment")
bridge.start_recording()

text_stim.setText(
    'Recording...\n\n'
    'Trigger annotations are being sent.\n'
    'Press SPACE to stop.'
)
text_stim.draw()
win.flip()

# --- Step 4: Send example annotations ---
# Demonstrates both ANT-Neuro-style annotation types:
#
# (a) Single time-point markers  (line 0) — instantaneous events
#     e.g. stimulus onset, button press
#
# (b) Interval annotations       (line 1) — sustained periods
#     e.g. eyes-open block, task epoch

# --- 4a: Three point markers, 1 s apart ---
for i in range(3):
    bridge.marker(line=0)          # ~1 ms pulse on line 0
    core.wait(1.0, hogCPUperiod=0.0)

# --- 4b: One interval annotation (2 s block) ---
bridge.interval_start(line=1)      # line 1 goes HIGH
core.wait(2.0, hogCPUperiod=0.0)   # interval lasts 2 seconds
bridge.interval_stop(line=1)       # line 1 goes LOW

# --- Step 5: Wait for user to stop recording ---
event.waitKeys(keyList=['space'])

bridge.stop_recording()

text_stim.setText('Recording stopped.\n\nPress any key to exit.')
text_stim.draw()
win.flip()
event.waitKeys()

# --- Cleanup: close TCP socket and PsychoPy window ---
bridge.close()
win.close()
core.quit()
