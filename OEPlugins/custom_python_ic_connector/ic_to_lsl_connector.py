#!/usr/bin/env python3
"""
Custom IC to LSL Connector
==========================

This application reads data from a custom IC/electrode and streams it via LSL
to Open Ephys or any other LSL-compatible software.

Usage:
    python ic_to_lsl_connector.py --port COM3 --channels 8 --srate 1000
    python ic_to_lsl_connector.py --config my_device.yaml
    python ic_to_lsl_connector.py --simulate  # Test without hardware

Author: Your Name
Date: 2026
"""

import argparse
import time
import struct
import threading
import queue
import logging
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import List, Optional, Callable
import numpy as np

try:
    import pylsl
except ImportError:
    print("ERROR: pylsl not installed. Run: pip install pylsl")
    exit(1)

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False
    print("WARNING: pyserial not installed. Serial interface disabled.")

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


# =============================================================================
# Configuration
# =============================================================================

@dataclass
class ICConfig:
    """Configuration for your custom IC"""
    # Device identification
    name: str = "CustomIC"
    device_id: str = "custom_ic_001"
    
    # Communication settings
    interface: str = "serial"  # serial, spi, usb, simulate
    port: str = "COM3"         # Serial port or device path
    baudrate: int = 2000000    # Baud rate for serial
    
    # Data format settings
    num_channels: int = 8
    sample_rate: float = 1000.0
    bits_per_sample: int = 16       # ADC resolution
    data_format: str = "int16"      # int16, int24, int32, float32
    byte_order: str = "little"      # little or big endian
    
    # Scaling
    vref: float = 3.3               # Reference voltage (V)
    gain: float = 1.0               # Amplifier gain
    adc_max: int = 32767            # Max ADC value (2^(bits-1) - 1 for signed)
    output_unit: str = "microvolts" # microvolts, millivolts, volts
    
    # Channel information
    channel_names: List[str] = field(default_factory=list)
    channel_types: List[str] = field(default_factory=list)
    
    # Protocol settings
    header_bytes: bytes = b'\xAA\x55'  # Packet header/sync bytes
    packet_size: int = 0                # Auto-calculated if 0
    has_timestamp: bool = False         # Does IC send timestamps?
    has_checksum: bool = False          # Does packet have checksum?
    
    def __post_init__(self):
        # Auto-generate channel names if not provided
        if not self.channel_names:
            self.channel_names = [f"Ch{i+1}" for i in range(self.num_channels)]
        if not self.channel_types:
            self.channel_types = ["EEG"] * self.num_channels
        
        # Calculate packet size if not specified
        if self.packet_size == 0:
            bytes_per_sample = self.bits_per_sample // 8
            self.packet_size = (
                len(self.header_bytes) +
                (4 if self.has_timestamp else 0) +
                (self.num_channels * bytes_per_sample) +
                (1 if self.has_checksum else 0)
            )


# =============================================================================
# IC Interface Abstraction
# =============================================================================

class ICInterface(ABC):
    """Abstract base class for IC communication interfaces"""
    
    @abstractmethod
    def connect(self) -> bool:
        """Connect to the IC"""
        pass
    
    @abstractmethod
    def disconnect(self):
        """Disconnect from the IC"""
        pass
    
    @abstractmethod
    def read_packet(self) -> Optional[bytes]:
        """Read one data packet from IC"""
        pass
    
    @abstractmethod
    def is_connected(self) -> bool:
        """Check if connection is active"""
        pass


class SerialInterface(ICInterface):
    """Serial/UART interface for IC communication"""
    
    def __init__(self, config: ICConfig):
        self.config = config
        self.serial = None
        self.buffer = bytearray()
        
    def connect(self) -> bool:
        if not HAS_SERIAL:
            logging.error("pyserial not installed")
            return False
        try:
            self.serial = serial.Serial(
                port=self.config.port,
                baudrate=self.config.baudrate,
                timeout=0.1
            )
            logging.info(f"Connected to {self.config.port} at {self.config.baudrate} baud")
            return True
        except Exception as e:
            logging.error(f"Failed to connect: {e}")
            return False
    
    def disconnect(self):
        if self.serial:
            self.serial.close()
            self.serial = None
    
    def is_connected(self) -> bool:
        return self.serial is not None and self.serial.is_open
    
    def read_packet(self) -> Optional[bytes]:
        if not self.is_connected():
            return None
        
        # Read available data
        if self.serial.in_waiting:
            self.buffer.extend(self.serial.read(self.serial.in_waiting))
        
        # Look for sync header
        header = self.config.header_bytes
        header_pos = self.buffer.find(header)
        
        if header_pos == -1:
            # No header found, keep last few bytes in case header is split
            if len(self.buffer) > len(header):
                self.buffer = self.buffer[-(len(header)-1):]
            return None
        
        # Discard data before header
        if header_pos > 0:
            self.buffer = self.buffer[header_pos:]
        
        # Check if we have a complete packet
        if len(self.buffer) < self.config.packet_size:
            return None
        
        # Extract packet
        packet = bytes(self.buffer[:self.config.packet_size])
        self.buffer = self.buffer[self.config.packet_size:]
        
        # Verify checksum if enabled
        if self.config.has_checksum:
            if not self._verify_checksum(packet):
                logging.warning("Checksum error, dropping packet")
                return None
        
        return packet
    
    def _verify_checksum(self, packet: bytes) -> bool:
        """Verify packet checksum - customize for your IC's algorithm"""
        # Example: XOR checksum
        checksum = 0
        for b in packet[:-1]:
            checksum ^= b
        return checksum == packet[-1]


