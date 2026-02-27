/*
    ------------------------------------------------------------------

    Paradigm Bridge - Editor Implementation

    ------------------------------------------------------------------
*/

#include "ParadigmBridgeEditor.h"
#include "ParadigmBridge.h"


ParadigmBridgeEditor::ParadigmBridgeEditor(ParadigmBridge* parentNode)
    : GenericEditor(parentNode),
      processor(parentNode)
{
    desiredWidth = 300;

    int y = 25;

    // ===== Row 1: TCP Port + Server Button =====

    portLabel = std::make_unique<Label>("PortLabel", "Port:");
    portLabel->setBounds(10, y, 35, 20);
    portLabel->setFont(Font("Default", 12, Font::plain));
    portLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(portLabel.get());

    portEditor = std::make_unique<Label>("PortEditor", String(processor->getServerPort()));
    portEditor->setBounds(45, y, 50, 20);
    portEditor->setFont(Font("Default", 12, Font::plain));
    portEditor->setColour(Label::textColourId, Colours::white);
    portEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    portEditor->setEditable(true);
    portEditor->addListener(this);
    addAndMakeVisible(portEditor.get());

    serverButton = std::make_unique<TextButton>("Start Server");
    serverButton->setBounds(100, y, 95, 20);
    serverButton->addListener(this);
    addAndMakeVisible(serverButton.get());

    // Auto-start toggle (same row, right side)
    autoStartButton = std::make_unique<ToggleButton>("Auto");
    autoStartButton->setBounds(200, y, 55, 20);
    autoStartButton->setToggleState(processor->getAutoStart(), dontSendNotification);
    autoStartButton->addListener(this);
    addAndMakeVisible(autoStartButton.get());

    // Launch GUI button (far right)
    launchGuiButton = std::make_unique<TextButton>("GUI");
    launchGuiButton->setBounds(255, y, 35, 20);
    launchGuiButton->setColour(TextButton::buttonColourId, Colour(0xff1565c0));
    launchGuiButton->setTooltip("Launch external Paradigm Bridge GUI");
    launchGuiButton->addListener(this);
    addAndMakeVisible(launchGuiButton.get());

    y += 24;

    // ===== Rows 2-3: Status Display =====

    serverStatusLabel = std::make_unique<Label>("ServerStatus", "Server: Stopped");
    serverStatusLabel->setBounds(10, y, 140, 16);
    serverStatusLabel->setFont(Font("Default", 11, Font::plain));
    serverStatusLabel->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(serverStatusLabel.get());

    connectionStatusLabel = std::make_unique<Label>("ConnStatus", "Client: --");
    connectionStatusLabel->setBounds(150, y, 140, 16);
    connectionStatusLabel->setFont(Font("Default", 11, Font::plain));
    connectionStatusLabel->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(connectionStatusLabel.get());

    y += 17;

    triggerCountLabel = std::make_unique<Label>("TrigCount", "Triggers: 0");
    triggerCountLabel->setBounds(10, y, 100, 16);
    triggerCountLabel->setFont(Font("Default", 11, Font::plain));
    triggerCountLabel->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(triggerCountLabel.get());

    lastCommandLabel = std::make_unique<Label>("LastCmd", "Last: --");
    lastCommandLabel->setBounds(110, y, 180, 16);
    lastCommandLabel->setFont(Font("Default", 11, Font::plain));
    lastCommandLabel->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(lastCommandLabel.get());

    y += 20;

    // ===== Row 4: Recording Controls =====

    recordingLabel = std::make_unique<Label>("RecLabel", "Rec");
    recordingLabel->setBounds(10, y, 28, 18);
    recordingLabel->setFont(Font("Default", 11, Font::bold));
    recordingLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(recordingLabel.get());

    recordingNameEditor = std::make_unique<Label>("RecName", "experiment");
    recordingNameEditor->setBounds(38, y, 100, 18);
    recordingNameEditor->setFont(Font("Default", 11, Font::plain));
    recordingNameEditor->setColour(Label::textColourId, Colours::white);
    recordingNameEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    recordingNameEditor->setEditable(true);
    recordingNameEditor->setTooltip("Recording directory name");
    addAndMakeVisible(recordingNameEditor.get());

    recordStartButton = std::make_unique<TextButton>("REC");
    recordStartButton->setBounds(143, y, 40, 18);
    recordStartButton->setColour(TextButton::buttonColourId, Colour(0xffc62828));
    recordStartButton->setTooltip("Start recording");
    recordStartButton->addListener(this);
    addAndMakeVisible(recordStartButton.get());

    recordStopButton = std::make_unique<TextButton>("STOP");
    recordStopButton->setBounds(187, y, 42, 18);
    recordStopButton->setColour(TextButton::buttonColourId, Colour(0xff424242));
    recordStopButton->setTooltip("Stop recording");
    recordStopButton->addListener(this);
    addAndMakeVisible(recordStopButton.get());

    y += 22;

    // ===== Row 5: Manual Trigger Controls =====

    triggerLabel = std::make_unique<Label>("TrigLabel", "Trigger");
    triggerLabel->setBounds(10, y, 50, 18);
    triggerLabel->setFont(Font("Default", 11, Font::bold));
    triggerLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(triggerLabel.get());

    triggerLineLabel = std::make_unique<Label>("LineLabel", "Line:");
    triggerLineLabel->setBounds(60, y, 32, 18);
    triggerLineLabel->setFont(Font("Default", 11, Font::plain));
    triggerLineLabel->setColour(Label::textColourId, Colours::darkgrey);
    addAndMakeVisible(triggerLineLabel.get());

    triggerLineEditor = std::make_unique<Label>("LineEditor", "0");
    triggerLineEditor->setBounds(92, y, 22, 18);
    triggerLineEditor->setFont(Font("Default", 11, Font::plain));
    triggerLineEditor->setColour(Label::textColourId, Colours::white);
    triggerLineEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    triggerLineEditor->setEditable(true);
    addAndMakeVisible(triggerLineEditor.get());

    triggerOnButton = std::make_unique<TextButton>("ON");
    triggerOnButton->setBounds(120, y, 50, 18);
    triggerOnButton->setColour(TextButton::buttonColourId, Colour(0xff2e7d32));
    triggerOnButton->addListener(this);
    addAndMakeVisible(triggerOnButton.get());

    triggerOffButton = std::make_unique<TextButton>("OFF");
    triggerOffButton->setBounds(175, y, 50, 18);
    triggerOffButton->setColour(TextButton::buttonColourId, Colour(0xffc62828));
    triggerOffButton->addListener(this);
    addAndMakeVisible(triggerOffButton.get());

    triggerPulseButton = std::make_unique<TextButton>("PULSE");
    triggerPulseButton->setBounds(230, y, 55, 18);
    triggerPulseButton->setColour(TextButton::buttonColourId, Colour(0xff1565c0));
    triggerPulseButton->setTooltip("Point annotation (immediate ON/OFF pulse)");
    triggerPulseButton->addListener(this);
    addAndMakeVisible(triggerPulseButton.get());

    // Initial state
    updateServerButton();
    startTimerHz(5); // 5 Hz status refresh
}


