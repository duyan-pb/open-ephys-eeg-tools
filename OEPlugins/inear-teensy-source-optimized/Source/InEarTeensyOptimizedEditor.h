/*
    ------------------------------------------------------------------

    InEar Teensy Optimized Source Plugin for Open Ephys
    
    Editor UI header.

    ------------------------------------------------------------------
*/

#ifndef INEAR_TEENSY_OPTIMIZED_EDITOR_H
#define INEAR_TEENSY_OPTIMIZED_EDITOR_H

#include <EditorHeaders.h>

class InEarTeensyOptimizedThread;

class InEarTeensyOptimizedEditor : public GenericEditor,
                                   public ComboBox::Listener,
                                   public Button::Listener,
                                   public Timer
{
public:
    InEarTeensyOptimizedEditor(GenericProcessor* parentNode, InEarTeensyOptimizedThread* thread);
    ~InEarTeensyOptimizedEditor();

    void resized() override;
    void comboBoxChanged(ComboBox* comboBox) override;
    void buttonClicked(Button* button) override;
    void timerCallback() override;

private:
    InEarTeensyOptimizedThread* thread;
    
    // UI Components
    std::unique_ptr<Label> portLabel;
    std::unique_ptr<ComboBox> portSelector;
    std::unique_ptr<TextButton> refreshButton;
    std::unique_ptr<ToggleButton> simulateButton;
    std::unique_ptr<TextButton> connectButton;
    std::unique_ptr<Label> statusLabel;
    std::unique_ptr<Label> infoLabel;
    std::unique_ptr<Label> statsLabel;
    
    void refreshPorts();
    void updateStatus();
    void updateConnectButton();
    void updateStats();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InEarTeensyOptimizedEditor);
};

#endif // INEAR_TEENSY_OPTIMIZED_EDITOR_H
