"""
PyLSL script to investigate LSL stream properties and fields.
Install: pip install pylsl

Usage:
    python investigate_lsl_streams.py              # Discover and investigate streams
    python investigate_lsl_streams.py --create-test  # Create a test stream
    python investigate_lsl_streams.py --help       # Show help
"""
import sys

try:
    import pylsl
except ImportError:
    print("ERROR: pylsl not installed. Install with: pip install pylsl")
    sys.exit(1)


def discover_streams(wait_time=2.0):
    """Discover all available LSL streams on the network."""
    print(f"Looking for LSL streams on the network (waiting {wait_time}s)...")
    streams = pylsl.resolve_streams(wait_time=wait_time)
    
    if not streams:
        print("No streams found!")
        return []
    
    print(f"\nFound {len(streams)} stream(s):\n")
    return streams


def print_stream_info(stream_info, verbose=False):
    """Print detailed information about an LSL stream."""
    print("=" * 70)
    print(f"  CORE PROPERTIES")
    print(f"  {'-'*50}")
    print(f"  Stream Name:      {stream_info.name()}")
    print(f"  Type:             {stream_info.type()}")
    print(f"  Channel Count:    {stream_info.channel_count()}")
    print(f"  Sampling Rate:    {stream_info.nominal_srate()} Hz")
    print(f"  Channel Format:   {format_channel_type(stream_info.channel_format())}")
    print(f"  Source ID:        {stream_info.source_id()}")
    
    print(f"\n  NETWORK/SESSION INFO")
    print(f"  {'-'*50}")
    print(f"  UID:              {stream_info.uid() or '(not assigned)'}")
    print(f"  Hostname:         {stream_info.hostname() or '(not assigned)'}")
    print(f"  Session ID:       {stream_info.session_id() or '(none)'}")
    print(f"  Version:          {stream_info.version()}")
    print(f"  Created At:       {stream_info.created_at()}")
    
    # Try to get channel labels/types/units (convenience methods)
    try:
        labels = stream_info.get_channel_labels()
        types = stream_info.get_channel_types()
        units = stream_info.get_channel_units()
        
        if any(labels) or any(types) or any(units):
            print(f"\n  CHANNEL METADATA (quick access)")
            print(f"  {'-'*50}")
            if any(labels):
                print(f"  Labels:           {labels[:8]}{'...' if len(labels) > 8 else ''}")
            if any(types):
                unique_types = list(set(types))
                print(f"  Types:            {unique_types}")
            if any(units):
                unique_units = list(set(units))
                print(f"  Units:            {unique_units}")
    except Exception:
        pass  # Methods may not be available in older pylsl versions
    
    # Parse XML for additional network info
    if verbose:
        try:
            import xml.etree.ElementTree as ET
            xml_str = stream_info.as_xml()
            root = ET.fromstring(xml_str)
            
            v4addr = root.findtext('v4address', '')
            v4data = root.findtext('v4data_port', '0')
            v4svc = root.findtext('v4service_port', '0')
            v6addr = root.findtext('v6address', '')
            
            if v4addr or v6addr:
                print(f"\n  NETWORK ENDPOINTS")
                print(f"  {'-'*50}")
                if v4addr:
                    print(f"  IPv4 Address:     {v4addr}")
                    print(f"  IPv4 Data Port:   {v4data}")
                    print(f"  IPv4 Service Port:{v4svc}")
                if v6addr:
                    print(f"  IPv6 Address:     {v6addr}")
        except Exception:
            pass
    
    print(f"\n  STREAM CATEGORY")
    print(f"  {'-'*50}")
    # Determine stream category
    if stream_info.nominal_srate() > 0:
        print(f"  Category:         DATA STREAM (regular sampling)")
    else:
        print(f"  Category:         MARKER/EVENT STREAM (irregular)")
    
    print("=" * 70)


def format_channel_type(channel_format):
    """Convert channel format enum to readable string."""
    format_names = {
        pylsl.cf_float32: "float32 (32-bit float)",
        pylsl.cf_double64: "double64 (64-bit double)",
        pylsl.cf_string: "string (variable length)",
        pylsl.cf_int32: "int32 (32-bit integer)",
        pylsl.cf_int16: "int16 (16-bit integer)",
        pylsl.cf_int8: "int8 (8-bit integer)",
        pylsl.cf_int64: "int64 (64-bit integer)",
        pylsl.cf_undefined: "undefined",
    }
    return format_names.get(channel_format, f"unknown ({channel_format})")


def print_stream_xml(stream_info):
    """Print the full XML description of a stream."""
    print("\nFull XML Description:")
    print("-" * 70)
    xml_str = stream_info.as_xml()
    # Pretty print with indentation
    try:
        import xml.dom.minidom
        dom = xml.dom.minidom.parseString(xml_str)
        print(dom.toprettyxml(indent="  "))
    except:
        print(xml_str)
    print("-" * 70)


