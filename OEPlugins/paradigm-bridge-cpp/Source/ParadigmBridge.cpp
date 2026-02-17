/*
    ------------------------------------------------------------------

    Paradigm Bridge - Main Processor Implementation

    ------------------------------------------------------------------
*/

#include "ParadigmBridge.h"
#include "ParadigmBridgeEditor.h"


ParadigmBridge::ParadigmBridge()
    : GenericProcessor("Paradigm Bridge"),
      acquisitionActive(false),
      triggerCount(0),
      serverPort(5557),
      autoStartServer(true)
{
    tcpServer = std::make_unique<TcpCommandServer>(this);
}


ParadigmBridge::~ParadigmBridge()
{
    tcpServer->stopServer();
}


// ==========================================================================
// GenericProcessor overrides
// ==========================================================================

AudioProcessorEditor* ParadigmBridge::createEditor()
{
    editor = std::make_unique<ParadigmBridgeEditor>(this);
    return editor.get();
}


void ParadigmBridge::registerParameters()
{
    addStringParameter(Parameter::GLOBAL_SCOPE,
        "port", "TCP Port",
        "TCP port for incoming paradigm connections",
        String(serverPort));

    addBooleanParameter(Parameter::GLOBAL_SCOPE,
        "auto_start", "Auto Start",
        "Automatically start TCP server when acquisition begins",
        autoStartServer);
}


void ParadigmBridge::parameterValueChanged(Parameter* param)
{
    if (param->getName() == "port")
    {
        int newPort = param->getValueAsString().getIntValue();
        if (newPort >= 1024 && newPort <= 65535)
            serverPort = newPort;
    }
    else if (param->getName() == "auto_start")
    {
        autoStartServer = (bool)param->getValue();
    }
}


void ParadigmBridge::updateSettings()
{
    // Create a TTL event channel with 8 lines for trigger injection.
    // Uses the simplified GenericProcessor helper which creates an
    // EventChannel on the first available data stream.
    addTTLChannel("Paradigm Bridge TTL");
}


void ParadigmBridge::process(AudioBuffer<float>& buffer)
{
    // Dequeue pending triggers from the TCP server thread and
    // inject them as TTL events into the signal chain.
    {
        const ScopedLock sl(triggerLock);

        if (!pendingTriggers.empty() && getDataStreams().size() > 0)
        {
            for (const auto& trigger : pendingTriggers)
            {
                // sampleIndex=0 places the event at the start of this buffer block.
                // setTTLState() creates a TTLEvent and adds it via addEvent().
                setTTLState(0, trigger.line, trigger.state);
            }
        }

        pendingTriggers.clear();
    }

    // All continuous data passes through unchanged (filter processor).
}


bool ParadigmBridge::startAcquisition()
{
    acquisitionActive = true;
    triggerCount = 0;

    // Clear any stale triggers from before acquisition
    {
        const ScopedLock sl(triggerLock);
        pendingTriggers.clear();
    }

    // Auto-start TCP server if configured and not already running
    if (autoStartServer && !tcpServer->isServerRunning())
    {
        if (tcpServer->startServer(serverPort))
        {
            LOGC("Paradigm Bridge: TCP server started on port ", serverPort);
        }
        else
        {
            LOGC("Paradigm Bridge: WARNING - Failed to start TCP server on port ", serverPort);
        }
    }

    LOGC("Paradigm Bridge: Acquisition started");
    return true;
}


bool ParadigmBridge::stopAcquisition()
{
    acquisitionActive = false;

    LOGC("Paradigm Bridge: Acquisition stopped (", triggerCount.load(), " triggers sent)");
    return true;
}


// ==========================================================================
// TcpCommandListener implementation
// ==========================================================================

void ParadigmBridge::triggerReceived(int line, bool state)
{
    // Queue the trigger for processing in the audio thread.
    // Only accept triggers when acquisition is active (TTL events
    // can only be injected while the signal chain is running).
    if (acquisitionActive.load())
    {
        const ScopedLock sl(triggerLock);
        pendingTriggers.push_back({ line, state });
        triggerCount++;
    }
}


