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
constexpr int BAUD_RATE = 921600;  // Increased from 115200 for higher throughput

/** Message types for bidirectional communication */
enum class MessageType : uint8_t{
  PING = 0x01,           // Heartbeat/connection check
  PONG = 0x02,           // Response to ping
  DATA_REQUEST = 0x10,   // Request data from peer
  DATA_RESPONSE = 0x11,  // Response with data
  COMMAND = 0x20,        // Send command to peer
  ACK = 0x30,            // Acknowledge received message
  NACK = 0x31,           // Negative acknowledgment
  STATUS = 0x40,         // Status update
  ERROR = 0xE0           // Error notification
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
