/*
    ------------------------------------------------------------------

    InEar Teensy Source Plugin for Open Ephys
    
    A DataThread plugin for reading EEG data from Teensy + ADS1299
    using the InEar Teensy fixed-length packet protocol.

    Protocol: 56-byte packets @ 1kHz
    - Header:      0xA5 0x5A (2B)
    - Timestamp:   Microseconds (4B, Big Endian)
    - Marker:      Event triggers (1B)
    - EEG:         5ch × 24-bit BE (15B)
    - Accel:       3ch × 16-bit BE (6B) - X, Y, Z
    - PPG:         3ch × 48-bit BE (18B) - Red, IR, Green
    - Temperature: 1ch × 16-bit BE (2B)
    - Battery:     1ch × 16-bit BE (2B)
    - Sync:        1ch × 16-bit BE (2B)
    - Counter:     Packet sequence (1B)
    - Checksum:    XOR validation (1B)
    - Footer:      0xC0 0xC0 (2B)

    ------------------------------------------------------------------
*/

#ifndef INEAR_TEENSY_THREAD_H
#define INEAR_TEENSY_THREAD_H

#include <DataThreadHeaders.h>
#include <atomic>
#include <vector>
#include <string>
#include <memory>
#include <deque>
#include <chrono>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <termios.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
#endif

namespace InEarTeensy {

// =============================================================================
// Protocol Constants
// =============================================================================

constexpr uint8_t HEADER_BYTE_1 = 0xA5;
constexpr uint8_t HEADER_BYTE_2 = 0x5A;
constexpr uint8_t FOOTER_BYTE_1 = 0xC0;
constexpr uint8_t FOOTER_BYTE_2 = 0xC0;

constexpr int PACKET_SIZE = 56;          // 2+4+1+15+6+18+2+2+2+1+1+2 = 56 bytes
constexpr int NUM_EEG_CHANNELS = 5;
constexpr int NUM_AUX_CHANNELS = 9;      // AccelX/Y/Z, PPG_Red/IR/Green, Temp, Battery, Sync
constexpr int EEG_SAMPLE_RATE = 1000;
constexpr int AUX_SAMPLE_RATE = 1000;

// Byte offsets within packet
constexpr int OFFSET_HEADER = 0;
constexpr int OFFSET_TIMESTAMP = 2;
constexpr int OFFSET_MARKER = 6;
constexpr int OFFSET_EEG = 7;            // 5ch × 3B = 15B
constexpr int OFFSET_ACCEL = 22;         // 3ch × 2B = 6B
constexpr int OFFSET_PPG = 28;           // 3ch × 6B = 18B
constexpr int OFFSET_TEMP = 46;          // 1ch × 2B = 2B
constexpr int OFFSET_BATTERY = 48;       // 1ch × 2B = 2B
constexpr int OFFSET_SYNC = 50;          // 1ch × 2B = 2B
constexpr int OFFSET_COUNTER = 52;
constexpr int OFFSET_CHECKSUM = 53;
constexpr int OFFSET_FOOTER = 54;

// Scale factors
constexpr float EEG_SCALE_UV = 0.0223517f;  // 24-bit to µV (ADS1299 @ Gain 24)
constexpr float AUX_SCALE = 1.0f;           // Raw values for aux channels

// =============================================================================
// Serial Port Wrapper
// =============================================================================

class SerialPort
{
public:
    SerialPort();
    ~SerialPort();
    
    bool open(const String& portName, int baudRate);
    void close();
    bool isOpen() const;
    int read(uint8_t* buffer, int maxBytes);
    int write(const uint8_t* data, int numBytes);
    int available();
    void flush();
    
    static StringArray getAvailablePorts();

private:
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int fd = -1;
#endif
};

// =============================================================================
// Parsed Data Structures
// =============================================================================

struct EEGSample
{
    uint32_t timestamp_us;      // Hardware timestamp (microseconds)
    uint8_t marker;             // Event marker byte
    float eeg[NUM_EEG_CHANNELS]; // EEG in µV
    float aux[NUM_AUX_CHANNELS]; // Aux data (accel or health)
    uint8_t counter;            // Packet counter (0-255)
    bool isHealthPacket;        // True if aux contains health data
    bool valid;                 // Checksum passed
};

// =============================================================================
// Ring Buffer for USB Chunk Reassembly
// =============================================================================

class ReassemblyBuffer
{
public:
    ReassemblyBuffer(size_t capacity = 8192);
    
    void push(const uint8_t* data, size_t length);
    bool tryExtractPacket(uint8_t* packet);
    void clear();
    size_t bytesAvailable() const;
    
private:
    std::deque<uint8_t> buffer;
    size_t maxCapacity;
    
