/*****************************************************************
 * File:      UartBidirectionalProtocol.h
 * Category:  communication/protocols
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Defines bidirectional UART communication protocol for CPU-GPU
 *    communication with message framing, checksums, and packet types.
 *****************************************************************/

#ifndef ARCOS_COMMUNICATION_PROTOCOLS_UART_BIDIRECTIONAL_PROTOCOL_H_
#define ARCOS_COMMUNICATION_PROTOCOLS_UART_BIDIRECTIONAL_PROTOCOL_H_

#include <stdint.h>

namespace arcos::communication{

/** Protocol constants */
constexpr uint8_t UART_START_BYTE = 0xAA;
constexpr uint8_t UART_END_BYTE = 0x55;
constexpr int MAX_PAYLOAD_SIZE = 256;
constexpr int BAUD_RATE = 2000000;  // Testing maximum baud rate (2 Mbps)

/** Message types for bidirectional communication */
enum class MessageType : uint8_t{
  PING = 0x01,           // Heartbeat/connection check
  PONG = 0x02,           // Response to ping
  DATA_REQUEST = 0x10,   // Request data from peer
  DATA_RESPONSE = 0x11,  // Response with data
  SENSOR_DATA = 0x12,    // Sensor data frame from CPU
  LED_DATA = 0x13,       // LED RGBW data frame from GPU
  COMMAND = 0x20,        // Send command to peer
  ACK = 0x30,            // Acknowledge received message
  NACK = 0x31,           // Negative acknowledgment
  STATUS = 0x40,         // Status update
  ERROR = 0xE0           // Error notification
};

/** Packed sensor data payload structure (CPU -> GPU)
 * Optimized for 60Hz transmission with bit-packed flags
 */
struct __attribute__((packed)) SensorDataPayload{
  // IMU Data (9 floats = 36 bytes)
  float accel_x, accel_y, accel_z;           // Accelerometer (g)
  float gyro_x, gyro_y, gyro_z;              // Gyroscope (deg/s)
  float mag_x, mag_y, mag_z;                 // Magnetometer (μT)
  
  // Environmental Data (3 floats = 12 bytes)
  float temperature;                         // °C
  float humidity;                            // %
  float pressure;                            // Pa
  
  // GPS Data (26 bytes)
  float latitude;                            // Decimal degrees
  float longitude;                           // Decimal degrees
  float altitude;                            // Meters
  float speed_knots;                         // Speed in knots
  float course;                              // Course in degrees
  uint8_t gps_satellites;                    // Number of satellites
  uint8_t gps_hour;                          // UTC hour
  uint8_t gps_minute;                        // UTC minute
  uint8_t gps_second;                        // UTC second
  uint8_t gps_flags;                         // Bit-packed: [7:4]=reserved, [3:2]=fix_quality, [1]=valid, [0]=reserved
  uint8_t _reserved_gps;                     // Alignment padding
  
  // Microphone Data (13 bytes)
  int32_t mic_current_sample;                // Current sample
  int32_t mic_peak_amplitude;                // Peak amplitude
  float mic_db_level;                        // dB level
  uint8_t mic_flags;                         // Bit-packed: [7:1]=reserved, [0]=clipping
  
  // Button States (1 byte)
  uint8_t button_flags;                      // Bit-packed: [7:4]=reserved, [3]=D, [2]=C, [1]=B, [0]=A
  
  // Validity Flags (1 byte)
  uint8_t sensor_valid_flags;                // Bit-packed: [7:4]=reserved, [3]=mic, [2]=gps, [1]=env, [0]=imu
  
  uint8_t _reserved_padding;                 // Final alignment padding
  
  // Total: 36 + 12 + 26 + 13 + 1 + 1 + 1 = 90 bytes (was 120 bytes, saved 30 bytes!)
  
  // Helper methods for bit manipulation
  
  // GPS flags helpers
  inline uint8_t getGpsFixQuality() const{ return (gps_flags >> 2) & 0x03; }
  inline bool getGpsValid() const{ return (gps_flags >> 1) & 0x01; }
  inline void setGpsFixQuality(uint8_t quality){ gps_flags = (gps_flags & 0xF3) | ((quality & 0x03) << 2); }
  inline void setGpsValid(bool valid){ gps_flags = (gps_flags & 0xFD) | ((valid ? 1 : 0) << 1); }
  
