/*
    OpenBCI Cyton Plugin for Open Ephys
    
    Implementation of DataThread for OpenBCI Cyton board
*/

#include "OpenBCICytonThread.h"
#include "OpenBCICytonEditor.h"

#ifdef _WIN32
    #include <setupapi.h>
    #pragma comment(lib, "setupapi.lib")
#else
    #include <dirent.h>
    #include <sys/ioctl.h>
#endif

using namespace OpenBCICyton;

OpenBCICytonThread::OpenBCICytonThread(SourceNode* sn)
    : DataThread(sn)
{
    // Initialize channel gains to default 24x
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        channelGains[i] = Gain::GAIN_24X;
        scaleFactors[i] = getScaleFactor(Gain::GAIN_24X);
    }
    
    // Initialize data buffers
    channelData.fill(0.0f);
    accelData.fill(0.0f);
    accelHighBytes.fill(0);
    previousDaisySamples.fill(0);
    
    serialBuffer.reserve(BUFFER_SIZE);
    
    // Create source buffer (8 channels default, will resize on Daisy mode)
    sourceBuffers.add(new DataBuffer(CYTON_CHANNELS, 10000));
}

OpenBCICytonThread::~OpenBCICytonThread()
{
    if (isStreaming)
    {
        stopAcquisition();
    }
    disconnect();
}

std::unique_ptr<GenericEditor> OpenBCICytonThread::createEditor(SourceNode* sn)
{
    std::unique_ptr<OpenBCICytonEditor> editor = std::make_unique<OpenBCICytonEditor>(sn, this);
    return editor;
}

bool OpenBCICytonThread::foundInputSource()
{
    return serialConnected;
}

bool OpenBCICytonThread::isReady()
{
    return serialConnected;
}

std::vector<std::string> OpenBCICytonThread::getAvailablePorts()
{
    std::vector<std::string> ports;
    
#ifdef _WIN32
    // Windows: enumerate COM ports
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        char valueName[256];
        char valueData[256];
        DWORD valueNameSize, valueDataSize, valueType;
        DWORD index = 0;
        
        while (true)
        {
            valueNameSize = sizeof(valueName);
            valueDataSize = sizeof(valueData);
            
            if (RegEnumValueA(hKey, index, valueName, &valueNameSize, NULL, &valueType, (LPBYTE)valueData, &valueDataSize) != ERROR_SUCCESS)
                break;
            
            if (valueType == REG_SZ)
            {
                ports.push_back(std::string(valueData));
            }
            index++;
        }
        RegCloseKey(hKey);
    }
#else
    // Unix: enumerate /dev/tty* devices
    DIR* dir = opendir("/dev");
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name = entry->d_name;
            // Look for USB serial devices
            if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0 || name.find("cu.usbserial") == 0)
            {
                ports.push_back("/dev/" + name);
            }
        }
        closedir(dir);
    }
#endif
    
    return ports;
}