class SimulatedInterface(ICInterface):
    """Simulated interface for testing without hardware"""
    
    def __init__(self, config: ICConfig):
        self.config = config
        self.connected = False
        self.sample_counter = 0
        self.last_time = 0
        
    def connect(self) -> bool:
        self.connected = True
        self.last_time = time.perf_counter()
        logging.info("Simulated IC connected")
        return True
    
    def disconnect(self):
        self.connected = False
    
    def is_connected(self) -> bool:
        return self.connected
    
    def read_packet(self) -> Optional[bytes]:
        if not self.connected:
            return None
        
        # Wait for next sample time
        sample_period = 1.0 / self.config.sample_rate
        now = time.perf_counter()
        if now - self.last_time < sample_period:
            return None
        self.last_time = now
        
        # Generate simulated EEG-like data
        packet = bytearray(self.config.header_bytes)
        
        if self.config.has_timestamp:
            packet.extend(struct.pack('<I', self.sample_counter))
        
        for ch in range(self.config.num_channels):
            # Simulate different frequencies per channel
            freq = 10 + ch * 2  # 10Hz, 12Hz, 14Hz, ...
            phase = 2 * np.pi * freq * self.sample_counter / self.config.sample_rate
            
            # Add noise and create realistic-looking signal
            signal = (
                50 * np.sin(phase) +                    # Main wave
                20 * np.sin(phase * 0.5) +              # Low freq component
                10 * np.random.randn()                  # Noise
            )
            
            # Convert to ADC counts
            adc_value = int(signal * self.config.adc_max / 500)
            adc_value = max(-self.config.adc_max, min(self.config.adc_max, adc_value))
            
            # Pack based on data format
            if self.config.data_format == "int16":
                packet.extend(struct.pack('<h', adc_value))
            elif self.config.data_format == "int24":
                packet.extend(adc_value.to_bytes(3, 'little', signed=True))
            elif self.config.data_format == "int32":
                packet.extend(struct.pack('<i', adc_value))
        
        if self.config.has_checksum:
            checksum = 0
            for b in packet:
                checksum ^= b
            packet.append(checksum)
        
        self.sample_counter += 1
        return bytes(packet)


# =============================================================================
# Data Parser
# =============================================================================

class DataParser:
    """Parse binary packets into channel data"""
    
    def __init__(self, config: ICConfig):
        self.config = config
        
        # Determine struct format
        if config.data_format == "int16":
            self.sample_struct = '<' + 'h' * config.num_channels
            self.bytes_per_sample = 2
        elif config.data_format == "int32":
            self.sample_struct = '<' + 'i' * config.num_channels
            self.bytes_per_sample = 4
        elif config.data_format == "float32":
            self.sample_struct = '<' + 'f' * config.num_channels
            self.bytes_per_sample = 4
        else:  # int24
            self.sample_struct = None
            self.bytes_per_sample = 3
        
        # Calculate scaling factor (ADC counts -> output units)
        voltage_per_count = config.vref / (config.adc_max * config.gain)
        if config.output_unit == "microvolts":
            self.scale = voltage_per_count * 1e6
        elif config.output_unit == "millivolts":
            self.scale = voltage_per_count * 1e3
        else:
            self.scale = voltage_per_count
    
    def parse(self, packet: bytes) -> Optional[np.ndarray]:
        """Parse packet and return array of channel values in output units"""
        try:
            # Skip header
            offset = len(self.config.header_bytes)
            
            # Skip timestamp if present
            if self.config.has_timestamp:
                offset += 4
            
            # Extract sample data
            data_bytes = packet[offset:offset + self.config.num_channels * self.bytes_per_sample]
            
            if self.config.data_format == "int24":
                # Handle 24-bit samples manually
                samples = []
                for i in range(self.config.num_channels):
                    b = data_bytes[i*3:(i+1)*3]
                    val = int.from_bytes(b, 'little', signed=True)
                    samples.append(val)
                raw_values = np.array(samples, dtype=np.float32)
            else:
                raw_values = np.array(struct.unpack(self.sample_struct, data_bytes), dtype=np.float32)
            
            # Apply scaling (skip for float32 if already calibrated)
            if self.config.data_format != "float32":
                return raw_values * self.scale
            return raw_values
            
        except Exception as e:
            logging.warning(f"Parse error: {e}")
            return None


