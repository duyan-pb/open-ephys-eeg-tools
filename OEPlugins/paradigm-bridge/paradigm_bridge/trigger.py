"""
Trigger Manager
===============

Sends TTL trigger annotations to the Open Ephys GUI via the Network Events
plugin (ZMQ on port 5556). These triggers appear as event markers in the
LFP Viewer and are saved alongside EEG data in recording files.

The Network Events plugin must be present in the signal chain for triggers
to work. Install it via the Plugin Installer (Ctrl+P in Open Ephys GUI).
"""

import time
import logging
from typing import Optional

logger = logging.getLogger(__name__)

# ZMQ is required only when triggers are used
_zmq = None


def _ensure_zmq():
    """Lazy-import zmq so the module can be imported without it installed."""
    global _zmq
    if _zmq is None:
        try:
            import zmq
            _zmq = zmq
        except ImportError:
            raise ImportError(
                "pyzmq is required for trigger annotations. "
                "Install it with: pip install pyzmq"
            )
    return _zmq


class TriggerManager:
    """Sends TTL trigger events to the Open Ephys Network Events plugin.

    The Network Events plugin listens on a ZMQ REQ/REP socket (default
    port 5556). Each trigger sets or clears a TTL line (1–256), which
    appears as an event marker in the Open Ephys GUI.

    Parameters
    ----------
    address : str
        IP address of the machine running Open Ephys GUI.
    port : int
        ZMQ port of the Network Events plugin (default 5556).
    recv_timeout_ms : int
        Timeout in milliseconds for receiving ZMQ responses.

    Examples
    --------
    >>> tm = TriggerManager()
    >>> tm.send(line=1, state=1)   # TTL line 1 ON
    >>> tm.send(line=1, state=0)   # TTL line 1 OFF
    >>> tm.pulse(line=2, duration_ms=5)  # 5 ms pulse on line 2
    """

    def __init__(
        self,
        address: str = "127.0.0.1",
        port: int = 5556,
        recv_timeout_ms: int = 1000,
    ):
        zmq = _ensure_zmq()

        self.url = f"tcp://{address}:{port}"
        self._context = zmq.Context()
        self._socket = self._context.socket(zmq.REQ)
        self._socket.RCVTIMEO = recv_timeout_ms
        self._socket.connect(self.url)
        self._connected = True

        logger.info("TriggerManager connected to %s", self.url)

    # ------------------------------------------------------------------
    # Core trigger methods
    # ------------------------------------------------------------------

    def send(self, line: int = 1, state: int = 1) -> Optional[str]:
        """Send a single TTL event.

        Parameters
        ----------
        line : int
            TTL line number (1–256).
        state : int or bool
            1 (or True) for ON, 0 (or False) for OFF.

        Returns
        -------
        str or None
            The response string from Network Events, or None on timeout.
        """
        state_val = 1 if state else 0
        message = f"TTL Line={line} State={state_val}"
        return self._send_message(message)

    def pulse(self, line: int = 1, duration_ms: float = 5.0):
        """Send an ON pulse followed by OFF after a short delay.

        This is the most common trigger pattern: a brief pulse that marks
        the exact moment of an event (stimulus onset, response, etc.).

        Parameters
        ----------
        line : int
            TTL line number (1–256).
        duration_ms : float
            Pulse duration in milliseconds.
        """
        self.send(line=line, state=1)
        time.sleep(duration_ms / 1000.0)
        self.send(line=line, state=0)

    def send_text(self, message: str) -> Optional[str]:
        """Send a raw text message to the Network Events plugin.

        This can be used for custom string-based event markers that
        the Network Events plugin may support.

        Parameters
        ----------
        message : str
            Arbitrary message string.

        Returns
        -------
        str or None
            Response from the plugin.
        """
        return self._send_message(message)

    # ------------------------------------------------------------------
    # Convenience trigger codes for common paradigm events
    # ------------------------------------------------------------------

    # Standard EEG paradigm event codes (by convention):
    #   Line 1  = stimulus onset
    #   Line 2  = stimulus offset
    #   Line 3  = response (button press)
    #   Line 4  = feedback
    #   Line 5  = block start/end
    #   Line 6  = experiment start/end
    #   Lines 7+ = user-defined

    def stimulus_on(self, line: int = 1):
        """Mark stimulus onset (TTL ON on the given line)."""
        self.send(line=line, state=1)

    def stimulus_off(self, line: int = 1):
        """Mark stimulus offset (TTL OFF on the given line)."""
        self.send(line=line, state=0)

    def stimulus_pulse(self, line: int = 1, duration_ms: float = 5.0):
        """Send a brief stimulus-onset pulse."""
        self.pulse(line=line, duration_ms=duration_ms)

    def response(self, line: int = 3):
        """Mark a participant response."""
        self.pulse(line=line, duration_ms=5.0)

    def block_start(self, line: int = 5):
        """Mark block start."""
        self.send(line=line, state=1)

    def block_end(self, line: int = 5):
        """Mark block end."""
        self.send(line=line, state=0)

    def experiment_start(self, line: int = 6):
        """Mark experiment start."""
        self.send(line=line, state=1)

    def experiment_end(self, line: int = 6):
        """Mark experiment end."""
        self.send(line=line, state=0)

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def is_connected(self) -> bool:
        """Check if the ZMQ socket was created successfully."""
        return self._connected

    def close(self):
        """Close the ZMQ socket and context."""
        if self._socket is not None:
            self._socket.close()
        if self._context is not None:
            self._context.term()
        self._connected = False
        logger.info("TriggerManager disconnected")

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    def _send_message(self, message: str) -> Optional[str]:
        """Send a string to the Network Events plugin and wait for reply."""
        try:
            self._socket.send_string(message)
            response = self._socket.recv().decode("utf-8")
            logger.debug("Sent: %s → Response: %s", message, response)
            return response
        except Exception as e:
            logger.warning("Failed to send trigger '%s': %s", message, e)
            return None

    # ------------------------------------------------------------------
    # Context manager
    # ------------------------------------------------------------------

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
