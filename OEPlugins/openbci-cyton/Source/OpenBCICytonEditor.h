/*
    OpenBCI Cyton Editor
    
    User interface for OpenBCI Cyton plugin
*/

#ifndef OPENBCI_CYTON_EDITOR_H
#define OPENBCI_CYTON_EDITOR_H

#include <EditorHeaders.h>

class OpenBCICytonThread;

class OpenBCICytonEditor : public GenericEditor,
                           public ComboBox::Listener,
                           public Button::Listener
{
public:
    /** Constructor */
    OpenBCICytonEditor(GenericProcessor* parentNode, OpenBCICytonThread* thread);
    
    /** Destructor */
    ~OpenBCICytonEditor() override;
    
    /** Called when combo box selection changes */
    void comboBoxChanged(ComboBox* comboBox) override;
    
    /** Called when button is clicked */
    void buttonClicked(Button* button) override;
    
    /** Updates the port list */
    void refreshPorts();
    
    /** Updates the connection status display */
    void updateConnectionStatus();
    
    /** Save custom parameters */
    void saveCustomParametersToXml(XmlElement* xml) override;
    
    /** Load custom parameters */
    void loadCustomParametersFromXml(XmlElement* xml) override;
    
private:
    /** Reference to the data thread */
    OpenBCICytonThread* cytonThread;
    
    /** UI Components */
    std::unique_ptr<ComboBox> portSelector;
    std::unique_ptr<Label> portLabel;
    
    std::unique_ptr<TextButton> connectButton;
    std::unique_ptr<TextButton> refreshButton;
    
    std::unique_ptr<ToggleButton> daisyToggle;
    
    std::unique_ptr<Label> statusLabel;
    std::unique_ptr<Label> firmwareLabel;
    
    /** Store last selected port */
    String lastSelectedPort;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenBCICytonEditor);
};

#endif // OPENBCI_CYTON_EDITOR_H
