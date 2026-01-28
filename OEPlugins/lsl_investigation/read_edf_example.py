"""
Read EDF files and optionally stream via LSL or convert to OpenEphys Binary format.

Install dependencies:
    pip install mne pylsl numpy

Usage:
    python read_edf_example.py <edf_file>              # Read and show info
    python read_edf_example.py <edf_file> --stream     # Stream via LSL
    python read_edf_example.py <edf_file> --convert    # Convert to Binary format
    python read_edf_example.py --help                  # Show help
"""
import os
import sys
import argparse

try:
    import numpy as np
except ImportError:
    print("ERROR: numpy not installed. Install with: pip install numpy")
    sys.exit(1)

try:
    import mne
    mne.set_log_level('WARNING')  # Reduce MNE verbosity
except ImportError:
    print("ERROR: mne not installed. Install with: pip install mne")
    sys.exit(1)


def read_edf_info(edf_path, show_channels=True):
    """Read EDF file and print detailed information."""
    print(f"\n{'='*70}")
    print(f"  EDF FILE INFORMATION")
    print(f"{'='*70}")
    
    if not os.path.exists(edf_path):
        print(f"ERROR: File not found: {edf_path}")
        return None
    
    raw = mne.io.read_raw_edf(edf_path, preload=False, verbose=False)
    
    print(f"  File:            {os.path.basename(edf_path)}")
    print(f"  Full Path:       {os.path.abspath(edf_path)}")
    print(f"  File Size:       {os.path.getsize(edf_path) / 1024 / 1024:.2f} MB")
    print(f"  ")
    print(f"  Channels:        {len(raw.ch_names)}")
    print(f"  Sampling Rate:   {raw.info['sfreq']} Hz")
    print(f"  Duration:        {raw.times[-1]:.2f} seconds ({raw.times[-1]/60:.2f} minutes)")
    print(f"  Total Samples:   {len(raw.times)}")
    print(f"  ")
    
    # Show channel types
    ch_types = raw.get_channel_types()
    type_counts = {}
    for t in ch_types:
        type_counts[t] = type_counts.get(t, 0) + 1
    print(f"  Channel Types:   {dict(type_counts)}")
    
    if show_channels:
        print(f"\n  Channel Names ({len(raw.ch_names)} total):")
        print(f"  {'-'*60}")
        # Show in columns
        cols = 4
        for i in range(0, len(raw.ch_names), cols):
            row = raw.ch_names[i:i+cols]
            print(f"    " + "  ".join(f"{j+i:3d}. {name:15s}" for j, name in enumerate(row)))
    
    # Show measurement info if available
    if raw.info.get('subject_info'):
        print(f"\n  Subject Info:    {raw.info['subject_info']}")
    
    print(f"{'='*70}\n")
    
    return raw


def read_edf_data(edf_path, start_time=0, duration=10):
    """Read a segment of EDF data."""
    raw = mne.io.read_raw_edf(edf_path, preload=False, verbose=False)
    
    # Calculate sample indices
    sfreq = raw.info['sfreq']
    start_sample = int(start_time * sfreq)
    n_samples = int(duration * sfreq)
    
    # Load only the segment we need
    data, times = raw[:, start_sample:start_sample + n_samples]
    
    print(f"\n  Data Segment: {start_time}s to {start_time + duration}s")
    print(f"  Shape: {data.shape} (channels x samples)")
    print(f"  Data range: [{data.min():.6f}, {data.max():.6f}]")
    
    return data, times, raw.ch_names


