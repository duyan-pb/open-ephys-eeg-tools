"""
Paradigm Bridge — Unified Interface
====================================

The main entry point that combines RecordingController (HTTP) and
TriggerManager (ZMQ/Network Events) into a single, easy-to-use class
for experiment paradigm integration.
"""

import time
import logging
from typing import Optional

from .recorder import RecordingController
from .trigger import TriggerManager

logger = logging.getLogger(__name__)


class ParadigmBridge:
    """Unified bridge between experiment software and Open Ephys GUI.

    Combines recording control (HTTP Server) and trigger annotations
    (Network Events) into a single object.

    Parameters
    ----------
    http_address : str
        IP address of the Open Ephys GUI (HTTP Server).
    http_port : int
        HTTP Server port (default 37497).
    zmq_address : str
        IP address for the Network Events ZMQ socket.
    zmq_port : int
        Network Events ZMQ port (default 5556).
    enable_triggers : bool
        If True, connect to Network Events for TTL triggers.
        Set to False if you only need recording control.
    verbose : bool
        If True, enable INFO-level logging to console.

    Examples
    --------
    Basic recording control (no triggers):

    >>> bridge = ParadigmBridge(enable_triggers=False)
    >>> bridge.start_recording("my_experiment")
    >>> import time; time.sleep(10)
    >>> bridge.stop_recording()

    Full paradigm with triggers:

    >>> bridge = ParadigmBridge()
    >>> bridge.start_recording("ERP_oddball")
    >>> bridge.send_trigger(line=1, state=1)  # stimulus ON
    >>> bridge.send_trigger(line=1, state=0)  # stimulus OFF
    >>> bridge.stop_recording()
    >>> bridge.close()
    """

    def __init__(
        self,
        http_address: str = "127.0.0.1",
        http_port: int = 37497,
        zmq_address: str = "127.0.0.1",
        zmq_port: int = 5556,
        enable_triggers: bool = True,
        verbose: bool = False,
    ):
        # Configure logging
        if verbose:
            logging.basicConfig(
                level=logging.INFO,
                format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
                datefmt="%H:%M:%S",
            )

        # Recording controller (always available — built into GUI)
        self.recorder = RecordingController(
            address=http_address,
            port=http_port,
        )

        # Trigger manager (optional — requires Network Events plugin)
        self.triggers: Optional[TriggerManager] = None
        if enable_triggers:
            try:
                self.triggers = TriggerManager(
                    address=zmq_address,
                    port=zmq_port,
                )
                logger.info("Triggers enabled (Network Events on port %d)", zmq_port)
            except ImportError as e:
                logger.warning("Triggers disabled: %s", e)
            except Exception as e:
                logger.warning("Could not connect to Network Events: %s", e)

    # ------------------------------------------------------------------
    # Connection
    # ------------------------------------------------------------------

    def is_connected(self) -> bool:
        """Check if the Open Ephys GUI HTTP Server is reachable."""
        return self.recorder.is_connected()

    def wait_for_gui(self, timeout: float = 30.0):
        """Block until the Open Ephys GUI is reachable.

        Parameters
        ----------
        timeout : float
            Maximum seconds to wait before raising TimeoutError.
        """
        self.recorder.wait_for_connection(timeout=timeout)

    def check_signal_chain(self) -> dict:
        """Verify the signal chain is ready for paradigm use.

        Returns
        -------
        dict
            Status report with keys:
            - 'connected': bool
            - 'mode': str
            - 'has_network_events': bool
            - 'triggers_available': bool
            - 'processors': list
        """
        report = {
            "connected": False,
            "mode": "UNKNOWN",
            "has_network_events": False,
            "triggers_available": self.triggers is not None,
            "processors": [],
        }
        try:
            report["connected"] = self.recorder.is_connected()
            if report["connected"]:
                report["mode"] = self.recorder.status()
                report["processors"] = [
                    p.get("name", "?") for p in self.recorder.get_processors()
                ]
                report["has_network_events"] = self.recorder.has_network_events()
        except Exception as e:
            logger.warning("Signal chain check failed: %s", e)
        return report

    # ------------------------------------------------------------------
    # Recording control (delegates to RecordingController)
    # ------------------------------------------------------------------

    def status(self) -> str:
        """Return the current GUI mode: 'IDLE', 'ACQUIRE', or 'RECORD'."""
        return self.recorder.status()

    def start_acquisition(self) -> str:
        """Start data acquisition."""
        return self.recorder.start_acquisition()

    def stop_acquisition(self) -> str:
        """Stop data acquisition entirely."""
        return self.recorder.stop_acquisition()

    def start_recording(self, name: Optional[str] = None) -> str:
        """Start recording.

        Parameters
        ----------
        name : str, optional
            Name prepended to the recording directory.
        """
        return self.recorder.start_recording(name=name)

    def stop_recording(self) -> str:
        """Stop recording (keep acquisition running)."""
        return self.recorder.stop_recording()

    def set_recording_name(self, name: str):
        """Set the recording directory prepend text."""
        self.recorder.set_recording_name(name)

    def set_recording_directory(self, path: str):
        """Set the parent directory for recordings."""
        self.recorder.set_recording_directory(path)

    # ------------------------------------------------------------------
    # Trigger annotations (delegates to TriggerManager)
    # ------------------------------------------------------------------

    def send_trigger(self, line: int = 1, state: int = 1) -> Optional[str]:
        """Send a TTL trigger event.

        Parameters
        ----------
        line : int
            TTL line number (1–256).
        state : int
            1 for ON, 0 for OFF.

        Returns
        -------
        str or None
            Response from Network Events, or None if triggers are disabled.
        """
        if self.triggers is None:
            logger.warning(
                "Trigger ignored (triggers not enabled). "
                "Pass enable_triggers=True and ensure Network Events plugin "
                "is in the signal chain."
            )
            return None
        return self.triggers.send(line=line, state=state)

    def trigger_pulse(self, line: int = 1, duration_ms: float = 5.0):
        """Send a brief ON/OFF pulse on a TTL line.

        Parameters
        ----------
        line : int
            TTL line number.
        duration_ms : float
            Pulse duration in milliseconds (default 5 ms).
        """
        if self.triggers is None:
            logger.warning("Trigger pulse ignored (triggers not enabled).")
            return
        self.triggers.pulse(line=line, duration_ms=duration_ms)

    def stimulus_on(self, line: int = 1):
        """Mark stimulus onset."""
        self.send_trigger(line=line, state=1)

    def stimulus_off(self, line: int = 1):
        """Mark stimulus offset."""
        self.send_trigger(line=line, state=0)

    def response(self, line: int = 3):
        """Mark a participant response."""
        if self.triggers:
            self.triggers.response(line=line)

    def block_start(self, line: int = 5):
        """Mark block start."""
        self.send_trigger(line=line, state=1)

    def block_end(self, line: int = 5):
        """Mark block end."""
        self.send_trigger(line=line, state=0)

    def experiment_start(self, line: int = 6):
        """Mark experiment start."""
        self.send_trigger(line=line, state=1)

    def experiment_end(self, line: int = 6):
        """Mark experiment end."""
        self.send_trigger(line=line, state=0)

    # ------------------------------------------------------------------
    # Paradigm helpers
    # ------------------------------------------------------------------

    def run_trial(
        self,
        trial_func,
        trial_number: int = 0,
        trigger_line: int = 1,
        log_triggers: bool = True,
    ):
        """Execute a single trial with automatic trigger bracketing.

        Sends a TTL ON before calling trial_func, then TTL OFF after.

        Parameters
        ----------
        trial_func : callable
            Function that runs the trial (e.g. presents a stimulus).
            Should accept no arguments.
        trial_number : int
            Trial index (for logging).
        trigger_line : int
            TTL line to use for stimulus triggers.
        log_triggers : bool
            Whether to log trigger events.
        """
        if log_triggers:
            logger.info("Trial %d: stimulus ON (line %d)", trial_number, trigger_line)
        self.send_trigger(line=trigger_line, state=1)

        trial_func()

        self.send_trigger(line=trigger_line, state=0)
        if log_triggers:
            logger.info("Trial %d: stimulus OFF (line %d)", trial_number, trigger_line)

    # ------------------------------------------------------------------
    # Cleanup
    # ------------------------------------------------------------------

    def close(self):
        """Clean up connections."""
        if self.triggers is not None:
            self.triggers.close()
        logger.info("ParadigmBridge closed")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
