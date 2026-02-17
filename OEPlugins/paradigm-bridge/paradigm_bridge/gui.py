"""
Standalone GUI for Paradigm Bridge
===================================

A simple tkinter-based window that lets users start/stop recordings and
send trigger events without needing PsychoPy or any other paradigm
software.  This is the "user input window" from Alexey's original script,
reimplemented with richer controls.

Usage
-----
Run directly:
    python -m paradigm_bridge.gui

Or from another script:
    from paradigm_bridge.gui import launch_gui
    launch_gui()
"""

import sys
import time
import logging
import threading
from typing import Optional

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Import check — tkinter is in the stdlib but may be missing on some systems
# ---------------------------------------------------------------------------
try:
    import tkinter as tk
    from tkinter import ttk, messagebox
except ImportError:
    tk = None  # type: ignore


def _require_tk():
    if tk is None:
        raise ImportError(
            "tkinter is required for the standalone GUI. "
            "On Debian/Ubuntu: sudo apt install python3-tk"
        )


# ===========================================================================
# GUI Application
# ===========================================================================

class ParadigmBridgeGUI:
    """Standalone tkinter application for controlling Open Ephys recordings
    and sending trigger annotations.
    """

    POLL_INTERVAL_MS = 2000  # status polling interval

    def __init__(
        self,
        http_address: str = "127.0.0.1",
        http_port: int = 37497,
        zmq_address: str = "127.0.0.1",
        zmq_port: int = 5556,
    ):
        _require_tk()

        from .bridge import ParadigmBridge

        self.bridge = ParadigmBridge(
            http_address=http_address,
            http_port=http_port,
            zmq_address=zmq_address,
            zmq_port=zmq_port,
            enable_triggers=True,
            verbose=True,
        )

        # ---- Main window ----
        self.root = tk.Tk()
        self.root.title("Paradigm Bridge — Open Ephys Controller")
        self.root.geometry("520x620")
        self.root.resizable(False, False)

        self._build_ui()
        self._poll_status()

    # ------------------------------------------------------------------
    # UI Construction
    # ------------------------------------------------------------------

    def _build_ui(self):
        root = self.root
        pad = {"padx": 10, "pady": 5}

        # ---- Connection status ----
        frm_conn = ttk.LabelFrame(root, text="Connection")
        frm_conn.pack(fill="x", **pad)

        self.lbl_status = ttk.Label(frm_conn, text="Status: checking…")
        self.lbl_status.pack(side="left", padx=10, pady=8)

        self.lbl_mode = ttk.Label(frm_conn, text="Mode: —")
        self.lbl_mode.pack(side="right", padx=10, pady=8)

        # ---- Recording name ----
        frm_name = ttk.LabelFrame(root, text="Recording Name")
        frm_name.pack(fill="x", **pad)

        self.var_name = tk.StringVar(value="")
        ent_name = ttk.Entry(frm_name, textvariable=self.var_name, width=50)
        ent_name.pack(side="left", padx=10, pady=8, fill="x", expand=True)

        btn_set_name = ttk.Button(frm_name, text="Set", command=self._set_name)
        btn_set_name.pack(side="right", padx=10, pady=8)

        # ---- Recording controls ----
        frm_rec = ttk.LabelFrame(root, text="Recording Control")
        frm_rec.pack(fill="x", **pad)

        btn_frame = ttk.Frame(frm_rec)
        btn_frame.pack(pady=10)

        self.btn_acquire = ttk.Button(
            btn_frame, text="▶ Start Acquisition", command=self._start_acquisition, width=22
        )
        self.btn_acquire.grid(row=0, column=0, padx=5, pady=3)

        self.btn_record = ttk.Button(
            btn_frame, text="⏺ Start Recording", command=self._start_recording, width=22
        )
        self.btn_record.grid(row=0, column=1, padx=5, pady=3)

        self.btn_stop_rec = ttk.Button(
            btn_frame, text="⏹ Stop Recording", command=self._stop_recording, width=22
        )
        self.btn_stop_rec.grid(row=1, column=0, padx=5, pady=3)

        self.btn_stop = ttk.Button(
            btn_frame, text="⏸ Stop All", command=self._stop_all, width=22
        )
        self.btn_stop.grid(row=1, column=1, padx=5, pady=3)

        # ---- Trigger controls ----
        frm_trig = ttk.LabelFrame(root, text="Trigger Annotations (Network Events)")
        frm_trig.pack(fill="x", **pad)

        trig_row1 = ttk.Frame(frm_trig)
        trig_row1.pack(pady=5)

        ttk.Label(trig_row1, text="TTL Line:").pack(side="left", padx=5)
        self.var_line = tk.IntVar(value=1)
        spn_line = ttk.Spinbox(
            trig_row1, from_=1, to=256, textvariable=self.var_line, width=5
        )
        spn_line.pack(side="left", padx=5)

        ttk.Label(trig_row1, text="Pulse (ms):").pack(side="left", padx=5)
        self.var_pulse_ms = tk.DoubleVar(value=5.0)
        spn_pulse = ttk.Spinbox(
            trig_row1, from_=1.0, to=1000.0, increment=1.0,
            textvariable=self.var_pulse_ms, width=8,
        )
        spn_pulse.pack(side="left", padx=5)

        trig_row2 = ttk.Frame(frm_trig)
        trig_row2.pack(pady=5)

        btn_on = ttk.Button(
            trig_row2, text="TTL ON", command=self._ttl_on, width=12
        )
        btn_on.grid(row=0, column=0, padx=4, pady=2)

        btn_off = ttk.Button(
            trig_row2, text="TTL OFF", command=self._ttl_off, width=12
        )
        btn_off.grid(row=0, column=1, padx=4, pady=2)

        btn_pulse = ttk.Button(
            trig_row2, text="Pulse", command=self._ttl_pulse, width=12
        )
        btn_pulse.grid(row=0, column=2, padx=4, pady=2)

        # Quick-trigger buttons for common paradigm events
        trig_row3 = ttk.Frame(frm_trig)
        trig_row3.pack(pady=(5, 10))

        for i, (label, cmd) in enumerate([
            ("Stim ON",  lambda: self._quick_trigger("stimulus_on")),
            ("Stim OFF", lambda: self._quick_trigger("stimulus_off")),
            ("Response", lambda: self._quick_trigger("response")),
            ("Block ▶",  lambda: self._quick_trigger("block_start")),
            ("Block ⏹",  lambda: self._quick_trigger("block_end")),
        ]):
            ttk.Button(trig_row3, text=label, command=cmd, width=10).grid(
                row=0, column=i, padx=2, pady=2
            )

        # ---- Log ----
        frm_log = ttk.LabelFrame(root, text="Log")
        frm_log.pack(fill="both", expand=True, **pad)

        self.txt_log = tk.Text(frm_log, height=8, state="disabled", wrap="word",
                               font=("Consolas", 9))
        self.txt_log.pack(fill="both", expand=True, padx=5, pady=5)

    # ------------------------------------------------------------------
    # Actions
    # ------------------------------------------------------------------

    def _log(self, message: str):
        ts = time.strftime("%H:%M:%S")
        self.txt_log.config(state="normal")
        self.txt_log.insert("end", f"[{ts}] {message}\n")
        self.txt_log.see("end")
        self.txt_log.config(state="disabled")

    def _set_name(self):
        name = self.var_name.get().strip()
        if name:
            try:
                self.bridge.set_recording_name(name)
                self._log(f"Recording name set to: {name}")
            except Exception as e:
                self._log(f"Error setting name: {e}")
        else:
            self._log("Name is empty — nothing to set.")

    def _start_acquisition(self):
        try:
            mode = self.bridge.start_acquisition()
            self._log(f"Acquisition started (mode={mode})")
        except Exception as e:
            self._log(f"Error: {e}")

    def _start_recording(self):
        name = self.var_name.get().strip() or None
        try:
            mode = self.bridge.start_recording(name=name)
            self._log(f"Recording started (mode={mode}, name={name})")
        except Exception as e:
            self._log(f"Error: {e}")

    def _stop_recording(self):
        try:
            mode = self.bridge.stop_recording()
            self._log(f"Recording stopped (mode={mode})")
        except Exception as e:
            self._log(f"Error: {e}")

    def _stop_all(self):
        try:
            mode = self.bridge.stop_acquisition()
            self._log(f"All stopped (mode={mode})")
        except Exception as e:
            self._log(f"Error: {e}")

    def _ttl_on(self):
        line = self.var_line.get()
        try:
            resp = self.bridge.send_trigger(line=line, state=1)
            self._log(f"TTL ON  line={line} → {resp}")
        except Exception as e:
            self._log(f"Trigger error: {e}")

    def _ttl_off(self):
        line = self.var_line.get()
        try:
            resp = self.bridge.send_trigger(line=line, state=0)
            self._log(f"TTL OFF line={line} → {resp}")
        except Exception as e:
            self._log(f"Trigger error: {e}")

    def _ttl_pulse(self):
        line = self.var_line.get()
        ms = self.var_pulse_ms.get()
        # Run pulse in a thread to avoid blocking the GUI
        def _do_pulse():
            try:
                self.bridge.trigger_pulse(line=line, duration_ms=ms)
                self.root.after(0, lambda: self._log(
                    f"Pulse line={line} duration={ms:.0f}ms"
                ))
            except Exception as e:
                self.root.after(0, lambda: self._log(f"Pulse error: {e}"))
        threading.Thread(target=_do_pulse, daemon=True).start()

    def _quick_trigger(self, method_name: str):
        try:
            getattr(self.bridge, method_name)()
            self._log(f"Quick trigger: {method_name}")
        except Exception as e:
            self._log(f"Trigger error ({method_name}): {e}")

    # ------------------------------------------------------------------
    # Status polling
    # ------------------------------------------------------------------

    def _poll_status(self):
        """Periodically check the Open Ephys GUI status."""
        def _check():
            try:
                connected = self.bridge.is_connected()
                if connected:
                    mode = self.bridge.status()
                    self.lbl_status.config(text="Status: ✅ Connected")
                    self.lbl_mode.config(text=f"Mode: {mode}")
                else:
                    self.lbl_status.config(text="Status: ❌ Disconnected")
                    self.lbl_mode.config(text="Mode: —")
            except Exception:
                self.lbl_status.config(text="Status: ❌ Error")
                self.lbl_mode.config(text="Mode: —")

        threading.Thread(target=_check, daemon=True).start()
        self.root.after(self.POLL_INTERVAL_MS, self._poll_status)

    # ------------------------------------------------------------------
    # Run
    # ------------------------------------------------------------------

    def run(self):
        """Start the tkinter event loop."""
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.mainloop()

    def _on_close(self):
        self.bridge.close()
        self.root.destroy()


# ===========================================================================
# Module entry point
# ===========================================================================

def launch_gui(**kwargs):
    """Launch the standalone Paradigm Bridge GUI.

    Parameters
    ----------
    **kwargs
        Passed to ParadigmBridgeGUI (http_address, http_port, zmq_address, zmq_port).
    """
    _require_tk()
    app = ParadigmBridgeGUI(**kwargs)
    app.run()


if __name__ == "__main__":
    launch_gui()
