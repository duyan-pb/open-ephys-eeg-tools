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

#ifndef CSV_FILE_SOURCE_H_DEFINED
#define CSV_FILE_SOURCE_H_DEFINED

#include <FileSourceHeaders.h>
#include <vector>

/**
 * CSV File Source Plugin
 * 
 * Allows Open Ephys File Reader to load CSV files containing EEG/time-series data.
 * 
 * Expected format:
 * - Each row is a time point
 * - Each column is a channel
 * - Optional header row with channel names
 * - Values are comma, tab, or semicolon separated
 * 
 * Example:
 *   Ch1,Ch2,Ch3
 *   0.123,0.456,0.789
 *   0.234,0.567,0.890
 *   ...
 */
class CSVFileSource : public FileSource
{
public:
    /** Constructor */
    CSVFileSource();

    /** Destructor */
    ~CSVFileSource();

    // FileSource Pure Virtual Methods
    bool open(File file) override;
    void fillRecordInfo() override;
    void updateActiveRecord(int index) override;
    void seekTo(int64 sampleNumber) override;
    int readData(float* buffer, int nSamples) override;
    void processEventData(EventInfo& info, int64 fromSampleNumber, int64 toSampleNumber) override;

private:
    /** Detect the delimiter used in the file */
    char detectDelimiter(const String& line);

    /** Check if the first line is a header */
    bool detectHeader(const String& line);

    /** Parse a line into values */
    std::vector<float> parseLine(const String& line, char delimiter);

    // Data storage
    std::vector<std::vector<float>> data;  // [sample][channel]
    std::vector<String> channelNames;
    
    // File properties
    int numChannels;
    int64 numSamples;
    float sampleRate;  // Default or from file
    char delimiter;
    bool hasHeader;
    
    // Reading state
    int64 currentSample;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CSVFileSource);
};

#endif // CSV_FILE_SOURCE_H_DEFINED
