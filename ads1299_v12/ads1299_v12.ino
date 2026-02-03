#include <SPI.h>
#include "SdFat.h"
#include "RingBuf.h"
#include "xorshift.h"

extern "C" uint32_t set_arm_clock(uint32_t frequency);

// @SETTING RECORDING LENGTH SECONDS, 3600 seconds per hour, set number of hours
#define REC_SEC 20

// SD Card data logger settings
// Size to log 8 ch LFP, 3 ch accel, 1 ch sync at 1kHz for 8 hours: (12 * 4) * (1000 * 3600 * 8) bytes
#define LOG_FILE_SIZE 1382400000


// Space to hold 530 ms of data, in case writing to SD card stalls
#define RING_BUF_CAPACITY 50 * 512
#define LOG_FILENAME "SdioLogger.bin"

SdFs sd;
FsFile file;

// RingBuf for File type FsFile.
RingBuf<FsFile, RING_BUF_CAPACITY> rb;

// Synchronissation signal
const int SYNC = 0;

// Defines the SYNC pulse seed, timing of pulse, min and max in msec
#define RNG_SEED 0x12345678
#define ON_LENGTH_MS 100
#define INTERVAL_MIN_MS 500
#define INTERVAL_MAX_MS 2000

uint32_t xorshift_state = RNG_SEED;
uint32_t previous_millis = 0;
uint8_t  out_state = LOW;
uint32_t next_interval = 1000; // ms
uint32_t random_interval = 0;
uint8_t  rng_ready = 0;

// ADXL settings
SPISettings ADXL_settings(5E6, MSBFIRST, SPI_MODE3);

const int ADXL_CS = 10;

// Register addresses: 0x0B
const uint8_t BW_RATE     = 0x2C;  // Data rate and power mode control
const uint8_t POWER_CTL   = 0x2D;  // Power-saving features control
const uint8_t DATA_FORMAT = 0x31;  // Data format control
const uint8_t DATAX0      = 0x32;  // X-Axis Data 0
const uint8_t DATAX1      = 0x33;  // X-Axis Data 1
const uint8_t DATAY0      = 0x34;  // Y-Axis Data 0
const uint8_t DATAY1      = 0x35;  // Y-Axis Data 1
const uint8_t DATAZ0      = 0x36;  // Z-Axis Data 0
const uint8_t DATAZ1      = 0x37;  // Z-Axis Data 1

// Register access modes
const uint8_t ADXL_WRITE     = 0x00;
const uint8_t ADXL_READ      = 0x80;
const uint8_t ADXL_READ_MULT = 0xC0;

void ADXL_init(void);
void ADXL_read(int16_t *x, int16_t *y, int16_t *z);

// ADS1299 settings

SPISettings ADS1299_SPI_settings(20E6, MSBFIRST, SPI_MODE1);

const int ADS1299_CS1 = 7;
const int ADS1299_CS2 = 6;
const int ADS1299_CS3 = 5;
const int START = 15;
//const int N_DRDY = 17; // v1
const int N_DRDY = 22; // v2
const int N_PWDN = 14;

const int N_CH = 8;

const int GAIN_1x =  0x00; // step: 536nV, range: +/- 4.500V
const int GAIN_2x =  0x10; // step: 268nV, range: +/- 2.250V
const int GAIN_4x =  0x20; // step: 134nV, range: +/- 1.125V
const int GAIN_6x =  0x30; // step:  89nV, range: +/- 0.750V
const int GAIN_8x =  0x40; // step:  67nV, range: +/- 0.563V
const int GAIN_12x = 0x50; // step:  45nV, range: +/- 0.375V
const int GAIN_24x = 0x60; // step:  22nV, range: +/- 0.188V

const int FS_250 = 0x06;
const int FS_500 = 0x05;
const int FS_1k =  0x04;
const int FS_2k =  0x03;
const int FS_4k =  0x02;
const int FS_8k =  0x01;
const int FS_16k = 0x00;

const int CHSET_INPUT = 0x00;
const int SHORT = 0x01;
const int TEST =  0x05;

const int SRB1_CLOSED = 0x20;

int32_t buffer[N_CH+3+1];
volatile bool ADS1299_data_ready = false;

volatile int accel_sampling_divider = 0;

