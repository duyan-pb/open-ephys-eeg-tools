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

#include "LSLOutlet.h"
#include "LSLOutletEditor.h"

LSLOutlet::LSLOutlet()
    : GenericProcessor("LSL Outlet"),
      streamName("OpenEphys"),
      streamType("EEG"),
      dataScale(1.0f),
      includeMarkers(true),
      forwardBroadcasts(true),
      streaming(false),
      totalSamplesPushed(0),
      broadcastMessagesForwarded(0)
{
    // Generate a unique source ID based on time
    sourceId = String("OpenEphys_") + String(Time::currentTimeMillis());
}

LSLOutlet::~LSLOutlet()
{
    destroyOutlets();
}

AudioProcessorEditor* LSLOutlet::createEditor()
{
    editor = std::make_unique<LSLOutletEditor>(this);
    return editor.get();
}

void LSLOutlet::registerParameters()
{
    addStringParameter(Parameter::GLOBAL_SCOPE,
                       "stream_name", "Stream Name",
                       "LSL stream name prefix",
                       streamName);

    addStringParameter(Parameter::GLOBAL_SCOPE,
                       "stream_type", "Stream Type",
                       "LSL stream content type (EEG, EMG, etc.)",
                       streamType);

    addIntParameter(Parameter::GLOBAL_SCOPE,
                    "scale", "Scale",
                    "Scale factor applied to data samples",
                    (int)dataScale, 1, 10000);

    addBooleanParameter(Parameter::GLOBAL_SCOPE,
                        "include_markers", "Include Markers",
                        "Stream TTL events as LSL markers",
                        includeMarkers);

    addBooleanParameter(Parameter::GLOBAL_SCOPE,
                        "forward_broadcasts", "Forward Broadcasts",
                        "Forward broadcast messages as LSL markers",
                        forwardBroadcasts);
}

void LSLOutlet::parameterValueChanged(Parameter* param)
{
    if (param->getName() == "stream_name")
    {
        streamName = param->getValueAsString();
    }
    else if (param->getName() == "stream_type")
    {
        streamType = param->getValueAsString();
    }
    else if (param->getName() == "scale")
    {
        dataScale = (float)param->getValue();
    }
    else if (param->getName() == "include_markers")
    {
        includeMarkers = (bool)param->getValue();
    }
    else if (param->getName() == "forward_broadcasts")
    {
        forwardBroadcasts = (bool)param->getValue();
    }
}

void LSLOutlet::updateSettings()
{
    // Settings will be applied when acquisition starts
}

void LSLOutlet::setStreamName(const String& name)
{
    if (!streaming)
    {
        streamName = name;
        Parameter* param = getParameter("stream_name");
        if (param) param->setNextValue(name);
    }
}

void LSLOutlet::setStreamType(const String& type)
{
    if (!streaming)
    {
        streamType = type;
        Parameter* param = getParameter("stream_type");
        if (param) param->setNextValue(type);
    }
}

void LSLOutlet::setDataScale(float scale)
{
    dataScale = scale;
    Parameter* param = getParameter("scale");
    if (param) param->setNextValue((int)scale);
}

void LSLOutlet::setIncludeMarkers(bool include)
{
    if (!streaming)
    {
        includeMarkers = include;
        Parameter* param = getParameter("include_markers");
        if (param) param->setNextValue(include ? 1.0f : 0.0f);
    }
}

int LSLOutlet::getNumConsumers() const
{
    if (dataOutlets.empty())
        return 0;
    
    // Return consumer count for first outlet
    auto it = dataOutlets.begin();
    if (it != dataOutlets.end() && it->second)
    {
        return it->second->have_consumers() ? 1 : 0;
    }
    return 0;
}

