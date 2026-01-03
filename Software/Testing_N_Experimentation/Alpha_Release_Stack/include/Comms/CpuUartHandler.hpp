/*****************************************************************
 * File:      CpuUartHandler.hpp
 * Category:  Communication/CPU
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side UART handler for streaming display frames to GPU.
 *    Sends HUB75 frames at 60fps and OLED frames at 15fps.
 * 
 * Pin Configuration:
 *    - TX: GPIO12
 *    - RX: GPIO11
 * 
 * Bandwidth Requirements:
 *    - HUB75: 128x32x3 = 12,288 bytes @ 60fps = 5.9 Mbps
 *    - OLED:  128x128/8 = 2,048 bytes @ 15fps = 0.25 Mbps
 *    - Total: ~6.2 Mbps + protocol overhead
 *****************************************************************/

#ifndef ARCOS_COMMS_CPU_UART_HANDLER_HPP_
#define ARCOS_COMMS_CPU_UART_HANDLER_HPP_

#include <Arduino.h>
#include "UartProtocol.hpp"

namespace arcos::comms {

class CpuUartHandler {
public:
  CpuUartHandler() : initialized_(false), frame_num_(0), 
                     last_ping_time_(0), pending_pong_(false),
                     has_message_(false), last_msg_type_(MsgType::ACK),
                     pending_frag_idx_(-1), pending_frame_type_(MsgType::ACK),
                     last_frag_send_time_(0) {
    memset(&stats_, 0, sizeof(stats_));
    memset(hub75_frame_cache_, 0, sizeof(hub75_frame_cache_));
    memset(oled_frame_cache_, 0, sizeof(oled_frame_cache_));
  }
  
  /** Initialize UART communication */
  bool init() {
    Serial1.begin(UART_BAUD_RATE, SERIAL_8N1, 11, 12);  // RX=11, TX=12
    Serial1.setRxBufferSize(4096);  // Buffer for ACK/NAK responses
    initialized_ = true;
    return true;
  }
  
  /** Send HUB75 RGB frame as 1KB fragments (128x32x3 = 12,288 bytes = 12 fragments) */
  bool sendFrame(const uint8_t* rgb_data, uint16_t width, uint16_t height, uint16_t frame_num) {
    if (!initialized_ || width != HUB75_WIDTH || height != HUB75_HEIGHT) {
      return false;
    }
    
    // Cache frame for potential retries
    memcpy(hub75_frame_cache_, rgb_data, HUB75_RGB_SIZE);
    cached_hub75_frame_num_ = frame_num;
    
    if (STREAMING_MODE) {
      // Streaming mode: send all fragments without waiting for ACK
      return sendFrameStreaming(MsgType::HUB75_FRAG, hub75_frame_cache_, HUB75_RGB_SIZE,
                                frame_num, HUB75_FRAGMENT_COUNT);
    } else {
      // ACK mode: wait for ACK after each fragment
      bool all_ok = true;
      for (uint8_t frag = 0; frag < HUB75_FRAGMENT_COUNT; frag++) {
        if (!sendFragment(MsgType::HUB75_FRAG, hub75_frame_cache_, HUB75_RGB_SIZE, 
                          frame_num, frag, HUB75_FRAGMENT_COUNT)) {
          all_ok = false;
        }
      }
      
      if (all_ok) {
        stats_.tx_frames++;
        hub75_frames_sent_++;
      }
      
      return all_ok;
    }
  }
  
