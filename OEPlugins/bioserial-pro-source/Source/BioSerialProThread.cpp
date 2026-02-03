/*
    ------------------------------------------------------------------

    BioSerial-Pro Source Plugin for Open Ephys
    
    Implementation of DataThread for BioSerial-Pro protocol.

    ------------------------------------------------------------------
*/

#include "BioSerialProThread.h"
#include "BioSerialProEditor.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BioSerialPro {

// ============================================================================
// SerialPort Implementation
// ============================================================================

SerialPort::SerialPort() {}

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
        LOGC("BioSerialPro: Failed to open port: ", portName.toStdString());
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
    
    // Set large buffer sizes for high throughput
    SetupComm(handle, 32768, 4096);
    
    LOGC("BioSerialPro: Port opened: ", portName.toStdString(), " @ ", baudRate, " baud");
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
        LOGC("BioSerialPro: Failed to open port: ", portName.toStdString());
        return false;
    }
    
    struct termios options;
    tcgetattr(fd, &options);
    
    speed_t speed;
    switch (baudRate)
    {
        case 9600:    speed = B9600;    break;
        case 19200:   speed = B19200;   break;
        case 38400:   speed = B38400;   break;
        case 57600:   speed = B57600;   break;
        case 115200:  speed = B115200;  break;
        case 230400:  speed = B230400;  break;
        case 460800:  speed = B460800;  break;
        case 921600:  speed = B921600;  break;
        default:      speed = B115200;  break;
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
    
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;
    
    tcsetattr(fd, TCSANOW, &options);
    
    LOGC("BioSerialPro: Port opened: ", portName.toStdString());
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
    File devDir("/dev");
    Array<File> files = devDir.findChildFiles(File::findFiles, false, "cu.usbmodem*");
    for (auto& f : files)
        ports.add(f.getFullPathName());
    files = devDir.findChildFiles(File::findFiles, false, "cu.usbserial*");
    for (auto& f : files)
        ports.add(f.getFullPathName());
#else
    File devDir("/dev");
    Array<File> files = devDir.findChildFiles(File::findFiles, false, "ttyACM*");
    for (auto& f : files)
        ports.add(f.getFullPathName());
    files = devDir.findChildFiles(File::findFiles, false, "ttyUSB*");
    for (auto& f : files)
        ports.add(f.getFullPathName());
#endif
    
    return ports;
}

#endif

// ============================================================================
// ReassemblyBuffer Implementation
// ============================================================================

ReassemblyBuffer::ReassemblyBuffer(size_t capacity)
    : maxCapacity(capacity)
{
}

void ReassemblyBuffer::push(const uint8_t* data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        buffer.push_back(data[i]);
    }
    
    // Trim if too large
    while (buffer.size() > maxCapacity)
    {
        buffer.pop_front();
    }
}

int ReassemblyBuffer::findSyncPosition() const
{
    if (buffer.size() < PACKET_SIZE)
        return -1;
    
    for (size_t i = 0; i <= buffer.size() - PACKET_SIZE; i++)
    {
        if (buffer[i] == HEADER_BYTE_1 && buffer[i + 1] == HEADER_BYTE_2)
        {
            // Also check footer
            if (buffer[i + OFFSET_FOOTER] == FOOTER_BYTE_1 &&
                buffer[i + OFFSET_FOOTER + 1] == FOOTER_BYTE_2)
            {
                return (int)i;
            }
        }
    }
    
    return -1;
}

