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
