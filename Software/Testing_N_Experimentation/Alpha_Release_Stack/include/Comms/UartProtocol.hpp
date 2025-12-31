/*****************************************************************
 * File:      UartProtocol.hpp
 * Category:  include/Comms
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Shared UART communication protocol definitions between
 *    CPU and GPU. Defines message structure, types, and
 *    common constants for high-speed serial communication.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_COMMS_UART_PROTOCOL_HPP_
#define ARCOS_INCLUDE_COMMS_UART_PROTOCOL_HPP_

#include <stdint.h>

namespace arcos::comms{

// ============================================================
// UART Configuration
// ============================================================

/** UART baud rate - 10 Mbps for high-speed data transfer */
constexpr uint32_t UART_BAUD_RATE = 10000000;

/** Maximum payload size in bytes */
constexpr uint16_t MAX_PAYLOAD_SIZE = 512;

/** UART buffer sizes - large buffers for high-speed transfer */
constexpr int UART_TX_BUFFER_SIZE = 8192;
constexpr int UART_RX_BUFFER_SIZE = 16384;

// ============================================================
// Pin Definitions
// ============================================================

/** CPU UART pins (connects to GPU) */
namespace cpu{
  constexpr int UART_RX_PIN = 11;  // CPU RX <- GPU TX
  constexpr int UART_TX_PIN = 12;  // CPU TX -> GPU RX
}

/** GPU UART pins (connects to CPU) */
namespace gpu{
  constexpr int UART_TX_PIN = 12;  // GPU TX -> CPU RX (GPIO 11)
  constexpr int UART_RX_PIN = 13;  // GPU RX <- CPU TX (GPIO 12)
}

// ============================================================
// Message Protocol
// ============================================================

/** Message frame markers */
constexpr uint8_t MSG_START_BYTE = 0xAA;
constexpr uint8_t MSG_END_BYTE = 0x55;

/** Message types */
enum class MsgType : uint8_t{
  // System messages (0x00 - 0x0F)
  PING        = 0x01,  // Request echo response
  PONG        = 0x02,  // Echo response with timestamp
  HEARTBEAT   = 0x03,  // Keep-alive signal
  ACK         = 0x04,  // Acknowledge receipt
  NACK        = 0x05,  // Negative acknowledge
  
  // Data messages (0x10 - 0x1F)
  DATA        = 0x10,  // Generic data payload
  COMMAND     = 0x11,  // Command from CPU to GPU
  STATUS      = 0x12,  // Status update
  
  // Display messages (0x20 - 0x2F)
  FRAME_DATA  = 0x20,  // Frame buffer data
  FRAME_SYNC  = 0x21,  // Frame synchronization
  
  // Error messages (0xF0 - 0xFF)
  ERROR       = 0xF0,  // Error notification
};

/** Message header structure
 * 
 * Frame format: [START][TYPE][LEN_L][LEN_H][DATA...][CHECKSUM][END]
 * 
 * - START: 0xAA (1 byte)
 * - TYPE: Message type (1 byte)
 * - LEN: Payload length (2 bytes, little-endian)
 * - DATA: Payload (0-512 bytes)
 * - CHECKSUM: XOR of TYPE, LEN, and DATA (1 byte)
 * - END: 0x55 (1 byte)
 */
struct MsgHeader{
  uint8_t start;
  uint8_t type;
  uint16_t length;
};

/** Calculate checksum for message
 * @param type Message type byte
 * @param data Pointer to payload data
 * @param len Length of payload
 * @return XOR checksum byte
 */
inline uint8_t calculateChecksum(uint8_t type, const uint8_t* data, uint16_t len){
  uint8_t checksum = type;
  checksum ^= (len & 0xFF);
  checksum ^= ((len >> 8) & 0xFF);
  for(uint16_t i = 0; i < len; i++){
    checksum ^= data[i];
  }
  return checksum;
}

/** Verify message checksum
 * @param type Message type byte
 * @param data Pointer to payload data
 * @param len Length of payload
 * @param expected Expected checksum value
 * @return true if checksum matches
 */
inline bool verifyChecksum(uint8_t type, const uint8_t* data, uint16_t len, uint8_t expected){
  return calculateChecksum(type, data, len) == expected;
}

} // namespace arcos::comms

#endif // ARCOS_INCLUDE_COMMS_UART_PROTOCOL_HPP_
