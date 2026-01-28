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
    desiredWidth = 230;

    // Stream Name Label
    streamNameLabel = std::make_unique<Label>("StreamNameLabel", "Stream Name:");
    streamNameLabel->setBounds(10, 25, 90, 20);
    streamNameLabel->setFont(Font("Default", 12, Font::plain));
    streamNameLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(streamNameLabel.get());

    // Stream Name Editor
    streamNameEditor = std::make_unique<Label>("StreamNameEditor", processor->getStreamName());
    streamNameEditor->setBounds(100, 25, 120, 20);
    streamNameEditor->setFont(Font("Default", 12, Font::plain));
    streamNameEditor->setColour(Label::textColourId, Colours::white);
    streamNameEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    streamNameEditor->setEditable(true);
    streamNameEditor->addListener(this);
    addAndMakeVisible(streamNameEditor.get());

    // Stream Type Label
    streamTypeLabel = std::make_unique<Label>("StreamTypeLabel", "Stream Type:");
    streamTypeLabel->setBounds(10, 50, 90, 20);
    streamTypeLabel->setFont(Font("Default", 12, Font::plain));
    streamTypeLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(streamTypeLabel.get());

    // Stream Type Editor
    streamTypeEditor = std::make_unique<Label>("StreamTypeEditor", processor->getStreamType());
    streamTypeEditor->setBounds(100, 50, 120, 20);
    streamTypeEditor->setFont(Font("Default", 12, Font::plain));
    streamTypeEditor->setColour(Label::textColourId, Colours::white);
    streamTypeEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    streamTypeEditor->setEditable(true);
    streamTypeEditor->addListener(this);
    addAndMakeVisible(streamTypeEditor.get());

    // Markers Label
    markersLabel = std::make_unique<Label>("MarkersLabel", "TTL Markers:");
    markersLabel->setBounds(10, 75, 90, 20);
    markersLabel->setFont(Font("Default", 12, Font::plain));
    markersLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(markersLabel.get());

    // Markers Toggle
    markersButton = std::make_unique<ToggleButton>("Include");
    markersButton->setBounds(100, 75, 80, 20);
    markersButton->setToggleState(processor->getIncludeMarkers(), dontSendNotification);
    markersButton->addListener(this);
    addAndMakeVisible(markersButton.get());

    // Status Label
    statusLabel = std::make_unique<Label>("StatusLabel", "Status: Ready");
    statusLabel->setBounds(10, 100, 210, 20);
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
}

void LSLOutletEditor::buttonClicked(Button* button)
{
    if (button == markersButton.get())
    {
        processor->setIncludeMarkers(markersButton->getToggleState());
    }
}

void LSLOutletEditor::setControlsEnabled(bool enabled)
{
    streamNameEditor->setEditable(enabled);
    streamTypeEditor->setEditable(enabled);
    markersButton->setEnabled(enabled);
    
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
        statusLabel->setText("Status: Streaming via LSL...", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::green);
    }
    else
    {
        statusLabel->setText("Status: Ready", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::grey);
    }
}
