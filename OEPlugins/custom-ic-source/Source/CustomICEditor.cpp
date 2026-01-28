/*
    ------------------------------------------------------------------

    Custom IC Source Plugin for Open Ephys
    
    Editor UI implementation.

    ------------------------------------------------------------------
*/

#include "CustomICEditor.h"
#include "CustomICThread.h"

CustomICEditor::CustomICEditor(GenericProcessor* parentNode, CustomICThread* t)
    : GenericEditor(parentNode),
      thread(t)
{
    desiredWidth = 340;
    
    // Port selection
    portLabel = std::make_unique<Label>("Port", "Port:");
    portLabel->setBounds(10, 25, 40, 20);
    addAndMakeVisible(portLabel.get());
    
    portSelector = std::make_unique<ComboBox>("PortSelector");
    portSelector->setBounds(50, 25, 100, 20);
    portSelector->addListener(this);
    addAndMakeVisible(portSelector.get());
    
    refreshButton = std::make_unique<TextButton>("Refresh");
    refreshButton->setBounds(155, 25, 55, 20);
    refreshButton->addListener(this);
    addAndMakeVisible(refreshButton.get());
    
    // Baud rate
    baudLabel = std::make_unique<Label>("Baud", "Baud:");
    baudLabel->setBounds(10, 50, 40, 20);
    addAndMakeVisible(baudLabel.get());
    
    baudSelector = std::make_unique<ComboBox>("BaudSelector");
    baudSelector->setBounds(50, 50, 100, 20);
    baudSelector->addItem("9600", 1);
    baudSelector->addItem("19200", 2);
    baudSelector->addItem("38400", 3);
    baudSelector->addItem("57600", 4);
    baudSelector->addItem("115200", 5);
    baudSelector->addItem("230400", 6);
    baudSelector->addItem("460800", 7);
    baudSelector->addItem("921600", 8);
    baudSelector->setSelectedId(5); // Default: 115200
    baudSelector->addListener(this);
    addAndMakeVisible(baudSelector.get());
    
    // Number of channels
    channelLabel = std::make_unique<Label>("Channels", "Channels:");
    channelLabel->setBounds(10, 75, 60, 20);
    addAndMakeVisible(channelLabel.get());
    
    channelValue = std::make_unique<Label>("ChannelValue", String(thread->getNumChannels()));
    channelValue->setBounds(70, 75, 40, 20);
    channelValue->setEditable(true);
    channelValue->setColour(Label::backgroundColourId, Colours::darkgrey);
    channelValue->addListener(this);
    addAndMakeVisible(channelValue.get());
    
    // Sample rate
    sampleRateLabel = std::make_unique<Label>("SampleRate", "Rate (Hz):");
    sampleRateLabel->setBounds(115, 75, 65, 20);
    addAndMakeVisible(sampleRateLabel.get());
    
    sampleRateValue = std::make_unique<Label>("SampleRateValue", String(thread->getSampleRate()));
    sampleRateValue->setBounds(180, 75, 50, 20);
    sampleRateValue->setEditable(true);
    sampleRateValue->setColour(Label::backgroundColourId, Colours::darkgrey);
    sampleRateValue->addListener(this);
    addAndMakeVisible(sampleRateValue.get());
    
    // Data format
    formatLabel = std::make_unique<Label>("Format", "Format:");
    formatLabel->setBounds(220, 25, 50, 20);
    addAndMakeVisible(formatLabel.get());
    
    formatSelector = std::make_unique<ComboBox>("FormatSelector");
    formatSelector->setBounds(270, 25, 60, 20);
    formatSelector->addItem("int16", 2);
    formatSelector->addItem("int24", 3);
    formatSelector->addItem("int32", 4);
    formatSelector->setSelectedId(2); // Default: int16
    formatSelector->addListener(this);
    addAndMakeVisible(formatSelector.get());
    
    // Scale factor
    scaleLabel = std::make_unique<Label>("Scale", "Scale:");
    scaleLabel->setBounds(220, 50, 45, 20);
    addAndMakeVisible(scaleLabel.get());
    
    scaleValue = std::make_unique<Label>("ScaleValue", "0.195");
    scaleValue->setBounds(265, 50, 65, 20);
    scaleValue->setEditable(true);
    scaleValue->setColour(Label::backgroundColourId, Colours::darkgrey);
    scaleValue->addListener(this);
    addAndMakeVisible(scaleValue.get());
    
    // Sync bytes
    syncLabel = std::make_unique<Label>("Sync", "Sync:");
    syncLabel->setBounds(235, 75, 35, 20);
    addAndMakeVisible(syncLabel.get());
    
    sync1Value = std::make_unique<Label>("Sync1", "A0");
    sync1Value->setBounds(270, 75, 30, 20);
    sync1Value->setEditable(true);
    sync1Value->setColour(Label::backgroundColourId, Colours::darkgrey);
    sync1Value->addListener(this);
    addAndMakeVisible(sync1Value.get());
    
    sync2Value = std::make_unique<Label>("Sync2", "5A");
    sync2Value->setBounds(302, 75, 30, 20);
    sync2Value->setEditable(true);
    sync2Value->setColour(Label::backgroundColourId, Colours::darkgrey);
    sync2Value->addListener(this);
    addAndMakeVisible(sync2Value.get());
    
    // Simulation mode
    simulateButton = std::make_unique<ToggleButton>("Simulate");
    simulateButton->setBounds(155, 50, 80, 20);
    simulateButton->setToggleState(false, dontSendNotification);
    simulateButton->addListener(this);
    addAndMakeVisible(simulateButton.get());
    
    // Connect button
    connectButton = std::make_unique<TextButton>("Connect");
    connectButton->setBounds(10, 100, 100, 25);
    connectButton->addListener(this);
    addAndMakeVisible(connectButton.get());
    
    // Status label
    statusLabel = std::make_unique<Label>("Status", "Not connected");
    statusLabel->setBounds(115, 100, 215, 25);
    statusLabel->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(statusLabel.get());
    
    // Initialize port list
    refreshPorts();
    updateStatus();
}

