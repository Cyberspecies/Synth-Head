/*****************************************************************
 * File:      GpuUartHandler.hpp
 * Category:  include/Comms
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side UART communication handler for ESP-IDF framework.
 *    Manages high-speed serial communication with CPU including
 *    frame buffer reception at 60fps.
 * 
 * Features:
 *    - Non-blocking frame reception
 *    - Chunked data reassembly
 *    - Double-buffered frame storage
 *    - Statistics tracking (TX/RX bytes, FPS, errors)
 *****************************************************************/

#ifndef ARCOS_INCLUDE_COMMS_GPU_UART_HANDLER_HPP_
#define ARCOS_INCLUDE_COMMS_GPU_UART_HANDLER_HPP_

#include <cstring>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "UartProtocol.hpp"

namespace arcos::comms{

/** Frame buffer for storing received image data */
struct UartFrameBuffer{
  static constexpr uint16_t MAX_WIDTH = 128;
  static constexpr uint16_t MAX_HEIGHT = 32;
  static constexpr uint32_t MAX_SIZE = MAX_WIDTH * MAX_HEIGHT * 3;
  
  uint8_t data[MAX_SIZE];
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t frame_num = 0;
  uint16_t chunks_expected = 0;
  uint16_t chunks_received = 0;
  bool complete = false;
};

/** GPU UART Handler - ESP-IDF Framework
 * 
 * Handles all UART communication from GPU to CPU.
 * Designed for high-speed frame buffer reception.
 */
class GpuUartHandler{
public:
  static constexpr const char* TAG = "GpuUartHandler";
  
  /** Configuration for the handler */
  struct Config{
    int rx_pin;
    int tx_pin;
    uint32_t baud_rate;
    uart_port_t uart_num;
    
    Config() : rx_pin(gpu::UART_RX_PIN), tx_pin(gpu::UART_TX_PIN),
               baud_rate(UART_BAUD_RATE), uart_num(UART_NUM_1) {}
  };
  
  /** Statistics structure */
  struct Stats{
    uint32_t tx_bytes = 0;
    uint32_t rx_bytes = 0;
    uint32_t rx_frames = 0;
    uint32_t errors = 0;
    uint32_t fps = 0;
    uint32_t last_frame_time = 0;
  };
  
  GpuUartHandler() = default;
  ~GpuUartHandler(){
    if(initialized_){
      uart_driver_delete(config_.uart_num);
    }
  }
  