void ParadigmBridge::recordStartReceived()
{
    // Must be called on the message thread for CoreServices safety
    MessageManager::callAsync([]()
    {
        if (CoreServices::getAcquisitionStatus())
        {
            CoreServices::setRecordingStatus(true);
            LOGC("Paradigm Bridge: Recording started via TCP command");
        }
        else
        {
            LOGC("Paradigm Bridge: Cannot start recording - acquisition not active");
        }
    });
}


void ParadigmBridge::recordStopReceived()
{
    MessageManager::callAsync([]()
    {
        CoreServices::setRecordingStatus(false);
        LOGC("Paradigm Bridge: Recording stopped via TCP command");
    });
}


void ParadigmBridge::setRecordingDirReceived(const String& dir)
{
    MessageManager::callAsync([dir]()
    {
        CoreServices::setRecordingParentDirectory(dir);
        LOGC("Paradigm Bridge: Recording directory set to ", dir);
    });
}


void ParadigmBridge::setRecordingNameReceived(const String& name)
{
    MessageManager::callAsync([name]()
    {
        CoreServices::setRecordingDirectoryBaseText(name);
        LOGC("Paradigm Bridge: Recording name set to ", name);
    });
}


void ParadigmBridge::newRecordingDirReceived()
{
    MessageManager::callAsync([]()
    {
        CoreServices::createNewRecordingDirectory();
        LOGC("Paradigm Bridge: New recording directory created");
    });
}


void ParadigmBridge::statusMessageReceived(const String& text)
{
    MessageManager::callAsync([text]()
    {
        CoreServices::sendStatusMessage("Paradigm: " + text);
    });
}


String ParadigmBridge::getStatusString()
{
    String status;
    status += "ACQUISITION=" + String(acquisitionActive.load() ? "ON" : "OFF");
    status += " RECORDING=" + String(CoreServices::getRecordingStatus() ? "ON" : "OFF");
    status += " TRIGGERS=" + String(triggerCount.load());
    return status;
}


// ==========================================================================
// Public API for editor
// ==========================================================================

bool ParadigmBridge::startServer(int port)
{
    serverPort = port;

    // Update parameter for save/restore
    Parameter* param = getParameter("port");
    if (param)
        param->setNextValue(String(port));

    bool success = tcpServer->startServer(port);

    if (success)
    {
        LOGC("Paradigm Bridge: TCP server started on port ", port);
    }
    else
    {
        LOGC("Paradigm Bridge: Failed to start TCP server on port ", port);
    }

    return success;
}


void ParadigmBridge::stopServer()
{
    tcpServer->stopServer();
    LOGC("Paradigm Bridge: TCP server stopped");
}


bool ParadigmBridge::isServerRunning() const
{
    return tcpServer->isServerRunning();
}


bool ParadigmBridge::isClientConnected() const
{
    return tcpServer->isClientConnected();
}


void ParadigmBridge::setServerPort(int port)
{
    if (!tcpServer->isServerRunning() && port >= 1024 && port <= 65535)
    {
        serverPort = port;
        Parameter* param = getParameter("port");
        if (param)
            param->setNextValue(String(port));
    }
}


void ParadigmBridge::setAutoStart(bool enable)
{
    autoStartServer = enable;
    Parameter* param = getParameter("auto_start");
    if (param)
        param->setNextValue(enable ? 1.0f : 0.0f);
}


int ParadigmBridge::getCommandCount() const
{
    return tcpServer->getCommandCount();
}


String ParadigmBridge::getLastCommand() const
{
    return tcpServer->getLastCommand();
}


void ParadigmBridge::sendManualTrigger(int line, bool state)
{
    // Reuse the same path as TCP-received triggers
    triggerReceived(line, state);
}
