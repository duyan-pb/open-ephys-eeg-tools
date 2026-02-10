# Protocol Analysis & Optimization Report

**InEar Teensy EEG Streaming Protocols — Findings, Comparisons & Optimization Tactics**

*Date: February 2026*

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [The Three Protocols](#2-the-three-protocols)
3. [Packet Structure Visualizations](#3-packet-structure-visualizations)
4. [Data Content Comparison](#4-data-content-comparison)
5. [Bandwidth Analysis](#5-bandwidth-analysis)
6. [Stream Behavior Over Time](#6-stream-behavior-over-time)
7. [Trade-Off Matrix (Original vs Optimized)](#7-trade-off-matrix-original-vs-optimized)
8. [Complete Optimization Taxonomy](#8-complete-optimization-taxonomy)
9. [Optimization Tactic Details (All 28 Tactics)](#9-optimization-tactic-details)
10. [Full Trade-Off Matrix (All 28 Tactics)](#10-full-trade-off-matrix-all-28-tactics)
11. [Recommended "Free Wins"](#11-recommended-free-wins)
12. [Why OpenBCI Comparison Is Not Apples-to-Apples](#12-why-openbci-comparison-is-not-apples-to-apples)
13. [Appendix: Serial 8-N-1 Bandwidth Math](#13-appendix-serial-8-n-1-bandwidth-math)

---

## 1. Executive Summary

This document analyzes three EEG streaming protocols used in the Open Ephys ecosystem:

| | OpenBCI Cyton | InEar Original | InEar Optimized |
|---|---|---|---|
| **Purpose** | General-purpose 8ch EEG | In-ear 5ch EEG + biometrics | Same hardware, smarter protocol |
| **Packet Size** | 33 bytes (fixed) | 56 bytes (fixed) | 26-55 bytes (variable) |
| **Sample Rate** | 250 Hz | 1000 Hz | 1000 Hz |
| **Baud Rate** | 115,200 | 2,000,000 | 2,000,000 |
| **Data Rate** | 8,250 B/s | 56,000 B/s | ~28,500 B/s |
| **Bandwidth Used** | 71.6% | 28.0% | 14.3% |
| **Error Detection** | None (relies on USB) | XOR checksum | XOR checksum |
| **Sensors** | 8 EEG + accelerometer | 5 EEG + accel + PPG + temp + battery + sync | Same sensors, sent at native rates |

**Key finding**: The optimized protocol saves ~49% bandwidth by only sending auxiliary sensor data when it actually changes, while maintaining identical EEG quality.

---

## 2. The Three Protocols

### 2.1 OpenBCI Cyton

A wireless EEG streaming protocol using RFduino (nRF51822) modules. The Cyton board transmits to a USB dongle over 2.4 GHz Gazell link, which presents as a virtual serial port.

- **8 EEG channels** at 24-bit resolution (ADS1299)
- **3-axis accelerometer** (LIS3DH)
- **250 Hz** sample rate (limited by Gazell bandwidth)
- **Wireless** link (RFduino / Nordic Gazell protocol, 2.4 GHz)
- **No checksum** — relies on USB's built-in error detection
- Max Gazell payload is 31 bytes; dongle adds 2 framing bytes → 33 bytes

### 2.2 InEar Teensy Original

A wired USB protocol for a Teensy 4.1 microcontroller with ADS1299 EEG front-end plus multiple biometric sensors. Designed for in-ear EEG devices.

- **5 EEG channels** at 24-bit resolution
- **9 auxiliary channels**: accelerometer (3), PPG heart sensor (3), temperature, battery, sync
- **1000 Hz** sample rate
- **Fixed 56-byte packets** — every packet carries ALL sensor data
- XOR checksum for error detection

### 2.3 InEar Teensy Optimized

Same hardware as the original, but with a redesigned protocol that sends sensor data only at each sensor's native rate:

- **Variable-length packets** (26-55 bytes) based on content
- **Multi-rate sampling**: EEG at 1000 Hz, accelerometer at 250 Hz, PPG at 100 Hz, temperature/battery at 10 Hz
- **Enumerated packet types** tell the receiver exactly what data is included
- **Sample-and-hold** on the receiver: when slow sensors aren't sent, the receiver uses the last known value
- Same error detection (XOR checksum)

---

## 3. Packet Structure Visualizations

### 3.1 OpenBCI Cyton — 33 Bytes

```
One packet = one snapshot of 8 brain channels

 ┌─────────── 33 bytes total ───────────┐
 │                                      │
 ▼                                      ▼
┌──────┬─────┬───────────────────────────────────────────────────┬────────────┬──────┐
│ 0xA0 │ #42 │  Ch1  │  Ch2  │  Ch3  │  Ch4  │  Ch5  │  Ch6  │  Ch7  │ Ch8 │ Aux  │ 0xC0 │
│ START│ SEQ │ brain │ brain │ brain │ brain │ brain │ brain │ brain │brain│motion│ END  │
│ 1B   │ 1B  │  3B   │  3B   │  3B   │  3B   │  3B   │  3B   │  3B   │ 3B  │  6B  │ 1B   │
└──────┴─────┴───────────────────────────────────────────────────┴────────────┴──────┘

 ┌──────────────────────────────────────────────────────────────────┐
 │  START (1B)  → "Here comes a new packet!" (always 0xA0)         │
 │  SEQ (1B)    → Packet number (0-255), detects if any got lost   │
 │  EEG (24B)   → 8 brain signal readings, very high precision     │
 │  AUX (6B)    → Head motion sensor (X, Y, Z) or timestamps       │
 │  END (1B)    → "Packet is done" (0xC0-0xC6, also tells type)   │
 └──────────────────────────────────────────────────────────────────┘

 ⚠ No checksum! Relies on USB's own error checking.
 📡 Sent over Gazell link → slower baud rate, fewer channels possible.
```

### 3.2 InEar Teensy Original — 56 Bytes

```
One packet = one snapshot of ALL sensors (brain + body + environment)

 ┌──────────────────────────────── 56 bytes total ────────────────────────────────┐
 │                                                                               │
 ▼                                                                               ▼
┌───────┬───────────┬────────┬─────────────────────┬─────────┬──────────────────────────┬─────┬────┬────┬──────────────┬─────┬───────┐
│ START │ TIMESTAMP │ MARKER │     EEG (brain)     │  ACCEL  │      PPG (heart)         │TEMP │BATT│SYNC│ CTR │ CHK │  END  │
│ A5 5A │  4 bytes  │  1 B   │ 5ch × 3B = 15 bytes │  6B     │   3ch × 6B = 18 bytes   │ 2B  │ 2B │ 2B │  1B  │ 1B  │ C0 C0 │
└───────┴───────────┴────────┴─────────────────────┴─────────┴──────────────────────────┴─────┴────┴────┴──────────────┴─────┴───────┘
  2B         4B        1B             15B               6B              18B              2B   2B   2B   1B    1B     2B

 ┌──────────────────────────────────────────────────────────────────────────────┐
 │  START (2B)     → "New packet!" Two magic bytes: 0xA5 0x5A                 │
 │  TIMESTAMP (4B) → When this sample was taken (microseconds)                │
 │  MARKER (1B)    → Experiment event tag ("stimulus shown now")              │
 │  EEG (15B)      → 5 brain channels, 24-bit precision each                 │
 │  ACCEL (6B)     → Head motion: X, Y, Z axes                               │
 │  PPG (18B)      → Heart pulse sensor: Red, Infrared, Green light          │
 │  TEMP (2B)      → Body temperature                                        │
 │  BATTERY (2B)   → Battery voltage remaining                               │
 │  SYNC (2B)      → External trigger signal                                 │
 │  COUNTER (1B)   → Packet counter (0-255)                                   │
 │  CHECKSUM (1B)  → Error detection: XOR of all previous bytes              │
 │  END (2B)       → "Packet complete" Two bytes: 0xC0 0xC0                  │
 └──────────────────────────────────────────────────────────────────────────────┘

 ✅ Has error checking (XOR checksum)
 ⚠ Sends ALL sensor data every packet, even slow-changing ones like temperature
```

### 3.3 InEar Teensy Optimized — 26 to 55 Bytes

```
The key idea: don't send data that hasn't changed!

 Brain signals change fast   → send EVERY packet      (1000 Hz)
 Head motion changes medium  → send every 4th packet   (250 Hz)
 Heart pulse changes slowly  → send every 10th packet  (100 Hz)
 Temperature barely changes  → send every 100th packet  (10 Hz)
```

**MOST COMMON packet (749 out of 1000 per second) — "EEG Only":**

```
 ┌──────── 26 bytes ────────┐
 ▼                          ▼
┌───────┬──────┬─────┬───────────┬────────────────┬─────┬───────┐
│ START │ TYPE │ SEQ │ TIMESTAMP │   EEG (brain)  │ CHK │  END  │
│ A5 5A │ 0x00 │  #  │  4 bytes  │ 5ch × 3B = 15B │ 1B  │ C0 C0 │
└───────┴──────┴─────┴───────────┴────────────────┴─────┴───────┘
  2B      1B    1B      4B              15B          1B     2B
```

**Every 4th packet — adds motion data:**

```
 ┌──────────── 32 bytes ─────────────┐
 ▼                                   ▼
┌───────┬──────┬─────┬───────────┬────────────────┬─────────┬─────┬───────┐
│ START │ TYPE │ SEQ │ TIMESTAMP │   EEG (brain)  │  ACCEL  │ CHK │  END  │
│ A5 5A │ 0x01 │  #  │  4 bytes  │     15 bytes   │ X, Y, Z │ 1B  │ C0 C0 │
└───────┴──────┴─────┴───────────┴────────────────┴─────────┴─────┴───────┘
  2B      1B    1B      4B              15B            6B      1B     2B
```

**Every 20th packet — adds motion + heart pulse:**

```
 ┌───────────────────── 50 bytes ──────────────────────┐
 ▼                                                     ▼
┌───────┬──────┬─────┬───────────┬──────────┬─────────┬──────────────────────┬─────┬───────┐
│ START │ TYPE │ SEQ │ TIMESTAMP │   EEG    │  ACCEL  │    PPG (heart)       │ CHK │  END  │
│ A5 5A │ 0x03 │  #  │  4 bytes  │  15 B    │ X, Y, Z │ Red, IR, Green       │ 1B  │ C0 C0 │
└───────┴──────┴─────┴───────────┴──────────┴─────────┴──────────────────────┴─────┴───────┘
  2B      1B    1B      4B          15B         6B              18B            1B     2B
```

**Once per second — full sync (everything):**

```
 ┌──────────────────────────── 54 bytes ────────────────────────────┐
 ▼                                                                  ▼
┌───────┬──────┬─────┬───────────┬──────────┬─────────┬─────────────────────┬────────────┬─────┬───────┐
│ START │ TYPE │ SEQ │ TIMESTAMP │   EEG    │  ACCEL  │    PPG (heart)      │TEMP + BATT │ CHK │  END  │
│ A5 5A │ 0x06 │  #  │  4 bytes  │  15 B    │ X, Y, Z │ Red, IR, Green      │  2B + 2B   │ 1B  │ C0 C0 │
└───────┴──────┴─────┴───────────┴──────────┴─────────┴─────────────────────┴────────────┴─────┴───────┘
  2B      1B    1B      4B          15B         6B              18B              4B         1B     2B
```

**Packet type reference:**

```
 ┌────────────────────────────────────────────────────────────┐
 │  0x00 = EEG only           (26 bytes)  ← 749× per second │
 │  0x01 = EEG + Motion       (32 bytes)  ← 200× per second │
 │  0x02 = EEG + Heart        (44 bytes)  ←  40× per second │
 │  0x03 = EEG + Motion+Heart (50 bytes)  ←   9× per second │
 │  0x04 = EEG + Health       (30 bytes)  ←   1× per second │
 │  0x05 = EEG + Motion+Health(36 bytes)  ←   ~0× per second│
 │  0x06 = EVERYTHING         (54 bytes)  ←   1× per second │
 │                                                           │
 │  Add 0x10 to any type → also includes experiment marker   │
 └────────────────────────────────────────────────────────────┘
```

---

## 4. Data Content Comparison

These three protocols transport fundamentally **different amounts and types of data**. A direct bandwidth comparison without accounting for this is misleading.

### 4.1 What Each Protocol Actually Carries

| Data Type | OpenBCI Cyton | InEar Original | InEar Optimized |
|-----------|:---:|:---:|:---:|
| **EEG Channels** | 8 | 5 | 5 |
| **EEG Resolution** | 24-bit | 24-bit | 24-bit |
| **EEG Bytes/Sample** | 24 | 15 | 15 |
| **Accelerometer** | 3-axis, 16-bit | 3-axis, 16-bit | 3-axis, 16-bit |
| **PPG (Heart Pulse)** | ❌ None | 3ch × 48-bit (18B!) | 3ch × 48-bit |
| **Temperature** | ❌ None | 16-bit | 16-bit |
| **Battery** | ❌ None | 16-bit | 16-bit |
| **Sync/Trigger** | ❌ None | 16-bit | ❌ Removed |
| **Experiment Marker** | Via footer byte | 1 byte | 1 byte (optional) |
| **Timestamp** | Via aux bytes (optional) | 4 bytes | 4 bytes |

### 4.2 Useful Payload Per Sample

| Protocol | EEG Bytes | Aux Bytes | Total Payload | Overhead | Overhead % |
|----------|:---------:|:---------:|:-------------:|:--------:|:----------:|
| OpenBCI Cyton | 24 | 6 | **30** | 3 (header+seq+footer) | 9% |
| InEar Original | 15 | 30 (accel+PPG+temp+batt+sync) | **45** | 11 (hdr+chk+ftr+marker+counter) | 20% |
| InEar Optimized (EEG-only) | 15 | 0 | **15** | 11 (hdr+type+seq+ts+chk+ftr) | 42% |
| InEar Optimized (avg) | 15 | ~13.5 (weighted) | **~28.5** | ~11 | ~28% |

### 4.3 The PPG Problem

PPG (photoplethysmography / heart pulse) data is the single biggest aux payload:

```
PPG = 3 channels × 6 bytes each = 18 bytes per sample

That's MORE than all 5 EEG channels combined (15 bytes)!
```

In the original protocol, this 18 bytes is sent 1000 times/sec = **18,000 bytes/sec just for PPG**.

The heart pulse signal has meaningful content up to ~25 Hz. Nyquist says we need ≥50 Hz sampling. The optimized protocol sends PPG at 100 Hz (2× Nyquist margin), saving:

```
Original PPG:   18 bytes × 1000 Hz = 18,000 B/s
Optimized PPG:  18 bytes × 100 Hz  =  1,800 B/s
Savings:        16,200 B/s (90% reduction for PPG alone!)
```

### 4.4 If OpenBCI Had To Carry The Same Data

If OpenBCI Cyton needed to send the same sensor suite as InEar at 1000 Hz:

```
Needed per sample:  15 (EEG) + 6 (accel) + 18 (PPG) + 2 (temp) + 2 (batt) + 5 (overhead) = 48 bytes
Needed per second:  48 × 1000 = 48,000 bytes/sec
Wire bit rate:      48,000 × 10 = 480,000 bps
Available (8-N-1):  115,200 / 10 = 11,520 bytes/sec max

Result: PHYSICALLY IMPOSSIBLE at 115200 baud — needs 4.2× more bandwidth than available!
```

This is why direct OpenBCI-vs-InEar bandwidth comparisons are misleading. They solve fundamentally different problems with fundamentally different hardware constraints.

---

## 5. Bandwidth Analysis

### 5.1 Serial 8-N-1 Basics

All three protocols use standard serial (UART) with 8-N-1 encoding:

```
Each byte on the wire:
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│ S │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ P │
│ T │   │   │   │   │   │   │   │   │ S │
│ A │   │   │ 8 data bits   │   │   │ T │
│ R │   │   │   │   │   │   │   │   │ O │
│ T │   │   │   │   │   │   │   │   │ P │
└───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
         10 bits per byte on the wire

So: Max bytes/sec = Baud rate ÷ 10
```

### 5.2 Bandwidth Table

| | OpenBCI Cyton | InEar Original | InEar Optimized |
|---|---:|---:|---:|
| **Baud rate** | 115,200 | 2,000,000 | 2,000,000 |
| **Max bytes/sec** (baud ÷ 10) | 11,520 | 200,000 | 200,000 |
| **Packet size** | 33 B | 56 B | 26-54 B (avg ~28.5) |
| **Sample rate** | 250 Hz | 1,000 Hz | 1,000 Hz |
| **Actual bytes/sec** | 8,250 | 56,000 | ~28,500 |
| **Wire bit rate** | 82,500 bps | 560,000 bps | ~285,000 bps |
| **Utilization** | **71.6%** | **28.0%** | **14.3%** |
| **Headroom** | **28.4%** ⚠️ LOW | **72.0%** ✅ | **85.7%** ✅ |

### 5.3 Headroom Visualization

```
Bandwidth utilization (% of serial link capacity used):

OpenBCI Cyton (115200 baud):
 ████████████████████████████████████░░░░░░░░░░░░░░  71.6% used
 ▲ data ──────────────────────────▲  ▲ headroom ▲
                                      TIGHT!

InEar Original (2 Mbaud):
 ██████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  28.0% used
 ▲ data ──────▲  ▲ ── lots of headroom ──────────▲

InEar Optimized (2 Mbaud):
 ███████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  14.3% used
 ▲ data▲  ▲ ────── massive headroom ─────────────▲
```

### 5.4 Where The Bytes Go (InEar Original)

```
56,000 bytes/sec breakdown:

 EEG (brain signals) ████████████████████████████  15,000 B/s (26.8%)
 PPG (heart pulse)   ████████████████████████████████████  18,000 B/s (32.1%)  ← BIGGEST!
 Accelerometer       ██████████████  6,000 B/s (10.7%)
 Temp + Battery      ████████  4,000 B/s (7.1%)
 Sync                ████  2,000 B/s (3.6%)
 Overhead            ████████████  6,000 B/s (10.7%)
                     ─────────────────────────────
                     Total: 56,000 B/s
```

### 5.5 Where The Bytes Go (InEar Optimized)

```
~28,500 bytes/sec breakdown:

 EEG (every packet)     ████████████████████████████  15,000 B/s (52.6%)
 Accel (every 4th)      ████  1,500 B/s (5.3%)
 PPG (every 10th)       ████  1,800 B/s (6.3%)
 Health (every 100th)   ▌  40 B/s (0.1%)
 Headers/footers        ████████████████████  ~10,160 B/s (35.6%)
                        ─────────────────────────────
                        Total: ~28,500 B/s

 Savings vs original: ~49%
```

---

## 6. Stream Behavior Over Time

### 6.1 Packet Rhythm Comparison

```
Timeline: each block = one packet on the wire

OpenBCI Cyton (250 Hz, steady and uniform):
 ──█───█───█───█───█───█───█───█───█───█───  (4 ms apart)
   33B  33B  33B  33B  33B  33B  33B  33B

InEar Original (1000 Hz, steady):
 ─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─  (1 ms apart)
  56 56 56 56 56 56 56 56 56 56 56 56 56 56  ← same size every time

InEar Optimized (1000 Hz, variable — adapts to content):
 ─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─█─  (1 ms apart)
  26 26 26 32 26 26 26 32 26 50 26 26 26 32  ← size adapts to what needs sending
  ▲        ▲           ▲     ▲
  │        │           │     └─ brain + motion + heart pulse (every 20th)
  │        │           └─ brain + motion (every 4th)
  │        └─ brain + motion (every 4th)
  └─ brain only (most common — small and fast!)
```

### 6.2 Optimized Packet Distribution (per second)

```
In one second (1000 packets), the optimized protocol sends:

 EEG-only (26B):           ███████████████████████████████████████████████████████  749 packets
 EEG+Accel (32B):          ████████████████  200 packets
 EEG+PPG (44B):            ████  40 packets
 EEG+Accel+PPG (50B):      █  9 packets
 EEG+Health (30B):          ▌  ~1 packet
 EEG+Full Sync (54B):       ▌  1 packet
                            ─────────────────
                            Total: 1000 packets

 75% of packets are the small 26-byte EEG-only type!
```

### 6.3 What The Receiver Does With Missing Data

When the optimized protocol sends an EEG-only packet (no accelerometer data), the receiver doesn't show a gap — it **holds the last known value**:

```
Time →  1ms   2ms   3ms   4ms   5ms   6ms   7ms   8ms
EEG:    ✦     ✦     ✦     ✦     ✦     ✦     ✦     ✦    (every packet)
Accel:  ✦     ·     ·     ✦     ·     ·     ·     ✦    (every 4th)
        ↑     ↑     ↑     ↑     ↑     ↑     ↑     ↑
        new   held  held  new   held  held  held  new
        data  value value data  value value value data

✦ = fresh data arrived    · = holding previous value

This is called "sample-and-hold" — perfectly fine for slow signals
because the accelerometer hasn't meaningfully changed in 4ms.
```

---

## 7. Trade-Off Matrix (Original vs Optimized)

| Dimension | InEar Original | InEar Optimized | Winner |
|-----------|:-:|:-:|:-:|
| **Bandwidth** | 56 KB/s | ~28.5 KB/s | ✅ Optimized (49% less) |
| **Latency** | 280 µs wire time | 130-270 µs wire time | ✅ Optimized (most packets smaller) |
| **Integrity** | XOR checksum | XOR checksum | = Tie |
| **Parser Complexity** | Simple: fixed offsets | Complex: 14 types, state machine | ✅ Original (much simpler) |
| **Firmware Complexity** | Simple: fill 56 bytes | Moderate: type selection, variable build | ✅ Original (simpler) |
| **Jitter Robustness** | Excellent: uniform packets | Good: variable sizes cause timing variation | ✅ Original (predictable) |
| **Packet Loss Robustness** | Good: each packet self-contained | Moderate: losing accel/PPG packet means stale data longer | ✅ Original |
| **Headroom for Growth** | 72% free bandwidth | 86% free bandwidth | ✅ Optimized (more room) |
| **Memory Efficiency** | All bytes used | Zero waste | = Tie |
| **Future Extensibility** | Fixed format | Must define new packet type | = Tie |

---

## 8. Complete Optimization Taxonomy

Starting from the **original 56-byte fixed protocol**, here are all 28 identified optimization tactics, organized by category.

### Category 1: Bandwidth Optimization (reduce bytes on the wire)

| ID | Tactic | Description |
|---|---|---|
| **1A** ✅ | Multi-rate aux sampling | Send aux sensors only at their Nyquist rate |
| **1B** | Delta encoding for EEG | Send differences instead of absolute values |
| **1C** | Variable-width integers (VarInt) | Small values use fewer bytes |
| **1E** | PPG to 24-bit (from 48-bit) | PPG sensors are 18-20 bit; 24-bit is plenty |
| **1F** | Smaller header | Reduce or compress header fields |
| **1G** | Multi-sample batching | Pack N samples per packet, amortize overhead |

### Category 2: Latency Optimization (reduce ADC-to-buffer time)

| ID | Tactic | Description |
|---|---|---|
| **2A** | Reduce packet size | Smaller packet = faster on wire (covered by Cat 1) |
| **2B** | Increase baud rate | Teensy 4.1 supports up to 6 Mbaud |
| **2C** | USB flush optimization | Use `Serial.send_now()` for immediate USB transfer |
| **2D** | DMA SPI→UART pipeline | Hardware transfers data without CPU involvement |

### Category 3: Integrity Optimization (better error detection/correction)

| ID | Tactic | Description |
|---|---|---|
| **3A** | CRC-8 or CRC-16 | Better error detection than XOR checksum |
| **3B** | Forward Error Correction (FEC) | Correct errors without retransmission |
| **3C** | Retransmission (ARQ) | Receiver requests re-send of bad packets |
| **3D** | Longer sync pattern | 3-4 byte sync for fewer false positives |
| **3E** | Explicit length field | Self-describing packets, forward-compatible |

### Category 4: Parser/CPU Optimization (reduce host processing cost)

| ID | Tactic | Description |
|---|---|---|
| **4A** | Ring buffer (replace `std::deque`) | Fixed-size circular buffer, no heap allocation |
| **4B** | Zero-copy parsing | Parse directly from buffer, skip memcpy |
| **4C** | SIMD byte conversion | Parallel 24-bit→32-bit conversion |
| **4D** | Remove `std::chrono` from hot path | Avoid syscalls during packet parsing |
| **4E** | Batch `addToBuffer()` calls | Fewer lock acquisitions in Open Ephys |

### Category 5: Firmware CPU Optimization (reduce Teensy processing cost)

| ID | Tactic | Description |
|---|---|---|
| **5A** | Precomputed packet templates | Pre-fill headers, only overwrite data fields |
| **5B** | Packet type lookup table | Replace modulo operations with array lookup |
| **5C** | ISR-based packet assembly | Build packets in interrupt handler |

### Category 6: Robustness Optimization (improve connection reliability)

| ID | Tactic | Description |
|---|---|---|
| **6A** | Heartbeat/watchdog | Detect disconnections immediately |
| **6B** | Connection handshake | Verify firmware version before streaming |
| **6C** | Configurable sample rate | Host can change rate (250/500/1000/2000 Hz) |
| **6D** | Per-channel gain control | Host can set ADS1299 gain per channel |

### Category 7: Data Quality Optimization (improve scientific utility)

| ID | Tactic | Description |
|---|---|---|
| **7A** | Hardware clock synchronization | Sub-microsecond timing via host↔device sync |
| **7B** | Impedance measurement | Report electrode impedance in packets |
| **7C** | Higher sample rate | ADS1299 supports up to 16 kHz |

---

## 9. Optimization Tactic Details

### Category 1 — Bandwidth Optimization

#### 1A — Multi-Rate Auxiliary Sampling ✅ (Already Implemented)

**What**: Instead of sending every sensor at 1000 Hz, send each sensor at a rate matching its actual information content (Nyquist rate).

```
ORIGINAL: Every packet = 56 bytes, all sensors, 1000× per second
┌─────────────────────────────────────────────────────────────────┐
│ ms 1: EEG+Accel+PPG+Temp+Batt+Sync  = 56 bytes                │
│ ms 2: EEG+Accel+PPG+Temp+Batt+Sync  = 56 bytes  (mostly same!)│
│ ms 3: EEG+Accel+PPG+Temp+Batt+Sync  = 56 bytes  (mostly same!)│
│ ms 4: EEG+Accel+PPG+Temp+Batt+Sync  = 56 bytes  (mostly same!)│
└─────────────────────────────────────────────────────────────────┘

OPTIMIZED: Each sensor at its meaningful rate
┌─────────────────────────────────────────────────────────────────┐
│ ms 1: EEG only                       = 26 bytes                │
│ ms 2: EEG only                       = 26 bytes                │
│ ms 3: EEG only                       = 26 bytes                │
│ ms 4: EEG + Accel                    = 32 bytes  (accel @ 250Hz)│
│ ...                                                             │
│ ms 10: EEG + PPG                     = 44 bytes  (PPG @ 100Hz) │
│ ...                                                             │
│ ms 100: EEG + Health                 = 30 bytes  (temp @ 10Hz) │
└─────────────────────────────────────────────────────────────────┘
```

**Physics justification**: Temperature changes over seconds, not milliseconds. Sending temperature 1000× per second is like checking the weather every 3.6 seconds — you'll get the same reading.

| Sensor | Bandwidth of interest | Nyquist minimum | Actual send rate | Over-sampling factor |
|--------|----------------------|-----------------|-----------------|---------------------|
| EEG | 0.1–300 Hz | 600 Hz | 1000 Hz | 1.67× ✅ |
| Accelerometer | 0–100 Hz | 200 Hz | 250 Hz | 1.25× ✅ |
| PPG | 0.5–25 Hz | 50 Hz | 100 Hz | 2× ✅ |
| Temperature | ~0 Hz (DC) | ~1 Hz | 10 Hz | 10× ✅ |

**Trade-off**: Parser complexity rises dramatically — from 1 packet type to 14 (7 base types × 2 with marker flag). Receiver must implement sample-and-hold state management for sensors not present in every packet. If a PPG packet is lost, the held value becomes stale for 10 ms instead of 1 ms.

**Impact**: Saves ~49% bandwidth (56,000 → ~28,500 B/s).

---

#### 1B — Delta Encoding for EEG

**What**: Send the first sample as an absolute 24-bit value, then send only the *difference* between consecutive samples using fewer bits.

**Why it works**: EEG signals at 1 kHz change very little sample-to-sample. A typical delta fits in 8-12 bits vs 24 bits.

```
Absolute:  sample₁ = 123456    → 3 bytes (24-bit)
           sample₂ = 123489    → 3 bytes (24-bit)
           sample₃ = 123501    → 3 bytes (24-bit)

Delta:     sample₁ = 123456    → 3 bytes (24-bit, keyframe)
           Δ₂ = +33            → 1 byte  (8-bit delta)
           Δ₃ = +12            → 1 byte  (8-bit delta)

Savings: 9 bytes → 5 bytes for 3 samples
```

**Critical trade-off**: If you lose ONE packet, every subsequent sample is corrupted until the next keyframe:

```
Sent:      [100000] [+5] [+3] [LOST!] [+7] [+2]
Received:  [100000] [100005] [100008] [???] [100015?] [100017?]
                                       ↑
                              Should be 100018, but we don't
                              know because we missed the -4 delta!
```

**Requires**: Periodic keyframe packets (e.g., every 100th sample sends full absolute values). Receiver must detect loss (via sequence number gap) and discard until next keyframe.

**Potential savings**: 40-60% for EEG data specifically, depending on delta width chosen and signal amplitude.

---

#### 1C — Variable-Width Integers (VarInt)

**What**: Encode values using a variable number of bytes — small values use fewer bytes, large values use more.

```
Standard VarInt encoding (like Protocol Buffers):
  Value          Binary              Encoded     Bytes
  0-127          0xxxxxxx            [0xxxxxxx]   1 byte
  128-16383      00xxxxxx xxxxxxxx   [1xxxxxxx] [0xxxxxxx]  2 bytes
  16384+         ...                 [1xxxxxxx] [1xxxxxxx] [0xxxxxxx]  3 bytes

Example with EEG deltas:
  Delta = +33  → 1 byte  (instead of always 3)
  Delta = +200 → 2 bytes (instead of always 3)
  Delta = +50000 → 3 bytes (same as absolute)
```

**Trade-off**:
- 🔴 Packets have **unpredictable size** — parser can't jump to fixed offsets
- 🔴 Bit-level parsing is expensive — need to inspect each byte's MSB
- 🔴 Combined with delta encoding, two layers of indirection compound error propagation
- 🔴 Cannot pre-validate packet size (size depends on data content)
- 🟢 Average savings of ~30% when combined with delta encoding

**Verdict**: High complexity for moderate savings. Better suited for file formats than real-time streams.

---

#### 1E — PPG to 24-bit (from 48-bit)

**What**: PPG is currently encoded as 48-bit (6 bytes per channel). Most PPG ADCs are 18-20 bit. Using 24-bit (3 bytes) is still more than enough resolution.

```
Current:   3 PPG channels × 6 bytes = 18 bytes/sample
Proposed:  3 PPG channels × 3 bytes =  9 bytes/sample
Savings:   9 bytes per PPG packet

At 100 Hz (optimized): saves 900 B/s
At 1000 Hz (original): saves 9,000 B/s
```

**Why 48-bit exists**: The MAX30105 PPG sensor has an 18-bit ADC with a 4-sample averaging mode, producing values that fit comfortably in 20 bits. The firmware currently packs this into 48 bits using `packInt48BE()`:

```c++
// Current firmware code:
void packInt48BE(uint8_t* buf, int64_t val) {
    buf[0] = (val >> 40) & 0xFF;  // These top 3 bytes
    buf[1] = (val >> 32) & 0xFF;  // are ALWAYS zero
    buf[2] = (val >> 24) & 0xFF;  // for 18-20 bit values
    buf[3] = (val >> 16) & 0xFF;
    buf[4] = (val >> 8) & 0xFF;
    buf[5] = val & 0xFF;
}
// Result: 3 bytes always zero, 3 bytes carry actual data
```

**Trade-off**: Essentially none. 24-bit gives 16.7 million levels of precision for a signal that has maybe 18 bits of useful information. This is a **free win**.

---

#### 1F — Smaller Header/Footer

**What**: Reduce the header and/or footer byte count.

```
Current (InEar both protocols):
  Header:  0xA5 0x5A  (2 bytes) — sync pattern
  Footer:  0xC0 0xC0  (2 bytes) — end marker
  Total:   4 bytes per packet

Options:
  a) Single-byte header:     0xA5 (1 byte) → save 1 byte
  b) No footer (length-based): use explicit length field → save 2 bytes, add 1 byte
  c) Unique start-only:       0xA5 0x5A, no footer → save 2 bytes
```

**Trade-off**:
- 🔴 Shorter sync patterns increase **false sync probability** — random data bytes matching the header
- 🔴 Without footer, parser can't confirm packet end → must rely entirely on length field or checksum
- 🟢 Saves 1-3 bytes per packet (1,000–3,000 B/s)

**False sync math**: With a 2-byte header (0xA5 0x5A), the chance of random data matching is 1 in 65,536. With a 1-byte header (0xA5), it's 1 in 256 — about 256× more likely to see a false sync.

**Verdict**: Marginal savings with meaningful reliability impact. Not recommended unless bandwidth is truly critical.

---

#### 1G — Multi-Sample Batching

**What**: Instead of one sample per packet, pack N samples into a single packet, amortizing the header/footer/checksum overhead across multiple samples.

```
Current (1 sample per packet):
  [HDR(8B) | EEG(15B) | CHK(1B) | FTR(2B)]  = 26 bytes, 11B overhead
  [HDR(8B) | EEG(15B) | CHK(1B) | FTR(2B)]  = 26 bytes, 11B overhead
  [HDR(8B) | EEG(15B) | CHK(1B) | FTR(2B)]  = 26 bytes, 11B overhead
  Total for 3 samples: 78 bytes, 33B overhead (42%)

Batched (3 samples per packet):
  [HDR(8B) | EEG(15B) | EEG(15B) | EEG(15B) | CHK(1B) | FTR(2B)] = 56 bytes, 11B overhead
  Total for 3 samples: 56 bytes, 11B overhead (20%)
  Savings: 22 bytes per 3 samples
```

**Trade-off**:
- 🔴 **Latency increases by N×**: must wait for N samples before sending (at 1000 Hz with N=4, that's 4 ms instead of 1 ms)
- 🔴 **Loss amplification**: losing one packet loses N samples instead of 1
- 🔴 **Batch-boundary edge cases**: markers, aux data scheduling become complex
- 🟢 **Significant overhead reduction**: at N=4, overhead drops from 42% to 17%

**Verdict**: Best for systems where latency tolerance is >5 ms and bandwidth is the primary constraint. Poor fit for real-time neurofeedback.

---

### Category 2 — Latency Optimization

#### 2A — Reduce Packet Size

**What**: This is not a separate tactic — it's the net result of Category 1 bandwidth optimizations. Smaller packets spend less time on the wire.

```
Wire time at 2 Mbaud (8-N-1):
  Original (56 bytes):   56 × 10 / 2,000,000 = 280 µs
  Optimized EEG-only:    26 × 10 / 2,000,000 = 130 µs  (54% faster)
  Optimized full sync:   54 × 10 / 2,000,000 = 270 µs  (~same)

For context: 1 sample period = 1,000 µs (1 ms)
So even the largest packet only uses 27% of the sample period.
```

**Trade-off**: None — it's a consequence of other optimizations.

---

#### 2B — Increase Baud Rate

**What**: The Teensy 4.1 (ARM Cortex-M7, 600 MHz) supports baud rates up to 6 Mbaud on its hardware UART. The current 2 Mbaud could be increased.

```
Baud Rate    Max B/s     Wire time (56B)  Wire time (26B)
2,000,000    200,000     280 µs           130 µs
3,000,000    300,000     187 µs            87 µs
4,000,000    400,000     140 µs            65 µs
6,000,000    600,000      93 µs            43 µs
```

**Trade-off**:
- 🔴 Higher baud rates increase **bit error rate (BER)** on the physical line — shorter bit duration means more susceptibility to noise, ringing, and impedance mismatch
- 🔴 Requires verifying USB-serial converter IC (FTDI, CH340, CP2102) supports the rate — many top out at 2-3 Mbaud
- 🔴 Cable length and quality become critical at >3 Mbaud
- 🟢 Direct linear reduction in wire latency
- 🟢 With current 14.3% utilization at 2 Mbaud, there's no bandwidth need — this is purely a latency play

**Verdict**: Low priority — the current 130 µs wire time is already small compared to the 1 ms sample period and OS scheduling jitter (~1 ms).

---

#### 2C — USB Flush Optimization

**What**: Use `Serial.send_now()` on Teensy to force immediate USB packet transmission instead of waiting for the USB stack's default buffering interval.

```
Without send_now():
  Teensy USB stack collects bytes into a 64-byte USB packet
  Sends when full OR after ~1 ms timeout
  Result: up to 1 ms extra latency (non-deterministic)

With send_now():
  Serial.write(packet, len);
  Serial.send_now();  // Force immediate USB transfer
  Result: USB packet sent on next USB frame (~125 µs for USB 2.0 HS)

Timeline:
  ADC sample → build packet → Serial.write() → [USB buffer wait] → Host receives
                                                  ↑
                              send_now() eliminates this 0-1 ms wait
```

**Trade-off**:
- 🔴 More frequent USB interrupts on Teensy — slight increase in interrupt overhead
- 🔴 Less efficient USB utilization (sending partially-filled USB packets)
- 🟢 Removes up to 1 ms of non-deterministic USB buffering latency
- 🟢 Single line of code to add

**Verdict**: Easy win for latency-sensitive applications (neurofeedback). The firmware currently does NOT call `send_now()`.

---

#### 2D — DMA SPI→UART Pipeline

**What**: Use Direct Memory Access (DMA) hardware to transfer data from the ADS1299 SPI bus directly to the UART transmit buffer without CPU involvement.

```
Current flow (CPU-mediated):
  ADS1299 ──SPI──► [CPU reads bytes] ──► [CPU builds packet] ──► [CPU writes UART]
                     ▲ CPU busy ▲           ▲ CPU busy ▲          ▲ CPU busy ▲

DMA flow (hardware-mediated):
  ADS1299 ──SPI──► [DMA copies to RAM] ──► [CPU builds packet] ──► [DMA writes UART]
                     ▲ HW, CPU free ▲       ▲ CPU busy (less) ▲    ▲ HW, CPU free ▲
```

**Trade-off**:
- 🔴 DMA setup is complex — channel allocation, buffer descriptors, interrupt handlers for completion
- 🔴 Teensy DMA on IMXRT1062 is powerful but poorly documented for chained SPI→UART use
- 🔴 Must handle DMA buffer wrap-around and synchronization
- 🟢 Frees CPU for other tasks (display, button handling, impedance check)
- 🟢 More deterministic timing — CPU jitter from ISR entry/exit removed

**Verdict**: High-effort, high-reward. Best suited if the Teensy needs to perform additional processing (FFT, filtering, impedance measurement) alongside streaming.

---

### Category 3 — Integrity Optimization

#### 3A — CRC-8 Instead of XOR

**What**: Replace the XOR checksum with CRC-8. Same 1 byte, much better error detection.

```
Error detection capability comparison:

                        XOR        CRC-8
Single-bit errors:     100%       100%
2-bit errors:          100%       100%
Odd # of bit errors:   100%       100%
Even # of bit errors:   0% !!     99.6%
Burst errors (≤8 bit): ~50%       100%
Random errors:         ~50%       99.6%
```

**Implementation**:

```c++
// XOR checksum (current):
uint8_t checksum = 0;
for (int i = 0; i < len; i++)
    checksum ^= data[i];

// CRC-8 (proposed):
static const uint8_t crc8_table[256] = { /* precomputed */ };
uint8_t crc = 0;
for (int i = 0; i < len; i++)
    crc = crc8_table[crc ^ data[i]];
```

**Trade-off**: CRC-8 needs a 256-byte lookup table and one table lookup per byte instead of one XOR per byte. At 1000 packets/sec × 56 bytes, that's 56,000 table lookups/sec — completely negligible on a 600 MHz Teensy 4.1 or any modern PC. This is a **free win**.

---

#### 3B — Forward Error Correction (FEC)

**What**: Add redundancy bytes that allow the receiver to *correct* errors without needing retransmission. Examples: Hamming codes, Reed-Solomon codes.

```
Simple Hamming (7,4) example:
  4 data bits + 3 parity bits = 7 bits total
  Can correct any single-bit error automatically
  Overhead: 75% increase in bits

Reed-Solomon for packets:
  26-byte packet + 4 bytes RS parity = 30 bytes
  Can correct up to 2 byte errors anywhere in packet
  Overhead: 15% increase
```

**Trade-off**:
- 🔴 **Massive complexity**: RS encoding/decoding requires Galois field arithmetic
- 🔴 **Bandwidth overhead**: 15-75% depending on correction strength
- 🔴 **Overkill for USB**: USB 2.0 has hardware-level CRC16 on every packet, so the physical link is already very reliable
- 🟢 Useful if a lossy link (e.g. Gazell) were ever added

**Verdict**: Overkill for wired USB. Only makes sense if migrating to Bluetooth, WiFi, or other lossy links.

---

#### 3C — Retransmission (ARQ)

**What**: Receiver sends a negative acknowledgment (NACK) when a bad packet is detected, and the firmware re-sends it.

```
Normal flow:
  Teensy ──► [Pkt 42] ──► Host (OK)
  Teensy ──► [Pkt 43] ──► Host (OK)

Error flow with ARQ:
  Teensy ──► [Pkt 42] ──► Host (OK)
  Teensy ──► [Pkt 43] ──► Host (CRC fail!)
  Host   ──► [NACK 43] ──► Teensy
  Teensy ──► [Pkt 43 retry] ──► Host (OK)
  Teensy ──► [Pkt 44] ──► Host (OK)
```

**Trade-off**:
- 🔴 **Adds latency**: round-trip NACK + resend takes 2-3 ms
- 🔴 **Requires bidirectional protocol**: firmware must listen for commands while streaming
- 🔴 **Teensy must buffer recent packets**: needs RAM for TX ring buffer (N × packet_size)
- 🔴 **Receiver must handle reordering**: packet 44 might arrive before retransmitted 43
- 🟢 **Guaranteed delivery**: no data loss (unless buffer overflow)

**Verdict**: Complex and adds latency. On USB, packet loss rate is near zero, making this unnecessary. Better for unreliable wireless links.

---

#### 3D — Longer Sync Pattern

**What**: Use a 3-4 byte sync pattern instead of the current 2-byte `0xA5 0x5A`.

```
Current: 0xA5 0x5A (2 bytes)
  False sync probability: 1 in 65,536 (2^16)
  At 200,000 bytes/sec: ~3 false syncs per second in random data

Proposed: 0xA5 0x5A 0x96 (3 bytes)
  False sync probability: 1 in 16,777,216 (2^24)
  At 200,000 bytes/sec: ~0.01 false syncs per second

Proposed: 0xA5 0x5A 0x96 0x69 (4 bytes)
  False sync probability: 1 in 4,294,967,296 (2^32)
  Virtually impossible in practice
```

**Trade-off**:
- 🔴 +1-2 bytes per packet (1,000-2,000 B/s overhead)
- 🟢 Dramatically reduces false synchronization risk
- 🟢 Faster re-sync after data corruption (fewer false starts to reject)

**Verdict**: Moderate value. The current 2-byte sync is fine because the checksum provides a second verification layer — a false sync that also passes checksum is extremely unlikely.

---

#### 3E — Explicit Length Field

**What**: Add a 1-byte length field to the packet header so the parser knows exactly how many bytes to expect before reading them.

```
Current (optimized):
  Parser reads TYPE byte → looks up expected size in table → reads that many bytes
  Problem: if a new type is added in firmware but not in plugin, parser is confused

With length field:
  ┌───────┬──────┬──────┬─────┬─────────────────────────┬─────┬───────┐
  │ START │ TYPE │ LEN  │ SEQ │     payload...          │ CHK │  END  │
  │ A5 5A │ 0x01 │  32  │  #  │     (LEN bytes total)   │     │ C0 C0 │
  └───────┴──────┴──────┴─────┴─────────────────────────┴─────┴───────┘
                    ↑
                    NEW: tells parser exact packet size
```

**Trade-off**:
- 🔴 +1 byte per packet (1,000 B/s)
- 🟢 **Forward-compatible**: host can skip unknown packet types gracefully
- 🟢 Parser doesn't need to know every type — reads LEN bytes, skips if unknown
- 🟢 Simplifies parser: no need for lookup table of sizes per type

**Verdict**: Excellent trade-off. 1 byte overhead for forward-compatibility is almost always worth it in evolving protocols.

---

### Category 4 — Parser/CPU Optimization (Host Side)

#### 4A — Ring Buffer Instead of `std::deque`

**What**: The current parser stores incoming bytes in a `std::deque<uint8_t>`. Each `push_back()` and `pop_front()` may allocate or free heap memory.

```
Current (std::deque):
 - Each byte may trigger a heap allocation
 - At 28,500 bytes/sec → up to 28,500 allocations/sec
 - Poor cache locality (memory scattered across heap)

Ring buffer:
 - Fixed 16 KB allocation at startup
 - Zero allocations during operation
 - Excellent cache locality (contiguous memory)
 - Push/pop = just move a pointer

 ┌──────────────────────────────────────┐
 │     Fixed-size circular buffer       │
 │  ┌───┬───┬───┬───┬───┬───┬───┬───┐  │
 │  │   │ D │ A │ T │ A │   │   │   │  │
 │  └───┴───┴───┴───┴───┴───┴───┴───┘  │
 │        ↑ head          ↑ tail        │
 │        (read from)     (write to)    │
 └──────────────────────────────────────┘
```

**Implementation sketch**:

```c++
class RingBuffer {
    uint8_t buffer[16384];  // 16 KB, power of 2 for fast modulo
    size_t head = 0, tail = 0;
public:
    void push(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; i++)
            buffer[(tail++) & 0x3FFF] = data[i];  // & instead of %
    }
    uint8_t operator[](size_t i) const {
        return buffer[(head + i) & 0x3FFF];
    }
    void consume(size_t n) { head += n; }
    size_t available() const { return tail - head; }
};
```

**Trade-off**: Must handle wrap-around when reading contiguous packet data that straddles the buffer boundary. Solution: always ensure buffer is large enough that a single packet never wraps (16 KB >> 55 byte max packet).

This is a **free win** — zero allocation hot path, better cache performance, same semantics.

---

#### 4B — Zero-Copy Parsing

**What**: Instead of copying packet bytes into a temporary `EEGSample` struct, parse values directly from the ring buffer.

```
Current (copy, then parse):
  Ring buffer → memcpy(tempPacket, 56 bytes) → parse tempPacket fields

Zero-copy:
  Ring buffer → directly read fields at buffer[head + offset]
  No intermediate copy needed
```

**Trade-off**:
- 🔴 Must handle ring buffer wrap-around mid-packet (if packet straddles the circular boundary)
- 🔴 Code is slightly harder to read — field access becomes `buffer[(head + OFFSET_EEG + i) & mask]` instead of `packet[OFFSET_EEG + i]`
- 🟢 Saves one memcpy per packet (56 bytes × 1000/sec = 56 KB/s of unnecessary copying eliminated)

**Verdict**: Moderate win. Most impactful when combined with the ring buffer (4A).

---

#### 4C — SIMD Byte Conversion

**What**: The parser converts 24-bit big-endian bytes to 32-bit signed integers for each EEG channel. With SIMD (SSE/AVX on x86), multiple channels can be converted in parallel.

```
Current (sequential, 5 channels):
  for (int ch = 0; ch < 5; ch++) {
      int32_t val = (buf[ch*3] << 16) | (buf[ch*3+1] << 8) | buf[ch*3+2];
      if (val & 0x800000) val |= 0xFF000000;  // sign extend
      samples[ch] = val;
  }
  // 5 iterations, each with shifts, ORs, branch

SIMD (parallel):
  // Load all 15 EEG bytes + 1 extra byte into 128-bit register
  // Shuffle bytes into 4×32-bit positions
  // Shift and sign-extend in parallel
  // Store 4 channels at once, then handle the 5th
```

**Trade-off**:
- 🔴 **Platform-specific**: SSE4.1/AVX2 on x86, NEON on ARM — needs separate code paths
- 🔴 **Only 5 channels** — not enough to justify the complexity (SIMD shines at 16+ operations)
- 🔴 Compiler may already auto-vectorize the loop
- 🟢 Potentially 2-4× faster for the conversion step specifically

**Verdict**: Micro-optimization. The current sequential code takes nanoseconds for 5 channels. Not worth the platform-specific complexity unless porting to a very constrained embedded receiver.

---

#### 4D — Remove `std::chrono` from Hot Path

**What**: The optimized parser currently calls `std::chrono::steady_clock::now()` for bandwidth monitoring on every `updateBuffer()` call. These system clock reads involve kernel syscalls on some platforms.

```
Current hot path:
  void updateBuffer() {
      auto now = std::chrono::steady_clock::now();  // ← SYSCALL on Windows
      // ... read serial bytes, parse packets ...
      auto elapsed = now - lastBandwidthCheck;
      if (elapsed > 5s) { reportBandwidth(); }
  }

Proposed:
  void updateBuffer() {
      // ... read serial bytes, parse packets ...
      bandwidthByteCounter += bytesRead;
      bandwidthPacketCounter++;
      // Check every N calls instead of using clock:
      if (bandwidthPacketCounter % 5000 == 0) { reportBandwidth(); }
  }
```

**Trade-off**: Bandwidth reporting becomes approximate (based on packet count rather than wall-clock time). At 1000 Hz, 5000 packets ≈ 5 seconds — close enough for monitoring.

**Implementation effort**: ~10 minutes. Replace 3-4 lines. This is the **easiest free win** in the entire taxonomy.

---

#### 4E — Batch `addToBuffer()` Calls

**What**: The Open Ephys DataThread API provides `addToBuffer()` to push samples into the signal chain. Currently called once per parsed packet. Batching N samples reduces lock acquisition overhead.

```
Current (1 call per packet):
  for each parsed packet:
      addToBuffer(1 sample)    // acquires lock, copies, releases lock
  // 1000 lock cycles per second

Batched (e.g., every 10 packets):
  float batch[10][5];  // 10 samples × 5 channels
  for each 10 parsed packets:
      fill batch array
  addToBuffer(batch, 10 samples)  // 1 lock cycle for 10 samples
  // 100 lock cycles per second
```

**Trade-off**:
- 🔴 Adds up to 10 ms of batching latency (waiting to fill the batch)
- 🔴 Need to handle partial batches (flush on timeout)
- 🟢 10× fewer lock acquisitions → less contention with the audio/processing thread

**Verdict**: Good for throughput, bad for latency. Can be made configurable.

---

### Category 5 — Firmware CPU Optimization (Teensy Side)

#### 5A — Precomputed Packet Templates

**What**: Instead of filling every byte of the packet buffer in the ISR, pre-fill the static fields (header, footer, type) at startup and only overwrite the changing data fields.

```
Current (every ISR call):
  buf[0] = 0xA5;       // header byte 1 — SAME EVERY TIME
  buf[1] = 0x5A;       // header byte 2 — SAME EVERY TIME
  buf[2] = packetType; // changes every packet
  buf[3] = seqNum++;   // changes every packet
  // ... fill timestamp, EEG, etc ...
  buf[len-2] = 0xC0;   // footer byte 1 — SAME EVERY TIME
  buf[len-1] = 0xC0;   // footer byte 2 — SAME EVERY TIME

Precomputed (at startup):
  // Fill 7 template buffers (one per packet type) with static fields
  for (int type = 0; type < 7; type++) {
      templates[type][0] = 0xA5;
      templates[type][1] = 0x5A;
      templates[type][2] = type;
      int len = getPacketSize(type);
      templates[type][len-2] = 0xC0;
      templates[type][len-1] = 0xC0;
  }

  // In ISR: just memcpy template, then overwrite dynamic fields
  memcpy(buf, templates[type], len);
  buf[3] = seqNum++;
  // ... overwrite EEG, timestamp, aux data ...
```

**Trade-off**: Uses 7 × 55 = 385 bytes of RAM for templates. Teensy 4.1 has 1 MB RAM, so negligible.

**Impact**: Saves 4-6 byte writes per ISR call. Minor but essentially free.

---

#### 5B — Packet Type Lookup Table

**What**: Replace the cascade of modulo operations in `getPacketType()` with a single array lookup.

```
Current firmware code:
  uint8_t getPacketType(uint32_t sampleNum) {
      if (sampleNum % 1000 == 0) return 0x06;  // full sync
      if (sampleNum % 100 == 0)  return 0x04;  // health
      if (sampleNum % 20 == 0)   return 0x03;  // accel + PPG
      if (sampleNum % 10 == 0)   return 0x02;  // PPG only
      if (sampleNum % 4 == 0)    return 0x01;  // accel only
      return 0x00;                              // EEG only
  }
  // 5 modulo operations + 5 branches = expensive in ISR

Lookup table:
  // Precompute for one full cycle (LCM of 4, 10, 20, 100, 1000 = 1000)
  static uint8_t typeTable[1000];
  void setupTypeTable() {
      for (int i = 0; i < 1000; i++)
          typeTable[i] = getPacketType(i);  // compute once
  }

  // In ISR: single array lookup
  uint8_t type = typeTable[sampleNum % 1000];
  // 1 modulo + 1 array read = much cheaper
```

**Trade-off**: Uses 1000 bytes of RAM. Teensy 4.1 has 1 MB. This is a **free win**.

---

#### 5C — ISR-Based Packet Assembly

**What**: Build the entire packet inside the IntervalTimer ISR (interrupt service routine) rather than the current approach of flagging `dataReady` and building in the main loop.

```
Current firmware flow:
  ISR (1 kHz):
    Read ADS1299 via SPI
    Set dataReady = true
    
  loop():
    if (dataReady) {
        buildPacket();        // Variable execution time
        Serial.write(packet); // May block
        dataReady = false;
    }

ISR-based assembly:
  ISR (1 kHz):
    Read ADS1299 via SPI
    buildPacket();            // Deterministic in ISR
    Serial.write(packet);    // Non-blocking on Teensy (buffered)
```

**Trade-off**:
- 🔴 **Longer ISR**: if packet building takes too long, next timer interrupt could be missed
- 🔴 **ISR overrun risk**: Serial.write() must be non-blocking (it is on Teensy's buffered UART)
- 🟢 **More deterministic timing**: no main-loop scheduling jitter
- 🟢 **Lower latency**: packet sent immediately after ADC read, no loop() delay

**Current status**: The existing firmware already uses `Serial.write()` in the ISR context (via IntervalTimer callback). The distinction is whether `buildPacket()` also runs there — in the current firmware, it does. So this is **already implemented**.

---

### Category 6 — Robustness Optimization

#### 6A — Heartbeat / Watchdog

**What**: Firmware sends a periodic non-data packet ("I'm alive") at a low rate (e.g., 1 Hz). If the host doesn't receive a heartbeat for N seconds, it knows the connection is lost.

```
Current behavior:
  If Teensy hangs or cable disconnects:
    Host sees: no new packets arriving
    Problem: Host doesn't know if it's a brief glitch or a hard disconnect
    Result: Sits there waiting indefinitely

With heartbeat:
  Teensy sends: [HB] every 1 second (when streaming or idle)
  Host monitors: if no [HB] for 3 seconds → declare connection lost

  ┌──────┐                              ┌────────┐
  │ Host │                              │ Teensy │
  └──┬───┘                              └───┬────┘
     │  ◄─── [Data] [Data] [HB] [Data] ──  │  Normal operation
     │  ◄─── [Data] [Data] [HB] [Data] ──  │  Normal operation
     │                                      │
     │  ◄─── [Data] [Data]                  │  Cable yanked!
     │       ...3 seconds, no heartbeat...  │
     │  ⚠ CONNECTION LOST ⚠                │
     │  → Show error to user               │
     │  → Clean up resources               │
     └──────────────────────────────────────┘
```

**Trade-off**:
- 🔴 +1 small packet/sec (~10 bytes) — negligible bandwidth
- 🔴 Must define a non-data packet type (easy with the optimized protocol's type system)
- 🟢 Fast disconnect detection
- 🟢 Can include device health info in heartbeat (CPU temperature, buffer utilization)

**Verdict**: Excellent addition. Very low cost for significant robustness improvement.

---

#### 6B — Connection Handshake

**What**: Instead of "open port and hope data appears", implement a proper startup sequence:

```
Current (fragile):
 Host opens port → Teensy starts blasting data → hope for the best

Proposed (robust):
 ┌──────┐                              ┌────────┐
 │ Host │                              │ Teensy │
 └──┬───┘                              └───┬────┘
    │                                      │
    │  ──── "HELLO" ─────────────────────► │  Host says hi
    │                                      │
    │  ◄─── Device Info ──────────────────  │  Teensy replies with:
    │       (firmware version,             │   - version
    │        channel count,                │   - sample rate
    │        sample rate,                  │   - channel config
    │        protocol type)                │
    │                                      │
    │  ──── "START" ─────────────────────► │  Host says go
    │                                      │
    │  ◄─── Data packets... ──────────────  │  Streaming begins
    │  ◄─── Data packets... ──────────────  │
    │  ◄─── Data packets... ──────────────  │
    │                                      │
    │  ──── "STOP" ──────────────────────► │  Host says stop
    │                                      │
    │  ◄─── "OK" ─────────────────────────  │  Teensy confirms
    └──────────────────────────────────────┘
```

**Benefits**:
- Auto-detect firmware version, channel count, sample rate
- Prevents protocol mismatch (e.g., loading original plugin with optimized firmware)
- Clean startup (no partial packet fragments from port-open transient)
- Clean shutdown (flush buffers, confirm stop)

**Trade-off**: Adds ~100 ms to connection startup (one round-trip). Requires bidirectional protocol support on firmware side.

---

#### 6C — Configurable Sample Rate

**What**: Allow the host to command the Teensy to change its sample rate at runtime via a serial command.

```
Host sends:  "RATE 500\n"
Teensy:      Reconfigures ADS1299 register, adjusts IntervalTimer
Teensy sends: "OK RATE 500\n"
Host:         Adjusts buffer sizes, display timebase

Supported rates (ADS1299 data sheet):
  250 Hz  — low power, extended battery life
  500 Hz  — balanced (sufficient for most EEG)
  1000 Hz — current default
  2000 Hz — high-frequency analysis
  4000 Hz — EMG, fast neural events
  8000 Hz — very specialized research
  16000 Hz — ADS1299 maximum (extreme bandwidth)
```

**Trade-off**:
- 🔴 Requires command protocol (bidirectional communication)
- 🔴 Must handle rate-change transient (brief data gap during ADS1299 reconfiguration)
- 🔴 Plugin must dynamically adjust buffer sizes and DSP parameters
- 🟢 Adaptable to use case — no need to recompile firmware for different experiments
- 🟢 Can reduce power consumption for battery-powered applications

**Verdict**: High value, moderate effort. Pairs naturally with the handshake protocol (6B).

---

#### 6D — Per-Channel Gain Control

**What**: Allow the host to set the ADS1299 programmable gain amplifier (PGA) per channel at runtime.

```
ADS1299 supported gains:
  1×  — ±187.5 mV range (for EMG, ECG)
  2×  — ±93.75 mV range
  4×  — ±46.875 mV range
  6×  — ±31.25 mV range
  8×  — ±23.4375 mV range
  12× — ±15.625 mV range
  24× — ±7.8125 mV range (default, for EEG)

Host sends:  "GAIN 1 24\n"  (channel 1, gain 24×)
Teensy:      Writes ADS1299 CHnSET register
Teensy sends: "OK GAIN 1 24\n"

Use case: In-ear EEG might want channels 1-4 at 24× (EEG) and channel 5
at 1× (EMG for jaw clench detection).
```

**Trade-off**:
- 🔴 Same as 6C — needs command protocol
- 🔴 Gain changes cause a brief transient artifact in the EEG signal
- 🔴 Plugin must track gain per channel for correct µV scaling
- 🟢 Optimize dynamic range per channel for different electrode placements

---

### Category 7 — Data Quality Optimization

#### 7A — Hardware Clock Synchronization

**What**: Synchronize the Teensy's hardware clock with the host PC clock to achieve sub-millisecond timing accuracy for EEG timestamps.

```
Current: Timestamps are Teensy's micros() counter
  Problem: Teensy's crystal drifts ~50 ppm from PC clock
  After 1 hour: 50 ppm × 3600 sec = 180 ms of drift!

Proposed: Periodic time sync exchange
  ┌──────┐                              ┌────────┐
  │ Host │                              │ Teensy │
  └──┬───┘                              └───┬────┘
     │  ──── T₁ (host time) ──────────────► │
     │                                      │ Records T₂ (device time)
     │  ◄─── T₂, T₃ (device times) ────────│
     │  Records T₄ (host time)              │
     │                                      │
     │  Offset = ((T₂ - T₁) + (T₃ - T₄))/2│
     │  Round-trip = (T₄ - T₁) - (T₃ - T₂) │
     └──────────────────────────────────────┘

  Every 10 seconds: recalculate offset
  After correction: timestamps accurate to ~100 µs
```

**Trade-off**:
- 🔴 Requires bidirectional protocol and sync packets (2-4 extra bytes per sync)
- 🔴 Math for drift estimation and correction is moderately complex
- 🟢 Critical for multi-device synchronization (two EEG devices on same subject)
- 🟢 Enables correlation with external event systems (eye trackers, stimulus computers)

---

#### 7B — Impedance Measurement

**What**: Use the ADS1299's built-in impedance measurement mode to report electrode-skin impedance, helping users verify electrode contact quality.

```
ADS1299 impedance measurement:
  1. Inject small AC current through electrode (6 nA or 24 nA)
  2. Measure resulting voltage
  3. Calculate impedance: Z = V / I

  Good contact:   < 10 kΩ   ✅ (gel electrodes, well-placed)
  Acceptable:     10-50 kΩ  ⚠ (dry electrodes, may work)
  Poor contact:   > 50 kΩ   ❌ (electrode lifted, needs adjustment)
  Not connected:  > 1 MΩ    ❌ (no electrode)

Report in packet:
  Could use health packet (Type 0x04/0x05/0x06) to include impedance values
  5 channels × 2 bytes = 10 extra bytes in health packets (sent at 10 Hz)
```

**Trade-off**:
- 🔴 Impedance measurement requires briefly switching ADS1299 to test mode — causes a gap in EEG data
- 🔴 Must carefully schedule measurements to minimize data disruption
- 🔴 Requires firmware mode switching and host-side display
- 🟢 Essential for clinical and research quality assurance
- 🟢 Can detect bad electrodes before experiment begins

**Verdict**: High value for research applications. Usually done during setup (before recording starts), not during live streaming.

---

#### 7C — Higher Sample Rate

**What**: Increase the ADS1299 sample rate beyond the current 1000 Hz for specialized applications.

```
ADS1299 sample rates and bandwidth requirements:

Rate      Bytes/sec (opt)    Utilization (2Mbaud)    Use case
250 Hz    ~7,125 B/s         3.6%                    Standard clinical EEG
500 Hz    ~14,250 B/s        7.1%                    Most research EEG
1000 Hz   ~28,500 B/s        14.3% ← CURRENT         High-frequency EEG
2000 Hz   ~57,000 B/s        28.5%                   Fast neural events
4000 Hz   ~114,000 B/s       57.0%                   EMG, specialized
8000 Hz   ~228,000 B/s       114% ⚠ EXCEEDS 2Mbaud!  Need higher baud
16000 Hz  ~456,000 B/s       228% ⚠ NOT POSSIBLE     Need 6 Mbaud+
```

**Trade-off**:
- 🔴 Linear bandwidth increase — at 4000 Hz, bandwidth utilization reaches 57%
- 🔴 Above 4000 Hz, the 2 Mbaud link becomes the bottleneck
- 🔴 Higher CPU load on both Teensy (ISR at 8+ kHz) and host (8000+ samples/sec to parse)
- 🔴 ADC noise increases at higher rates (less internal averaging)
- 🟢 Better temporal resolution for high-frequency neural oscillations (gamma band >80 Hz)
- 🟢 Enables EMG applications (muscle signals have meaningful content up to 500 Hz)

**Verdict**: Valuable when paired with configurable sample rate (6C). The current protocol design supports up to ~4000 Hz within the 2 Mbaud link budget.

---

## 10. Full Trade-Off Matrix (All 28 Tactics)

Legend:
- ▼ = reduces / improves this metric (good or bad depending on context)
- ▲ = increases / worsens this metric
- = = negligible change

| Tactic | Bandwidth | Latency | Integrity | Parser CPU | FW CPU | Complexity | Robustness |
|--------|:---------:|:-------:|:---------:|:----------:|:------:|:----------:|:----------:|
| **1A** Multi-rate ✅ | ▼▼ 49% saved | ▼ smaller pkts | = same checksum | ▲▲ state machine | ▲ modulo logic | ▲▲ 14 pkt types | ▼ stale aux risk |
| **1B** Delta encoding | ▼▼▼ 60% EEG saved | ▼ smaller pkts | ▼▼▼ error propagates | ▲▲ accumulator | ▲ delta math | ▲▲▲ keyframes | ▼▼▼ lost pkt corrupts stream |
| **1C** VarInt | ▼▼ ~30% saved | ▼ smaller avg | ▼ no pre-validation | ▲▲▲ bit parsing | ▲ encoding | ▲▲▲ no fixed offsets | ▼▼ unpredictable size |
| **1E** PPG 24-bit | ▼ 9B per PPG pkt | ▼ 9B less | = same coverage | ▼ reuse 24-bit parser | ▼ simpler pack | ▼ fewer widths | = no change |
| **1F** Smaller header | ▼ 2-4B/pkt | ▼ slightly faster | ▼ less resync info | = trivial | ▼ fewer bytes | ▲ tighter packing | ▼ less recovery info |
| **1G** Multi-sample batch | ▼▼ amortize overhead | ▲▲ N×1ms latency | ▲ lose N per bad pkt | ▲ unpack N | ▲ buffer N | ▲▲ batch edge cases | ▼▼ more loss per pkt |
| **2A** Reduce pkt size | ▼ (via Cat 1) | ▼ fewer bits | = same | = same | = same | = same | = same |
| **2B** Higher baud | = same bytes | ▼ faster wire | ▼ higher BER | = no change | = config only | = config only | ▼ more bit errors |
| **2C** USB send_now() | = same bytes | ▼ ~0.5ms saved | = no change | = no change | ▲ more USB IRQs | ▲ ISR timing | = no change |
| **2D** DMA SPI→UART | = same bytes | ▼▼ no CPU copy | = no change | = no change | ▼▼ CPU freed | ▲▲ DMA setup | ▲ HW transfer |
| **3A** CRC-8/16 | = same (CRC-8) | = no change | ▼▼ 99.6% vs 50% | ▲ table lookup | ▲ table lookup | ▲ 256B table | ▲ fewer undetected |
| **3B** FEC (Hamming/RS) | ▲▲ 20-50% overhead | ▲ encode/decode | ▼▼▼ can correct errors | ▲▲▲ decode algo | ▲▲▲ encode algo | ▲▲▲ overkill for USB | ▲▲ errors corrected |
| **3C** Retransmission | ▲ NACK + retransmits | ▲▲ round-trip delay | ▼▼ guaranteed delivery | ▲▲ reorder buffer | ▲▲ TX buffer | ▲▲▲ bidirectional | ▲▲▲ no data loss |
| **3D** Longer sync | ▲ +1-2B/pkt | = negligible | ▲ lower false sync | = trivial | = trivial | = trivial | ▲ reliable framing |
| **3E** Length field | ▲ +1B/pkt | = negligible | ▲ validate early | ▼ no type lookup | ▲ one more byte | ▼ forward-compatible | ▲ new types safe |
| **4A** Ring buffer | = no wire change | ▼ less alloc jitter | = no change | ▼▼ no heap alloc | = no FW change | ▲ wraparound code | ▲ deterministic |
| **4B** Zero-copy parse | = no wire change | ▼ less memcpy | = no change | ▼ no pkt copy | = no FW change | ▲ wraparound parse | = no change |
| **4C** SIMD conversion | = no wire change | ▼ faster convert | = no change | ▼ parallel ops | = no FW change | ▲▲ platform-specific | = no change |
| **4D** Remove chrono | = no wire change | ▼ fewer syscalls | = no change | ▼ no syscall/pkt | = no FW change | ▼ simpler code | ▲ deterministic |
| **4E** Batch addToBuffer | = no wire change | ▲ batch delay | = no change | ▼ fewer locks | = no FW change | ▲ batch code | = no change |
| **5A** Precomputed templates | = no wire change | = no change | = no change | = no change | ▼ fewer writes | ▼ cleaner FW | = no change |
| **5B** Packet type LUT | = no wire change | = no change | = no change | = no change | ▼ 1 lookup vs 5 mod | ▼ simpler FW | = no change |
| **5C** ISR-based assembly | = no wire change | ▼ no loop delay | = no change | = no change | ▲ longer ISR | ▲ ISR overrun risk | ▲ more deterministic |
| **6A** Heartbeat/watchdog | ▲ ~1 pkt/sec | = no change | = no change | ▲ timeout logic | ▲ HB generation | ▲ state machine | ▲▲ detect disconnect |
| **6B** Handshake | ▲ handshake bytes | ▲ ~100ms startup | ▲ version verify | ▲ handshake FSM | ▲ command parser | ▲▲ bidirectional | ▲▲ version mismatch |
| **6C** Config sample rate | = steady-state | = no change | = no change | ▲ handle changes | ▲ timer reconfig | ▲▲ command protocol | ▲ adaptable |
| **6D** Per-ch gain control | = steady-state | = no change | = no change | ▲ track gain | ▲ ADS1299 writes | ▲▲ command protocol | ▲ adapt to signal |
| **7A** Clock sync | ▲ 2-4B/pkt | = no change | = no change | ▲ drift math | ▲ sync respond | ▲▲ sync protocol | ▲ drift detection |
| **7B** Impedance | ▲ bytes in sync pkt | = no change | = no change | ▲ impedance display | ▲▲ ADS1299 mode | ▲▲ mode switching | ▲▲ bad electrode detect |
| **7C** Higher sample rate | ▲▲ linear with rate | ▼ temporal resolution | = same per-pkt | ▲ more samples/sec | ▲ tighter timing | ▲ verify headroom | = same per-pkt |

---

## 11. Recommended "Free Wins"

These tactics improve at least one metric without meaningfully hurting any other:

| Priority | Tactic | Effort | Benefit |
|:--------:|--------|:------:|---------|
| 🥇 | **4D** Remove `std::chrono` from hot path | 10 min | Eliminates 1-3 syscalls per packet from parsing loop |
| 🥇 | **4A** Ring buffer instead of `std::deque` | 1 hour | Eliminates ~28,500 heap allocs/sec, better cache performance |
| 🥇 | **1E** PPG to 24-bit instead of 48-bit | 30 min | Saves 9 bytes per PPG packet, no meaningful resolution loss |
| 🥈 | **3A** CRC-8 instead of XOR | 30 min | Same 1-byte cost, catches 99.6% of burst errors (vs 50%) |
| 🥈 | **5B** Packet type LUT on firmware | 15 min | Replace 5 modulo + branch operations with 1 array lookup |
| 🥈 | **5A** Precomputed packet templates | 20 min | Pre-fill headers at startup, saves byte writes per sample |
| 🥉 | **4E** Batch addToBuffer calls | 30 min | Reduce lock contention in Open Ephys data pipeline |
| 🥉 | **3E** Explicit length field | 30 min | +1 byte cost, but makes protocol self-describing and forward-compatible |

---

## 12. Why OpenBCI Comparison Is Not Apples-to-Apples

The three protocols solve fundamentally different problems:

| | OpenBCI Cyton | InEar Teensy |
|---|---|---|
| **Link** | Wireless (RFduino Gazell, 2.4 GHz) | Wired (USB) |
| **Bottleneck** | Gazell BW (31 bytes/packet max) | Effectively unlimited |
| **EEG channels** | 8 | 5 |
| **Sensor suite** | EEG + accelerometer only | EEG + accel + PPG + temp + battery + sync |
| **Primary use** | General-purpose research EEG | Specialized in-ear wearable |
| **Baud rate** | 115,200 (Gazell-limited) | 2,000,000 (USB-capable) |
| **Sample rate** | 250 Hz (Gazell-limited) | 1,000 Hz |

**OpenBCI's 71.6% utilization is not "worse"** — it's at the physical limit of its Gazell link. Can't go faster without new hardware. InEar has headroom because USB is much faster than 2.4 GHz Gazell.

**If OpenBCI had to carry InEar's data**, it would need >4× its available bandwidth — physically impossible without hardware changes.

---

## 13. Appendix: Serial 8-N-1 Bandwidth Math

A common mistake in serial bandwidth calculations is using `baud_rate / 8` instead of `baud_rate / 10`. Here's why:

### What "8-N-1" Means

```
8 = 8 data bits per character
N = No parity bit
1 = 1 stop bit

Total bits per byte on the wire:
  1 (start bit) + 8 (data bits) + 0 (no parity) + 1 (stop bit) = 10 bits
```

### The Math

```
Baud rate = bits per second on the physical wire
Bytes per second = Baud rate ÷ 10 (NOT ÷ 8!)

Examples:
  115,200 baud ÷ 10 = 11,520 bytes/sec max
  2,000,000 baud ÷ 10 = 200,000 bytes/sec max

Common mistake:
  2,000,000 ÷ 8 = 250,000 ← WRONG (ignores start/stop bits)
  2,000,000 ÷ 10 = 200,000 ← CORRECT
```

### Utilization Calculations

```
OpenBCI Cyton:
  Data rate = 33 bytes × 250 Hz = 8,250 bytes/sec
  Max throughput = 115,200 / 10 = 11,520 bytes/sec
  Utilization = 8,250 / 11,520 = 71.6%

InEar Original:
  Data rate = 56 bytes × 1,000 Hz = 56,000 bytes/sec
  Max throughput = 2,000,000 / 10 = 200,000 bytes/sec
  Utilization = 56,000 / 200,000 = 28.0%

InEar Optimized:
  Data rate ≈ 28,500 bytes/sec (measured average)
  Max throughput = 2,000,000 / 10 = 200,000 bytes/sec
  Utilization = 28,500 / 200,000 = 14.3%
```

---

*Generated from analysis of the open-ephys-eeg-tools repository — OpenBCI Cyton, InEar Teensy Source, and InEar Teensy Optimized plugins.*
