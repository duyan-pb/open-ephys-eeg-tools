"""
Paradigm Bridge — Interactive Demo & Test Script
=================================================

This script lets you test the Paradigm Bridge without PsychoPy.
It works in two modes:

  Mode 1 (default): Simulated — spins up a mock TCP server to verify
         the protocol works end-to-end, no Open Ephys required.

  Mode 2 (--live):  Connects to a running Open Ephys with the
         paradigm-bridge-cpp plugin loaded. Sends real triggers.

Usage:
  python test_paradigm_bridge_demo.py          # simulated test
  python test_paradigm_bridge_demo.py --live   # connect to Open Ephys
"""

import sys
import time
import socket
import threading
import argparse


# ──────────────────────────────────────────────────────────────────
#  Mock TCP server (simulates the C++ plugin for offline testing)
# ──────────────────────────────────────────────────────────────────

class MockParadigmBridgeServer:
    """Minimal TCP server that emulates the paradigm-bridge-cpp protocol."""

    def __init__(self, host="127.0.0.1", port=15557):
        self.host = host
        self.port = port
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((host, port))
        self.server.listen(1)
        self.running = True
        self.trigger_count = 0
        self.commands_received = []
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        self.server.settimeout(1.0)
        while self.running:
            try:
                client, addr = self.server.accept()
                print(f"  [SERVER] Client connected from {addr}")
                self._handle_client(client)
            except socket.timeout:
                continue

    def _handle_client(self, client: socket.socket):
        client.settimeout(1.0)
        buf = ""
        while self.running:
            try:
                data = client.recv(4096).decode()
                if not data:
                    break
                buf += data
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    self.commands_received.append(line)
                    response = self._process(line)
                    client.sendall((response + "\n").encode())
            except socket.timeout:
                continue
            except ConnectionError:
                break
        client.close()

    def _process(self, cmd: str) -> str:
        parts = cmd.split()
        verb = parts[0].upper() if parts else ""

        if verb == "PING":
            return "OK PONG"
        elif verb == "TRIGGER":
            self.trigger_count += 1
            line_n = parts[1] if len(parts) > 1 else "?"
            state = parts[2] if len(parts) > 2 else "?"
            return f"OK TRIGGER {line_n} {state}"
        elif verb == "RECORD":
            action = parts[1].upper() if len(parts) > 1 else "?"
            return f"OK RECORD {action} ACCEPTED"
        elif verb == "MESSAGE":
            text = " ".join(parts[1:])
            return f"OK MESSAGE {text}"
        elif verb == "STATUS":
            return f"OK ACQUISITION=ON RECORDING=OFF TRIGGERS={self.trigger_count} DROPPED=0 REMOTE=OFF"
        elif verb == "AUTH":
            return "OK AUTH"
        else:
            return f"ERROR unknown command: {cmd}"

    def stop(self):
        self.running = False
        self.server.close()


# ──────────────────────────────────────────────────────────────────
#  Simple TCP client (same as in the release scripts)
# ──────────────────────────────────────────────────────────────────

class SimpleBridgeClient:
    def __init__(self, host="127.0.0.1", port=5557):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(10.0)
        self.sock.connect((host, port))

    def send(self, cmd: str) -> str:
        self.sock.sendall((cmd + "\n").encode())
        return self.sock.recv(4096).decode().strip()

    def reconnect(self):
        try:
            self.sock.close()
        except Exception:
            pass
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(10.0)
        self.sock.connect((self.host, self.port))

    def close(self):
        self.sock.close()


# ──────────────────────────────────────────────────────────────────
#  Test Scenarios
# ──────────────────────────────────────────────────────────────────

