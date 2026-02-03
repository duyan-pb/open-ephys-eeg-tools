#!/usr/bin/env python3
"""
BioSerial-Pro Protocol Test Utility

This script tests the BioSerial-Pro protocol by:
1. Simulating a Teensy device (for testing without hardware)
2. Reading and parsing packets from a real device
3. Validating packet structure and checksum

Usage:
    python bioserial_pro_test.py --mode simulate    # Simulate device
    python bioserial_pro_test.py --mode read --port COM5  # Read from device
    python bioserial_pro_test.py --mode parse       # Parse example packet
"""

import argparse
import struct
import time
import numpy as np
from dataclasses import dataclass
from typing import Optional, List
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
# Protocol Constants (Must match firmware and plugin!)
# =============================================================================

HEADER_BYTE_1 = 0xA5
HEADER_BYTE_2 = 0x5A
FOOTER_BYTE_1 = 0xC0
FOOTER_BYTE_2 = 0xC0
PACKET_SIZE = 56
BAUD_RATE = 2000000

NUM_EEG_CHANNELS = 5
NUM_ACCEL_CHANNELS = 3
NUM_PPG_CHANNELS = 3
NUM_AUX_CHANNELS = 9  # AccelX/Y/Z, PPG_Red/IR/Green, Temp, Battery, Sync
EEG_SAMPLE_RATE = 1000  # Hz

# Byte offsets within 56-byte packet
OFFSET_HEADER = 0
OFFSET_TIMESTAMP = 2
OFFSET_MARKER = 6
OFFSET_EEG = 7           # 5ch × 3B = 15B
OFFSET_ACCEL = 22        # 3ch × 2B = 6B
OFFSET_PPG = 28          # 3ch × 6B = 18B
OFFSET_TEMP = 46         # 1ch × 2B = 2B
OFFSET_BATTERY = 48      # 1ch × 2B = 2B
OFFSET_SYNC = 50         # 1ch × 2B = 2B
OFFSET_COUNTER = 52
OFFSET_CHECKSUM = 53
OFFSET_FOOTER = 54

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
    eeg: List[float]      # µV (5 channels)
    accel: List[float]    # Raw (3 channels: X, Y, Z)
    ppg: List[int]        # Raw (3 channels: Red, IR, Green)
    temperature: int      # centi-degrees C
    battery: int          # mV
    sync: int             # Raw sync value
    counter: int
    checksum_valid: bool
    raw_bytes: bytes

# =============================================================================
# Protocol Functions
# =============================================================================

def compute_checksum(data: bytes) -> int:
    """Compute XOR checksum of bytes 0-52"""
    checksum = 0
    for i in range(53):  # XOR bytes 0 through 52
        checksum ^= data[i]
    return checksum