  /** Initialize UART communication
   * @param config Optional configuration
   * @return true if initialization successful
   */
  bool init(const Config& config = Config()){
    config_ = config;
    
    uart_config_t uart_config = {
      .baud_rate = static_cast<int>(config_.baud_rate),
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT
    };
    
    esp_err_t ret = uart_param_config(config_.uart_num, &uart_config);
    if(ret != ESP_OK){
      ESP_LOGE(TAG, "uart_param_config failed: %d", ret);
      return false;
    }
    
    ret = uart_set_pin(config_.uart_num,
                       config_.tx_pin,
                       config_.rx_pin,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if(ret != ESP_OK){
      ESP_LOGE(TAG, "uart_set_pin failed: %d", ret);
      return false;
    }
    
    ret = uart_driver_install(config_.uart_num, 
                               4096,                       // 4KB RX buffer (small, write directly to display)
                               512,                        // 512B TX buffer
                               0, nullptr, 0);
    if(ret != ESP_OK){
      ESP_LOGE(TAG, "uart_driver_install failed: %d", ret);
      return false;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "UART initialized at %lu baud", config_.baud_rate);
    return true;
  }
  
  /** Process incoming UART data - call frequently
   * @param max_bytes Maximum bytes to process per call (0 = unlimited)
   * @return Number of complete messages processed
   */
  int process(int max_bytes = 8192){
    if(!initialized_) return 0;
    
    int messages_processed = 0;
    int bytes_processed = 0;
    
    // Read in chunks for better performance
    uint8_t chunk[512];
    while(max_bytes == 0 || bytes_processed < max_bytes){
      int chunk_size = (max_bytes == 0) ? sizeof(chunk) : 
                       (max_bytes - bytes_processed < (int)sizeof(chunk) ? 
                        max_bytes - bytes_processed : sizeof(chunk));
      
      int len = uart_read_bytes(config_.uart_num, chunk, chunk_size, 0);
      if(len <= 0) break;
      
      bytes_processed += len;
      stats_.rx_bytes += len;
      
      // Process each byte through state machine
      for(int i = 0; i < len; i++){
        if(processRxByte(chunk[i])){
          messages_processed++;
          handleMessage();
        }
      }
    }
    
    return messages_processed;
  }
  
  /** Send a raw message
   * @param type Message type
   * @param data Payload data (can be nullptr if len is 0)
   * @param len Payload length
   * @return true if sent successfully
   */
  bool sendMessage(MsgType type, const uint8_t* data, uint16_t len){
    if(!initialized_) return false;
    if(len > MAX_PAYLOAD_SIZE) return false;
    
    // Build frame
    uint8_t header[4] = {
      MSG_START_BYTE,
      static_cast<uint8_t>(type),
      static_cast<uint8_t>(len & 0xFF),
      static_cast<uint8_t>((len >> 8) & 0xFF)
    };
    
    // Calculate checksum
    uint8_t checksum = calculateChecksum(static_cast<uint8_t>(type), data, len);
    
    // Send frame
    uart_write_bytes(config_.uart_num, header, 4);
    if(len > 0 && data != nullptr){
      uart_write_bytes(config_.uart_num, data, len);
    }
    uart_write_bytes(config_.uart_num, &checksum, 1);
    uint8_t end_byte = MSG_END_BYTE;
    uart_write_bytes(config_.uart_num, &end_byte, 1);
    
    stats_.tx_bytes += 6 + len;
    return true;
  }
  
  /** Send pong response with timing data
   * @param ping_id Ping sequence number from received ping
   * @return true if sent successfully
   */
  bool sendPong(uint16_t ping_id){
    uint8_t data[4];
    data[0] = ping_id & 0xFF;
    data[1] = (ping_id >> 8) & 0xFF;
    uint32_t timestamp = esp_timer_get_time() / 1000;  // ms
    data[2] = timestamp & 0xFF;
    data[3] = (timestamp >> 8) & 0xFF;
    return sendMessage(MsgType::PONG, data, 4);
  }
  
  /** Check if a new frame is ready
   * @return true if a complete frame is available
   */
  bool hasFrame() const{
    return frame_buffers_[read_buffer_].complete;
  }
  
  /** Get the current frame buffer
   * @return Pointer to the completed frame buffer
   */
  const UartFrameBuffer* getFrame() const{
    return &frame_buffers_[read_buffer_];
  }
  
  /** Mark current frame as consumed and swap buffers */
  void consumeFrame(){
    frame_buffers_[read_buffer_].complete = false;
    read_buffer_ = 1 - read_buffer_;
  }
  
  /** Get RGB pixel from current frame
   * @param x X coordinate
   * @param y Y coordinate
   * @param r Output red value
   * @param g Output green value
   * @param b Output blue value
   * @return true if pixel is valid
   */
  bool getPixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b) const{
    const UartFrameBuffer* fb = &frame_buffers_[read_buffer_];
    if(!fb->complete || x < 0 || x >= fb->width || y < 0 || y >= fb->height){
      r = g = b = 0;
      return false;
    }
    
    uint32_t idx = (y * fb->width + x) * 3;
    r = fb->data[idx];
    g = fb->data[idx + 1];
    b = fb->data[idx + 2];
    return true;
  }
  
  /** Get statistics */
  const Stats& getStats() const { return stats_; }
  
  /** Reset statistics */
  void resetStats(){ stats_ = Stats(); }
  
  /** Check if initialized */
  bool isInitialized() const { return initialized_; }
  
private:
  /** Process a single received byte
   * @return true if a complete message was received
   */
  bool processRxByte(uint8_t byte){
    switch(rx_state_){
      case RxState::WAIT_START:
        if(byte == MSG_START_BYTE){
          rx_state_ = RxState::WAIT_TYPE;
        }
        break;
        
      case RxState::WAIT_TYPE:
        rx_type_ = byte;
        rx_state_ = RxState::WAIT_LEN_L;
        break;
        
      case RxState::WAIT_LEN_L:
        rx_len_ = byte;
        rx_state_ = RxState::WAIT_LEN_H;
        break;
        
      case RxState::WAIT_LEN_H:
        rx_len_ |= (byte << 8);
        if(rx_len_ > MAX_PAYLOAD_SIZE){
          rx_state_ = RxState::WAIT_START;
          stats_.errors++;
          return false;
        }
        rx_idx_ = 0;
        if(rx_len_ == 0){
          rx_state_ = RxState::WAIT_CHECKSUM;
        }else{
          rx_state_ = RxState::WAIT_DATA;
        }
        break;
        
      case RxState::WAIT_DATA:
        rx_buffer_[rx_idx_++] = byte;
        if(rx_idx_ >= rx_len_){
          rx_state_ = RxState::WAIT_CHECKSUM;
        }
        break;
        
      case RxState::WAIT_CHECKSUM:
        rx_checksum_ = byte;
        rx_state_ = RxState::WAIT_END;
        break;
        
      case RxState::WAIT_END:
        rx_state_ = RxState::WAIT_START;
        if(byte == MSG_END_BYTE){
          // Verify checksum
          uint8_t calc_checksum = calculateChecksum(rx_type_, rx_buffer_, rx_len_);
          if(calc_checksum == rx_checksum_){
            last_msg_type_ = static_cast<MsgType>(rx_type_);
            last_msg_len_ = rx_len_;
            return true;
          }else{
            stats_.errors++;
          }
        }else{
          stats_.errors++;
        }
        break;
    }
    return false;
  }
  
