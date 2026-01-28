"""
Read and analyze EEG annotation CSV files and clinical information.

Usage:
    python read_annotations.py                    # Show all available data
    python read_annotations.py --annotations      # Analyze annotation files
    python read_annotations.py --clinical         # Show clinical information
    python read_annotations.py --summary          # Summary statistics
    python read_annotations.py --help             # Show help
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
    import pandas as pd
except ImportError:
    print("ERROR: pandas not installed. Install with: pip install pandas")
    sys.exit(1)


# Default data directories (relative to this script)
DATA_DIRS = {
    'eeg_data': os.path.join(os.path.dirname(__file__), '..', '..', 'test_data', 'eeg_data'),
    'eeg_data_2': os.path.join(os.path.dirname(__file__), '..', '..', 'test_data', 'eeg_data_2'),
}


def load_annotations(csv_path):
    """
    Load annotations CSV file.
    
    The annotation files contain binary labels (0/1) for each time sample.
    - Each row represents one time sample
    - Each column represents one annotation label/channel (e.g., seizure regions)
    - Values: 0 = annotation absent, 1 = annotation present
    """
    print(f"\n{'='*70}")
    print(f"  ANNOTATION FILE")
    print(f"{'='*70}")
    print(f"  File: {os.path.basename(csv_path)}")
    
    if not os.path.exists(csv_path):
        print(f"  ERROR: File not found!")
        return None
    
    df = pd.read_csv(csv_path)
    
    print(f"  Shape: {df.shape[0]} samples x {df.shape[1]} labels")
    print(f"  Column names: {list(df.columns[:10])}... (showing first 10)")
    
    # Analyze annotations
    annotations = df.values
    total_positive = annotations.sum()
    samples_with_annotations = (annotations.sum(axis=1) > 0).sum()
    
    print(f"\n  Annotation Statistics:")
    print(f"  {'-'*50}")
    print(f"  Total positive annotations:     {total_positive:,}")
    print(f"  Samples with any annotation:    {samples_with_annotations:,} ({100*samples_with_annotations/df.shape[0]:.1f}%)")
    print(f"  Samples without annotation:     {df.shape[0] - samples_with_annotations:,}")
    
    # Per-column statistics
    print(f"\n  Per-Label Statistics (top 10 by count):")
    print(f"  {'-'*50}")
    label_counts = annotations.sum(axis=0)
    sorted_idx = np.argsort(label_counts)[::-1]
    
    for i in sorted_idx[:10]:
        count = label_counts[i]
        pct = 100 * count / df.shape[0]
        print(f"    Label {df.columns[i]:5s}: {count:6,} annotations ({pct:5.1f}%)")
    
    print(f"{'='*70}\n")
    
    return df


def load_clinical_info(csv_path):
    """
    Load clinical information CSV file.
    
    Contains patient demographics, diagnoses, and EEG recording metadata.
    """
    print(f"\n{'='*70}")
    print(f"  CLINICAL INFORMATION")
    print(f"{'='*70}")
    print(f"  File: {os.path.basename(csv_path)}")
    
    if not os.path.exists(csv_path):
        print(f"  ERROR: File not found!")
        return None
    
    df = pd.read_csv(csv_path)
    
    print(f"  Total EEG recordings: {len(df)}")
    print(f"  Columns: {list(df.columns)}")
    
    # Show summary statistics
    print(f"\n  Summary Statistics:")
    print(f"  {'-'*50}")
    
    # Gender distribution
    if 'Gender' in df.columns:
        gender_counts = df['Gender'].value_counts()
        print(f"  Gender distribution:")
        for g, c in gender_counts.items():
            print(f"    {g}: {c}")
    
    # Diagnoses
    if 'Diagnosis' in df.columns:
        print(f"\n  Common diagnoses:")
        diag_counts = df['Diagnosis'].value_counts().head(10)
        for d, c in diag_counts.items():
            print(f"    {d[:40]:40s}: {c}")
    
    # Seizure annotations
    if 'Number of Reviewers Annotating Seizure' in df.columns:
        seizure_col = 'Number of Reviewers Annotating Seizure'
        with_seizures = (df[seizure_col] > 0).sum()
        print(f"\n  Recordings with seizure annotations: {with_seizures} ({100*with_seizures/len(df):.1f}%)")
    
    print(f"{'='*70}\n")
    
    return df


def show_clinical_table(df, max_rows=20):
    """Display clinical information as a formatted table."""
    if df is None:
        return
    
    # Select key columns
    cols_to_show = ['ID', 'EEG file', 'Gender', 'GA (weeks)', 'Diagnosis', 
                    'Number of Reviewers Annotating Seizure', 'Primary Localisation']
    cols_available = [c for c in cols_to_show if c in df.columns]
    
    print(f"\n  Clinical Data Table (first {max_rows} rows):")
    print(f"  {'-'*100}")
    
    # Header
    header = "  "
    for col in cols_available:
        width = min(20, max(len(col), 10))
        header += f"{col[:width]:<{width}}  "
    print(header)
    print(f"  {'='*100}")
    
    # Rows
    for idx, row in df.head(max_rows).iterrows():
        line = "  "
        for col in cols_available:
            val = str(row[col])[:20] if pd.notna(row[col]) else "N/A"
            width = min(20, max(len(col), 10))
            line += f"{val:<{width}}  "
        print(line)


def analyze_eeg_file_mapping(clinical_df, annotations_df, data_dir):
    """Analyze mapping between clinical info, annotations, and EDF files."""
    print(f"\n{'='*70}")
    print(f"  EEG FILE MAPPING ANALYSIS")
    print(f"{'='*70}")
    
    # List EDF files
    edf_files = []
    if os.path.exists(data_dir):
        edf_files = sorted([f for f in os.listdir(data_dir) if f.endswith('.edf')])
    
    print(f"  EDF files found: {len(edf_files)}")
    
    if clinical_df is not None:
        clinical_eeg_files = clinical_df['EEG file'].tolist() if 'EEG file' in clinical_df.columns else []
        print(f"  Clinical info entries: {len(clinical_eeg_files)}")
        
        # Check for matches
        matched = []
        for edf in edf_files:
            base_name = os.path.splitext(edf)[0]
            if base_name in clinical_eeg_files:
                matched.append(base_name)
        
        print(f"  Matched EDF files with clinical info: {len(matched)}")
    
    if annotations_df is not None:
        print(f"  Annotation samples per recording: {annotations_df.shape[0]}")
    
    print(f"{'='*70}\n")


def load_metadata_eeg_data_2(metadata_path):
    """
    Load metadata CSV file from eeg_data_2 dataset.
    
    Contains EEG epoch information, grades, and quality comments.
    """
    print(f"\n{'='*70}")
    print(f"  EEG METADATA (eeg_data_2)")
    print(f"{'='*70}")
    print(f"  File: {os.path.basename(metadata_path)}")
    
    if not os.path.exists(metadata_path):
        print(f"  ERROR: File not found!")
        return None
    
    df = pd.read_csv(metadata_path)
    
    print(f"  Total epochs: {len(df)}")
    print(f"  Columns: {list(df.columns)}")
    
    # Summary statistics
    print(f"\n  Summary Statistics:")
    print(f"  {'-'*50}")
    
    # Unique babies
    if 'baby_ID' in df.columns:
        unique_babies = df['baby_ID'].nunique()
        print(f"  Unique babies: {unique_babies}")
    
    # Grade distribution
    if 'grade' in df.columns:
        print(f"\n  Grade Distribution:")
        grade_counts = df['grade'].value_counts().sort_index()
        grade_labels = {
            1: "Normal/mild",
            2: "Moderately abnormal",
            3: "Severely abnormal",
            4: "Inactive"
        }
        for grade, count in grade_counts.items():
            label = grade_labels.get(grade, "Unknown")
            print(f"    Grade {grade} ({label:20s}): {count:3d} epochs ({100*count/len(df):5.1f}%)")
    
    # Sampling frequency
    if 'sampling_freq' in df.columns:
        print(f"\n  Sampling Frequencies:")
        freq_counts = df['sampling_freq'].value_counts()
        for freq, count in freq_counts.items():
            print(f"    {freq} Hz: {count} epochs")
    
    # Seizures
    if 'seizures_YN' in df.columns:
        with_seizures = (df['seizures_YN'] == 'Y').sum()
        print(f"\n  Epochs with seizures: {with_seizures} ({100*with_seizures/len(df):.1f}%)")
    
    print(f"{'='*70}\n")
    
    return df


def load_eeg_grades(grades_path):
    """
    Load EEG grades file from eeg_data_2 dataset.
    """
    print(f"\n{'='*70}")
    print(f"  EEG GRADES (eeg_data_2)")
    print(f"{'='*70}")
    print(f"  File: {os.path.basename(grades_path)}")
    
    if not os.path.exists(grades_path):
        print(f"  ERROR: File not found!")
        return None
    
    df = pd.read_csv(grades_path)
    
    print(f"  Total entries: {len(df)}")
    print(f"  Columns: {list(df.columns)}")
    
    print(f"{'='*70}\n")
    
    return df


def create_combined_dataset(clinical_path, annotations_path, output_path=None):
    """
    Create a combined dataset merging clinical info with annotations.
    Useful for machine learning pipelines.
    """
    clinical_df = pd.read_csv(clinical_path)
    annotations_df = pd.read_csv(annotations_path)
    
    # Create summary per EEG file
    summary_data = []
    
    for idx, row in clinical_df.iterrows():
        eeg_file = row.get('EEG file', f'eeg{idx+1}')
        
        summary = {
            'eeg_file': eeg_file,
            'gender': row.get('Gender', 'N/A'),
            'ga_weeks': row.get('GA (weeks)', 'N/A'),
            'diagnosis': row.get('Diagnosis', 'N/A'),
            'num_seizure_reviewers': row.get('Number of Reviewers Annotating Seizure', 0),
            'primary_localisation': row.get('Primary Localisation', 'N/A'),
            'has_seizures': row.get('Number of Reviewers Annotating Seizure', 0) > 0
        }
        summary_data.append(summary)
    
    summary_df = pd.DataFrame(summary_data)
    
    if output_path:
        summary_df.to_csv(output_path, index=False)
        print(f"Saved combined dataset to: {output_path}")
    
    return summary_df


def list_available_files(data_dir=None):
    """List all available data files from all directories."""
    print(f"\n{'='*70}")
    print(f"  AVAILABLE DATA FILES")
    print(f"{'='*70}")
    
    dirs_to_check = {data_dir: 'specified'} if data_dir else DATA_DIRS
    
    for name, data_path in dirs_to_check.items():
        print(f"\n  [{name}]")
        print(f"  Directory: {os.path.abspath(data_path)}")
        
        if not os.path.exists(data_path):
            print(f"  ERROR: Directory not found!")
            continue
        
        files = os.listdir(data_path)
        
        # Categorize files
        edf_files = sorted([f for f in files if f.endswith('.edf')])
        csv_files = sorted([f for f in files if f.endswith('.csv')])
        mat_files = sorted([f for f in files if f.endswith('.mat')])
        xz_files = sorted([f for f in files if f.endswith('.csv.xz')])
        subdirs = sorted([f for f in files if os.path.isdir(os.path.join(data_path, f))])
        
        # Check for nested directories (eeg_data_2 structure)
        for subdir in subdirs:
            subdir_path = os.path.join(data_path, subdir)
            nested_path = os.path.join(subdir_path, subdir)  # Double nested like CSV_format/CSV_format
            if os.path.exists(nested_path):
                nested_files = os.listdir(nested_path)
                nested_edf = [f for f in nested_files if f.endswith('.edf')]
                nested_xz = [f for f in nested_files if f.endswith('.csv.xz')]
                if nested_edf:
                    print(f"\n    Subfolder: {subdir}/{subdir}/")
                    print(f"      EDF Files: {len(nested_edf)}")
                if nested_xz:
                    print(f"\n    Subfolder: {subdir}/{subdir}/")
                    print(f"      CSV.XZ Files: {len(nested_xz)}")
        
        if edf_files:
            print(f"\n    EDF Files ({len(edf_files)}):")
            for f in edf_files[:10]:
                size = os.path.getsize(os.path.join(data_path, f)) / 1024 / 1024
                print(f"      {f:25s} ({size:6.2f} MB)")
            if len(edf_files) > 10:
                print(f"      ... and {len(edf_files) - 10} more")
        
        if csv_files:
            print(f"\n    CSV Files ({len(csv_files)}):")
            for f in csv_files:
                size = os.path.getsize(os.path.join(data_path, f)) / 1024
                print(f"      {f:40s} ({size:8.1f} KB)")
        
        if xz_files:
            print(f"\n    Compressed CSV Files ({len(xz_files)}):")
            for f in xz_files[:5]:
                size = os.path.getsize(os.path.join(data_path, f)) / 1024
                print(f"      {f:40s} ({size:8.1f} KB)")
            if len(xz_files) > 5:
                print(f"      ... and {len(xz_files) - 5} more")
        
        if mat_files:
            print(f"\n    MAT Files ({len(mat_files)}):")
            for f in mat_files:
                size = os.path.getsize(os.path.join(data_path, f)) / 1024 / 1024
                print(f"      {f:25s} ({size:6.2f} MB)")
    
    print(f"\n{'='*70}\n")


def main():
    parser = argparse.ArgumentParser(
        description='Read and analyze EEG annotation and clinical information files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Data Directories:
  eeg_data/    - Neonatal seizure dataset with annotations and clinical info
  eeg_data_2/  - HIE graded EEG dataset with metadata and grades

Examples:
  python read_annotations.py --list
  python read_annotations.py --annotations
  python read_annotations.py --clinical
  python read_annotations.py --metadata     # eeg_data_2 metadata
  python read_annotations.py --summary
        """
    )
    
    parser.add_argument('--data-dir', type=str, default=None,
                        help='Specific data directory to use')
    parser.add_argument('--list', action='store_true', 
                        help='List all available data files')
    parser.add_argument('--annotations', action='store_true',
                        help='Analyze annotation files (eeg_data)')
    parser.add_argument('--clinical', action='store_true',
                        help='Show clinical information (eeg_data)')
    parser.add_argument('--metadata', action='store_true',
                        help='Show metadata (eeg_data_2)')
    parser.add_argument('--grades', action='store_true',
                        help='Show EEG grades (eeg_data_2)')
    parser.add_argument('--summary', action='store_true',
                        help='Show summary statistics for all datasets')
    parser.add_argument('--table', action='store_true',
                        help='Show clinical data as table')
    parser.add_argument('--file', type=str,
                        help='Specific file to analyze')
    
    args = parser.parse_args()
    
    data_dir_1 = DATA_DIRS['eeg_data']
    data_dir_2 = DATA_DIRS['eeg_data_2']
    
    # If no specific action, show overview
    if not any([args.list, args.annotations, args.clinical, args.metadata, 
                args.grades, args.summary, args.table, args.file]):
        # Show file listing
        list_available_files()
        
        # Load clinical info from eeg_data
        clinical_path = os.path.join(data_dir_1, 'clinical_information.csv')
        if os.path.exists(clinical_path):
            clinical_df = load_clinical_info(clinical_path)
            show_clinical_table(clinical_df, max_rows=5)
        
        # Load metadata from eeg_data_2
        metadata_path = os.path.join(data_dir_2, 'metadata.csv')
        if os.path.exists(metadata_path):
            load_metadata_eeg_data_2(metadata_path)
        
        return
    
    if args.list:
        list_available_files(args.data_dir)
    
    if args.file:
        filepath = args.file
        # Try to find the file
        if not os.path.isabs(filepath):
            for name, data_dir in DATA_DIRS.items():
                test_path = os.path.join(data_dir, filepath)
                if os.path.exists(test_path):
                    filepath = test_path
                    break
        
        if 'annotation' in filepath.lower():
            load_annotations(filepath)
        elif 'clinical' in filepath.lower():
            df = load_clinical_info(filepath)
            show_clinical_table(df)
        elif 'metadata' in filepath.lower():
            load_metadata_eeg_data_2(filepath)
        elif 'grades' in filepath.lower():
            load_eeg_grades(filepath)
        else:
            print(f"Unknown file type: {filepath}")
    
    if args.clinical:
        clinical_path = os.path.join(data_dir_1, 'clinical_information.csv')
        df = load_clinical_info(clinical_path)
        show_clinical_table(df)
    
    if args.metadata:
        metadata_path = os.path.join(data_dir_2, 'metadata.csv')
        load_metadata_eeg_data_2(metadata_path)
    
    if args.grades:
        grades_path = os.path.join(data_dir_2, 'eeg_grades.csv')
        load_eeg_grades(grades_path)
    
    if args.annotations:
        # eeg_data annotations
        if os.path.exists(data_dir_1):
            annotation_files = sorted([f for f in os.listdir(data_dir_1) 
                                       if 'annotation' in f.lower() and f.endswith('.csv')])
            for ann_file in annotation_files:
                load_annotations(os.path.join(data_dir_1, ann_file))
    
    if args.summary:
        print(f"\n{'='*70}")
        print(f"  DATASET SUMMARY")
        print(f"{'='*70}")
        
        # eeg_data summary
        print(f"\n  [eeg_data] - Neonatal Seizure Dataset")
        print(f"  {'-'*50}")
        if os.path.exists(data_dir_1):
            edf_count = len([f for f in os.listdir(data_dir_1) if f.endswith('.edf')])
            print(f"    EDF recordings: {edf_count}")
            
            clinical_path = os.path.join(data_dir_1, 'clinical_information.csv')
            if os.path.exists(clinical_path):
                clinical_df = pd.read_csv(clinical_path)
                seizure_col = 'Number of Reviewers Annotating Seizure'
                if seizure_col in clinical_df.columns:
                    with_seizures = (clinical_df[seizure_col] > 0).sum()
                    print(f"    With seizure annotations: {with_seizures}")
                    print(f"    Without seizures: {len(clinical_df) - with_seizures}")
        else:
            print(f"    Directory not found")
        
        # eeg_data_2 summary
        print(f"\n  [eeg_data_2] - HIE Graded EEG Dataset")
        print(f"  {'-'*50}")
        edf_dir_2 = os.path.join(data_dir_2, 'EDF_format', 'EDF_format')
        if os.path.exists(edf_dir_2):
            edf_count_2 = len([f for f in os.listdir(edf_dir_2) if f.endswith('.edf')])
            print(f"    EDF epochs: {edf_count_2}")
        
        metadata_path = os.path.join(data_dir_2, 'metadata.csv')
        if os.path.exists(metadata_path):
            metadata_df = pd.read_csv(metadata_path)
            print(f"    Unique babies: {metadata_df['baby_ID'].nunique()}")
            if 'grade' in metadata_df.columns:
                print(f"    Grade distribution:")
                for grade in sorted(metadata_df['grade'].unique()):
                    count = (metadata_df['grade'] == grade).sum()
                    print(f"      Grade {grade}: {count}")
        
        print(f"\n{'='*70}\n")
    
    if args.table:
        clinical_path = os.path.join(data_dir_1, 'clinical_information.csv')
        if os.path.exists(clinical_path):
            df = pd.read_csv(clinical_path)
            show_clinical_table(df, max_rows=50)


if __name__ == "__main__":
    main()