# =============================================================================
# LSL Streamer
# =============================================================================

class LSLStreamer:
    """Stream data to LSL"""
    
    def __init__(self, config: ICConfig):
        self.config = config
        self.outlet = None
        
    def start(self):
        """Create and start LSL outlet"""
        # Create stream info
        info = pylsl.StreamInfo(
            name=self.config.name,
            type='EEG',
            channel_count=self.config.num_channels,
            nominal_srate=self.config.sample_rate,
            channel_format='float32',
            source_id=self.config.device_id
        )
        
        # Add channel metadata
        channels = info.desc().append_child("channels")
        for i in range(self.config.num_channels):
            ch = channels.append_child("channel")
            ch.append_child_value("label", self.config.channel_names[i])
            ch.append_child_value("unit", self.config.output_unit)
            ch.append_child_value("type", self.config.channel_types[i])
        
        # Add device info
        device = info.desc().append_child("device")
        device.append_child_value("name", self.config.name)
        device.append_child_value("id", self.config.device_id)
        device.append_child_value("sample_rate", str(self.config.sample_rate))
        device.append_child_value("channels", str(self.config.num_channels))
        
        # Create outlet
        self.outlet = pylsl.StreamOutlet(info)
        logging.info(f"LSL outlet created: {self.config.name}")
        logging.info(f"  Channels: {self.config.num_channels}")
        logging.info(f"  Sample rate: {self.config.sample_rate} Hz")
        logging.info(f"  Channel names: {', '.join(self.config.channel_names)}")
    
    def push_sample(self, sample: np.ndarray, timestamp: float = None):
        """Push a single sample to LSL"""
        if self.outlet:
            if timestamp:
                self.outlet.push_sample(sample.tolist(), timestamp)
            else:
                self.outlet.push_sample(sample.tolist())
    
    def push_chunk(self, samples: np.ndarray, timestamps: List[float] = None):
        """Push multiple samples to LSL"""
        if self.outlet:
            if timestamps:
                self.outlet.push_chunk(samples.tolist(), timestamps)
            else:
                self.outlet.push_chunk(samples.tolist())
    
    def stop(self):
        """Stop LSL outlet"""
        self.outlet = None


# =============================================================================
# Main Connector Application
# =============================================================================

class ICConnector:
    """Main application that ties everything together"""
    
    def __init__(self, config: ICConfig):
        self.config = config
        self.running = False
        self.stats = {
            'samples': 0,
            'errors': 0,
            'start_time': 0
        }
        
        # Create components
        if config.interface == "serial":
            self.interface = SerialInterface(config)
        elif config.interface == "simulate":
            self.interface = SimulatedInterface(config)
        else:
            raise ValueError(f"Unknown interface: {config.interface}")
        
        self.parser = DataParser(config)
        self.streamer = LSLStreamer(config)
    
    def start(self):
        """Start the connector"""
        logging.info("Starting IC Connector...")
        
        # Connect to IC
        if not self.interface.connect():
            raise RuntimeError("Failed to connect to IC")
        
        # Start LSL
        self.streamer.start()
        
        # Start acquisition loop
        self.running = True
        self.stats['start_time'] = time.time()
        
        logging.info("Connector running. Press Ctrl+C to stop.")
        
        try:
            self._acquisition_loop()
        except KeyboardInterrupt:
            logging.info("Interrupted by user")
        finally:
            self.stop()
    
    def _acquisition_loop(self):
        """Main data acquisition loop"""
        last_stats_time = time.time()
        
        while self.running:
            # Read packet from IC
            packet = self.interface.read_packet()
            
            if packet:
                # Parse data
                sample = self.parser.parse(packet)
                
                if sample is not None:
                    # Send to LSL
                    self.streamer.push_sample(sample)
                    self.stats['samples'] += 1
                else:
                    self.stats['errors'] += 1
            
            # Print stats every 5 seconds
            now = time.time()
            if now - last_stats_time >= 5.0:
                elapsed = now - self.stats['start_time']
                rate = self.stats['samples'] / elapsed if elapsed > 0 else 0
                logging.info(f"Stats: {self.stats['samples']} samples, "
                           f"{rate:.1f} Hz actual, "
                           f"{self.stats['errors']} errors")
                last_stats_time = now
    
    def stop(self):
        """Stop the connector"""
        self.running = False
        self.interface.disconnect()
        self.streamer.stop()
        
        elapsed = time.time() - self.stats['start_time']
        logging.info(f"Stopped. Total: {self.stats['samples']} samples in {elapsed:.1f}s")


