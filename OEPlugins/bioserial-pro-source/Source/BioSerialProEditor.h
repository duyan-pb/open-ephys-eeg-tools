/*
    ------------------------------------------------------------------

    BioSerial-Pro Source Plugin for Open Ephys
    
    Editor UI for configuring the BioSerial-Pro data source.

    ------------------------------------------------------------------
*/

#ifndef BIOSERIAL_PRO_EDITOR_H
#define BIOSERIAL_PRO_EDITOR_H

#include <EditorHeaders.h>

class BioSerialProThread;

/**
 * Editor for the BioSerial-Pro Source plugin.
 * 
 * Provides UI controls for:
 * - Serial port selection
 * - Connection status
 * - Simulation mode
 * - Statistics display
 */
class BioSerialProEditor : public GenericEditor,
                           public ComboBox::Listener,
                           public Button::Listener,
                           public Timer
{
public:
    BioSerialProEditor(GenericProcessor* parentNode, BioSerialProThread* thread);
    ~BioSerialProEditor();

    /** Called when editor becomes visible */
    void resized() override;
    
    /** ComboBox callback */
    void comboBoxChanged(ComboBox* comboBox) override;
    
    /** Button callback */
    void buttonClicked(Button* button) override;
    
    /** Timer callback for status updates */
    void timerCallback() override;

private:
    BioSerialProThread* thread;
    
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BioSerialProEditor);
};

#endif // BIOSERIAL_PRO_EDITOR_H
