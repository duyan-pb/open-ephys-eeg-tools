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

#include "CSVFileSource.h"
#include "XZDecompress.h"
#include <sstream>

CSVFileSource::CSVFileSource()
    : numChannels(0),
      numSamples(0),
      sampleRate(1000.0f),  // Default sample rate
      delimiter(','),
      hasHeader(false),
      currentSample(0)
{
}

CSVFileSource::~CSVFileSource()
{
}

char CSVFileSource::detectDelimiter(const String& line)
{
    // Count occurrences of common delimiters
    int commas = 0, tabs = 0, semicolons = 0;
    
    for (int i = 0; i < line.length(); i++)
    {
        char c = line[i];
        if (c == ',') commas++;
        else if (c == '\t') tabs++;
        else if (c == ';') semicolons++;
    }
    
    // Return the most common delimiter
    if (tabs >= commas && tabs >= semicolons)
        return '\t';
    else if (semicolons >= commas)
        return ';';
    else
        return ',';
}

bool CSVFileSource::detectHeader(const String& line)
{
    // Try to parse as numbers - if all fail, it's probably a header
    String delimStr = String::charToString(delimiter);
    StringArray tokens;
    tokens.addTokens(line, delimStr, "\"");
    
    if (tokens.size() == 0)
        return false;
    
    int numericCount = 0;
    for (int i = 0; i < tokens.size(); i++)
    {
        String token = tokens[i].trim();
        if (token.isEmpty())
            continue;
            
        // Try to convert to float
        bool isNumeric = true;
        bool hasDecimal = false;
        bool hasSign = false;
        
        for (int j = 0; j < token.length(); j++)
        {
            char c = token[j];
            if (c == '-' || c == '+')
            {
                if (hasSign || j > 0) { isNumeric = false; break; }
                hasSign = true;
            }
            else if (c == '.')
            {
                if (hasDecimal) { isNumeric = false; break; }
                hasDecimal = true;
            }
            else if (c == 'e' || c == 'E')
            {
                // Scientific notation - allow it
            }
            else if (!CharacterFunctions::isDigit(c))
            {
                isNumeric = false;
                break;
            }
        }
        
        if (isNumeric)
            numericCount++;
    }
    
    // If less than half are numeric, it's probably a header
    return numericCount < tokens.size() / 2;
}

std::vector<float> CSVFileSource::parseLine(const String& line, char delim)
{
    std::vector<float> values;
    String delimStr = String::charToString(delim);
    StringArray tokens;
    tokens.addTokens(line, delimStr, "\"");
    
    for (int i = 0; i < tokens.size(); i++)
    {
        String token = tokens[i].trim();
        if (token.isNotEmpty())
        {
            float val = token.getFloatValue();
            values.push_back(val);
        }
    }
    
    return values;
}

