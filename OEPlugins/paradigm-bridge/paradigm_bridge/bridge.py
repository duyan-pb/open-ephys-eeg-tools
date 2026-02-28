"""
Paradigm Bridge — Unified Interface
====================================

The main entry point that combines RecordingController (HTTP) and
trigger annotations into a single, easy-to-use class for experiment
paradigm integration with Open Ephys.

Two trigger backends are supported:
  - **tcp** (default): Connects to the paradigm-bridge-cpp plugin via
    plain TCP. No extra plugins or dependencies needed.
  - **zmq**: Connects to the Network Events plugin via ZMQ. Requires
    pyzmq and the Network Events plugin installed in the signal chain.
"""

import logging
from contextlib import contextmanager
from typing import Optional

from .recorder import RecordingController
from .tcp_trigger import TcpTriggerClient

logger = logging.getLogger(__name__)


class ParadigmBridge:
    """Unified bridge between experiment software and Open Ephys GUI.

    Combines recording control (HTTP Server) and trigger annotations
    into a single object. Uses the paradigm-bridge-cpp TCP plugin
    by default — no extra dependencies or plugins needed.

    Parameters
    ----------
    address : str
        IP address of the machine running Open Ephys GUI.
    http_port : int
        HTTP Server port (default 37497).
    tcp_port : int
        paradigm-bridge-cpp TCP port (default 5557).
    trigger_backend : str
        ``"tcp"`` (default) uses paradigm-bridge-cpp plugin.
        ``"zmq"`` uses Network Events plugin (requires pyzmq).
        ``None`` or ``"none"`` disables triggers entirely.
    zmq_port : int
        Network Events ZMQ port (only used when trigger_backend="zmq").
    verbose : bool
        If True, enable INFO-level logging to console.

    Examples
    --------
    Default usage (TCP → paradigm-bridge-cpp plugin):

    >>> bridge = ParadigmBridge()
    >>> bridge.start_recording("my_experiment")
    >>> bridge.send_trigger(line=0, state=1)   # stimulus ON
    >>> bridge.send_trigger(line=0, state=0)   # stimulus OFF
    >>> bridge.stop_recording()
    >>> bridge.close()

    Recording only (no triggers):

    >>> bridge = ParadigmBridge(trigger_backend=None)
    >>> bridge.start_recording("my_experiment")

    Legacy ZMQ mode (requires Network Events plugin + pyzmq):

    >>> bridge = ParadigmBridge(trigger_backend="zmq")

    Backward-compatible constructor (deprecated kwargs still work):

    >>> bridge = ParadigmBridge(enable_triggers=True)  # same as tcp
    """

    def __init__(
        self,
        address: str = "127.0.0.1",
        http_port: int = 37497,
        tcp_port: int = 5557,
        trigger_backend: str = "tcp",
        zmq_port: int = 5556,
        verbose: bool = False,
        # ---- backward-compat kwargs (ignored if trigger_backend is set) ----
        http_address: Optional[str] = None,
        zmq_address: Optional[str] = None,
        enable_triggers: Optional[bool] = None,
    ):
        # Configure logging
        if verbose:
            logging.basicConfig(
                level=logging.INFO,
                format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
                datefmt="%H:%M:%S",
            )

        # Handle deprecated kwargs
        resolved_address = http_address or address
        zmq_addr = zmq_address or address

        # Handle deprecated enable_triggers kwarg
        if enable_triggers is not None and trigger_backend == "tcp":
            if not enable_triggers:
                trigger_backend = None

        # Recording controller (always available — built into GUI)
        self.recorder = RecordingController(
            address=resolved_address,
            port=http_port,
        )

        # Trigger client
        self._trigger_backend = trigger_backend
        self._tcp_client: Optional[TcpTriggerClient] = None
        self._zmq_triggers = None  # TriggerManager (lazy)

        if trigger_backend == "tcp":
            try:
                self._tcp_client = TcpTriggerClient(
                    address=resolved_address,
                    port=tcp_port,
                    auto_connect=True,
                )
                logger.info(
                    "Triggers enabled (TCP → paradigm-bridge-cpp on port %d)",
                    tcp_port,
                )
            except ConnectionError as e:
                logger.warning(
                    "Could not connect to paradigm-bridge-cpp on port %d: %s. "
                    "Make sure the plugin is loaded and the server is started.",
                    tcp_port, e,
                )
        elif trigger_backend == "zmq":
            try:
                from .trigger import TriggerManager
                self._zmq_triggers = TriggerManager(
                    address=zmq_addr,
                    port=zmq_port,
                )
                logger.info(
                    "Triggers enabled (ZMQ → Network Events on port %d)",
                    zmq_port,
                )
            except ImportError as e:
                logger.warning("ZMQ triggers disabled: %s", e)
            except Exception as e:
                logger.warning("Could not connect to Network Events: %s", e)
        elif trigger_backend is None or trigger_backend == "none":
            logger.info("Triggers disabled")
        else:
            raise ValueError(
                f"Unknown trigger_backend={trigger_backend!r}. "
                f"Use 'tcp', 'zmq', or None."
            )

        # Backward-compat alias
        self.triggers = self._zmq_triggers

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
            - 'trigger_backend': str or None
            - 'triggers_available': bool
            - 'processors': list
        """
        report = {
            "connected": False,
            "mode": "UNKNOWN",
            "trigger_backend": self._trigger_backend,
            "triggers_available": self._has_triggers(),
            "processors": [],
        }
        try:
            report["connected"] = self.recorder.is_connected()
            if report["connected"]:
                report["mode"] = self.recorder.status()
                report["processors"] = [
                    p.get("name", "?") for p in self.recorder.get_processors()
                ]
        except Exception as e:
            logger.warning("Signal chain check failed: %s", e)
        return report

    def _has_triggers(self) -> bool:
        """Return True if any trigger backend is available."""
        if self._tcp_client is not None and self._tcp_client.is_connected():
            return True
        if self._zmq_triggers is not None:
            return True
        return False

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
    # Trigger annotations (dispatches to TCP or ZMQ backend)
    # ------------------------------------------------------------------

    def send_trigger(self, line: int = 0, state: int = 1) -> Optional[str]:
        """Send a TTL trigger event.

        Parameters
        ----------
        line : int
            TTL line number. TCP backend: 0–7. ZMQ backend: 1–256.
        state : int
            1 for ON, 0 for OFF.

        Returns
        -------
        str or None
            Server response, or None if triggers are disabled.
        """
        # TCP backend (paradigm-bridge-cpp)
        if self._tcp_client is not None:
            try:
                return self._tcp_client.send_trigger(line=line, state=state)
            except ConnectionError as e:
                logger.warning("TCP trigger failed: %s", e)
                return None

        # ZMQ backend (Network Events)
        if self._zmq_triggers is not None:
            return self._zmq_triggers.send(line=line, state=state)

        logger.warning(
            "Trigger ignored (no trigger backend connected). "
            "Use trigger_backend='tcp' and start the plugin server, "
            "or trigger_backend='zmq' with Network Events plugin."
        )
        return None

    def trigger_pulse(self, line: int = 0, duration_ms: float = 5.0):
        """Send a brief ON/OFF pulse on a TTL line.

        Parameters
        ----------
        line : int
            TTL line number.
        duration_ms : float
            Pulse duration in milliseconds (default 5 ms).
        """
        # TCP backend
        if self._tcp_client is not None:
            try:
                if duration_ms <= 0:
                    self._tcp_client.send_point(line=line)
                else:
                    self._tcp_client.pulse(line=line, duration_ms=duration_ms)
                return
            except ConnectionError as e:
                logger.warning("TCP trigger pulse failed: %s", e)
                return

        # ZMQ backend
        if self._zmq_triggers is not None:
            self._zmq_triggers.pulse(line=line, duration_ms=duration_ms)
            return

        logger.warning("Trigger pulse ignored (no trigger backend connected).")

    def stimulus_on(self, line: int = 0):
        """Mark stimulus onset."""
        self.send_trigger(line=line, state=1)

    def stimulus_off(self, line: int = 0):
        """Mark stimulus offset."""
        self.send_trigger(line=line, state=0)

    def response(self, line: int = 2):
        """Mark a participant response."""
        self.annotation_point(line=line)

    def block_start(self, line: int = 4):
        """Mark block start."""
        self.send_trigger(line=line, state=1)

    def block_end(self, line: int = 4):
        """Mark block end."""
        self.send_trigger(line=line, state=0)

    def experiment_start(self, line: int = 5):
        """Mark experiment start."""
        self.send_trigger(line=line, state=1)

    def experiment_end(self, line: int = 5):
        """Mark experiment end."""
        self.send_trigger(line=line, state=0)

    # ------------------------------------------------------------------
    # Annotation helpers (point vs interval)
    # ------------------------------------------------------------------

    def annotation_point(self, line: int = 0):
        """Send a single time-point annotation.

        TCP backend uses the plugin's single-command ``PULSE`` path when
        available. ZMQ backend falls back to an immediate ON/OFF pulse.
        """
        if self._tcp_client is not None:
            try:
                return self._tcp_client.send_point(line=line)
            except ConnectionError as e:
                logger.warning("TCP point annotation failed: %s", e)
                return None

        if self._zmq_triggers is not None:
            return self._zmq_triggers.pulse(line=line, duration_ms=0.0)

        logger.warning("Point annotation ignored (no trigger backend connected).")
        return None

    def annotation_start(self, line: int = 0):
        """Start an interval annotation (TTL ON)."""
        return self.send_trigger(line=line, state=1)

    def annotation_end(self, line: int = 0):
        """End an interval annotation (TTL OFF)."""
        return self.send_trigger(line=line, state=0)

    begin_interval = annotation_start
    end_interval = annotation_end
    mark_point = annotation_point

    @contextmanager
    def annotation_interval(self, line: int = 0):
        """Context manager for interval annotations.

        Example
        -------
        >>> with bridge.annotation_interval(line=0):
        ...     present_stimulus()
        """
        self.annotation_start(line=line)
        try:
            yield
        finally:
            self.annotation_end(line=line)

    # ------------------------------------------------------------------
    # Paradigm helpers
    # ------------------------------------------------------------------

    def run_trial(
        self,
        trial_func,
        trial_number: int = 0,
        trigger_line: int = 0,
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
        if self._tcp_client is not None:
            self._tcp_client.close()
        if self._zmq_triggers is not None:
            self._zmq_triggers.close()
        logger.info("ParadigmBridge closed")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
