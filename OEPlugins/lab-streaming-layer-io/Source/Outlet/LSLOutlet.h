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

#ifndef LSLOUTLET_H_DEFINED
#define LSLOUTLET_H_DEFINED

#include <ProcessorHeaders.h>
#include <lsl_cpp.h>

#include <memory>
#include <vector>
#include <map>

/**
 * LSL Outlet Plugin
 * 
 * Streams continuous data from Open Ephys to Lab Streaming Layer,
 * allowing external applications to receive the data in real-time.
 * 
 * Features:
 * - Streams all continuous data channels via LSL
 * - Streams TTL events as string markers
 * - Configurable stream name and type
 * - Adjustable data scale factor (like LSL Inlet)
 * - Channel metadata (labels, units) included in LSL stream info
 * - Responds to broadcast messages (forwards as markers)
 * - Statistics tracking (samples pushed, consumers connected)
 * - Parameter persistence for all settings
 */
class LSLOutlet : public GenericProcessor
{
public:
    /** Constructor */
    LSLOutlet();

    /** Destructor */
    ~LSLOutlet();

    /** Creates the custom editor for this plugin */
    AudioProcessorEditor* createEditor() override;

    /** Called when settings need to be updated */
    void updateSettings() override;

    /** Called when acquisition starts */
    bool startAcquisition() override;

    /** Called when acquisition stops */
    bool stopAcquisition() override;

    /** Processes incoming data and streams it via LSL */
    void process(AudioBuffer<float>& buffer) override;

    /** Handles incoming TTL events */
    void handleTTLEvent(TTLEventPtr event) override;

    /** Register parameters for saving/loading */
    void registerParameters() override;

    /** Handle parameter changes */
    void parameterValueChanged(Parameter* param) override;

    /** Handle broadcast messages from other processors */
    void handleBroadcastMessage(const String& message, int64 messageTime) override;

    /** Get the stream name */
    String getStreamName() const { return streamName; }

    /** Set the stream name */
    void setStreamName(const String& name);

    /** Get the stream type */
    String getStreamType() const { return streamType; }

    /** Set the stream type */
    void setStreamType(const String& type);

    /** Get the data scale factor */
    float getDataScale() const { return dataScale; }

    /** Set the data scale factor */
    void setDataScale(float scale);

    /** Get whether markers are included */
    bool getIncludeMarkers() const { return includeMarkers; }

    /** Set whether to include markers */
    void setIncludeMarkers(bool include);

    /** Check if outlet is currently streaming */
    bool isStreaming() const { return streaming; }

    /** Get number of connected consumers for first data outlet */
    int getNumConsumers() const;

    /** Get total samples pushed since acquisition start */
    int64 getTotalSamplesPushed() const { return totalSamplesPushed; }

private:
    /** Create LSL outlet streams for all data streams */
    void createOutlets();

    /** Destroy all LSL outlet streams */
    void destroyOutlets();

    /** Stream name for LSL */
    String streamName;

    /** Stream type for LSL */
    String streamType;

    /** Data scale factor (like Inlet's dataScale) */
    float dataScale;

    /** Whether to include TTL markers */
    bool includeMarkers;

    /** Whether to forward broadcast messages as markers */
    bool forwardBroadcasts;

    /** Map of data stream ID to LSL outlet */
    std::map<uint16, std::unique_ptr<lsl::stream_outlet>> dataOutlets;

    /** LSL outlet for TTL events/markers */
    std::unique_ptr<lsl::stream_outlet> markerOutlet;

    /** Buffer for interleaved samples */
    std::vector<float> sampleBuffer;

    /** Whether we are currently streaming */
    bool streaming;

    /** Source ID for LSL streams */
    String sourceId;

    /** Total samples pushed (for stats) */
    int64 totalSamplesPushed;

    /** Track broadcast messages received */
    int broadcastMessagesForwarded;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSLOutlet);
};

#endif // LSLOUTLET_H_DEFINED
