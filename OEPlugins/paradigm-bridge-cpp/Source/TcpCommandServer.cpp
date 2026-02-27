/*
    ------------------------------------------------------------------

    Paradigm Bridge - TCP Command Server Implementation

    ------------------------------------------------------------------
*/

#include "TcpCommandServer.h"

#include <climits>


TcpCommandServer::TcpCommandServer(TcpCommandListener* listener)
    : Thread("ParadigmBridge_TCP"),
      listener(listener),
      currentPort(0),
      serverRunning(false),
      clientConnected(false),
      commandCount(0),
      allowRemoteConnections(false)
{
}


TcpCommandServer::~TcpCommandServer()
{
    stopServer();
}


bool TcpCommandServer::startServer(int port)
{
    stopServer();

    currentPort = port;

    if (serverSocket.createListener(port))
    {
        serverRunning = true;
        startThread();
        return true;
    }

    return false;
}


void TcpCommandServer::stopServer()
{
    serverRunning = false;
    serverSocket.close();
    stopThread(3000);
    clientConnected = false;
}


String TcpCommandServer::getLastCommand() const
{
    const ScopedLock sl(lastCommandLock);
    return lastCommand;
}

void TcpCommandServer::setRequiredAuthToken(const String& token)
{
    const ScopedLock sl(authTokenLock);
    requiredAuthToken = token.trim();
}

String TcpCommandServer::getRequiredAuthToken() const
{
    const ScopedLock sl(authTokenLock);
    return requiredAuthToken;
}


// ==========================================================================
// Background thread: accept connections and handle clients
// ==========================================================================

void TcpCommandServer::run()
{
    while (!threadShouldExit() && serverRunning.load())
    {
        // Wait for an incoming connection (200ms timeout to allow exit checks)
        int ready = serverSocket.waitUntilReady(true, 200);

        if (ready == 1)
        {
            StreamingSocket* client = serverSocket.waitForNextConnection();

            if (client != nullptr)
            {
                clientConnected = true;
                handleClient(client);
                delete client;
                clientConnected = false;
            }
        }
        else if (ready < 0)
        {
            // Socket error (likely closed during shutdown)
            break;
        }
        // ready == 0 means timeout, just loop and check exit flag
    }
}


void TcpCommandServer::handleClient(StreamingSocket* client)
{
    String buffer;
    char readBuffer[4096];
    ClientSessionState session;

    const String remoteHost = client->getHostName().trim();
    if (!allowRemoteConnections.load() && !isLoopbackHost(remoteHost))
    {
        const String response =
            "ERROR remote connections are disabled; use localhost or enable remote access\n";
        client->write(response.toRawUTF8(), (int)response.getNumBytesAsUTF8());
        return;
    }

    session.authenticated = getRequiredAuthToken().isEmpty();

    while (!threadShouldExit() && serverRunning.load() && client->isConnected())
    {
        // Check for incoming data with 100ms timeout
        int ready = client->waitUntilReady(true, 100);

        if (ready == 1)
        {
            int bytesRead = client->read(readBuffer, sizeof(readBuffer) - 1, false);

            if (bytesRead <= 0)
                break; // Client disconnected

            readBuffer[bytesRead] = '\0';
            buffer += String::fromUTF8(readBuffer, bytesRead);

            if (buffer.getNumBytesAsUTF8() > kMaxBufferedBytes)
            {
                const String response = "ERROR input buffer exceeded maximum size\n";
                client->write(response.toRawUTF8(), (int)response.getNumBytesAsUTF8());
                break;
            }

            // Process all complete lines (newline-delimited)
            int newlinePos;
            while ((newlinePos = buffer.indexOf("\n")) >= 0)
            {
                String line = buffer.substring(0, newlinePos).trim();
                buffer = buffer.substring(newlinePos + 1);

                if (line.isNotEmpty())
                {
                    if (line.getNumBytesAsUTF8() > kMaxCommandBytes)
                    {
                        String response = "ERROR command too long\n";
                        client->write(response.toRawUTF8(), (int)response.getNumBytesAsUTF8());
                        continue;
                    }

                    String response = processCommand(line, session) + "\n";
                    client->write(response.toRawUTF8(), (int)response.getNumBytesAsUTF8());
                }
            }
        }
        else if (ready < 0)
        {
            break; // Socket error
        }
    }
}


// ==========================================================================
// Command parsing and dispatch
// ==========================================================================

