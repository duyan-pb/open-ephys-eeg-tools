/*
    ------------------------------------------------------------------

    InEar Teensy Optimized Source Plugin for Open Ephys
    
    Editor UI implementation.

    ------------------------------------------------------------------
*/

#include "InEarTeensyOptimizedEditor.h"
#include "InEarTeensyOptimizedThread.h"

InEarTeensyOptimizedEditor::InEarTeensyOptimizedEditor(GenericProcessor* parentNode, 
                                                       InEarTeensyOptimizedThread* t)
    : GenericEditor(parentNode),
      thread(t)
{
    desiredWidth = 300;
    
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
    statusLabel->setBounds(95, 55, 195, 25);
    statusLabel->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(statusLabel.get());
    
    // Info label (protocol info)
    infoLabel = std::make_unique<Label>("Info", "Optimized Protocol | Variable Packets");
    infoLabel->setBounds(10, 82, 280, 18);
    infoLabel->setColour(Label::textColourId, Colours::lightgrey);
    infoLabel->setFont(Font(11.0f));
    addAndMakeVisible(infoLabel.get());
    
    // Stats label
    statsLabel = std::make_unique<Label>("Stats", "Pkts: 0 | Drop: 0 | Err: 0");
    statsLabel->setBounds(10, 98, 280, 18);
    statsLabel->setColour(Label::textColourId, Colours::lightgrey);
    statsLabel->setFont(Font(10.0f));
    addAndMakeVisible(statsLabel.get());
    
    // Initialize port list
    refreshPorts();
    updateStatus();
    
    // Start timer for status updates
    startTimer(250);
}

InEarTeensyOptimizedEditor::~InEarTeensyOptimizedEditor()
{
    stopTimer();
}

void InEarTeensyOptimizedEditor::resized()
{
    GenericEditor::resized();
}

void InEarTeensyOptimizedEditor::timerCallback()
{
    updateStatus();
    updateStats();
}

void InEarTeensyOptimizedEditor::refreshPorts()
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

void InEarTeensyOptimizedEditor::comboBoxChanged(ComboBox* comboBox)
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

void InEarTeensyOptimizedEditor::buttonClicked(Button* button)
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
                LOGC("InEarTeensyOptimized: Connected successfully");
            }
            else
            {
                LOGC("InEarTeensyOptimized: Connection failed");
            }
        }
        
        updateConnectButton();
        updateStatus();
    }
}

void InEarTeensyOptimizedEditor::updateStatus()
{
    if (thread->isSimulating())
    {
        statusLabel->setText("SIMULATION MODE", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::yellow);
        infoLabel->setText("Generating simulated multi-rate data", dontSendNotification);
    }
    else if (thread->isConnected())
    {
        statusLabel->setText("Connected: " + thread->getPort(), dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::green);
        infoLabel->setText("EEG@1kHz | Acc@250Hz | PPG@100Hz | Health@10Hz", dontSendNotification);
    }
    else
    {
        statusLabel->setText("Disconnected", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::grey);
        infoLabel->setText("Optimized Protocol | Variable Packets", dontSendNotification);
    }
    
    updateConnectButton();
}

void InEarTeensyOptimizedEditor::updateStats()
{
    uint64_t received = thread->getPacketsReceived();
    uint64_t dropped = thread->getPacketsDropped();
    uint64_t errors = thread->getChecksumErrors();
    
    String statsText = "Pkts: " + String(received) + 
                       " | Drop: " + String(dropped) + 
                       " | Err: " + String(errors);
    
    // Add drop rate if significant
    if (received > 100 && dropped > 0)
    {
        float dropRate = 100.0f * dropped / (received + dropped);
        statsText += " (" + String(dropRate, 1) + "%)";
    }
    
    statsLabel->setText(statsText, dontSendNotification);
    
    // Color code based on errors
    if (errors > 0 || dropped > received / 100)
    {
        statsLabel->setColour(Label::textColourId, Colours::orange);
    }
    else
    {
        statsLabel->setColour(Label::textColourId, Colours::lightgrey);
    }
}

void InEarTeensyOptimizedEditor::updateConnectButton()
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
        connectButton->setColour(TextButton::buttonColourId, Colour(0xff8b0000));
    }
    else
    {
        connectButton->setEnabled(true);
        connectButton->setButtonText("Connect");
        connectButton->setColour(TextButton::buttonColourId, Colour(0xff006400));
    }
}
