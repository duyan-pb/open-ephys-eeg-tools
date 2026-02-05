# InEar Teensy Firmware Optimized (Variable-Length Protocol)

Firmware for Teensy 4.1 that streams EEG data using variable-length packets (26-55 bytes) at 1 kHz, reducing bandwidth by ~49%.

## Protocol Overview

- **Packet Size**: 26-55 bytes (variable)
- **Sample Rate**: 1000 Hz (EEG), multi-rate aux
- **Baud Rate**: 2,000,000 (2 Mbaud)
- **Bandwidth**: ~28.5 KB/s (49% savings vs original)

## Multi-Rate Sampling

| Data Type | Sample Rate | Packets |
|-----------|-------------|---------|
| EEG | 1000 Hz | Every packet |
| Accel | 250 Hz | Every 4th packet |
| PPG | 100 Hz | Every 10th packet |
| Health | 10 Hz | Every 100th packet |

## Packet Types

| Type | Code | Size | Contents |
|------|------|------|----------|
| EEG_ONLY | 0x00 | 26B | EEG only (~64%) |
| EEG_ACCEL | 0x01 | 32B | EEG + Accel (~25%) |
| EEG_ACCEL_PPG | 0x03 | 50B | EEG + Accel + PPG (~10%) |
| EEG_FULL_SYNC | 0x06 | 55B | All sensors (~1%) |

## Packet Structure

```
┌─────────────────────────────────────────────────────────────────────────┐
│ HEADER (8B) │ EEG (15B) │ [MARKER] │ [ACCEL] │ [PPG] │ [HEALTH] │ FOOTER (3B) │
└─────────────────────────────────────────────────────────────────────────┘
```

| Field | Size | Description |
|-------|------|-------------|
| Sync | 2B | `0xA5 0x5A` |
| Type | 1B | Packet type enum |
| Sequence | 1B | 0-255 counter |
| Timestamp | 4B | Microseconds (BE) |
| EEG | 15B | 5 channels × 24-bit |
| Optional | Varies | Accel, PPG, Health |
| Checksum | 1B | XOR of all preceding |
| Footer | 2B | `0xC0 0xC0` |

## Hardware Requirements

- Teensy 4.1 microcontroller
- ADS1299 EEG analog front-end
- Optional: Accelerometer, PPG sensor

## Flashing

1. Open Arduino IDE
2. Open `inear_teensy_firmware_optimized.ino`
3. Select Tools → Board → Teensy 4.1
4. Click Upload

Or via command line:
```bash
arduino-cli upload -p COM5 --fqbn teensy:avr:teensy41 inear_teensy_firmware_optimized.ino
```

## Compatible Plugin

Use with `inear-teensy-source-optimized` plugin in Open Ephys.

## Comparison with Original

| Feature | Original | Optimized |
|---------|----------|-----------|
| Packet Size | 56B fixed | 26-55B variable |
| Bandwidth | 54.7 KB/s | 28.5 KB/s |
| Savings | - | 49% |

## License

MIT License
