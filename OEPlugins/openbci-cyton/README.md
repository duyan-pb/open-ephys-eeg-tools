# OpenBCI Cyton Plugin for Open Ephys

This plugin enables direct connection to OpenBCI Cyton boards (8-channel and 16-channel with Daisy) in the Open Ephys GUI.

## Features

- **8-channel Cyton board support** - Full 8-channel EEG acquisition at 250 Hz
- **16-channel Cyton+Daisy support** - Full 16-channel EEG acquisition with Daisy expansion module
- **Automatic serial port detection** - Finds available COM ports (Windows) or /dev/tty devices (Linux/macOS)
- **Real-time data streaming** - Binary protocol parsing with proper scale factors
- **Accelerometer data** - Parses accelerometer data from aux bytes (currently for internal use)

## Requirements

- OpenBCI Cyton board with RFDuino USB dongle
- Open Ephys GUI v1.0.x
- USB drivers for FTDI (usually installed automatically)

## Building from Source

### Prerequisites

- CMake 3.15 or higher
- Visual Studio 2022 (Windows) / GCC (Linux) / Xcode (macOS)
- Open Ephys GUI source code (placed at `../../plugin-GUI` relative to this folder)

### Build Instructions

#### Windows

```bash
mkdir Build
cd Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

Then copy `Build/Release/openbci-cyton.dll` to your Open Ephys `plugins` folder.

#### Linux

```bash
mkdir Build && cd Build
cmake -G "Unix Makefiles" ..
make -j4
```

Copy the resulting `openbci-cyton.so` to your plugins folder.

#### macOS

```bash
mkdir Build && cd Build
cmake ..
make -j4
```

Copy the resulting `openbci-cyton.bundle` to your plugins folder.

## Usage

1. Connect your OpenBCI Cyton dongle to a USB port
2. In Open Ephys, drag "OpenBCI Cyton" from the Sources panel
3. Select the correct COM port from the dropdown
4. Enable "16-ch (Daisy)" if using a Daisy expansion module
5. Click "Connect"
6. Start acquisition

## Data Format

The plugin parses the native OpenBCI binary format (verified against official documentation):

### Packet Structure (33 bytes)
| Byte(s) | Content |
|---------|---------|
| 1 | Header: `0xA0` |
| 2 | Sample Number (0-255) |
| 3-26 | 8 channels × 3 bytes (24-bit signed EEG values, MSB first) |
| 27-32 | Aux data (accelerometer or user-defined, depends on footer) |
| 33 | Footer: `0xCX` where X indicates packet type |

### Footer/Packet Types
| Footer | Description |
|--------|-------------|
| `0xC0` | Standard with accelerometer (AX, AY, AZ as 16-bit signed) |
| `0xC1` | Standard with raw aux bytes |
| `0xC2` | User defined |
| `0xC3` | Timestamp sync + interleaved accelerometer |
| `0xC4` | Timestamp + interleaved accelerometer |
| `0xC5` | Timestamp sync + raw aux |
| `0xC6` | Timestamp + raw aux |

### Scale Factors
```
EEG Scale Factor (V/count) = 4.5V / gain / (2^23 - 1)
At 24x gain (default): 0.02235 µV/count

Accelerometer Scale Factor = 0.002 / 2^4 = 0.000125 G/count
```

### 16-Channel Daisy Mode
When using Cyton+Daisy:
- Odd sample numbers (1,3,5...): Cyton (board) data → channels 1-8
- Even sample numbers (2,4,6...): Daisy data → channels 9-16
- Sample 0 is invalid and skipped
- Effective sample rate per channel: 125 Hz (data is pre-averaged on board)
- The board performs averaging internally before transmission

### Channel Naming
- Channels 1-8: Cyton board channels
- Channels 9-16: Daisy expansion channels (if enabled)

## Technical Details

### Serial Communication
- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1

### Commands Used
- `v` - Soft reset/initialize board (returns firmware version)
- `b` - Start streaming
- `s` - Stop streaming
- `C` - Enable Daisy module (16 channels)
- `c` - Disable Daisy module (8 channels)

### 24-bit to 32-bit Signed Conversion
```cpp
int32_t value = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
if (value & 0x00800000)  // Sign bit set
    value |= 0xFF000000;  // Sign extend
```

## Troubleshooting

### "No ports found"
- Check that the dongle is plugged in
- Try a different USB port
- Check Device Manager (Windows) for the COM port number

### "Connection failed"
- Make sure no other application is using the port (OpenBCI GUI, etc.)
- Try unplugging and replugging the dongle
- Click "Refresh" and try again

### No data appearing
- Check that the Cyton board is powered on
- Verify the correct port is selected
- Check the battery level on the Cyton board

## References

- [OpenBCI Cyton Data Format](https://docs.openbci.com/Cyton/CytonDataFormat/)
- [OpenBCI Cyton SDK](https://docs.openbci.com/Cyton/CytonSDK/)
- [Open Ephys Plugin Development](https://open-ephys.github.io/gui-docs/)

## License

MIT License - See LICENSE file
