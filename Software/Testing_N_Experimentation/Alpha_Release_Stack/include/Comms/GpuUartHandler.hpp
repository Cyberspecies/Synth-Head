/*****************************************************************
 * File:      GpuUartHandler.hpp
 * Category:  Communication/GPU
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side UART handler for receiving display frames from CPU.
 *    Receives HUB75 frames at 60fps and OLED frames at 15fps.
 *    ESP-IDF compatible (no Arduino dependency).
 * 
 * Pin Configuration:
 *    - TX: GPIO12 (GPU TX → CPU RX GPIO11)
 *    - RX: GPIO13 (GPU RX ← CPU TX GPIO12)
 * 
 * Frame Buffers:
 *    - HUB75: 12,288 bytes (128x32 RGB)
 *    - OLED: 2,048 bytes (128x128 mono, 1-bit)
 *****************************************************************/

#ifndef ARCOS_COMMS_GPU_UART_HANDLER_HPP_
#define ARCOS_COMMS_GPU_UART_HANDLER_HPP_

// ESP-IDF includes
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

#include "UartProtocol.hpp"

namespace arcos::comms {

// Frame buffer structure for GPU
struct UartFrameBuffer {
  uint8_t* data;
  uint16_t width;
  uint16_t height;
  uint16_t frame_num;
  bool complete;
};

class GpuUartHandler {
public:
  struct Config {
    uint32_t baud_rate;
    uint8_t rx_pin;
    uint8_t tx_pin;
    uint16_t rx_buffer_size;
    
    Config() : baud_rate(UART_BAUD_RATE), rx_pin(13), tx_pin(12), rx_buffer_size(16384) {}
  };

  GpuUartHandler() : initialized_(false), 
                     uart_num_(UART_NUM_1),
                     rx_temp_buffer_(nullptr),
                     hub75_write_idx_(0),
                     hub75_read_idx_(0),
                     oled_write_idx_(0),
                     oled_read_idx_(0),
                     hub75_frame_ready_(false),
                     oled_frame_ready_(false),
                     last_hub75_time_(0),
                     last_oled_time_(0),
                     hub75_frag_received_(0),
                     oled_frag_received_(0),
                     current_hub75_frame_(0),
                     current_oled_frame_(0) {
    memset(&stats_, 0, sizeof(stats_));
    hub75_buffer_[0] = nullptr;
    hub75_buffer_[1] = nullptr;
    oled_buffer_[0] = nullptr;
    oled_buffer_[1] = nullptr;
  }
  
  ~GpuUartHandler() {
    if (hub75_buffer_[0]) free(hub75_buffer_[0]);
    if (hub75_buffer_[1]) free(hub75_buffer_[1]);
    if (oled_buffer_[0]) free(oled_buffer_[0]);
    if (oled_buffer_[1]) free(oled_buffer_[1]);
    if (rx_temp_buffer_) free(rx_temp_buffer_);
  }
  
