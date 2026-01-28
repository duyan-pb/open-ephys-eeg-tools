#!/usr/bin/env python3
"""
Decompress .csv.xz files to .csv for use with Open Ephys File Reader

Usage:
    python decompress_csv.py <input_folder> [output_folder]
    
Example:
    python decompress_csv.py "C:/path/to/CSV_format" "C:/path/to/decompressed"
"""

import lzma
import os
import sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse


def decompress_file(xz_path: Path, output_dir: Path) -> str:
    """Decompress a single .xz file"""
    output_path = output_dir / xz_path.stem  # Remove .xz extension
    
    if output_path.exists():
        return f"Skipped (exists): {output_path.name}"
    
    try:
        with lzma.open(xz_path, 'rb') as f_in:
            with open(output_path, 'wb') as f_out:
                # Read in chunks to handle large files
                while True:
                    chunk = f_in.read(1024 * 1024)  # 1MB chunks
                    if not chunk:
                        break
                    f_out.write(chunk)
        
        size_mb = output_path.stat().st_size / (1024 * 1024)
        return f"Decompressed: {output_path.name} ({size_mb:.1f} MB)"
    except Exception as e:
        return f"Error: {xz_path.name} - {e}"


def main():
    parser = argparse.ArgumentParser(description='Decompress .csv.xz files')
    parser.add_argument('input_folder', help='Folder containing .csv.xz files')
    parser.add_argument('output_folder', nargs='?', help='Output folder (default: input_folder/decompressed)')
    parser.add_argument('--workers', '-w', type=int, default=4, help='Number of parallel workers')
    args = parser.parse_args()
    
    input_dir = Path(args.input_folder)
    output_dir = Path(args.output_folder) if args.output_folder else input_dir / 'decompressed'
    
    if not input_dir.exists():
        print(f"Error: Input folder not found: {input_dir}")
        sys.exit(1)
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Find all .xz files
    xz_files = list(input_dir.glob('*.xz'))
    if not xz_files:
        xz_files = list(input_dir.rglob('*.xz'))  # Try recursive
    
    if not xz_files:
        print(f"No .xz files found in {input_dir}")
        sys.exit(1)
    
    print(f"Found {len(xz_files)} compressed files")
    print(f"Output directory: {output_dir}")
    print(f"Using {args.workers} workers\n")
    
    # Decompress in parallel
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = [
            executor.submit(decompress_file, f, output_dir)
            for f in xz_files
        ]
        
        for i, future in enumerate(futures, 1):
            result = future.result()
            print(f"[{i}/{len(xz_files)}] {result}")
    
    print(f"\nDone! Decompressed files are in: {output_dir}")
    print("You can now load these CSV files in Open Ephys File Reader.")


if __name__ == '__main__':
    main()