  /** Handle a complete received message */
  void handleMessage(){
    switch(last_msg_type_){
      case MsgType::PING:
        handlePing();
        break;
        
      case MsgType::FRAME_SYNC:
        handleFrameSync();
        break;
        
      case MsgType::FRAME_DATA:
        handleFrameData();
        break;
        
      default:
        break;
    }
  }
  
  /** Handle ping request */
  void handlePing(){
    if(last_msg_len_ >= 2){
      uint16_t ping_id = rx_buffer_[0] | (rx_buffer_[1] << 8);
      sendPong(ping_id);
    }
  }
  
  /** Handle frame sync (start of new frame) */
  void handleFrameSync(){
    if(last_msg_len_ < 8) return;
    
    UartFrameBuffer* fb = &frame_buffers_[write_buffer_];
    
    fb->width = rx_buffer_[0] | (rx_buffer_[1] << 8);
    fb->height = rx_buffer_[2] | (rx_buffer_[3] << 8);
    fb->frame_num = rx_buffer_[4] | (rx_buffer_[5] << 8);
    fb->chunks_expected = rx_buffer_[6] | (rx_buffer_[7] << 8);
    fb->chunks_received = 0;
    fb->complete = false;
    
    // Validate dimensions
    if(fb->width > UartFrameBuffer::MAX_WIDTH) fb->width = UartFrameBuffer::MAX_WIDTH;
    if(fb->height > UartFrameBuffer::MAX_HEIGHT) fb->height = UartFrameBuffer::MAX_HEIGHT;
  }
  
  /** Handle frame data chunk */
  void handleFrameData(){
    if(last_msg_len_ < 3) return;
    
    UartFrameBuffer* fb = &frame_buffers_[write_buffer_];
    
    uint16_t chunk_num = rx_buffer_[0] | (rx_buffer_[1] << 8);
    uint16_t data_len = last_msg_len_ - 2;
    
    // Calculate offset in frame buffer
    uint16_t chunk_size = MAX_PAYLOAD_SIZE - 8;
    uint32_t offset = chunk_num * chunk_size;
    uint32_t max_size = fb->width * fb->height * 3;
    
    if(offset + data_len <= max_size){
      memcpy(&fb->data[offset], &rx_buffer_[2], data_len);
      fb->chunks_received++;
      
      // Check if frame is complete
      if(fb->chunks_received >= fb->chunks_expected){
        fb->complete = true;
        stats_.rx_frames++;
        
        // Calculate FPS
        uint32_t now = esp_timer_get_time() / 1000;
        if(stats_.last_frame_time > 0){
          uint32_t delta = now - stats_.last_frame_time;
          if(delta > 0){
            stats_.fps = 1000 / delta;
          }
        }
        stats_.last_frame_time = now;
        
        // Swap write buffer
        write_buffer_ = 1 - write_buffer_;
      }
    }
  }
  
  // Receiver state machine
  enum class RxState{
    WAIT_START,
    WAIT_TYPE,
    WAIT_LEN_L,
    WAIT_LEN_H,
    WAIT_DATA,
    WAIT_CHECKSUM,
    WAIT_END
  };
  
  Config config_;
  Stats stats_;
  bool initialized_ = false;
  
  // RX state machine
  RxState rx_state_ = RxState::WAIT_START;
  uint8_t rx_type_ = 0;
  uint16_t rx_len_ = 0;
  uint16_t rx_idx_ = 0;
  uint8_t rx_checksum_ = 0;
  uint8_t rx_buffer_[MAX_PAYLOAD_SIZE];
  
  // Message handling
  MsgType last_msg_type_ = MsgType::PING;
  uint16_t last_msg_len_ = 0;
  
  // Double-buffered frame storage
  UartFrameBuffer frame_buffers_[2];
  int write_buffer_ = 0;
  int read_buffer_ = 0;
};

}  // namespace arcos::comms

#endif  // ARCOS_INCLUDE_COMMS_GPU_UART_HANDLER_HPP_
