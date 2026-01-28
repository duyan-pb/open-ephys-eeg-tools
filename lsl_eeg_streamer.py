"""
LSL EEG Data Streamer for Open Ephys

This script streams synthetic EEG data (or reads from a file) via Lab Streaming Layer,
allowing you to visualize it in Open Ephys using the LSL Inlet plugin.

Usage:
    python lsl_eeg_streamer.py                    # Stream synthetic EEG
    python lsl_eeg_streamer.py --file data.npy    # Stream from numpy file
    python lsl_eeg_streamer.py --channels 8 --srate 1000  # Custom config

Requirements:
    pip install pylsl numpy
"""

import argparse
import time
import numpy as np

try:
    from pylsl import StreamInfo, StreamOutlet, local_clock
except ImportError:
    print("Error: pylsl not installed. Install with: pip install pylsl")
    exit(1)


def generate_synthetic_eeg(n_channels: int, n_samples: int, srate: float) -> np.ndarray:
    """
    Generate synthetic EEG-like data with multiple frequency components.
    
    Args:
        n_channels: Number of EEG channels
        n_samples: Number of samples to generate
        srate: Sampling rate in Hz
    
    Returns:
        numpy array of shape (n_samples, n_channels)
    """
    t = np.arange(n_samples) / srate
    data = np.zeros((n_samples, n_channels))
    
    for ch in range(n_channels):
        # Mix of different frequency bands typical in EEG
        # Delta (0.5-4 Hz)
        delta = 50 * np.sin(2 * np.pi * (1 + ch * 0.1) * t)
        # Theta (4-8 Hz)
        theta = 30 * np.sin(2 * np.pi * (6 + ch * 0.2) * t)
        # Alpha (8-13 Hz)
        alpha = 40 * np.sin(2 * np.pi * (10 + ch * 0.3) * t)
        # Beta (13-30 Hz)
        beta = 20 * np.sin(2 * np.pi * (20 + ch * 0.5) * t)
        # Add some noise
        noise = 10 * np.random.randn(n_samples)
        
        data[:, ch] = delta + theta + alpha + beta + noise
    
    return data.astype(np.float32)


def create_lsl_outlet(stream_name: str, stream_type: str, n_channels: int, 
                      srate: float, channel_names: list = None) -> StreamOutlet:
    """
    Create an LSL outlet for streaming EEG data.
    
    Args:
        stream_name: Name of the LSL stream
        stream_type: Type of stream (e.g., 'EEG')
        n_channels: Number of channels
        srate: Sampling rate in Hz
        channel_names: Optional list of channel names
    
    Returns:
        LSL StreamOutlet object
    """
    # Create stream info
    info = StreamInfo(
        name=stream_name,
        type=stream_type,
        channel_count=n_channels,
        nominal_srate=srate,
        channel_format='float32',
        source_id=f'PythonEEGStreamer_{stream_name}'
    )
    
    # Add channel metadata
    channels = info.desc().append_child("channels")
    for i in range(n_channels):
        ch = channels.append_child("channel")
        name = channel_names[i] if channel_names and i < len(channel_names) else f"Ch{i+1}"
        ch.append_child_value("label", name)
        ch.append_child_value("unit", "microvolts")
        ch.append_child_value("type", "EEG")
    
    # Add acquisition info
    acq = info.desc().append_child("acquisition")
    acq.append_child_value("manufacturer", "Python LSL Streamer")
    acq.append_child_value("model", "Synthetic EEG Generator")
    
    # Create outlet with chunk size for efficiency
    chunk_size = int(srate / 20)  # ~50ms chunks
    outlet = StreamOutlet(info, chunk_size)
    
    return outlet