bool OpenBCICytonThread::openSerialPort(const std::string& portName)
{
#ifdef _WIN32
    std::string portPath = "\\\\.\\" + portName;
    
    serialHandle = CreateFileA(
        portPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (serialHandle == INVALID_HANDLE_VALUE)
    {
        LOGD("OpenBCICyton: Failed to open port ", portName);
        return false;
    }
    
    // Configure serial port
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    
    if (!GetCommState(serialHandle, &dcb))
    {
        CloseHandle(serialHandle);
        serialHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    dcb.BaudRate = BAUD_RATE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    
    if (!SetCommState(serialHandle, &dcb))
    {
        CloseHandle(serialHandle);
        serialHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    // Set timeouts
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    
    SetCommTimeouts(serialHandle, &timeouts);
    
    // Clear buffers
    PurgeComm(serialHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    
    return true;
    
#else
    serialFd = open(portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    
    if (serialFd < 0)
    {
        LOGD("OpenBCICyton: Failed to open port ", portName);
        return false;
    }
    
    // Configure serial port
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(serialFd, &tty) != 0)
    {
        close(serialFd);
        serialFd = -1;
        return false;
    }
    
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;
    
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;
    
    if (tcsetattr(serialFd, TCSANOW, &tty) != 0)
    {
        close(serialFd);
        serialFd = -1;
        return false;
    }
    
    return true;
#endif
}

void OpenBCICytonThread::closeSerialPort()
{
#ifdef _WIN32
    if (serialHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(serialHandle);
        serialHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (serialFd >= 0)
    {
        close(serialFd);
        serialFd = -1;
    }
#endif
}

int OpenBCICytonThread::readSerial(uint8_t* buffer, int numBytes)
{
#ifdef _WIN32
    if (serialHandle == INVALID_HANDLE_VALUE)
        return -1;
    
    DWORD bytesRead = 0;
    if (!ReadFile(serialHandle, buffer, numBytes, &bytesRead, NULL))
        return -1;
    
    return static_cast<int>(bytesRead);
#else
    if (serialFd < 0)
        return -1;
    
    return static_cast<int>(read(serialFd, buffer, numBytes));
#endif
}

int OpenBCICytonThread::writeSerial(const uint8_t* buffer, int numBytes)
{
#ifdef _WIN32
    if (serialHandle == INVALID_HANDLE_VALUE)
        return -1;
    
    DWORD bytesWritten = 0;
    if (!WriteFile(serialHandle, buffer, numBytes, &bytesWritten, NULL))
        return -1;
    
    return static_cast<int>(bytesWritten);
#else
    if (serialFd < 0)
        return -1;
    
    return static_cast<int>(write(serialFd, buffer, numBytes));
#endif
}

bool OpenBCICytonThread::sendCommand(char cmd)
{
    uint8_t c = static_cast<uint8_t>(cmd);
    return writeSerial(&c, 1) == 1;
}

bool OpenBCICytonThread::sendCommand(const std::string& cmd)
{
    return writeSerial(reinterpret_cast<const uint8_t*>(cmd.c_str()), 
                       static_cast<int>(cmd.length())) == static_cast<int>(cmd.length());
}

std::string OpenBCICytonThread::readResponse(int timeoutMs)
{
    std::string response;
    uint8_t buffer[256];
    
    auto startTime = std::chrono::steady_clock::now();
    
    while (true)
    {
        int bytesRead = readSerial(buffer, sizeof(buffer) - 1);
        
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            response.append(reinterpret_cast<char*>(buffer), bytesRead);
            
            // Check for end of response
            if (response.find("$$$") != std::string::npos)
                break;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        
        if (elapsed > timeoutMs)
            break;
        
        Thread::sleep(10);
    }
    
    return response;
}

bool OpenBCICytonThread::waitForReady(int timeoutMs)
{
    std::string response = readResponse(timeoutMs);
    
    // Parse firmware version if present
    size_t vPos = response.find("v");
    if (vPos != std::string::npos)
    {
        size_t endPos = response.find_first_of("\r\n", vPos);
        if (endPos != std::string::npos)
        {
            firmwareVersion = response.substr(vPos, endPos - vPos);
        }
    }
    
    return response.find("$$$") != std::string::npos;
}

bool OpenBCICytonThread::initializeBoard()
{
    // Reset the board
    if (!resetBoard())
    {
        LOGD("OpenBCICyton: Failed to reset board");
        return false;
    }
    
    // Set Daisy mode if needed
    if (useDaisy)
    {
        // Enable Daisy module
        sendCommand('C');  // Turn on Daisy module
        Thread::sleep(100);
        readResponse(1000);
    }
    else
    {
        // Disable Daisy module (8 channel mode)
        sendCommand('c');  // Turn off Daisy module
        Thread::sleep(100);
        readResponse(1000);
    }
    
    return true;
}

bool OpenBCICytonThread::resetBoard()
{
    // Send soft reset command
    sendCommand('v');
    Thread::sleep(500);
    
    // Wait for ready response
    return waitForReady(5000);
}

bool OpenBCICytonThread::connectToPort(const std::string& portName)
{
    if (serialConnected)
    {
        disconnect();
    }
    
    if (!openSerialPort(portName))
    {
        return false;
    }
    
    currentPort = portName;
    
    // Wait a bit for the connection to stabilize
    Thread::sleep(500);
    
    // Initialize the board
    if (!initializeBoard())
    {
        closeSerialPort();
        currentPort.clear();
        return false;
    }
    
    serialConnected = true;
    LOGD("OpenBCICyton: Connected to ", portName);
    
    return true;
}

void OpenBCICytonThread::disconnect()
{
    if (isStreaming)
    {
        stopAcquisition();
    }
    
    closeSerialPort();
    serialConnected = false;
    currentPort.clear();
    firmwareVersion.clear();
    
    LOGD("OpenBCICyton: Disconnected");
}

void OpenBCICytonThread::setDaisyMode(bool enabled)
{
    useDaisy = enabled;
    
    // Resize buffer for new channel count
    int numChannels = enabled ? MAX_CHANNELS : CYTON_CHANNELS;
    sourceBuffers[0]->resize(numChannels, 10000);
    
    // If connected, reconfigure the board
    if (serialConnected && !isStreaming)
    {
        initializeBoard();
    }
}

void OpenBCICytonThread::setChannelGain(int channel, Gain gain)
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        channelGains[channel] = gain;
        scaleFactors[channel] = getScaleFactor(gain);
    }
}

double OpenBCICytonThread::getScaleFactor(Gain gain)
{
    // Scale Factor (Volts/count) = 4.5 Volts / gain / (2^23 - 1)
    return 4.5 / static_cast<int>(gain) / (8388607.0);
}

int32_t OpenBCICytonThread::interpret24BitAsInt32(const uint8_t* bytes)
{
    // Documentation: https://docs.openbci.com/Cyton/CytonDataFormat/
    // Mask with 0xFF to ensure proper unsigned behavior before shifting
    int32_t value = ((0xFF & bytes[0]) << 16) |
                    ((0xFF & bytes[1]) << 8) |
                    (0xFF & bytes[2]);
    
    // Sign extend if negative (bit 23 set)
    if (value & 0x00800000)
    {
        value |= 0xFF000000;
    }
    
    return value;
}

int16_t OpenBCICytonThread::interpret16BitAsInt16(const uint8_t* bytes)
{
    return static_cast<int16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
}

bool OpenBCICytonThread::parsePacket(const uint8_t* packet)
{
    // Validate header
    if (packet[0] != PACKET_HEADER)
    {
        return false;
    }
    
    // Validate footer
    uint8_t footer = packet[32];
    if ((footer & 0xF0) != PACKET_FOOTER_MASK)
    {
        return false;
    }
    
    // Get sample number
    uint8_t sampleNumber = packet[1];
    
    // Parse 8 channels of EEG data (24-bit signed values)
    for (int ch = 0; ch < CYTON_CHANNELS; ch++)
    {
        int offset = 2 + (ch * 3);
        int32_t rawValue = interpret24BitAsInt32(&packet[offset]);
        
        // Apply scale factor and convert to microvolts
        float scaledValue = static_cast<float>(rawValue * scaleFactors[ch] * 1e6);
        
        if (useDaisy)
        {
            // In Daisy mode, odd samples are from Cyton, even from Daisy
            if (sampleNumber % 2 == 1)
            {
                // Cyton channels (1-8)
                channelData[ch] = scaledValue;
            }
            else
            {
                // Daisy channels (9-16)
                channelData[ch + DAISY_CHANNELS] = scaledValue;
            }
        }
        else
        {
            channelData[ch] = scaledValue;
        }
    }
    
    // Parse aux data based on packet type
    PacketType packetType = static_cast<PacketType>(footer);
    
    switch (packetType)
    {
        case PacketType::STANDARD_ACCEL:
            // Accelerometer data in aux bytes 27-32 (0-indexed: 26-31)
            // Each axis is 16-bit signed, MSB first
            accelData[0] = interpret16BitAsInt16(&packet[26]) * ACCEL_SCALE_FACTOR;  // X
            accelData[1] = interpret16BitAsInt16(&packet[28]) * ACCEL_SCALE_FACTOR;  // Y
            accelData[2] = interpret16BitAsInt16(&packet[30]) * ACCEL_SCALE_FACTOR;  // Z
            break;
            
        case PacketType::TIMESTAMP_ACCEL:
        case PacketType::TIMESTAMP_SET_ACCEL:
            // Byte 27 (index 26) = AC (accelerometer code: 'X', 'x', 'Y', 'y', 'Z', 'z')
            // Byte 28 (index 27) = AV (accelerometer value byte)
            // Bytes 29-32 (index 28-31) = T3-T0 (timestamp, MSB first)
            {
                char accelCode = static_cast<char>(packet[26]);
                uint8_t accelValue = packet[27];
                
                // Parse interleaved accelerometer data
                switch (accelCode)
                {
                    case 'X': accelHighBytes[0] = accelValue; break;
                    case 'x': accelData[0] = static_cast<int16_t>((accelHighBytes[0] << 8) | accelValue) * ACCEL_SCALE_FACTOR; break;
                    case 'Y': accelHighBytes[1] = accelValue; break;
                    case 'y': accelData[1] = static_cast<int16_t>((accelHighBytes[1] << 8) | accelValue) * ACCEL_SCALE_FACTOR; break;
                    case 'Z': accelHighBytes[2] = accelValue; break;
                    case 'z': accelData[2] = static_cast<int16_t>((accelHighBytes[2] << 8) | accelValue) * ACCEL_SCALE_FACTOR; break;
                    default: break;
                }
                
                // Parse 32-bit timestamp (ms since board started)
                boardTimestamp = (static_cast<uint32_t>(packet[28]) << 24) |
                                (static_cast<uint32_t>(packet[29]) << 16) |
                                (static_cast<uint32_t>(packet[30]) << 8) |
                                static_cast<uint32_t>(packet[31]);
            }
            break;
            
        case PacketType::TIMESTAMP_SET_RAW_AUX:
        case PacketType::TIMESTAMP_RAW_AUX:
            // Bytes 27-28 (index 26-27) = UDF (user defined)
            // Bytes 29-32 (index 28-31) = T3-T0 (timestamp)
            boardTimestamp = (static_cast<uint32_t>(packet[28]) << 24) |
                            (static_cast<uint32_t>(packet[29]) << 16) |
                            (static_cast<uint32_t>(packet[30]) << 8) |
                            static_cast<uint32_t>(packet[31]);
            break;
            
        case PacketType::STANDARD_RAW_AUX:
        case PacketType::USER_DEFINED:
            // User defined or raw aux - ignore for now
            break;
            
        default:
            break;
    }
    
    lastSampleNumber = sampleNumber;
    return true;
}

bool OpenBCICytonThread::startAcquisition()
{
    if (!serialConnected)
    {
        LOGD("OpenBCICyton: Cannot start - not connected");
        return false;
    }
    
    // Clear any pending data
    uint8_t tempBuffer[1024];
    while (readSerial(tempBuffer, sizeof(tempBuffer)) > 0) {}
    
    serialBuffer.clear();
    sampleCount = 0;
    streamStartTime = std::chrono::steady_clock::now();
    
    // Send start streaming command
    if (!sendCommand('b'))
    {
        LOGD("OpenBCICyton: Failed to send start command");
        return false;
    }
    
    isStreaming = true;
    LOGD("OpenBCICyton: Started streaming");
    
    return true;
}

bool OpenBCICytonThread::stopAcquisition()
{
    if (!serialConnected || !isStreaming)
    {
        return false;
    }
    
    // Send stop streaming command
    sendCommand('s');
    
    isStreaming = false;
    
    // Clear remaining data
    Thread::sleep(100);
    uint8_t tempBuffer[1024];
    while (readSerial(tempBuffer, sizeof(tempBuffer)) > 0) {}
    
    LOGD("OpenBCICyton: Stopped streaming");
    
    return true;
}

void OpenBCICytonThread::updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                                        OwnedArray<EventChannel>* eventChannels,
                                        OwnedArray<SpikeChannel>* spikeChannels,
                                        OwnedArray<DataStream>* dataStreams,
                                        OwnedArray<DeviceInfo>* devices,
                                        OwnedArray<ConfigurationObject>* configObjects)
{
    // Clear existing settings
    continuousChannels->clear();
    eventChannels->clear();
    spikeChannels->clear();
    dataStreams->clear();
    devices->clear();
    configObjects->clear();
    
    // Create device info
    DeviceInfo::Settings deviceSettings
    {
        "OpenBCI Cyton",
        "OpenBCI",
        "Unknown",
        useDaisy ? "Cyton+Daisy" : "Cyton"
    };
    
    devices->add(new DeviceInfo(deviceSettings));
    
    // Create data stream
    int numChannels = getNumChannels();
    
    DataStream::Settings streamSettings
    {
        "OpenBCI_Cyton",
        "EEG data from OpenBCI Cyton board",
        "openbci.cyton",
        sampleRate
    };
    
    dataStreams->add(new DataStream(streamSettings));
    DataStream* stream = (*dataStreams)[0];
    
    // Create continuous channels
    for (int ch = 0; ch < numChannels; ch++)
    {
        ContinuousChannel::Settings channelSettings
        {
            ContinuousChannel::Type::ELECTRODE,
            "CH" + String(ch + 1),
            "EEG channel " + String(ch + 1),
            "openbci.cyton.ch" + String(ch + 1),
            1.0f,  // bitVolts = 1.0 since we store data in microvolts
            stream
        };
        
        continuousChannels->add(new ContinuousChannel(channelSettings));
    }
    
    // Add accelerometer channels (optional)
    // These could be added as AUX channels if needed
}

bool OpenBCICytonThread::updateBuffer()
{
    if (!serialConnected || !isStreaming)
    {
        return false;
    }
    
    // Read available data
    uint8_t readBuffer[BUFFER_SIZE];
    int bytesRead = readSerial(readBuffer, sizeof(readBuffer));
    
    if (bytesRead > 0)
    {
        // Append to buffer
        serialBuffer.insert(serialBuffer.end(), readBuffer, readBuffer + bytesRead);
    }
    
    // Process complete packets
    while (serialBuffer.size() >= PACKET_SIZE)
    {
        // Find packet header
        size_t headerPos = 0;
        while (headerPos < serialBuffer.size() && serialBuffer[headerPos] != PACKET_HEADER)
        {
            headerPos++;
        }
        
        // Remove any bytes before header
        if (headerPos > 0)
        {
            serialBuffer.erase(serialBuffer.begin(), serialBuffer.begin() + headerPos);
        }
        
        // Check if we have a complete packet
        if (serialBuffer.size() < PACKET_SIZE)
        {
            break;
        }
        
        // Validate footer
        uint8_t footer = serialBuffer[PACKET_SIZE - 1];
        if ((footer & 0xF0) != PACKET_FOOTER_MASK)
        {
            // Invalid packet, skip this byte and continue searching
            serialBuffer.erase(serialBuffer.begin());
            continue;
        }
        
        // Parse the packet
        if (parsePacket(serialBuffer.data()))
        {
            int numChannels = getNumChannels();
            
            // Compute timestamp
            double timestamp = static_cast<double>(sampleCount) / sampleRate;
            uint64 eventCode = 0;  // No events for now
            
            // Add samples to buffer
            if (useDaisy)
            {
                // In Daisy mode:
                // - Sample 0 is invalid (skip)
                // - Odd samples (1,3,5...) contain Cyton (board) data for channels 1-8
                // - Even samples (2,4,6...) contain Daisy data for channels 9-16
                // We push a complete 16-channel sample after receiving Daisy data (even sample)
                if (lastSampleNumber > 0 && lastSampleNumber % 2 == 0)
                {
                    sourceBuffers[0]->addToBuffer(channelData.data(), &sampleCount, &timestamp, &eventCode, 1);
                    sampleCount++;
                }
            }
            else
            {
                // In regular 8-channel mode, send every sample
                sourceBuffers[0]->addToBuffer(channelData.data(), &sampleCount, &timestamp, &eventCode, 1);
                sampleCount++;
            }
        }
        
        // Remove processed packet
        serialBuffer.erase(serialBuffer.begin(), serialBuffer.begin() + PACKET_SIZE);
    }
    
    // Prevent buffer from growing too large
    if (serialBuffer.size() > BUFFER_SIZE * 2)
    {
        serialBuffer.erase(serialBuffer.begin(), serialBuffer.end() - BUFFER_SIZE);
    }
    
    return true;
}

void OpenBCICytonThread::registerParameters()
{
    // Add any custom parameters here if needed
}
