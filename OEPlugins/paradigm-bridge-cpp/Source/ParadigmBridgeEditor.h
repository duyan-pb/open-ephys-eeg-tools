/*
    ------------------------------------------------------------------

    Paradigm Bridge - Editor (GUI panel in Open Ephys)

    Provides:
    - TCP server port configuration and start/stop button
    - Connection status indicator
    - Trigger count and last command display
    - Auto-start toggle
    - Manual trigger controls (line selector + ON/OFF buttons)

    ------------------------------------------------------------------
*/

#ifndef PARADIGM_BRIDGE_EDITOR_H_DEFINED
#define PARADIGM_BRIDGE_EDITOR_H_DEFINED

#include <EditorHeaders.h>

class ParadigmBridge;

class ParadigmBridgeEditor : public GenericEditor,
                             public Button::Listener,
                             public Label::Listener,
                             public Timer
{
public:
    ParadigmBridgeEditor(ParadigmBridge* parentNode);
    ~ParadigmBridgeEditor();

    /** Handle button clicks (server start/stop, manual triggers) */
    void buttonClicked(Button* button) override;

    /** Handle port label edits */
    void labelTextChanged(Label* label) override;

    /** Periodic status update (5 Hz) */
    void timerCallback() override;

    /** Lock controls during acquisition */
    void startAcquisition() override;

    /** Unlock controls after acquisition */
    void stopAcquisition() override;

private:
    ParadigmBridge* processor;

    // --- Row 1: Server controls ---
    std::unique_ptr<Label> portLabel;
    std::unique_ptr<Label> portEditor;
    std::unique_ptr<TextButton> serverButton;

    // --- Row 2: Auto-start toggle ---
    std::unique_ptr<Label> autoStartLabel;
    std::unique_ptr<ToggleButton> autoStartButton;

    // --- Row 3-5: Status display ---
    std::unique_ptr<Label> serverStatusLabel;
    std::unique_ptr<Label> connectionStatusLabel;
    std::unique_ptr<Label> triggerCountLabel;
    std::unique_ptr<Label> lastCommandLabel;

    // --- Row 6: Manual trigger controls ---
    std::unique_ptr<Label> triggerLabel;
    std::unique_ptr<Label> triggerLineLabel;
    std::unique_ptr<Label> triggerLineEditor;
    std::unique_ptr<TextButton> triggerOnButton;
    std::unique_ptr<TextButton> triggerOffButton;

    void updateServerButton();
    void updateStatusDisplay();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParadigmBridgeEditor);
};

#endif // PARADIGM_BRIDGE_EDITOR_H_DEFINED
