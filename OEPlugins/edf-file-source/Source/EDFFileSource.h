/*
 ------------------------------------------------------------------

 This file is part of the Open Ephys GUI
 Copyright (C) 2022 Open Ephys

 ------------------------------------------------------------------

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#ifndef EDF_FILE_SOURCE_H_DEFINED
#define EDF_FILE_SOURCE_H_DEFINED

#include <FileSourceHeaders.h>
#include <vector>
#include <map>

/**
 * EDF File Source Plugin
 * 
 * Allows Open Ephys File Reader to load EDF (European Data Format) files,
 * commonly used for EEG recordings.
 * 
 * Supports:
 * - EDF (European Data Format)
 * - EDF+ (with annotations)
 * - BDF (BioSemi Data Format, 24-bit)
 */
class EDFFileSource : public FileSource
{
public:
    /** Constructor */
    EDFFileSource();

    /** Destructor */
    ~EDFFileSource();

    // ------------------------------------------------------------
    //             FileSource Pure Virtual Methods
    // ------------------------------------------------------------

    /** Attempt to open the EDF file */
    bool open(File file) override;

    /** Fill recording info arrays */
    void fillRecordInfo() override;

    /** Update which recording/stream to read from */
    void updateActiveRecord(int index) override;

    /** Seek to a specific sample number */
    void seekTo(int64 sampleNumber) override;

    /** Read samples into buffer */
    int readData(float* buffer, int nSamples) override;

    /** Process event/annotation data */
    void processEventData(EventInfo& info, int64 fromSampleNumber, int64 toSampleNumber) override;

private:
    // EDF Header structures
    struct EDFHeader
    {
        String version;
        String patientId;
        String recordingId;
        String startDate;
        String startTime;
        int headerBytes;
        String reserved;
        int numDataRecords;
        double dataRecordDuration;  // in seconds
        int numSignals;
        bool isBDF;  // BioSemi format (24-bit)
        bool isEDFPlus;
    };

    struct EDFSignal
    {
        String label;
        String transducerType;
        String physicalDimension;
        double physicalMin;
        double physicalMax;
        int digitalMin;
        int digitalMax;
        String prefiltering;
        int numSamplesPerRecord;
        String reserved;
        
        // Derived values
        double scaleFactor;
        double offset;
        int64 totalSamples;
    };

    struct EDFAnnotation
    {
        double onset;       // Time in seconds
        double duration;    // Duration in seconds
        String annotation;  // Annotation text
    };

    /** Parse the EDF header */
    bool parseHeader();

    /** Parse signal headers */
    bool parseSignalHeaders();

    /** Parse annotations from EDF+ */
    void parseAnnotations();

    /** Read a fixed-length ASCII string from file */
    String readAscii(int length);

    /** Read a data record from file */
    bool readDataRecord(int recordIndex);

    /** Convert digital value to physical value */
    float digitalToPhysical(int signalIndex, int digitalValue);

    // File handle
    std::unique_ptr<FileInputStream> fileStream;

    // Header information
    EDFHeader header;
    std::vector<EDFSignal> signals;
    std::vector<EDFAnnotation> annotations;

    // Data record buffer
    std::vector<std::vector<int16>> recordBuffer;  // [signal][sample]
    std::vector<std::vector<int32>> recordBuffer24; // For BDF (24-bit)
    int currentRecord;

    // Reading state
    int64 currentSample;
    int annotationSignalIndex;  // Index of annotation signal in EDF+, -1 if none

    // Computed values
    double sampleRate;
    int numChannels;
    int64 totalSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EDFFileSource);
};

#endif // EDF_FILE_SOURCE_H_DEFINED
