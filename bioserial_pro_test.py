#!/usr/bin/env python3
"""
BioSerial-Pro Protocol Test Utility

This script tests the BioSerial-Pro protocol by:
1. Simulating a Teensy device (for testing without hardware)
2. Reading and parsing packets from a real device
3. Validating packet structure and checksum

Usage:
    python bioserial_pro_test.py --mode simulate    # Simulate device
    python bioserial_pro_test.py --mode read --port COM3  # Read from device
    python bioserial_pro_test.py --mode parse       # Parse example packet
"""

import argparse
import struct
import time
import threading
import numpy as np
from dataclasses import dataclass
from typing import Optional, List, Tuple
from collections import deque

# Try to import serial, but make it optional for parse-only mode
try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False
    print("Warning: pyserial not installed. Serial port functions unavailable.")

# =============================================================================
# Protocol Constants
# =============================================================================

HEADER_BYTE_1 = 0xA5
HEADER_BYTE_2 = 0x5A
FOOTER_BYTE_1 = 0xC0
FOOTER_BYTE_2 = 0xC0
PACKET_SIZE = 32

NUM_EEG_CHANNELS = 5
NUM_AUX_CHANNELS = 3
EEG_SAMPLE_RATE = 1000  # Hz
ACCEL_SAMPLE_RATE = 200  # Hz

# Byte offsets
OFFSET_HEADER = 0
OFFSET_TIMESTAMP = 2
OFFSET_MARKER = 6
OFFSET_EEG = 7
OFFSET_AUX = 22
OFFSET_COUNTER = 28
OFFSET_CHECKSUM = 29
OFFSET_FOOTER = 30

# Scale factors
EEG_SCALE_UV = 0.0223517  # 24-bit to µV (ADS1299 @ Gain 24)
ACCEL_SCALE_G = 0.0039    # ADXL345 @ ±16g

# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class BioSerialPacket:
    """Parsed BioSerial-Pro packet"""
    timestamp_us: int
    marker: int
    eeg: List[float]      # µV
    aux: List[float]      # g or health values
    counter: int
    is_health_packet: bool
    checksum_valid: bool
    raw_bytes: bytes

# =============================================================================
# Protocol Functions
# =============================================================================

def compute_checksum(data: bytes) -> int:
    """Compute XOR checksum of data"""
    checksum = 0
    for byte in data:
        checksum ^= byte
    return checksum


def build_packet(timestamp_us: int, marker: int, eeg_raw: List[int], 
                 aux_raw: List[int], counter: int) -> bytes:
    """Build a BioSerial-Pro packet from raw values"""
    packet = bytearray(PACKET_SIZE)
    
    # Header
    packet[0] = HEADER_BYTE_1
    packet[1] = HEADER_BYTE_2
    
    # Timestamp (4 bytes, big-endian)
    packet[2:6] = struct.pack('>I', timestamp_us & 0xFFFFFFFF)
    
    # Marker
    packet[6] = marker & 0xFF
    
    # EEG data (5 channels × 3 bytes, big-endian 24-bit)
    for i, val in enumerate(eeg_raw[:NUM_EEG_CHANNELS]):
        # Convert to 24-bit signed
        if val < 0:
            val = val & 0xFFFFFF
        packet[OFFSET_EEG + i*3] = (val >> 16) & 0xFF
        packet[OFFSET_EEG + i*3 + 1] = (val >> 8) & 0xFF
        packet[OFFSET_EEG + i*3 + 2] = val & 0xFF
    
    # Aux data (3 channels × 2 bytes, big-endian 16-bit)
    for i, val in enumerate(aux_raw[:NUM_AUX_CHANNELS]):
        if val < 0:
            val = val & 0xFFFF
        packet[OFFSET_AUX + i*2] = (val >> 8) & 0xFF
        packet[OFFSET_AUX + i*2 + 1] = val & 0xFF
    
    # Counter
    packet[OFFSET_COUNTER] = counter & 0xFF
    
    # Checksum (XOR of bytes 0-28)
    packet[OFFSET_CHECKSUM] = compute_checksum(packet[:OFFSET_CHECKSUM])
    
    # Footer
    packet[OFFSET_FOOTER] = FOOTER_BYTE_1
    packet[OFFSET_FOOTER + 1] = FOOTER_BYTE_2
    
    return bytes(packet)