def stream_edf_via_lsl(edf_path, speed_factor=1.0, chunk_size=32):
    """Stream EDF data via LSL (for testing LSL receivers)."""
    try:
        import pylsl
    except ImportError:
        print("ERROR: pylsl not installed. Install with: pip install pylsl")
        return
    
    import time
    
    print(f"\nLoading EDF file: {edf_path}")
    raw = mne.io.read_raw_edf(edf_path, preload=True, verbose=False)
    data = raw.get_data()  # [channels x samples]
    sfreq = raw.info['sfreq']
    n_channels = len(raw.ch_names)
    n_samples = data.shape[1]
    
    print(f"  Channels: {n_channels}")
    print(f"  Samples: {n_samples}")
    print(f"  Duration: {n_samples / sfreq:.2f}s")
    
    # Create LSL stream
    info = pylsl.StreamInfo(
        name='EDF_Stream',
        type='EEG',
        channel_count=n_channels,
        nominal_srate=sfreq * speed_factor,
        channel_format=pylsl.cf_float32,
        source_id=f'edf_{os.path.basename(edf_path)}'
    )
    
    # Add channel metadata
    desc = info.desc()
    channels = desc.append_child("channels")
    for ch_name in raw.ch_names:
        ch = channels.append_child("channel")
        ch.append_child_value("label", ch_name)
        ch.append_child_value("unit", "microvolts")
        ch.append_child_value("type", "EEG")
    
    # Add source file info
    src = desc.append_child("source")
    src.append_child_value("file", os.path.basename(edf_path))
    src.append_child_value("original_srate", str(sfreq))
    src.append_child_value("speed_factor", str(speed_factor))
    
    outlet = pylsl.StreamOutlet(info, chunk_size=chunk_size)
    
    print(f"\n{'='*70}")
    print(f"  LSL STREAM CREATED")
    print(f"{'='*70}")
    print(f"  Stream Name:     EDF_Stream")
    print(f"  Type:            EEG")
    print(f"  Channels:        {n_channels}")
    print(f"  Output Rate:     {sfreq * speed_factor} Hz (speed factor: {speed_factor}x)")
    print(f"  Chunk Size:      {chunk_size} samples")
    print(f"{'='*70}")
    print(f"\n  Streaming... (Ctrl+C to stop)")
    
    sample_interval = 1.0 / (sfreq * speed_factor)
    sample_idx = 0
    start_time = time.time()
    
    try:
        while sample_idx < n_samples:
            # Push sample
            sample = data[:, sample_idx].tolist()
            outlet.push_sample(sample)
            sample_idx += 1
            
            # Print progress every second of data
            if sample_idx % int(sfreq) == 0:
                elapsed = time.time() - start_time
                data_time = sample_idx / sfreq
                print(f"    Streamed: {data_time:.1f}s of data ({elapsed:.1f}s elapsed, {sample_idx}/{n_samples} samples)")
            
            # Maintain timing
            expected_time = sample_idx * sample_interval
            actual_elapsed = time.time() - start_time
            if expected_time > actual_elapsed:
                time.sleep(expected_time - actual_elapsed)
        
        print(f"\n  Finished streaming entire file!")
        
    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        print(f"\n  Stopped. Streamed {sample_idx} samples ({sample_idx/sfreq:.1f}s) in {elapsed:.1f}s")