  /** Send OLED monochrome frame as 1KB fragments (128x128/8 = 2,048 bytes = 2 fragments) */
  bool sendOledFrame(const uint8_t* mono_data, uint16_t frame_num) {
    if (!initialized_) return false;
    
    // Cache frame for potential retries
    memcpy(oled_frame_cache_, mono_data, OLED_MONO_SIZE);
    cached_oled_frame_num_ = frame_num;
    
    if (STREAMING_MODE) {
      // Streaming mode: send all fragments without waiting for ACK
      return sendFrameStreaming(MsgType::OLED_FRAG, oled_frame_cache_, OLED_MONO_SIZE,
                                frame_num, OLED_FRAGMENT_COUNT);
    } else {
      // ACK mode: wait for ACK after each fragment
      bool all_ok = true;
      for (uint8_t frag = 0; frag < OLED_FRAGMENT_COUNT; frag++) {
        if (!sendFragment(MsgType::OLED_FRAG, oled_frame_cache_, OLED_MONO_SIZE,
                          frame_num, frag, OLED_FRAGMENT_COUNT)) {
          all_ok = false;
        }
      }
      
      if (all_ok) {
        stats_.tx_frames++;
        oled_frames_sent_++;
      }
      
      return all_ok;
    }
  }
  
  /** Send frame in streaming mode (no per-fragment ACK wait) */
  bool sendFrameStreaming(MsgType type, const uint8_t* frame_data, uint32_t frame_size,
                          uint16_t frame_num, uint8_t frag_count) {
    // Send all fragments back-to-back
    for (uint8_t frag = 0; frag < frag_count; frag++) {
      uint32_t offset = frag * FRAGMENT_SIZE;
      uint16_t frag_len = min((uint32_t)FRAGMENT_SIZE, frame_size - offset);
      
      // Build fragment packet
      PacketHeader hdr;
      hdr.sync1 = SYNC_BYTE_1;
      hdr.sync2 = SYNC_BYTE_2;
      hdr.sync3 = SYNC_BYTE_3;
      hdr.msg_type = static_cast<uint8_t>(type);
      hdr.payload_len = frag_len;
      hdr.frame_num = frame_num;
      hdr.frag_index = frag;
      hdr.frag_total = frag_count;
      
      // Calculate checksum
      uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
      checksum += calcChecksum(frame_data + offset, frag_len);
      
      PacketFooter ftr;
      ftr.checksum = checksum;
      ftr.end_byte = SYNC_BYTE_2;
      
      // Send fragment
      Serial1.write((uint8_t*)&hdr, sizeof(hdr));
      Serial1.write(frame_data + offset, frag_len);
      Serial1.write((uint8_t*)&ftr, sizeof(ftr));
      
      stats_.tx_bytes += sizeof(hdr) + frag_len + sizeof(ftr);
      stats_.tx_fragments++;
    }
    
    // Flush all data
    Serial1.flush();
    
    stats_.tx_frames++;
    if (type == MsgType::HUB75_FRAG) {
      hub75_frames_sent_++;
    } else {
      oled_frames_sent_++;
    }
    
    return true;
  }
  
  /** Send a single fragment with retry support */
  bool sendFragment(MsgType type, const uint8_t* frame_data, uint32_t frame_size,
                    uint16_t frame_num, uint8_t frag_idx, uint8_t frag_total) {
    
    uint32_t offset = frag_idx * FRAGMENT_SIZE;
    uint16_t frag_len = min((uint32_t)FRAGMENT_SIZE, frame_size - offset);
    
    for (uint8_t retry = 0; retry <= MAX_RETRIES; retry++) {
      // Build fragment packet
      PacketHeader hdr;
      hdr.sync1 = SYNC_BYTE_1;
      hdr.sync2 = SYNC_BYTE_2;
      hdr.sync3 = SYNC_BYTE_3;
      hdr.msg_type = static_cast<uint8_t>(type);
      hdr.payload_len = frag_len;
      hdr.frame_num = frame_num;
      hdr.frag_index = frag_idx;
      hdr.frag_total = frag_total;
      
      // Calculate checksum
      uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
      checksum += calcChecksum(frame_data + offset, frag_len);
      
      PacketFooter ftr;
      ftr.checksum = checksum;
      ftr.end_byte = SYNC_BYTE_2;
      
      // Send fragment
      Serial1.write((uint8_t*)&hdr, sizeof(hdr));
      Serial1.write(frame_data + offset, frag_len);
      Serial1.write((uint8_t*)&ftr, sizeof(ftr));
      Serial1.flush();
      
      stats_.tx_bytes += sizeof(hdr) + frag_len + sizeof(ftr);
      stats_.tx_fragments++;
      
      // Wait for ACK/NAK
      pending_frag_idx_ = frag_idx;
      pending_frame_type_ = type;
      last_frag_send_time_ = micros();
      
      if (waitForAck()) {
        if (retry > 0) {
          stats_.retry_success++;
        }
        return true;  // Success
      }
      
      // Timeout or NAK - retry
      if (retry < MAX_RETRIES) {
        stats_.retries++;
      }
    }
    
    stats_.timeout_errors++;
    return false;  // All retries failed
  }
  