ParadigmBridgeEditor::~ParadigmBridgeEditor()
{
    stopTimer();
}


// ==========================================================================
// Button / Label handlers
// ==========================================================================

void ParadigmBridgeEditor::buttonClicked(Button* button)
{
    if (button == serverButton.get())
    {
        if (processor->isServerRunning())
        {
            processor->stopServer();
        }
        else
        {
            int port = portEditor->getText().getIntValue();
            if (port >= 1024 && port <= 65535)
            {
                if (!processor->startServer(port))
                {
                    CoreServices::sendStatusMessage(
                        "Paradigm Bridge: Failed to start server on port " + String(port));
                }
            }
            else
            {
                CoreServices::sendStatusMessage(
                    "Paradigm Bridge: Invalid port (must be 1024-65535)");
            }
        }
        updateServerButton();
    }
    else if (button == autoStartButton.get())
    {
        processor->setAutoStart(autoStartButton->getToggleState());
    }
    else if (button == triggerOnButton.get())
    {
        int line = triggerLineEditor->getText().getIntValue();
        if (line >= 0 && line <= 7 && processor->isAcquisitionActive())
            processor->sendManualTrigger(line, true);
    }
    else if (button == triggerOffButton.get())
    {
        int line = triggerLineEditor->getText().getIntValue();
        if (line >= 0 && line <= 7 && processor->isAcquisitionActive())
            processor->sendManualTrigger(line, false);
    }
    else if (button == triggerPulseButton.get())
    {
        int line = triggerLineEditor->getText().getIntValue();
        if (line >= 0 && line <= 7 && processor->isAcquisitionActive())
        {
            processor->sendManualTrigger(line, true);
            processor->sendManualTrigger(line, false);
        }
    }
    else if (button == launchGuiButton.get())
    {
        launchExternalGui();
    }
    else if (button == recordStartButton.get())
    {
        // Use Open Ephys HTTP API to start recording
        String recName = recordingNameEditor->getText();
        // Set recording directory via HTTP API then start recording
        URL urlDir("http://localhost:37497/api/recording");
        String body = "{\"base_text\":\"" + recName + "\"}";
        URL urlWithBody = urlDir.withPOSTData(body);

        // Send PUT request to set recording name
        StringPairArray responseHeaders;
        int statusCode = 0;
        auto stream = urlDir.createInputStream(
            URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                .withHttpRequestCmd("PUT")
                .withExtraHeaders("Content-Type: application/json")
                .withConnectionTimeoutMs(2000));

        // Now start recording
        URL urlStatus("http://localhost:37497/api/status");
        auto stream2 = urlStatus.createInputStream(
            URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                .withHttpRequestCmd("PUT")
                .withExtraHeaders("Content-Type: application/json")
                .withConnectionTimeoutMs(2000));

        CoreServices::sendStatusMessage("Paradigm Bridge: Recording started (" + recName + ")");
    }
    else if (button == recordStopButton.get())
    {
        // Stop recording via HTTP API
        URL urlStatus("http://localhost:37497/api/status");
        auto stream = urlStatus.createInputStream(
            URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                .withHttpRequestCmd("PUT")
                .withExtraHeaders("Content-Type: application/json")
                .withConnectionTimeoutMs(2000));

        CoreServices::sendStatusMessage("Paradigm Bridge: Recording stopped");
    }
}


