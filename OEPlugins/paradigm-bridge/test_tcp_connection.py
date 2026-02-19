"""
Quick test: verify TCP connection to paradigm-bridge-cpp plugin.

Usage:
  1. Launch Open Ephys GUI
  2. Add [File Reader] → [Paradigm Bridge] → [LFP Viewer]
  3. Press Play (acquire)
  4. Run this script: python test_tcp_connection.py
"""

import sys
import os

# Add the package to path so we can import without installing
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from paradigm_bridge.tcp_trigger import TcpTriggerClient
import time


def main():
    print("=" * 60)
    print("Paradigm Bridge TCP Client — Connection Test")
    print("=" * 60)

    # Step 1: Connect
    print("\n[1] Connecting to paradigm-bridge-cpp on 127.0.0.1:5557 ...")
    try:
        client = TcpTriggerClient(address="127.0.0.1", port=5557, auto_connect=True)
        print("    ✓ Connected!")
    except ConnectionError as e:
        print(f"    ✗ Connection failed: {e}")
        print("    Make sure Open Ephys is running with the Paradigm Bridge plugin loaded.")
        return 1

    # Step 2: Ping
    print("\n[2] Sending PING ...")
    try:
        resp = client.ping()
        print(f"    ✓ Response: {resp}")
    except Exception as e:
        print(f"    ✗ Ping failed: {e}")
        client.close()
        return 1

    # Step 3: Status
    print("\n[3] Requesting STATUS ...")
    try:
        resp = client.get_status()
        print(f"    ✓ Status: {resp}")
    except Exception as e:
        print(f"    ✗ Status failed: {e}")

    # Step 4: Send triggers
    print("\n[4] Sending trigger pulses on lines 0-3 ...")
    for line in range(4):
        try:
            resp = client.send_trigger(line=line, state=1)
            print(f"    Line {line} ON  → {resp}")
            time.sleep(0.05)
            resp = client.send_trigger(line=line, state=0)
            print(f"    Line {line} OFF → {resp}")
            time.sleep(0.1)
        except Exception as e:
            print(f"    ✗ Trigger on line {line} failed: {e}")
            break

    # Step 5: Pulse helper
    print("\n[5] Sending quick pulse on line 0 (5ms) ...")
    try:
        client.pulse(line=0, duration_ms=5.0)
        print("    ✓ Pulse sent")
    except Exception as e:
        print(f"    ✗ Pulse failed: {e}")

    # Step 6: Message
    print("\n[6] Sending MESSAGE annotation ...")
    try:
        resp = client.send_command("MESSAGE test_from_python")
        print(f"    ✓ Response: {resp}")
    except Exception as e:
        print(f"    ✗ Message failed: {e}")

    # Cleanup
    print("\n[7] Closing connection ...")
    client.close()
    print("    ✓ Done!")

    print("\n" + "=" * 60)
    print("All tests passed! The TCP client can talk to paradigm-bridge-cpp.")
    print("Check the Open Ephys GUI — you should see TTL events in LFP Viewer.")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