// ============================================================================
// BioSerial-Pro Protocol (for Open Ephys plugin + LSL Outlet)
// ============================================================================
// Packet structure (56 bytes):
// - Header:      0xA5 0x5A (2B)
// - Timestamp:   Microseconds, big-endian (4B)
// - Marker:      Event trigger byte (1B)  
// - EEG:         5 channels × 24-bit BE (15B)
// - AccelX/Y/Z:  3 channels × 16-bit BE (6B)
// - PPG1/2/3:    3 channels × 48-bit BE (18B) - Red, IR, Green
// - Temperature: 1 channel × 16-bit BE (2B)
// - Battery:     1 channel × 16-bit BE (2B)
// - Sync:        1 channel × 16-bit BE (2B)
// - Counter:     Packet sequence 0-255 (1B)
// - Checksum:    XOR of bytes 0-52 (1B)
// - Footer:      0xC0 0xC0 (2B)
// ============================================================================

#define ENABLE_USB_STREAMING true
#define USB_BAUD_RATE 2000000  // 2 Mbps for Teensy USB

#define BSP_NUM_EEG_CH 5      // Only send first 5 EEG channels
#define BSP_PACKET_SIZE 56
#define BSP_HEADER_1 0xA5
#define BSP_HEADER_2 0x5A
#define BSP_FOOTER_1 0xC0
#define BSP_FOOTER_2 0xC0

uint8_t bsp_packet[BSP_PACKET_SIZE];
uint8_t bsp_counter = 0;

// ============================================================================
// SIMULATION MODE - Generate synthetic sensor data
// ============================================================================
#define SIMULATION_MODE true

// Simulated sensor values
int64_t ppg_red = 0;
int64_t ppg_ir = 0;
int64_t ppg_green = 0;
int16_t temperature = 2500;   // 25.00°C
int16_t battery_mv = 4200;    // 4.2V (full charge)

// Simulation phase counters
float sim_phase = 0.0f;
#ifndef TWO_PI
#define TWO_PI 6.28318530718f
#endif

// Generate simulated data for all sensors
void generateSimulatedData() {
    float t = sim_phase;
    
    // EEG: 5 channels with different frequencies (visible sine waves)
    // Using large amplitude for clear visibility: ~100,000 counts = ~2.2mV
    buffer[0] = (int32_t)(100000.0f * sin(TWO_PI * 3.0f * t));   // 3 Hz - Delta
    buffer[1] = (int32_t)(80000.0f * sin(TWO_PI * 6.0f * t));    // 6 Hz - Theta  
    buffer[2] = (int32_t)(60000.0f * sin(TWO_PI * 10.0f * t));   // 10 Hz - Alpha
    buffer[3] = (int32_t)(40000.0f * sin(TWO_PI * 20.0f * t));   // 20 Hz - Beta
    buffer[4] = (int32_t)(30000.0f * sin(TWO_PI * 40.0f * t));   // 40 Hz - Gamma
    // Remaining channels (for SD card logging)
    buffer[5] = (int32_t)(50000.0f * sin(TWO_PI * 8.0f * t));
    buffer[6] = (int32_t)(50000.0f * sin(TWO_PI * 12.0f * t));
    buffer[7] = (int32_t)(50000.0f * sin(TWO_PI * 15.0f * t));
    
    // Accelerometer: Simulate gentle motion
    buffer[N_CH + 0] = (int32_t)(100.0f * sin(TWO_PI * 0.5f * t));   // AccelX - 0.5 Hz sway
    buffer[N_CH + 1] = (int32_t)(100.0f * sin(TWO_PI * 0.7f * t));   // AccelY - 0.7 Hz sway
    buffer[N_CH + 2] = (int32_t)(256.0f + 50.0f * sin(TWO_PI * 0.3f * t)); // AccelZ - ~1g + wobble
    
    // PPG: Simulate heartbeat at ~72 BPM (1.2 Hz) with typical waveform
    // 48-bit values, using ~20-bit range for realistic PPG
    float heartbeat = sin(TWO_PI * 1.2f * t);
    float ppg_pulse = (heartbeat > 0) ? pow(heartbeat, 2) : 0;  // Sharp peaks
    ppg_red   = (int64_t)(500000.0 * ppg_pulse + 100000.0 * sin(TWO_PI * 0.15f * t)); // + breathing
    ppg_ir    = (int64_t)(600000.0 * ppg_pulse + 120000.0 * sin(TWO_PI * 0.15f * t));
    ppg_green = (int64_t)(400000.0 * ppg_pulse + 80000.0 * sin(TWO_PI * 0.15f * t));
    
    // Temperature: Slow drift around 25°C (2500 centi-degrees)
    temperature = (int16_t)(2500 + 50 * sin(TWO_PI * 0.01f * t));  // ±0.5°C drift
    
    // Battery: Slow discharge simulation (4200mV to 3300mV over ~2 hours simulated)
    battery_mv = (int16_t)(4200 - (int)(t * 0.1f) % 900);  // Cycles 4200-3300
    
    // Advance simulation phase (1 kHz sample rate)
    sim_phase += 0.001f;  // 1ms per sample
    if (sim_phase > 1000.0f) sim_phase -= 1000.0f;  // Wrap every 1000 seconds
}

