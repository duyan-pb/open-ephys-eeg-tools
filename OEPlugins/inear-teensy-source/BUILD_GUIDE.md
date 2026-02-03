# InEar Teensy Build & Deploy Guide

Complete instructions for building, deploying, and running the InEar Teensy system on Windows, Linux, and macOS.

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

The firmware runs on Teensy 4.1 and streams EEG data over USB using the InEar Teensy protocol.

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
| **Windows** | `& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn teensy:avr:teensy41 "path\to\inear_teensy_firmware.ino"` |
| **Linux/macOS** | `arduino-cli compile --fqbn teensy:avr:teensy41 ./inear_teensy_firmware/inear_teensy_firmware.ino` |

**Flags explained:**
- `compile` - Compile without uploading
- `--fqbn teensy:avr:teensy41` - Fully Qualified Board Name for Teensy 4.1

### 2.3 Upload to Teensy

Compiles (if needed) and uploads to the connected Teensy.

| Platform | Command |
|----------|---------|
| **Windows** | `& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" upload -p COM5 --fqbn teensy:avr:teensy41 "path\to\inear_teensy_firmware.ino"` |
| **Linux** | `arduino-cli upload -p /dev/ttyACM0 --fqbn teensy:avr:teensy41 ./inear_teensy_firmware/inear_teensy_firmware.ino` |
| **macOS** | `arduino-cli upload -p /dev/cu.usbmodem* --fqbn teensy:avr:teensy41 ./inear_teensy_firmware/inear_teensy_firmware.ino` |

**Flags explained:**
- `upload` - Compile and upload to board
- `-p COM5` - Serial port (adjust for your system)

---

## 3. Open Ephys Plugin

The plugin reads data from the Teensy and provides it to Open Ephys for visualization and recording.

### 3.1 Configure CMake (First Time Only)

Creates build system files in the `Build` directory.

| Platform | Command |
|----------|---------|
| **Windows** | `cd OEPlugins\InEar Teensy-source; mkdir Build; cd Build; cmake -G "Visual Studio 17 2022" -A x64 ..` |
| **Linux** | `cd OEPlugins/InEar Teensy-source && mkdir -p Build && cd Build && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..` |
| **macOS** | `cd OEPlugins/InEar Teensy-source && mkdir -p Build && cd Build && cmake -G "Xcode" ..` |

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
| **Windows** | `Build\Release\InEar Teensy-source.dll` | `<Open Ephys>\plugins\` |
| **Linux** | `Build/libInEar Teensy-source.so` | `~/.local/lib/open-ephys/plugins/` |
| **macOS** | `Build/Release/InEar Teensy-source.bundle` | `~/Library/Application Support/open-ephys/plugins/` |

**Commands:**

```powershell
# Windows (PowerShell)
Copy-Item -Force "Build\Release\InEar Teensy-source.dll" "C:\path\to\open-ephys\plugins\"
```

```bash
# Linux
mkdir -p ~/.local/lib/open-ephys/plugins
cp Build/libInEar Teensy-source.so ~/.local/lib/open-ephys/plugins/
```

```bash
# macOS
mkdir -p ~/Library/Application\ Support/open-ephys/plugins
cp Build/Release/InEar Teensy-source.bundle ~/Library/Application\ Support/open-ephys/plugins/
```

### 3.4 Alternative Plugin Locations

System-wide installation (requires admin/root):

| Platform | Path |
|----------|------|
| **Windows** | `C:\Program Files\Open Ephys\plugins\` |
| **Linux** | `/usr/local/lib/open-ephys/plugins/` |
| **macOS** | `/Library/Application Support/open-ephys/plugins/` |

---

## 4. Python Test Script

The test script validates the protocol and helps debug communication issues.

### 4.1 Setup Virtual Environment

```bash
# Windows (PowerShell)
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install pyserial

