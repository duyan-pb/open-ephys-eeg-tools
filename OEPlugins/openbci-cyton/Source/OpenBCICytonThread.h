/*
    OpenBCI Cyton Plugin for Open Ephys
    
    Connects to OpenBCI Cyton board via serial port (RFDuino dongle)
    Supports 8-channel Cyton and 16-channel Cyton+Daisy configurations
*/

#ifndef OPENBCI_CYTON_THREAD_H
#define OPENBCI_CYTON_THREAD_H

#include <DataThreadHeaders.h>
#include <atomic>
#include <vector>
#include <array>
#include <chrono>

// Serial port handling
#ifdef _WIN32
    #include <windows.h>
#else
    #include <termios.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace OpenBCICyton
{
    // Constants
    constexpr int CYTON_CHANNELS = 8;
    constexpr int DAISY_CHANNELS = 8;
    constexpr int MAX_CHANNELS = 16;
    constexpr int PACKET_SIZE = 33;
    constexpr uint8_t PACKET_HEADER = 0xA0;
    constexpr uint8_t PACKET_FOOTER_MASK = 0xC0;
    constexpr int BAUD_RATE = 115200;
    constexpr float DEFAULT_SAMPLE_RATE = 250.0f;
    
    // Scale factors
    // EEG: 4.5V / gain / (2^23 - 1)
    // Default gain = 24x
    constexpr double EEG_SCALE_FACTOR_UV = 0.02235;  // microvolts per count at 24x gain
    constexpr double EEG_SCALE_FACTOR_V = EEG_SCALE_FACTOR_UV * 1e-6;  // volts per count
    
    // Accelerometer: 2mG per digit, assuming 4G range
    constexpr double ACCEL_SCALE_FACTOR = 0.002 / 16.0;  // G per count
    
    // Gain settings
    enum class Gain : int
    {
        GAIN_1X = 1,
        GAIN_2X = 2,
        GAIN_4X = 4,
        GAIN_6X = 6,
        GAIN_8X = 8,
        GAIN_12X = 12,
        GAIN_24X = 24
    };
    
    // Packet types based on footer byte
    enum class PacketType : uint8_t
    {
        STANDARD_ACCEL = 0xC0,
        STANDARD_RAW_AUX = 0xC1,
        USER_DEFINED = 0xC2,
        TIMESTAMP_SET_ACCEL = 0xC3,
        TIMESTAMP_ACCEL = 0xC4,
        TIMESTAMP_SET_RAW_AUX = 0xC5,
        TIMESTAMP_RAW_AUX = 0xC6
    };
}

class OpenBCICytonThread : public DataThread
{
public:
    /** Constructor */
    OpenBCICytonThread(SourceNode* sn);
    
    /** Destructor */
    ~OpenBCICytonThread();
    
    /** Creates the UI */
    std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;
    
    /** Called by the editor to connect/disconnect */
    bool foundInputSource() override;
    
    /** Returns true if data source is connected */
    bool isReady() override;
    
    /** Starts data acquisition */
    bool startAcquisition() override;
    
    /** Stops data acquisition */
    bool stopAcquisition() override;
    
    /** Updates settings */
    void updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                       OwnedArray<EventChannel>* eventChannels,
                       OwnedArray<SpikeChannel>* spikeChannels,
                       OwnedArray<DataStream>* dataStreams,
                       OwnedArray<DeviceInfo>* devices,
                       OwnedArray<ConfigurationObject>* configObjects) override;
    
    /** Main data acquisition loop */
    bool updateBuffer() override;
    
    /** Register parameters */
    void registerParameters() override;
    
    /** Serial port management */
    std::vector<std::string> getAvailablePorts();
    bool connectToPort(const std::string& portName);
    void disconnect();
    bool isConnected() const { return serialConnected; }
    
    /** Configuration */
    void setDaisyMode(bool enabled);
    bool isDaisyMode() const { return useDaisy; }
    void setChannelGain(int channel, OpenBCICyton::Gain gain);
    
    /** Get current port */
    std::string getCurrentPort() const { return currentPort; }
    
    /** Get sample rate */
    float getSampleRate() const { return sampleRate; }
    
    /** Get number of active channels */
    int getNumChannels() const { return useDaisy ? OpenBCICyton::MAX_CHANNELS : OpenBCICyton::CYTON_CHANNELS; }
    
    /** Check for firmware version */
    std::string getFirmwareVersion() const { return firmwareVersion; }
    
    // Type identifier
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenBCICytonThread);
    
private:
    /** Serial port operations */
    bool openSerialPort(const std::string& portName);
    void closeSerialPort();
    int readSerial(uint8_t* buffer, int numBytes);
    int writeSerial(const uint8_t* buffer, int numBytes);
    bool sendCommand(char cmd);
    bool sendCommand(const std::string& cmd);
    std::string readResponse(int timeoutMs = 2000);
    bool waitForReady(int timeoutMs = 5000);
    
    /** Initialize the board */
    bool initializeBoard();
    bool resetBoard();
    
    /** Parse incoming data */
    bool parsePacket(const uint8_t* packet);
    int32_t interpret24BitAsInt32(const uint8_t* bytes);
    int16_t interpret16BitAsInt16(const uint8_t* bytes);
    
    /** Calculate scale factor based on gain */
    double getScaleFactor(OpenBCICyton::Gain gain);
    
    // Serial port handle
#ifdef _WIN32
    HANDLE serialHandle = INVALID_HANDLE_VALUE;
#else
    int serialFd = -1;
#endif
    
    // State
    std::atomic<bool> serialConnected{false};
    std::atomic<bool> isStreaming{false};
    std::string currentPort;
    std::string firmwareVersion;
    
    // Configuration
    bool useDaisy = false;
    float sampleRate = OpenBCICyton::DEFAULT_SAMPLE_RATE;
    std::array<OpenBCICyton::Gain, OpenBCICyton::MAX_CHANNELS> channelGains;
    std::array<double, OpenBCICyton::MAX_CHANNELS> scaleFactors;
    
    // Data buffers
    std::array<float, OpenBCICyton::MAX_CHANNELS> channelData;
    std::array<float, 3> accelData;  // X, Y, Z
    std::array<uint8_t, 3> accelHighBytes;  // For interleaved accel parsing in timestamp mode
    uint8_t lastSampleNumber = 0;
    
    // For Daisy mode averaging
    std::array<int32_t, OpenBCICyton::DAISY_CHANNELS> previousDaisySamples;
    bool hasPreviousDaisySample = false;
    
    // Packet parsing
    std::vector<uint8_t> serialBuffer;
    static constexpr size_t BUFFER_SIZE = 4096;
    
    // Timestamps
    uint32_t boardTimestamp = 0;
    std::chrono::steady_clock::time_point streamStartTime;
    int64_t sampleCount = 0;
};

#endif // OPENBCI_CYTON_THREAD_H