void sendBioSerialProPacket() {
    if (!ENABLE_USB_STREAMING) return;
    
    int idx = 0;
    
    // Header (2 bytes)
    bsp_packet[idx++] = BSP_HEADER_1;
    bsp_packet[idx++] = BSP_HEADER_2;
    
    // Timestamp (4 bytes, microseconds, big-endian)
    uint32_t ts = micros();
    bsp_packet[idx++] = (ts >> 24) & 0xFF;
    bsp_packet[idx++] = (ts >> 16) & 0xFF;
    bsp_packet[idx++] = (ts >> 8) & 0xFF;
    bsp_packet[idx++] = ts & 0xFF;
    
    // Marker byte (1 byte) - sync state as event marker
    bsp_packet[idx++] = out_state;
    
    // EEG channels (5 × 3 bytes = 15 bytes, 24-bit signed big-endian)
    for (int ch = 0; ch < BSP_NUM_EEG_CH; ch++) {
        int32_t val = buffer[ch];
        bsp_packet[idx++] = (val >> 16) & 0xFF;
        bsp_packet[idx++] = (val >> 8) & 0xFF;
        bsp_packet[idx++] = val & 0xFF;
    }
    
    // Accelerometer (3 × 2 bytes = 6 bytes, 16-bit signed big-endian)
    for (int i = 0; i < 3; i++) {
        int16_t val = (int16_t) buffer[N_CH + i];
        bsp_packet[idx++] = (val >> 8) & 0xFF;
        bsp_packet[idx++] = val & 0xFF;
    }
    
    // PPG channels (3 × 6 bytes = 18 bytes, 48-bit signed big-endian)
    // PPG Red
    bsp_packet[idx++] = (ppg_red >> 40) & 0xFF;
    bsp_packet[idx++] = (ppg_red >> 32) & 0xFF;
    bsp_packet[idx++] = (ppg_red >> 24) & 0xFF;
    bsp_packet[idx++] = (ppg_red >> 16) & 0xFF;
    bsp_packet[idx++] = (ppg_red >> 8) & 0xFF;
    bsp_packet[idx++] = ppg_red & 0xFF;
    // PPG IR
    bsp_packet[idx++] = (ppg_ir >> 40) & 0xFF;
    bsp_packet[idx++] = (ppg_ir >> 32) & 0xFF;
    bsp_packet[idx++] = (ppg_ir >> 24) & 0xFF;
    bsp_packet[idx++] = (ppg_ir >> 16) & 0xFF;
    bsp_packet[idx++] = (ppg_ir >> 8) & 0xFF;
    bsp_packet[idx++] = ppg_ir & 0xFF;
    // PPG Green
    bsp_packet[idx++] = (ppg_green >> 40) & 0xFF;
    bsp_packet[idx++] = (ppg_green >> 32) & 0xFF;
    bsp_packet[idx++] = (ppg_green >> 24) & 0xFF;
    bsp_packet[idx++] = (ppg_green >> 16) & 0xFF;
    bsp_packet[idx++] = (ppg_green >> 8) & 0xFF;
    bsp_packet[idx++] = ppg_green & 0xFF;
    
    // Temperature (2 bytes, 16-bit signed big-endian, centi-degrees)
    bsp_packet[idx++] = (temperature >> 8) & 0xFF;
    bsp_packet[idx++] = temperature & 0xFF;
    
    // Battery (2 bytes, 16-bit unsigned big-endian, millivolts)
    bsp_packet[idx++] = (battery_mv >> 8) & 0xFF;
    bsp_packet[idx++] = battery_mv & 0xFF;
    
    // Sync signal (2 bytes, 16-bit)
    int16_t sync_val = (int16_t) out_state;
    bsp_packet[idx++] = (sync_val >> 8) & 0xFF;
    bsp_packet[idx++] = sync_val & 0xFF;
    
    // Counter (1 byte)
    bsp_packet[idx++] = bsp_counter++;
    
    // Checksum (1 byte) - XOR of all previous bytes
    uint8_t checksum = 0;
    for (int i = 0; i < idx; i++) {
        checksum ^= bsp_packet[i];
    }
    bsp_packet[idx++] = checksum;
    
    // Footer (2 bytes)
    bsp_packet[idx++] = BSP_FOOTER_1;
    bsp_packet[idx++] = BSP_FOOTER_2;
    
    // Send packet via USB serial
    Serial.write(bsp_packet, BSP_PACKET_SIZE);
}