def parse_packet(data: bytes) -> Optional[BioSerialPacket]:
    """Parse a BioSerial-Pro packet"""
    if len(data) != PACKET_SIZE:
        return None
    
    # Check header
    if data[0] != HEADER_BYTE_1 or data[1] != HEADER_BYTE_2:
        return None
    
    # Check footer
    if data[OFFSET_FOOTER] != FOOTER_BYTE_1 or data[OFFSET_FOOTER + 1] != FOOTER_BYTE_2:
        return None
    
    # Validate checksum
    expected_checksum = compute_checksum(data[:OFFSET_CHECKSUM])
    checksum_valid = (data[OFFSET_CHECKSUM] == expected_checksum)
    
    # Parse timestamp (big-endian)
    timestamp_us = struct.unpack('>I', data[OFFSET_TIMESTAMP:OFFSET_TIMESTAMP+4])[0]
    
    # Parse marker
    marker = data[OFFSET_MARKER]
    
    # Parse EEG channels (24-bit signed, big-endian)
    eeg_raw = []
    for i in range(NUM_EEG_CHANNELS):
        offset = OFFSET_EEG + i * 3
        val = (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]
        # Sign extend
        if val & 0x800000:
            val |= ~0xFFFFFF
        eeg_raw.append(val * EEG_SCALE_UV)
    
    # Parse aux channels (16-bit signed, big-endian)
    counter = data[OFFSET_COUNTER]
    is_health_packet = (counter == 0)
    
    aux = []
    for i in range(NUM_AUX_CHANNELS):
        offset = OFFSET_AUX + i * 2
        val = struct.unpack('>h', data[offset:offset+2])[0]
        if is_health_packet:
            aux.append(float(val))  # Raw health values
        else:
            aux.append(val * ACCEL_SCALE_G)  # Accelerometer in g
    
    return BioSerialPacket(
        timestamp_us=timestamp_us,
        marker=marker,
        eeg=eeg_raw,
        aux=aux,
        counter=counter,
        is_health_packet=is_health_packet,
        checksum_valid=checksum_valid,
        raw_bytes=data
    )


# =============================================================================
# Packet Reassembly Buffer
# =============================================================================

class ReassemblyBuffer:
    """Buffer for reassembling fragmented USB packets"""
    
    def __init__(self, max_size: int = 8192):
        self.buffer = deque(maxlen=max_size)
    
    def push(self, data: bytes):
        """Add data to buffer"""
        for byte in data:
            self.buffer.append(byte)
    
    def find_sync(self) -> int:
        """Find sync bytes in buffer"""
        buf_list = list(self.buffer)
        for i in range(len(buf_list) - PACKET_SIZE + 1):
            if buf_list[i] == HEADER_BYTE_1 and buf_list[i + 1] == HEADER_BYTE_2:
                # Check footer too
                if (buf_list[i + OFFSET_FOOTER] == FOOTER_BYTE_1 and 
                    buf_list[i + OFFSET_FOOTER + 1] == FOOTER_BYTE_2):
                    return i
        return -1
    
    def try_extract_packet(self) -> Optional[bytes]:
        """Try to extract a complete packet"""
        if len(self.buffer) < PACKET_SIZE:
            return None
        
        sync_pos = self.find_sync()
        if sync_pos < 0:
            # No valid packet found, keep last byte
            while len(self.buffer) > 1:
                self.buffer.popleft()
            return None
        
        # Remove bytes before sync
        for _ in range(sync_pos):
            self.buffer.popleft()
        
        if len(self.buffer) < PACKET_SIZE:
            return None
        
        # Extract packet
        packet = bytes([self.buffer[i] for i in range(PACKET_SIZE)])
        
        # Remove packet from buffer
        for _ in range(PACKET_SIZE):
            self.buffer.popleft()
        
        return packet


# =============================================================================
# Device Simulator
# =============================================================================