  /** Wait for ACK with timeout */
  bool waitForAck() {
    uint32_t start = micros();
    
    while ((micros() - start) < ACK_TIMEOUT_US) {
      if (Serial1.available() >= (int)sizeof(PacketHeader)) {
        // Try to read response
        if (Serial1.peek() == SYNC_BYTE_1) {
          PacketHeader hdr;
          Serial1.readBytes((uint8_t*)&hdr, sizeof(hdr));
          
          if (validateSync(hdr)) {
            // Read minimal payload for ACK/NAK (just frag index)
            uint8_t ack_frag_idx = 0;
            if (hdr.payload_len >= 1) {
              Serial1.readBytes(&ack_frag_idx, 1);
              // Skip remaining payload
              for (uint16_t i = 1; i < hdr.payload_len; i++) {
                Serial1.read();
              }
            }
            
            // Read footer
            PacketFooter ftr;
            Serial1.readBytes((uint8_t*)&ftr, sizeof(ftr));
            
            stats_.rx_bytes += sizeof(hdr) + hdr.payload_len + sizeof(ftr);
            
            MsgType msg_type = static_cast<MsgType>(hdr.msg_type);
            
            if (msg_type == MsgType::ACK && ack_frag_idx == pending_frag_idx_) {
              return true;
            }
            else if (msg_type == MsgType::NACK) {
              return false;  // NAK - will retry
            }
          }
        } else {
          Serial1.read();  // Discard non-sync byte
          stats_.sync_errors++;
        }
      }
      delayMicroseconds(50);  // Small delay to avoid busy-waiting
    }
    
    return false;  // Timeout
  }
  
  /** Send ping for latency measurement */
  void sendPing(uint16_t seq_num) {
    PacketHeader hdr;
    hdr.sync1 = SYNC_BYTE_1;
    hdr.sync2 = SYNC_BYTE_2;
    hdr.sync3 = SYNC_BYTE_3;
    hdr.msg_type = static_cast<uint8_t>(MsgType::PING);
    hdr.payload_len = sizeof(PingPayload);
    hdr.frame_num = seq_num;
    hdr.frag_index = 0;
    hdr.frag_total = 1;
    
    PingPayload ping;
    ping.timestamp_us = micros();
    ping.seq_num = seq_num;
    
    uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
    checksum += calcChecksum((uint8_t*)&ping, sizeof(ping));
    
    PacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end_byte = SYNC_BYTE_2;
    
    Serial1.write((uint8_t*)&hdr, sizeof(hdr));
    Serial1.write((uint8_t*)&ping, sizeof(ping));
    Serial1.write((uint8_t*)&ftr, sizeof(ftr));
    
    last_ping_time_ = ping.timestamp_us;
    pending_pong_ = true;
    stats_.tx_bytes += sizeof(hdr) + sizeof(ping) + sizeof(ftr);
  }
  