  // Microphone flags helpers
  inline bool getMicClipping() const{ return mic_flags & 0x01; }
  inline void setMicClipping(bool clipping){ mic_flags = (mic_flags & 0xFE) | (clipping ? 1 : 0); }
  
  // Button flags helpers
  inline bool getButtonA() const{ return button_flags & 0x01; }
  inline bool getButtonB() const{ return (button_flags >> 1) & 0x01; }
  inline bool getButtonC() const{ return (button_flags >> 2) & 0x01; }
  inline bool getButtonD() const{ return (button_flags >> 3) & 0x01; }
  inline void setButtonA(bool pressed){ button_flags = (button_flags & 0xFE) | (pressed ? 1 : 0); }
  inline void setButtonB(bool pressed){ button_flags = (button_flags & 0xFD) | ((pressed ? 1 : 0) << 1); }
  inline void setButtonC(bool pressed){ button_flags = (button_flags & 0xFB) | ((pressed ? 1 : 0) << 2); }
  inline void setButtonD(bool pressed){ button_flags = (button_flags & 0xF7) | ((pressed ? 1 : 0) << 3); }
  
  // Sensor validity flags helpers
  inline bool getImuValid() const{ return sensor_valid_flags & 0x01; }
  inline bool getEnvValid() const{ return (sensor_valid_flags >> 1) & 0x01; }
  inline bool getGpsValidFlag() const{ return (sensor_valid_flags >> 2) & 0x01; }
  inline bool getMicValid() const{ return (sensor_valid_flags >> 3) & 0x01; }
  inline void setImuValid(bool valid){ sensor_valid_flags = (sensor_valid_flags & 0xFE) | (valid ? 1 : 0); }
  inline void setEnvValid(bool valid){ sensor_valid_flags = (sensor_valid_flags & 0xFD) | ((valid ? 1 : 0) << 1); }
  inline void setGpsValidFlag(bool valid){ sensor_valid_flags = (sensor_valid_flags & 0xFB) | ((valid ? 1 : 0) << 2); }
  inline void setMicValid(bool valid){ sensor_valid_flags = (sensor_valid_flags & 0xF7) | ((valid ? 1 : 0) << 3); }
};

/** LED strip configuration for the robot head
 * - Strip 1 (Left Fin): 13 LEDs - GPIO 18
 * - Strip 2 (Tongue): 9 LEDs - GPIO 8
 * - Strip 4 (Right Fin): 13 LEDs - GPIO 38
 * - Strip 5 (Scale): 14 LEDs - GPIO 37
 * Total: 49 LEDs
 */
constexpr uint16_t LED_COUNT_LEFT_FIN = 13;
constexpr uint16_t LED_COUNT_TONGUE = 9;
constexpr uint16_t LED_COUNT_RIGHT_FIN = 13;
constexpr uint16_t LED_COUNT_SCALE = 14;
constexpr uint16_t LED_COUNT_TOTAL = LED_COUNT_LEFT_FIN + LED_COUNT_TONGUE + LED_COUNT_RIGHT_FIN + LED_COUNT_SCALE; // 49

constexpr uint16_t LED_OFFSET_LEFT_FIN = 0;
constexpr uint16_t LED_OFFSET_TONGUE = LED_OFFSET_LEFT_FIN + LED_COUNT_LEFT_FIN;     // 13
constexpr uint16_t LED_OFFSET_RIGHT_FIN = LED_OFFSET_TONGUE + LED_COUNT_TONGUE;      // 22
constexpr uint16_t LED_OFFSET_SCALE = LED_OFFSET_RIGHT_FIN + LED_COUNT_RIGHT_FIN;    // 35

/** RGBW color structure (4 bytes per LED) */
struct __attribute__((packed)) RgbwColor{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
  
  RgbwColor() : r(0), g(0), b(0), w(0) {}
  RgbwColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t white = 0)
    : r(red), g(green), b(blue), w(white) {}
};

/** LED data payload structure (GPU -> CPU)
 * Flattened array of all LEDs in strip order + fan control
 */
struct __attribute__((packed)) LedDataPayload{
  RgbwColor leds[LED_COUNT_TOTAL];  // 49 LEDs × 4 bytes = 196 bytes
  uint8_t fan_speed;                // Fan PWM speed (0-255): 0=off, 255=full speed
  uint8_t _reserved[3];             // Reserved for future use (alignment to 200 bytes)
  