def read_channel_metadata(inlet):
    """Read and display channel metadata from stream description."""
    full_info = inlet.info()
    desc = full_info.desc()
    
    # Look for channels element
    channels = desc.child("channels")
    if channels.empty():
        print("  No channel metadata available in stream description.")
        return
    
    print("\n  Channel Information:")
    print("  " + "-" * 50)
    ch = channels.child("channel")
    ch_idx = 0
    while not ch.empty():
        label = ch.child_value("label") or f"CH{ch_idx}"
        unit = ch.child_value("unit") or "unknown"
        type_ = ch.child_value("type") or "unknown"
        print(f"    Channel {ch_idx:3d}: {label:20s} | Type: {type_:10s} | Unit: {unit}")
        ch = ch.next_sibling("channel")
        ch_idx += 1
    print("  " + "-" * 50)


def read_samples_from_stream(stream_info, num_samples=10, timeout=5.0):
    """Read sample data from an LSL stream."""
    print(f"\nConnecting to stream: {stream_info.name()}...")
    
    # Create inlet with buffer
    inlet = pylsl.StreamInlet(stream_info, max_buflen=360)
    
    # Open stream explicitly
    inlet.open_stream(timeout=timeout)
    print("  Stream opened successfully.")
    
    # Read channel metadata
    read_channel_metadata(inlet)
    
    # Read samples
    print(f"\n  Reading {num_samples} samples...")
    print("  " + "-" * 60)
    
    n_channels = stream_info.channel_count()
    
    for i in range(num_samples):
        sample, timestamp = inlet.pull_sample(timeout=timeout)
        if sample:
            # Show first few channels if many
            if n_channels <= 8:
                data_str = str(sample)
            else:
                data_str = f"[{sample[0]:.4f}, {sample[1]:.4f}, ... {sample[-1]:.4f}] ({n_channels} ch)"
            print(f"    Sample {i+1:3d}: ts={timestamp:.6f}, data={data_str}")
        else:
            print(f"    Sample {i+1:3d}: TIMEOUT - no data received")
            break
    
    print("  " + "-" * 60)
    inlet.close_stream()
    print("  Stream closed.")


def create_test_outlet():
    """Create a test LSL outlet for demonstration."""
    import time
    import random
    
    print("Creating test EEG stream outlet...")
    
    # Create stream info
    info = pylsl.StreamInfo(
        name='TestEEGStream',
        type='EEG',
        channel_count=8,
        nominal_srate=256,
        channel_format=pylsl.cf_float32,
        source_id='test_device_001'
    )
    
    # Add metadata description
    desc = info.desc()
    
    # Add channel information
    channels = desc.append_child("channels")
    channel_names = ['Fp1', 'Fp2', 'F3', 'F4', 'C3', 'C4', 'O1', 'O2']
    for i, name in enumerate(channel_names):
        ch = channels.append_child("channel")
        ch.append_child_value("label", name)
        ch.append_child_value("unit", "microvolts")
        ch.append_child_value("type", "EEG")
    
    # Add acquisition info
    acq = desc.append_child("acquisition")
    acq.append_child_value("manufacturer", "TestDevice")
    acq.append_child_value("model", "Demo LSL Source")
    acq.append_child_value("serial_number", "TEST001")
    
    # Create outlet
    outlet = pylsl.StreamOutlet(info, chunk_size=0, max_buffered=360)
    
    print(f"\n  Stream Name:    {info.name()}")
    print(f"  Type:           {info.type()}")
    print(f"  Channels:       {info.channel_count()}")
    print(f"  Sample Rate:    {info.nominal_srate()} Hz")
    print(f"  Source ID:      {info.source_id()}")
    
    print("\n  Stream is now discoverable on the network.")
    print("  Pushing samples (Ctrl+C to stop)...\n")
    
    sample_count = 0
    start_time = time.time()
    
    try:
        while True:
            # Generate random EEG-like data (microvolts range)
            sample = [random.gauss(0, 50) for _ in range(8)]
            outlet.push_sample(sample)
            sample_count += 1
            
            # Print status every second
            if sample_count % 256 == 0:
                elapsed = time.time() - start_time
                print(f"  Pushed {sample_count} samples ({elapsed:.1f}s elapsed)")
            
            time.sleep(1.0 / 256)  # 256 Hz
            
    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        print(f"\n  Stopped. Total: {sample_count} samples in {elapsed:.1f}s")


def create_marker_outlet():
    """Create a test LSL marker stream for demonstration."""
    import time
    
    print("Creating test Marker stream outlet...")
    
    # Marker streams have irregular rate (0) and string format
    info = pylsl.StreamInfo(
        name='TestMarkerStream',
        type='Markers',
        channel_count=1,
        nominal_srate=0,  # Irregular rate for markers
        channel_format=pylsl.cf_string,
        source_id='test_markers_001'
    )
    
    outlet = pylsl.StreamOutlet(info)
    
    print(f"\n  Stream Name:    {info.name()}")
    print(f"  Type:           {info.type()}")
    print(f"  Channel Format: string (for event markers)")
    
    print("\n  Stream is now discoverable on the network.")
    print("  Sending markers every 2 seconds (Ctrl+C to stop)...\n")
    
    markers = ['stimulus_onset', 'response', 'trial_start', 'trial_end', 'baseline']
    marker_idx = 0
    
    try:
        while True:
            marker = markers[marker_idx % len(markers)]
            outlet.push_sample([marker])
            print(f"  Sent marker: '{marker}'")
            marker_idx += 1
            time.sleep(2.0)
            
    except KeyboardInterrupt:
        print(f"\n  Stopped. Sent {marker_idx} markers.")