void ADS1299_dataReady_ISR() {
    SPI.beginTransaction(ADS1299_SPI_settings);
    delayNanoseconds(10);
    digitalWriteFast(ADS1299_CS1, LOW);
    delayNanoseconds(6); // CS low to first serial clock, at DVDD > 2.7V
    SPI.transfer(0x00, 3); // Skip status registers
    for(int i = 0; i < N_CH; i++) {
        int32_t sample = 0;
        sample = SPI.transfer(0x00);
        sample = (sample<<8) | SPI.transfer(0x00);
        sample = (sample<<8) | SPI.transfer(0x00);
        sample = (sample & 0x800000) ? (sample | 0xFF000000) : sample; // Sign
        buffer[i] = sample;
    }
    delayMicroseconds(2); // Final serial clock falling edge to CS high: t(CLK) * 4 = 2us
    digitalWriteFast(ADS1299_CS1, HIGH);
    SPI.endTransaction();
    ADS1299_data_ready = true;

    // Accelerometer samples
    if (accel_sampling_divider == 0) {
      int16_t x, y, z;
      ADXL_read(&x, &y, &z);
      buffer[N_CH+0] = (int32_t) x;
      buffer[N_CH+1] = (int32_t) y;
      buffer[N_CH+2] = (int32_t) z;
      accel_sampling_divider = ((1000/200) - 1);
    } else {
      accel_sampling_divider--;
    }

    // Sync signal
    buffer[N_CH+3] = out_state;
}

void RESET(const int CS) {
  SPI.beginTransaction(ADS1299_SPI_settings);
  digitalWrite(CS, LOW);
  delayNanoseconds(6); // CS low to first serial clock, at DVDD > 2.7V
  SPI.transfer(0x06); // RESET
  delayMicroseconds(2); // Final serial clock falling edge to CS high: t(CLK) * 4 = 2us
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
}

void SDATAC(const int CS) {
  SPI.beginTransaction(ADS1299_SPI_settings);
  digitalWrite(CS, LOW);
  delayNanoseconds(6); // CS low to first serial clock, at DVDD > 2.7V
  SPI.transfer(0x11); // SDATAC
  delayMicroseconds(2); // Final serial clock falling edge to CS high: t(CLK) * 4 = 2us
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
}

void RDATAC(const int CS) {
  SPI.beginTransaction(ADS1299_SPI_settings);
  digitalWrite(CS, LOW);
  delayNanoseconds(6); // CS low to first serial clock, at DVDD > 2.7V
  SPI.transfer(0x10); // RDATAC
  delayMicroseconds(2); // Final serial clock falling edge to CS high: t(CLK) * 4 = 2us
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
}

void WREG(const int CS, uint8_t ADDRESS, uint8_t BYTE) {
  SPI.beginTransaction(ADS1299_SPI_settings);
  digitalWrite(CS, LOW);
  delayNanoseconds(6); // CS low to first serial clock, at DVDD > 2.7V
  SPI.transfer(ADDRESS + 0x40);
  delayMicroseconds(2); // Wait at least t(Serial decode) = t(CLK) * 4 = 2us between bytes
  SPI.transfer(0x00); // Notifying to write 1 byte
  delayMicroseconds(2); // Wait at least t(Serial decode) = t(CLK) * 4 = 2us between bytes
  SPI.transfer(BYTE);
  delayMicroseconds(2); // Final serial clock falling edge to CS high: t(CLK) * 4 = 2us
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
}

