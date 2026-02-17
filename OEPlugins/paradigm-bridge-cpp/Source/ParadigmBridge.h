/*
    ------------------------------------------------------------------

    Paradigm Bridge - Main Processor

    A native C++ Open Ephys processor plugin that bridges external
    paradigm software (PsychoPy, MATLAB, custom scripts) to the
    Open Ephys signal chain.

    Features:
    - TCP server for receiving commands from external software
    - TTL event injection into the signal chain (8 lines)
    - Recording control via CoreServices (start/stop/directory)
    - Status message forwarding to the GUI console
    - Manual trigger buttons in the editor UI

    Signal chain placement: Source → [Paradigm Bridge] → Record Node
    Plugin type: FILTER (passes all continuous data through unchanged)

    ------------------------------------------------------------------
*/

#ifndef PARADIGM_BRIDGE_H_DEFINED
#define PARADIGM_BRIDGE_H_DEFINED

#include <ProcessorHeaders.h>
#include "TcpCommandServer.h"

#include <vector>
#include <atomic>

/**
 * Paradigm Bridge Processor
 *
 * Sits in the Open Ephys signal chain as a filter processor.
 * Receives commands via TCP from external paradigm software and:
 * - Injects TTL events into the signal chain
 * - Controls recording (start/stop/directory) via CoreServices
 * - Forwards status messages to the GUI console
 *
 * All continuous data passes through unchanged.
 */
class ParadigmBridge : public GenericProcessor,
                       public TcpCommandListener
{
public:
    ParadigmBridge();
    ~ParadigmBridge();

    // === GenericProcessor overrides ===

    /** Creates the custom editor with server controls and trigger buttons */
    AudioProcessorEditor* createEditor() override;

    /** Registers saveable parameters (port, auto-start) */
    void registerParameters() override;

    /** Handles parameter changes from saved configurations */
    void parameterValueChanged(Parameter* param) override;

    /** Creates the TTL event channel for trigger injection */
    void updateSettings() override;

    /** Processes pending trigger commands and injects TTL events */
    void process(AudioBuffer<float>& buffer) override;

    /** Called when acquisition starts - optionally auto-starts TCP server */
    bool startAcquisition() override;

    /** Called when acquisition stops */
    bool stopAcquisition() override;


    // === TcpCommandListener overrides ===

    void triggerReceived(int line, bool state) override;
    void recordStartReceived() override;
    void recordStopReceived() override;
    void setRecordingDirReceived(const String& dir) override;
    void setRecordingNameReceived(const String& name) override;
    void newRecordingDirReceived() override;
    void statusMessageReceived(const String& text) override;
    String getStatusString() override;


    // === Public API for editor ===

    /** Start the TCP server on the specified port */
    bool startServer(int port);

    /** Stop the TCP server */
    void stopServer();

    /** Check if server is currently listening */
    bool isServerRunning() const;

    /** Check if a paradigm client is connected */
    bool isClientConnected() const;

    /** Get the configured server port */
    int getServerPort() const { return serverPort; }

    /** Set the server port (only when not running) */
    void setServerPort(int port);

    /** Get total trigger count since last acquisition start */
    int getTriggerCount() const { return triggerCount.load(); }

    /** Get total command count since server started */
    int getCommandCount() const;

    /** Get the last received command string */
    String getLastCommand() const;

    /** Check if acquisition is currently active */
    bool isAcquisitionActive() const { return acquisitionActive.load(); }

    /** Get auto-start setting */
    bool getAutoStart() const { return autoStartServer; }

    /** Set auto-start setting */
    void setAutoStart(bool enable);

    /** Send a manual trigger from the editor UI */
    void sendManualTrigger(int line, bool state);


private:
    /** Pending trigger to be processed in the audio thread */
    struct PendingTrigger
    {
        int line;
        bool state;
    };

    std::unique_ptr<TcpCommandServer> tcpServer;

    // Thread-safe trigger queue (TCP thread → audio thread)
    CriticalSection triggerLock;
    std::vector<PendingTrigger> pendingTriggers;

    // State
    std::atomic<bool> acquisitionActive;
    std::atomic<int> triggerCount;
    int serverPort;
    bool autoStartServer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParadigmBridge);
};

#endif // PARADIGM_BRIDGE_H_DEFINED
