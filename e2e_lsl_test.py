"""End-to-end LSL simulation test — producer + consumer in one process."""
import pylsl
import numpy as np
import time
import threading

print("=== End-to-End LSL Simulation ===\n")

# Create outlet (producer)
info = pylsl.StreamInfo("TestEEG", "EEG", 8, 250, "float32", "sim_test_001")
channels = info.desc().append_child("channels")
for i in range(8):
    ch = channels.append_child("channel")
    ch.append_child_value("label", f"Ch{i+1}")
    ch.append_child_value("unit", "microvolts")
outlet = pylsl.StreamOutlet(info, 12)
print("[Producer] LSL outlet created: TestEEG, 8ch, 250Hz")

# Consumer in thread
results = {"samples": 0, "latencies": []}

def consumer():
    time.sleep(0.5)
    streams = pylsl.resolve_byprop("source_id", "sim_test_001", timeout=5.0)
    if not streams:
        print("[Consumer] FAILED: no stream found")
        return
    print(f"[Consumer] Resolved stream: {streams[0].name()}")
    inlet = pylsl.StreamInlet(streams[0])
    for _ in range(100):
        sample, ts = inlet.pull_sample(timeout=2.0)
        if sample:
            results["samples"] += 1
            results["latencies"].append(pylsl.local_clock() - ts)

t_consumer = threading.Thread(target=consumer, daemon=True)
t_consumer.start()

# Push data
print("[Producer] Streaming 5 seconds of synthetic EEG...")
for sec in range(5):
    chunk = np.random.randn(250, 8).astype(np.float32) * 50
    t_arr = np.arange(250) / 250.0
    chunk[:, 0] += 40 * np.sin(2 * np.pi * 10 * t_arr)  # alpha on ch0
    for row in chunk:
        outlet.push_sample(row.tolist())
    time.sleep(1.0)
    print(f"  Pushed {(sec+1)*250} samples...")

t_consumer.join(timeout=5)

print()
n = results["samples"]
print(f"[Consumer] Received: {n} samples")
if results["latencies"]:
    avg_lat = np.mean(results["latencies"]) * 1000
    print(f"[Consumer] Avg latency: {avg_lat:.1f} ms")
print("\n=== Simulation Complete ===")
