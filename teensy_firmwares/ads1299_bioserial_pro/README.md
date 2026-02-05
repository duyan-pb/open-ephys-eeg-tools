# ADS1299 BioSerial Pro Firmware (Fixed-Length Protocol)

Firmware for Teensy 4.1 that streams EEG data from ADS1299 using the BioSerial Pro fixed 56-byte packet protocol.

## Protocol Overview

- **Packet Size**: 56 bytes (fixed)
- **Sample Rate**: 1000 Hz
- **Baud Rate**: 2,000,000 (2 Mbaud)
- **Bandwidth**: 54.7 KB/s

## Compatible With

Use with corresponding Open Ephys plugin or test scripts.

## Flashing

1. Open Arduino IDE
2. Open `ads1299_bioserial_pro.ino`
3. Select Tools → Board → Teensy 4.1
4. Click Upload

## License

MIT License
