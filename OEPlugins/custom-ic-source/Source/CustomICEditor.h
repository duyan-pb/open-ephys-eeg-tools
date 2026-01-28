/*
    ------------------------------------------------------------------

    Custom IC Source Plugin for Open Ephys
    
    Editor UI for configuring the Custom IC data source.

    ------------------------------------------------------------------
*/

#ifndef CUSTOM_IC_EDITOR_H
#define CUSTOM_IC_EDITOR_H

#include <EditorHeaders.h>

class CustomICThread;

/**
 * Editor for the Custom IC Source plugin.
 * 
 * Provides UI controls for:
 * - Serial port selection
 * - Baud rate configuration
 * - Number of channels
 * - Sample rate
 * - Data format (bytes per sample)
 * - Scale factor
 * - Sync bytes
 * - Simulation mode
 */
class CustomICEditor : public GenericEditor,
                       public ComboBox::Listener,
                       public Button::Listener,
                       public Label::Listener
{
public:
    CustomICEditor(GenericProcessor* parentNode, CustomICThread* thread);
    ~CustomICEditor();

    /** Called when editor becomes visible */
    void resized() override;
    
    /** ComboBox callback */
    void comboBoxChanged(ComboBox* comboBox) override;
    
    /** Button callback */
    void buttonClicked(Button* button) override;
    
    /** Label callback for text entry */
    void labelTextChanged(Label* label) override;

private:
    CustomICThread* thread;
    
    // Port selection
    std::unique_ptr<ComboBox> portSelector;
    std::unique_ptr<Label> portLabel;
    std::unique_ptr<TextButton> refreshButton;
    
    // Baud rate
    std::unique_ptr<ComboBox> baudSelector;
    std::unique_ptr<Label> baudLabel;
    
    // Channel count
    std::unique_ptr<Label> channelLabel;
    std::unique_ptr<Label> channelValue;
    
    // Sample rate
    std::unique_ptr<Label> sampleRateLabel;
    std::unique_ptr<Label> sampleRateValue;
    
    // Data format
    std::unique_ptr<ComboBox> formatSelector;
    std::unique_ptr<Label> formatLabel;
    
    // Scale factor
    std::unique_ptr<Label> scaleLabel;
    std::unique_ptr<Label> scaleValue;
    
    // Sync bytes
    std::unique_ptr<Label> syncLabel;
    std::unique_ptr<Label> sync1Value;
    std::unique_ptr<Label> sync2Value;
    
    // Simulation mode
    std::unique_ptr<ToggleButton> simulateButton;
    
    // Connect button
    std::unique_ptr<TextButton> connectButton;
    
    // Status
    std::unique_ptr<Label> statusLabel;
    
    /** Refresh the port list */
    void refreshPorts();
    
    /** Update status display */
    void updateStatus();
    
    /** Update the connect button state */
    void updateConnectButton();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomICEditor);
};

#endif // CUSTOM_IC_EDITOR_H
