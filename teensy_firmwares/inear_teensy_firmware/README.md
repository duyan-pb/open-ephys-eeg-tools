# InEar Teensy Firmware (Fixed-Length Protocol)

Firmware for Teensy 4.1 that streams EEG data using fixed 56-byte packets at 1 kHz.

## Protocol Overview

- **Packet Size**: 56 bytes (fixed)
- **Sample Rate**: 1000 Hz
- **Baud Rate**: 2,000,000 (2 Mbaud)
- **Bandwidth**: 54.7 KB/s

## Packet Structure

```
┌────────────────────────────────────────────────────────────────────┐
│ A5 5A │ TS[4] │ MKR │ EEG[15] │ ACCEL[6] │ PPG[18] │ TEMP │ BAT │ SYNC │ CNT │ CHK │ C0 C0 │
└────────────────────────────────────────────────────────────────────┘
```

| Field | Bytes | Description |
|-------|-------|-------------|
| Header | 2 | `0xA5 0x5A` sync bytes |
| Timestamp | 4 | Microseconds (Big Endian) |
| Marker | 1 | Event trigger byte |
| EEG | 15 | 5 channels × 24-bit BE |
| Accel | 6 | 3 channels × 16-bit BE |
| PPG | 18 | 3 channels × 48-bit BE |
| Temp | 2 | Temperature × 16-bit BE |
| Battery | 2 | Battery voltage × 16-bit BE |
| Sync | 2 | Sync signal × 16-bit BE |
| Counter | 1 | Sequence 0-255 |
| Checksum | 1 | XOR of bytes 0-52 |
| Footer | 2 | `0xC0 0xC0` |

## Hardware Requirements

- Teensy 4.1 microcontroller
- ADS1299 EEG analog front-end
- Optional: Accelerometer, PPG sensor

## Flashing

1. Open Arduino IDE
2. Open `inear_teensy_firmware.ino`
3. Select Tools → Board → Teensy 4.1
4. Click Upload

Or via command line:
```bash
arduino-cli upload -p COM5 --fqbn teensy:avr:teensy41 inear_teensy_firmware.ino
```

## Compatible Plugin

Use with `inear-teensy-source` plugin in Open Ephys.

## License

MIT License