def pack_int24_be(val: int) -> bytes:
    """Pack a 24-bit signed integer as 3 bytes big-endian"""
    if val < 0:
        val = val & 0xFFFFFF
    return bytes([(val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF])


def pack_int16_be(val: int) -> bytes:
    """Pack a 16-bit signed integer as 2 bytes big-endian"""
    return struct.pack('>h', val)


def pack_uint16_be(val: int) -> bytes:
    """Pack a 16-bit unsigned integer as 2 bytes big-endian"""
    return struct.pack('>H', val)


def pack_int48_be(val: int) -> bytes:
    """Pack a 48-bit integer as 6 bytes big-endian"""
    return bytes([
        (val >> 40) & 0xFF,
        (val >> 32) & 0xFF,
        (val >> 24) & 0xFF,
        (val >> 16) & 0xFF,
        (val >> 8) & 0xFF,
        val & 0xFF
    ])


def unpack_int24_be(data: bytes) -> int:
    """Unpack 3 bytes big-endian to 24-bit signed integer"""
    val = (data[0] << 16) | (data[1] << 8) | data[2]
    if val & 0x800000:  # Sign extend
        val |= ~0xFFFFFF
    return val


def unpack_int16_be(data: bytes) -> int:
    """Unpack 2 bytes big-endian to 16-bit signed integer"""
    return struct.unpack('>h', data)[0]


def unpack_uint16_be(data: bytes) -> int:
    """Unpack 2 bytes big-endian to 16-bit unsigned integer"""
    return struct.unpack('>H', data)[0]


def unpack_int48_be(data: bytes) -> int:
    """Unpack 6 bytes big-endian to 48-bit signed integer"""
    val = ((data[0] << 40) | (data[1] << 32) | (data[2] << 24) |
           (data[3] << 16) | (data[4] << 8) | data[5])
    if val & 0x800000000000:  # Sign extend from 48-bit
        val |= ~0xFFFFFFFFFFFF
    return val


def build_packet(timestamp_us: int, marker: int, eeg_raw: List[int],
                 accel_raw: List[int], ppg_raw: List[int],
                 temp: int, battery: int, sync: int, counter: int) -> bytes:
    """Build a BioSerial-Pro 56-byte packet from raw values"""
    packet = bytearray(PACKET_SIZE)
    
    # Header (bytes 0-1)
    packet[0] = HEADER_BYTE_1
    packet[1] = HEADER_BYTE_2
    
    # Timestamp (bytes 2-5, big-endian)
    packet[2:6] = struct.pack('>I', timestamp_us & 0xFFFFFFFF)
    
    # Marker (byte 6)
    packet[6] = marker & 0xFF
    
    # EEG data (bytes 7-21: 5 channels × 3 bytes)
    for i, val in enumerate(eeg_raw[:NUM_EEG_CHANNELS]):
        packet[OFFSET_EEG + i*3:OFFSET_EEG + i*3 + 3] = pack_int24_be(val)
    
    # Accelerometer data (bytes 22-27: 3 channels × 2 bytes)
    for i, val in enumerate(accel_raw[:NUM_ACCEL_CHANNELS]):
        packet[OFFSET_ACCEL + i*2:OFFSET_ACCEL + i*2 + 2] = pack_int16_be(val)
    
    # PPG data (bytes 28-45: 3 channels × 6 bytes)
    for i, val in enumerate(ppg_raw[:NUM_PPG_CHANNELS]):
        packet[OFFSET_PPG + i*6:OFFSET_PPG + i*6 + 6] = pack_int48_be(val)
    
    # Temperature (bytes 46-47)
    packet[OFFSET_TEMP:OFFSET_TEMP + 2] = pack_int16_be(temp)
    
    # Battery (bytes 48-49)
    packet[OFFSET_BATTERY:OFFSET_BATTERY + 2] = pack_uint16_be(battery)
    
    # Sync (bytes 50-51)
    packet[OFFSET_SYNC:OFFSET_SYNC + 2] = pack_uint16_be(sync)
    
    # Counter (byte 52)
    packet[OFFSET_COUNTER] = counter & 0xFF
    
    # Checksum (byte 53) - XOR of bytes 0-52
    packet[OFFSET_CHECKSUM] = compute_checksum(packet)
    
    # Footer (bytes 54-55)
    packet[OFFSET_FOOTER] = FOOTER_BYTE_1
    packet[OFFSET_FOOTER + 1] = FOOTER_BYTE_2
    
    return bytes(packet)


def parse_packet(data: bytes) -> Optional[BioSerialPacket]:
    """Parse a BioSerial-Pro 56-byte packet"""
    if len(data) != PACKET_SIZE:
        return None
    
    # Check header
    if data[0] != HEADER_BYTE_1 or data[1] != HEADER_BYTE_2:
        return None
    
    # Check footer
    if data[OFFSET_FOOTER] != FOOTER_BYTE_1 or data[OFFSET_FOOTER + 1] != FOOTER_BYTE_2:
        return None
    
    # Validate checksum
    expected_checksum = compute_checksum(data)
    checksum_valid = (data[OFFSET_CHECKSUM] == expected_checksum)
    
    # Parse timestamp (big-endian)
    timestamp_us = struct.unpack('>I', data[OFFSET_TIMESTAMP:OFFSET_TIMESTAMP+4])[0]
    
    # Parse marker
    marker = data[OFFSET_MARKER]
    
    # Parse EEG channels (5 × 24-bit signed, big-endian)
    eeg = []
    for i in range(NUM_EEG_CHANNELS):
        offset = OFFSET_EEG + i * 3
        val = unpack_int24_be(data[offset:offset+3])
        eeg.append(val * EEG_SCALE_UV)
    
    # Parse accelerometer (3 × 16-bit signed, big-endian)
    accel = []
    for i in range(NUM_ACCEL_CHANNELS):
        offset = OFFSET_ACCEL + i * 2
        accel.append(unpack_int16_be(data[offset:offset+2]))
    
    # Parse PPG (3 × 48-bit signed, big-endian)
    ppg = []
    for i in range(NUM_PPG_CHANNELS):
        offset = OFFSET_PPG + i * 6
        ppg.append(unpack_int48_be(data[offset:offset+6]))
    
    # Parse temperature (16-bit signed)
    temperature = unpack_int16_be(data[OFFSET_TEMP:OFFSET_TEMP+2])
    
    # Parse battery (16-bit unsigned)
    battery = unpack_uint16_be(data[OFFSET_BATTERY:OFFSET_BATTERY+2])
    
    # Parse sync (16-bit unsigned)
    sync = unpack_uint16_be(data[OFFSET_SYNC:OFFSET_SYNC+2])
    
    # Parse counter
    counter = data[OFFSET_COUNTER]
    
    return BioSerialPacket(
        timestamp_us=timestamp_us,
        marker=marker,
        eeg=eeg,
        accel=accel,
        ppg=ppg,
        temperature=temperature,
        battery=battery,
        sync=sync,
        counter=counter,
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
    
    def __init__(self, port: str, baudrate: int = BAUD_RATE):
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
        
        # Generate sine waves for EEG (3, 6, 10, 20, 40 Hz)
        eeg_freqs = [3.0, 6.0, 10.0, 20.0, 40.0]
        eeg_raw = []
        for ch, freq in enumerate(eeg_freqs):
            amplitude = int(100000 * (1.0 + ch * 0.1))  # ~100µV
            val = int(amplitude * np.sin(2 * np.pi * freq * self.phase / EEG_SAMPLE_RATE))
            eeg_raw.append(val)
        
        # Generate accelerometer data (gentle motion)
        accel_raw = [
            int(100 * np.sin(2 * np.pi * 0.2 * self.phase / EEG_SAMPLE_RATE)),  # X
            int(50 * np.sin(2 * np.pi * 0.15 * self.phase / EEG_SAMPLE_RATE)),  # Y
            int(16384 + 30 * np.sin(2 * np.pi * 0.25 * self.phase / EEG_SAMPLE_RATE))  # Z (gravity)
        ]
        
        # Generate PPG heartbeat at 72 BPM (1.2 Hz)
        heart_phase = (self.phase / EEG_SAMPLE_RATE * 1.2) % 1.0
        if heart_phase < 0.15:
            ppg_pulse = np.sin(np.pi * heart_phase / 0.15)
        elif heart_phase < 0.4:
            ppg_pulse = np.cos(np.pi * (heart_phase - 0.15) / 0.5) * 0.8
        else:
            ppg_pulse = 0
        
        ppg_raw = [
            int(100000000000 + ppg_pulse * 5000000000),   # Red
            int(120000000000 + ppg_pulse * 6000000000),   # IR
            int(80000000000 + ppg_pulse * 4000000000)     # Green
        ]
        
        # Temperature: slow drift around 36.5°C
        temp = int(3650 + 10 * np.sin(2 * np.pi * 0.001 * self.phase / EEG_SAMPLE_RATE))
        
        # Battery: slow discharge
        battery = max(3700, 4200 - int(self.phase * 0.001))
        
        # Sync: periodic pulse
        sync = 1 if (self.counter % 1000) < 100 else 0
        
        packet = build_packet(timestamp_us, marker, eeg_raw, accel_raw, ppg_raw,
                              temp, battery, sync, self.counter)
        
        self.counter = (self.counter + 1) & 0xFF
        self.phase += 1.0
        
        return packet
    
    def run(self):
        """Run the simulator, outputting packets to serial port"""
        print(f"Starting simulator on {self.port} at {self.baudrate} baud")
        print(f"Protocol: 56-byte packets @ 1kHz")
        print(f"Press Ctrl+C to stop\n")
        
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=0.001)
        except serial.SerialException as e:
            print(f"Failed to open port: {e}")
            return
        
        self.running = True
        interval = 1.0 / EEG_SAMPLE_RATE
        packets_sent = 0
        
        try:
            while self.running:
                start = time.perf_counter()
                
                packet = self.generate_packet()
                ser.write(packet)
                packets_sent += 1
                
                if packets_sent % 1000 == 0:
                    print(f"Sent {packets_sent} packets")
                
                # Precise timing
                elapsed = time.perf_counter() - start
                if elapsed < interval:
                    time.sleep(interval - elapsed)
                    
        except KeyboardInterrupt:
            print(f"\nSimulator stopped. Sent {packets_sent} packets.")
        finally:
            ser.close()
            self.running = False


# =============================================================================
# Device Reader
# =============================================================================

class BioSerialReader:
    """Reads and parses packets from a BioSerial-Pro device"""
    
    def __init__(self, port: str, baudrate: int = BAUD_RATE):
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
        print(f"Protocol: 56-byte packets @ 1kHz\n")
        
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
                data = ser.read(4096)
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
                                  f"PPG[0]={packet.ppg[0]}, "
                                  f"Temp={packet.temperature/100:.1f}°C, "
                                  f"Batt={packet.battery}mV")
                
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
    print("=== Testing BioSerial-Pro 56-byte Packet Build/Parse ===\n")
    
    # Build a test packet
    timestamp = 1000000  # 1 second
    marker = 0x01
    eeg_raw = [100000, -50000, 25000, -12500, 6250]  # Raw 24-bit values
    accel_raw = [100, -50, 16384]  # Raw 16-bit values (X, Y, Z)
    ppg_raw = [100000000000, 120000000000, 80000000000]  # Raw 48-bit values
    temp = 3650  # 36.50°C
    battery = 4100  # mV
    sync = 1
    counter = 42
    
    packet = build_packet(timestamp, marker, eeg_raw, accel_raw, ppg_raw,
                          temp, battery, sync, counter)
    
    print(f"Packet size: {len(packet)} bytes (expected: 56)")
    print(f"Header: 0x{packet[0]:02X} 0x{packet[1]:02X} (expected: 0xA5 0x5A)")
    print(f"Footer: 0x{packet[54]:02X} 0x{packet[55]:02X} (expected: 0xC0 0xC0)")
    print(f"Packet hex:\n  {packet[:28].hex()}")
    print(f"  {packet[28:].hex()}")
    print()
    
    # Parse it back
    parsed = parse_packet(packet)
    if parsed:
        print(f"✓ Timestamp: {parsed.timestamp_us} µs")
        print(f"✓ Marker: 0x{parsed.marker:02X}")
        print(f"✓ EEG channels (µV): {[f'{v:.2f}' for v in parsed.eeg]}")
        print(f"✓ Accel (raw): {parsed.accel}")
        print(f"✓ PPG (raw): {parsed.ppg}")
        print(f"✓ Temperature: {parsed.temperature/100:.2f}°C")
        print(f"✓ Battery: {parsed.battery} mV")
        print(f"✓ Sync: {parsed.sync}")
        print(f"✓ Counter: {parsed.counter}")
        print(f"✓ Checksum valid: {parsed.checksum_valid}")
    else:
        print("✗ Failed to parse packet!")
    
    print()
    
    # Verify round-trip
    print("=== Verifying Round-trip ===")
    errors = []
    if parsed.timestamp_us != timestamp:
        errors.append(f"Timestamp mismatch: {parsed.timestamp_us} != {timestamp}")
    if parsed.marker != marker:
        errors.append(f"Marker mismatch: {parsed.marker} != {marker}")
    if parsed.counter != counter:
        errors.append(f"Counter mismatch: {parsed.counter} != {counter}")
    if parsed.temperature != temp:
        errors.append(f"Temperature mismatch: {parsed.temperature} != {temp}")
    if parsed.battery != battery:
        errors.append(f"Battery mismatch: {parsed.battery} != {battery}")
    
    if errors:
        print("✗ Round-trip errors:")
        for e in errors:
            print(f"  - {e}")
    else:
        print("✓ All values match after round-trip!")


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
    parser.add_argument('--port', default='COM5', help='Serial port')
    parser.add_argument('--baud', type=int, default=BAUD_RATE, help='Baud rate')
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
