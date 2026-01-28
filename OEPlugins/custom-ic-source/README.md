# Custom IC Source Plugin for Open Ephys

A native Open Ephys plugin for reading data directly from custom integrated circuits (ICs) via serial/UART communication. This is a **DataThread** plugin that appears as a Source Processor in Open Ephys.

## Features

- **Direct serial/UART communication** - No intermediate software required
- **Configurable protocol** - Sync bytes, data format, checksum
- **Multiple data formats** - int16, int24, int32 (big-endian)
- **Adjustable parameters** - Channel count, sample rate, scale factor
- **Simulation mode** - Test without hardware
- **Cross-platform** - Windows, macOS, Linux

## Building

### Prerequisites

1. **Open Ephys GUI** built from source (with CMake)
2. **CMake** 3.15 or higher
3. **Visual Studio 2022** (Windows) or appropriate compiler

### Build Instructions

```bash
# Windows (from custom-ic-source directory)
mkdir Build
cd Build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Linux/macOS
mkdir Build && cd Build
cmake ..
make -j4
```

### Install

Copy the built plugin to your Open Ephys plugins directory:
- **Windows**: `Build/Release/custom-ic-source.dll` → `<OpenEphys>/plugins/`
- **Linux**: `Build/custom-ic-source.so` → `<OpenEphys>/plugins/`
- **macOS**: `Build/custom-ic-source.bundle` → `<OpenEphys>/plugins/`

## Usage

1. **Launch Open Ephys**
2. **Add Source**: Drag "Custom IC" from the Source Processors
3. **Configure**:
   - Select COM port (or check "Simulate" for testing)
   - Set baud rate (default: 115200)
   - Set number of channels
   - Set sample rate (Hz)
   - Set data format (int16/int24/int32)
   - Set scale factor (to convert to microvolts)
   - Set sync bytes (hex, default: A0 5A)
4. **Click "Connect"**
5. **Start acquisition**

## Protocol Configuration

### Default Protocol

The default protocol expects packets in this format:

```
[SYNC1][SYNC2][CH1_H][CH1_L][CH2_H][CH2_L]...[CHECKSUM]
```

| Field | Size | Default | Description |
|-------|------|---------|-------------|
| SYNC1 | 1 byte | 0xA0 | First sync byte |
| SYNC2 | 1 byte | 0x5A | Second sync byte |
| Data | N × bytes_per_sample | - | Channel data (big-endian) |
| Checksum | 1 byte | - | XOR of all previous bytes |

### Data Formats

| Format | Bytes/Sample | Range | Typical Use |
|--------|-------------|-------|-------------|
| int16 | 2 | ±32,767 | Most ADCs |
| int24 | 3 | ±8,388,607 | High-resolution ADCs (ADS1299) |
| int32 | 4 | ±2,147,483,647 | 32-bit ADCs |

### Scale Factor

The scale factor converts raw ADC values to microvolts:

```
μV = raw_value × scale_factor
```

**Common scale factors:**
- **ADS1299** (24-bit, ±4.5V, gain=24): `0.195` μV/LSB
- **Generic 16-bit** (±5V): `152.6` μV/LSB (5V / 32768)

## Customizing the Protocol

To customize for your IC, edit `CustomICThread.cpp`:

### 1. Parse Function
```cpp
// In ProtocolParser::parse()
// Modify sync byte detection, packet structure, etc.
```

### 2. Bytes to Sample
```cpp
// In ProtocolParser::bytesToSample()
// Change endianness, bit packing, etc.
```

### 3. Checksum Validation
```cpp
// In ProtocolParser::validateChecksum()
// Implement your checksum algorithm (CRC, sum, etc.)
```

## Example: ADS1299 Configuration

For TI ADS1299 EEG AFE:

| Parameter | Value |
|-----------|-------|
| Channels | 8 |
| Sample Rate | 250 Hz |
| Data Format | int24 |
| Scale Factor | 0.195 |
| Baud Rate | 921600 |

## Troubleshooting

### No COM Ports Listed
- Check USB drivers are installed
- Verify device is connected
- Try "Refresh" button

### No Data After Connect
- Verify baud rate matches device
- Check sync bytes are correct
- Try simulation mode first to verify Open Ephys setup

### Corrupted Data
- Check data format (int16 vs int24)
- Verify scale factor
- Check endianness (default: big-endian)

### Port Access Denied
- Close other programs using the port
- On Linux: Add user to `dialout` group

## License

GPL-3.0 (same as Open Ephys)

## Author

Custom IC Source Plugin for Open Ephys