# Linux/macOS
python3 -m venv .venv
source .venv/bin/activate
pip install pyserial
```

### 4.2 Run Tests

**Read from Teensy (validate hardware):**

| Platform | Command |
|----------|---------|
| **Windows** | `python inear_teensy_test.py --mode read --port COM5 --duration 5` |
| **Linux** | `python inear_teensy_test.py --mode read --port /dev/ttyACM0 --duration 5` |
| **macOS** | `python inear_teensy_test.py --mode read --port /dev/cu.usbmodem* --duration 5` |

**Round-trip test (no hardware needed):**

```bash
python inear_teensy_test.py --mode roundtrip
```

**Expected output:**
```
Reading from COM5 for 5 seconds...
Packets: 5000 | Rate: 999.8 Hz | Dropped: 0 | CRC Errors: 0
```

---

## 5. Running the System

### 5.1 Launch Open Ephys

| Platform | Command |
|----------|---------|
| **Windows** | `Start-Process "C:\path\to\open-ephys.exe"` |
| **Linux** | `./open-ephys` or `open-ephys` (if installed) |
| **macOS** | `open /path/to/open-ephys.app` or `open -a "Open Ephys"` |

### 5.2 Configure Signal Chain

1. **Add InEar Teensy Source**: Drag from plugin list to signal chain
2. **Configure Port**: Select your COM port (or enable Simulation mode)
3. **Add LFP Viewer**: Drag and connect after the source
4. **Press Play**: Start acquisition

---

## 6. Complete Build Scripts

### Windows (PowerShell)

Save as `build_all.ps1`:

```powershell
# InEar Teensy Complete Build Script for Windows

# Configuration - EDIT THESE PATHS
$ARDUINO_CLI = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$WORKSPACE = "C:\Users\duyan\Desktop\workspace\open-ephys"
$PORT = "COM5"

# Derived paths
$FIRMWARE_PATH = "$WORKSPACE\inear_teensy_firmware\inear_teensy_firmware.ino"
$PLUGIN_DIR = "$WORKSPACE\OEPlugins\InEar Teensy-source"
$OE_PLUGINS = "$WORKSPACE\plugin-GUI\Build\Release\plugins"
$OE_EXE = "$WORKSPACE\plugin-GUI\Build\Release\open-ephys.exe"

Write-Host "=== InEar Teensy Build Script ===" -ForegroundColor Cyan

# 1. Build & Upload Firmware
Write-Host "`n[1/4] Compiling firmware..." -ForegroundColor Yellow
& $ARDUINO_CLI compile --fqbn teensy:avr:teensy41 $FIRMWARE_PATH
if ($LASTEXITCODE -ne 0) { Write-Host "Firmware compile failed!" -ForegroundColor Red; exit 1 }

Write-Host "`n[2/4] Uploading to Teensy on $PORT..." -ForegroundColor Yellow
& $ARDUINO_CLI upload -p $PORT --fqbn teensy:avr:teensy41 $FIRMWARE_PATH
if ($LASTEXITCODE -ne 0) { Write-Host "Firmware upload failed!" -ForegroundColor Red; exit 1 }

# 2. Build Plugin
Write-Host "`n[3/4] Building plugin..." -ForegroundColor Yellow
Push-Location "$PLUGIN_DIR\Build"
cmake --build . --config Release
if ($LASTEXITCODE -ne 0) { Pop-Location; Write-Host "Plugin build failed!" -ForegroundColor Red; exit 1 }
Pop-Location

# 3. Deploy Plugin
Write-Host "`n[4/4] Deploying plugin..." -ForegroundColor Yellow
Copy-Item -Force "$PLUGIN_DIR\Build\Release\InEar Teensy-source.dll" $OE_PLUGINS

Write-Host "`n=== Build Complete ===" -ForegroundColor Green
Write-Host "Run Open Ephys: Start-Process `"$OE_EXE`""
```

### Linux (Bash)

Save as `build_all.sh`:

