# ADS1299 BioSerial Pro Firmware Optimized (Variable-Length Protocol)

Firmware for Teensy 4.1 that streams EEG data from ADS1299 using the BioSerial Pro optimized variable-length packet protocol.

## Protocol Overview

- **Packet Size**: 26-55 bytes (variable)
- **Sample Rate**: 1000 Hz (EEG), multi-rate aux
- **Baud Rate**: 2,000,000 (2 Mbaud)
- **Bandwidth**: ~28.5 KB/s (~49% savings)

## Multi-Rate Sampling

| Data Type | Sample Rate |
|-----------|-------------|
| EEG | 1000 Hz |
| Accel | 250 Hz |
| PPG | 100 Hz |
| Health | 10 Hz |

## Compatible With

Use with corresponding Open Ephys plugin or test scripts.

## Flashing

1. Open Arduino IDE
2. Open `ads1299_bioserial_pro_optimized.ino`
3. Select Tools → Board → Teensy 4.1
4. Click Upload

## License

MIT License