  /** Process incoming UART data */
  void process() {
    while (Serial1.available() >= (int)sizeof(PacketHeader)) {
      // Look for sync bytes
      if (Serial1.peek() != SYNC_BYTE_1) {
        Serial1.read();  // Discard
        stats_.sync_errors++;
        continue;
      }
      
      // Read header
      PacketHeader hdr;
      Serial1.readBytes((uint8_t*)&hdr, sizeof(hdr));
      
      if (!validateSync(hdr)) {
        stats_.sync_errors++;
        continue;
      }
      
      // Read payload (skip for now, just count bytes)
      uint8_t payload_buf[64];  // Small buffer for control messages
      size_t to_read = min((size_t)hdr.payload_len, sizeof(payload_buf));
      Serial1.readBytes(payload_buf, to_read);
      
      // Skip any remaining payload
      if (hdr.payload_len > sizeof(payload_buf)) {
        for (size_t i = sizeof(payload_buf); i < hdr.payload_len; i++) {
          Serial1.read();
        }
      }
      
      // Read footer
      PacketFooter ftr;
      Serial1.readBytes((uint8_t*)&ftr, sizeof(ftr));
      
      stats_.rx_bytes += sizeof(hdr) + hdr.payload_len + sizeof(ftr);
      stats_.rx_frames++;
      
      // Handle message type
      MsgType type = static_cast<MsgType>(hdr.msg_type);
      
      if (type == MsgType::PONG && pending_pong_) {
        PingPayload* pong = (PingPayload*)payload_buf;
        stats_.last_rtt_us = micros() - pong->timestamp_us;
        pending_pong_ = false;
        last_msg_type_ = type;
        has_message_ = true;
      }
      else if (type == MsgType::STATUS) {
        StatusPayload* status = (StatusPayload*)payload_buf;
        stats_.hub75_fps_actual = status->hub75_fps;
        stats_.oled_fps_actual = status->oled_fps;
        last_msg_type_ = type;
        has_message_ = true;
      }
      else if (type == MsgType::ACK || type == MsgType::NACK) {
        last_msg_type_ = type;
        has_message_ = true;
      }
    }
  }
  
  /** Check if a message was received */
  bool hasMessage() const { return has_message_; }
  
  /** Get last received message type */
  MsgType getLastMessageType() const { return last_msg_type_; }
  
  /** Clear message flag */
  void clearMessage() { has_message_ = false; }
  
  /** Get statistics */
  const UartStats& getStats() const { return stats_; }
  
  /** Reset statistics */
  void resetStats() { memset(&stats_, 0, sizeof(stats_)); }
  
  /** Get frame counts */
  uint32_t getHub75FramesSent() const { return hub75_frames_sent_; }
  uint32_t getOledFramesSent() const { return oled_frames_sent_; }
  
  /** Print error rate summary */
  void printStats() {
    Serial.println("\n═══ UART TX Statistics ═══");
    Serial.printf("  TX Frames: %lu (HUB75: %lu, OLED: %lu)\n", 
                  stats_.tx_frames, hub75_frames_sent_, oled_frames_sent_);
    Serial.printf("  TX Fragments: %lu\n", stats_.tx_fragments);
    Serial.printf("  Retries: %lu (%.2f%%)\n", stats_.retries, stats_.getFragmentErrorRate());
    Serial.printf("  Retry Success: %lu\n", stats_.retry_success);
    Serial.printf("  Timeouts: %lu\n", stats_.timeout_errors);
    Serial.printf("  Sync Errors: %lu\n", stats_.sync_errors);
    Serial.printf("  TX Bytes: %lu\n", stats_.tx_bytes);
    Serial.println("═══════════════════════════\n");
  }

private:
  bool initialized_;
  uint16_t frame_num_;
  uint32_t last_ping_time_;
  bool pending_pong_;
  MsgType last_msg_type_;
  bool has_message_;
  UartStats stats_;
  uint32_t hub75_frames_sent_ = 0;
  uint32_t oled_frames_sent_ = 0;
  
  // Fragment retry support
  int8_t pending_frag_idx_;
  MsgType pending_frame_type_;
  uint32_t last_frag_send_time_;
  
  // Frame caches for retries
  uint8_t hub75_frame_cache_[HUB75_RGB_SIZE];
  uint8_t oled_frame_cache_[OLED_MONO_SIZE];
  uint16_t cached_hub75_frame_num_ = 0;
  uint16_t cached_oled_frame_num_ = 0;
};

} // namespace arcos::comms

#endif // ARCOS_COMMS_CPU_UART_HANDLER_HPP_