  // Total: 196 + 1 + 3 = 200 bytes
  
  // Helper methods to access LED strips
  inline RgbwColor* getLeftFinLeds(){ return &leds[LED_OFFSET_LEFT_FIN]; }
  inline RgbwColor* getTongueLeds(){ return &leds[LED_OFFSET_TONGUE]; }
  inline RgbwColor* getRightFinLeds(){ return &leds[LED_OFFSET_RIGHT_FIN]; }
  inline RgbwColor* getScaleLeds(){ return &leds[LED_OFFSET_SCALE]; }
  
  inline const RgbwColor* getLeftFinLeds() const{ return &leds[LED_OFFSET_LEFT_FIN]; }
  inline const RgbwColor* getTongueLeds() const{ return &leds[LED_OFFSET_TONGUE]; }
  inline const RgbwColor* getRightFinLeds() const{ return &leds[LED_OFFSET_RIGHT_FIN]; }
  inline const RgbwColor* getScaleLeds() const{ return &leds[LED_OFFSET_SCALE]; }
  
  // Set entire strip to a single color
  void setLeftFinColor(const RgbwColor& color){
    for(uint16_t i = 0; i < LED_COUNT_LEFT_FIN; i++) leds[LED_OFFSET_LEFT_FIN + i] = color;
  }
  void setTongueColor(const RgbwColor& color){
    for(uint16_t i = 0; i < LED_COUNT_TONGUE; i++) leds[LED_OFFSET_TONGUE + i] = color;
  }
  void setRightFinColor(const RgbwColor& color){
    for(uint16_t i = 0; i < LED_COUNT_RIGHT_FIN; i++) leds[LED_OFFSET_RIGHT_FIN + i] = color;
  }
  void setScaleColor(const RgbwColor& color){
    for(uint16_t i = 0; i < LED_COUNT_SCALE; i++) leds[LED_OFFSET_SCALE + i] = color;
  }
  
  // Set all LEDs to a single color
  void setAllColor(const RgbwColor& color){
    for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++) leds[i] = color;
  }
};

/** Message packet structure */
struct UartPacket{
  uint8_t start_byte;              // Always UART_START_BYTE
  MessageType message_type;        // Type of message
  uint8_t payload_length;          // Length of payload (0-255)
  uint8_t payload[MAX_PAYLOAD_SIZE]; // Message payload
  uint8_t checksum;                // Simple XOR checksum
  uint8_t end_byte;                // Always UART_END_BYTE
  
  /** Calculate checksum for packet */
  uint8_t calculateChecksum() const{
    uint8_t sum = static_cast<uint8_t>(message_type) ^ payload_length;
    for(int i = 0; i < payload_length; i++){
      sum ^= payload[i];
    }
    return sum;
  }
  
  /** Validate packet integrity */
  bool isValid() const{
    return (start_byte == UART_START_BYTE) &&
           (end_byte == UART_END_BYTE) &&
           (checksum == calculateChecksum());
  }
};

/** Interface for UART bidirectional communication */
class IUartBidirectional{
public:
  virtual ~IUartBidirectional() = default;
  
  /** Initialize UART communication
   * @param baud_rate Baud rate for communication
   * @return true if successful
   */
  virtual bool init(int baud_rate = BAUD_RATE) = 0;
  
  /** Send a packet
   * @param type Message type
   * @param payload Data to send
   * @param length Length of payload
   * @return true if sent successfully
   */
  virtual bool sendPacket(MessageType type, const uint8_t* payload, uint8_t length) = 0;
  
  /** Receive a packet (non-blocking)
   * @param packet Output packet structure
   * @return true if packet received and valid
   */
  virtual bool receivePacket(UartPacket& packet) = 0;
  
  /** Check if data is available
   * @return Number of bytes available
   */
  virtual int available() = 0;
  
  /** Send ping message
   * @return true if sent successfully
   */
  virtual bool sendPing() = 0;
  
  /** Send acknowledgment
   * @param ack_data Optional acknowledgment data
   * @return true if sent successfully
   */
  virtual bool sendAck(uint8_t ack_data = 0) = 0;
  
  /** Process incoming messages
   * Should be called regularly in loop
   */
  virtual void update() = 0;
};

} // namespace arcos::communication

#endif // ARCOS_COMMUNICATION_PROTOCOLS_UART_BIDIRECTIONAL_PROTOCOL_H_