  /** Initialize UART communication */
  bool init(const Config& config = Config()) {
    config_ = config;
    
    // Configure UART parameters
    uart_config_t uart_config = {};
    uart_config.baud_rate = config.baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    // Install UART driver with large RX buffer
    esp_err_t err = uart_driver_install(uart_num_, config.rx_buffer_size, 1024, 0, NULL, 0);
    if (err != ESP_OK) {
      ESP_LOGE("GpuUart", "uart_driver_install failed: %d", err);
      return false;
    }
    
    err = uart_param_config(uart_num_, &uart_config);
    if (err != ESP_OK) {
      ESP_LOGE("GpuUart", "uart_param_config failed: %d", err);
      return false;
    }
    
    // Set UART pins
    err = uart_set_pin(uart_num_, config.tx_pin, config.rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
      ESP_LOGE("GpuUart", "uart_set_pin failed: %d", err);
      return false;
    }
    
    // Allocate DOUBLE BUFFERS for HUB75 (receive into one, display from other)
    hub75_buffer_[0] = (uint8_t*)malloc(HUB75_RGB_SIZE);
    hub75_buffer_[1] = (uint8_t*)malloc(HUB75_RGB_SIZE);
    oled_buffer_[0] = (uint8_t*)malloc(OLED_MONO_SIZE);
    oled_buffer_[1] = (uint8_t*)malloc(OLED_MONO_SIZE);
    rx_temp_buffer_ = (uint8_t*)malloc(HUB75_RGB_SIZE);  // Temp buffer for receiving
    
    if (!hub75_buffer_[0] || !hub75_buffer_[1] || 
        !oled_buffer_[0] || !oled_buffer_[1] || !rx_temp_buffer_) {
      ESP_LOGE("GpuUart", "Failed to allocate frame buffers");
      return false;
    }
    
    memset(hub75_buffer_[0], 0, HUB75_RGB_SIZE);
    memset(hub75_buffer_[1], 0, HUB75_RGB_SIZE);
    memset(oled_buffer_[0], 0, OLED_MONO_SIZE);
    memset(oled_buffer_[1], 0, OLED_MONO_SIZE);
    memset(rx_temp_buffer_, 0, HUB75_RGB_SIZE);
    
    // Initialize HUB75 frame struct (points to read buffer)
    hub75_frame_.data = hub75_buffer_[hub75_read_idx_];
    hub75_frame_.width = HUB75_WIDTH;
    hub75_frame_.height = HUB75_HEIGHT;
    hub75_frame_.frame_num = 0;
    hub75_frame_.complete = false;
    
    // Initialize OLED frame struct
    oled_frame_.data = oled_buffer_[oled_read_idx_];
    oled_frame_.width = OLED_WIDTH;
    oled_frame_.height = OLED_HEIGHT;
    oled_frame_.frame_num = 0;
    oled_frame_.complete = false;
    
    initialized_ = true;
    return true;
  }
  
