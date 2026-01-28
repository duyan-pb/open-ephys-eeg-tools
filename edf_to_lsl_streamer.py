"""
EDF to LSL Streamer for Open Ephys

Reads EEG data from EDF files and streams them via Lab Streaming Layer (LSL)
so you can visualize them in real-time using Open Ephys GUI with the LSL Inlet plugin.

Usage:
    python edf_to_lsl_streamer.py                           # Stream eeg1.edf
    python edf_to_lsl_streamer.py --file eeg5.edf           # Stream specific file
    python edf_to_lsl_streamer.py --file eeg1.edf --loop    # Loop the file
    python edf_to_lsl_streamer.py --list                    # List all EDF files

Requirements:
    pip install pylsl mne numpy

Author: Generated for Open Ephys visualization
"""

import argparse
import time
import sys
import os
from pathlib import Path

# Check dependencies
try:
    import numpy as np
except ImportError:
    print("Error: numpy not installed. Run: pip install numpy")
    sys.exit(1)

try:
    import mne
    mne.set_log_level('WARNING')  # Reduce MNE verbosity
except ImportError:
    print("Error: mne not installed. Run: pip install mne")
    sys.exit(1)

try:
    from pylsl import StreamInfo, StreamOutlet
except ImportError:
    print("Error: pylsl not installed. Run: pip install pylsl")
    sys.exit(1)


# Default data directory
EEG_DATA_DIR = Path(__file__).parent / "eeg_data"


def list_edf_files(data_dir: Path) -> list:
    """List all EDF files in the data directory."""
    edf_files = sorted(data_dir.glob("*.edf"), key=lambda x: (len(x.stem), x.stem))
    return edf_files


def load_edf_file(filepath: Path) -> tuple:
    """
    Load an EDF file using MNE.
    
    Returns:
        tuple: (data array, channel names, sample rate)
    """
    print(f"\nLoading: {filepath.name}")
    
    # Read the EDF file
    raw = mne.io.read_raw_edf(str(filepath), preload=True, verbose=False)
    
    # Get data and info
    data = raw.get_data()  # Shape: (n_channels, n_samples)
    srate = raw.info['sfreq']
    channel_names = raw.ch_names
    
    # Convert to microvolts (EDF is often in volts, EEG is typically shown in µV)
    # MNE loads EDF data in Volts, convert to microvolts
    data = data * 1e6
    
    print(f"  Channels: {len(channel_names)}")
    print(f"  Sample rate: {srate} Hz")
    print(f"  Duration: {data.shape[1] / srate:.2f} seconds")
    print(f"  Channel names: {', '.join(channel_names[:5])}{'...' if len(channel_names) > 5 else ''}")
    
    return data, channel_names, srate


def create_lsl_outlet(stream_name: str, stream_type: str, n_channels: int,
                      srate: float, channel_names: list) -> StreamOutlet:
    """Create an LSL outlet for streaming EEG data."""
    
    # Create stream info
    info = StreamInfo(
        name=stream_name,
        type=stream_type,
        channel_count=n_channels,
        nominal_srate=srate,
        channel_format='float32',
        source_id=f'EDF_Streamer_{stream_name}'
    )
    
    # Add channel metadata
    channels = info.desc().append_child("channels")
    for i, name in enumerate(channel_names):
        ch = channels.append_child("channel")
        ch.append_child_value("label", name)
        ch.append_child_value("unit", "microvolts")
        ch.append_child_value("type", "EEG")
    
    # Add acquisition metadata
    acq = info.desc().append_child("acquisition")
    acq.append_child_value("manufacturer", "EDF File Streamer")
    acq.append_child_value("model", "MNE-Python")
    
    # Create outlet with efficient chunk size
    chunk_size = max(1, int(srate / 20))  # ~50ms chunks
    outlet = StreamOutlet(info, chunk_size)
    
    return outlet


