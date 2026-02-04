/*
    ------------------------------------------------------------------

    InEar Teensy Optimized Source Plugin for Open Ephys
    
    DataThread implementation for variable-length packet protocol.

    ------------------------------------------------------------------
*/

#ifndef INEAR_TEENSY_OPTIMIZED_THREAD_H
#define INEAR_TEENSY_OPTIMIZED_THREAD_H

#include <DataThreadHeaders.h>
#include "OptimizedProtocol.h"
#include <atomic>
#include <vector>
#include <deque>
#include <chrono>
#include <mutex>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <termios.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
#endif

using namespace OptimizedProtocol;

// =============================================================================
// Serial Port Wrapper
// =============================================================================

class OptimizedSerialPort
{
public:
    OptimizedSerialPort();
    ~OptimizedSerialPort();
    
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
// Sensor State Tracking
// =============================================================================

struct SensorState
{
    float values[6];            // Up to 6 values per sensor type
    int numChannels;
    uint32_t lastUpdateSample;  // Sample number of last update
    uint64_t lastUpdateTime;    // System time of last update
    bool isValid;               // Has received at least one update
    
    SensorState(int channels = 1) 
        : numChannels(channels), lastUpdateSample(0), lastUpdateTime(0), isValid(false)
    {
        for (int i = 0; i < 6; i++) values[i] = 0.0f;
    }
    
    void update(const float* newValues, uint32_t sampleNum, uint64_t time)
    {
        for (int i = 0; i < numChannels; i++)
            values[i] = newValues[i];
        lastUpdateSample = sampleNum;
        lastUpdateTime = time;
        isValid = true;
    }
    
    int getAge(uint32_t currentSample) const
    {
        if (!isValid) return -1;
        return static_cast<int>(currentSample - lastUpdateSample);
    }
    
    bool isStale(uint32_t currentSample, int maxAge) const
    {
        return !isValid || getAge(currentSample) > maxAge;
    }
};

// =============================================================================
// Parsed Sample Structure
// =============================================================================

struct OptimizedSample
{
    uint32_t timestamp_us;
    uint8_t sequence;
    PacketType packetType;
    
    float eeg[NUM_EEG_CHANNELS];
    float accel[NUM_ACCEL_CHANNELS];
    float ppg[NUM_PPG_CHANNELS];
    float temperature;
    float battery;
    uint8_t marker;
    
    bool hasAccelData;
    bool hasPPGData;
    bool hasHealthData;
    bool hasMarkerData;
    
    // Age tracking (samples since last update)
    int accelAge;
    int ppgAge;
    int healthAge;
    
    bool valid;
};

// =============================================================================
// Protocol Parser with Reassembly Buffer
// =============================================================================

class OptimizedProtocolParser
{
public:
    OptimizedProtocolParser();
    
    /** Feed raw bytes and extract parsed samples */
    std::vector<OptimizedSample> parse(const uint8_t* data, int numBytes);
    
    /** Reset parser state */
    void reset();
    
    /** Get statistics */
    uint64_t getPacketsReceived() const { return packetsReceived; }
    uint64_t getPacketsDropped() const { return packetsDropped; }
    uint64_t getChecksumErrors() const { return checksumErrors; }
    uint64_t getFramingErrors() const { return framingErrors; }
    uint64_t getBytesReceived() const { return bytesReceived; }
    
    /** Get current sensor states */
    const SensorState& getAccelState() const { return accelState; }
    const SensorState& getPPGState() const { return ppgState; }
    const SensorState& getHealthState() const { return healthState; }

private:
    std::deque<uint8_t> buffer;
    
    // Sensor state tracking (for interpolation/holdover)
    SensorState accelState{NUM_ACCEL_CHANNELS};
    SensorState ppgState{NUM_PPG_CHANNELS};
    SensorState healthState{2};  // Temp + Battery
    
    // Sequence tracking
    uint8_t lastSequence = 0;
    bool firstPacket = true;
    uint32_t currentSampleNum = 0;
    
    // Statistics
    uint64_t packetsReceived = 0;
    uint64_t packetsDropped = 0;
    uint64_t checksumErrors = 0;
    uint64_t framingErrors = 0;
    uint64_t bytesReceived = 0;
    
    // Parsing helpers
    int findSyncPosition();
    bool tryParsePacket(OptimizedSample& sample);
    bool parsePacketData(const uint8_t* packet, int size, OptimizedSample& sample);
    
    // Data extraction helpers
    int32_t bytes24ToInt32(const uint8_t* bytes);
    int16_t bytes16ToInt16(const uint8_t* bytes);
    int64_t bytes48ToInt64(const uint8_t* bytes);
    uint32_t bytes32ToUint32(const uint8_t* bytes);
};

// =============================================================================
// InEarTeensyOptimizedThread - DataThread Implementation
// =============================================================================

class InEarTeensyOptimizedThread : public DataThread
{
public:
    InEarTeensyOptimizedThread(SourceNode* sn);
    ~InEarTeensyOptimizedThread();

    // DataThread interface
    std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;
    bool foundInputSource() override;
    bool isReady() override;
    bool startAcquisition() override;
    bool stopAcquisition() override;
    void updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                       OwnedArray<EventChannel>* eventChannels,
                       OwnedArray<SpikeChannel>* spikeChannels,
                       OwnedArray<DataStream>* dataStreams,
                       OwnedArray<DeviceInfo>* devices,
                       OwnedArray<ConfigurationObject>* configObjects) override;
    bool updateBuffer() override;
    void registerParameters() override;

    // Connection management
    StringArray getAvailablePorts();
    bool connect();
    void disconnect();
    bool isConnected() const { return serialConnected; }
    
    // Port configuration
    void setPort(const String& portName) { selectedPort = portName; }
    String getPort() const { return selectedPort; }
    
    // Simulation mode
    void setSimulationMode(bool enable);
    bool isSimulating() const { return simulationMode; }
    
    // Statistics access
    uint64_t getPacketsReceived() const { return parser.getPacketsReceived(); }
    uint64_t getPacketsDropped() const { return parser.getPacketsDropped(); }
    uint64_t getChecksumErrors() const { return parser.getChecksumErrors(); }

private:
    // Serial port
    std::unique_ptr<OptimizedSerialPort> serial;
    String selectedPort;
    std::atomic<bool> serialConnected{false};
    
    // Parser
    OptimizedProtocolParser parser;
    
    // Sample buffer
    std::vector<float> sampleBuffer;
    std::mutex bufferMutex;
    
    // Timing
    std::chrono::steady_clock::time_point streamStartTime;
    uint64_t sampleCount = 0;
    
    // Simulation
    std::atomic<bool> simulationMode{true};  // Default to simulation for testing
    double simPhase = 0.0;
    uint32_t simSampleNum = 0;
    
    // Channel configuration (matching original InEar Teensy: 5 EEG + 9 AUX)
    static constexpr int NUM_AUX_CHANNELS = 9;  // AccelX/Y/Z, PPG_Red/IR/Green, Temp, Battery, Sync
    static constexpr int TOTAL_CHANNELS = NUM_EEG_CHANNELS + NUM_AUX_CHANNELS;
    // = 5 + 9 = 14 channels total
    
    // Helpers
    void generateSimulatedData(float* buffer, int numSamples);
    int readFromSerial(uint8_t* buffer, int maxBytes);
};

#endif // INEAR_TEENSY_OPTIMIZED_THREAD_H
