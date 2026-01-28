/*
    ------------------------------------------------------------------

    Custom IC Source Plugin for Open Ephys
    
    A DataThread plugin for reading data from custom integrated circuits
    via serial/UART communication.

    ------------------------------------------------------------------
*/

#ifndef CUSTOM_IC_THREAD_H
#define CUSTOM_IC_THREAD_H

#include <DataThreadHeaders.h>
#include <atomic>
#include <vector>
#include <string>
#include <memory>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <termios.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
#endif

namespace CustomIC {

/**
 * Serial port wrapper for cross-platform communication
 */
class SerialPort
{
public:
    SerialPort();
    ~SerialPort();
    
    /** Open a serial port */
    bool open(const String& portName, int baudRate);
    
    /** Close the port */
    void close();
    
    /** Check if port is open */
    bool isOpen() const;
    
    /** Read data from port */
    int read(uint8_t* buffer, int maxBytes);
    
    /** Write data to port */
    int write(const uint8_t* data, int numBytes);
    
    /** Get available bytes */
    int available();
    
    /** Flush buffers */
    void flush();
    
    /** Get list of available ports */
    static StringArray getAvailablePorts();

private:
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int fd = -1;
#endif
};

/**
 * Data packet structure for parsed samples
 */
struct DataPacket
{
    std::vector<float> samples;  // One sample per channel
    int64 timestamp;             // Hardware timestamp if available
    bool valid;
};

/**
 * Protocol parser for custom IC data format
 */
class ProtocolParser
{
public:
    ProtocolParser();
    virtual ~ProtocolParser() = default;
    
    /** Configure parser parameters */
    void configure(int numChannels, int bytesPerSample, float scaleFactor);
    
    /** Set sync bytes for packet detection */
    void setSyncBytes(uint8_t sync1, uint8_t sync2);
    
    /** Process incoming bytes and extract packets */
    virtual std::vector<DataPacket> parse(const uint8_t* data, int numBytes);
    
    /** Reset parser state */
    virtual void reset();
    
    /** Get expected packet size */
    int getPacketSize() const;

protected:
    /** Convert raw bytes to sample value */
    virtual float bytesToSample(const uint8_t* bytes, int numBytes);
    
    /** Validate packet checksum */
    virtual bool validateChecksum(const uint8_t* packet, int size);

    int numChannels = 8;
    int bytesPerSample = 2;
    float scaleFactor = 0.195f;
    uint8_t syncByte1 = 0xA0;
    uint8_t syncByte2 = 0x5A;
    bool useChecksum = true;
    
    std::vector<uint8_t> buffer;
    static const int MAX_BUFFER_SIZE = 65536;
};

} // namespace CustomIC


/**
 * CustomICThread - DataThread for custom IC data acquisition
 */
class CustomICThread : public DataThread
{
public:
    CustomICThread(SourceNode* sn);
    ~CustomICThread();

    // ------------------------------------------------------------
    //                  PURE VIRTUAL METHODS
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
    //                    VIRTUAL METHODS
    // ------------------------------------------------------------
    
    std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;
    void registerParameters() override;
    void parameterValueChanged(Parameter* param) override;

    // Configuration methods
    void setPort(const String& portName);
    void setBaudRate(int rate);
    void setNumChannels(int num);
    void setSampleRate(float rate);
    void setDataFormat(int bytesPerSample);
    void setScaleFactor(float scale);
    void setSyncBytes(uint8_t sync1, uint8_t sync2);
    void setSimulationMode(bool simulate);
    
    // Status methods
    String getPort() const { return portName; }
    int getBaudRate() const { return baudRate; }
    int getNumChannels() const { return numChannels; }
    float getSampleRate() const { return sampleRate; }
    bool isSimulating() const { return simulationMode; }
    bool isConnected() const;
    
    StringArray getAvailablePorts() const;
    
    bool connect();
    void disconnect();

    static DataThread* createDataThread(SourceNode* sn);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomICThread);

private:
    // Serial communication
    std::unique_ptr<CustomIC::SerialPort> serial;
    String portName = "";
    int baudRate = 115200;
    
    // Data configuration
    int numChannels = 8;
    float sampleRate = 256.0f;
    int bytesPerSample = 2;
    float scaleFactor = 0.195f;
    
    // Protocol parser
    std::unique_ptr<CustomIC::ProtocolParser> parser;
    
    // Buffers
    std::vector<uint8_t> readBuffer;
    static const int READ_BUFFER_SIZE = 4096;
    
    float* dataBuffer;
    double* timestampBuffer;
    int64* sampleNumbers;
    uint64* ttlEventWords;
    int bufferSize = 1024;
    
    // Simulation mode
    bool simulationMode = false;
    double simPhase = 0.0;
    int64 totalSamples = 0;
    double initialTimestamp = -1.0;
    
    // Status
    std::atomic<bool> connected{false};
    
    void generateSimulatedData();
};

#endif // CUSTOM_IC_THREAD_H
