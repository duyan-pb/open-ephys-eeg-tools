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

#include "EDFFileSource.h"

EDFFileSource::EDFFileSource()
    : currentRecord(-1),
      currentSample(0),
      annotationSignalIndex(-1),
      sampleRate(0),
      numChannels(0),
      totalSamples(0)
{
}

EDFFileSource::~EDFFileSource()
{
    if (fileStream)
        fileStream.reset();
}

String EDFFileSource::readAscii(int length)
{
    if (!fileStream)
        return String();

    std::vector<char> buffer(length + 1);
    fileStream->read(buffer.data(), length);
    buffer[length] = '\0';
    
    return String(buffer.data()).trim();
}

bool EDFFileSource::parseHeader()
{
    if (!fileStream)
        return false;

    fileStream->setPosition(0);

    // Version (8 bytes) - "0       " for EDF, "\xFFBIOSEMI" for BDF
    char versionBuf[9];
    fileStream->read(versionBuf, 8);
    versionBuf[8] = '\0';
    header.version = String(versionBuf).trim();
    
    // Check if BDF format
    header.isBDF = (versionBuf[0] == (char)0xFF);

    // Patient ID (80 bytes)
    header.patientId = readAscii(80);

    // Recording ID (80 bytes)
    header.recordingId = readAscii(80);

    // Start date (8 bytes) dd.mm.yy
    header.startDate = readAscii(8);

    // Start time (8 bytes) hh.mm.ss
    header.startTime = readAscii(8);

    // Header bytes (8 bytes)
    header.headerBytes = readAscii(8).getIntValue();

    // Reserved (44 bytes) - contains "EDF+C" or "EDF+D" for EDF+
    header.reserved = readAscii(44);
    header.isEDFPlus = header.reserved.startsWith("EDF+");

    // Number of data records (8 bytes)
    header.numDataRecords = readAscii(8).getIntValue();

    // Data record duration (8 bytes) in seconds
    header.dataRecordDuration = readAscii(8).getDoubleValue();

    // Number of signals (4 bytes)
    header.numSignals = readAscii(4).getIntValue();

    if (header.numSignals <= 0 || header.numSignals > 512)
    {
        LOGE("EDF: Invalid number of signals: ", header.numSignals);
        return false;
    }

    LOGC("EDF Header parsed:");
    LOGC("  Format: ", header.isBDF ? "BDF" : (header.isEDFPlus ? "EDF+" : "EDF"));
    LOGC("  Patient: ", header.patientId);
    LOGC("  Date: ", header.startDate, " ", header.startTime);
    LOGC("  Data records: ", header.numDataRecords);
    LOGC("  Record duration: ", header.dataRecordDuration, " s");
    LOGC("  Signals: ", header.numSignals);

    return true;
}

bool EDFFileSource::parseSignalHeaders()
{
    if (!fileStream || header.numSignals <= 0)
        return false;

    signals.resize(header.numSignals);
    annotationSignalIndex = -1;

    // Read all labels (16 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].label = readAscii(16);

    // Read all transducer types (80 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].transducerType = readAscii(80);

    // Read all physical dimensions (8 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].physicalDimension = readAscii(8);

    // Read all physical minimums (8 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].physicalMin = readAscii(8).getDoubleValue();

    // Read all physical maximums (8 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].physicalMax = readAscii(8).getDoubleValue();

    // Read all digital minimums (8 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].digitalMin = readAscii(8).getIntValue();

    // Read all digital maximums (8 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].digitalMax = readAscii(8).getIntValue();

    // Read all prefiltering info (80 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].prefiltering = readAscii(80);

    // Read all samples per record (8 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].numSamplesPerRecord = readAscii(8).getIntValue();

    // Read all reserved fields (32 bytes each)
    for (int i = 0; i < header.numSignals; i++)
        signals[i].reserved = readAscii(32);

    // Calculate derived values and find annotation channel
    for (int i = 0; i < header.numSignals; i++)
    {
        EDFSignal& sig = signals[i];

        // Check for annotation signal
        if (sig.label.contains("Annotation") || sig.label == "EDF Annotations")
        {
            annotationSignalIndex = i;
            LOGC("  Found annotation channel at index ", i);
        }

        // Calculate scale factor and offset for digital to physical conversion
        // Physical = (Digital - offset) * scaleFactor
        double digitalRange = sig.digitalMax - sig.digitalMin;
        double physicalRange = sig.physicalMax - sig.physicalMin;
        
        if (digitalRange != 0)
        {
            sig.scaleFactor = physicalRange / digitalRange;
            sig.offset = sig.physicalMax / sig.scaleFactor - sig.digitalMax;
        }
        else
        {
            sig.scaleFactor = 1.0;
            sig.offset = 0.0;
        }

        sig.totalSamples = (int64)sig.numSamplesPerRecord * header.numDataRecords;

        LOGC("  Signal ", i, ": ", sig.label, 
             " (", sig.numSamplesPerRecord, " samples/record, ",
             sig.physicalDimension, ")");
    }

    return true;
}

