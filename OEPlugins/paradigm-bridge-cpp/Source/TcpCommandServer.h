/*
    ------------------------------------------------------------------

    Paradigm Bridge - TCP Command Server
    
    Listens for incoming TCP connections from external paradigm 
    software (PsychoPy, MATLAB, custom scripts) and routes commands
    to the ParadigmBridge processor.

    Protocol: newline-terminated text commands
    
    Commands:
        TRIGGER <line> <state>    - Send TTL trigger interval edge (line 0-7, state 0|1)
        PULSE <line>              - Send a point-style marker (ON then OFF immediately)
        RECORD START              - Start recording
        RECORD STOP               - Stop recording
        RECORD DIR <path>         - Set recording parent directory
        RECORD NAME <name>        - Set recording directory base name
        RECORD NEWDIR             - Create new recording directory
        MESSAGE <text>            - Send status message to GUI console
        PING                      - Connection test (responds: OK PONG)
        STATUS                    - Get acquisition/recording status

    Responses:
        OK [data]                 - Command succeeded
        ERROR <message>           - Command failed

    ------------------------------------------------------------------
*/

#ifndef TCP_COMMAND_SERVER_H_DEFINED
#define TCP_COMMAND_SERVER_H_DEFINED

#include <JuceHeader.h>
#include <atomic>

/**
 * Listener interface for receiving parsed commands from the TCP server.
 * Implemented by ParadigmBridge processor.
 */
class TcpCommandListener
{
public:
    virtual ~TcpCommandListener() {}

    /** Called when a TTL trigger command is received (line 0-7, state on/off) */
    virtual void triggerReceived(int line, bool state) = 0;

    /** Called when RECORD START is received */
    virtual void recordStartReceived() = 0;

    /** Called when RECORD STOP is received */
    virtual void recordStopReceived() = 0;

    /** Called when RECORD DIR <path> is received */
    virtual void setRecordingDirReceived(const String& dir) = 0;

    /** Called when RECORD NAME <name> is received */
    virtual void setRecordingNameReceived(const String& name) = 0;

    /** Called when RECORD NEWDIR is received */
    virtual void newRecordingDirReceived() = 0;

    /** Called when MESSAGE <text> is received */
    virtual void statusMessageReceived(const String& text) = 0;

    /** Called when STATUS is received - return current status string */
    virtual String getStatusString() = 0;

    /** Returns whether acquisition is active (used for preflight checks). */
    virtual bool isAcquisitionActiveForCommands() const = 0;
};


/**
 * TCP server that listens for connections from external paradigm software.
 * Runs in a background JUCE thread. Handles one client at a time.
 * Commands are newline-terminated text strings.
 */
class TcpCommandServer : public Thread
{
public:
    TcpCommandServer(TcpCommandListener* listener);
    ~TcpCommandServer();

    /** Start listening on the specified port. Returns true on success. */
    bool startServer(int port);

    /** Stop the server and disconnect any active client. */
    void stopServer();

    /** Returns true if the server is currently listening. */
    bool isServerRunning() const { return serverRunning.load(); }

    /** Returns the port the server is listening on. */
    int getPort() const { return currentPort; }

    /** Returns true if a client is currently connected. */
    bool isClientConnected() const { return clientConnected.load(); }

    /** Returns the total number of commands processed. */
    int getCommandCount() const { return commandCount.load(); }

    /** Returns the last command string received (thread-safe copy). */
    String getLastCommand() const;

    /** Restrict accepted connections to loopback only unless enabled. */
    void setAllowRemoteConnections(bool allow) { allowRemoteConnections.store(allow); }

    /** Require AUTH <token> before processing privileged commands. */
    void setRequiredAuthToken(const String& token);

private:
    struct ClientSessionState
    {
        bool authenticated = false;
    };

    void run() override;
    void handleClient(StreamingSocket* client);
    String processCommand(const String& command, ClientSessionState& session);
    static bool tryParseIntStrict(const String& text, int& valueOut);
    static bool isLoopbackHost(const String& host);
    static String getCommandPreview(const String& command);
    String getRequiredAuthToken() const;

    TcpCommandListener* listener;
    StreamingSocket serverSocket;
    int currentPort;
    std::atomic<bool> serverRunning;
    std::atomic<bool> clientConnected;
    std::atomic<int> commandCount;
    std::atomic<bool> allowRemoteConnections;

    CriticalSection lastCommandLock;
    String lastCommand;
    mutable CriticalSection authTokenLock;
    String requiredAuthToken;

    static constexpr int kMaxCommandBytes = 1024;
    static constexpr int kMaxBufferedBytes = 8192;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TcpCommandServer);
};

#endif // TCP_COMMAND_SERVER_H_DEFINED
