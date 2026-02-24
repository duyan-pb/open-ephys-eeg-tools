# Teensy Firmware Collection

Firmware implementations for Teensy 4.1 microcontrollers used with Open Ephys plugins.

## Available Firmware

| Firmware | Protocol | Bandwidth | Plugin |
|----------|----------|-----------|--------|
| **[inear_teensy_firmware](inear_teensy_firmware/)** | Fixed 56-byte packets | 54.7 KB/s | `inear-teensy-source` |
| **[inear_teensy_firmware_optimized](inear_teensy_firmware_optimized/)** | Variable 26-55 byte packets | ~28.5 KB/s | `inear-teensy-source-optimized` |
| **[ads1299_bioserial_pro](ads1299_bioserial_pro/)** | BioSerial Pro fixed | 54.7 KB/s | `bioserial-pro-source` |
| **[ads1299_bioserial_pro_optimized](ads1299_bioserial_pro_optimized/)** | BioSerial Pro optimized | ~28.5 KB/s | `bioserial-pro-source` |
| **[ads1299_v12](ads1299_v12/)** | Legacy ADS1299 protocol | Varies | *(deprecated)* |

## Recommended Setup

For most use cases, use the **optimized** firmware/plugin pair:

```
Teensy: inear_teensy_firmware_optimized
Plugin: inear-teensy-source-optimized
Result: ~49% bandwidth reduction
```

## Hardware Requirements

All firmware requires:
- **Teensy 4.1** microcontroller
- **ADS1299** EEG analog front-end
- USB connection to PC (2 Mbaud)

Optional sensors:
- Accelerometer (ADXL345 or similar)
- PPG sensor (MAX30105, 3 LEDs: Red/IR/Green)
- Temperature sensor
- Battery voltage monitor

## Flashing Firmware

### Using Arduino IDE (Recommended)

1. Install [Teensyduino](https://www.pjrc.com/teensy/teensyduino.html)
2. Open firmware `.ino` file
3. Select Tools → Board → Teensy 4.1
4. Click Upload

### Using Arduino CLI

```bash
# Install Teensy support
arduino-cli core install teensy:avr

# Compile
arduino-cli compile --fqbn teensy:avr:teensy41 <firmware-folder>/<firmware>.ino

# Upload (replace COM5 with your port)
arduino-cli upload -p COM5 --fqbn teensy:avr:teensy41 <firmware-folder>/<firmware>.ino
```

## Protocol Comparison

### Fixed vs Optimized

| Feature | Fixed Protocol | Optimized Protocol |
|---------|----------------|-------------------|
| Packet Size | 56 bytes (constant) | 26-55 bytes (variable) |
| Aux Data Rate | 1000 Hz (oversampled) | Native rates (10-250 Hz) |
| Bandwidth | 54.7 KB/s | ~28.5 KB/s |
| Savings | Baseline | 49% |
| Complexity | Simple | Moderate |

### Multi-Rate Sampling (Optimized Only)

| Data | Rate | Rationale |
|------|------|-----------|
| EEG | 1000 Hz | Full resolution |
| Accel | 250 Hz | Sufficient for motion |
| PPG | 100 Hz | Cardiac signals |
| Health | 10 Hz | Slow-changing |

## Shared Components

- **[xorshift.h](xorshift.h)** - Fast PRNG for test data generation

## License

MIT License
