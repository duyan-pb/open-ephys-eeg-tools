/*
    ------------------------------------------------------------------

    BioSerial-Pro Source Plugin for Open Ephys
    
    Editor UI implementation.

    ------------------------------------------------------------------
*/

#include "BioSerialProEditor.h"
#include "BioSerialProThread.h"

BioSerialProEditor::BioSerialProEditor(GenericProcessor* parentNode, BioSerialProThread* t)
    : GenericEditor(parentNode),
      thread(t)
{
    desiredWidth = 280;
    
    // Port selection
    portLabel = std::make_unique<Label>("Port", "Port:");
    portLabel->setBounds(10, 28, 35, 20);
    addAndMakeVisible(portLabel.get());
    
    portSelector = std::make_unique<ComboBox>("PortSelector");
    portSelector->setBounds(45, 28, 100, 20);
    portSelector->addListener(this);
    addAndMakeVisible(portSelector.get());
    
    refreshButton = std::make_unique<TextButton>("↻");
    refreshButton->setBounds(148, 28, 25, 20);
    refreshButton->addListener(this);
    refreshButton->setTooltip("Refresh port list");
    addAndMakeVisible(refreshButton.get());
    
    // Simulation mode
    simulateButton = std::make_unique<ToggleButton>("Simulate");
    simulateButton->setBounds(178, 28, 90, 20);
    simulateButton->setToggleState(false, dontSendNotification);
    simulateButton->addListener(this);
    simulateButton->setTooltip("Generate simulated EEG data");
    addAndMakeVisible(simulateButton.get());
    
    // Connect button
    connectButton = std::make_unique<TextButton>("Connect");
    connectButton->setBounds(10, 55, 80, 25);
    connectButton->addListener(this);
    addAndMakeVisible(connectButton.get());
    
    // Status label
    statusLabel = std::make_unique<Label>("Status", "Disconnected");
    statusLabel->setBounds(95, 55, 170, 25);
    statusLabel->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(statusLabel.get());
    
    // Info label (protocol info)
    infoLabel = std::make_unique<Label>("Info", "5 EEG + 9 Aux @ 1kHz | 56B packets");
    infoLabel->setBounds(10, 85, 260, 20);
    infoLabel->setColour(Label::textColourId, Colours::lightgrey);
    infoLabel->setFont(Font(12.0f));
    addAndMakeVisible(infoLabel.get());
    
    // Initialize port list
    refreshPorts();
    updateStatus();
    
    // Start timer for status updates
    startTimer(500);
}

BioSerialProEditor::~BioSerialProEditor()
{
    stopTimer();
}

void BioSerialProEditor::resized()
{
    GenericEditor::resized();
}

void BioSerialProEditor::timerCallback()
{
    updateStatus();
}

void BioSerialProEditor::refreshPorts()
{
    portSelector->clear();
    
    StringArray ports = thread->getAvailablePorts();
    
    int id = 1;
    for (const auto& port : ports)
    {
        portSelector->addItem(port, id++);
    }
    
    // Try to select current port if set
    String currentPort = thread->getPort();
    if (currentPort.isNotEmpty())
    {
        for (int i = 0; i < ports.size(); i++)
        {
            if (ports[i].startsWith(currentPort))
            {
                portSelector->setSelectedId(i + 1);
                return;
            }
        }
    }
    
    if (ports.size() > 0)
        portSelector->setSelectedId(1);
}

void BioSerialProEditor::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox == portSelector.get())
    {
        String selectedPort = portSelector->getText();
        
        // Remove "(in use)" suffix if present
        if (selectedPort.contains("(in use)"))
        {
            selectedPort = selectedPort.upToFirstOccurrenceOf(" (in use)", false, true);
        }
        
        thread->setPort(selectedPort);
        updateStatus();
    }
}

void BioSerialProEditor::buttonClicked(Button* button)
{
    if (button == refreshButton.get())
    {
        refreshPorts();
    }
    else if (button == simulateButton.get())
    {
        thread->setSimulationMode(simulateButton->getToggleState());
        
        // Disable port selection when simulating
        portSelector->setEnabled(!simulateButton->getToggleState());
        connectButton->setEnabled(!simulateButton->getToggleState());
        
        updateStatus();
        
        // Update signal chain to reflect changes
        CoreServices::updateSignalChain(this);
    }
    else if (button == connectButton.get())
    {
        if (thread->isConnected())
        {
            thread->disconnect();
        }
        else
        {
            if (thread->connect())
            {
                LOGC("BioSerialPro: Connected successfully");
            }
            else
            {
                LOGC("BioSerialPro: Connection failed");
            }
        }
        
        updateConnectButton();
        updateStatus();
    }
}

void BioSerialProEditor::updateStatus()
{
    if (thread->isSimulating())
    {
        statusLabel->setText("SIMULATION MODE", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::yellow);
        infoLabel->setText("Generating simulated EEG/PPG waves", dontSendNotification);
    }
    else if (thread->isConnected())
    {
        statusLabel->setText("Connected: " + thread->getPort(), dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::green);
        infoLabel->setText("5 EEG + 9 Aux @ 1kHz | 56B packets", dontSendNotification);
    }
    else
    {
        statusLabel->setText("Disconnected", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::grey);
        infoLabel->setText("BioSerial-Pro Protocol (Teensy + ADS1299)", dontSendNotification);
    }
    
    updateConnectButton();
}

void BioSerialProEditor::updateConnectButton()
{
    if (thread->isSimulating())
    {
        connectButton->setEnabled(false);
        connectButton->setButtonText("Connect");
        connectButton->setColour(TextButton::buttonColourId, Colours::darkgrey);
    }
    else if (thread->isConnected())
    {
        connectButton->setEnabled(true);
        connectButton->setButtonText("Disconnect");
        connectButton->setColour(TextButton::buttonColourId, Colour(0xff8b0000));  // Dark red
    }
    else
    {
        connectButton->setEnabled(true);
        connectButton->setButtonText("Connect");
        connectButton->setColour(TextButton::buttonColourId, Colour(0xff006400));  // Dark green
    }
}