  /** Process incoming UART data - call frequently with max bytes to process */
  void process(size_t max_bytes = 16384) {
    if (!initialized_) return;
    
    size_t bytes_processed = 0;
    size_t available = 0;
    uart_get_buffered_data_len(uart_num_, &available);
    
    while (bytes_processed < max_bytes && available >= (sizeof(PacketHeader) + sizeof(PacketFooter))) {
      
      // Look for 3-byte sync pattern (0xAA, 0x55, 0xCC)
      uint8_t sync_buf[3];
      if (uart_read_bytes(uart_num_, sync_buf, 1, 0) != 1) break;
      bytes_processed++;
      
      if (sync_buf[0] != SYNC_BYTE_1) {
        stats_.sync_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Read next 2 bytes for sync verification
      if (uart_read_bytes(uart_num_, &sync_buf[1], 2, pdMS_TO_TICKS(5)) != 2) {
        stats_.sync_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      bytes_processed += 2;
      
      if (sync_buf[1] != SYNC_BYTE_2 || sync_buf[2] != SYNC_BYTE_3) {
        stats_.sync_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Read rest of header (already have sync bytes)
      PacketHeader hdr;
      hdr.sync1 = sync_buf[0];
      hdr.sync2 = sync_buf[1];
      hdr.sync3 = sync_buf[2];
      int read_count = uart_read_bytes(uart_num_, ((uint8_t*)&hdr) + 3, sizeof(hdr) - 3, pdMS_TO_TICKS(10));
      bytes_processed += read_count;
      
      if (read_count != (int)(sizeof(hdr) - 3)) {
        stats_.sync_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Validate payload length (max 1KB fragment)
      if (hdr.payload_len > FRAGMENT_SIZE + 64) {
        stats_.checksum_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      MsgType type = static_cast<MsgType>(hdr.msg_type);
      
      // Handle HUB75 fragment
      if (type == MsgType::HUB75_FRAG) {
        processHub75Fragment(hdr, bytes_processed);
      }
      // Handle OLED fragment
      else if (type == MsgType::OLED_FRAG) {
        processOledFragment(hdr, bytes_processed);
      }
      // Handle legacy full HUB75 frame
      else if (type == MsgType::HUB75_FRAME) {
        processLegacyHub75Frame(hdr, bytes_processed);
      }
      // Handle legacy full OLED frame
      else if (type == MsgType::OLED_FRAME) {
        processLegacyOledFrame(hdr, bytes_processed);
      }
      // Handle PING
      else if (type == MsgType::PING) {
        PingPayload ping;
        uart_read_bytes(uart_num_, (uint8_t*)&ping, sizeof(ping), pdMS_TO_TICKS(10));
        bytes_processed += sizeof(ping);
        
        PacketFooter ftr;
        uart_read_bytes(uart_num_, (uint8_t*)&ftr, sizeof(ftr), pdMS_TO_TICKS(10));
        bytes_processed += sizeof(ftr);
        
        sendPong(ping);
        stats_.rx_bytes += sizeof(hdr) + sizeof(ping) + sizeof(ftr);
      }
      else {
        // Unknown or control message - skip payload
        flushPayload(hdr.payload_len + sizeof(PacketFooter));
        bytes_processed += hdr.payload_len + sizeof(PacketFooter);
      }
      
      uart_get_buffered_data_len(uart_num_, &available);
    }
  }
  
  /** Process HUB75 fragment (1KB) */
  void processHub75Fragment(const PacketHeader& hdr, size_t& bytes_processed) {
    uint8_t frag_idx = hdr.frag_index;
    uint8_t frag_total = hdr.frag_total;
    uint16_t frame_num = hdr.frame_num;
    uint16_t frag_len = hdr.payload_len;
    
    // Validate fragment parameters
    if (frag_idx >= frag_total || frag_total != HUB75_FRAGMENT_COUNT || frag_len > FRAGMENT_SIZE) {
      stats_.checksum_errors++;
      flushPayload(frag_len + sizeof(PacketFooter));
      bytes_processed += frag_len + sizeof(PacketFooter);
      if (!STREAMING_MODE) sendNack(frag_idx);
      return;
    }
    
    // Check for new frame (reset fragment tracking)
    if (frame_num != current_hub75_frame_) {
      current_hub75_frame_ = frame_num;
      hub75_frag_received_ = 0;
    }
    
    // Read fragment data into correct position in temp buffer
    uint32_t offset = frag_idx * FRAGMENT_SIZE;
    int read_count = uart_read_bytes(uart_num_, rx_temp_buffer_ + offset, frag_len, pdMS_TO_TICKS(20));
    bytes_processed += read_count;
    
    if (read_count != (int)frag_len) {
      stats_.checksum_errors++;
      flushPayload(sizeof(PacketFooter));
      bytes_processed += sizeof(PacketFooter);
      if (!STREAMING_MODE) sendNack(frag_idx);
      return;
    }
    
    // Read footer
    PacketFooter ftr;
    uart_read_bytes(uart_num_, (uint8_t*)&ftr, sizeof(ftr), pdMS_TO_TICKS(5));
    bytes_processed += sizeof(ftr);
    
    // Validate checksum
    uint16_t calc_checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
    calc_checksum += calcChecksum(rx_temp_buffer_ + offset, frag_len);
    
    if (calc_checksum != ftr.checksum) {
      stats_.checksum_errors++;
      stats_.retries++;  // Will trigger retry
      if (!STREAMING_MODE) sendNack(frag_idx);
      return;
    }
    
    // Fragment valid - mark as received
    hub75_frag_received_ |= (1 << frag_idx);
    stats_.rx_fragments++;
    stats_.rx_bytes += sizeof(hdr) + frag_len + sizeof(ftr);
    
    // Only send ACK in non-streaming mode
    if (!STREAMING_MODE) {
      sendAck(frag_idx);
    }
    
    // Check if all fragments received
    uint16_t all_frags_mask = (1 << HUB75_FRAGMENT_COUNT) - 1;
    if (hub75_frag_received_ == all_frags_mask) {
      // Complete frame - copy to display buffer and swap
      uint8_t write_idx = 1 - hub75_read_idx_;
      memcpy(hub75_buffer_[write_idx], rx_temp_buffer_, HUB75_RGB_SIZE);
      
      hub75_read_idx_ = write_idx;
      hub75_frame_.data = hub75_buffer_[hub75_read_idx_];
      hub75_frame_.frame_num = frame_num;
      hub75_frame_.complete = true;
      hub75_frame_ready_ = true;
      
      stats_.rx_frames++;
      updateHub75Fps();
      
      // Reset for next frame
      hub75_frag_received_ = 0;
    }
  }
  
  /** Process OLED fragment (1KB) */
  void processOledFragment(const PacketHeader& hdr, size_t& bytes_processed) {
    uint8_t frag_idx = hdr.frag_index;
    uint8_t frag_total = hdr.frag_total;
    uint16_t frame_num = hdr.frame_num;
    uint16_t frag_len = hdr.payload_len;
    
    // Validate fragment parameters
    if (frag_idx >= frag_total || frag_total != OLED_FRAGMENT_COUNT || frag_len > FRAGMENT_SIZE) {
      stats_.checksum_errors++;
      flushPayload(frag_len + sizeof(PacketFooter));
      bytes_processed += frag_len + sizeof(PacketFooter);
      if (!STREAMING_MODE) sendNack(frag_idx);
      return;
    }
    
    // Check for new frame
    if (frame_num != current_oled_frame_) {
      current_oled_frame_ = frame_num;
      oled_frag_received_ = 0;
    }
    
    // Read fragment data
    uint32_t offset = frag_idx * FRAGMENT_SIZE;
    // For last fragment, only read remaining bytes
    uint16_t expected_len = (frag_idx == frag_total - 1) ? 
                            (OLED_MONO_SIZE - offset) : FRAGMENT_SIZE;
    
    int read_count = uart_read_bytes(uart_num_, rx_temp_buffer_ + offset, frag_len, pdMS_TO_TICKS(20));
    bytes_processed += read_count;
    
    if (read_count != (int)frag_len) {
      stats_.checksum_errors++;
      flushPayload(sizeof(PacketFooter));
      bytes_processed += sizeof(PacketFooter);
      if (!STREAMING_MODE) sendNack(frag_idx);
      return;
    }
    
    // Read footer
    PacketFooter ftr;
    uart_read_bytes(uart_num_, (uint8_t*)&ftr, sizeof(ftr), pdMS_TO_TICKS(5));
    bytes_processed += sizeof(ftr);
    
    // Validate checksum
    uint16_t calc_checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
    calc_checksum += calcChecksum(rx_temp_buffer_ + offset, frag_len);
    
    if (calc_checksum != ftr.checksum) {
      stats_.checksum_errors++;
      stats_.retries++;
      if (!STREAMING_MODE) sendNack(frag_idx);
      return;
    }
    
    // Fragment valid
    oled_frag_received_ |= (1 << frag_idx);
    stats_.rx_fragments++;
    stats_.rx_bytes += sizeof(hdr) + frag_len + sizeof(ftr);
    
    // Only send ACK in non-streaming mode
    if (!STREAMING_MODE) {
      sendAck(frag_idx);
    }
    
    // Check if all fragments received
    uint8_t all_frags_mask = (1 << OLED_FRAGMENT_COUNT) - 1;
    if (oled_frag_received_ == all_frags_mask) {
      // Complete frame
      uint8_t write_idx = 1 - oled_read_idx_;
      memcpy(oled_buffer_[write_idx], rx_temp_buffer_, OLED_MONO_SIZE);
      
      oled_read_idx_ = write_idx;
      oled_frame_.data = oled_buffer_[oled_read_idx_];
      oled_frame_.frame_num = frame_num;
      oled_frame_.complete = true;
      oled_frame_ready_ = true;
      
      stats_.rx_frames++;
      updateOledFps();
      
      oled_frag_received_ = 0;
    }
  }
  
  /** Process legacy full HUB75 frame (for backwards compatibility) */
  void processLegacyHub75Frame(const PacketHeader& hdr, size_t& bytes_processed) {
    if (hdr.payload_len != HUB75_RGB_SIZE) {
      stats_.checksum_errors++;
      flushPayload(hdr.payload_len + sizeof(PacketFooter));
      return;
    }
    
    int read_count = uart_read_bytes(uart_num_, rx_temp_buffer_, HUB75_RGB_SIZE, pdMS_TO_TICKS(100));
    bytes_processed += read_count;
    
    if (read_count == (int)HUB75_RGB_SIZE) {
      PacketFooter ftr;
      uart_read_bytes(uart_num_, (uint8_t*)&ftr, sizeof(ftr), pdMS_TO_TICKS(10));
      bytes_processed += sizeof(ftr);
      
      uint16_t calc_checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
      calc_checksum += calcChecksum(rx_temp_buffer_, HUB75_RGB_SIZE);
      
      if (calc_checksum == ftr.checksum) {
        uint8_t write_idx = 1 - hub75_read_idx_;
        memcpy(hub75_buffer_[write_idx], rx_temp_buffer_, HUB75_RGB_SIZE);
        
        hub75_read_idx_ = write_idx;
        hub75_frame_.data = hub75_buffer_[hub75_read_idx_];
        hub75_frame_.frame_num = hdr.frame_num;
        hub75_frame_.complete = true;
        hub75_frame_ready_ = true;
        
        stats_.rx_frames++;
        stats_.rx_bytes += sizeof(hdr) + HUB75_RGB_SIZE + sizeof(ftr);
        updateHub75Fps();
      } else {
        stats_.checksum_errors++;
      }
    }
  }
  
  /** Process legacy full OLED frame */
  void processLegacyOledFrame(const PacketHeader& hdr, size_t& bytes_processed) {
    if (hdr.payload_len != OLED_MONO_SIZE) {
      stats_.checksum_errors++;
      flushPayload(hdr.payload_len + sizeof(PacketFooter));
      return;
    }
    
    int read_count = uart_read_bytes(uart_num_, rx_temp_buffer_, OLED_MONO_SIZE, pdMS_TO_TICKS(50));
    bytes_processed += read_count;
    
    if (read_count == (int)OLED_MONO_SIZE) {
      PacketFooter ftr;
      uart_read_bytes(uart_num_, (uint8_t*)&ftr, sizeof(ftr), pdMS_TO_TICKS(10));
      bytes_processed += sizeof(ftr);
      
      uint16_t calc_checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
      calc_checksum += calcChecksum(rx_temp_buffer_, OLED_MONO_SIZE);
      
      if (calc_checksum == ftr.checksum) {
        uint8_t write_idx = 1 - oled_read_idx_;
        memcpy(oled_buffer_[write_idx], rx_temp_buffer_, OLED_MONO_SIZE);
        
        oled_read_idx_ = write_idx;
        oled_frame_.data = oled_buffer_[oled_read_idx_];
        oled_frame_.frame_num = hdr.frame_num;
        oled_frame_.complete = true;
        oled_frame_ready_ = true;
        
        stats_.rx_frames++;
        stats_.rx_bytes += sizeof(hdr) + OLED_MONO_SIZE + sizeof(ftr);
        updateOledFps();
      } else {
        stats_.checksum_errors++;
      }
    }
  }
  
  /** Check if HUB75 frame is ready */
  bool hasFrame() const { return hub75_frame_ready_; }
  
  /** Check if OLED frame is ready */
  bool hasOledFrame() const { return oled_frame_ready_; }
  
  /** Get HUB75 frame buffer (returns pointer to frame struct) */
  const UartFrameBuffer* getFrame() const {
    if (hub75_frame_ready_) return &hub75_frame_;
    return nullptr;
  }
  
  /** Get OLED frame buffer */
  const UartFrameBuffer* getOledFrame() const {
    if (oled_frame_ready_) return &oled_frame_;
    return nullptr;
  }
  
  /** Mark HUB75 frame as consumed */
  void consumeFrame() {
    hub75_frame_ready_ = false;
    hub75_frame_.complete = false;
  }
  
  /** Mark OLED frame as consumed */
  void consumeOledFrame() {
    oled_frame_ready_ = false;
    oled_frame_.complete = false;
  }
  
  /** Send generic message */
  void sendMessage(MsgType type, const uint8_t* data, size_t len) {
    PacketHeader hdr;
    hdr.sync1 = SYNC_BYTE_1;
    hdr.sync2 = SYNC_BYTE_2;
    hdr.sync3 = SYNC_BYTE_3;
    hdr.msg_type = static_cast<uint8_t>(type);
    hdr.payload_len = len;
    hdr.frame_num = 0;
    hdr.frag_index = 0;
    hdr.frag_total = 1;
    
    uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
    if (data && len > 0) {
      checksum += calcChecksum(data, len);
    }
    
    PacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end_byte = SYNC_BYTE_2;
    
    uart_write_bytes(uart_num_, (const char*)&hdr, sizeof(hdr));
    if (data && len > 0) {
      uart_write_bytes(uart_num_, (const char*)data, len);
    }
    uart_write_bytes(uart_num_, (const char*)&ftr, sizeof(ftr));
    
    stats_.tx_bytes += sizeof(hdr) + len + sizeof(ftr);
  }
  
  /** Send status back to CPU */
  void sendStatus() {
    StatusPayload status;
    status.uptime_ms = esp_timer_get_time() / 1000;
    status.hub75_fps = stats_.hub75_fps * 10;
    status.oled_fps = stats_.oled_fps * 10;
    status.frames_rx = stats_.rx_frames & 0xFFFF;
    status.frames_drop = (stats_.checksum_errors + stats_.sync_errors) & 0xFFFF;
    status.hub75_ok = hub75_frame_ready_ ? 1 : 0;
    status.oled_ok = oled_frame_ready_ ? 1 : 0;
    
    sendMessage(MsgType::STATUS, (uint8_t*)&status, sizeof(status));
  }
  
  /** Get statistics */
  const UartStats& getStats() const { return stats_; }
  
  /** Get actual FPS values */
  uint8_t getHub75Fps() const { return stats_.hub75_fps; }
  uint8_t getOledFps() const { return stats_.oled_fps; }
  
  /** Reset statistics */
  void resetStats() { 
    uint8_t hub75_fps = stats_.hub75_fps;
    uint8_t oled_fps = stats_.oled_fps;
    memset(&stats_, 0, sizeof(stats_));
    stats_.hub75_fps = hub75_fps;
    stats_.oled_fps = oled_fps;
  }
  
  /** Print statistics */
  void printStats() {
    ESP_LOGI("GpuUart", "═══ UART RX Statistics ═══");
    ESP_LOGI("GpuUart", "  RX Frames: %lu", stats_.rx_frames);
    ESP_LOGI("GpuUart", "  RX Fragments: %lu", stats_.rx_fragments);
    ESP_LOGI("GpuUart", "  Retries Requested: %lu (%.2f%%)", stats_.retries, stats_.getFragmentErrorRate());
    ESP_LOGI("GpuUart", "  Checksum Errors: %lu", stats_.checksum_errors);
    ESP_LOGI("GpuUart", "  Sync Errors: %lu", stats_.sync_errors);
    ESP_LOGI("GpuUart", "  RX Bytes: %lu, TX Bytes: %lu", stats_.rx_bytes, stats_.tx_bytes);
    ESP_LOGI("GpuUart", "  HUB75 FPS: %u, OLED FPS: %u", stats_.hub75_fps, stats_.oled_fps);
    ESP_LOGI("GpuUart", "═══════════════════════════");
  }

private:
  Config config_;
  bool initialized_;
  uart_port_t uart_num_;
  
  // Double buffers for HUB75 and OLED
  uint8_t* hub75_buffer_[2];  // Double buffer
  uint8_t* oled_buffer_[2];   // Double buffer
  uint8_t* rx_temp_buffer_;   // Temp buffer for receiving (validates before swap)
  uint8_t hub75_write_idx_;
  uint8_t hub75_read_idx_;
  uint8_t oled_write_idx_;
  uint8_t oled_read_idx_;
  
  UartFrameBuffer hub75_frame_;
  UartFrameBuffer oled_frame_;
  bool hub75_frame_ready_;
  bool oled_frame_ready_;
  uint32_t last_hub75_time_ = 0;
  uint32_t last_oled_time_ = 0;
  UartStats stats_;
  
  // Fragment assembly tracking
  uint16_t hub75_frag_received_;  // Bitmask of received fragments
  uint8_t oled_frag_received_;    // Bitmask of received fragments
  uint16_t current_hub75_frame_;  // Current frame number being assembled
  uint16_t current_oled_frame_;   // Current frame number being assembled
  
  /** Send ACK for fragment */
  void sendAck(uint8_t frag_idx) {
    PacketHeader hdr;
    hdr.sync1 = SYNC_BYTE_1;
    hdr.sync2 = SYNC_BYTE_2;
    hdr.sync3 = SYNC_BYTE_3;
    hdr.msg_type = static_cast<uint8_t>(MsgType::ACK);
    hdr.payload_len = 1;
    hdr.frame_num = 0;
    hdr.frag_index = frag_idx;
    hdr.frag_total = 1;
    
    uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
    checksum += frag_idx;
    
    PacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end_byte = SYNC_BYTE_2;
    
    uart_write_bytes(uart_num_, (const char*)&hdr, sizeof(hdr));
    uart_write_bytes(uart_num_, (const char*)&frag_idx, 1);
    uart_write_bytes(uart_num_, (const char*)&ftr, sizeof(ftr));
    
    stats_.tx_bytes += sizeof(hdr) + 1 + sizeof(ftr);
  }
  
  /** Send NACK for fragment (request resend) */
  void sendNack(uint8_t frag_idx) {
    PacketHeader hdr;
    hdr.sync1 = SYNC_BYTE_1;
    hdr.sync2 = SYNC_BYTE_2;
    hdr.sync3 = SYNC_BYTE_3;
    hdr.msg_type = static_cast<uint8_t>(MsgType::NACK);
    hdr.payload_len = 1;
    hdr.frame_num = 0;
    hdr.frag_index = frag_idx;
    hdr.frag_total = 1;
    
    uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
    checksum += frag_idx;
    
    PacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end_byte = SYNC_BYTE_2;
    
    uart_write_bytes(uart_num_, (const char*)&hdr, sizeof(hdr));
    uart_write_bytes(uart_num_, (const char*)&frag_idx, 1);
    uart_write_bytes(uart_num_, (const char*)&ftr, sizeof(ftr));
    
    stats_.tx_bytes += sizeof(hdr) + 1 + sizeof(ftr);
  }
  
  void flushPayload(size_t bytes) {
    uint8_t discard[64];
    while (bytes > 0) {
      size_t to_read = bytes > sizeof(discard) ? sizeof(discard) : bytes;
      int read = uart_read_bytes(uart_num_, discard, to_read, pdMS_TO_TICKS(10));
      if (read <= 0) break;
      bytes -= read;
    }
  }
  
  void updateHub75Fps() {
    uint32_t now = esp_timer_get_time() / 1000;
    if (last_hub75_time_ > 0 && now > last_hub75_time_) {
      uint32_t dt = now - last_hub75_time_;
      stats_.hub75_fps = (dt > 0) ? (1000 / dt) : 0;
    }
    last_hub75_time_ = now;
  }
  
  void updateOledFps() {
    uint32_t now = esp_timer_get_time() / 1000;
    if (last_oled_time_ > 0 && now > last_oled_time_) {
      uint32_t dt = now - last_oled_time_;
      stats_.oled_fps = (dt > 0) ? (1000 / dt) : 0;
    }
    last_oled_time_ = now;
  }
  
  void sendPong(const PingPayload& ping) {
    PacketHeader hdr;
    hdr.sync1 = SYNC_BYTE_1;
    hdr.sync2 = SYNC_BYTE_2;
    hdr.sync3 = SYNC_BYTE_3;
    hdr.msg_type = static_cast<uint8_t>(MsgType::PONG);
    hdr.payload_len = sizeof(PingPayload);
    hdr.frame_num = ping.seq_num;
    hdr.frag_index = 0;
    hdr.frag_total = 1;
    
    uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
    checksum += calcChecksum((uint8_t*)&ping, sizeof(ping));
    
    PacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end_byte = SYNC_BYTE_2;
    
    uart_write_bytes(uart_num_, (const char*)&hdr, sizeof(hdr));
    uart_write_bytes(uart_num_, (const char*)&ping, sizeof(ping));
    uart_write_bytes(uart_num_, (const char*)&ftr, sizeof(ftr));
    
    stats_.tx_bytes += sizeof(hdr) + sizeof(ping) + sizeof(ftr);
  }
};

} // namespace arcos::comms

#endif // ARCOS_COMMS_GPU_UART_HANDLER_HPP_
