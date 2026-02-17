"""
Recording Controller
====================

Wraps the Open Ephys HTTP Server API (port 37497) for recording control.
This is a higher-level wrapper than the raw open-ephys-python-tools library,
with retry logic, connection health checks, and paradigm-specific helpers.
"""

import time
import logging
from typing import Optional

import requests

logger = logging.getLogger(__name__)


class RecordingController:
    """Controls Open Ephys GUI recording via the built-in HTTP Server.

    The HTTP Server is built into Open Ephys GUI v0.6.x+ and listens on
    port 37497 by default. No additional plugins are needed for this
    functionality.

    Parameters
    ----------
    address : str
        IP address of the Open Ephys GUI instance.
    port : int
        HTTP server port (default 37497 = "EPHYS" on a phone keypad).
    timeout : float
        Request timeout in seconds.
    max_retries : int
        Number of connection retries before giving up.

    Examples
    --------
    >>> rc = RecordingController()
    >>> rc.is_connected()
    True
    >>> rc.start_recording("ERP_session_01")
    >>> rc.status()
    'RECORD'
    >>> rc.stop_recording()
    """

    # Open Ephys GUI modes
    MODE_IDLE = "IDLE"
    MODE_ACQUIRE = "ACQUIRE"
    MODE_RECORD = "RECORD"

    def __init__(
        self,
        address: str = "127.0.0.1",
        port: int = 37497,
        timeout: float = 2.0,
        max_retries: int = 3,
    ):
        self.base_url = f"http://{address}:{port}"
        self.timeout = timeout
        self.max_retries = max_retries

    # ------------------------------------------------------------------
    # Low-level HTTP helpers
    # ------------------------------------------------------------------

    def _get(self, endpoint: str) -> dict:
        """Send a GET request to the Open Ephys HTTP API."""
        url = self.base_url + endpoint
        for attempt in range(1, self.max_retries + 1):
            try:
                resp = requests.get(url, timeout=self.timeout)
                resp.raise_for_status()
                return resp.json()
            except requests.exceptions.ConnectionError:
                logger.warning(
                    "Connection attempt %d/%d failed for GET %s",
                    attempt, self.max_retries, endpoint,
                )
                if attempt < self.max_retries:
                    time.sleep(0.5)
            except requests.exceptions.Timeout:
                logger.warning("Timeout on GET %s (attempt %d)", endpoint, attempt)
            except Exception as e:
                logger.error("Unexpected error on GET %s: %s", endpoint, e)
                raise
        raise ConnectionError(
            f"Failed to connect to Open Ephys at {self.base_url} "
            f"after {self.max_retries} attempts. "
            "Is the GUI running with the HTTP Server enabled?"
        )

    def _put(self, endpoint: str, payload: dict) -> dict:
        """Send a PUT request to the Open Ephys HTTP API."""
        url = self.base_url + endpoint
        for attempt in range(1, self.max_retries + 1):
            try:
                resp = requests.put(
                    url, json=payload, timeout=self.timeout
                )
                resp.raise_for_status()
                return resp.json()
            except requests.exceptions.ConnectionError:
                logger.warning(
                    "Connection attempt %d/%d failed for PUT %s",
                    attempt, self.max_retries, endpoint,
                )
                if attempt < self.max_retries:
                    time.sleep(0.5)
            except requests.exceptions.Timeout:
                logger.warning("Timeout on PUT %s (attempt %d)", endpoint, attempt)
            except Exception as e:
                logger.error("Unexpected error on PUT %s: %s", endpoint, e)
                raise
        raise ConnectionError(
            f"Failed to connect to Open Ephys at {self.base_url} "
            f"after {self.max_retries} attempts."
        )

    # ------------------------------------------------------------------
    # Connection
    # ------------------------------------------------------------------

    def is_connected(self) -> bool:
        """Check whether the Open Ephys HTTP Server is reachable."""
        try:
            self._get("/api/status")
            return True
        except (ConnectionError, Exception):
            return False

    def wait_for_connection(self, timeout: float = 30.0, poll_interval: float = 1.0):
        """Block until the Open Ephys GUI is reachable or timeout expires.

        Parameters
        ----------
        timeout : float
            Maximum seconds to wait.
        poll_interval : float
            Seconds between connection attempts.

        Raises
        ------
        TimeoutError
            If the GUI is not reachable within the timeout.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.is_connected():
                logger.info("Connected to Open Ephys at %s", self.base_url)
                return
            time.sleep(poll_interval)
        raise TimeoutError(
            f"Open Ephys GUI not reachable at {self.base_url} "
            f"after {timeout:.0f}s. Is it running?"
        )

    # ------------------------------------------------------------------
    # Status
    # ------------------------------------------------------------------

    def status(self) -> str:
        """Return the current GUI mode: 'IDLE', 'ACQUIRE', or 'RECORD'."""
        data = self._get("/api/status")
        return data.get("mode", "UNKNOWN")

    def is_idle(self) -> bool:
        return self.status() == self.MODE_IDLE

    def is_acquiring(self) -> bool:
        return self.status() == self.MODE_ACQUIRE

    def is_recording(self) -> bool:
        return self.status() == self.MODE_RECORD

    # ------------------------------------------------------------------
    # Recording control
    # ------------------------------------------------------------------

    def start_acquisition(self) -> str:
        """Start data acquisition (IDLE → ACQUIRE).

        Returns
        -------
        str
            New GUI mode after the command.
        """
        data = self._put("/api/status", {"mode": self.MODE_ACQUIRE})
        mode = data.get("mode", "UNKNOWN")
        logger.info("Acquisition started (mode=%s)", mode)
        return mode

    def stop_acquisition(self) -> str:
        """Stop data acquisition (ACQUIRE/RECORD → IDLE).

        Returns
        -------
        str
            New GUI mode after the command.
        """
        data = self._put("/api/status", {"mode": self.MODE_IDLE})
        mode = data.get("mode", "UNKNOWN")
        logger.info("Acquisition stopped (mode=%s)", mode)
        return mode

    def start_recording(self, name: Optional[str] = None) -> str:
        """Start recording (ACQUIRE/IDLE → RECORD).

        Parameters
        ----------
        name : str, optional
            If provided, sets the recording directory prepend text
            (e.g. "ERP_session_01" → directory becomes "ERP_session_01_2026-02-17_...").

        Returns
        -------
        str
            New GUI mode after the command.
        """
        if name is not None:
            self.set_recording_name(name)
        data = self._put("/api/status", {"mode": self.MODE_RECORD})
        mode = data.get("mode", "UNKNOWN")
        logger.info("Recording started (mode=%s, name=%s)", mode, name)
        return mode

    def stop_recording(self) -> str:
        """Stop recording but keep acquisition alive (RECORD → ACQUIRE).

        Returns
        -------
        str
            New GUI mode after the command.
        """
        data = self._put("/api/status", {"mode": self.MODE_ACQUIRE})
        mode = data.get("mode", "UNKNOWN")
        logger.info("Recording stopped (mode=%s)", mode)
        return mode

    # ------------------------------------------------------------------
    # Recording configuration
    # ------------------------------------------------------------------

    def set_recording_name(self, name: str) -> dict:
        """Set the prepend text for the recording directory.

        Parameters
        ----------
        name : str
            Text prepended to the auto-generated recording directory name.
        """
        return self._put("/api/recording", {"prepend_text": name})

    def set_recording_directory(self, path: str) -> dict:
        """Set the parent directory for recordings.

        Parameters
        ----------
        path : str
            Absolute path to the recording parent directory.
        """
        return self._put("/api/recording", {"parent_directory": path})

    def set_append_text(self, text: str) -> dict:
        """Set the append text for the recording directory."""
        return self._put("/api/recording", {"append_text": text})

    def get_recording_info(self) -> dict:
        """Return the current recording configuration."""
        return self._get("/api/recording")

    # ------------------------------------------------------------------
    # Message broadcast
    # ------------------------------------------------------------------

    def send_message(self, message: str) -> dict:
        """Broadcast a text message to all processors during acquisition.

        Parameters
        ----------
        message : str
            Free-form text message.
        """
        return self._put("/api/message", {"text": message})

    # ------------------------------------------------------------------
    # Signal chain info
    # ------------------------------------------------------------------

    def get_processors(self) -> list:
        """Return a list of processors currently in the signal chain."""
        data = self._get("/api/processors")
        return data.get("processors", [])

    def has_network_events(self) -> bool:
        """Check if a Network Events plugin is present in the signal chain."""
        try:
            processors = self.get_processors()
            return any(p.get("name") == "Network Events" for p in processors)
        except Exception:
            return False