class BioSerialSimulator:
    """Simulates a BioSerial-Pro device for testing"""
    
    def __init__(self, port: str, baudrate: int = 12000000):
        if not SERIAL_AVAILABLE:
            raise RuntimeError("pyserial not installed")
        
        self.port = port
        self.baudrate = baudrate
        self.running = False
        self.counter = 0
        self.phase = 0.0
        self.start_time = None
        
    def generate_packet(self) -> bytes:
        """Generate a simulated packet"""
        if self.start_time is None:
            self.start_time = time.time()
        
        timestamp_us = int((time.time() - self.start_time) * 1_000_000) & 0xFFFFFFFF
        marker = 0
        
        # Generate 10Hz sine waves for EEG
        eeg_raw = []
        for ch in range(NUM_EEG_CHANNELS):
            phase = self.phase + (ch * np.pi / 5.0)
            amplitude = int(100000 * (1.0 + ch * 0.1))  # ~100µV
            val = int(amplitude * np.sin(2 * np.pi * 10.0 * phase / EEG_SAMPLE_RATE))
            eeg_raw.append(val)
        
        # Generate accelerometer data
        if self.counter == 0:
            # Health packet
            aux_raw = [3700, 250, 1000]  # Battery mV, Temp 0.1°C, Impedance
        else:
            aux_raw = [10, 15, 256]  # Accel XYZ (raw, ~1g on Z)
        
        packet = build_packet(timestamp_us, marker, eeg_raw, aux_raw, self.counter)
        
        self.counter = (self.counter + 1) & 0xFF
        self.phase += 1.0
        
        return packet
    
    def run(self):
        """Run the simulator, outputting packets to serial port"""
        print(f"Starting simulator on {self.port} at {self.baudrate} baud")
        
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=0.001)
        except serial.SerialException as e:
            print(f"Failed to open port: {e}")
            return
        
        self.running = True
        interval = 1.0 / EEG_SAMPLE_RATE
        
        try:
            while self.running:
                start = time.perf_counter()
                
                packet = self.generate_packet()
                ser.write(packet)
                
                # Precise timing
                elapsed = time.perf_counter() - start
                if elapsed < interval:
                    time.sleep(interval - elapsed)
                    
        except KeyboardInterrupt:
            print("\nSimulator stopped")
        finally:
            ser.close()
            self.running = False


# =============================================================================
# Device Reader
# =============================================================================

class BioSerialReader:
    """Reads and parses packets from a BioSerial-Pro device"""
    
    def __init__(self, port: str, baudrate: int = 12000000):
        if not SERIAL_AVAILABLE:
            raise RuntimeError("pyserial not installed")
        
        self.port = port
        self.baudrate = baudrate
        self.running = False
        self.buffer = ReassemblyBuffer()
        
        # Statistics
        self.packets_received = 0
        self.checksum_errors = 0
        self.dropped_packets = 0
        self.last_counter = None
    
    def run(self, duration: float = 10.0, verbose: bool = True):
        """Read packets for specified duration"""
        print(f"Reading from {self.port} at {self.baudrate} baud for {duration}s")
        
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=0.01)
        except serial.SerialException as e:
            print(f"Failed to open port: {e}")
            return
        
        self.running = True
        start_time = time.time()
        last_print = start_time
        
        try:
            while self.running and (time.time() - start_time) < duration:
                # Read available data
                data = ser.read(1024)
                if data:
                    self.buffer.push(data)
                
                # Extract packets
                while True:
                    packet_data = self.buffer.try_extract_packet()
                    if packet_data is None:
                        break
                    
                    packet = parse_packet(packet_data)
                    if packet:
                        self.packets_received += 1
                        
                        if not packet.checksum_valid:
                            self.checksum_errors += 1
                        
                        # Check for dropped packets
                        if self.last_counter is not None:
                            expected = (self.last_counter + 1) & 0xFF
                            if packet.counter != expected:
                                dropped = (packet.counter - expected) & 0xFF
                                self.dropped_packets += dropped
                        
                        self.last_counter = packet.counter
                        
                        if verbose and self.packets_received % 100 == 0:
                            print(f"Packet {self.packets_received}: "
                                  f"ts={packet.timestamp_us}µs, "
                                  f"EEG[0]={packet.eeg[0]:.2f}µV, "
                                  f"Counter={packet.counter}")
                
                # Print stats every second
                now = time.time()
                if now - last_print >= 1.0:
                    rate = self.packets_received / (now - start_time)
                    print(f"[{now - start_time:.1f}s] "
                          f"Packets: {self.packets_received}, "
                          f"Rate: {rate:.1f} Hz, "
                          f"Dropped: {self.dropped_packets}, "
                          f"CRC Errors: {self.checksum_errors}")
                    last_print = now
                    
        except KeyboardInterrupt:
            print("\nReader stopped")
        finally:
            ser.close()
            self.running = False
        
        # Final stats
        elapsed = time.time() - start_time
        print(f"\n=== Final Statistics ===")
        print(f"Duration: {elapsed:.2f}s")
        print(f"Packets received: {self.packets_received}")
        print(f"Average rate: {self.packets_received / elapsed:.1f} Hz")
        print(f"Dropped packets: {self.dropped_packets}")
        print(f"Checksum errors: {self.checksum_errors}")