# =============================================================================
# Configuration Loading
# =============================================================================

def load_config_from_yaml(filepath: str) -> ICConfig:
    """Load configuration from YAML file"""
    if not HAS_YAML:
        raise ImportError("PyYAML not installed. Run: pip install pyyaml")
    
    with open(filepath, 'r') as f:
        data = yaml.safe_load(f)
    
    return ICConfig(**data)


def create_example_config(filepath: str):
    """Create an example YAML configuration file"""
    example = """# Custom IC Configuration
# ======================

# Device identification
name: "MyCustomIC"
device_id: "custom_ic_001"

# Communication settings
interface: "serial"     # serial, spi, usb, simulate
port: "COM3"           # Serial port (Windows) or /dev/ttyUSB0 (Linux)
baudrate: 2000000      # Baud rate

# Data format
num_channels: 8
sample_rate: 1000.0    # Hz
bits_per_sample: 16    # ADC resolution
data_format: "int16"   # int16, int24, int32, float32
byte_order: "little"   # little or big endian

# Voltage scaling
vref: 3.3              # Reference voltage (V)
gain: 1.0              # Amplifier gain
adc_max: 32767         # Max ADC value
output_unit: "microvolts"  # microvolts, millivolts, volts

# Channel names (optional)
channel_names:
  - "F3"
  - "F4"
  - "C3"
  - "C4"
  - "P3"
  - "P4"
  - "O1"
  - "O2"

channel_types:
  - "EEG"
  - "EEG"
  - "EEG"
  - "EEG"
  - "EEG"
  - "EEG"
  - "EEG"
  - "EEG"

# Protocol settings
header_bytes: !!binary |
  qlU=
# Explanation: This is base64 for bytes 0xAA, 0x55 (sync header)
# For hex 0xAA 0x55, the base64 is 'qlU='

has_timestamp: false   # Does IC send timestamps?
has_checksum: false    # Does packet have checksum?
"""
    with open(filepath, 'w') as f:
        f.write(example)
    print(f"Example config written to: {filepath}")


# =============================================================================
# Main Entry Point
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Custom IC to LSL Connector',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run with simulated data (test mode)
  python ic_to_lsl_connector.py --simulate
  
  # Connect to serial port
  python ic_to_lsl_connector.py --port COM3 --baudrate 2000000
  
  # Use configuration file
  python ic_to_lsl_connector.py --config my_device.yaml
  
  # Create example config file
  python ic_to_lsl_connector.py --create-config my_device.yaml
"""
    )
    
    parser.add_argument('--config', '-c', help='YAML configuration file')
    parser.add_argument('--create-config', metavar='FILE', help='Create example config file')
    parser.add_argument('--simulate', '-s', action='store_true', help='Use simulated data')
    parser.add_argument('--port', '-p', default='COM3', help='Serial port')
    parser.add_argument('--baudrate', '-b', type=int, default=2000000, help='Baud rate')
    parser.add_argument('--channels', '-n', type=int, default=8, help='Number of channels')
    parser.add_argument('--srate', '-r', type=float, default=1000.0, help='Sample rate (Hz)')
    parser.add_argument('--name', default='CustomIC', help='Device name')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    # Setup logging
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format='%(asctime)s [%(levelname)s] %(message)s',
        datefmt='%H:%M:%S'
    )
    
    # Create example config if requested
    if args.create_config:
        create_example_config(args.create_config)
        return
    
    # Load or create configuration
    if args.config:
        config = load_config_from_yaml(args.config)
    else:
        config = ICConfig(
            name=args.name,
            interface="simulate" if args.simulate else "serial",
            port=args.port,
            baudrate=args.baudrate,
            num_channels=args.channels,
            sample_rate=args.srate
        )
    
    # Force simulate mode if --simulate flag is set
    if args.simulate:
        config.interface = "simulate"
    
    # Create and run connector
    connector = ICConnector(config)
    connector.start()


if __name__ == '__main__':
    main()