uint8_t RREG(const int CS, uint8_t ADDRESS) {
  uint8_t val = 0;
  SPI.beginTransaction(ADS1299_SPI_settings);
  digitalWrite(CS, LOW);
  delayNanoseconds(6); // CS low to first serial clock, at DVDD > 2.7V
  SPI.transfer(ADDRESS + 0x20);
  delayMicroseconds(2); // Wait at least t(Serial decode) = t(CLK) * 4 = 2us between bytes
  SPI.transfer(0x00); // Requesting 1 byte
  delayMicroseconds(2); // Wait at least t(Serial decode) = t(CLK) * 4 = 2us between bytes
  val = SPI.transfer(0x00); // Produce clock for receiving data by transmitting empty byte
  delayMicroseconds(2); // Final serial clock falling edge to CS high: t(CLK) * 4 = 2us
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
  return val;
}

int logData(int32_t* x, int frame_size) {
  // Amount of data in ringBuf.
  uint64_t n = rb.bytesUsed();

  // Less than one full frame's worth of space left in file
  if ((n + file.curPosition()) > ((uint64_t) LOG_FILE_SIZE - frame_size*4)) {
    return 1;
  }

  if (n >= 512 && !file.isBusy()) {
    // Not busy only allows one sector before possible busy wait
    // Write one sector from RingBuf to file
    if (512 != rb.writeOut(512)) {
      return 1;  // Writeout failed
    }
  }

  rb.write((uint8_t*) x, frame_size*4);

  // Check for write error from too few free bytes in RingBuf
  if (rb.getWriteError()) {
    Serial.println("rb write error");
    return 1;
  }

  // Logging completed, passed all checks
  return 0;
}