    int findSyncPosition() const;
    bool validatePacket(const uint8_t* data) const;
    static uint8_t computeChecksum(const uint8_t* data, int length);
};

// =============================================================================
// InEar Teensy Protocol Parser
// =============================================================================

class ProtocolParser
{
public:
    ProtocolParser();
    
    /** Parse raw bytes and extract valid samples */
    std::vector<EEGSample> parse(const uint8_t* data, int numBytes);
    
    /** Reset parser state */
    void reset();
    
    /** Get statistics */
    uint64_t getPacketsReceived() const { return packetsReceived; }
    uint64_t getChecksumErrors() const { return checksumErrors; }
    uint64_t getFramingErrors() const { return framingErrors; }
    uint64_t getDroppedPackets() const { return droppedPackets; }
    
private:
    ReassemblyBuffer reassemblyBuffer;
    
    // Statistics
    uint64_t packetsReceived = 0;
    uint64_t checksumErrors = 0;
    uint64_t framingErrors = 0;
    uint64_t droppedPackets = 0;
    uint8_t lastCounter = 0;
    bool firstPacket = true;
    
    EEGSample parsePacket(const uint8_t* packet);
    int32_t bytes24ToInt32(const uint8_t* bytes);
    int16_t bytes16ToInt16(const uint8_t* bytes);
    int64_t bytes48ToInt64(const uint8_t* bytes);
};

} // namespace InEarTeensy


// =============================================================================
// InEarTeensyThread - DataThread Implementation
// =============================================================================

class InEarTeensyThread : public DataThread
{
public:
    InEarTeensyThread(SourceNode* sn);
    ~InEarTeensyThread();

    // ------------------------------------------------------------
    // Pure Virtual Methods (Required by DataThread)
    // ------------------------------------------------------------
    
    bool foundInputSource() override;
    bool startAcquisition() override;
    bool stopAcquisition() override;
    bool updateBuffer() override;
    
    void updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                        OwnedArray<EventChannel>* eventChannels,
                        OwnedArray<SpikeChannel>* spikeChannels,
                        OwnedArray<DataStream>* sourceStreams,
                        OwnedArray<DeviceInfo>* devices,
                        OwnedArray<ConfigurationObject>* configurationObjects) override;

    // ------------------------------------------------------------
    // Virtual Methods
    // ------------------------------------------------------------
    
    std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;
    void registerParameters() override;
    void parameterValueChanged(Parameter* param) override;

    // Configuration
    void setPort(const String& portName);
    void setBaudRate(int rate);
    void setSimulationMode(bool simulate);
    
    // Status
    String getPort() const { return portName; }
    int getBaudRate() const { return baudRate; }
    bool isSimulating() const { return simulationMode; }
    bool isConnected() const;
    
    StringArray getAvailablePorts() const;
    
    bool connect();
    void disconnect();

    static DataThread* createDataThread(SourceNode* sn);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InEarTeensyThread);

private:
    // Serial communication
    std::unique_ptr<InEarTeensy::SerialPort> serial;
    String portName = "";
    int baudRate = 2000000;  // 2 Mbaud for USB
    
    // Protocol parser
    std::unique_ptr<InEarTeensy::ProtocolParser> parser;
    
    // Read buffer
    std::vector<uint8_t> readBuffer;
    static const int READ_BUFFER_SIZE = 8192;
    
    // Output buffers for Open Ephys
    float* eegBuffer;
    float* auxBuffer;
    double* timestampBuffer;
    int64* sampleNumbers;
    uint64* eventWords;
    int bufferSize = 2048;
    
    // Simulation mode
    bool simulationMode = false;
    double simPhase = 0.0;
    int64 totalSamples = 0;
    double initialTimestamp = -1.0;
    std::chrono::high_resolution_clock::time_point simStartTime;
    
    // Bandwidth monitoring
    std::chrono::steady_clock::time_point bandwidthStartTime;
    std::chrono::steady_clock::time_point acquisitionStartTime;
    uint64_t bandwidthBytes = 0;
    uint64_t totalBytesReceived = 0;
    int bandwidthLogIntervalSec = 5;  // Log every 5 seconds
    
    // Packet size tracking (for consistent logging with optimized version)
    int minPacketSize = INT_MAX;
    int maxPacketSize = 0;
    uint64_t totalPacketBytes = 0;
    uint64_t packetCount = 0;
    
    // Status
    std::atomic<bool> connected{false};
    
    // Accelerometer decimation
    int accelDecimationCounter = 0;
    float lastAccel[3] = {0, 0, 0};
    
    void generateSimulatedData();
};

#endif // INEAR_TEENSY_THREAD_H