uint8_t ReassemblyBuffer::computeChecksum(const uint8_t* data, int length)
{
    uint8_t checksum = 0;
    for (int i = 0; i < length; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

bool ReassemblyBuffer::validatePacket(const uint8_t* data) const
{
    // Verify header
    if (data[0] != HEADER_BYTE_1 || data[1] != HEADER_BYTE_2)
        return false;
    
    // Verify footer
    if (data[OFFSET_FOOTER] != FOOTER_BYTE_1 || data[OFFSET_FOOTER + 1] != FOOTER_BYTE_2)
        return false;
    
    // Verify checksum (XOR of bytes 0-28)
    uint8_t expectedChecksum = computeChecksum(data, OFFSET_CHECKSUM);
    if (data[OFFSET_CHECKSUM] != expectedChecksum)
        return false;
    
    return true;
}

bool ReassemblyBuffer::tryExtractPacket(uint8_t* packet)
{
    int syncPos = findSyncPosition();
    
    if (syncPos < 0)
    {
        // No valid packet found, trim buffer keeping last byte
        while (buffer.size() > 1)
            buffer.pop_front();
        return false;
    }
    
    // Remove any bytes before sync
    for (int i = 0; i < syncPos; i++)
        buffer.pop_front();
    
    if (buffer.size() < PACKET_SIZE)
        return false;
    
    // Extract packet
    for (int i = 0; i < PACKET_SIZE; i++)
    {
        packet[i] = buffer[i];
    }
    
    // Validate
    if (!validatePacket(packet))
    {
        // Bad packet, skip first byte and try again
        buffer.pop_front();
        return false;
    }
    
    // Remove packet from buffer
    for (int i = 0; i < PACKET_SIZE; i++)
        buffer.pop_front();
    
    return true;
}

void ReassemblyBuffer::clear()
{
    buffer.clear();
}

size_t ReassemblyBuffer::bytesAvailable() const
{
    return buffer.size();
}

// ============================================================================
// ProtocolParser Implementation
// ============================================================================

ProtocolParser::ProtocolParser()
    : reassemblyBuffer(8192)
{
}

void ProtocolParser::reset()
{
    reassemblyBuffer.clear();
    packetsReceived = 0;
    checksumErrors = 0;
    framingErrors = 0;
    droppedPackets = 0;
    firstPacket = true;
}

int32_t ProtocolParser::bytes24ToInt32(const uint8_t* bytes)
{
    // Big-endian 24-bit to signed 32-bit
    int32_t value = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
    
    // Sign extend
    if (value & 0x800000)
        value |= 0xFF000000;
    
    return value;
}

int16_t ProtocolParser::bytes16ToInt16(const uint8_t* bytes)
{
    // Big-endian 16-bit to signed 16-bit
    return (int16_t)((bytes[0] << 8) | bytes[1]);
}

int64_t ProtocolParser::bytes48ToInt64(const uint8_t* bytes)
{
    // Big-endian 48-bit to signed 64-bit
    int64_t value = ((int64_t)bytes[0] << 40) |
                    ((int64_t)bytes[1] << 32) |
                    ((int64_t)bytes[2] << 24) |
                    ((int64_t)bytes[3] << 16) |
                    ((int64_t)bytes[4] << 8) |
                    bytes[5];
    
    // Sign extend from 48-bit
    if (value & 0x800000000000LL)
        value |= 0xFFFF000000000000LL;
    
    return value;
}

EEGSample ProtocolParser::parsePacket(const uint8_t* packet)
{
    EEGSample sample;
    sample.valid = true;
    
    // Timestamp (4 bytes, big-endian)
    sample.timestamp_us = ((uint32_t)packet[OFFSET_TIMESTAMP] << 24) |
                          ((uint32_t)packet[OFFSET_TIMESTAMP + 1] << 16) |
                          ((uint32_t)packet[OFFSET_TIMESTAMP + 2] << 8) |
                          packet[OFFSET_TIMESTAMP + 3];
    
    // Marker
    sample.marker = packet[OFFSET_MARKER];
    
    // EEG channels (5 × 24-bit)
    for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++)
    {
        int32_t raw = bytes24ToInt32(&packet[OFFSET_EEG + ch * 3]);
        sample.eeg[ch] = raw * EEG_SCALE_UV;  // Convert to µV
    }
    
    // Counter
    sample.counter = packet[OFFSET_COUNTER];
    
    // Aux channels - parse each type separately
    int auxIdx = 0;
    
    // Accelerometer (3 × 16-bit)
    for (int i = 0; i < 3; i++)
    {
        int16_t raw = bytes16ToInt16(&packet[OFFSET_ACCEL + i * 2]);
        sample.aux[auxIdx++] = (float)raw;
    }
    
    // PPG (3 × 48-bit)
    for (int i = 0; i < 3; i++)
    {
        int64_t raw = bytes48ToInt64(&packet[OFFSET_PPG + i * 6]);
        sample.aux[auxIdx++] = (float)raw;
    }
    
    // Temperature (1 × 16-bit)
    sample.aux[auxIdx++] = (float)bytes16ToInt16(&packet[OFFSET_TEMP]);
    
    // Battery (1 × 16-bit)
    sample.aux[auxIdx++] = (float)bytes16ToInt16(&packet[OFFSET_BATTERY]);
    
    // Sync (1 × 16-bit)
    sample.aux[auxIdx++] = (float)bytes16ToInt16(&packet[OFFSET_SYNC]);
    
    return sample;
}

std::vector<EEGSample> ProtocolParser::parse(const uint8_t* data, int numBytes)
{
    std::vector<EEGSample> samples;
    
    // Add incoming data to reassembly buffer
    reassemblyBuffer.push(data, numBytes);
    
    // Extract as many complete packets as possible
    uint8_t packet[PACKET_SIZE];
    
    while (reassemblyBuffer.tryExtractPacket(packet))
    {
        EEGSample sample = parsePacket(packet);
        
        // Check for dropped packets
        if (!firstPacket)
        {
            uint8_t expectedCounter = (lastCounter + 1) & 0xFF;
            if (sample.counter != expectedCounter)
            {
                int dropped = (sample.counter - expectedCounter) & 0xFF;
                droppedPackets += dropped;
                LOGC("BioSerialPro: Dropped ", dropped, " packets");
            }
        }
        
        firstPacket = false;
        lastCounter = sample.counter;
        packetsReceived++;
        
        samples.push_back(sample);
    }
    
    return samples;
}

} // namespace BioSerialPro

