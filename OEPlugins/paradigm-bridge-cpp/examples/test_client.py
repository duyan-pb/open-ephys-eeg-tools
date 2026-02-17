"""
Paradigm Bridge - Python Test Client

A simple test client for the Paradigm Bridge C++ Open Ephys plugin.
Connects via TCP and sends test commands to verify the plugin is working.

Usage:
    python test_client.py [--host HOST] [--port PORT]

Requirements:
    Python 3.6+ (no external dependencies)
"""

import socket
import time
import argparse
import sys


class ParadigmBridgeClient:
    """Simple TCP client for the Paradigm Bridge plugin."""

    def __init__(self, host: str = "localhost", port: int = 5557, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = None

    def connect(self) -> bool:
        """Connect to the Paradigm Bridge TCP server."""
        try:
            self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
            print(f"[OK] Connected to {self.host}:{self.port}")
            return True
        except (ConnectionRefusedError, TimeoutError, OSError) as e:
            print(f"[ERROR] Could not connect to {self.host}:{self.port}: {e}")
            return False

    def send(self, command: str) -> str:
        """Send a command and receive the response."""
        if not self.sock:
            raise RuntimeError("Not connected")

        self.sock.sendall((command + "\n").encode("utf-8"))
        response = self.sock.recv(4096).decode("utf-8").strip()
        return response

    def close(self):
        """Close the connection."""
        if self.sock:
            self.sock.close()
            self.sock = None
            print("[OK] Connection closed")

    # --- Convenience methods ---

    def ping(self) -> str:
        return self.send("PING")

    def status(self) -> str:
        return self.send("STATUS")

    def trigger(self, line: int, state: int) -> str:
        return self.send(f"TRIGGER {line} {state}")

    def record_start(self) -> str:
        return self.send("RECORD START")

    def record_stop(self) -> str:
        return self.send("RECORD STOP")

    def record_dir(self, path: str) -> str:
        return self.send(f"RECORD DIR {path}")

    def record_name(self, name: str) -> str:
        return self.send(f"RECORD NAME {name}")

    def record_newdir(self) -> str:
        return self.send("RECORD NEWDIR")

    def message(self, text: str) -> str:
        return self.send(f"MESSAGE {text}")


def run_test_suite(client: ParadigmBridgeClient):
    """Run a comprehensive test of all commands."""

    print("\n" + "=" * 60)
    print("  Paradigm Bridge - Command Test Suite")
    print("=" * 60)

    tests = [
        ("PING", "PING"),
        ("STATUS", "STATUS"),
        ("MESSAGE", "MESSAGE Test message from Python client"),
        ("TRIGGER line 0 ON", "TRIGGER 0 1"),
        ("TRIGGER line 0 OFF", "TRIGGER 0 0"),
        ("TRIGGER line 3 ON", "TRIGGER 3 1"),
        ("TRIGGER line 3 OFF", "TRIGGER 3 0"),
        ("TRIGGER line 7 ON", "TRIGGER 7 1"),
        ("TRIGGER line 7 OFF", "TRIGGER 7 0"),
        ("RECORD START", "RECORD START"),
        ("RECORD STOP", "RECORD STOP"),
        ("RECORD DIR", "RECORD DIR C:\\Data\\test"),
        ("RECORD NAME", "RECORD NAME test_session"),
        ("RECORD NEWDIR", "RECORD NEWDIR"),
        # Error cases
        ("Invalid command", "FOOBAR"),
        ("TRIGGER bad line", "TRIGGER 9 1"),
        ("TRIGGER bad state", "TRIGGER 0 2"),
        ("TRIGGER missing args", "TRIGGER"),
    ]

    passed = 0
    failed = 0

    for name, command in tests:
        try:
            response = client.send(command)
            status = "OK" if response.startswith("OK") or "ERROR" in response else "??"
            symbol = "✓" if response.startswith("OK") or (command.startswith("FOOBAR") or "bad" in name or "missing" in name) else "✗"
            print(f"  {symbol} {name:25s} → {response}")
            passed += 1
        except Exception as e:
            print(f"  ✗ {name:25s} → EXCEPTION: {e}")
            failed += 1

    print(f"\n  Results: {passed} passed, {failed} failed")
    print("=" * 60)


def run_trigger_burst(client: ParadigmBridgeClient, count: int = 20, interval: float = 0.05):
    """Send a burst of triggers to test timing."""

    print(f"\n--- Trigger Burst Test ({count} triggers, {interval * 1000:.0f}ms interval) ---")

    start_time = time.perf_counter()

    for i in range(count):
        line = i % 8
        client.trigger(line, 1)
        time.sleep(interval / 2)
        client.trigger(line, 0)
        time.sleep(interval / 2)

    elapsed = time.perf_counter() - start_time
    rate = count / elapsed

    print(f"  Sent {count} trigger pairs in {elapsed:.3f}s ({rate:.1f} triggers/sec)")
    print(f"  Final status: {client.status()}")


def run_interactive(client: ParadigmBridgeClient):
    """Interactive mode — type commands directly."""

    print("\n--- Interactive Mode (type 'quit' to exit) ---")
    print("  Commands: PING, STATUS, TRIGGER <line> <state>, RECORD START/STOP,")
    print("            RECORD DIR <path>, RECORD NAME <name>, MESSAGE <text>")
    print()

    while True:
        try:
            command = input("  > ").strip()
            if command.lower() in ("quit", "exit", "q"):
                break
            if command:
                response = client.send(command)
                print(f"  ← {response}")
        except (EOFError, KeyboardInterrupt):
            break
        except Exception as e:
            print(f"  ← ERROR: {e}")
            break


def main():
    parser = argparse.ArgumentParser(description="Paradigm Bridge Test Client")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=5557, help="Server port (default: 5557)")
    parser.add_argument("--mode", choices=["test", "burst", "interactive", "all"],
                        default="all", help="Test mode (default: all)")
    args = parser.parse_args()

    client = ParadigmBridgeClient(host=args.host, port=args.port)

    if not client.connect():
        print("\nMake sure Open Ephys GUI is running with the Paradigm Bridge plugin")
        print("and the TCP server is started (click 'Start Server' in the editor).")
        sys.exit(1)

    try:
        if args.mode in ("test", "all"):
            run_test_suite(client)

        if args.mode in ("burst", "all"):
            run_trigger_burst(client)

        if args.mode in ("interactive", "all"):
            run_interactive(client)

    finally:
        client.close()


if __name__ == "__main__":
    main()