float EDFFileSource::digitalToPhysical(int signalIndex, int digitalValue)
{
    const EDFSignal& sig = signals[signalIndex];
    return (float)((digitalValue + sig.offset) * sig.scaleFactor);
}

bool EDFFileSource::readDataRecord(int recordIndex)
{
    if (!fileStream || recordIndex < 0 || recordIndex >= header.numDataRecords)
        return false;

    if (recordIndex == currentRecord)
        return true;  // Already loaded

    // Calculate position of this data record
    int64 recordSize = 0;
    for (int i = 0; i < header.numSignals; i++)
    {
        int bytesPerSample = header.isBDF ? 3 : 2;
        recordSize += signals[i].numSamplesPerRecord * bytesPerSample;
    }

    int64 recordPos = header.headerBytes + (int64)recordIndex * recordSize;
    fileStream->setPosition(recordPos);

    // Resize buffers if needed
    if (header.isBDF)
    {
        recordBuffer24.resize(header.numSignals);
        for (int i = 0; i < header.numSignals; i++)
            recordBuffer24[i].resize(signals[i].numSamplesPerRecord);
    }
    else
    {
        recordBuffer.resize(header.numSignals);
        for (int i = 0; i < header.numSignals; i++)
            recordBuffer[i].resize(signals[i].numSamplesPerRecord);
    }

    // Read each signal's data for this record
    for (int sig = 0; sig < header.numSignals; sig++)
    {
        int numSamples = signals[sig].numSamplesPerRecord;

        if (header.isBDF)
        {
            // 24-bit samples (3 bytes, little-endian, two's complement)
            for (int s = 0; s < numSamples; s++)
            {
                uint8 bytes[3];
                fileStream->read(bytes, 3);
                int32 value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
                // Sign extend
                if (value & 0x800000)
                    value |= 0xFF000000;
                recordBuffer24[sig][s] = value;
            }
        }
        else
        {
            // 16-bit samples (2 bytes, little-endian)
            for (int s = 0; s < numSamples; s++)
            {
                int16 value;
                fileStream->read(&value, 2);
                recordBuffer[sig][s] = value;
            }
        }
    }

    currentRecord = recordIndex;
    return true;
}

bool EDFFileSource::open(File file)
{
    // Reset all state from previous file
    if (fileStream)
        fileStream.reset();
    signals.clear();
    recordBuffer.clear();
    recordBuffer24.clear();
    annotations.clear();
    header = EDFHeader();  // Reset header to defaults
    currentRecord = -1;
    currentSample = 0;
    annotationSignalIndex = -1;
    sampleRate = 0;
    numChannels = 0;
    totalSamples = 0;
    
    fileStream = std::make_unique<FileInputStream>(file);

    if (!fileStream->openedOk())
    {
        LOGE("EDF: Failed to open file: ", file.getFullPathName());
        fileStream.reset();
        return false;
    }

    // Parse header
    if (!parseHeader())
    {
        LOGE("EDF: Failed to parse header");
        fileStream.reset();
        return false;
    }

    // Parse signal headers
    if (!parseSignalHeaders())
    {
        LOGE("EDF: Failed to parse signal headers");
        fileStream.reset();
        return false;
    }

    // Determine sample rate and channel count (excluding annotation channel)
    numChannels = 0;
    sampleRate = 0;

    for (int i = 0; i < header.numSignals; i++)
    {
        if (i == annotationSignalIndex)
            continue;

        numChannels++;
        
        // Calculate sample rate from samples per record and record duration
        double sigSampleRate = signals[i].numSamplesPerRecord / header.dataRecordDuration;
        
        if (sampleRate == 0)
            sampleRate = sigSampleRate;
        else if (std::abs(sampleRate - sigSampleRate) > 0.001)
        {
            LOGC("EDF: Warning - signals have different sample rates, using first: ", sampleRate);
        }
    }

    if (numChannels > 0)
        totalSamples = signals[0].totalSamples;  // Assuming uniform sampling

    LOGC("EDF: Opened successfully - ", numChannels, " channels at ", sampleRate, " Hz");

    return true;
}