// ============================================================================
// BioSerialProThread Implementation
// ============================================================================

DataThread* BioSerialProThread::createDataThread(SourceNode* sn)
{
    return new BioSerialProThread(sn);
}

BioSerialProThread::BioSerialProThread(SourceNode* sn)
    : DataThread(sn)
{
    serial = std::make_unique<BioSerialPro::SerialPort>();
    parser = std::make_unique<BioSerialPro::ProtocolParser>();
    readBuffer.resize(READ_BUFFER_SIZE);
    
    // Create source buffers:
    // - Stream 0: EEG (5 channels @ 1000 Hz)
    // - Stream 1: Aux (3 channels @ 200 Hz - decimated)
    sourceBuffers.add(new DataBuffer(BioSerialPro::NUM_EEG_CHANNELS, 100000));
    sourceBuffers.add(new DataBuffer(BioSerialPro::NUM_AUX_CHANNELS, 20000));
    
    // Allocate working buffers
    eegBuffer = (float*)malloc(BioSerialPro::NUM_EEG_CHANNELS * bufferSize * sizeof(float));
    auxBuffer = (float*)malloc(BioSerialPro::NUM_AUX_CHANNELS * bufferSize * sizeof(float));
    timestampBuffer = (double*)malloc(bufferSize * sizeof(double));
    sampleNumbers = (int64*)malloc(bufferSize * sizeof(int64));
    eventWords = (uint64*)malloc(bufferSize * sizeof(uint64));
    
    // Initialize event buffer
    for (int i = 0; i < bufferSize; i++)
        eventWords[i] = 0;
}

BioSerialProThread::~BioSerialProThread()
{
    disconnect();
    
    free(eegBuffer);
    free(auxBuffer);
    free(timestampBuffer);
    free(sampleNumbers);
    free(eventWords);
}

std::unique_ptr<GenericEditor> BioSerialProThread::createEditor(SourceNode* sn)
{
    std::unique_ptr<GenericEditor> editor = std::make_unique<BioSerialProEditor>(sn, this);
    return editor;
}