```bash
#!/bin/bash
set -e

# InEar Teensy Complete Build Script for Linux

# Configuration - EDIT THESE PATHS
WORKSPACE="$HOME/workspace/open-ephys"
PORT="/dev/ttyACM0"

# Derived paths
FIRMWARE_PATH="$WORKSPACE/inear_teensy_firmware/inear_teensy_firmware.ino"
PLUGIN_DIR="$WORKSPACE/OEPlugins/InEar Teensy-source"
OE_PLUGINS="$HOME/.local/lib/open-ephys/plugins"

echo "=== InEar Teensy Build Script ==="

# 1. Build & Upload Firmware
echo -e "\n[1/4] Compiling firmware..."
arduino-cli compile --fqbn teensy:avr:teensy41 "$FIRMWARE_PATH"

echo -e "\n[2/4] Uploading to Teensy on $PORT..."
arduino-cli upload -p "$PORT" --fqbn teensy:avr:teensy41 "$FIRMWARE_PATH"

# 2. Build Plugin
echo -e "\n[3/4] Building plugin..."
cd "$PLUGIN_DIR/Build"
cmake --build . --config Release

# 3. Deploy Plugin
echo -e "\n[4/4] Deploying plugin..."
mkdir -p "$OE_PLUGINS"
cp libInEar Teensy-source.so "$OE_PLUGINS/"

echo -e "\n=== Build Complete ==="
echo "Run Open Ephys: open-ephys"
```

### macOS (Bash)

Save as `build_all.sh`:

```bash
#!/bin/bash
set -e

# InEar Teensy Complete Build Script for macOS

# Configuration - EDIT THESE PATHS
WORKSPACE="$HOME/workspace/open-ephys"
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)

# Derived paths
FIRMWARE_PATH="$WORKSPACE/inear_teensy_firmware/inear_teensy_firmware.ino"
PLUGIN_DIR="$WORKSPACE/OEPlugins/InEar Teensy-source"
OE_PLUGINS="$HOME/Library/Application Support/open-ephys/plugins"

echo "=== InEar Teensy Build Script ==="

# 1. Build & Upload Firmware
echo -e "\n[1/4] Compiling firmware..."
arduino-cli compile --fqbn teensy:avr:teensy41 "$FIRMWARE_PATH"

echo -e "\n[2/4] Uploading to Teensy on $PORT..."
arduino-cli upload -p "$PORT" --fqbn teensy:avr:teensy41 "$FIRMWARE_PATH"

# 2. Build Plugin
echo -e "\n[3/4] Building plugin..."
cd "$PLUGIN_DIR/Build"
cmake --build . --config Release

# 3. Deploy Plugin
echo -e "\n[4/4] Deploying plugin..."
mkdir -p "$OE_PLUGINS"
cp Release/InEar Teensy-source.bundle "$OE_PLUGINS/"

echo -e "\n=== Build Complete ==="
echo "Run Open Ephys: open -a 'Open Ephys'"
```

---

## 7. Quick Reference

### File Extensions by Platform

| Platform | Plugin Extension | Serial Port Format |
|----------|------------------|-------------------|
| Windows | `.dll` | `COM5` |
| Linux | `.so` | `/dev/ttyACM0` |
| macOS | `.bundle` | `/dev/cu.usbmodem*` |

### CMake Generators

| Platform | Generator |
|----------|-----------|
| Windows | `"Visual Studio 17 2022"` |
| Linux | `"Unix Makefiles"` |
| macOS | `"Xcode"` |

### Common Issues

| Issue | Solution |
|-------|----------|
| Port not found | Check USB connection, install Teensy drivers |
| Plugin not loading | Verify plugin is in correct folder, check Open Ephys console for errors |
| UI freezes | Rebuild plugin with latest code (includes sleep fix) |
| No data in viewer | Check port selection in plugin editor, verify Teensy is streaming |

---

## Version History

- **v1.0** - Initial release with 56-byte InEar Teensy protocol
- 5 EEG channels (24-bit) + 9 Aux channels @ 1kHz
- Header: `0xA5 0x5A`, Footer: `0xC0 0xC0`
- Checksum: XOR of bytes 0-52

