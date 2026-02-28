"""
TCP Trigger Client for Paradigm Bridge C++ Plugin
==================================================

Sends TTL trigger commands to the paradigm-bridge-cpp Open Ephys plugin
over a plain TCP socket (default port 5557). This is the primary trigger
backend — it talks directly to the C++ plugin without requiring any
additional Open Ephys plugins (no Network Events / ZMQ needed).

Protocol (newline-terminated text):
    TRIGGER <line> <state>   → OK TRIGGER <line> <state>
    PULSE <line>             → OK PULSE <line>
    RECORD START             → OK RECORD START ACCEPTED
    RECORD STOP              → OK RECORD STOP ACCEPTED
    RECORD DIR <path>        → OK RECORD DIR ACCEPTED
    RECORD NAME <name>       → OK RECORD NAME ACCEPTED
    RECORD NEWDIR            → OK RECORD NEWDIR ACCEPTED
    MESSAGE <text>           → OK MESSAGE
    PING                     → OK PONG
    STATUS                   → OK <status string>
"""

import socket
import time
import logging
from typing import Optional

logger = logging.getLogger(__name__)


class TcpTriggerClient:
    """Sends TTL triggers and commands to the paradigm-bridge-cpp plugin.

    Connects via plain TCP to the C++ plugin's command server. No extra
    dependencies required (no ZMQ, no Network Events plugin).

    Parameters
    ----------
    address : str
        IP address of the machine running Open Ephys GUI.
    port : int
        TCP port of the paradigm-bridge-cpp plugin (default 5557).
    timeout : float
        Socket timeout in seconds for connect and recv.
    auto_connect : bool
        If True, connect immediately on construction.

    Examples
    --------
    >>> client = TcpTriggerClient()
    >>> client.send_trigger(line=0, state=1)   # TTL line 0 ON
    >>> client.send_trigger(line=0, state=0)   # TTL line 0 OFF
    >>> client.pulse(line=1, duration_ms=5)    # 5 ms pulse on line 1
    >>> client.close()
    """

    def __init__(
        self,
        address: str = "127.0.0.1",
        port: int = 5557,
        timeout: float = 2.0,
        auto_connect: bool = True,
    ):
        self._address = address
        self._port = port
        self._timeout = timeout
        self._socket: Optional[socket.socket] = None
        self._recv_buffer = ""

        if auto_connect:
            self.connect()

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def connect(self, retries: int = 3, retry_delay: float = 0.5):
        """Connect to the paradigm-bridge-cpp TCP server.

        Parameters
        ----------
        retries : int
            Number of connection attempts.
        retry_delay : float
            Seconds between retries.

        Raises
        ------
        ConnectionError
            If all connection attempts fail.
        """
        self.close()

        for attempt in range(1, retries + 1):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(self._timeout)
                sock.connect((self._address, self._port))
                self._socket = sock
                self._recv_buffer = ""
                logger.info(
                    "Connected to paradigm-bridge-cpp at %s:%d",
                    self._address, self._port,
                )
                return
            except (socket.error, OSError) as e:
                logger.warning(
                    "Connection attempt %d/%d to %s:%d failed: %s",
                    attempt, retries, self._address, self._port, e,
                )
                if attempt < retries:
                    time.sleep(retry_delay)

        raise ConnectionError(
            f"Failed to connect to paradigm-bridge-cpp at "
            f"{self._address}:{self._port} after {retries} attempts. "
            f"Make sure the plugin is loaded in Open Ephys and the "
            f"server is started (click 'Start Server' in the plugin editor)."
        )

    def close(self):
        """Close the TCP connection."""
        if self._socket is not None:
            try:
                self._socket.close()
            except OSError:
                pass
            self._socket = None
            self._recv_buffer = ""
            logger.info("Disconnected from paradigm-bridge-cpp")

    def is_connected(self) -> bool:
        """Check if the TCP socket is connected."""
        return self._socket is not None

    def reconnect(self):
        """Reconnect to the server (e.g. after a disconnect)."""
        self.connect()

    # ------------------------------------------------------------------
    # Low-level command interface
    # ------------------------------------------------------------------

    def send_command(self, command: str) -> str:
        """Send a command and return the response.

        Parameters
        ----------
        command : str
            The command string (without trailing newline).

        Returns
        -------
        str
            The response line from the server.

        Raises
        ------
        ConnectionError
            If not connected or the connection was lost.
        """
        if self._socket is None:
            raise ConnectionError(
                "Not connected to paradigm-bridge-cpp. "
                "Call connect() or set auto_connect=True."
            )

        try:
            # Send newline-terminated command
            msg = command.strip() + "\n"
            self._socket.sendall(msg.encode("utf-8"))

            # Read response (newline-terminated)
            return self._read_line()

        except (socket.error, OSError) as e:
            logger.error("TCP communication error: %s", e)
            self.close()
            raise ConnectionError(f"Lost connection to paradigm-bridge-cpp: {e}")

    def _read_line(self) -> str:
        """Read one newline-terminated line from the socket."""
        while "\n" not in self._recv_buffer:
            chunk = self._socket.recv(4096)
            if not chunk:
                raise ConnectionError("Server closed the connection")
            self._recv_buffer += chunk.decode("utf-8")

        line, self._recv_buffer = self._recv_buffer.split("\n", 1)
        return line.strip()

    # ------------------------------------------------------------------
    # Trigger commands
    # ------------------------------------------------------------------

    def send_trigger(self, line: int = 0, state: int = 1) -> str:
        """Send a TTL trigger event.

        Parameters
        ----------
        line : int
            TTL line number (0–7).
        state : int or bool
            1 (or True) for ON, 0 (or False) for OFF.

        Returns
        -------
        str
            Server response (e.g. "OK TRIGGER 0 1").
        """
        state_val = 1 if state else 0
        return self.send_command(f"TRIGGER {line} {state_val}")

    def pulse(self, line: int = 0, duration_ms: float = 5.0):
        """Send an ON pulse followed by OFF after a short delay.

        Parameters
        ----------
        line : int
            TTL line number (0–7).
        duration_ms : float
            Pulse duration in milliseconds.
        """
        self.send_trigger(line=line, state=1)
        time.sleep(duration_ms / 1000.0)
        self.send_trigger(line=line, state=0)

    def send_point(self, line: int = 0) -> str:
        """Send a point-style annotation (server-side immediate ON/OFF).

        This uses the plugin's single-command ``PULSE`` path when available.
        If connected to an older plugin version that does not support
        ``PULSE``, it falls back to back-to-back ``TRIGGER`` ON/OFF commands.
        """
        resp = self.send_command(f"PULSE {line}")
        if resp.startswith("OK"):
            return resp

        if not resp.startswith("ERROR unknown command"):
            return resp

        logger.warning("Server-side PULSE unsupported (%s); falling back", resp)
        self.send_trigger(line=line, state=1)
        return self.send_trigger(line=line, state=0)

    # ------------------------------------------------------------------
    # Convenience trigger codes for common paradigm events
    # ------------------------------------------------------------------

    # Convention (matching the C++ plugin's 8 TTL lines 0–7):
    #   Line 0 = stimulus onset / offset
    #   Line 1 = stimulus type (standard vs deviant)
    #   Line 2 = response (button press)
    #   Line 3 = feedback
    #   Line 4 = block start / end
    #   Line 5 = experiment start / end
    #   Lines 6–7 = user-defined

    def stimulus_on(self, line: int = 0):
        """Mark stimulus onset (TTL ON)."""
        self.send_trigger(line=line, state=1)

    def stimulus_off(self, line: int = 0):
        """Mark stimulus offset (TTL OFF)."""
        self.send_trigger(line=line, state=0)

    def stimulus_pulse(self, line: int = 0, duration_ms: float = 5.0):
        """Send a brief stimulus-onset pulse."""
        if duration_ms <= 0:
            self.send_point(line=line)
        else:
            self.pulse(line=line, duration_ms=duration_ms)

    def response(self, line: int = 2):
        """Mark a participant response."""
        self.send_point(line=line)

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
    # Recording commands (via the C++ plugin's TCP protocol)
    # ------------------------------------------------------------------

    def record_start(self) -> str:
        """Start recording via the paradigm-bridge-cpp plugin."""
        return self.send_command("RECORD START")

    def record_stop(self) -> str:
        """Stop recording via the paradigm-bridge-cpp plugin."""
        return self.send_command("RECORD STOP")

    def record_dir(self, path: str) -> str:
        """Set the recording parent directory."""
        return self.send_command(f"RECORD DIR {path}")

    def record_name(self, name: str) -> str:
        """Set the recording directory base name."""
        return self.send_command(f"RECORD NAME {name}")

    def record_newdir(self) -> str:
        """Create a new recording directory."""
        return self.send_command("RECORD NEWDIR")

    # ------------------------------------------------------------------
    # Utility commands
    # ------------------------------------------------------------------

    def ping(self) -> bool:
        """Ping the server. Returns True if it responds."""
        try:
            resp = self.send_command("PING")
            return resp.startswith("OK")
        except (ConnectionError, OSError):
            return False

    def get_status(self) -> str:
        """Get the current status from the C++ plugin."""
        return self.send_command("STATUS")

    def send_message(self, text: str) -> str:
        """Send a status message to the Open Ephys GUI console."""
        return self.send_command(f"MESSAGE {text}")

    # ------------------------------------------------------------------
    # Context manager
    # ------------------------------------------------------------------

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def __del__(self):
        self.close()