void BioSerialProThread::registerParameters()
{
    // Port selection
    StringArray ports = getAvailablePorts();
    if (ports.size() == 0)
        ports.add("None");
    
    // Convert StringArray to Array<String>
    Array<String> portArray;
    for (const auto& port : ports)
        portArray.add(port);
    
    addCategoricalParameter(Parameter::GLOBAL_SCOPE,
                           "port",
                           "Serial Port",
                           "Select the COM port for the Teensy device",
                           portArray,
                           0);
    
    // Simulation mode
    addBooleanParameter(Parameter::GLOBAL_SCOPE,
                       "simulation",
                       "Simulation Mode",
                       "Generate simulated data without hardware",
                       false);
}

void BioSerialProThread::parameterValueChanged(Parameter* param)
{
    if (param->getName() == "port")
    {
        CategoricalParameter* p = (CategoricalParameter*)param;
        String newPort = p->getValueAsString();
        if (newPort != "None" && !newPort.contains("(in use)"))
        {
            setPort(newPort);
        }
    }
    else if (param->getName() == "simulation")
    {
        BooleanParameter* p = (BooleanParameter*)param;
        setSimulationMode(p->getBoolValue());
    }
}

void BioSerialProThread::updateSettings(
    OwnedArray<ContinuousChannel>* continuousChannels,
    OwnedArray<EventChannel>* eventChannels,
    OwnedArray<SpikeChannel>* spikeChannels,
    OwnedArray<DataStream>* sourceStreams,
    OwnedArray<DeviceInfo>* devices,
    OwnedArray<ConfigurationObject>* configurationObjects)
{
    // Clear existing
    continuousChannels->clear();
    eventChannels->clear();
    spikeChannels->clear();
    sourceStreams->clear();
    devices->clear();
    configurationObjects->clear();
    
    // Create device info
    DeviceInfo::Settings deviceSettings;
    deviceSettings.name = "BioSerial-Pro";
    deviceSettings.description = "Teensy + ADS1299 EEG Acquisition";
    deviceSettings.identifier = "bioserial-pro";
    deviceSettings.manufacturer = "Open Ephys";
    deviceSettings.serial_number = "0001";
    
    DeviceInfo* device = new DeviceInfo(deviceSettings);
    devices->add(device);
    
    // ========== EEG Data Stream ==========
    DataStream::Settings eegStreamSettings;
    eegStreamSettings.name = "EEG";
    eegStreamSettings.description = "BioSerial-Pro EEG data";
    eegStreamSettings.identifier = "bioserial.eeg";
    eegStreamSettings.sample_rate = (float)BioSerialPro::EEG_SAMPLE_RATE;
    eegStreamSettings.generates_timestamps = true;
    
    DataStream* eegStream = new DataStream(eegStreamSettings);
    sourceStreams->add(eegStream);
    
    // Add EEG channels
    for (int ch = 0; ch < BioSerialPro::NUM_EEG_CHANNELS; ch++)
    {
        ContinuousChannel::Settings chanSettings;
        chanSettings.type = ContinuousChannel::ELECTRODE;
        chanSettings.name = "EEG" + String(ch + 1);
        chanSettings.description = "EEG channel " + String(ch + 1);
        chanSettings.identifier = "bioserial.eeg." + String(ch);
        chanSettings.bitVolts = 0.195f;  // µV per bit after scaling
        chanSettings.stream = eegStream;
        
        ContinuousChannel* channel = new ContinuousChannel(chanSettings);
        continuousChannels->add(channel);
    }
    
    // ========== Aux Data Stream (9 channels @ 1kHz) ==========
    DataStream::Settings auxStreamSettings;
    auxStreamSettings.name = "Aux";
    auxStreamSettings.description = "BioSerial-Pro Auxiliary Channels";
    auxStreamSettings.identifier = "bioserial.aux";
    auxStreamSettings.sample_rate = (float)BioSerialPro::AUX_SAMPLE_RATE;  // 1kHz
    auxStreamSettings.generates_timestamps = true;
    
    DataStream* auxStream = new DataStream(auxStreamSettings);
    sourceStreams->add(auxStream);
    
    // Add Aux channels - 9 channels total
    String auxNames[] = {"AccelX", "AccelY", "AccelZ", "PPG_Red", "PPG_IR", "PPG_Green", "Temperature", "Battery", "Sync"};
    String auxDescriptions[] = {
        "Accelerometer X axis",
        "Accelerometer Y axis", 
        "Accelerometer Z axis",
        "PPG Red LED",
        "PPG Infrared LED",
        "PPG Green LED",
        "Temperature (centi-degrees C)",
        "Battery voltage (mV)",
        "Sync signal"
    };
    for (int ch = 0; ch < BioSerialPro::NUM_AUX_CHANNELS; ch++)
    {
        ContinuousChannel::Settings chanSettings;
        chanSettings.type = ContinuousChannel::AUX;
        chanSettings.name = auxNames[ch];
        chanSettings.description = auxDescriptions[ch];
        chanSettings.identifier = "bioserial.aux." + String(ch);
        chanSettings.bitVolts = 1.0f;
        chanSettings.stream = auxStream;
        
        ContinuousChannel* channel = new ContinuousChannel(chanSettings);
        continuousChannels->add(channel);
    }
    
    // ========== Event Channel for Markers ==========
    EventChannel::Settings eventSettings;
    eventSettings.type = EventChannel::TTL;
    eventSettings.name = "Markers";
    eventSettings.description = "Hardware event markers";
    eventSettings.identifier = "bioserial.markers";
    eventSettings.stream = eegStream;
    eventSettings.maxTTLBits = 8;
    
    EventChannel* markerChannel = new EventChannel(eventSettings);
    eventChannels->add(markerChannel);
}