bool CSVFileSource::open(File file)
{
    // Reset all state from previous file
    data.clear();
    channelNames.clear();
    numChannels = 0;
    numSamples = 0;
    sampleRate = 1000.0f;
    delimiter = ',';
    hasHeader = false;
    currentSample = 0;
    
    // Read file - automatically decompress if XZ compressed
    StringArray lines;
    
    if (XZDecompress::hasXZExtension(file) || XZDecompress::isXZFile(file))
    {
        LOGC("CSV: Detected XZ compressed file");
        if (!XZDecompress::readFileLines(file, lines))
        {
            LOGE("CSV: Failed to decompress XZ file: ", file.getFullPathName());
            return false;
        }
    }
    else
    {
        file.readLines(lines);
    }
    
    if (lines.size() < 2)
    {
        LOGE("CSV: File too short: ", file.getFullPathName());
        return false;
    }
    
    // Detect delimiter from first line
    delimiter = detectDelimiter(lines[0]);
    String delimDisplay = (delimiter == '\t') ? "TAB" : String::charToString(delimiter);
    LOGC("CSV: Detected delimiter: '", delimDisplay, "'");
    
    // Detect if first line is header
    hasHeader = detectHeader(lines[0]);
    LOGC("CSV: Has header: ", hasHeader ? "yes" : "no");
    
    int startLine = 0;
    int timeColumnIndex = -1;  // Index of time column to skip
    
    // Parse header if present
    if (hasHeader)
    {
        String delimStr = String::charToString(delimiter);
        StringArray tokens;
        tokens.addTokens(lines[0], delimStr, "\"");
        for (int i = 0; i < tokens.size(); i++)
        {
            String name = tokens[i].trim().toLowerCase();
            // Check if this is a time column
            if (name == "time" || name == "timestamp" || name == "t" || name == "seconds" || name == "sec")
            {
                timeColumnIndex = i;
                LOGC("CSV: Found time column at index ", i);
            }
            else if (tokens[i].trim().isNotEmpty())
            {
                channelNames.push_back(tokens[i].trim());  // Use original case
            }
        }
        startLine = 1;
    }
    
    // Parse data and detect sample rate from time column
    numChannels = 0;
    float firstTime = 0.0f;
    float secondTime = 0.0f;
    bool sampleRateDetected = false;
    
    for (int i = startLine; i < lines.size(); i++)
    {
        String line = lines[i].trim();
        if (line.isEmpty())
            continue;
            
        std::vector<float> allValues = parseLine(line, delimiter);
        
        if (allValues.empty())
            continue;
        
        // Extract time value if present (for sample rate detection)
        if (timeColumnIndex >= 0 && timeColumnIndex < (int)allValues.size())
        {
            float timeVal = allValues[timeColumnIndex];
            if (data.empty())
                firstTime = timeVal;
            else if (data.size() == 1 && !sampleRateDetected)
            {
                secondTime = timeVal;
                float dt = secondTime - firstTime;
                if (dt > 0)
                {
                    sampleRate = 1.0f / dt;
                    sampleRateDetected = true;
                    LOGC("CSV: Detected sample rate from time column: ", sampleRate, " Hz");
                }
            }
        }
        
        // Build channel data (excluding time column)
        std::vector<float> values;
        for (int j = 0; j < (int)allValues.size(); j++)
        {
            if (j != timeColumnIndex)
                values.push_back(allValues[j]);
        }
        
        if (values.empty())
            continue;
        
        // Set number of channels from first data line
        if (numChannels == 0)
        {
            numChannels = (int)values.size();
            LOGC("CSV: Detected ", numChannels, " channels (excluding time column)");
        }
        
        // Ensure consistent channel count
        if ((int)values.size() == numChannels)
        {
            data.push_back(values);
        }
        else
        {
            LOGC("CSV: Skipping line ", i, " - expected ", numChannels, " values, got ", values.size());
        }
    }
    
    numSamples = (int64)data.size();
    
    if (numSamples == 0 || numChannels == 0)
    {
        LOGE("CSV: No valid data found");
        return false;
    }
    
    // Generate channel names if not from header
    if (channelNames.empty())
    {
        for (int i = 0; i < numChannels; i++)
            channelNames.push_back("Ch" + String(i + 1));
    }
    // Ensure channel names match channel count
    else if ((int)channelNames.size() != numChannels)
    {
        LOGC("CSV: Adjusting channel names to match data (", channelNames.size(), " -> ", numChannels, ")");
        while ((int)channelNames.size() > numChannels)
            channelNames.pop_back();
        while ((int)channelNames.size() < numChannels)
            channelNames.push_back("Ch" + String((int)channelNames.size() + 1));
    }
    
    // Try to detect sample rate from filename if not detected from time column
    if (!sampleRateDetected)
    {
        String filename = file.getFileNameWithoutExtension().toLowerCase();
        for (int rate : {256, 250, 200, 512, 500, 1000, 1024, 2000, 2048, 5000})
        {
            if (filename.contains(String(rate)))
            {
                sampleRate = (float)rate;
                sampleRateDetected = true;
                break;
            }
        }
    }
    
    LOGC("CSV: Loaded ", numSamples, " samples x ", numChannels, " channels at ", sampleRate, " Hz (assumed)");
    
    return true;
}

void CSVFileSource::fillRecordInfo()
{
    infoArray.clear();
    
    RecordInfo info;
    info.name = "CSV Recording";
    info.sampleRate = sampleRate;
    info.numSamples = numSamples;
    info.startSampleNumber = 0;
    
    for (int i = 0; i < numChannels; i++)
    {
        RecordedChannelInfo chInfo;
        chInfo.name = channelNames[i];
        chInfo.bitVolts = 1.0f;  // Assume data is already in microvolts
        chInfo.type = 0;
        info.channels.add(chInfo);
    }
    
    infoArray.add(info);
    numRecords = 1;
}

void CSVFileSource::updateActiveRecord(int index)
{
    if (index >= 0 && index < numRecords)
    {
        activeRecord = index;
        currentSample = 0;
    }
}

void CSVFileSource::seekTo(int64 sampleNumber)
{
    if (sampleNumber < 0)
        sampleNumber = 0;
    if (sampleNumber >= numSamples)
        sampleNumber = numSamples - 1;
    
    currentSample = sampleNumber;
}

int CSVFileSource::readData(float* buffer, int nSamples)
{
    int samplesRead = 0;
    
    while (samplesRead < nSamples && currentSample < numSamples)
    {
        // Copy sample data (interleaved)
        for (int ch = 0; ch < numChannels; ch++)
        {
            buffer[samplesRead * numChannels + ch] = data[currentSample][ch];
        }
        
        samplesRead++;
        currentSample++;
    }
    
    return samplesRead;
}

void CSVFileSource::processEventData(EventInfo& info, int64 fromSampleNumber, int64 toSampleNumber)
{
    // No events in CSV files
    info.channels.clear();
    info.channelStates.clear();
    info.sampleNumbers.clear();
    info.text.clear();
}