def print_help():
    """Print help information."""
    print("""
LSL Stream Investigation Tool
=============================

Usage:
    python investigate_lsl_streams.py [options]

Options:
    (no options)        Discover all streams and show their properties
    --create-test       Create a test EEG data stream (256 Hz, 8 channels)
    --create-markers    Create a test marker stream (event strings)
    --xml               Also print full XML description for each stream
    --verbose, -v       Show additional network endpoint info (ports, addresses)
    --read N            Read N samples from discovered data streams (default: 10)
    --wait T            Wait T seconds for stream discovery (default: 2.0)
    --help              Show this help message

Examples:
    # Discover all LSL streams on the network
    python investigate_lsl_streams.py
    
    # Discover streams with verbose output
    python investigate_lsl_streams.py --verbose
    
    # Discover streams and read 20 samples
    python investigate_lsl_streams.py --read 20
    
    # Create a test stream for other applications to receive
    python investigate_lsl_streams.py --create-test
    
    # Show full XML descriptions
    python investigate_lsl_streams.py --xml

LSL StreamInfo Properties (Core):
    - name():           Human-readable stream name
    - type():           Content type (EEG, Markers, Gaze, Audio, etc.)
    - channel_count():  Number of channels per sample
    - nominal_srate():  Sampling rate in Hz (0 = irregular/marker stream)
    - channel_format(): Data type (float32, double64, string, int32, int16, int8, int64)
    - source_id():      Unique device/source identifier (user-provided)

LSL StreamInfo Properties (Network/Session):
    - uid():            Unique stream instance ID (auto-generated on network)
    - hostname():       Machine name providing the stream
    - session_id():     Optional session identifier for grouping streams
    - version():        LSL protocol version
    - created_at():     Stream creation timestamp

LSL StreamInfo Properties (Channel Metadata):
    - get_channel_labels():  List of channel names (e.g., ['Fp1', 'Fp2', ...])
    - get_channel_types():   List of channel types (e.g., ['EEG', 'EMG', ...])
    - get_channel_units():   List of units (e.g., ['uV', 'mV', ...])

XML Structure (via as_xml() or desc()):
    - v4address, v4data_port, v4service_port:  IPv4 network info
    - v6address, v6data_port, v6service_port:  IPv6 network info  
    - desc/channels/channel/*:  Per-channel label, type, unit
    - desc/*:                   Custom metadata (manufacturer, model, etc.)
""")


def main():
    args = sys.argv[1:]
    
    # Parse arguments
    show_xml = '--xml' in args
    verbose = '--verbose' in args or '-v' in args
    num_samples = 10
    wait_time = 2.0
    
    if '--help' in args or '-h' in args:
        print_help()
        return
    
    if '--create-test' in args:
        create_test_outlet()
        return
    
    if '--create-markers' in args:
        create_marker_outlet()
        return
    
    if '--read' in args:
        try:
            idx = args.index('--read')
            num_samples = int(args[idx + 1])
        except (IndexError, ValueError):
            print("Error: --read requires a number argument")
            return
    
    if '--wait' in args:
        try:
            idx = args.index('--wait')
            wait_time = float(args[idx + 1])
        except (IndexError, ValueError):
            print("Error: --wait requires a number argument")
            return
    
    # Discover streams
    streams = discover_streams(wait_time)
    
    if not streams:
        print("\nTip: Run 'python investigate_lsl_streams.py --create-test' in another terminal")
        print("     to create a test stream, then run this script again.")
        return
    
    # Print info for each stream
    for i, stream in enumerate(streams):
        print(f"\n{'='*70}")
        print(f"  STREAM {i+1} of {len(streams)}")
        print_stream_info(stream, verbose=verbose)
        
        if show_xml:
            print_stream_xml(stream)
    
    # Read samples from data streams
    print("\n" + "=" * 70)
    print("  READING SAMPLES FROM DATA STREAMS")
    print("=" * 70)
    
    data_streams = [s for s in streams if s.nominal_srate() > 0]
    marker_streams = [s for s in streams if s.nominal_srate() == 0]
    
    print(f"\n  Found {len(data_streams)} data stream(s) and {len(marker_streams)} marker stream(s)")
    
    for stream in data_streams:
        read_samples_from_stream(stream, num_samples)
    
    if marker_streams:
        print("\n  Marker streams (irregular rate) - showing info only:")
        for stream in marker_streams:
            print(f"    - {stream.name()} ({stream.type()})")


if __name__ == "__main__":
    main()
