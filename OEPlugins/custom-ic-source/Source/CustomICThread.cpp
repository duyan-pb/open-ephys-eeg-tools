/*
    ------------------------------------------------------------------

    Custom IC Source Plugin for Open Ephys
    
    Implementation of DataThread for custom IC communication.

    ------------------------------------------------------------------
*/

#include "CustomICThread.h"
#include "CustomICEditor.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace CustomIC {

// ============================================================================
// SerialPort Implementation
// ============================================================================

SerialPort::SerialPort()
{
}

SerialPort::~SerialPort()
{
    close();
}

#ifdef _WIN32
// Windows implementation

bool SerialPort::open(const String& portName, int baudRate)
{
    close();
    
    String fullName = portName;
    if (!portName.startsWith("\\\\.\\"))
        fullName = "\\\\.\\" + portName;
    
    handle = CreateFileA(
        fullName.toRawUTF8(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (handle == INVALID_HANDLE_VALUE)
    {
        LOGC("Failed to open serial port: ", portName.toStdString());
        return false;
    }
    
    // Configure port
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    
    if (!GetCommState(handle, &dcb))
    {
        close();
        return false;
    }
    
    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    
    if (!SetCommState(handle, &dcb))
    {
        close();
        return false;
    }
    
    // Set timeouts for non-blocking reads
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    
    SetCommTimeouts(handle, &timeouts);
    
    // Set buffer sizes
    SetupComm(handle, 4096, 4096);
    
    LOGC("Serial port opened: ", portName.toStdString(), " at ", baudRate, " baud");
    return true;
}

void SerialPort::close()
{
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

bool SerialPort::isOpen() const
{
    return handle != INVALID_HANDLE_VALUE;
}

int SerialPort::read(uint8_t* buffer, int maxBytes)
{
    if (!isOpen()) return -1;
    
    DWORD bytesRead = 0;
    if (ReadFile(handle, buffer, maxBytes, &bytesRead, NULL))
        return (int)bytesRead;
    return -1;
}

int SerialPort::write(const uint8_t* data, int numBytes)
{
    if (!isOpen()) return -1;
    
    DWORD bytesWritten = 0;
    if (WriteFile(handle, data, numBytes, &bytesWritten, NULL))
        return (int)bytesWritten;
    return -1;
}

int SerialPort::available()
{
    if (!isOpen()) return 0;
    
    COMSTAT stat;
    DWORD errors;
    if (ClearCommError(handle, &errors, &stat))
        return (int)stat.cbInQue;
    return 0;
}

void SerialPort::flush()
{
    if (isOpen())
        PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

StringArray SerialPort::getAvailablePorts()
{
    StringArray ports;
    
    // Check COM1-COM256
    for (int i = 1; i <= 256; i++)
    {
        String portName = "COM" + String(i);
        String fullName = "\\\\.\\" + portName;
        
        HANDLE h = CreateFileA(
            fullName.toRawUTF8(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (h != INVALID_HANDLE_VALUE)
        {
            ports.add(portName);
            CloseHandle(h);
        }
        else if (GetLastError() == ERROR_ACCESS_DENIED)
        {
            // Port exists but is in use
            ports.add(portName + " (in use)");
        }
    }
    
    return ports;
}

#else
// Linux/macOS implementation

bool SerialPort::open(const String& portName, int baudRate)
{
    close();
    
    fd = ::open(portName.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        LOGC("Failed to open serial port: ", portName.toStdString());
        return false;
    }
    
    struct termios options;
    tcgetattr(fd, &options);
    
    // Set baud rate
    speed_t speed;
    switch (baudRate)
    {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        default:     speed = B115200; break;
    }
    
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    
    // 8N1
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    
    // No flow control
    options.c_cflag &= ~CRTSCTS;
    options.c_cflag |= CREAD | CLOCAL;
    
    // Raw input
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    
    // Non-blocking read
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;
    
    tcsetattr(fd, TCSANOW, &options);
    
    LOGC("Serial port opened: ", portName.toStdString());
    return true;
}

void SerialPort::close()
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

bool SerialPort::isOpen() const
{
    return fd >= 0;
}

int SerialPort::read(uint8_t* buffer, int maxBytes)
{
    if (!isOpen()) return -1;
    return (int)::read(fd, buffer, maxBytes);
}

int SerialPort::write(const uint8_t* data, int numBytes)
{
    if (!isOpen()) return -1;
    return (int)::write(fd, data, numBytes);
}

int SerialPort::available()
{
    if (!isOpen()) return 0;
    int bytes = 0;
    ioctl(fd, FIONREAD, &bytes);
    return bytes;
}

void SerialPort::flush()
{
    if (isOpen())
        tcflush(fd, TCIOFLUSH);
}

StringArray SerialPort::getAvailablePorts()
{
    StringArray ports;
    
#ifdef __APPLE__
    // macOS
    File devDir("/dev");
    Array<File> files = devDir.findChildFiles(File::findFiles, false, "cu.*");
    for (auto& f : files)
        ports.add(f.getFullPathName());
#else
    // Linux
    File devDir("/dev");
    Array<File> files = devDir.findChildFiles(File::findFiles, false, "ttyUSB*");
    for (auto& f : files)
        ports.add(f.getFullPathName());
    
    files = devDir.findChildFiles(File::findFiles, false, "ttyACM*");
    for (auto& f : files)
        ports.add(f.getFullPathName());
#endif
    
    return ports;
}

#endif

// ============================================================================
// ProtocolParser Implementation
// ============================================================================

ProtocolParser::ProtocolParser()
{
    buffer.reserve(MAX_BUFFER_SIZE);
}

void ProtocolParser::configure(int channels, int bytes, float scale)
{
    numChannels = channels;
    bytesPerSample = bytes;
    scaleFactor = scale;
}

void ProtocolParser::setSyncBytes(uint8_t sync1, uint8_t sync2)
{
    syncByte1 = sync1;
    syncByte2 = sync2;
}

int ProtocolParser::getPacketSize() const
{
    // Sync(2) + Data(channels * bytesPerSample) + Checksum(1)
    return 2 + (numChannels * bytesPerSample) + (useChecksum ? 1 : 0);
}

void ProtocolParser::reset()
{
    buffer.clear();
}

std::vector<DataPacket> ProtocolParser::parse(const uint8_t* data, int numBytes)
{
    std::vector<DataPacket> packets;
    
    // Add new data to buffer
    for (int i = 0; i < numBytes; i++)
    {
        buffer.push_back(data[i]);
        if (buffer.size() > MAX_BUFFER_SIZE)
            buffer.erase(buffer.begin());
    }
    
    int packetSize = getPacketSize();
    
    // Search for complete packets
    while (buffer.size() >= (size_t)packetSize)
    {
        // Look for sync bytes
        bool foundSync = false;
        size_t syncPos = 0;
        
        for (size_t i = 0; i <= buffer.size() - packetSize; i++)
        {
            if (buffer[i] == syncByte1 && buffer[i + 1] == syncByte2)
            {
                foundSync = true;
                syncPos = i;
                break;
            }
        }
        
        if (!foundSync)
        {
            // No sync found, keep last few bytes
            if (buffer.size() > 2)
                buffer.erase(buffer.begin(), buffer.end() - 2);
            break;
        }
        
        // Remove bytes before sync
        if (syncPos > 0)
            buffer.erase(buffer.begin(), buffer.begin() + syncPos);
        
        if (buffer.size() < (size_t)packetSize)
            break;
        
        // Validate checksum if enabled
        if (useChecksum && !validateChecksum(buffer.data(), packetSize))
        {
            // Bad checksum, skip this sync and try next
            buffer.erase(buffer.begin());
            continue;
        }
        
        // Parse samples
        DataPacket packet;
        packet.samples.resize(numChannels);
        packet.valid = true;
        packet.timestamp = 0;
        
        const uint8_t* samplePtr = buffer.data() + 2; // Skip sync bytes
        
        for (int ch = 0; ch < numChannels; ch++)
        {
            packet.samples[ch] = bytesToSample(samplePtr, bytesPerSample);
            samplePtr += bytesPerSample;
        }
        
        packets.push_back(packet);
        
        // Remove processed packet
        buffer.erase(buffer.begin(), buffer.begin() + packetSize);
    }
    
    return packets;
}

float ProtocolParser::bytesToSample(const uint8_t* bytes, int numBytes)
{
    int32_t rawValue = 0;
    
    switch (numBytes)
    {
        case 2: // int16 big-endian
            rawValue = (int16_t)((bytes[0] << 8) | bytes[1]);
            break;
            
        case 3: // int24 big-endian (sign-extend)
            rawValue = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
            if (rawValue & 0x800000)
                rawValue |= 0xFF000000; // Sign extend
            break;
            
        case 4: // int32 or float32 big-endian
            rawValue = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
            break;
            
        default:
            return 0.0f;
    }
    
    return rawValue * scaleFactor;
}

bool ProtocolParser::validateChecksum(const uint8_t* packet, int size)
{
    // Simple XOR checksum
    uint8_t checksum = 0;
    for (int i = 0; i < size - 1; i++)
        checksum ^= packet[i];
    
    return checksum == packet[size - 1];
}

} // namespace CustomIC

// ============================================================================
// CustomICThread Implementation
// ============================================================================

DataThread* CustomICThread::createDataThread(SourceNode* sn)
{
    return new CustomICThread(sn);
}

CustomICThread::CustomICThread(SourceNode* sn)
    : DataThread(sn)
{
    serial = std::make_unique<CustomIC::SerialPort>();
    parser = std::make_unique<CustomIC::ProtocolParser>();
    readBuffer.resize(READ_BUFFER_SIZE);
    
    // Allocate buffers
    sourceBuffers.add(new DataBuffer(numChannels, 100000));
    
    dataBuffer = (float*) malloc(numChannels * bufferSize * sizeof(float));
    timestampBuffer = (double*) malloc(bufferSize * sizeof(double));
    sampleNumbers = (int64*) malloc(bufferSize * sizeof(int64));
    ttlEventWords = (uint64*) malloc(bufferSize * sizeof(uint64));
    
    // Initialize TTL buffer
    for (int i = 0; i < bufferSize; i++)
        ttlEventWords[i] = 0;
    
    // Default configuration (8 channels @ 256 Hz, 16-bit samples)
    parser->configure(numChannels, bytesPerSample, scaleFactor);
}

CustomICThread::~CustomICThread()
{
    disconnect();
    
    free(dataBuffer);
    free(timestampBuffer);
    free(sampleNumbers);
    free(ttlEventWords);
}

void CustomICThread::registerParameters()
{
    // Simulation mode
    addBooleanParameter(Parameter::PROCESSOR_SCOPE, "simulate", "Simulate", 
        "Enable simulation mode (no hardware required)", false);
    
    // Channel configuration
    addIntParameter(Parameter::PROCESSOR_SCOPE, "channels", "Channels",
        "Number of data channels", 8, 1, 256);
    
    addIntParameter(Parameter::PROCESSOR_SCOPE, "sample_rate", "Sample Rate",
        "Sample rate in Hz", 256, 1, 100000);
    
    // Serial port configuration
    addStringParameter(Parameter::PROCESSOR_SCOPE, "port", "Port",
        "Serial port name (e.g., COM3)", "");
    
    Array<String> baudRates = {"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"};
    addCategoricalParameter(Parameter::PROCESSOR_SCOPE, "baud_rate", "Baud Rate",
        "Serial communication baud rate", baudRates, 4); // Default: 115200
    
    // Data format configuration
    Array<String> dataFormats = {"int16", "int24", "int32"};
    addCategoricalParameter(Parameter::PROCESSOR_SCOPE, "data_format", "Data Format",
        "Sample data format", dataFormats, 0); // Default: int16
    
    addFloatParameter(Parameter::PROCESSOR_SCOPE, "scale_factor", "Scale Factor",
        "Scale factor to convert to microvolts", "uV/LSB", 0.195f, 0.0001f, 1000.0f, 0.001f);
    
    // Sync bytes (stored as hex strings)
    addStringParameter(Parameter::PROCESSOR_SCOPE, "sync_byte_1", "Sync Byte 1",
        "First sync byte (hex)", "A0");
    
    addStringParameter(Parameter::PROCESSOR_SCOPE, "sync_byte_2", "Sync Byte 2",
        "Second sync byte (hex)", "5A");
}

void CustomICThread::parameterValueChanged(Parameter* param)
{
    if (param->getName() == "simulate")
    {
        simulationMode = (bool)param->getValue();
        if (simulationMode)
            connected = true;
    }
    else if (param->getName() == "channels")
    {
        numChannels = (int)param->getValue();
        parser->configure(numChannels, bytesPerSample, scaleFactor);
        CoreServices::updateSignalChain(sn->getEditor());
    }
    else if (param->getName() == "sample_rate")
    {
        sampleRate = (float)(int)param->getValue();
        CoreServices::updateSignalChain(sn->getEditor());
    }
    else if (param->getName() == "port")
    {
        portName = param->getValue().toString();
    }
    else if (param->getName() == "baud_rate")
    {
        String baudStr = ((CategoricalParameter*)param)->getValueAsString();
        baudRate = baudStr.getIntValue();
    }
    else if (param->getName() == "data_format")
    {
        int formatIdx = (int)param->getValue();
        bytesPerSample = formatIdx + 2; // 0=int16(2), 1=int24(3), 2=int32(4)
        parser->configure(numChannels, bytesPerSample, scaleFactor);
    }
    else if (param->getName() == "scale_factor")
    {
        scaleFactor = (float)param->getValue();
        parser->configure(numChannels, bytesPerSample, scaleFactor);
    }
    else if (param->getName() == "sync_byte_1")
    {
        String hexStr = param->getValue().toString();
        uint8_t sync1 = (uint8_t)hexStr.getHexValue32();
        uint8_t sync2 = (uint8_t)getParameter("sync_byte_2")->getValue().toString().getHexValue32();
        parser->setSyncBytes(sync1, sync2);
    }
    else if (param->getName() == "sync_byte_2")
    {
        String hexStr = param->getValue().toString();
        uint8_t sync1 = (uint8_t)getParameter("sync_byte_1")->getValue().toString().getHexValue32();
        uint8_t sync2 = (uint8_t)hexStr.getHexValue32();
        parser->setSyncBytes(sync1, sync2);
    }
}

std::unique_ptr<GenericEditor> CustomICThread::createEditor(SourceNode* sn)
{
    std::unique_ptr<CustomICEditor> editor = std::make_unique<CustomICEditor>(sn, this);
    return editor;
}

void CustomICThread::setPort(const String& port)
{
    portName = port;
    if (hasParameter("port"))
        getParameter("port")->setNextValue(port);
}

void CustomICThread::setBaudRate(int rate)
{
    baudRate = rate;
}

void CustomICThread::setNumChannels(int num)
{
    numChannels = num;
    parser->configure(numChannels, bytesPerSample, scaleFactor);
    if (hasParameter("channels"))
        getParameter("channels")->setNextValue(num);
}

void CustomICThread::setSampleRate(float rate)
{
    sampleRate = rate;
    if (hasParameter("sample_rate"))
        getParameter("sample_rate")->setNextValue((int)rate);
}

void CustomICThread::setDataFormat(int bytes)
{
    bytesPerSample = bytes;
    parser->configure(numChannels, bytesPerSample, scaleFactor);
}

void CustomICThread::setScaleFactor(float scale)
{
    scaleFactor = scale;
    parser->configure(numChannels, bytesPerSample, scaleFactor);
}

void CustomICThread::setSyncBytes(uint8_t sync1, uint8_t sync2)
{
    parser->setSyncBytes(sync1, sync2);
}

void CustomICThread::setSimulationMode(bool simulate)
{
    simulationMode = simulate;
    if (simulationMode)
        connected = true;
}

StringArray CustomICThread::getAvailablePorts() const
{
    return CustomIC::SerialPort::getAvailablePorts();
}

bool CustomICThread::isConnected() const
{
    return connected.load();
}

bool CustomICThread::connect()
{
    if (simulationMode)
    {
        connected = true;
        LOGC("Custom IC connected (simulation mode)");
        return true;
    }
    
    if (portName.isEmpty())
    {
        LOGC("No port selected");
        return false;
    }
    
    if (!serial->open(portName, baudRate))
    {
        LOGC("Failed to open port: ", portName.toStdString());
        return false;
    }
    
    serial->flush();
    parser->reset();
    connected = true;
    
    LOGC("Custom IC connected on ", portName.toStdString());
    return true;
}

void CustomICThread::disconnect()
{
    connected = false;
    
    if (serial && serial->isOpen())
        serial->close();
    
    if (parser)
        parser->reset();
}

bool CustomICThread::foundInputSource()
{
    return simulationMode || connected.load();
}

bool CustomICThread::startAcquisition()
{
    if (!connected)
    {
        if (!connect())
            return false;
    }
    
    totalSamples = 0;
    initialTimestamp = -1.0;
    simPhase = 0.0;
    
    // Resize buffers
    sourceBuffers[0]->resize(numChannels, 100000);
    
    if (auto newBuffer = (float*) realloc(dataBuffer, numChannels * bufferSize * sizeof(float)))
        dataBuffer = newBuffer;
    
    if (serial && serial->isOpen())
        serial->flush();
    
    if (parser)
        parser->reset();
    
    startThread();
    return true;
}

bool CustomICThread::stopAcquisition()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
    }
    
    if (MessageManager::getInstance()->isThisTheMessageThread())
    {
        stopThread(500);
    }
    
    sourceBuffers[0]->clear();
    return true;
}

void CustomICThread::updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                                     OwnedArray<EventChannel>* eventChannels,
                                     OwnedArray<SpikeChannel>* spikeChannels,
                                     OwnedArray<DataStream>* sourceStreams,
                                     OwnedArray<DeviceInfo>* devices,
                                     OwnedArray<ConfigurationObject>* configurationObjects)
{
    continuousChannels->clear();
    eventChannels->clear();
    devices->clear();
    spikeChannels->clear();
    configurationObjects->clear();
    sourceStreams->clear();
    
    // Create a data stream
    DataStream::Settings streamSettings {
        "Custom IC",
        "Custom IC data stream",
        "custom-ic-source",
        sampleRate
    };
    sourceStreams->add(new DataStream(streamSettings));
    
    // Create channels
    for (int ch = 0; ch < numChannels; ch++)
    {
        ContinuousChannel::Settings channelSettings {
            ContinuousChannel::Type::ELECTRODE,
            "CH" + String(ch + 1),
            "Custom IC channel " + String(ch + 1),
            "custom-ic-ch" + String(ch + 1),
            scaleFactor,
            sourceStreams->getFirst()
        };
        
        continuousChannels->add(new ContinuousChannel(channelSettings));
    }
    
    // Create event channel
    EventChannel::Settings eventSettings {
        EventChannel::Type::TTL,
        "Custom IC Events",
        "TTL events from custom IC",
        "custom-ic-events",
        sourceStreams->getFirst(),
        8
    };
    
    eventChannels->add(new EventChannel(eventSettings));
}

bool CustomICThread::updateBuffer()
{
    if (simulationMode)
    {
        generateSimulatedData();
        return true;
    }
    
    // Read from serial port
    if (!serial || !serial->isOpen())
        return false;
    
    int bytesRead = serial->read(readBuffer.data(), READ_BUFFER_SIZE);
    
    if (bytesRead > 0)
    {
        // Parse data packets
        auto packets = parser->parse(readBuffer.data(), bytesRead);
        
        int numPackets = (int)packets.size();
        if (numPackets > 0)
        {
            // Prepare buffers
            for (int i = 0; i < numPackets; i++)
            {
                const auto& packet = packets[i];
                if (!packet.valid)
                    continue;
                
                // Copy samples to channel-major buffer
                for (int ch = 0; ch < numChannels; ch++)
                {
                    dataBuffer[ch * numPackets + i] = packet.samples[ch];
                }
                
                sampleNumbers[i] = totalSamples + i;
                
                // Calculate timestamp
                double timestamp = (double)(totalSamples + i) / sampleRate;
                if (initialTimestamp < 0)
                    initialTimestamp = timestamp;
                timestampBuffer[i] = timestamp - initialTimestamp;
                
                ttlEventWords[i] = 0;
            }
            
            // Add to buffer
            sourceBuffers[0]->addToBuffer(
                dataBuffer,
                sampleNumbers,
                timestampBuffer,
                ttlEventWords,
                numPackets);
            
            totalSamples += numPackets;
        }
    }
    
    // Small sleep to avoid busy-waiting
    sleep(1);
    
    return true;
}

void CustomICThread::generateSimulatedData()
{
    // Generate simulated neural-like data
    const int samplesPerUpdate = (int)(sampleRate / 100.0); // ~10ms of data
    const double dt = 1.0 / sampleRate;
    
    for (int s = 0; s < samplesPerUpdate; s++)
    {
        for (int ch = 0; ch < numChannels; ch++)
        {
            // Mix of frequencies to simulate EEG
            double alpha = 50.0 * std::sin(2.0 * M_PI * 10.0 * simPhase);  // 10 Hz alpha
            double beta = 20.0 * std::sin(2.0 * M_PI * 20.0 * simPhase);   // 20 Hz beta
            double noise = (rand() % 100 - 50) * 0.2;                       // Random noise
            
            // Phase offset per channel
            double phaseOffset = ch * 0.1;
            dataBuffer[ch * samplesPerUpdate + s] = (float)(alpha * std::cos(phaseOffset) + 
                                  beta * std::sin(phaseOffset) + 
                                  noise);
        }
        
        sampleNumbers[s] = totalSamples + s;
        
        double timestamp = (double)(totalSamples + s) / sampleRate;
        if (initialTimestamp < 0)
            initialTimestamp = timestamp;
        timestampBuffer[s] = timestamp - initialTimestamp;
        
        ttlEventWords[s] = 0;
        
        simPhase += dt;
    }
    
    // Add to buffer
    sourceBuffers[0]->addToBuffer(
        dataBuffer,
        sampleNumbers,
        timestampBuffer,
        ttlEventWords,
        samplesPerUpdate);
    
    totalSamples += samplesPerUpdate;
    
    // Sleep to match real-time
    sleep(10);
}
