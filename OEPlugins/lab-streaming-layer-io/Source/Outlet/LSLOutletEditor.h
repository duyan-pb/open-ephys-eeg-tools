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

#ifndef LSLOUTLETEDITOR_H_DEFINED
#define LSLOUTLETEDITOR_H_DEFINED

#include <EditorHeaders.h>

class LSLOutlet;

/**
 * Editor for the LSL Outlet plugin
 * 
 * Allows configuration of:
 * - Stream name and type
 * - Data scale factor
 * - TTL marker streaming
 * - Broadcast message forwarding
 */
class LSLOutletEditor : public GenericEditor,
                        public Label::Listener,
                        public Button::Listener
{
public:
    /** Constructor */
    LSLOutletEditor(LSLOutlet* parentNode);

    /** Destructor */
    ~LSLOutletEditor();

    /** Called when a label's text changes */
    void labelTextChanged(Label* label) override;

    /** Called when a button is clicked */
    void buttonClicked(Button* button) override;

    /** Update the display when acquisition state changes */
    void startAcquisition() override;
    void stopAcquisition() override;

private:
    /** Pointer to the processor */
    LSLOutlet* processor;

    /** Stream name label and editor */
    std::unique_ptr<Label> streamNameLabel;
    std::unique_ptr<Label> streamNameEditor;

    /** Stream type label and editor */
    std::unique_ptr<Label> streamTypeLabel;
    std::unique_ptr<Label> streamTypeEditor;

    /** Scale factor label and editor */
    std::unique_ptr<Label> scaleLabel;
    std::unique_ptr<Label> scaleEditor;

    /** Include markers checkbox */
    std::unique_ptr<ToggleButton> markersButton;

    /** Forward broadcasts checkbox */
    std::unique_ptr<ToggleButton> broadcastButton;

    /** Status indicator */
    std::unique_ptr<Label> statusLabel;

    /** Update status display */
    void updateStatus();

    /** Enable/disable controls during acquisition */
    void setControlsEnabled(bool enabled);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSLOutletEditor);
};

#endif // LSLOUTLETEDITOR_H_DEFINED