bool BioSerialProThread::foundInputSource()
{
    if (simulationMode)
        return true;
    
    return connected.load();
}

bool BioSerialProThread::startAcquisition()
{
    parser->reset();
    totalSamples = 0;
    initialTimestamp = -1.0;
    accelDecimationCounter = 0;
    
    if (simulationMode)
    {
        simPhase = 0.0;
        simStartTime = std::chrono::high_resolution_clock::now();
        LOGC("BioSerialPro: Starting acquisition (SIMULATION MODE)");
        startThread();  // Start the DataThread run() loop
        return true;
    }
    
    if (!connected.load())
    {
        if (!connect())
        {
            LOGC("BioSerialPro: Failed to connect");
            return false;
        }
    }
    
    // Flush any stale data
    serial->flush();
    
    LOGC("BioSerialPro: Starting acquisition on ", portName.toStdString());
    startThread();  // Start the DataThread run() loop
    return true;
}

bool BioSerialProThread::stopAcquisition()
{
    LOGC("BioSerialPro: Stopping acquisition");
    
    // Stop the DataThread run() loop
    stopThread(1000);
    
    LOGC("  Packets received: ", parser->getPacketsReceived());
    LOGC("  Dropped packets: ", parser->getDroppedPackets());
    LOGC("  Checksum errors: ", parser->getChecksumErrors());
    
    return true;
}

