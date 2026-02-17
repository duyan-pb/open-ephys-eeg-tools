/*
    ------------------------------------------------------------------

    Paradigm Bridge - TCP Command Server Implementation

    ------------------------------------------------------------------
*/

#include "TcpCommandServer.h"


TcpCommandServer::TcpCommandServer(TcpCommandListener* listener)
    : Thread("ParadigmBridge_TCP"),
      listener(listener),
      currentPort(0),
      serverRunning(false),
      clientConnected(false),
      commandCount(0)
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

            // Process all complete lines (newline-delimited)
            int newlinePos;
            while ((newlinePos = buffer.indexOf("\n")) >= 0)
            {
                String line = buffer.substring(0, newlinePos).trim();
                buffer = buffer.substring(newlinePos + 1);

                if (line.isNotEmpty())
                {
                    String response = processCommand(line) + "\n";
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

String TcpCommandServer::processCommand(const String& command)
{
    // Record the command for status display
    {
        const ScopedLock sl(lastCommandLock);
        lastCommand = command;
    }
    commandCount++;

    String trimmed = command.trim();

    // --- TRIGGER <line> <state> ---
    if (trimmed.startsWithIgnoreCase("TRIGGER "))
    {
        StringArray parts;
        parts.addTokens(trimmed, " ", "");

        if (parts.size() < 3)
            return "ERROR TRIGGER requires <line> <state>";

        int line = parts[1].getIntValue();
        int state = parts[2].getIntValue();

        if (line < 0 || line > 7)
            return "ERROR line must be 0-7";

        if (state != 0 && state != 1)
            return "ERROR state must be 0 or 1";

        if (listener)
            listener->triggerReceived(line, state != 0);

        return "OK TRIGGER " + String(line) + " " + String(state);
    }

    // --- RECORD START ---
    else if (trimmed.startsWithIgnoreCase("RECORD START"))
    {
        if (listener) listener->recordStartReceived();
        return "OK RECORD START";
    }

    // --- RECORD STOP ---
    else if (trimmed.startsWithIgnoreCase("RECORD STOP"))
    {
        if (listener) listener->recordStopReceived();
        return "OK RECORD STOP";
    }

    // --- RECORD DIR <path> ---
    else if (trimmed.startsWithIgnoreCase("RECORD DIR "))
    {
        String dir = trimmed.substring(11).trim(); // len("RECORD DIR ") = 11
        if (dir.isEmpty())
            return "ERROR RECORD DIR requires a path";

        if (listener) listener->setRecordingDirReceived(dir);
        return "OK RECORD DIR " + dir;
    }

    // --- RECORD NAME <name> ---
    else if (trimmed.startsWithIgnoreCase("RECORD NAME "))
    {
        String name = trimmed.substring(12).trim(); // len("RECORD NAME ") = 12
        if (name.isEmpty())
            return "ERROR RECORD NAME requires a name";

        if (listener) listener->setRecordingNameReceived(name);
        return "OK RECORD NAME " + name;
    }

    // --- RECORD NEWDIR ---
    else if (trimmed.startsWithIgnoreCase("RECORD NEWDIR"))
    {
        if (listener) listener->newRecordingDirReceived();
        return "OK RECORD NEWDIR";
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