def convert_edf_to_binary(edf_path, output_dir=None):
    """Convert EDF to OpenEphys Binary format."""
    import json
    
    if output_dir is None:
        output_dir = os.path.splitext(edf_path)[0] + "_binary"
    
    print(f"\nConverting EDF to OpenEphys Binary format...")
    print(f"  Input:  {edf_path}")
    print(f"  Output: {output_dir}")
    
    # Load EDF
    raw = mne.io.read_raw_edf(edf_path, preload=True, verbose=False)
    data = raw.get_data() * 1e6  # Convert to microvolts (MNE loads in Volts)
    sfreq = raw.info['sfreq']
    n_channels = len(raw.ch_names)
    n_samples = data.shape[1]
    
    print(f"  Channels: {n_channels}")
    print(f"  Samples: {n_samples}")
    print(f"  Sample Rate: {sfreq} Hz")
    
    # Create output directory structure
    stream_name = "EDF_Data"
    continuous_dir = os.path.join(output_dir, "continuous", stream_name)
    os.makedirs(continuous_dir, exist_ok=True)
    
    # Determine bit_volts scaling
    # We want to store in int16 with reasonable precision
    data_range = max(abs(data.min()), abs(data.max()))
    bit_volts = data_range / 32000  # Leave some headroom from int16 max
    bit_volts = max(bit_volts, 0.195)  # Minimum typical value
    
    print(f"  Data range: [{data.min():.2f}, {data.max():.2f}] µV")
    print(f"  Bit volts: {bit_volts:.6f}")
    
    # Save continuous.dat (int16, samples x channels)
    # OpenEphys Binary format stores data as [samples x channels]
    scaled_data = (data.T / bit_volts).astype('int16')  # Transpose to [samples x channels]
    dat_path = os.path.join(continuous_dir, "continuous.dat")
    scaled_data.tofile(dat_path)
    print(f"  Saved: continuous.dat ({os.path.getsize(dat_path) / 1024 / 1024:.2f} MB)")
    
    # Save sample_numbers.npy
    sample_numbers = np.arange(n_samples, dtype=np.int64)
    np.save(os.path.join(continuous_dir, "sample_numbers.npy"), sample_numbers)
    print(f"  Saved: sample_numbers.npy")
    
    # Save timestamps.npy
    timestamps = sample_numbers / sfreq
    np.save(os.path.join(continuous_dir, "timestamps.npy"), timestamps.astype(np.float64))
    print(f"  Saved: timestamps.npy")
    
    # Create structure.oebin
    oebin = {
        "GUI version": "0.6.0",
        "continuous": [{
            "folder_name": stream_name,
            "sample_rate": float(sfreq),
            "stream_name": stream_name,
            "source_processor_id": 100,
            "source_processor_name": "EDF Import",
            "num_channels": n_channels,
            "channels": [
                {
                    "channel_name": raw.ch_names[i],
                    "bit_volts": float(bit_volts)
                }
                for i in range(n_channels)
            ]
        }],
        "spikes": [],
        "events": []
    }
    
    oebin_path = os.path.join(output_dir, "structure.oebin")
    with open(oebin_path, 'w') as f:
        json.dump(oebin, f, indent=4)
    print(f"  Saved: structure.oebin")
    
    print(f"\n  Conversion complete!")
    print(f"  Output directory: {os.path.abspath(output_dir)}")
    
    # Show how to load
    print(f"\n  To load in Python:")
    print(f"    from open_ephys.analysis import Session")
    print(f"    session = Session('{output_dir}')")
    print(f"    recording = session.recordnodes[0].recordings[0]")
    print(f"    data = recording.continuous[0].get_samples(0, 1000)")
    
    return output_dir


def plot_edf_segment(edf_path, start_time=0, duration=10, channels=None):
    """Plot a segment of EDF data."""
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("ERROR: matplotlib not installed. Install with: pip install matplotlib")
        return
    
    raw = mne.io.read_raw_edf(edf_path, preload=False, verbose=False)
    sfreq = raw.info['sfreq']
    
    # Select channels
    if channels is None:
        channels = raw.ch_names[:8]  # First 8 channels
    
    # Load data segment
    start_sample = int(start_time * sfreq)
    n_samples = int(duration * sfreq)
    
    picks = [raw.ch_names.index(ch) for ch in channels if ch in raw.ch_names]
    data, times = raw[picks, start_sample:start_sample + n_samples]
    
    # Convert to microvolts
    data = data * 1e6
    
    # Plot
    fig, axes = plt.subplots(len(picks), 1, figsize=(14, 2*len(picks)), sharex=True)
    if len(picks) == 1:
        axes = [axes]
    
    for i, (ax, ch_idx) in enumerate(zip(axes, picks)):
        ax.plot(times, data[i], 'b-', linewidth=0.5)
        ax.set_ylabel(f'{raw.ch_names[ch_idx]}\n(µV)')
        ax.grid(True, alpha=0.3)
    
    axes[-1].set_xlabel('Time (s)')
    fig.suptitle(f'EDF: {os.path.basename(edf_path)} ({start_time}s - {start_time+duration}s)')
    plt.tight_layout()
    plt.show()


# Data directory paths (relative to this script)
DATA_DIRS = {
    'eeg_data': os.path.join(os.path.dirname(__file__), '..', '..', 'test_data', 'eeg_data'),
    'eeg_data_2': os.path.join(os.path.dirname(__file__), '..', '..', 'test_data', 'eeg_data_2', 'EDF_format', 'EDF_format'),
}