bool BioSerialProThread::updateBuffer()
{
    static int callCount = 0;
    if (callCount++ % 500 == 0)
    {
        LOGC("BioSerialPro: updateBuffer called, count=", callCount, ", simMode=", simulationMode);
    }
    
    if (simulationMode)
    {
        generateSimulatedData();
        return true;
    }
    
    if (!serial->isOpen())
    {
        LOGC("BioSerialPro: Serial port not open!");
        return false;
    }
    
    // Read available data from serial port
    int bytesRead = serial->read(readBuffer.data(), READ_BUFFER_SIZE);
    
    if (callCount % 500 == 1)
    {
        LOGC("BioSerialPro: read() returned ", bytesRead, " bytes");
    }
    
    if (bytesRead <= 0)
        return true;  // No data available, but not an error
    
    // Parse packets
    auto samples = parser->parse(readBuffer.data(), bytesRead);
    
    if (samples.empty())
    {
        static int emptyCounter = 0;
        if (emptyCounter++ % 100 == 0)
        {
            LOGC("BioSerialPro: parser returned 0 samples from ", bytesRead, " bytes");
        }
        return true;
    }
    
    static int logCounter = 0;
    if (logCounter++ % 1000 == 0)
    {
        LOGC("BioSerialPro: Got ", samples.size(), " samples, bytesRead=", bytesRead, ", EEG[0]=", samples[0].eeg[0]);
    }
    
    // Process each sample
    int eegSampleCount = 0;
    int auxSampleCount = 0;
    
    for (const auto& sample : samples)
    {
        if (!sample.valid)
            continue;
        
        // Set initial timestamp
        if (initialTimestamp < 0)
            initialTimestamp = Time::getMillisecondCounterHiRes();
        
        // Calculate timestamp for this sample
        double ts = initialTimestamp + (totalSamples * 1000.0 / BioSerialPro::EEG_SAMPLE_RATE);
        
        // Store EEG data
        for (int ch = 0; ch < BioSerialPro::NUM_EEG_CHANNELS; ch++)
        {
            eegBuffer[eegSampleCount * BioSerialPro::NUM_EEG_CHANNELS + ch] = sample.eeg[ch];
        }
        
        timestampBuffer[eegSampleCount] = ts;
        sampleNumbers[eegSampleCount] = totalSamples;
        eventWords[eegSampleCount] = sample.marker;
        
        eegSampleCount++;
        totalSamples++;
        
        // Aux data (6 channels @ 1kHz - no decimation)
        for (int ch = 0; ch < BioSerialPro::NUM_AUX_CHANNELS; ch++)
        {
            auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + ch] = sample.aux[ch];
        }
        auxSampleCount++;
        
        // Batch write when buffer is getting full
        if (eegSampleCount >= bufferSize - 1)
        {
            // Write EEG data to source buffer
            sourceBuffers[0]->addToBuffer(
                eegBuffer,
                sampleNumbers,
                timestampBuffer,
                eventWords,
                eegSampleCount
            );
            eegSampleCount = 0;
        }
        
        if (auxSampleCount >= bufferSize - 1)
        {
            // Write aux data to source buffer (use zero event codes)
            static uint64 zeroEvents[2048] = {0};
            sourceBuffers[1]->addToBuffer(
                auxBuffer,
                sampleNumbers,
                timestampBuffer,
                zeroEvents,
                auxSampleCount
            );
            auxSampleCount = 0;
        }
    }
    
    // Write remaining samples
    if (eegSampleCount > 0)
    {
        sourceBuffers[0]->addToBuffer(
            eegBuffer,
            sampleNumbers,
            timestampBuffer,
            eventWords,
            eegSampleCount
        );
    }
    
    if (auxSampleCount > 0)
    {
        static uint64 zeroEvents[2048] = {0};
        sourceBuffers[1]->addToBuffer(
            auxBuffer,
            sampleNumbers,
            timestampBuffer,
            zeroEvents,
            auxSampleCount
        );
    }
    
    return true;
}

