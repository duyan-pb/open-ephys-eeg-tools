/*
    ------------------------------------------------------------------

    InEar Teensy Optimized Source Plugin for Open Ephys
    
    DataThread implementation.

    ------------------------------------------------------------------
*/

#include "InEarTeensyOptimizedThread.h"
#include "InEarTeensyOptimizedEditor.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// Serial Port Implementation
// =============================================================================

OptimizedSerialPort::OptimizedSerialPort()
{
}

OptimizedSerialPort::~OptimizedSerialPort()
{
    close();
}

#ifdef _WIN32

bool OptimizedSerialPort::open(const String& portName, int baudRate)
{
    String fullPath = "\\\\.\\" + portName;
    
    handle = CreateFileA(
        fullPath.toStdString().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (handle == INVALID_HANDLE_VALUE)
    {
        LOGD("OptimizedSerial: Failed to open ", portName);
        return false;
    }
    
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
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    
    if (!SetCommState(handle, &dcb))
    {
        close();
        return false;
    }
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    
    SetCommTimeouts(handle, &timeouts);
    
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    
    LOGD("OptimizedSerial: Opened ", portName, " at ", baudRate, " baud");
    return true;
}

void OptimizedSerialPort::close()
{
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

bool OptimizedSerialPort::isOpen() const
{
    return handle != INVALID_HANDLE_VALUE;
}

int OptimizedSerialPort::read(uint8_t* buffer, int maxBytes)
{
    if (!isOpen()) return -1;
    
    DWORD bytesRead = 0;
    if (ReadFile(handle, buffer, maxBytes, &bytesRead, NULL))
    {
        return static_cast<int>(bytesRead);
    }
    return -1;
}

int OptimizedSerialPort::write(const uint8_t* data, int numBytes)
{
    if (!isOpen()) return -1;
    
    DWORD bytesWritten = 0;
    if (WriteFile(handle, data, numBytes, &bytesWritten, NULL))
    {
        return static_cast<int>(bytesWritten);
    }
    return -1;
}

int OptimizedSerialPort::available()
{
    if (!isOpen()) return 0;
    
    COMSTAT status;
    DWORD errors;
    if (ClearCommError(handle, &errors, &status))
    {
        return static_cast<int>(status.cbInQue);
    }
    return 0;
}

void OptimizedSerialPort::flush()
{
    if (isOpen())
    {
        PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    }
}

StringArray OptimizedSerialPort::getAvailablePorts()
{
    StringArray ports;
    
    for (int i = 1; i <= 256; i++)
    {
        String portName = "COM" + String(i);
        String fullPath = "\\\\.\\" + portName;
        
        HANDLE hTest = CreateFileA(
            fullPath.toStdString().c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hTest != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hTest);
            ports.add(portName);
        }
        else if (GetLastError() == ERROR_ACCESS_DENIED)
        {
            ports.add(portName + " (in use)");
        }
    }
    
    return ports;
}

#else // Linux/macOS

bool OptimizedSerialPort::open(const String& portName, int baudRate)
{
    fd = ::open(portName.toStdString().c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    
    if (fd < 0)
    {
        LOGD("OptimizedSerial: Failed to open ", portName);
        return false;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0)
    {
        close();
        return false;
    }
    
    speed_t speed;
    switch (baudRate)
    {
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 921600: speed = B921600; break;
        case 1000000: speed = B1000000; break;
        case 2000000: speed = B2000000; break;
        default: speed = B115200;
    }
    
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;
    
    tcsetattr(fd, TCSANOW, &tty);
    tcflush(fd, TCIOFLUSH);
    
    LOGD("OptimizedSerial: Opened ", portName, " at ", baudRate, " baud");
    return true;
}

void OptimizedSerialPort::close()
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

bool OptimizedSerialPort::isOpen() const
{
    return fd >= 0;
}

int OptimizedSerialPort::read(uint8_t* buffer, int maxBytes)
{
    if (!isOpen()) return -1;
    return ::read(fd, buffer, maxBytes);
}

int OptimizedSerialPort::write(const uint8_t* data, int numBytes)
{
    if (!isOpen()) return -1;
    return ::write(fd, data, numBytes);
}

int OptimizedSerialPort::available()
{
    if (!isOpen()) return 0;
    int bytes = 0;
    ioctl(fd, FIONREAD, &bytes);
    return bytes;
}

void OptimizedSerialPort::flush()
{
    if (isOpen())
    {
        tcflush(fd, TCIOFLUSH);
    }
}

StringArray OptimizedSerialPort::getAvailablePorts()
{
    StringArray ports;
    
#ifdef __APPLE__
    const char* patterns[] = {"/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/tty.usbmodem*"};
#else
    const char* patterns[] = {"/dev/ttyUSB*", "/dev/ttyACM*"};
#endif
    
    for (const char* pattern : patterns)
    {
        glob_t glob_result;
        if (glob(pattern, GLOB_TILDE, nullptr, &glob_result) == 0)
        {
            for (size_t i = 0; i < glob_result.gl_pathc; i++)
            {
                ports.add(String(glob_result.gl_pathv[i]));
            }
            globfree(&glob_result);
        }
    }
    
    return ports;
}

#endif

// =============================================================================
// Protocol Parser Implementation
// =============================================================================

OptimizedProtocolParser::OptimizedProtocolParser()
{
    reset();
}

void OptimizedProtocolParser::reset()
{
    buffer.clear();
    accelState = SensorState(NUM_ACCEL_CHANNELS);
    ppgState = SensorState(NUM_PPG_CHANNELS);
    healthState = SensorState(2);
    lastSequence = 0;
    firstPacket = true;
    currentSampleNum = 0;
    packetsReceived = 0;
    packetsDropped = 0;
    checksumErrors = 0;
    framingErrors = 0;
    bytesReceived = 0;
}

int OptimizedProtocolParser::findSyncPosition()
{
    for (size_t i = 0; i < buffer.size() - 1; i++)
    {
        if (buffer[i] == SYNC_BYTE_1 && buffer[i + 1] == SYNC_BYTE_2)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int32_t OptimizedProtocolParser::bytes24ToInt32(const uint8_t* bytes)
{
    int32_t value = ((0xFF & bytes[0]) << 16) | 
                    ((0xFF & bytes[1]) << 8) | 
                    (0xFF & bytes[2]);
    // Sign extend
    if (value & 0x800000)
    {
        value |= 0xFF000000;
    }
    return value;
}

int16_t OptimizedProtocolParser::bytes16ToInt16(const uint8_t* bytes)
{
    return static_cast<int16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
}

int64_t OptimizedProtocolParser::bytes48ToInt64(const uint8_t* bytes)
{
    int64_t value = 0;
    for (int i = 0; i < 6; i++)
    {
        value = (value << 8) | bytes[i];
    }
    // Sign extend from 48 bits
    if (value & 0x800000000000LL)
    {
        value |= 0xFFFF000000000000LL;
    }
    return value;
}

uint32_t OptimizedProtocolParser::bytes32ToUint32(const uint8_t* bytes)
{
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
}

bool OptimizedProtocolParser::parsePacketData(const uint8_t* packet, int size, OptimizedSample& sample)
{
    // Parse header
    sample.packetType = static_cast<PacketType>(packet[2]);
    sample.sequence = packet[3];
    sample.timestamp_us = bytes32ToUint32(&packet[4]);
    
    // Initialize flags
    sample.hasAccelData = hasAccel(sample.packetType);
    sample.hasPPGData = hasPPG(sample.packetType);
    sample.hasHealthData = hasHealth(sample.packetType);
    sample.hasMarkerData = hasMarker(sample.packetType);
    
    // Current position in packet (after header)
    int pos = SIZE_HEADER;
    
    // Parse EEG (always present)
    for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++)
    {
        int32_t raw = bytes24ToInt32(&packet[pos]);
        sample.eeg[ch] = raw * EEG_SCALE_UV;
        pos += 3;
    }
    
    // Parse marker if present
    if (sample.hasMarkerData)
    {
        sample.marker = packet[pos++];
    }
    else
    {
        sample.marker = 0;
    }
    
    // Parse accelerometer if present
    if (sample.hasAccelData)
    {
        float accelVals[3];
        for (int ch = 0; ch < NUM_ACCEL_CHANNELS; ch++)
        {
            int16_t raw = bytes16ToInt16(&packet[pos]);
            accelVals[ch] = raw * ACCEL_SCALE_G;
            sample.accel[ch] = accelVals[ch];
            pos += 2;
        }
        // Update state
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        accelState.update(accelVals, currentSampleNum, now);
    }
    else
    {
        // Use last known values
        for (int ch = 0; ch < NUM_ACCEL_CHANNELS; ch++)
        {
            sample.accel[ch] = accelState.values[ch];
        }
    }
    sample.accelAge = accelState.getAge(currentSampleNum);
    
    // Parse PPG if present
    if (sample.hasPPGData)
    {
        float ppgVals[3];
        for (int ch = 0; ch < NUM_PPG_CHANNELS; ch++)
        {
            int64_t raw = bytes48ToInt64(&packet[pos]);
            ppgVals[ch] = static_cast<float>(raw) * PPG_SCALE;
            sample.ppg[ch] = ppgVals[ch];
            pos += 6;
        }
        // Update state
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        ppgState.update(ppgVals, currentSampleNum, now);
    }
    else
    {
        // Use last known values
        for (int ch = 0; ch < NUM_PPG_CHANNELS; ch++)
        {
            sample.ppg[ch] = ppgState.values[ch];
        }
    }
    sample.ppgAge = ppgState.getAge(currentSampleNum);
    
    // Parse health data if present
    if (sample.hasHealthData)
    {
        int16_t rawTemp = bytes16ToInt16(&packet[pos]);
        sample.temperature = rawTemp * TEMP_SCALE;
        pos += 2;
        
        int16_t rawBattery = bytes16ToInt16(&packet[pos]);
        sample.battery = rawBattery * BATTERY_SCALE;
        pos += 2;
        
        // Update state
        float healthVals[2] = {sample.temperature, sample.battery};
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        healthState.update(healthVals, currentSampleNum, now);
    }
    else
    {
        // Use last known values
        sample.temperature = healthState.values[0];
        sample.battery = healthState.values[1];
    }
    sample.healthAge = healthState.getAge(currentSampleNum);
    
    sample.valid = true;
    return true;
}

bool OptimizedProtocolParser::tryParsePacket(OptimizedSample& sample)
{
    if (buffer.size() < MIN_PACKET_SIZE)
        return false;
    
    // Check sync bytes
    if (buffer[0] != SYNC_BYTE_1 || buffer[1] != SYNC_BYTE_2)
    {
        framingErrors++;
        buffer.pop_front();
        return false;
    }
    
    // Get packet type and expected size
    PacketType pktType = static_cast<PacketType>(buffer[2]);
    int expectedSize = getPacketSize(pktType);
    
    if (expectedSize < 0)
    {
        // Invalid packet type
        framingErrors++;
        buffer.pop_front();
        buffer.pop_front();
        return false;
    }
    
    if (buffer.size() < static_cast<size_t>(expectedSize))
    {
        // Need more data
        return false;
    }
    
    // Copy packet data
    std::vector<uint8_t> packet(expectedSize);
    for (int i = 0; i < expectedSize; i++)
    {
        packet[i] = buffer[i];
    }
    
    // Verify footer
    if (packet[expectedSize - 2] != FOOTER_BYTE_1 || 
        packet[expectedSize - 1] != FOOTER_BYTE_2)
    {
        framingErrors++;
        buffer.pop_front();
        buffer.pop_front();
        return false;
    }
    
    // Verify checksum (XOR of all bytes except last 3)
    uint8_t expectedChecksum = packet[expectedSize - 3];
    uint8_t computedChecksum = computeChecksum(packet.data(), expectedSize - 3);
    
    if (expectedChecksum != computedChecksum)
    {
        checksumErrors++;
        buffer.pop_front();
        buffer.pop_front();
        return false;
    }
    
    // Check sequence number for gaps
    uint8_t seq = packet[3];
    if (!firstPacket)
    {
        uint8_t expectedSeq = (lastSequence + 1) & 0xFF;
        if (seq != expectedSeq)
        {
            int dropped = (seq - expectedSeq + 256) % 256;
            packetsDropped += dropped;
            LOGD("OptimizedParser: Dropped ", dropped, " packets (seq ", 
                 (int)lastSequence, " -> ", (int)seq, ")");
        }
    }
    firstPacket = false;
    lastSequence = seq;
    currentSampleNum++;
    
    // Parse packet content
    if (!parsePacketData(packet.data(), expectedSize, sample))
    {
        framingErrors++;
        for (int i = 0; i < expectedSize; i++)
            buffer.pop_front();
        return false;
    }
    
    // Remove parsed packet from buffer
    for (int i = 0; i < expectedSize; i++)
        buffer.pop_front();
    
    packetsReceived++;
    return true;
}

std::vector<OptimizedSample> OptimizedProtocolParser::parse(const uint8_t* data, int numBytes)
{
    std::vector<OptimizedSample> samples;
    
    // Add new data to buffer
    for (int i = 0; i < numBytes; i++)
    {
        buffer.push_back(data[i]);
    }
    bytesReceived += numBytes;
    
    // Limit buffer size to prevent memory issues
    while (buffer.size() > 16384)
    {
        buffer.pop_front();
        framingErrors++;
    }
    
    // Try to parse packets
    OptimizedSample sample;
    while (buffer.size() >= MIN_PACKET_SIZE)
    {
        // Find sync position
        int syncPos = findSyncPosition();
        
        if (syncPos < 0)
        {
            // No sync found, keep last byte in case it's start of sync
            while (buffer.size() > 1)
                buffer.pop_front();
            break;
        }
        
        if (syncPos > 0)
        {
            // Discard bytes before sync
            for (int i = 0; i < syncPos; i++)
                buffer.pop_front();
            framingErrors++;
        }
        
        // Try to parse packet at current position
        if (tryParsePacket(sample))
        {
            samples.push_back(sample);
        }
    }
    
    return samples;
}

// =============================================================================
// DataThread Implementation
// =============================================================================

InEarTeensyOptimizedThread::InEarTeensyOptimizedThread(SourceNode* sn)
    : DataThread(sn),
      serial(std::make_unique<OptimizedSerialPort>())
{
    sampleBuffer.resize(TOTAL_CHANNELS * 256);
    
    // Create source buffers - MUST match the number of DataStreams created in updateSettings
    // Stream 0: EEG (5 channels @ 1000 Hz)
    // Stream 1: Aux (9 channels @ 1000 Hz)
    sourceBuffers.add(new DataBuffer(NUM_EEG_CHANNELS, 100000));
    sourceBuffers.add(new DataBuffer(NUM_AUX_CHANNELS, 100000));
}

InEarTeensyOptimizedThread::~InEarTeensyOptimizedThread()
{
    disconnect();
}

std::unique_ptr<GenericEditor> InEarTeensyOptimizedThread::createEditor(SourceNode* sn)
{
    std::unique_ptr<InEarTeensyOptimizedEditor> editor = 
        std::make_unique<InEarTeensyOptimizedEditor>(sn, this);
    return editor;
}

void InEarTeensyOptimizedThread::registerParameters()
{
}

bool InEarTeensyOptimizedThread::foundInputSource()
{
    return serialConnected || simulationMode;
}

bool InEarTeensyOptimizedThread::isReady()
{
    return serialConnected || simulationMode;
}

StringArray InEarTeensyOptimizedThread::getAvailablePorts()
{
    return OptimizedSerialPort::getAvailablePorts();
}

bool InEarTeensyOptimizedThread::connect()
{
    if (selectedPort.isEmpty())
    {
        LOGD("OptimizedThread: No port selected");
        return false;
    }
    
    if (serial->open(selectedPort, BAUD_RATE))
    {
        serialConnected = true;
        parser.reset();
        LOGD("OptimizedThread: Connected to ", selectedPort);
        return true;
    }
    
    return false;
}

void InEarTeensyOptimizedThread::disconnect()
{
    if (serialConnected)
    {
        serial->close();
        serialConnected = false;
        LOGD("OptimizedThread: Disconnected");
    }
}

void InEarTeensyOptimizedThread::setSimulationMode(bool enable)
{
    simulationMode = enable;
    if (enable)
    {
        simPhase = 0.0;
        simSampleNum = 0;
    }
}

void InEarTeensyOptimizedThread::updateSettings(
    OwnedArray<ContinuousChannel>* continuousChannels,
    OwnedArray<EventChannel>* eventChannels,
    OwnedArray<SpikeChannel>* spikeChannels,
    OwnedArray<DataStream>* dataStreams,
    OwnedArray<DeviceInfo>* devices,
    OwnedArray<ConfigurationObject>* configObjects)
{
    // Clear existing (like the original plugin)
    continuousChannels->clear();
    eventChannels->clear();
    spikeChannels->clear();
    dataStreams->clear();
    devices->clear();
    configObjects->clear();
    
    // Create device info
    DeviceInfo::Settings deviceSettings;
    deviceSettings.name = "InEar Teensy Optimized";
    deviceSettings.description = "Teensy 4.0 + ADS1299 (Optimized Protocol)";
    deviceSettings.identifier = "InEar Teensy Opt";
    deviceSettings.manufacturer = "Open Ephys";
    deviceSettings.serial_number = "0001";
    
    DeviceInfo* device = new DeviceInfo(deviceSettings);
    devices->add(device);
    
    // ========== EEG Data Stream ==========
    DataStream::Settings eegStreamSettings;
    eegStreamSettings.name = "EEG";
    eegStreamSettings.description = "InEar Teensy Optimized EEG data";
    eegStreamSettings.identifier = "bioserial.opt.eeg";
    eegStreamSettings.sample_rate = static_cast<float>(EEG_SAMPLE_RATE);
    eegStreamSettings.generates_timestamps = true;
    
    DataStream* eegStream = new DataStream(eegStreamSettings);
    dataStreams->add(eegStream);
    
    // Add EEG channels (5 channels)
    for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++)
    {
        ContinuousChannel::Settings chanSettings;
        chanSettings.type = ContinuousChannel::ELECTRODE;
        chanSettings.name = "EEG" + String(ch + 1);
        chanSettings.description = "EEG channel " + String(ch + 1);
        chanSettings.identifier = "bioserial.opt.eeg." + String(ch);
        chanSettings.bitVolts = 0.195f;  // µV per bit
        chanSettings.stream = eegStream;
        
        ContinuousChannel* channel = new ContinuousChannel(chanSettings);
        continuousChannels->add(channel);
    }
    
    // ========== Aux Data Stream (9 channels @ 1kHz) ==========
    DataStream::Settings auxStreamSettings;
    auxStreamSettings.name = "Aux";
    auxStreamSettings.description = "InEar Teensy Optimized Auxiliary Channels";
    auxStreamSettings.identifier = "bioserial.opt.aux";
    auxStreamSettings.sample_rate = static_cast<float>(EEG_SAMPLE_RATE);  // 1kHz
    auxStreamSettings.generates_timestamps = true;
    
    DataStream* auxStream = new DataStream(auxStreamSettings);
    dataStreams->add(auxStream);
    
    // Add Aux channels - 9 channels total (matching original)
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
    
    for (int ch = 0; ch < NUM_AUX_CHANNELS; ch++)
    {
        ContinuousChannel::Settings chanSettings;
        chanSettings.type = ContinuousChannel::AUX;
        chanSettings.name = auxNames[ch];
        chanSettings.description = auxDescriptions[ch];
        chanSettings.identifier = "bioserial.opt.aux." + String(ch);
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
    eventSettings.identifier = "bioserial.opt.markers";
    eventSettings.stream = eegStream;
    eventSettings.maxTTLBits = 8;
    
    EventChannel* markerChannel = new EventChannel(eventSettings);
    eventChannels->add(markerChannel);
}

bool InEarTeensyOptimizedThread::startAcquisition()
{
    if (!serialConnected && !simulationMode)
    {
        LOGC("OptimizedThread: Cannot start - not connected and not in simulation mode");
        return false;
    }
    
    // Clear serial buffer
    if (serialConnected)
    {
        uint8_t tempBuffer[4096];
        while (serial->read(tempBuffer, sizeof(tempBuffer)) > 0) {}
    }
    
    parser.reset();
    sampleCount = 0;
    simPhase = 0.0;
    streamStartTime = std::chrono::steady_clock::now();
    
    // Reset bandwidth monitoring
    bandwidthStartTime = std::chrono::steady_clock::now();
    acquisitionStartTime = bandwidthStartTime;
    bandwidthBytes = 0;
    totalBytesReceived = 0;
    
    LOGC("OptimizedThread: Starting acquisition, simMode=", simulationMode.load(), 
         ", serialConnected=", serialConnected);
    LOGC("OptimizedThread: Bandwidth monitoring enabled - logging every 5 seconds");
    
    // Start the DataThread run() loop - THIS IS ESSENTIAL!
    startThread();
    
    return true;
}

bool InEarTeensyOptimizedThread::stopAcquisition()
{
    LOGC("OptimizedThread: Stopping acquisition...");
    
    // Stop the DataThread run() loop
    stopThread(1000);
    
    // Calculate total acquisition time
    auto now = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(now - acquisitionStartTime).count();
    double avgBandwidthKBps = (totalDuration > 0) ? (totalBytesReceived / 1024.0 / totalDuration) : 0.0;
    
    LOGC("OptimizedThread: ======== ACQUISITION SUMMARY ========");
    LOGC("  Duration: ", totalDuration, " seconds");
    LOGC("  Total bytes received: ", totalBytesReceived, " (", totalBytesReceived / 1024.0, " KB)");
    LOGC("  Average bandwidth: ", avgBandwidthKBps, " KB/s");
    LOGC("  Packets received: ", parser.getPacketsReceived());
    LOGC("  Packets dropped: ", parser.getPacketsDropped());
    LOGC("  Checksum errors: ", parser.getChecksumErrors());
    LOGC("OptimizedThread: =====================================");
    return true;
}

void InEarTeensyOptimizedThread::generateSimulatedData(float* buffer, int numSamples)
{
    for (int s = 0; s < numSamples; s++)
    {
        int offset = s * TOTAL_CHANNELS;
        
        // EEG: Alpha waves (10 Hz) + noise (channels 0-4)
        for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++)
        {
            double alpha = 50.0 * std::sin(2.0 * M_PI * 10.0 * simPhase + ch * 0.5);
            double noise = (std::rand() / (double)RAND_MAX - 0.5) * 20.0;
            buffer[offset + ch] = static_cast<float>(alpha + noise);
        }
        
        // AUX channels (9 total, starting at index 5):
        // 0-2: AccelX, AccelY, AccelZ
        // 3-5: PPG_Red, PPG_IR, PPG_Green  
        // 6: Temperature
        // 7: Battery
        // 8: Sync
        
        // Accelerometer (channels 5-7 = aux 0-2)
        buffer[offset + 5] = static_cast<float>(0.1 * std::sin(2.0 * M_PI * 1.0 * simPhase));
        buffer[offset + 6] = static_cast<float>(0.1 * std::cos(2.0 * M_PI * 1.0 * simPhase));
        buffer[offset + 7] = static_cast<float>(1.0 + 0.05 * std::sin(2.0 * M_PI * 0.5 * simPhase));
        
        // PPG (channels 8-10 = aux 3-5)
        double heartRate = 1.2;  // ~72 BPM
        buffer[offset + 8] = static_cast<float>(10000.0 + 500.0 * std::sin(2.0 * M_PI * heartRate * simPhase));
        buffer[offset + 9] = static_cast<float>(12000.0 + 600.0 * std::sin(2.0 * M_PI * heartRate * simPhase));
        buffer[offset + 10] = static_cast<float>(8000.0 + 400.0 * std::sin(2.0 * M_PI * heartRate * simPhase));
        
        // Temperature (channel 11 = aux 6) - in centi-degrees C
        buffer[offset + 11] = 3650.0f + 10.0f * std::sin(2.0 * M_PI * 0.01 * simPhase);  // 36.50°C
        
        // Battery (channel 12 = aux 7) - in mV
        buffer[offset + 12] = 4200.0f - 0.1f * (simSampleNum / 1000.0f);  // Slowly draining
        
        // Sync (channel 13 = aux 8)
        buffer[offset + 13] = (simSampleNum % 1000 < 100) ? 1.0f : 0.0f;  // Pulse every second
        
        simPhase += 1.0 / EEG_SAMPLE_RATE;
        simSampleNum++;
    }
}

int InEarTeensyOptimizedThread::readFromSerial(uint8_t* buffer, int maxBytes)
{
    if (!serialConnected || !serial->isOpen())
        return 0;
    
    return serial->read(buffer, maxBytes);
}

bool InEarTeensyOptimizedThread::updateBuffer()
{
    if (simulationMode)
    {
        // Calculate how many samples we should have generated by now
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - streamStartTime);
        int64_t targetSamples = (elapsed.count() * EEG_SAMPLE_RATE) / 1000000;
        
        int samplesToGenerate = static_cast<int>(targetSamples - sampleCount);
        if (samplesToGenerate <= 0)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            return true;
        }
        
        // Limit to reasonable batch size
        if (samplesToGenerate > 100)
            samplesToGenerate = 100;
        
        // Allocate buffers for batch processing
        std::vector<float> eegBuffer(NUM_EEG_CHANNELS * samplesToGenerate);
        std::vector<float> auxBuffer(NUM_AUX_CHANNELS * samplesToGenerate);
        std::vector<int64_t> sampleNumbers(samplesToGenerate);
        std::vector<double> timestamps(samplesToGenerate);
        std::vector<uint64> eventCodes(samplesToGenerate, 0);
        
        // Generate all samples
        for (int s = 0; s < samplesToGenerate; s++)
        {
            // EEG: Alpha waves with different frequencies per channel
            for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++)
            {
                double freq = 3.0 + ch * 4.0;  // 3, 7, 11, 15, 19 Hz
                double amplitude = 100.0 * (1.0 + ch * 0.2);  // 100-180 µV
                eegBuffer[s * NUM_EEG_CHANNELS + ch] = 
                    static_cast<float>(amplitude * std::sin(2.0 * M_PI * freq * simPhase / EEG_SAMPLE_RATE));
            }
            
            // Aux channels
            auxBuffer[s * NUM_AUX_CHANNELS + 0] = 1000.0f * std::sin(2.0 * M_PI * 0.5 * simPhase / EEG_SAMPLE_RATE);  // AccelX
            auxBuffer[s * NUM_AUX_CHANNELS + 1] = 1000.0f * std::sin(2.0 * M_PI * 0.7 * simPhase / EEG_SAMPLE_RATE);  // AccelY
            auxBuffer[s * NUM_AUX_CHANNELS + 2] = 16384.0f + 500.0f * std::sin(2.0 * M_PI * 0.3 * simPhase / EEG_SAMPLE_RATE);  // AccelZ
            auxBuffer[s * NUM_AUX_CHANNELS + 3] = 100000.0f + 5000.0f * std::sin(2.0 * M_PI * 1.2 * simPhase / EEG_SAMPLE_RATE);  // PPG_Red
            auxBuffer[s * NUM_AUX_CHANNELS + 4] = 120000.0f + 6000.0f * std::sin(2.0 * M_PI * 1.2 * simPhase / EEG_SAMPLE_RATE);  // PPG_IR
            auxBuffer[s * NUM_AUX_CHANNELS + 5] = 80000.0f + 4000.0f * std::sin(2.0 * M_PI * 1.2 * simPhase / EEG_SAMPLE_RATE);  // PPG_Green
            auxBuffer[s * NUM_AUX_CHANNELS + 6] = 3650.0f + 50.0f * std::sin(2.0 * M_PI * 0.05 * simPhase / EEG_SAMPLE_RATE);  // Temp
            auxBuffer[s * NUM_AUX_CHANNELS + 7] = 4000.0f + 200.0f * std::sin(2.0 * M_PI * 0.02 * simPhase / EEG_SAMPLE_RATE);  // Battery
            auxBuffer[s * NUM_AUX_CHANNELS + 8] = (std::fmod(simPhase, 1000.0) < 100) ? 1.0f : 0.0f;  // Sync
            
            sampleNumbers[s] = static_cast<int64_t>(sampleCount + s);
            timestamps[s] = static_cast<double>(sampleCount + s);
            
            simPhase += 1.0;
        }
        
        // Write to source buffers in batch
        sourceBuffers[0]->addToBuffer(
            eegBuffer.data(),
            sampleNumbers.data(),
            timestamps.data(),
            eventCodes.data(),
            samplesToGenerate
        );
        
        sourceBuffers[1]->addToBuffer(
            auxBuffer.data(),
            sampleNumbers.data(),
            timestamps.data(),
            eventCodes.data(),
            samplesToGenerate
        );
        
        sampleCount += samplesToGenerate;
        return true;
    }
    
    // Read from serial
    uint8_t readBuffer[4096];
    int bytesRead = readFromSerial(readBuffer, sizeof(readBuffer));
    
    static int logCounter = 0;
    if (logCounter++ % 500 == 0)
    {
        LOGC("OptimizedThread: updateBuffer called, bytesRead=", bytesRead, 
             ", serialConnected=", serialConnected, ", simulationMode=", simulationMode.load());
    }
    
    if (bytesRead <= 0)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        return true;
    }
    
    // Parse packets
    auto samples = parser.parse(readBuffer, bytesRead);
    
    if (logCounter % 500 == 1)
    {
        LOGC("OptimizedThread: Parsed ", samples.size(), " samples from ", bytesRead, " bytes");
        LOGC("  Parser stats: received=", parser.getPacketsReceived(), 
             ", dropped=", parser.getPacketsDropped(), 
             ", checksumErrors=", parser.getChecksumErrors());
    }
    
    // Add parsed samples to buffer
    for (const auto& sample : samples)
    {
        if (!sample.valid)
            continue;
        
        // Build EEG data array (5 channels)
        float eegData[NUM_EEG_CHANNELS];
        for (int ch = 0; ch < NUM_EEG_CHANNELS; ch++)
            eegData[ch] = sample.eeg[ch];
        
        // Build AUX data array (9 channels: AccelXYZ, PPG x3, Temp, Battery, Sync)
        float auxData[NUM_AUX_CHANNELS];
        auxData[0] = sample.accel[0];  // AccelX (in G)
        auxData[1] = sample.accel[1];  // AccelY (in G)
        auxData[2] = sample.accel[2];  // AccelZ (in G)
        auxData[3] = sample.ppg[0];    // PPG_Red (raw counts)
        auxData[4] = sample.ppg[1];    // PPG_IR (raw counts)
        auxData[5] = sample.ppg[2];    // PPG_Green (raw counts)
        auxData[6] = sample.temperature * 100.0f;  // Temperature (centi-degrees for display)
        auxData[7] = sample.battery;               // Battery (mV, already in mV)
        auxData[8] = 0.0f;             // Sync (placeholder - not in optimized protocol)
        
        // Add to Open Ephys buffers
        int64_t sampleNum = static_cast<int64_t>(sampleCount);
        double timestamp = static_cast<double>(sampleCount);
        uint64 eventCode = sample.hasMarkerData ? sample.marker : 0;
        
        // EEG -> buffer 0
        sourceBuffers[0]->addToBuffer(eegData, &sampleNum, &timestamp, &eventCode, 1);
        
        // AUX -> buffer 1 (no events)
        uint64 zeroEvent = 0;
        sourceBuffers[1]->addToBuffer(auxData, &sampleNum, &timestamp, &zeroEvent, 1);
        
        sampleCount++;
    }
    
    return true;
}