CustomICEditor::~CustomICEditor()
{
}

void CustomICEditor::resized()
{
    GenericEditor::resized();
}

void CustomICEditor::refreshPorts()
{
    portSelector->clear();
    
    StringArray ports = thread->getAvailablePorts();
    
    int id = 1;
    for (const auto& port : ports)
    {
        portSelector->addItem(port, id++);
    }
    
    if (ports.size() > 0)
        portSelector->setSelectedId(1);
}

void CustomICEditor::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox == portSelector.get())
    {
        String selectedPort = portSelector->getText();
        // Remove "(in use)" suffix if present
        if (selectedPort.contains("(in use)"))
            selectedPort = selectedPort.upToFirstOccurrenceOf(" (in use)", false, true);
        thread->setPort(selectedPort);
    }
    else if (comboBox == baudSelector.get())
    {
        int baud = baudSelector->getText().getIntValue();
        thread->setBaudRate(baud);
    }
    else if (comboBox == formatSelector.get())
    {
        int bytes = formatSelector->getSelectedId();
        thread->setDataFormat(bytes);
    }
}

void CustomICEditor::buttonClicked(Button* button)
{
    if (button == refreshButton.get())
    {
        refreshPorts();
    }
    else if (button == simulateButton.get())
    {
        thread->setSimulationMode(simulateButton->getToggleState());
        updateStatus();
    }
    else if (button == connectButton.get())
    {
        if (thread->isConnected())
        {
            thread->disconnect();
        }
        else
        {
            thread->connect();
        }
        updateConnectButton();
        updateStatus();
    }
}

void CustomICEditor::labelTextChanged(Label* label)
{
    if (label == channelValue.get())
    {
        int channels = channelValue->getText().getIntValue();
        if (channels > 0 && channels <= 256)
        {
            thread->setNumChannels(channels);
            CoreServices::updateSignalChain(this);
        }
        else
        {
            channelValue->setText(String(thread->getNumChannels()), dontSendNotification);
        }
    }
    else if (label == sampleRateValue.get())
    {
        float rate = sampleRateValue->getText().getFloatValue();
        if (rate > 0 && rate <= 100000)
        {
            thread->setSampleRate(rate);
            CoreServices::updateSignalChain(this);
        }
        else
        {
            sampleRateValue->setText(String(thread->getSampleRate()), dontSendNotification);
        }
    }
    else if (label == scaleValue.get())
    {
        float scale = scaleValue->getText().getFloatValue();
        if (scale != 0)
        {
            thread->setScaleFactor(scale);
        }
    }
    else if (label == sync1Value.get() || label == sync2Value.get())
    {
        uint8_t sync1 = (uint8_t)sync1Value->getText().getHexValue32();
        uint8_t sync2 = (uint8_t)sync2Value->getText().getHexValue32();
        thread->setSyncBytes(sync1, sync2);
    }
}

void CustomICEditor::updateStatus()
{
    if (thread->isSimulating())
    {
        statusLabel->setText("Simulation mode", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::yellow);
    }
    else if (thread->isConnected())
    {
        statusLabel->setText("Connected: " + thread->getPort(), dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::green);
    }
    else
    {
        statusLabel->setText("Not connected", dontSendNotification);
        statusLabel->setColour(Label::textColourId, Colours::grey);
    }
    
    updateConnectButton();
}

void CustomICEditor::updateConnectButton()
{
    if (thread->isConnected())
    {
        connectButton->setButtonText("Disconnect");
        connectButton->setColour(TextButton::buttonColourId, Colours::darkred);
    }
    else
    {
        connectButton->setButtonText("Connect");
        connectButton->setColour(TextButton::buttonColourId, Colours::darkgreen);
    }
}