def run_protocol_tests(client: SimpleBridgeClient):
    """Run through all protocol commands and print results."""

    print("\n" + "=" * 60)
    print("  PARADIGM BRIDGE — PROTOCOL TEST")
    print("=" * 60)

    tests = [
        ("PING",                     "OK PONG"),
        ("STATUS",                   "OK ACQUISITION"),
        ("RECORD NAME test_subject", "OK RECORD NAME"),
        ("RECORD DIR C:\\Data",      "OK RECORD DIR"),
        ("RECORD START",             "OK RECORD START"),
        ("TRIGGER 0 1",              "OK TRIGGER 0 1"),
        ("TRIGGER 0 0",              "OK TRIGGER 0 0"),
        ("TRIGGER 1 1",              "OK TRIGGER 1 1"),
        ("TRIGGER 1 0",              "OK TRIGGER 1 0"),
        ("MESSAGE Trial 1 started",  "OK MESSAGE"),
        ("RECORD STOP",              "OK RECORD STOP"),
    ]

    passed = 0
    failed = 0

    for cmd, expected_prefix in tests:
        try:
            resp = client.send(cmd)
            ok = resp.startswith(expected_prefix)
            status = "PASS" if ok else "FAIL"
            if ok:
                passed += 1
            else:
                failed += 1
            print(f"  [{status}]  {cmd:35s} -> {resp}")
        except Exception as e:
            failed += 1
            print(f"  [FAIL]  {cmd:35s} -> ERROR: {e}")

    print("-" * 60)
    print(f"  Results: {passed} passed, {failed} failed, {passed + failed} total")
    print("=" * 60)
    return failed == 0


def run_trigger_burst(client: SimpleBridgeClient, count=5, interval=0.5):
    """Send a burst of trigger pulses to simulate an experiment."""

    print(f"\n  Sending {count} trigger pulses (line 0, {interval}s apart)...")
    for i in range(count):
        try:
            r1 = client.send("TRIGGER 0 1")
            time.sleep(0.005)  # 5ms pulse
            r2 = client.send("TRIGGER 0 0")
            print(f"    Pulse {i+1}/{count}: ON={r1}, OFF={r2}")
        except (TimeoutError, ConnectionError, OSError) as e:
            print(f"    Pulse {i+1}/{count}: connection lost ({e}), reconnecting...")
            try:
                client.reconnect()
                r1 = client.send("TRIGGER 0 1")
                time.sleep(0.005)
                r2 = client.send("TRIGGER 0 0")
                print(f"    Pulse {i+1}/{count} (retry): ON={r1}, OFF={r2}")
            except Exception as e2:
                print(f"    Pulse {i+1}/{count}: retry failed ({e2}), skipping")
        if i < count - 1:
            time.sleep(interval)
    print("  Done!\n")


# ──────────────────────────────────────────────────────────────────
#  Main
# ──────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Paradigm Bridge test & demo")
    parser.add_argument("--live", action="store_true",
                        help="Connect to a running Open Ephys instance (port 5557)")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5557)
    args = parser.parse_args()

    if args.live:
        # ── LIVE MODE: connect to real Open Ephys ──
        print(f"\n  Connecting to Open Ephys at {args.host}:{args.port}...")
        try:
            client = SimpleBridgeClient(args.host, args.port)
        except ConnectionRefusedError:
            print("\n  ERROR: Connection refused!")
            print("  Make sure:")
            print("    1. Open Ephys GUI is running")
            print("    2. Paradigm Bridge plugin is in the signal chain")
            print("    3. Acquisition is started (press Play)")
            print("    4. The server is started in the plugin editor\n")
            sys.exit(1)

        print("  Connected!\n")
        run_protocol_tests(client)
        run_trigger_burst(client, count=5, interval=0.5)
        client.close()

    else:
        # ── SIMULATED MODE: spin up mock server ──
        print("\n  Starting mock Paradigm Bridge server on port 15557...")
        server = MockParadigmBridgeServer(port=15557)
        time.sleep(0.3)

        try:
            client = SimpleBridgeClient("127.0.0.1", 15557)
            print("  Connected to mock server.\n")

            all_passed = run_protocol_tests(client)
            run_trigger_burst(client, count=3, interval=0.3)

            print(f"  Server received {len(server.commands_received)} commands total:")
            for cmd in server.commands_received:
                print(f"    > {cmd}")
            print()

            client.close()

            if all_passed:
                print("  All tests PASSED. The protocol is working correctly.")
                print("  Next step: run with --live to test against Open Ephys.\n")
            else:
                print("  Some tests FAILED. Check the output above.\n")
                sys.exit(1)

        finally:
            server.stop()


if __name__ == "__main__":
    main()
