/*
    OpenBCI Cyton Editor
    
    Implementation of the user interface
*/

#include "OpenBCICytonEditor.h"
#include "OpenBCICytonThread.h"

OpenBCICytonEditor::OpenBCICytonEditor(GenericProcessor* parentNode, OpenBCICytonThread* thread)
    : GenericEditor(parentNode)
    , cytonThread(thread)
{
    // Set editor size
    desiredWidth = 280;
    
    // Port selector label
    portLabel = std::make_unique<Label>("PortLabel", "Port:");
    portLabel->setBounds(10, 25, 40, 20);
    portLabel->setFont(Font(12.0f));
    portLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(portLabel.get());
    
    // Port selector combo box
    portSelector = std::make_unique<ComboBox>("PortSelector");
    portSelector->setBounds(50, 25, 120, 20);
    portSelector->addListener(this);
    addAndMakeVisible(portSelector.get());
    
    // Refresh button
    refreshButton = std::make_unique<TextButton>("Refresh");
    refreshButton->setBounds(175, 25, 50, 20);
    refreshButton->addListener(this);
    addAndMakeVisible(refreshButton.get());
    
    // Connect button
    connectButton = std::make_unique<TextButton>("Connect");
    connectButton->setBounds(230, 25, 45, 20);
    connectButton->addListener(this);
    addAndMakeVisible(connectButton.get());
    
    // Daisy mode toggle
    daisyToggle = std::make_unique<ToggleButton>("16-ch (Daisy)");
    daisyToggle->setBounds(10, 50, 120, 20);
    daisyToggle->setToggleState(false, dontSendNotification);
    daisyToggle->addListener(this);
    addAndMakeVisible(daisyToggle.get());
    
    // Status label
    statusLabel = std::make_unique<Label>("StatusLabel", "Not connected");
    statusLabel->setBounds(10, 75, 130, 20);
    statusLabel->setFont(Font(11.0f));
    statusLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(statusLabel.get());
    
    // Firmware label
    firmwareLabel = std::make_unique<Label>("FirmwareLabel", "");
    firmwareLabel->setBounds(140, 75, 135, 20);
    firmwareLabel->setFont(Font(11.0f));
    firmwareLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(firmwareLabel.get());
    
    // Populate initial port list
    refreshPorts();
}

OpenBCICytonEditor::~OpenBCICytonEditor()
{
}

void OpenBCICytonEditor::refreshPorts()
{
    // Save current selection
    String currentSelection = portSelector->getText();
    
    // Clear and repopulate
    portSelector->clear(dontSendNotification);
    
    std::vector<std::string> ports = cytonThread->getAvailablePorts();
    
    int id = 1;
    int selectedId = 0;
    
    for (const auto& port : ports)
    {
        portSelector->addItem(String(port), id);
        
        if (String(port) == currentSelection || String(port) == lastSelectedPort)
        {
            selectedId = id;
        }
        id++;
    }
    
    if (ports.empty())
    {
        portSelector->addItem("No ports found", 1);
        portSelector->setSelectedId(1, dontSendNotification);
    }
    else if (selectedId > 0)
    {
        portSelector->setSelectedId(selectedId, dontSendNotification);
    }
    else if (portSelector->getNumItems() > 0)
    {
        portSelector->setSelectedId(1, dontSendNotification);
    }
}

void OpenBCICytonEditor::updateConnectionStatus()
{
    if (cytonThread->isConnected())
    {
        String portName = String(cytonThread->getCurrentPort());
        int numChannels = cytonThread->getNumChannels();
        float sampleRate = cytonThread->getSampleRate();
        
        statusLabel->setText(String(numChannels) + "ch @ " + String(static_cast<int>(sampleRate)) + "Hz", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::green);
        
        connectButton->setButtonText("Disc");
        
        String firmware = String(cytonThread->getFirmwareVersion());
        if (firmware.isNotEmpty())
        {
            firmwareLabel->setText(firmware, dontSendNotification);
        }
        
        // Disable port selector and refresh while connected
        portSelector->setEnabled(false);
        refreshButton->setEnabled(false);
        daisyToggle->setEnabled(false);
    }
    else
    {
        statusLabel->setText("Not connected", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::darkgrey);
        
        firmwareLabel->setText("", dontSendNotification);
        
        connectButton->setButtonText("Connect");
        
        // Enable port selector and refresh
        portSelector->setEnabled(true);
        refreshButton->setEnabled(true);
        daisyToggle->setEnabled(true);
    }
}

void OpenBCICytonEditor::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox == portSelector.get())
    {
        lastSelectedPort = portSelector->getText();
    }
}

void OpenBCICytonEditor::buttonClicked(Button* button)
{
    if (button == connectButton.get())
    {
        if (cytonThread->isConnected())
        {
            // Disconnect
            cytonThread->disconnect();
        }
        else
        {
            // Connect to selected port
            String portName = portSelector->getText();
            
            if (portName.isNotEmpty() && portName != "No ports found")
            {
                // Set Daisy mode before connecting
                cytonThread->setDaisyMode(daisyToggle->getToggleState());
                
                statusLabel->setText("Connecting...", dontSendNotification);
                statusLabel->setColour(Label::textColourId, Colours::orange);
                
                // Try to connect
                if (!cytonThread->connectToPort(portName.toStdString()))
                {
                    statusLabel->setText("Connection failed", dontSendNotification);
                    statusLabel->setColour(Label::textColourId, Colours::red);
                }
            }
        }
        
        updateConnectionStatus();
        CoreServices::updateSignalChain(this);
    }
    else if (button == refreshButton.get())
    {
        refreshPorts();
    }
    else if (button == daisyToggle.get())
    {
        // Daisy mode changed - will take effect on next connect
    }
}

void OpenBCICytonEditor::saveCustomParametersToXml(XmlElement* xml)
{
    XmlElement* cytonParams = xml->createNewChildElement("OPENBCI_CYTON");
    
    cytonParams->setAttribute("port", lastSelectedPort);
    cytonParams->setAttribute("daisy", daisyToggle->getToggleState());
}

void OpenBCICytonEditor::loadCustomParametersFromXml(XmlElement* xml)
{
    XmlElement* cytonParams = xml->getChildByName("OPENBCI_CYTON");
    
    if (cytonParams)
    {
        lastSelectedPort = cytonParams->getStringAttribute("port", "");
        
        bool daisyMode = cytonParams->getBoolAttribute("daisy", false);
        daisyToggle->setToggleState(daisyMode, dontSendNotification);
        
        // Refresh ports and try to select the saved one
        refreshPorts();
    }
}