# =============================================================================
# Test Functions
# =============================================================================

def test_packet_build_parse():
    """Test packet building and parsing"""
    print("=== Testing Packet Build/Parse ===\n")
    
    # Build a test packet
    timestamp = 1000000  # 1 second
    marker = 0x01
    eeg_raw = [100000, -50000, 25000, -12500, 6250]  # Raw 24-bit values
    aux_raw = [256, 128, -64]  # Raw 16-bit values
    counter = 42
    
    packet = build_packet(timestamp, marker, eeg_raw, aux_raw, counter)
    
    print(f"Packet size: {len(packet)} bytes")
    print(f"Packet hex: {packet.hex()}")
    print()
    
    # Parse it back
    parsed = parse_packet(packet)
    if parsed:
        print(f"Timestamp: {parsed.timestamp_us} µs")
        print(f"Marker: 0x{parsed.marker:02X}")
        print(f"EEG channels (µV): {[f'{v:.2f}' for v in parsed.eeg]}")
        print(f"Aux channels: {[f'{v:.4f}' for v in parsed.aux]}")
        print(f"Counter: {parsed.counter}")
        print(f"Is health packet: {parsed.is_health_packet}")
        print(f"Checksum valid: {parsed.checksum_valid}")
    else:
        print("Failed to parse packet!")
    
    print()
    
    # Test health packet (counter = 0)
    print("=== Testing Health Packet ===\n")
    health_packet = build_packet(2000000, 0, eeg_raw, [3700, 250, 1000], 0)
    parsed_health = parse_packet(health_packet)
    if parsed_health:
        print(f"Is health packet: {parsed_health.is_health_packet}")
        print(f"Health values: Battery={parsed_health.aux[0]:.0f}mV, "
              f"Temp={parsed_health.aux[1]/10:.1f}°C, "
              f"Impedance={parsed_health.aux[2]:.0f}")


def list_ports():
    """List available serial ports"""
    if not SERIAL_AVAILABLE:
        print("pyserial not installed")
        return
    
    print("=== Available Serial Ports ===\n")
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found")
    else:
        for port in ports:
            print(f"  {port.device}: {port.description}")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="BioSerial-Pro Protocol Test Utility")
    parser.add_argument('--mode', choices=['simulate', 'read', 'parse', 'list'],
                       default='parse', help='Operation mode')
    parser.add_argument('--port', default='COM3', help='Serial port')
    parser.add_argument('--baud', type=int, default=12000000, help='Baud rate')
    parser.add_argument('--duration', type=float, default=10.0, help='Read duration (seconds)')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    if args.mode == 'list':
        list_ports()
    elif args.mode == 'parse':
        test_packet_build_parse()
    elif args.mode == 'simulate':
        sim = BioSerialSimulator(args.port, args.baud)
        sim.run()
    elif args.mode == 'read':
        reader = BioSerialReader(args.port, args.baud)
        reader.run(args.duration, args.verbose)


if __name__ == '__main__':
    main()