String TcpCommandServer::processCommand(const String& command, ClientSessionState& session)
{
    const String trimmed = command.trim();

    // Record a sanitized command preview for status display
    {
        const ScopedLock sl(lastCommandLock);
        lastCommand = getCommandPreview(trimmed);
    }
    commandCount++;

    const String requiredToken = getRequiredAuthToken();

    // --- AUTH <token> ---
    if (trimmed.startsWithIgnoreCase("AUTH "))
    {
        if (requiredToken.isEmpty())
            return "OK AUTH NOT_REQUIRED";

        const String suppliedToken = trimmed.substring(5).trim();
        if (suppliedToken.isEmpty())
            return "ERROR AUTH requires a token";

        if (suppliedToken == requiredToken)
        {
            session.authenticated = true;
            return "OK AUTH";
        }

        return "ERROR invalid auth token";
    }

    if (!requiredToken.isEmpty() && !session.authenticated)
    {
        return "ERROR unauthorized; send AUTH <token>";
    }

    // --- PULSE <line> ---
    if (trimmed.startsWithIgnoreCase("PULSE "))
    {
        StringArray parts;
        parts.addTokens(trimmed, " ", "");

        if (parts.size() != 2)
            return "ERROR PULSE requires <line>";

        int line = 0;
        if (!tryParseIntStrict(parts[1], line))
            return "ERROR line must be an integer";

        if (line < 0 || line > 7)
            return "ERROR line must be 0-7";

        if (listener)
        {
            // Point annotation semantics: enqueue ON and OFF back-to-back.
            // This avoids a client-side sleep/round-trip and keeps the
            // command path lightweight under load.
            listener->triggerReceived(line, true);
            listener->triggerReceived(line, false);
        }

        return "OK PULSE " + String(line);
    }

    // --- TRIGGER <line> <state> ---
    else if (trimmed.startsWithIgnoreCase("TRIGGER "))
    {
        StringArray parts;
        parts.addTokens(trimmed, " ", "");

        if (parts.size() != 3)
            return "ERROR TRIGGER requires <line> <state>";

        int line = 0;
        int state = 0;

        if (!tryParseIntStrict(parts[1], line))
            return "ERROR line must be an integer";

        if (!tryParseIntStrict(parts[2], state))
            return "ERROR state must be 0 or 1";

        if (line < 0 || line > 7)
            return "ERROR line must be 0-7";

        if (state != 0 && state != 1)
            return "ERROR state must be 0 or 1";

        if (listener)
            listener->triggerReceived(line, state != 0);

        return "OK TRIGGER " + String(line) + " " + String(state);
    }

    // --- RECORD START ---
    else if (trimmed.equalsIgnoreCase("RECORD START"))
    {
        if (listener == nullptr)
            return "ERROR listener unavailable";

        if (!listener->isAcquisitionActiveForCommands())
            return "ERROR acquisition must be active";

        listener->recordStartReceived();
        return "OK RECORD START ACCEPTED";
    }

    // --- RECORD STOP ---
    else if (trimmed.equalsIgnoreCase("RECORD STOP"))
    {
        if (listener) listener->recordStopReceived();
        return "OK RECORD STOP ACCEPTED";
    }

    // --- RECORD DIR <path> ---
    else if (trimmed.startsWithIgnoreCase("RECORD DIR "))
    {
        String dir = trimmed.substring(11).trim(); // len("RECORD DIR ") = 11
        if (dir.isEmpty())
            return "ERROR RECORD DIR requires a path";

        if (listener) listener->setRecordingDirReceived(dir);
        return "OK RECORD DIR ACCEPTED";
    }

    // --- RECORD NAME <name> ---
    else if (trimmed.startsWithIgnoreCase("RECORD NAME "))
    {
        String name = trimmed.substring(12).trim(); // len("RECORD NAME ") = 12
        if (name.isEmpty())
            return "ERROR RECORD NAME requires a name";

        if (listener) listener->setRecordingNameReceived(name);
        return "OK RECORD NAME ACCEPTED";
    }

    // --- RECORD NEWDIR ---
    else if (trimmed.equalsIgnoreCase("RECORD NEWDIR"))
    {
        if (listener) listener->newRecordingDirReceived();
        return "OK RECORD NEWDIR ACCEPTED";
    }

    // --- MESSAGE <text> ---
    else if (trimmed.startsWithIgnoreCase("MESSAGE "))
    {
        String text = trimmed.substring(8).trim(); // len("MESSAGE ") = 8
        if (text.isEmpty())
            return "ERROR MESSAGE requires text";

        if (listener) listener->statusMessageReceived(text);
        return "OK MESSAGE";
    }

    // --- PING ---
    else if (trimmed.equalsIgnoreCase("PING"))
    {
        return "OK PONG";
    }

    // --- STATUS ---
    else if (trimmed.equalsIgnoreCase("STATUS"))
    {
        if (listener)
            return "OK " + listener->getStatusString();
        return "OK UNKNOWN";
    }

    // --- Unknown command ---
    else
    {
        return "ERROR unknown command: " + trimmed;
    }
}

bool TcpCommandServer::tryParseIntStrict(const String& text, int& valueOut)
{
    const String s = text.trim();
    if (s.isEmpty())
        return false;

    int index = 0;
    bool negative = false;

    if (s[0] == '+' || s[0] == '-')
    {
        negative = (s[0] == '-');
        index = 1;
        if (index >= s.length())
            return false;
    }

    long long value = 0;
    for (; index < s.length(); ++index)
    {
        const juce_wchar c = s[index];
        if (c < '0' || c > '9')
            return false;

        value = (value * 10) + (c - '0');
        if ((!negative && value > INT_MAX) || (negative && value > (long long)INT_MAX + 1))
            return false;
    }

    valueOut = negative ? (int)-value : (int)value;
    return true;
}

bool TcpCommandServer::isLoopbackHost(const String& host)
{
    const String normalized = host.trim().toLowerCase();
    return normalized == "127.0.0.1"
        || normalized == "::1"
        || normalized == "::ffff:127.0.0.1"
        || normalized == "localhost";
}

String TcpCommandServer::getCommandPreview(const String& command)
{
    if (command.startsWithIgnoreCase("AUTH "))
        return "AUTH [REDACTED]";

    constexpr int previewLength = 120;
    if (command.length() > previewLength)
        return command.substring(0, previewLength - 3) + "...";

    return command;
}
