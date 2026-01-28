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

#include "LSLOutletEditor.h"
#include "LSLOutlet.h"

LSLOutletEditor::LSLOutletEditor(LSLOutlet* parentNode)
    : GenericEditor(parentNode),
      processor(parentNode)
{
    desiredWidth = 250;

    int yPos = 25;

    // Stream Name
    streamNameLabel = std::make_unique<Label>("StreamNameLabel", "Name:");
    streamNameLabel->setBounds(10, yPos, 50, 20);
    streamNameLabel->setFont(Font("Default", 12, Font::plain));
    streamNameLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(streamNameLabel.get());

    streamNameEditor = std::make_unique<Label>("StreamNameEditor", processor->getStreamName());
    streamNameEditor->setBounds(60, yPos, 90, 20);
    streamNameEditor->setFont(Font("Default", 12, Font::plain));
    streamNameEditor->setColour(Label::textColourId, Colours::white);
    streamNameEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    streamNameEditor->setEditable(true);
    streamNameEditor->addListener(this);
    addAndMakeVisible(streamNameEditor.get());

    // Stream Type
    streamTypeLabel = std::make_unique<Label>("StreamTypeLabel", "Type:");
    streamTypeLabel->setBounds(155, yPos, 40, 20);
    streamTypeLabel->setFont(Font("Default", 12, Font::plain));
    streamTypeLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(streamTypeLabel.get());

    streamTypeEditor = std::make_unique<Label>("StreamTypeEditor", processor->getStreamType());
    streamTypeEditor->setBounds(195, yPos, 45, 20);
    streamTypeEditor->setFont(Font("Default", 12, Font::plain));
    streamTypeEditor->setColour(Label::textColourId, Colours::white);
    streamTypeEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    streamTypeEditor->setEditable(true);
    streamTypeEditor->addListener(this);
    addAndMakeVisible(streamTypeEditor.get());

    yPos += 25;

    // Scale Factor
    scaleLabel = std::make_unique<Label>("ScaleLabel", "Scale:");
    scaleLabel->setBounds(10, yPos, 50, 20);
    scaleLabel->setFont(Font("Default", 12, Font::plain));
    scaleLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(scaleLabel.get());

    scaleEditor = std::make_unique<Label>("ScaleEditor", String(processor->getDataScale(), 1));
    scaleEditor->setBounds(60, yPos, 50, 20);
    scaleEditor->setFont(Font("Default", 12, Font::plain));
    scaleEditor->setColour(Label::textColourId, Colours::white);
    scaleEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    scaleEditor->setEditable(true);
    scaleEditor->addListener(this);
    addAndMakeVisible(scaleEditor.get());

    // TTL Markers toggle
    markersButton = std::make_unique<ToggleButton>("TTL");
    markersButton->setBounds(115, yPos, 55, 20);
    markersButton->setToggleState(processor->getIncludeMarkers(), dontSendNotification);
    markersButton->addListener(this);
    addAndMakeVisible(markersButton.get());

    // Broadcast toggle
    broadcastButton = std::make_unique<ToggleButton>("Bcast");
    broadcastButton->setBounds(175, yPos, 65, 20);
    broadcastButton->setToggleState(true, dontSendNotification);
    broadcastButton->addListener(this);
    addAndMakeVisible(broadcastButton.get());

    yPos += 25;

    // Status Label
    statusLabel = std::make_unique<Label>("StatusLabel", "Ready");
    statusLabel->setBounds(10, yPos, 230, 20);
    statusLabel->setFont(Font("Default", 11, Font::plain));
    statusLabel->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(statusLabel.get());

    updateStatus();
}

LSLOutletEditor::~LSLOutletEditor()
{
}

void LSLOutletEditor::labelTextChanged(Label* label)
{
    if (label == streamNameEditor.get())
    {
        processor->setStreamName(label->getText());
    }
    else if (label == streamTypeEditor.get())
    {
        processor->setStreamType(label->getText());
    }
    else if (label == scaleEditor.get())
    {
        float scale = label->getText().getFloatValue();
        if (scale > 0 && scale <= 10000)
        {
            processor->setDataScale(scale);
        }
        else
        {
            label->setText(String(processor->getDataScale(), 1), dontSendNotification);
        }
    }
}

void LSLOutletEditor::buttonClicked(Button* button)
{
    if (button == markersButton.get())
    {
        processor->setIncludeMarkers(markersButton->getToggleState());
    }
    else if (button == broadcastButton.get())
    {
        // Forward broadcasts setting handled via parameter
        Parameter* param = processor->getParameter("forward_broadcasts");
        if (param) param->setNextValue(broadcastButton->getToggleState() ? 1.0f : 0.0f);
    }
}

void LSLOutletEditor::setControlsEnabled(bool enabled)
{
    streamNameEditor->setEditable(enabled);
    streamTypeEditor->setEditable(enabled);
    markersButton->setEnabled(enabled);
    broadcastButton->setEnabled(enabled);
    
    Colour bgColour = enabled ? Colours::darkgrey : Colours::grey;
    streamNameEditor->setColour(Label::backgroundColourId, bgColour);
    streamTypeEditor->setColour(Label::backgroundColourId, bgColour);
}

void LSLOutletEditor::startAcquisition()
{
    setControlsEnabled(false);
    updateStatus();
}

void LSLOutletEditor::stopAcquisition()
{
    setControlsEnabled(true);
    updateStatus();
}

void LSLOutletEditor::updateStatus()
{
    if (processor->isStreaming())
    {
        int consumers = processor->getNumConsumers();
        String status = "Streaming";
        if (consumers > 0)
            status += " (" + String(consumers) + " consumer" + (consumers > 1 ? "s" : "") + ")";
        statusLabel->setText(status, dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::green);
    }
    else
    {
        statusLabel->setText("Ready", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::grey);
    }
}