void EDFFileSource::fillRecordInfo()
{
    infoArray.clear();

    // Create one record info for the entire file
    RecordInfo info;
    info.name = "EDF Recording";
    info.sampleRate = (float)sampleRate;
    info.numSamples = totalSamples;
    info.startSampleNumber = 0;

    // Add channel info
    for (int i = 0; i < header.numSignals; i++)
    {
        if (i == annotationSignalIndex)
            continue;

        RecordedChannelInfo chInfo;
        chInfo.name = signals[i].label;
        
        // Convert units to microvolts if needed
        String unit = signals[i].physicalDimension.toLowerCase();
        if (unit.contains("mv") || unit.contains("millivolt"))
            chInfo.bitVolts = (float)(signals[i].scaleFactor * 1000.0);  // mV to µV
        else if (unit.contains("v") && !unit.contains("uv") && !unit.contains("µv"))
            chInfo.bitVolts = (float)(signals[i].scaleFactor * 1000000.0);  // V to µV
        else
            chInfo.bitVolts = (float)signals[i].scaleFactor;  // Already in µV or unknown
        
        chInfo.type = 0;  // Continuous data
        info.channels.add(chInfo);
    }

    infoArray.add(info);
    numRecords = 1;
}

void EDFFileSource::updateActiveRecord(int index)
{
    if (index >= 0 && index < numRecords)
    {
        activeRecord = index;
        currentSample = 0;
        currentRecord = -1;  // Force reload
    }
}

void EDFFileSource::seekTo(int64 sampleNumber)
{
    if (sampleNumber < 0)
        sampleNumber = 0;
    if (sampleNumber >= totalSamples)
        sampleNumber = totalSamples - 1;
    
    currentSample = sampleNumber;
}

int EDFFileSource::readData(float* buffer, int nSamples)
{
    if (!fileStream || numChannels == 0)
        return 0;

    int samplesRead = 0;
    int samplesPerRecord = signals[0].numSamplesPerRecord;

    while (samplesRead < nSamples && currentSample < totalSamples)
    {
        // Determine which data record and sample within record
        int recordIndex = (int)(currentSample / samplesPerRecord);
        int sampleInRecord = (int)(currentSample % samplesPerRecord);

        // Load data record if needed
        if (!readDataRecord(recordIndex))
            break;

        // Copy samples for all channels (interleaved: s1ch1, s1ch2, ..., s2ch1, s2ch2, ...)
        int chIndex = 0;
        for (int sig = 0; sig < header.numSignals; sig++)
        {
            if (sig == annotationSignalIndex)
                continue;

            int digitalValue;
            if (header.isBDF)
                digitalValue = recordBuffer24[sig][sampleInRecord];
            else
                digitalValue = recordBuffer[sig][sampleInRecord];

            buffer[samplesRead * numChannels + chIndex] = digitalToPhysical(sig, digitalValue);
            chIndex++;
        }

        samplesRead++;
        currentSample++;
    }

    return samplesRead;
}

void EDFFileSource::processEventData(EventInfo& info, int64 fromSampleNumber, int64 toSampleNumber)
{
    // EDF+ annotations would be processed here
    // For now, we return empty event info
    info.channels.clear();
    info.channelStates.clear();
    info.sampleNumbers.clear();
    info.text.clear();
}
