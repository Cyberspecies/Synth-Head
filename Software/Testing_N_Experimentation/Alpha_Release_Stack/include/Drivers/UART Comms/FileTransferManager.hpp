/*****************************************************************
 * File:      FileTransferManager.hpp
 * Category:  communication/file_transfer
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Manages file transfers over UART between CPU and GPU.
 *    Handles fragmentation, sequencing, and reassembly of large
 *    data transfers. Operates with lower priority than main
 *    sensor/LED data streams.
 * 
 * Features:
 *    - Fragment-based transmission (200 bytes per fragment)
 *    - Automatic retry on failure
 *    - Progress tracking
 *    - Non-blocking operation
 *    - Priority-based scheduling (yields to sensor/LED data)
 *****************************************************************/

#ifndef ARCOS_COMMUNICATION_FILE_TRANSFER_MANAGER_HPP_
#define ARCOS_COMMUNICATION_FILE_TRANSFER_MANAGER_HPP_

#include "UartBidirectionalProtocol.h"
#include <cstring>
#include <functional>

// Platform-independent millisecond timing
#ifdef ARDUINO
  #include <Arduino.h>
  inline uint32_t getMillis() { return millis(); }
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  inline uint32_t getMillis() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }
#endif

// Platform-specific timing functions
#ifdef ARDUINO
  #include <Arduino.h>
  #define GET_MILLIS() millis()
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #define GET_MILLIS() (xTaskGetTickCount() * portTICK_PERIOD_MS)
#endif

namespace arcos::communication{

/** File transfer state */
enum class TransferState{
  IDLE,
  SENDING_METADATA,
  SENDING_DATA,
  WAITING_ACK,
  COMPLETED,
  ERROR
};

/** File transfer statistics */
struct TransferStats{
  uint32_t bytes_sent = 0;
  uint32_t fragments_sent = 0;
  uint32_t fragments_acked = 0;
  uint32_t retries = 0;
  uint32_t errors = 0;
  uint32_t start_time = 0;
  uint32_t end_time = 0;
};

/** Callback types */
using TransferProgressCallback = std::function<void(uint32_t bytes_sent, uint32_t total_bytes)>;
using TransferCompleteCallback = std::function<void(bool success, const char* error_msg)>;
using TransferReceiveCallback = std::function<void(uint32_t file_id, const uint8_t* data, uint32_t size)>;

/** File Transfer Manager - Sender Side */
class FileTransferManager{
public:
  FileTransferManager() 
    : state_(TransferState::IDLE)
    , current_file_id_(0)
    , current_fragment_(0)
    , total_fragments_(0)
    , retry_count_(0)
    , last_send_time_(0)
    , data_buffer_(nullptr)
    , data_size_(0)
    , uart_comm_(nullptr)
  {
    memset(&metadata_, 0, sizeof(metadata_));
    memset(&stats_, 0, sizeof(stats_));
  }
  
  /**
   * @brief Initialize file transfer manager
   * @param uart_comm Pointer to UART communication interface
   */
  void init(IUartBidirectional* uart_comm){
    uart_comm_ = uart_comm;
    state_ = TransferState::IDLE;
  }
  
  /**
   * @brief Start file transfer
   * @param data Data buffer to send
   * @param size Size of data in bytes
   * @param filename Optional filename (for logging)
   * @return true if transfer started successfully
   */
  bool startTransfer(const uint8_t* data, uint32_t size, const char* filename = "data"){
    // Allow starting new transfer if previous one completed or errored
    if(state_ != TransferState::IDLE && 
       state_ != TransferState::COMPLETED && 
       state_ != TransferState::ERROR){
      return false;  // Transfer already in progress
    }
    
    if(!uart_comm_ || !data || size == 0){
      return false;
    }
    
    // Setup transfer
    data_buffer_ = data;
    data_size_ = size;
    current_file_id_ = generateFileId();
    current_fragment_ = 0;
    retry_count_ = 0;
    
    // Calculate fragments
    constexpr uint16_t FRAGMENT_SIZE = 200;
    total_fragments_ = (size + FRAGMENT_SIZE - 1) / FRAGMENT_SIZE;
    
    // Setup metadata
    metadata_.file_id = current_file_id_;
    metadata_.total_size = size;
    metadata_.fragment_size = FRAGMENT_SIZE;
    metadata_.total_fragments = total_fragments_;
    strncpy(metadata_.filename, filename, 31);
    metadata_.filename[31] = '\0';
    
    // Reset stats
    memset(&stats_, 0, sizeof(stats_));
    stats_.start_time = getMillis();
    
    // Start transfer
    state_ = TransferState::SENDING_METADATA;
    
    return true;
  }
  
