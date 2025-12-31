/*****************************************************************
 * File:      CpuUartHandler.hpp
 * Category:  include/Comms
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side UART communication handler for Arduino framework.
 *    Manages high-speed serial communication with GPU including
 *    frame buffer transmission at 60fps.
 * 
 * Features:
 *    - Asynchronous frame transmission
 *    - Chunked data transfer for large payloads
 *    - Statistics tracking (TX/RX bytes, FPS, errors)
 *    - Simple API for sending frames and commands
 *****************************************************************/

#ifndef ARCOS_INCLUDE_COMMS_CPU_UART_HANDLER_HPP_
#define ARCOS_INCLUDE_COMMS_CPU_UART_HANDLER_HPP_

#include <Arduino.h>
#include "UartProtocol.hpp"

namespace arcos::comms{

/** CPU UART Handler - Arduino Framework
 * 
 * Handles all UART communication from CPU to GPU.
 * Designed for high-speed frame buffer transmission.
 */
class CpuUartHandler{
public:
  /** Configuration for the handler */
  struct Config{
    int rx_pin;
    int tx_pin;
    uint32_t baud_rate;
    int uart_num;
    
    Config() : rx_pin(cpu::UART_RX_PIN), tx_pin(cpu::UART_TX_PIN), 
               baud_rate(UART_BAUD_RATE), uart_num(1) {}
  };
  
  /** Statistics structure */
  struct Stats{
    uint32_t tx_bytes = 0;
    uint32_t rx_bytes = 0;
    uint32_t tx_frames = 0;
    uint32_t rx_frames = 0;
    uint32_t errors = 0;
    uint32_t last_rtt_us = 0;
  };
  
  CpuUartHandler() = default;
  ~CpuUartHandler() = default;
  
  /** Initialize UART communication
   * @param config Optional configuration
   * @return true if initialization successful
   */
  bool init(const Config& config = Config()){
    config_ = config;
    
    // Initialize Serial1 with high-speed baud rate
    Serial1.begin(config_.baud_rate, SERIAL_8N1, config_.rx_pin, config_.tx_pin);
    Serial1.setRxBufferSize(UART_RX_BUFFER_SIZE * 4);
    Serial1.setTxBufferSize(UART_TX_BUFFER_SIZE * 4);
    
    initialized_ = true;
    return true;
  }
  
  /** Process incoming UART data - call frequently in loop
   * @return Number of complete messages processed
   */
  int process(){
    if(!initialized_) return 0;
    
    int messages_processed = 0;
    
    while(Serial1.available() > 0){
      uint8_t byte = Serial1.read();
      stats_.rx_bytes++;
      
      if(processRxByte(byte)){
        messages_processed++;
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
    Serial1.write(header, 4);
    if(len > 0 && data != nullptr){
      Serial1.write(data, len);
    }
    Serial1.write(checksum);
    Serial1.write(MSG_END_BYTE);
    
    stats_.tx_bytes += 6 + len;
    return true;
  }
  
  /** Send frame data to GPU for HUB75 display
   * @param frame_data RGB pixel data (128x32x3 = 12288 bytes)
   * @param width Frame width
   * @param height Frame height
   * @param frame_num Frame sequence number
   * @return true if sent successfully
   * 
   * Uses chunked transfer for large frames
   */
  bool sendFrame(const uint8_t* frame_data, uint16_t width, uint16_t height, uint16_t frame_num){
    if(!initialized_) return false;
    
    uint32_t total_size = width * height * 3;  // RGB data
    uint16_t chunk_size = MAX_PAYLOAD_SIZE - 8;  // Reserve space for header
    uint16_t num_chunks = (total_size + chunk_size - 1) / chunk_size;
    
    // Send frame start
    uint8_t frame_header[8] = {
      static_cast<uint8_t>(width & 0xFF),
      static_cast<uint8_t>((width >> 8) & 0xFF),
      static_cast<uint8_t>(height & 0xFF),
      static_cast<uint8_t>((height >> 8) & 0xFF),
      static_cast<uint8_t>(frame_num & 0xFF),
      static_cast<uint8_t>((frame_num >> 8) & 0xFF),
      static_cast<uint8_t>(num_chunks & 0xFF),
      static_cast<uint8_t>((num_chunks >> 8) & 0xFF)
    };
    
    sendMessage(MsgType::FRAME_SYNC, frame_header, 8);
    
    // Send frame chunks
    for(uint16_t chunk = 0; chunk < num_chunks; chunk++){
      uint32_t offset = chunk * chunk_size;
      uint16_t this_chunk_size = (offset + chunk_size > total_size) ? 
                                  (total_size - offset) : chunk_size;
      
      // Chunk header: [chunk_num_L][chunk_num_H][data...]
      uint8_t chunk_buffer[MAX_PAYLOAD_SIZE];
      chunk_buffer[0] = chunk & 0xFF;
      chunk_buffer[1] = (chunk >> 8) & 0xFF;
      memcpy(&chunk_buffer[2], &frame_data[offset], this_chunk_size);
      
      sendMessage(MsgType::FRAME_DATA, chunk_buffer, this_chunk_size + 2);
    }
    
    stats_.tx_frames++;
    return true;
  }
  
  /** Send ping request to GPU
   * @param ping_id Ping sequence number
   * @return true if sent successfully
   */
  bool sendPing(uint16_t ping_id){
    uint8_t data[4];
    uint32_t timestamp = micros();
    data[0] = ping_id & 0xFF;
    data[1] = (ping_id >> 8) & 0xFF;
    data[2] = timestamp & 0xFF;
    data[3] = (timestamp >> 8) & 0xFF;
    
    last_ping_time_ = timestamp;
    return sendMessage(MsgType::PING, data, 4);
  }
  
  /** Check if a message was received
   * @return true if a complete message is available
   */
  bool hasMessage() const { return message_ready_; }
  
  /** Get the last received message type */
  MsgType getLastMessageType() const { return last_msg_type_; }
  
  /** Get the last received message data */
  const uint8_t* getLastMessageData() const { return rx_buffer_; }
  
  /** Get the last received message length */
  uint16_t getLastMessageLength() const { return last_msg_len_; }
  
  /** Clear the message ready flag */
  void clearMessage(){ message_ready_ = false; }
  
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
            message_ready_ = true;
            stats_.rx_frames++;
            
            // Calculate RTT for PONG messages
            if(last_msg_type_ == MsgType::PONG){
              stats_.last_rtt_us = micros() - last_ping_time_;
            }
            
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
  bool message_ready_ = false;
  MsgType last_msg_type_ = MsgType::PING;
  uint16_t last_msg_len_ = 0;
  
  // RTT tracking
  uint32_t last_ping_time_ = 0;
};

}  // namespace arcos::comms

#endif  // ARCOS_INCLUDE_COMMS_CPU_UART_HANDLER_HPP_