void LSLOutlet::createOutlets()
{
    destroyOutlets();
    totalSamplesPushed = 0;
    broadcastMessagesForwarded = 0;

    // Create an LSL outlet for each data stream
    for (auto stream : getDataStreams())
    {
        int numChannels = stream->getChannelCount();
        float sampleRate = stream->getSampleRate();
        uint16 streamId = stream->getStreamId();

        if (numChannels > 0)
        {
            // Create stream info
            String outletName = streamName + "_" + stream->getName();
            lsl::stream_info info(
                outletName.toStdString(),
                streamType.toStdString(),
                numChannels,
                sampleRate,
                lsl::cf_float32,
                (sourceId + "_" + String(streamId)).toStdString()
            );

            // Add channel metadata
            lsl::xml_element channels = info.desc().append_child("channels");
            for (int ch = 0; ch < numChannels; ch++)
            {
                auto channel = stream->getContinuousChannels()[ch];
                lsl::xml_element chan = channels.append_child("channel");
                chan.append_child_value("label", channel->getName().toStdString());
                chan.append_child_value("unit", "uV");
                chan.append_child_value("type", streamType.toStdString());
            }

            // Add acquisition info
            lsl::xml_element acq = info.desc().append_child("acquisition");
            acq.append_child_value("manufacturer", "Open Ephys");
            acq.append_child_value("model", "Open Ephys GUI");
            acq.append_child_value("plugin", "LSL Outlet");
            acq.append_child_value("scale_factor", std::to_string(dataScale));

            // Create the outlet with a chunk size for efficiency
            int chunkSize = (int)(sampleRate / 20); // ~50ms chunks
            if (chunkSize < 1) chunkSize = 1;
            dataOutlets[streamId] = std::make_unique<lsl::stream_outlet>(info, chunkSize);

            LOGC("LSL Outlet: Created outlet '", outletName, "' with ", numChannels, 
                 " channels at ", sampleRate, " Hz (scale=", dataScale, ")");
        }
    }

    // Create marker outlet for TTL events and broadcast messages
    if (includeMarkers || forwardBroadcasts)
    {
        lsl::stream_info markerInfo(
            (streamName + "_Markers").toStdString(),
            "Markers",
            1,
            lsl::IRREGULAR_RATE,
            lsl::cf_string,
            (sourceId + "_markers").toStdString()
        );

        // Add marker metadata
        lsl::xml_element desc = markerInfo.desc();
        desc.append_child_value("manufacturer", "Open Ephys");
        desc.append_child_value("ttl_format", "TTL_Line<N>_State<0|1>");
        desc.append_child_value("broadcast_prefix", "BROADCAST:");

        markerOutlet = std::make_unique<lsl::stream_outlet>(markerInfo);
        LOGC("LSL Outlet: Created marker outlet '", streamName, "_Markers'");
    }
}

void LSLOutlet::destroyOutlets()
{
    if (totalSamplesPushed > 0)
    {
        LOGC("LSL Outlet: Session ended. Total samples: ", totalSamplesPushed,
             ", Broadcasts forwarded: ", broadcastMessagesForwarded);
    }
    dataOutlets.clear();
    markerOutlet.reset();
}

bool LSLOutlet::startAcquisition()
{
    createOutlets();
    streaming = true;
    LOGC("LSL Outlet: Started streaming");
    return true;
}

bool LSLOutlet::stopAcquisition()
{
    streaming = false;
    destroyOutlets();
    LOGC("LSL Outlet: Stopped streaming");
    return true;
}

void LSLOutlet::process(AudioBuffer<float>& buffer)
{
    if (!streaming)
        return;

    // Process each data stream
    for (auto stream : getDataStreams())
    {
        uint16 streamId = stream->getStreamId();
        
        // Check if we have an outlet for this stream
        auto it = dataOutlets.find(streamId);
        if (it == dataOutlets.end() || !it->second)
            continue;

        int numChannels = stream->getChannelCount();
        int numSamples = getNumSamplesInBlock(streamId);

        if (numSamples == 0 || numChannels == 0)
            continue;

        // Resize buffer if needed
        size_t requiredSize = (size_t)(numChannels * numSamples);
        if (sampleBuffer.size() < requiredSize)
        {
            sampleBuffer.resize(requiredSize);
        }

        // Copy samples in channel-major order (LSL expects interleaved data)
        // Apply scale factor like the Inlet does
        for (int sample = 0; sample < numSamples; sample++)
        {
            for (int ch = 0; ch < numChannels; ch++)
            {
                int globalIndex = stream->getContinuousChannels()[ch]->getGlobalIndex();
                sampleBuffer[sample * numChannels + ch] = 
                    buffer.getSample(globalIndex, sample) * dataScale;
            }
        }

        // Push the chunk to LSL
        it->second->push_chunk_multiplexed(sampleBuffer.data(), 
                                           (unsigned long)(numSamples * numChannels));
        totalSamplesPushed += numSamples;
    }
}

void LSLOutlet::handleTTLEvent(TTLEventPtr event)
{
    if (!streaming || !markerOutlet || !includeMarkers)
        return;

    // Create marker string with TTL info
    String marker = "TTL_Line" + String(event->getLine()) + 
                    "_State" + String(event->getState() ? "1" : "0");
    
    std::string markerStr = marker.toStdString();
    markerOutlet->push_sample(&markerStr);
}

void LSLOutlet::handleBroadcastMessage(const String& message, int64 messageTime)
{
    if (!streaming || !markerOutlet || !forwardBroadcasts)
        return;

    // Forward broadcast message as marker with prefix
    String marker = "BROADCAST:" + message;
    std::string markerStr = marker.toStdString();
    markerOutlet->push_sample(&markerStr);
    broadcastMessagesForwarded++;
}