void ParadigmBridgeEditor::labelTextChanged(Label* label)
{
    if (label == portEditor.get())
    {
        int port = label->getText().getIntValue();
        if (port >= 1024 && port <= 65535)
        {
            processor->setServerPort(port);
        }
        else
        {
            // Revert to current port
            label->setText(String(processor->getServerPort()), dontSendNotification);
        }
    }
}


// ==========================================================================
// Timer callback - periodic status refresh
// ==========================================================================

void ParadigmBridgeEditor::timerCallback()
{
    updateStatusDisplay();
}


// ==========================================================================
// Acquisition lifecycle
// ==========================================================================

void ParadigmBridgeEditor::startAcquisition()
{
    // Lock port editing during acquisition
    portEditor->setEditable(false);
    portEditor->setColour(Label::backgroundColourId, Colours::grey.darker());
    updateStatusDisplay();
}


void ParadigmBridgeEditor::stopAcquisition()
{
    // Unlock port editing
    if (!processor->isServerRunning())
    {
        portEditor->setEditable(true);
        portEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
    }
    updateStatusDisplay();
}


// ==========================================================================
// UI update helpers
// ==========================================================================

void ParadigmBridgeEditor::updateServerButton()
{
    if (processor->isServerRunning())
    {
        serverButton->setButtonText("Stop Server");
        portEditor->setEditable(false);
        portEditor->setColour(Label::backgroundColourId, Colours::grey.darker());
    }
    else
    {
        serverButton->setButtonText("Start Server");
        if (!processor->isAcquisitionActive())
        {
            portEditor->setEditable(true);
            portEditor->setColour(Label::backgroundColourId, Colours::darkgrey);
        }
    }
}


void ParadigmBridgeEditor::updateStatusDisplay()
{
    // Server status
    if (processor->isServerRunning())
    {
        serverStatusLabel->setText(
            "Server: :" + String(processor->getServerPort()), dontSendNotification);
        serverStatusLabel->setColour(Label::textColourId, Colours::green);
    }
    else
    {
        serverStatusLabel->setText("Server: Stopped", dontSendNotification);
        serverStatusLabel->setColour(Label::textColourId, Colours::grey);
    }

    // Client connection
    if (processor->isClientConnected())
    {
        connectionStatusLabel->setText("Client: Connected", dontSendNotification);
        connectionStatusLabel->setColour(Label::textColourId, Colours::green);
    }
    else
    {
        connectionStatusLabel->setText("Client: --", dontSendNotification);
        connectionStatusLabel->setColour(Label::textColourId, Colours::grey);
    }

    // Trigger count
    triggerCountLabel->setText(
        "Triggers: " + String(processor->getTriggerCount()), dontSendNotification);

    // Last command (truncate for display)
    String lastCmd = processor->getLastCommand();
    if (lastCmd.isNotEmpty())
    {
        if (lastCmd.length() > 22)
            lastCmd = lastCmd.substring(0, 19) + "...";
        lastCommandLabel->setText("Last: " + lastCmd, dontSendNotification);
    }

    // Update server button state
    updateServerButton();
}


// ==========================================================================
// Launch external GUI
// ==========================================================================

void ParadigmBridgeEditor::launchExternalGui()
{
    // Try to find python executable and launch paradigm_bridge GUI
    // Search common locations for the workspace venv
    File pluginDir = File::getSpecialLocation(File::currentExecutableFile).getParentDirectory();

    // Walk up to find the workspace root (look for .venv)
    File searchDir = pluginDir;
    File venvPython;
    for (int i = 0; i < 8; i++)
    {
        File candidate = searchDir.getChildFile(".venv").getChildFile("Scripts").getChildFile("python.exe");
        if (candidate.existsAsFile())
        {
            venvPython = candidate;
            break;
        }
        candidate = searchDir.getChildFile(".venv").getChildFile("bin").getChildFile("python");
        if (candidate.existsAsFile())
        {
            venvPython = candidate;
            break;
        }
        searchDir = searchDir.getParentDirectory();
    }

    String pythonExe;
    if (venvPython.existsAsFile())
    {
        pythonExe = venvPython.getFullPathName();
    }
    else
    {
        // Fall back to system python
        pythonExe = "python";
    }

    String port = String(processor->getServerPort());

    // Launch: python -m paradigm_bridge --port <port>
    ChildProcess* guiProcess = new ChildProcess();
    StringArray args;
    args.add(pythonExe);
    args.add("-m");
    args.add("paradigm_bridge");
    args.add("--port");
    args.add(port);

    if (guiProcess->start(args))
    {
        CoreServices::sendStatusMessage(
            "Paradigm Bridge: Launched external GUI (port " + port + ")");
    }
    else
    {
        CoreServices::sendStatusMessage(
            "Paradigm Bridge: Failed to launch GUI. Is paradigm_bridge installed?");
        delete guiProcess;
    }
}