def stream_synthetic_eeg(stream_name: str = "PythonEEG", 
                         stream_type: str = "EEG",
                         n_channels: int = 8, 
                         srate: float = 250.0,
                         duration: float = None):
    """
    Stream synthetic EEG data continuously via LSL.
    
    Args:
        stream_name: Name of the LSL stream
        stream_type: Type of the stream
        n_channels: Number of EEG channels
        srate: Sampling rate in Hz
        duration: Duration in seconds (None for infinite)
    """
    print(f"\n{'='*60}")
    print(f"LSL EEG Streamer")
    print(f"{'='*60}")
    print(f"Stream Name: {stream_name}")
    print(f"Stream Type: {stream_type}")
    print(f"Channels: {n_channels}")
    print(f"Sample Rate: {srate} Hz")
    print(f"Duration: {'Infinite' if duration is None else f'{duration}s'}")
    print(f"{'='*60}")
    
    # Create LSL outlet
    outlet = create_lsl_outlet(stream_name, stream_type, n_channels, srate)
    print(f"\n✓ LSL outlet created. Waiting for consumers...")
    
    # Calculate timing
    chunk_samples = int(srate / 20)  # 50ms chunks
    chunk_duration = chunk_samples / srate
    
    start_time = time.time()
    sample_count = 0
    
    print(f"✓ Starting stream... Press Ctrl+C to stop.\n")
    
    try:
        while True:
            # Check duration limit
            if duration is not None and (time.time() - start_time) >= duration:
                break
            
            # Generate a chunk of data
            chunk = generate_synthetic_eeg(n_channels, chunk_samples, srate)
            
            # Push to LSL
            outlet.push_chunk(chunk.tolist())
            
            sample_count += chunk_samples
            
            # Print status every second
            elapsed = time.time() - start_time
            if int(elapsed) > int(elapsed - chunk_duration):
                print(f"\r  Streaming: {elapsed:.1f}s | Samples: {sample_count:,} | "
                      f"Rate: {sample_count/elapsed:.1f} Hz", end="", flush=True)
            
            # Sleep to maintain timing
            time.sleep(chunk_duration)
            
    except KeyboardInterrupt:
        print(f"\n\n✓ Stream stopped by user.")
    
    elapsed = time.time() - start_time
    print(f"✓ Total: {sample_count:,} samples in {elapsed:.2f}s")
    print(f"✓ Effective rate: {sample_count/elapsed:.2f} Hz")


def stream_from_file(filepath: str, stream_name: str = "FileEEG",
                     stream_type: str = "EEG", srate: float = 250.0,
                     loop: bool = True):
    """
    Stream EEG data from a numpy file via LSL.
    
    Args:
        filepath: Path to numpy file (shape: samples x channels)
        stream_name: Name of the LSL stream
        stream_type: Type of the stream
        srate: Sampling rate in Hz
        loop: Whether to loop the data
    """
    print(f"\nLoading data from: {filepath}")
    data = np.load(filepath).astype(np.float32)
    
    if data.ndim == 1:
        data = data.reshape(-1, 1)
    
    n_samples, n_channels = data.shape
    print(f"Data shape: {n_samples} samples x {n_channels} channels")
    
    outlet = create_lsl_outlet(stream_name, stream_type, n_channels, srate)
    print(f"✓ LSL outlet created. Starting stream...")
    
    chunk_samples = int(srate / 20)
    chunk_duration = chunk_samples / srate
    
    idx = 0
    try:
        while True:
            # Get chunk
            end_idx = min(idx + chunk_samples, n_samples)
            chunk = data[idx:end_idx, :]
            
            # Push to LSL
            outlet.push_chunk(chunk.tolist())
            
            idx = end_idx
            if idx >= n_samples:
                if loop:
                    idx = 0
                    print("\n  [Looping data...]")
                else:
                    break
            
            time.sleep(chunk_duration)
            
    except KeyboardInterrupt:
        print("\n✓ Stream stopped.")


def main():
    parser = argparse.ArgumentParser(
        description="Stream EEG data via Lab Streaming Layer for Open Ephys visualization",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python lsl_eeg_streamer.py                         # 8-channel synthetic EEG at 250 Hz
  python lsl_eeg_streamer.py -c 16 -s 1000           # 16-channel at 1000 Hz
  python lsl_eeg_streamer.py --name MyEEG --type EEG # Custom stream name
  python lsl_eeg_streamer.py --file recording.npy   # Stream from file
  python lsl_eeg_streamer.py --duration 60          # Stream for 60 seconds
        """
    )
    
    parser.add_argument('-n', '--name', type=str, default='PythonEEG',
                        help='LSL stream name (default: PythonEEG)')
    parser.add_argument('-t', '--type', type=str, default='EEG',
                        help='LSL stream type (default: EEG)')
    parser.add_argument('-c', '--channels', type=int, default=8,
                        help='Number of channels (default: 8)')
    parser.add_argument('-s', '--srate', type=float, default=250.0,
                        help='Sampling rate in Hz (default: 250)')
    parser.add_argument('-d', '--duration', type=float, default=None,
                        help='Duration in seconds (default: infinite)')
    parser.add_argument('-f', '--file', type=str, default=None,
                        help='Path to numpy file to stream')
    parser.add_argument('--no-loop', action='store_true',
                        help='Do not loop file data')
    
    args = parser.parse_args()
    
    if args.file:
        stream_from_file(
            filepath=args.file,
            stream_name=args.name,
            stream_type=args.type,
            srate=args.srate,
            loop=not args.no_loop
        )
    else:
        stream_synthetic_eeg(
            stream_name=args.name,
            stream_type=args.type,
            n_channels=args.channels,
            srate=args.srate,
            duration=args.duration
        )


if __name__ == "__main__":
    main()
