# InEar Teensy Optimized Build & Deploy Guide

Complete instructions for building, deploying, and running the InEar Teensy Optimized system on Windows, Linux, and macOS.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Teensy Firmware](#2-teensy-firmware)
3. [Open Ephys Plugin](#3-open-ephys-plugin)
4. [Python Test Script](#4-python-test-script)
5. [Running the System](#5-running-the-system)
6. [Complete Build Scripts](#6-complete-build-scripts)
7. [Quick Reference](#7-quick-reference)

---

## 1. Prerequisites

### Arduino CLI (for Teensy Firmware)

| Platform | Installation Command |
|----------|---------------------|
| **Windows** | `winget install ArduinoSA.ArduinoCLI` or download from [arduino.cc](https://arduino.cc/en/software) |
| **Linux** | `curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \| sh` |
| **macOS** | `brew install arduino-cli` |

After installing Arduino CLI, install Teensy board support:

```bash
arduino-cli core update-index
arduino-cli core install teensy:avr
```

### C++ Build Tools (for Plugin)

| Platform | Installation |
|----------|--------------|
| **Windows** | Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++" workload |
| **Linux** | `sudo apt install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype6-dev libasound2-dev` |
| **macOS** | `xcode-select --install && brew install cmake` |

### Python (for Test Script)

Python 3.8+ with `pyserial` package.

---

## 2. Teensy Firmware

The optimized firmware runs on Teensy 4.1 and streams **variable-length packets** over USB using the optimized protocol.

### 2.1 Find Your Serial Port

| Platform | Command |
|----------|---------|
| **Windows** | `Get-WmiObject Win32_SerialPort \| Select-Object DeviceID, Description` |
| **Linux** | `ls /dev/ttyACM* /dev/ttyUSB*` |
| **macOS** | `ls /dev/cu.usbmodem*` |

### 2.2 Compile Firmware

Compiles the sketch without uploading - useful to check for errors.

| Platform | Command |
|----------|---------|
| **Windows** | `& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn teensy:avr:teensy41 "teensy_firmwares\inear_teensy_firmware_optimized\inear_teensy_firmware_optimized.ino"` |
| **Linux/macOS** | `arduino-cli compile --fqbn teensy:avr:teensy41 ./teensy_firmwares/inear_teensy_firmware_optimized/inear_teensy_firmware_optimized.ino` |

**Flags explained:**
- `compile` - Compile without uploading
- `--fqbn teensy:avr:teensy41` - Fully Qualified Board Name for Teensy 4.1

### 2.3 Upload to Teensy

Compiles (if needed) and uploads to the connected Teensy.

| Platform | Command |
|----------|---------|
| **Windows** | `& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" upload -p COM5 --fqbn teensy:avr:teensy41 "teensy_firmwares\inear_teensy_firmware_optimized\inear_teensy_firmware_optimized.ino"` |
| **Linux** | `arduino-cli upload -p /dev/ttyACM0 --fqbn teensy:avr:teensy41 ./teensy_firmwares/inear_teensy_firmware_optimized/inear_teensy_firmware_optimized.ino` |
| **macOS** | `arduino-cli upload -p /dev/cu.usbmodem* --fqbn teensy:avr:teensy41 ./teensy_firmwares/inear_teensy_firmware_optimized/inear_teensy_firmware_optimized.ino` |

**Flags explained:**
- `upload` - Compile and upload to board
- `-p COM5` - Serial port (adjust for your system)

---

## 3. Open Ephys Plugin

The plugin reads variable-length packets from the Teensy and provides them to Open Ephys for visualization and recording.

### 3.1 Configure CMake (First Time Only)

Creates build system files in the `Build` directory.

| Platform | Command |
|----------|---------|
| **Windows** | `cd OEPlugins\inear-teensy-source-optimized; mkdir Build; cd Build; cmake -G "Visual Studio 17 2022" -A x64 ..` |
| **Linux** | `cd OEPlugins/inear-teensy-source-optimized && mkdir -p Build && cd Build && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..` |
| **macOS** | `cd OEPlugins/inear-teensy-source-optimized && mkdir -p Build && cd Build && cmake -G "Xcode" ..` |

**Flags explained:**
- `-G "Visual Studio 17 2022"` - Generator for VS2022 (Windows)
- `-G "Unix Makefiles"` - Generator for Make (Linux)
- `-G "Xcode"` - Generator for Xcode (macOS)
- `-A x64` - 64-bit architecture (Windows only)
- `-DCMAKE_BUILD_TYPE=Release` - Optimized build (Linux only)
- `..` - Path to CMakeLists.txt in parent directory

### 3.2 Build Plugin

Compiles the plugin into a shared library.

| Platform | Command |
|----------|---------|
| **All** | `cmake --build . --config Release` |
| **Linux (alt)** | `make -j$(nproc)` |
| **macOS (alt)** | `xcodebuild -configuration Release` |

**Flags explained:**
- `--build .` - Build in current directory
- `--config Release` - Optimized release build
- `-j$(nproc)` - Parallel build using all CPU cores

### 3.3 Deploy Plugin

Copy the built plugin to Open Ephys plugins folder.

| Platform | Source File | Destination |
|----------|-------------|-------------|
| **Windows** | `Build\Release\inear-teensy-source-optimized.dll` | `<Open Ephys>\plugins\` |
| **Linux** | `Build/libinear-teensy-source-optimized.so` | `~/.local/lib/open-ephys/plugins/` |
| **macOS** | `Build/Release/inear-teensy-source-optimized.bundle` | `~/Library/Application Support/open-ephys/plugins/` |

**Commands:**

```powershell
# Windows (PowerShell)
Copy-Item -Force "Build\Release\inear-teensy-source-optimized.dll" "C:\path\to\plugin-GUI\Build\Release\Plugins\"
```

```bash
# Linux
mkdir -p ~/.local/lib/open-ephys/plugins
cp Build/libinear-teensy-source-optimized.so ~/.local/lib/open-ephys/plugins/

# macOS
cp -r Build/Release/inear-teensy-source-optimized.bundle ~/Library/Application\ Support/open-ephys/plugins/
```

---

## 4. Python Test Script

A Python script can be used to verify the Teensy is sending valid optimized packets.

### 4.1 Install Dependencies

```bash
pip install pyserial
```

### 4.2 Test Script

```python
#!/usr/bin/env python3
"""Test script for InEar Teensy Optimized Protocol"""

import serial
import struct
import time

PACKET_TYPES = {
    0x00: ("EEG_ONLY", 26),
    0x01: ("EEG_ACCEL", 32),
    0x02: ("EEG_PPG", 44),
    0x03: ("EEG_ACCEL_PPG", 50),
    0x04: ("EEG_HEALTH", 30),
    0x05: ("EEG_ACCEL_HEALTH", 36),
    0x06: ("EEG_FULL_SYNC", 54),
}

def get_packet_size(pkt_type):
    base = pkt_type & 0x0F
    marker = 1 if (pkt_type & 0x10) else 0
    if base in [t for t, _ in PACKET_TYPES.values()]:
        return PACKET_TYPES.get(base, ("UNKNOWN", 26))[1] + marker
    return 26 + marker

def main():
    port = "COM5"  # Adjust for your system
    ser = serial.Serial(port, 2000000, timeout=1)
    
    print(f"Connected to {port}")
    print("Waiting for packets...")
    
    packet_counts = {}
    start_time = time.time()
    
    while time.time() - start_time < 5:  # Run for 5 seconds
        # Find sync bytes
        if ser.read(1) == b'\xA5' and ser.read(1) == b'\x5A':
            pkt_type = ser.read(1)[0]
            base_type = pkt_type & 0x0F
            type_name = PACKET_TYPES.get(base_type, ("UNKNOWN", 26))[0]
            
            packet_counts[type_name] = packet_counts.get(type_name, 0) + 1
    
    print("\nPacket Statistics:")
    for pkt_type, count in sorted(packet_counts.items()):
        print(f"  {pkt_type}: {count}")
    
    ser.close()

if __name__ == "__main__":
    main()
```

---

## 5. Running the System

### 5.1 Startup Sequence

1. **Flash Teensy** with `inear_teensy_firmware_optimized.ino`
2. **Launch Open Ephys GUI**
3. **Add InEar Teensy Opt** from Sources menu
4. **Select COM port** and click Connect
5. **Add processing chain** (e.g., Bandpass Filter → LFP Viewer)
6. **Click Play** to start acquisition

### 5.2 Verify Data Flow

- Check LED on Teensy blinks at 2 Hz
- LFP Viewer should show EEG sine waves
- Console shows packet statistics if verbose mode enabled

---

## 6. Complete Build Scripts

### 6.1 Windows PowerShell

```powershell
# Build everything from workspace root
$ErrorActionPreference = "Stop"

# Build plugin
cd "$env:USERPROFILE\Desktop\workspace\open-ephys\OEPlugins\inear-teensy-source-optimized"
if (-not (Test-Path "Build")) { mkdir Build }
cd Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Deploy plugin
$dest = "$env:USERPROFILE\Desktop\workspace\open-ephys\plugin-GUI\Build\Release\Plugins"
Copy-Item -Force "Release\inear-teensy-source-optimized.dll" $dest

Write-Host "Build complete! Plugin deployed to $dest"
```

### 6.2 Linux/macOS Bash

```bash
#!/bin/bash
set -e

# Build plugin
cd ~/workspace/open-ephys/OEPlugins/inear-teensy-source-optimized
mkdir -p Build && cd Build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Deploy plugin
if [[ "$OSTYPE" == "darwin"* ]]; then
    cp -r Release/inear-teensy-source-optimized.bundle ~/Library/Application\ Support/open-ephys/plugins/
else
    mkdir -p ~/.local/lib/open-ephys/plugins
    cp libinear-teensy-source-optimized.so ~/.local/lib/open-ephys/plugins/
fi

echo "Build complete!"
```

---

## 7. Quick Reference

### Plugin Identification

| Property | Value |
|----------|-------|
| Plugin Name | InEar Teensy Opt |
| Library Name | `inear-teensy-source-optimized` |
| Protocol | Variable-length optimized |
| Matching Firmware | `inear_teensy_firmware_optimized.ino` |

### Packet Type Quick Reference

| Sample # | Packet Type | Size |
|----------|-------------|------|
| Every 1 | EEG_ONLY | 26B |
| Every 4 | EEG_ACCEL | 32B |
| Every 10 | EEG_PPG | 44B |
| Every 20 | EEG_ACCEL_PPG | 50B |
| Every 100 | EEG_HEALTH | 30B |
| Every 1000 | EEG_FULL_SYNC | 54B |

### Common Issues

| Issue | Solution |
|-------|----------|
| No data in LFP Viewer | Check Teensy is flashed with **optimized** firmware |
| Plugin crash on start | Ensure both DataBuffers are created (check sourceBuffers) |
| Packet parsing errors | Verify firmware and plugin use same protocol version |
| Wrong COM port | Use Device Manager (Windows) or `ls /dev/tty*` (Linux/macOS) |
