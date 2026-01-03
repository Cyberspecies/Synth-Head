/*****************************************************************
 * File:      UartProtocol.hpp
 * Category:  Communication Protocol
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    UART protocol definitions for CPU-GPU display streaming.
 *    Supports HUB75 (128x32 RGB) and OLED (128x128 mono) frames.
 * 
 * Bandwidth Requirements:
 *    - HUB75: 128x32x3 = 12,288 bytes @ 60fps = 5.9 Mbps
 *    - OLED:  128x128/8 = 2,048 bytes @ 15fps = 0.25 Mbps
 *    - Total: ~6.2 Mbps + protocol overhead
 *    - Recommended baud: 8,000,000 (8 Mbps)
 *****************************************************************/

#ifndef ARCOS_COMMS_UART_PROTOCOL_HPP_
#define ARCOS_COMMS_UART_PROTOCOL_HPP_

#include <stdint.h>

namespace arcos::comms {

// ============================================================
// Protocol Constants
// ============================================================

constexpr uint8_t SYNC_BYTE_1 = 0xAA;
constexpr uint8_t SYNC_BYTE_2 = 0x55;
constexpr uint8_t SYNC_BYTE_3 = 0xCC;

// Baud rate - 10 Mbps for reliable 1KB fragmented streaming
constexpr uint32_t UART_BAUD_RATE = 10000000;

// Fragment configuration (tested optimal: 1KB @ 10 Mbps = 100% reliable)
constexpr uint16_t FRAGMENT_SIZE = 1024;  // 1KB packets
constexpr uint8_t HUB75_FRAGMENT_COUNT = 12;  // 12KB / 1KB = 12 fragments
constexpr uint8_t OLED_FRAGMENT_COUNT = 2;   // 2KB / 1KB = 2 fragments
constexpr uint8_t MAX_RETRIES = 3;  // Max retries per fragment
constexpr uint32_t ACK_TIMEOUT_US = 2000;  // 2ms ACK timeout (ACK ~13 bytes = 13μs @ 10Mbps)

// Streaming mode: 0 = wait for ACK per fragment, 1 = stream all then verify
constexpr bool STREAMING_MODE = true;  // True = no per-fragment ACK (faster)

// Display dimensions
constexpr uint16_t HUB75_WIDTH = 128;
constexpr uint16_t HUB75_HEIGHT = 32;
constexpr uint32_t HUB75_RGB_SIZE = HUB75_WIDTH * HUB75_HEIGHT * 3;  // 12,288 bytes

constexpr uint16_t OLED_WIDTH = 128;
constexpr uint16_t OLED_HEIGHT = 128;
constexpr uint32_t OLED_MONO_SIZE = (OLED_WIDTH * OLED_HEIGHT) / 8;  // 2,048 bytes

// Frame rates
constexpr uint8_t HUB75_TARGET_FPS = 60;
constexpr uint8_t HUB75_MIN_FPS = 30;
constexpr uint8_t OLED_TARGET_FPS = 15;
constexpr uint8_t OLED_MIN_FPS = 10;

// Maximum packet payload (fragment size for transfers)
constexpr uint16_t MAX_PAYLOAD_SIZE = FRAGMENT_SIZE;

// ============================================================
// Message Types
// ============================================================

enum class MsgType : uint8_t {
  // Control messages (0x0X)
  PING          = 0x01,
  PONG          = 0x02,
  ACK           = 0x03,
  NACK          = 0x04,
  STATUS        = 0x05,
  FRAME_REQUEST = 0x06,  // GPU requests a frame from CPU
  RESEND_FRAG   = 0x07,  // GPU requests fragment resend
  
  // Display frames (0x1X)
  HUB75_FRAME   = 0x10,  // Full HUB75 RGB frame (12,288 bytes) - legacy
  HUB75_FRAG    = 0x11,  // HUB75 frame fragment (1KB)
  OLED_FRAME    = 0x12,  // Full OLED monochrome frame (2,048 bytes) - legacy
  OLED_FRAG     = 0x13,  // OLED frame fragment (1KB)
  