void setup() {
  // Set CPU clock at ~150MHz
  set_arm_clock(151.2E6);

  // USB Serial for BioSerial-Pro streaming to Open Ephys
  Serial.begin(USB_BAUD_RATE);
  while (!Serial && millis() < 3000) { } // Wait up to 3s for USB connection
  
  SPI.begin();

  // Sync setup
  pinMode(SYNC, OUTPUT);
  digitalWriteFast(SYNC, LOW);

  // SIMULATION MODE: Skip hardware init, just start streaming
  if (SIMULATION_MODE) {
    Serial.println("=== SIMULATION MODE ===");
    Serial.println("Generating synthetic data for all sensors");
    Serial.println("EEG: 5ch (3,6,10,20,40 Hz sine waves)");
    Serial.println("Accel: 3ch (gentle motion)");
    Serial.println("PPG: 3ch (72 BPM heartbeat)");
    Serial.println("Temp: 25C +/- 0.5C drift");
    Serial.println("Battery: 4.2V cycling");
    Serial.println("Starting in 1 second...");
    delay(1000);
    Serial.println("Streaming at 1 kHz...");
    return;  // Skip all hardware setup
  }

  // ADXL Setup
  pinMode(ADXL_CS, OUTPUT);
  digitalWriteFast(ADXL_CS, HIGH);
  ADXL_init();

  // ADS1299 Setup

  // 
  pinMode(START, OUTPUT);
  digitalWrite(START, LOW);
  pinMode(N_PWDN, OUTPUT);
  digitalWrite(N_PWDN, LOW);

  //
  pinMode(N_DRDY, INPUT);

  // SPI
  pinMode(ADS1299_CS1, OUTPUT);
  pinMode(ADS1299_CS2, OUTPUT);
  pinMode(ADS1299_CS3, OUTPUT);
  digitalWriteFast(ADS1299_CS1, HIGH);
  digitalWriteFast(ADS1299_CS2, HIGH);
  digitalWriteFast(ADS1299_CS3, HIGH);

  // ------------------------------
  // ADS1299 Startup sequence BEGIN

  // Set N_PWDN = 1 and N_RESET = 1
  digitalWrite(N_PWDN, HIGH);

  // Wait at least t(Power on Reset) = t(CLK) * 2^18 = 128ms
  delay(128); 

  // Issue reset command
  RESET(ADS1299_CS1);

  // Wait at least t(CLK) * 18 = 9us for reset to take effect
  delayMicroseconds(9);

  // Send SDATAC command (Device wakes up in RDATAC mode after reset)
  SDATAC(ADS1299_CS1);

  // @SETTING
  // Send command for internal reference
  WREG(ADS1299_CS1, 0x01, 0x90 | 0x20 | FS_1k); // CONFIG1, enable clock output, 1000Hz sampling rate

  WREG(ADS1299_CS1, 0x03, 0xE0); // CONFIG3, enable internal reference buffer, diable BIAS measurement

  // Wait for internal reference to settle
  delay(10); // @TODO: measure minimum acceptable delay later

  // Read deivce ID
  Serial.println(RREG(ADS1299_CS1, 0x00));
  // @SETTING FOR RECORDING TEST
  // Send register settings

  /*
  WREG(ADS1299_CS1, 0x02, 0xD0); // CONFIG2, enable test source at 1Hz, small amplitude

  WREG(ADS1299_CS1, 0x05, GAIN_24x | TEST); // CH1SET
  WREG(ADS1299_CS1, 0x06, GAIN_24x | TEST); // CH2SET
  WREG(ADS1299_CS1, 0x07, GAIN_24x | TEST); // CH3SET
  WREG(ADS1299_CS1, 0x08, GAIN_24x | TEST); // CH4SET
  WREG(ADS1299_CS1, 0x09, GAIN_24x | TEST); // CH5SET
  WREG(ADS1299_CS1, 0x0A, GAIN_24x | TEST); // CH6SET
  WREG(ADS1299_CS1, 0x0B, GAIN_24x | TEST); // CH7SET
  WREG(ADS1299_CS1, 0x0C, GAIN_24x | TEST); // CH8SET
  */

  // @SETTING FOR RECORDING REAL INPUT
  WREG(ADS1299_CS1, 0x05, GAIN_24x | CHSET_INPUT); // CH1SET
  WREG(ADS1299_CS1, 0x06, GAIN_24x | CHSET_INPUT); // CH2SET
  WREG(ADS1299_CS1, 0x07, GAIN_24x | CHSET_INPUT); // CH3SET
  WREG(ADS1299_CS1, 0x08, GAIN_24x | CHSET_INPUT); // CH4SET
  WREG(ADS1299_CS1, 0x09, GAIN_24x | CHSET_INPUT); // CH5SET
  WREG(ADS1299_CS1, 0x0A, GAIN_24x | CHSET_INPUT); // CH6SET
  WREG(ADS1299_CS1, 0x0B, GAIN_24x | CHSET_INPUT); // CH7SET
  WREG(ADS1299_CS1, 0x0C, GAIN_24x | CHSET_INPUT); // CH8SET

  // SD Card
  while (!sd.begin(SdioConfig(FIFO_SDIO))) {
    Serial.println("Card not present");
    delay(1000); // 1s
  }

  // Open or create file - truncate existing file
  if (!file.open(LOG_FILENAME, O_RDWR | O_CREAT | O_TRUNC)) {
    Serial.println("File open failed");
    return; // @TODO
  }
  // File must be pre-allocated to avoid huge
  // delays searching for free clusters
  if (!file.preAllocate(LOG_FILE_SIZE)) {
    Serial.println("File preallocation failed");
    file.close();
    return; // @TODO
  }
  // Initialize the ring buffer
  rb.begin(&file);
  
  // Activate conversion
  digitalWrite(START, HIGH);
  
  // Return device to RDATAC mode
  RDATAC(ADS1299_CS1);

  // ADS1299 Startup sequence DONE
  // -----------------------------

  Serial.println("Setup done");
  
  // @SETTING WAIT THIS LONG BEFORE RECORDING (milliseconds)
  delay(3000);
  
  Serial.println("Starting");
  attachInterrupt(digitalPinToInterrupt(N_DRDY), ADS1299_dataReady_ISR, FALLING);
}


