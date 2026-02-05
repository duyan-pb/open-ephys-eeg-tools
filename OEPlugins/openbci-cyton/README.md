# OpenBCI Cyton Plugin for Open Ephys

A DataThread plugin that reads EEG data from OpenBCI Cyton boards (8-channel and 16-channel with Daisy) via the RFDuino USB dongle in the Open Ephys GUI.

> **Protocol**: Implements the official [OpenBCI Cyton Data Format](https://docs.openbci.com/Cyton/CytonDataFormat/) and [SDK](https://docs.openbci.com/Cyton/CytonSDK/).

## Protocol: OpenBCI Cyton

33-byte fixed-length packets transmitted at 250 Hz over USB at 115200 baud via RFDuino wireless link.

- **Bandwidth**: ~8.25 KB/s (33 bytes × 250 Hz)
- **Sample Rate**: 250 Hz (firmware limit due to radio constraints)
- **Packet Size**: 33 bytes (fixed)
- **Channels**: 8 (Cyton) or 16 (Cyton+Daisy)

### Packet Structure

| Offset | Size | Field          | Description                                     |
|--------|------|----------------|-------------------------------------------------|
| 0      | 1B   | Header         | Sync byte: `0xA0`                               |
| 1      | 1B   | Sample Number  | Counter 0-255                                   |
| 2-25   | 24B  | EEG Data       | 8 channels × 24-bit Big Endian signed           |
| 26-31  | 6B   | Aux Data       | Accelerometer or user-defined (see footer)      |
| 32     | 1B   | Footer         | Packet type: `0xCX` where X = 0-F               |

### Footer/Packet Types (Firmware v2.0.0+)

| Footer | Aux Bytes 26-31           | Description                              |
|--------|---------------------------|------------------------------------------|
| `0xC0` | AX1,AX0,AY1,AY0,AZ1,AZ0   | Standard with accelerometer (16-bit × 3) |
| `0xC1` | User defined              | Standard with raw aux bytes              |
| `0xC2` | User defined              | User defined                             |
| `0xC3` | AC,AV,T3,T2,T1,T0         | Timestamp sync + interleaved accel       |
| `0xC4` | AC,AV,T3,T2,T1,T0         | Timestamp + interleaved accel            |
| `0xC5` | UDF,UDF,T3,T2,T1,T0       | Timestamp sync + raw aux                 |
| `0xC6` | UDF,UDF,T3,T2,T1,T0       | Timestamp + raw aux                      |

### Scale Factors (from ADS1299 datasheet)

```
EEG Scale Factor (V/count) = 4.5V / gain / (2^23 - 1)

Gain    Scale Factor (µV/count)
----    -----------------------
1x      0.5364
2x      0.2682
4x      0.1341
6x      0.0894
8x      0.0671
12x     0.0447
24x     0.02235  (default)

Accelerometer Scale Factor = 0.002 / 2^4 = 0.000125 G/count (LIS3DH at 4G range)
```

## Data Streams

The plugin creates one data stream in Open Ephys:

1. **EEG Stream** (250 Hz)
   - 8 channels (Cyton) or 16 channels (Cyton+Daisy)
   - 24-bit resolution, scaled to µV
   - Accelerometer data parsed internally (X, Y, Z)

### 16-Channel Daisy Mode

When using Cyton+Daisy, the board interleaves data:

| Sample # | Packet Contains          | Maps To      |
|----------|--------------------------|--------------|
| 0        | Invalid                  | Discarded    |
| 1        | avg(board0, board1)      | Channels 1-8 |
| 2        | avg(daisy1, daisy2)      | Channels 9-16|
| 3        | avg(board2, board3)      | Channels 1-8 |
| 4        | avg(daisy3, daisy4)      | Channels 9-16|
| ...      | ...                      | ...          |

- Effective sample rate per channel: **125 Hz** (data pre-averaged on board)
- Odd samples → Cyton (channels 1-8)
- Even samples → Daisy (channels 9-16)

## Building

### Prerequisites

- Open Ephys GUI source code (plugin-GUI)
- CMake 3.15+
- Visual Studio 2022 (Windows) or GCC/Clang (Linux/macOS)

### Build Steps

```bash
# Windows
cd Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Deploy (requires admin on Windows)
Copy-Item "Release\openbci-cyton.dll" "C:\Program Files\Open Ephys\plugins\" -Force
```

```bash
# Linux
mkdir Build && cd Build
cmake -G "Unix Makefiles" ..
make -j4
cp openbci-cyton.so ~/.local/share/open-ephys/plugins/
```

```bash
# macOS
mkdir Build && cd Build
cmake ..
make -j4
cp openbci-cyton.bundle ~/Library/Application\ Support/open-ephys/plugins/
```

## Usage

1. **Connect Dongle** - Plug in the RFDuino USB dongle
2. **Power Cyton** - Turn on the Cyton board (battery or USB power)
3. **Load Plugin** - Drag "OpenBCI Cyton" from Sources panel
4. **Select Port** - Choose the COM port from dropdown
5. **Enable Daisy** - Check "16-ch (Daisy)" if using expansion module
6. **Click Connect** - Wait for initialization (`$$$` response)
7. **Start Acquisition** - Press play button

## Signal Chain Examples

### Basic Recording
```
[OpenBCI Cyton] → [Bandpass Filter 1-50Hz] → [Record Node] → [LFP Viewer]
```

### Stream to External Software
```
[OpenBCI Cyton] → [Bandpass Filter] → [LSL Outlet] → [Python/MATLAB/BCI2000]
```

### Multi-modal with Teensy
```
[OpenBCI Cyton (EEG 8ch)] → [Merger] → [Record Node]
[InEar Teensy (PPG/Accel)] → [Merger]
```

## Serial Communication

| Parameter | Value |
|-----------|-------|
| Baud Rate | 115200 (default), up to 921600 with `0xF0 0x0A` command |
| Data Bits | 8 |
| Parity    | None |
| Stop Bits | 1 |
| Flow Control | None |

### Commands Used

| Command | Description | Response |
|---------|-------------|----------|
| `v`     | Soft reset  | Firmware version + `$$$` |
| `b`     | Start streaming | None |
| `s`     | Stop streaming | None |
| `C`     | Enable Daisy (16ch) | `daisy attached16$$$` or `16$$$` |
| `c`     | Disable Daisy (8ch) | `daisy removed$$$` |
| `d`     | Reset to default settings | `updating channel settings to default$$$` |
| `?`     | Query registers | Verbose register dump + `$$$` |

### Gain Configuration

Send `x(CHANNEL)(POWER)(GAIN)(INPUT)(BIAS)(SRB2)(SRB1)X` to configure channels:

```
Example: x1060100X
         │││││││└─ Latch command
         ││││││└── SRB1: 0=off
         │││││└─── SRB2: 0=off  
         ││││└──── BIAS: 1=include
         │││└───── INPUT: 0=normal
         ││└────── GAIN: 6=24x
         │└─────── POWER: 0=on
         └──────── CHANNEL: 1
```

## Comparison with InEar Teensy

| Feature | OpenBCI Cyton | InEar Teensy |
|---------|---------------|--------------|
| Channels | 8 (16 w/Daisy) | 5 EEG + 9 aux |
| Sample Rate | 250 Hz | 1000 Hz |
| Resolution | 24-bit | 24-bit (EEG) |
| Connection | Wireless (RFDuino) | USB Direct |
| Bandwidth | ~8.25 KB/s | ~54.7 KB/s |
| Latency | Higher (radio) | Lower (USB) |
| ADC Chip | ADS1299 | ADS1299 |

## Hardware Requirements

- **OpenBCI Cyton Board** (v3) with onboard ADS1299 + LIS3DH
- **RFDuino USB Dongle** (included with Cyton)
- **Daisy Module** (optional, for 16 channels)
- **Lithium battery** or USB power for Cyton board

## Troubleshooting

### "No ports found"
- Verify dongle is plugged in
- Check Device Manager (Windows) for FTDI COM port
- Try different USB port (avoid hubs)

### "Connection failed"
- Ensure no other app is using the port (OpenBCI GUI, serial monitor)
- Unplug/replug dongle and click "Refresh"
- Check that Cyton board is powered on

### No data appearing
- Verify Cyton board battery is charged
- Check that green LED is blinking on Cyton
- Reset by sending `v` command or power cycling

### Dropped packets
- Move dongle closer to Cyton board
- Reduce radio interference
- Try different radio channel with `0xF0 0x01 (channel)` command

### Noisy signal
- Check electrode connections
- Verify SRB2 jumper is set correctly
- Use `0` command to test internal GND (should show ~0.1 µVrms)

## References

- [OpenBCI Cyton Data Format](https://docs.openbci.com/Cyton/CytonDataFormat/)
- [OpenBCI Cyton SDK](https://docs.openbci.com/Cyton/CytonSDK/)
- [OpenBCI Cyton Specs](https://docs.openbci.com/Cyton/CytonSpecs/)
- [Open Ephys Plugin Development](https://open-ephys.github.io/gui-docs/)
- [ADS1299 Datasheet](https://www.ti.com/product/ADS1299)

## License

MIT License - See LICENSE file