void BioSerialProThread::generateSimulatedData()
{
    // Calculate how many samples we should have generated by now
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - simStartTime);
    int64 targetSamples = (elapsed.count() * BioSerialPro::EEG_SAMPLE_RATE) / 1000000;
    
    int samplesToGenerate = (int)(targetSamples - totalSamples);
    if (samplesToGenerate <= 0)
        return;
    
    if (samplesToGenerate > bufferSize)
        samplesToGenerate = bufferSize;
    
    double ts = initialTimestamp < 0 ? Time::getMillisecondCounterHiRes() : initialTimestamp;
    if (initialTimestamp < 0)
        initialTimestamp = ts;
    
    int auxSampleCount = 0;
    
    for (int i = 0; i < samplesToGenerate; i++)
    {
        // Generate EEG: Different frequency sine waves per channel
        for (int ch = 0; ch < BioSerialPro::NUM_EEG_CHANNELS; ch++)
        {
            double freq = 3.0 + ch * 4.0;  // 3, 7, 11, 15, 19 Hz
            double amplitude = 100.0 * (1.0 + ch * 0.2);  // 100-180 µV
            eegBuffer[i * BioSerialPro::NUM_EEG_CHANNELS + ch] = 
                (float)(amplitude * sin(2.0 * M_PI * freq * simPhase / BioSerialPro::EEG_SAMPLE_RATE));
        }
        
        timestampBuffer[i] = ts + (totalSamples + i) * 1000.0 / BioSerialPro::EEG_SAMPLE_RATE;
        sampleNumbers[i] = totalSamples + i;
        eventWords[i] = 0;
        
        // Aux data: 9 channels matching protocol order
        // [0] AccelX: 0.5 Hz sway
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 0] = 
            1000.0f * sin(2.0 * M_PI * 0.5 * simPhase / BioSerialPro::EEG_SAMPLE_RATE);
        
        // [1] AccelY: 0.7 Hz
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 1] = 
            1000.0f * sin(2.0 * M_PI * 0.7 * simPhase / BioSerialPro::EEG_SAMPLE_RATE);
        
        // [2] AccelZ: 0.3 Hz + gravity
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 2] = 
            16384.0f + 500.0f * sin(2.0 * M_PI * 0.3 * simPhase / BioSerialPro::EEG_SAMPLE_RATE);
        
        // [3] PPG_Red: 1.2 Hz (~72 BPM heartbeat)
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 3] = 
            100000.0f + 5000.0f * sin(2.0 * M_PI * 1.2 * simPhase / BioSerialPro::EEG_SAMPLE_RATE);
        
        // [4] PPG_IR: 1.2 Hz with different offset
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 4] = 
            120000.0f + 6000.0f * sin(2.0 * M_PI * 1.2 * simPhase / BioSerialPro::EEG_SAMPLE_RATE);
        
        // [5] PPG_Green: 1.2 Hz with different offset
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 5] = 
            80000.0f + 4000.0f * sin(2.0 * M_PI * 1.2 * simPhase / BioSerialPro::EEG_SAMPLE_RATE);
        
        // [6] Temperature: 0.05 Hz around 36.5°C (in centi-degrees)
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 6] = 
            3650.0f + 50.0f * sin(2.0 * M_PI * 0.05 * simPhase / BioSerialPro::EEG_SAMPLE_RATE);
        
        // [7] Battery: 0.02 Hz around 4000mV
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 7] = 
            4000.0f + 200.0f * sin(2.0 * M_PI * 0.02 * simPhase / BioSerialPro::EEG_SAMPLE_RATE);
        
        // [8] Sync: slow pulse
        auxBuffer[auxSampleCount * BioSerialPro::NUM_AUX_CHANNELS + 8] = 
            (fmod(simPhase, 1000.0) < 100) ? 1.0f : 0.0f;
        
        auxSampleCount++;
        simPhase += 1.0;
    }
    
    // Write to source buffers
    sourceBuffers[0]->addToBuffer(
        eegBuffer,
        sampleNumbers,
        timestampBuffer,
        eventWords,
        samplesToGenerate
    );
    
    if (auxSampleCount > 0)
    {
        static uint64 zeroEvents[2048] = {0};
        sourceBuffers[1]->addToBuffer(
            auxBuffer,
            sampleNumbers,
            timestampBuffer,
            zeroEvents,
            auxSampleCount
        );
    }
    
    totalSamples += samplesToGenerate;
}

void BioSerialProThread::setPort(const String& port)
{
    if (port != portName)
    {
        disconnect();
        portName = port;
    }
}

void BioSerialProThread::setBaudRate(int rate)
{
    baudRate = rate;
}

void BioSerialProThread::setSimulationMode(bool simulate)
{
    simulationMode = simulate;
    if (simulate)
    {
        disconnect();
    }
}

bool BioSerialProThread::isConnected() const
{
    return connected.load();
}

StringArray BioSerialProThread::getAvailablePorts() const
{
    return BioSerialPro::SerialPort::getAvailablePorts();
}

bool BioSerialProThread::connect()
{
    if (portName.isEmpty())
    {
        LOGC("BioSerialPro: No port specified");
        return false;
    }
    
    if (serial->open(portName, baudRate))
    {
        connected = true;
        return true;
    }
    
    return false;
}

void BioSerialProThread::disconnect()
{
    connected = false;
    serial->close();
}