void ADXL_init() {
  SPI.beginTransaction(ADXL_settings);

  // Data format
  digitalWriteFast(ADXL_CS, LOW);
  delayNanoseconds(5); // CS low to first serial clock
  SPI.transfer(DATA_FORMAT | ADXL_WRITE);
  SPI.transfer(0x0B); // 4-pin SPI, full scale (+/-16g), active high interrupts
  delayNanoseconds(10); // Final serial clock falling edge to CS high
  digitalWriteFast(ADXL_CS, HIGH);

  delayNanoseconds(150); // Minimum CS deassertion between commands
  
  // Sampling rate
  digitalWriteFast(ADXL_CS, LOW);
  delayNanoseconds(5); // CS low to first serial clock
  SPI.transfer(BW_RATE | ADXL_WRITE);
  SPI.transfer(0x0B); // 200Hz sampling rate, normal power mode (low noise)
  delayNanoseconds(10); // Final serial clock falling edge to CS high
  digitalWriteFast(ADXL_CS, HIGH);

  delayNanoseconds(150); // Minimum CS deassertion between commands
  
  // Power state
  digitalWriteFast(ADXL_CS, LOW);
  delayNanoseconds(5); // CS low to first serial clock
  SPI.transfer(POWER_CTL | ADXL_WRITE);
  SPI.transfer(0b00101000); // Measurement active 
  delayNanoseconds(10); // Final serial clock falling edge to CS high
  digitalWriteFast(ADXL_CS, HIGH);

  SPI.endTransaction();
}

void ADXL_read(int16_t *x, int16_t *y, int16_t *z) {
  SPI.beginTransaction(ADXL_settings);
  digitalWriteFast(ADXL_CS, LOW);
  delayNanoseconds(5); // CS low to first serial clock
  SPI.transfer(DATAX0 | ADXL_READ_MULT);
  *x = (int16_t) SPI.transfer(0) | ((int16_t) SPI.transfer(0) << 8);
  *y = (int16_t) SPI.transfer(0) | ((int16_t) SPI.transfer(0) << 8);
  *z = (int16_t) SPI.transfer(0) | ((int16_t) SPI.transfer(0) << 8);
  delayNanoseconds(10); // Final serial clock falling edge to CS high
  digitalWriteFast(ADXL_CS, HIGH);
  SPI.endTransaction();
}

void loop() {
  static int counter = 0;
  static unsigned long last_sample_time = 0;
  
  // SIMULATION MODE: Generate data at 1kHz without hardware
  if (SIMULATION_MODE) {
    unsigned long now = micros();
    if (now - last_sample_time >= 1000) {  // 1000 µs = 1 kHz
      last_sample_time = now;
      
      // Generate all simulated sensor data
      generateSimulatedData();
      
      // Send via USB
      sendBioSerialProPacket();
      
      counter++;
      if (counter >= REC_SEC * 1000) {
        Serial.println("Simulation complete");
        while(1);
      }
    }
  }
  // HARDWARE MODE: Wait for ADS1299 interrupt
  else if (ADS1299_data_ready) {
    ADS1299_data_ready = false;
    
    // Send data via USB using BioSerial-Pro protocol
    // Open Ephys plugin reads this -> LSL Outlet plugin streams to LSL
    sendBioSerialProPacket();
    
    if (counter == REC_SEC*1000 || logData(buffer, N_CH+4)) {
      // Should end
      rb.sync();
      file.truncate();
      file.close();

      digitalWrite(SYNC, LOW); // Turn OFF SYNC LED
      digitalWrite(START, LOW); // Amplifier stops recording
      digitalWrite(N_PWDN, LOW); // Power down the amplifiers
      digitalWrite(13, LOW); // Turns OFF the Teensy LED
      Serial.println("Stopping");
      
      while(1);
    }
    else if ((counter % 1000) == 0) {
      file.sync();
    }
    counter++;
  }

  unsigned long current_millis = millis();

  if (current_millis - previous_millis >= next_interval) {
    previous_millis = current_millis;
    if (out_state == LOW) {
      out_state = HIGH;
      next_interval = ON_LENGTH_MS;
    } else {
      out_state = LOW;
      next_interval = random_interval - ON_LENGTH_MS;
      rng_ready = 0; // we used up the random sample
    }
    digitalWrite(SYNC, out_state);
    // make sure this is run last, so does not delay timing
    if (!rng_ready) {
      xorshift32(&xorshift_state);
      random_interval = INTERVAL_MIN_MS + ((((uint64_t) xorshift_state) * (INTERVAL_MAX_MS - INTERVAL_MIN_MS)) >> 32);
      rng_ready = 1;
    }
  }
}