  /**
   * @brief Update file transfer (call regularly, non-blocking)
   * @param allow_send If false, will not send packets (yields to higher priority)
   * @return true if transfer is active
   */
  bool update(bool allow_send = true){
    if(state_ == TransferState::IDLE || state_ == TransferState::COMPLETED){
      return false;
    }
    
    uint32_t current_time = getMillis();
    
    switch(state_){
      case TransferState::SENDING_METADATA:
        if(allow_send){
          if(sendMetadata()){
            state_ = TransferState::SENDING_DATA;
            last_send_time_ = current_time;
          }else{
            retry_count_++;
            if(retry_count_ >= MAX_RETRIES){
              state_ = TransferState::ERROR;
              if(complete_callback_){
                complete_callback_(false, "Failed to send metadata");
              }
            }
          }
        }
        break;
        
      case TransferState::SENDING_DATA:
        if(allow_send){
          if(sendNextFragment()){
            stats_.fragments_sent++;
            last_send_time_ = current_time;
            
            // Update progress
            if(progress_callback_){
              uint32_t bytes_sent = current_fragment_ * metadata_.fragment_size;
              if(bytes_sent > data_size_) bytes_sent = data_size_;
              progress_callback_(bytes_sent, data_size_);
            }
            
            // Check if all fragments sent
            if(current_fragment_ >= total_fragments_){
              state_ = TransferState::COMPLETED;
              stats_.end_time = current_time;
              if(complete_callback_){
                complete_callback_(true, "Transfer completed");
              }
            }
          }else{
            retry_count_++;
            if(retry_count_ >= MAX_RETRIES){
              state_ = TransferState::ERROR;
              stats_.errors++;
              if(complete_callback_){
                complete_callback_(false, "Failed to send fragment");
              }
            }
          }
        }
        break;
        
      case TransferState::WAITING_ACK:
        // Wait for acknowledgment (timeout after 100ms)
        if(current_time - last_send_time_ > 100){
          retry_count_++;
          if(retry_count_ >= MAX_RETRIES){
            state_ = TransferState::ERROR;
            stats_.errors++;
            if(complete_callback_){
              complete_callback_(false, "ACK timeout");
            }
          }else{
            state_ = TransferState::SENDING_DATA;
          }
        }
        break;
        
      default:
        break;
    }
    
    return state_ != TransferState::IDLE && state_ != TransferState::COMPLETED;
  }
  
  /**
   * @brief Handle received acknowledgment
   */
  void handleAck(const FileTransferAck& ack){
    if(ack.file_id != current_file_id_){
      return;  // Wrong file
    }
    
    stats_.fragments_acked++;
    retry_count_ = 0;  // Reset retry counter on successful ACK
    
    if(ack.status == 0){
      // Success - can proceed
      if(state_ == TransferState::WAITING_ACK){
        state_ = TransferState::SENDING_DATA;
      }
    }else{
      // Retry requested
      stats_.retries++;
    }
  }
  
  /**
   * @brief Check if transfer is active
   */
  bool isActive() const{
    return state_ != TransferState::IDLE && 
           state_ != TransferState::COMPLETED && 
           state_ != TransferState::ERROR;
  }
  
  /**
   * @brief Get current transfer state
   */
  TransferState getState() const{
    return state_;
  }
  
  /**
   * @brief Get transfer statistics
   */
  const TransferStats& getStats() const{
    return stats_;
  }
  
  /**
   * @brief Set progress callback
   */
  void setProgressCallback(TransferProgressCallback callback){
    progress_callback_ = callback;
  }
  
  /**
   * @brief Set completion callback
   */
  void setCompleteCallback(TransferCompleteCallback callback){
    complete_callback_ = callback;
  }
  
  /**
   * @brief Cancel current transfer
   */
  void cancel(){
    state_ = TransferState::IDLE;
    data_buffer_ = nullptr;
    data_size_ = 0;
  }
  
  /**
   * @brief Get transfer progress (0.0 - 1.0)
   */
  float getProgress() const{
    if(total_fragments_ == 0) return 0.0f;
    return static_cast<float>(current_fragment_) / static_cast<float>(total_fragments_);
  }

private:
  static constexpr uint8_t MAX_RETRIES = 5;
  static constexpr uint32_t RETRY_DELAY_MS = 50;
  
  TransferState state_;
  FileTransferMetadata metadata_;
  TransferStats stats_;
  
  uint32_t current_file_id_;
  uint16_t current_fragment_;
  uint16_t total_fragments_;
  uint8_t retry_count_;
  uint32_t last_send_time_;
  
  const uint8_t* data_buffer_;
  uint32_t data_size_;
  
  IUartBidirectional* uart_comm_;
  
  TransferProgressCallback progress_callback_;
  TransferCompleteCallback complete_callback_;
  
  /**
   * @brief Generate unique file ID
   */
  uint32_t generateFileId(){
    static uint32_t id_counter = 0;
    return (getMillis() << 16) | (++id_counter & 0xFFFF);
  }
  
  /**
   * @brief Send metadata packet
   */
  bool sendMetadata(){
    return uart_comm_->sendPacket(
      MessageType::FILE_TRANSFER_START,
      reinterpret_cast<const uint8_t*>(&metadata_),
      sizeof(metadata_)
    );
  }
  
