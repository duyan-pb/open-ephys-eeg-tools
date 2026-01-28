# Custom Python IC Connector (Open Ephys Plugin)

A Python-based plugin that streams data from custom IC/electrodes to Open Ephys via LSL (Lab Streaming Layer).

This is a **Python bridge plugin** - it runs alongside Open Ephys and connects via the LSL Inlet plugin.

## Installation

```bash
cd OEPlugins/custom_python_ic_connector
pip install -r requirements.txt
```

## Quick Start

### 1. Test with Simulated Data
```bash
cd OEPlugins/custom_python_ic_connector
python ic_to_lsl_connector.py --simulate
```

### 2. Connect to Open Ephys
1. Start Open Ephys
2. Add **LSL Inlet** to your signal chain
3. Click the LSL Inlet and select your stream ("CustomIC")
4. Press Play!

## Usage

### Command Line Options
```bash
# Simulate 8-channel EEG at 1000 Hz
python ic_to_lsl_connector.py --simulate --channels 8 --srate 1000

# Connect to real hardware
python ic_to_lsl_connector.py --port COM3 --baudrate 2000000

# Use configuration file
python ic_to_lsl_connector.py --config my_device.yaml

# Create example config
python ic_to_lsl_connector.py --create-config my_device.yaml
```

### Arguments
| Argument | Description | Default |
|----------|-------------|---------|
| `--simulate` | Use simulated data | False |
| `--port` | Serial port | COM3 |
| `--baudrate` | Baud rate | 2000000 |
| `--channels` | Number of channels | 8 |
| `--srate` | Sample rate (Hz) | 1000 |
| `--name` | Device name | CustomIC |
| `--config` | YAML config file | None |
| `--verbose` | Debug output | False |

## Configuration File

For complex setups, use a YAML config file. See `example_config.yaml`:

```yaml
name: "MyEEG_IC"
interface: "serial"
port: "COM3"
baudrate: 2000000
num_channels: 8
sample_rate: 1000.0
bits_per_sample: 16
data_format: "int16"
vref: 3.3
gain: 24.0
channel_names: ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "O1", "O2"]
```

## Adapting for Your IC

### 1. Define Your Data Format

Edit `ICConfig` in the script or create a YAML file:

```python
config = ICConfig(
    # Your IC's packet format
    header_bytes=b'\xAA\x55',    # Sync bytes
    num_channels=8,              # Channels
    data_format='int16',         # ADC format
    has_timestamp=True,          # 4-byte timestamp?
    has_checksum=True,           # XOR checksum?
    
    # Voltage scaling
    vref=3.3,                    # Reference voltage
    gain=24.0,                   # Amplifier gain
    bits_per_sample=16,          # ADC bits
)
```

### 2. Customize Packet Parsing

If your IC has a unique format, modify `DataParser.parse()`:

```python
def parse(self, packet: bytes) -> Optional[np.ndarray]:
    # Example: Custom packet with status byte
    header = packet[0:2]      # 2-byte header
    status = packet[2]        # Status byte
    data = packet[3:19]       # 8 x int16 = 16 bytes
    checksum = packet[19]     # Checksum
    
    # Parse channels
    samples = struct.unpack('<8h', data)
    return np.array(samples) * self.scale
```

### 3. Add Custom Interface

For SPI/I2C/USB, create a new interface class:

```python
class SPIInterface(ICInterface):
    def __init__(self, config: ICConfig):
        import spidev
        self.spi = spidev.SpiDev()
        
    def connect(self) -> bool:
        self.spi.open(0, 0)
        self.spi.max_speed_hz = 10000000
        return True
    
    def read_packet(self) -> Optional[bytes]:
        return bytes(self.spi.readbytes(self.config.packet_size))
```

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    ic_to_lsl_connector.py                   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐ │
│  │  ICConfig   │───▶│ ICInterface │───▶│   DataParser    │ │
│  │ (settings)  │    │ (Serial/SPI)│    │ (binary→float)  │ │
│  └─────────────┘    └─────────────┘    └────────┬────────┘ │
│                                                  │          │
│                                          ┌──────▼────────┐ │
│                                          │  LSLStreamer  │ │
│                                          │ (→ network)   │ │
│                                          └───────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │   Open Ephys    │
                    │   (LSL Inlet)   │
                    └─────────────────┘
```

## Troubleshooting

### No data in Open Ephys?
1. Check if stream is detected: `python -c "import pylsl; print(pylsl.resolve_streams())"`
2. Verify connector shows "samples/sec" in logs
3. Ensure LSL Inlet is connected to the right stream

### Wrong sample rate?
- Check `sample_rate` matches your IC's actual rate
- Verify `baudrate` is high enough for your data throughput

### Corrupted data?
- Verify `header_bytes` matches your IC's sync pattern
- Check `data_format` and `byte_order` match your IC
- Enable `has_checksum` if your IC sends checksums

## Dependencies

```bash
pip install pylsl numpy pyserial pyyaml
```

## License

MIT License - Free to use and modify for your project.
