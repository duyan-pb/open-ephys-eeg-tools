/*
    ------------------------------------------------------------------

    InEar Teensy Source Plugin for Open Ephys
    
    Editor UI for configuring the InEar Teensy data source.

    ------------------------------------------------------------------
*/

#ifndef INEAR_TEENSY_EDITOR_H
#define INEAR_TEENSY_EDITOR_H

#include <EditorHeaders.h>

class InEarTeensyThread;

/**
 * Editor for the InEar Teensy Source plugin.
 * 
 * Provides UI controls for:
 * - Serial port selection
 * - Connection status
 * - Simulation mode
 * - Statistics display
 */
class InEarTeensyEditor : public GenericEditor,
                           public ComboBox::Listener,
                           public Button::Listener,
                           public Timer
{
public:
    InEarTeensyEditor(GenericProcessor* parentNode, InEarTeensyThread* thread);
    ~InEarTeensyEditor();

    /** Called when editor becomes visible */
    void resized() override;
    
    /** ComboBox callback */
    void comboBoxChanged(ComboBox* comboBox) override;
    
    /** Button callback */
    void buttonClicked(Button* button) override;
    
    /** Timer callback for status updates */
    void timerCallback() override;

private:
    InEarTeensyThread* thread;
    
    // Port selection
    std::unique_ptr<ComboBox> portSelector;
    std::unique_ptr<Label> portLabel;
    std::unique_ptr<TextButton> refreshButton;
    
    // Simulation mode
    std::unique_ptr<ToggleButton> simulateButton;
    
    // Connect button
    std::unique_ptr<TextButton> connectButton;
    
    // Status display
    std::unique_ptr<Label> statusLabel;
    std::unique_ptr<Label> infoLabel;
    
    /** Refresh the port list */
    void refreshPorts();
    
    /** Update status display */
    void updateStatus();
    
    /** Update the connect button state */
    void updateConnectButton();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InEarTeensyEditor);
};

#endif // INEAR_TEENSY_EDITOR_H