  /**
   * @brief Send next data fragment
   */
  bool sendNextFragment(){
    FileTransferFragment fragment;
    fragment.file_id = current_file_id_;
    fragment.fragment_index = current_fragment_;
    
    // Calculate data for this fragment
    uint32_t offset = current_fragment_ * metadata_.fragment_size;
    uint32_t remaining = data_size_ - offset;
    uint16_t fragment_data_size = (remaining > metadata_.fragment_size) 
      ? metadata_.fragment_size 
      : static_cast<uint16_t>(remaining);
    
    fragment.data_length = fragment_data_size;
    memcpy(fragment.data, data_buffer_ + offset, fragment_data_size);
    
    bool success = uart_comm_->sendPacket(
      MessageType::FILE_TRANSFER_DATA,
      reinterpret_cast<const uint8_t*>(&fragment),
      sizeof(fragment)
    );
    
    if(success){
      current_fragment_++;
      stats_.bytes_sent += fragment_data_size;
    }
    
    return success;
  }
};

/** File Transfer Receiver - Receiver Side */
class FileTransferReceiver{
public:
  FileTransferReceiver()
    : receiving_(false)
    , current_file_id_(0)
    , total_size_(0)
    , bytes_received_(0)
    , next_fragment_(0)
    , receive_buffer_(nullptr)
    , uart_comm_(nullptr)
  {
    memset(&metadata_, 0, sizeof(metadata_));
  }
  
  ~FileTransferReceiver(){
    if(receive_buffer_){
      delete[] receive_buffer_;
    }
  }
  
  /**
   * @brief Initialize receiver
   */
  void init(IUartBidirectional* uart_comm){
    uart_comm_ = uart_comm;
  }
  
  /**
   * @brief Handle received metadata packet
   */
  bool handleMetadata(const FileTransferMetadata& metadata){
    // Cleanup any previous transfer
    if(receive_buffer_){
      delete[] receive_buffer_;
      receive_buffer_ = nullptr;
    }
    
    // Setup new transfer
    metadata_ = metadata;
    current_file_id_ = metadata.file_id;
    total_size_ = metadata.total_size;
    bytes_received_ = 0;
    next_fragment_ = 0;
    
    // Allocate buffer
    receive_buffer_ = new uint8_t[total_size_];
    if(!receive_buffer_){
      return false;  // Allocation failed
    }
    
    receiving_ = true;
    return true;
  }
  
  /**
   * @brief Handle received data fragment
   */
  bool handleFragment(const FileTransferFragment& fragment){
    if(!receiving_ || fragment.file_id != current_file_id_){
      return false;
    }
    
    // Check for correct sequence
    if(fragment.fragment_index != next_fragment_){
      // Out of order - send NACK
      sendAck(fragment.fragment_index, 1);  // status=1 means retry
      return false;
    }
    
    // Copy data to buffer
    uint32_t offset = fragment.fragment_index * metadata_.fragment_size;
    if(offset + fragment.data_length > total_size_){
      // Invalid fragment
      sendAck(fragment.fragment_index, 2);  // status=2 means error
      return false;
    }
    
    memcpy(receive_buffer_ + offset, fragment.data, fragment.data_length);
    bytes_received_ += fragment.data_length;
    next_fragment_++;
    
    // Send ACK
    sendAck(fragment.fragment_index, 0);  // status=0 means success
    
    // Check if complete
    if(next_fragment_ >= metadata_.total_fragments){
      // Transfer complete
      if(receive_callback_){
        receive_callback_(current_file_id_, receive_buffer_, total_size_);
      }
      
      // Cleanup
      delete[] receive_buffer_;
      receive_buffer_ = nullptr;
      receiving_ = false;
      
      return true;
    }
    
    return true;
  }
  
  /**
   * @brief Set receive completion callback
   */
  void setReceiveCallback(TransferReceiveCallback callback){
    receive_callback_ = callback;
  }
  
  /**
   * @brief Check if currently receiving
   */
  bool isReceiving() const{
    return receiving_;
  }
  
  /**
   * @brief Get receive progress (0.0 - 1.0)
   */
  float getProgress() const{
    if(total_size_ == 0) return 0.0f;
    return static_cast<float>(bytes_received_) / static_cast<float>(total_size_);
  }

private:
  bool receiving_;
  FileTransferMetadata metadata_;
  uint32_t current_file_id_;
  uint32_t total_size_;
  uint32_t bytes_received_;
  uint16_t next_fragment_;
  uint8_t* receive_buffer_;
  
  IUartBidirectional* uart_comm_;
  TransferReceiveCallback receive_callback_;
  
  /**
   * @brief Send acknowledgment for fragment
   */
  void sendAck(uint16_t fragment_index, uint8_t status){
    if(!uart_comm_) return;
    
    FileTransferAck ack;
    ack.file_id = current_file_id_;
    ack.fragment_index = fragment_index;
    ack.status = status;
    ack._reserved = 0;
    
    uart_comm_->sendPacket(
      MessageType::FILE_TRANSFER_ACK,
      reinterpret_cast<const uint8_t*>(&ack),
      sizeof(ack)
    );
  }
};

} // namespace arcos::communication

#endif // ARCOS_COMMUNICATION_FILE_TRANSFER_MANAGER_HPP_