  // Settings (0x2X)
  SET_FPS       = 0x20,  // Change target FPS
  SET_BRIGHTNESS = 0x21, // Set display brightness
  
  // Diagnostics (0x3X)
  STATS_REQUEST = 0x30,
  STATS_RESPONSE = 0x31,
};

// ============================================================
// Packet Header Structure
// ============================================================

#pragma pack(push, 1)

struct PacketHeader {
  uint8_t sync1;        // SYNC_BYTE_1 (0xAA)
  uint8_t sync2;        // SYNC_BYTE_2 (0x55)
  uint8_t sync3;        // SYNC_BYTE_3 (0xCC)
  uint8_t msg_type;     // MsgType
  uint16_t payload_len; // Payload length (little-endian)
  uint16_t frame_num;   // Frame sequence number
  uint8_t frag_index;   // Fragment index (0 for non-fragmented)
  uint8_t frag_total;   // Total fragments (1 for non-fragmented)
  // Total: 10 bytes
};

struct PacketFooter {
  uint16_t checksum;    // CRC16 or simple sum
  uint8_t end_byte;     // 0x55
  // Total: 3 bytes
};

// ============================================================
// Payload Structures
// ============================================================

/** HUB75 full frame (12,288 bytes RGB data) */
struct Hub75FramePayload {
  uint8_t rgb_data[HUB75_RGB_SIZE];
};

/** OLED full frame (2,048 bytes monochrome, 1 bit per pixel) */
struct OledFramePayload {
  uint8_t mono_data[OLED_MONO_SIZE];
};

/** Frame fragment for reliable transfer */
struct FrameFragmentPayload {
  uint16_t offset;      // Byte offset in full frame
  uint16_t length;      // Length of this fragment
  uint8_t data[];       // Variable length data
};

/** Status response */
struct StatusPayload {
  uint32_t uptime_ms;
  uint16_t hub75_fps;   // Actual HUB75 FPS (x10 for 1 decimal)
  uint16_t oled_fps;    // Actual OLED FPS (x10 for 1 decimal)  
  uint16_t frames_rx;   // Frames received
  uint16_t frames_drop; // Frames dropped
  uint8_t hub75_ok;     // HUB75 display status
  uint8_t oled_ok;      // OLED display status
};

/** Ping/Pong payload */
struct PingPayload {
  uint32_t timestamp_us;
  uint16_t seq_num;
};

#pragma pack(pop)

// ============================================================
// Helper Functions
// ============================================================

/** Calculate simple checksum (sum of all bytes) */
inline uint16_t calcChecksum(const uint8_t* data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
  }
  return sum;
}

/** Validate packet header sync bytes */
inline bool validateSync(const PacketHeader& hdr) {
  return (hdr.sync1 == SYNC_BYTE_1) && 
         (hdr.sync2 == SYNC_BYTE_2) && 
         (hdr.sync3 == SYNC_BYTE_3);
}

// ============================================================
// UART Statistics
// ============================================================

struct UartStats {
  uint32_t tx_bytes;
  uint32_t rx_bytes;
  uint32_t tx_frames;
  uint32_t rx_frames;
  uint32_t tx_fragments;     // Total fragments sent
  uint32_t rx_fragments;     // Total fragments received
  uint32_t retries;          // Fragment retries requested
  uint32_t retry_success;    // Successful retries
  uint32_t checksum_errors;
  uint32_t sync_errors;
  uint32_t timeout_errors;   // ACK timeout errors
  uint32_t last_rtt_us;
  uint16_t hub75_fps_actual;  // x10
  uint16_t oled_fps_actual;   // x10
  uint8_t hub75_fps;          // Actual FPS
  uint8_t oled_fps;           // Actual FPS
  
  // Error rate calculation helpers
  float getFragmentErrorRate() const {
    if (tx_fragments == 0) return 0.0f;
    return 100.0f * retries / tx_fragments;
  }
  
  float getChecksumErrorRate() const {
    if (rx_fragments == 0) return 0.0f;
    return 100.0f * checksum_errors / rx_fragments;
  }
};

} // namespace arcos::comms

#endif // ARCOS_COMMS_UART_PROTOCOL_HPP_