def list_all_edf_files():
    """List all EDF files from both data directories."""
    print(f"\n{'='*70}")
    print(f"  AVAILABLE EDF FILES")
    print(f"{'='*70}")
    
    total_files = 0
    
    for name, data_dir in DATA_DIRS.items():
        if os.path.exists(data_dir):
            edf_files = sorted([f for f in os.listdir(data_dir) if f.endswith('.edf')])
            print(f"\n  [{name}] - {data_dir}")
            print(f"  {'-'*60}")
            
            # Show files (limit to first 20)
            for i, f in enumerate(edf_files[:20]):
                filepath = os.path.join(data_dir, f)
                size = os.path.getsize(filepath) / 1024 / 1024
                print(f"    {f:25s} ({size:6.2f} MB)")
            
            if len(edf_files) > 20:
                print(f"    ... and {len(edf_files) - 20} more files")
            
            print(f"  Total: {len(edf_files)} files")
            total_files += len(edf_files)
        else:
            print(f"\n  [{name}] - NOT FOUND: {data_dir}")
    
    print(f"\n{'='*70}")
    print(f"  GRAND TOTAL: {total_files} EDF files")
    print(f"{'='*70}\n")


def find_edf_file(filename):
    """Find an EDF file by name, searching in all data directories."""
    # If it's already a full path, return it
    if os.path.isabs(filename) and os.path.exists(filename):
        return filename
    
    # Search in data directories
    for name, data_dir in DATA_DIRS.items():
        if os.path.exists(data_dir):
            filepath = os.path.join(data_dir, filename)
            if os.path.exists(filepath):
                return filepath
            # Try with .edf extension
            if not filename.endswith('.edf'):
                filepath = os.path.join(data_dir, filename + '.edf')
                if os.path.exists(filepath):
                    return filepath
    
    # Check relative to current directory
    if os.path.exists(filename):
        return filename
    
    return None


def main():
    parser = argparse.ArgumentParser(
        description='Read, stream, or convert EDF files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python read_edf_example.py --list
  python read_edf_example.py eeg1.edf                    # Auto-finds in eeg_data/
  python read_edf_example.py ID01_epoch1.edf             # Auto-finds in eeg_data_2/
  python read_edf_example.py eeg1.edf --stream
  python read_edf_example.py ID01_epoch1.edf --convert
  python read_edf_example.py eeg1.edf --plot --start 10 --duration 5
        """
    )
    
    parser.add_argument('edf_file', nargs='?', help='Path to EDF file (will search in eeg_data and eeg_data_2)')
    parser.add_argument('--stream', action='store_true', help='Stream via LSL')
    parser.add_argument('--speed', type=float, default=1.0, help='Streaming speed factor (default: 1.0)')
    parser.add_argument('--convert', action='store_true', help='Convert to OpenEphys Binary format')
    parser.add_argument('--output', type=str, help='Output directory for conversion')
    parser.add_argument('--plot', action='store_true', help='Plot a segment of the data')
    parser.add_argument('--start', type=float, default=0, help='Start time in seconds (default: 0)')
    parser.add_argument('--duration', type=float, default=10, help='Duration in seconds (default: 10)')
    parser.add_argument('--list', action='store_true', help='List EDF files in all data directories')
    
    args = parser.parse_args()
    
    # List files mode
    if args.list:
        list_all_edf_files()
        return
    
    if not args.edf_file:
        parser.print_help()
        print("\n\nTip: Use --list to see available EDF files in test_data/eeg_data/ and test_data/eeg_data_2/")
        return
    
    # Find the EDF file
    edf_path = find_edf_file(args.edf_file)
    if edf_path is None:
        print(f"ERROR: Could not find EDF file: {args.edf_file}")
        print("Use --list to see available files.")
        return
    
    # Read and show info
    raw = read_edf_info(edf_path)
    if raw is None:
        return
    
    # Stream mode
    if args.stream:
        stream_edf_via_lsl(edf_path, speed_factor=args.speed)
        return
    
    # Convert mode
    if args.convert:
        convert_edf_to_binary(edf_path, args.output)
        return
    
    # Plot mode
    if args.plot:
        plot_edf_segment(edf_path, start_time=args.start, duration=args.duration)
        return


if __name__ == "__main__":
    main()