def stream_edf_data(filepath: Path, stream_name: str = "EDF_EEG",
                    stream_type: str = "EEG", loop: bool = False,
                    speed: float = 1.0):
    """
    Stream EDF data via LSL.
    
    Args:
        filepath: Path to EDF file
        stream_name: Name of the LSL stream
        stream_type: Type of stream
        loop: Whether to loop the data
        speed: Playback speed multiplier (1.0 = real-time)
    """
    # Load the EDF file
    data, channel_names, srate = load_edf_file(filepath)
    n_channels, n_samples = data.shape
    
    # Transpose for easier iteration (samples x channels)
    data = data.T.astype(np.float32)
    
    # Create LSL outlet
    outlet = create_lsl_outlet(stream_name, stream_type, n_channels, srate, channel_names)
    
    print(f"\n{'='*60}")
    print(f"LSL Stream Created")
    print(f"{'='*60}")
    print(f"  Stream Name: {stream_name}")
    print(f"  Stream Type: {stream_type}")
    print(f"  Channels: {n_channels}")
    print(f"  Sample Rate: {srate} Hz")
    print(f"  Playback Speed: {speed}x")
    print(f"  Loop: {'Yes' if loop else 'No'}")
    print(f"{'='*60}")
    print(f"\n>>> Now open Open Ephys GUI:")
    print(f"    1. Add 'LSL Inlet' as data source")
    print(f"    2. Click 'Refresh' to find '{stream_name}'")
    print(f"    3. Select the stream and add 'LFP Viewer'")
    print(f"    4. Press Play to start acquisition")
    print(f"\nStreaming... Press Ctrl+C to stop.\n")
    
    # Calculate chunk parameters
    chunk_samples = max(1, int(srate / 20))  # ~50ms chunks
    chunk_duration = chunk_samples / srate / speed
    
    sample_idx = 0
    start_time = time.time()
    total_samples_sent = 0
    loop_count = 0
    
    try:
        while True:
            # Get chunk of data
            end_idx = min(sample_idx + chunk_samples, n_samples)
            chunk = data[sample_idx:end_idx, :]
            
            # Push chunk to LSL
            outlet.push_chunk(chunk.tolist())
            
            total_samples_sent += len(chunk)
            sample_idx = end_idx
            
            # Check if we've reached the end
            if sample_idx >= n_samples:
                if loop:
                    sample_idx = 0
                    loop_count += 1
                    print(f"\n  [Loop {loop_count + 1}]", end="")
                else:
                    print(f"\n\n✓ End of file reached.")
                    break
            
            # Status update
            elapsed = time.time() - start_time
            file_position = sample_idx / srate
            file_duration = n_samples / srate
            
            status = (f"\r  Time: {elapsed:.1f}s | "
                     f"File: {file_position:.1f}/{file_duration:.1f}s | "
                     f"Samples: {total_samples_sent:,}")
            print(status, end="", flush=True)
            
            # Sleep to maintain real-time (or adjusted) playback
            time.sleep(chunk_duration)
            
    except KeyboardInterrupt:
        print(f"\n\n✓ Streaming stopped by user.")
    
    elapsed = time.time() - start_time
    print(f"✓ Sent {total_samples_sent:,} samples in {elapsed:.2f}s")


def main():
    parser = argparse.ArgumentParser(
        description="Stream EDF EEG data via LSL for Open Ephys visualization",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python edf_to_lsl_streamer.py                      # Stream first EDF file
  python edf_to_lsl_streamer.py --file eeg1.edf      # Stream specific file
  python edf_to_lsl_streamer.py --file eeg5.edf --loop  # Loop playback
  python edf_to_lsl_streamer.py --speed 2.0          # 2x speed playback
  python edf_to_lsl_streamer.py --list               # List available files

In Open Ephys:
  1. Add "LSL Inlet" processor (Data Source)
  2. Click "Refresh" to discover streams
  3. Select the EDF_EEG stream
  4. Add "LFP Viewer" to visualize
  5. Press Play!
        """
    )
    
    parser.add_argument('-f', '--file', type=str, default=None,
                        help='EDF file to stream (default: first file found)')
    parser.add_argument('-d', '--data-dir', type=str, default=None,
                        help='Directory containing EDF files')
    parser.add_argument('-n', '--name', type=str, default='EDF_EEG',
                        help='LSL stream name (default: EDF_EEG)')
    parser.add_argument('-t', '--type', type=str, default='EEG',
                        help='LSL stream type (default: EEG)')
    parser.add_argument('-l', '--loop', action='store_true',
                        help='Loop the EDF file continuously')
    parser.add_argument('-s', '--speed', type=float, default=1.0,
                        help='Playback speed multiplier (default: 1.0)')
    parser.add_argument('--list', action='store_true',
                        help='List available EDF files and exit')
    
    args = parser.parse_args()
    
    # Determine data directory
    if args.data_dir:
        data_dir = Path(args.data_dir)
    else:
        data_dir = EEG_DATA_DIR
    
    if not data_dir.exists():
        print(f"Error: Data directory not found: {data_dir}")
        sys.exit(1)
    
    # List files mode
    if args.list:
        edf_files = list_edf_files(data_dir)
        print(f"\nEDF files in {data_dir}:\n")
        for i, f in enumerate(edf_files, 1):
            size_mb = f.stat().st_size / (1024 * 1024)
            print(f"  {i:2}. {f.name} ({size_mb:.1f} MB)")
        print(f"\nTotal: {len(edf_files)} files")
        return
    
    # Determine which file to stream
    if args.file:
        filepath = data_dir / args.file
        if not filepath.exists():
            # Try as absolute path
            filepath = Path(args.file)
        if not filepath.exists():
            print(f"Error: File not found: {args.file}")
            sys.exit(1)
    else:
        edf_files = list_edf_files(data_dir)
        if not edf_files:
            print(f"Error: No EDF files found in {data_dir}")
            sys.exit(1)
        filepath = edf_files[0]
        print(f"No file specified, using: {filepath.name}")
    
    # Stream the data
    stream_edf_data(
        filepath=filepath,
        stream_name=args.name,
        stream_type=args.type,
        loop=args.loop,
        speed=args.speed
    )


if __name__ == "__main__":
    main()
