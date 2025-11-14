/*****************************************************************
 * File:      GpuUartBidirectional.impl.hpp
 * Category:  communication/implementations
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side UART bidirectional communication implementation.
 *    Header-only implementation file.
 *****************************************************************/

#ifndef ARCOS_COMMUNICATION_GPU_UART_BIDIRECTIONAL_IMPL_HPP_
#define ARCOS_COMMUNICATION_GPU_UART_BIDIRECTIONAL_IMPL_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstring>
#include "GpuUartBidirectional.hpp"

namespace arcos::communication{

inline GpuUartBidirectional::GpuUartBidirectional()
  : initialized_(false)
  , frame_counter_(0)
  , last_frame_time_(0)
{
  memset(&analytics_, 0, sizeof(analytics_));
}

inline bool GpuUartBidirectional::init(int baud_rate){
  ESP_LOGI("GPU_UART", "========================================");
  ESP_LOGI("GPU_UART", "GPU: High-Throughput UART System");
  ESP_LOGI("GPU_UART", "========================================");
  ESP_LOGI("GPU_UART", "TX: %d bytes (1568 bits) @ 60Hz", GPU_SEND_BYTES);
  ESP_LOGI("GPU_UART", "RX: %d bytes (316 bits) @ 60Hz", GPU_RECV_BYTES);
  ESP_LOGI("GPU_UART", "========================================");
  
  uart_config_t uart_config = {};
  uart_config.baud_rate = baud_rate;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_APB;
  
  // Increased RX buffer to 8KB to handle large file transfers (31 fragments × ~214 bytes = ~6.6KB)
  esp_err_t err = uart_driver_install(GPU_UART_NUM, 8192, 2048, 0, NULL, 0);
  if(err != ESP_OK){
    ESP_LOGE("GPU_UART", "Failed to install UART driver: %d", err);
    return false;
  }
  
  err = uart_param_config(GPU_UART_NUM, &uart_config);
  if(err != ESP_OK){
    ESP_LOGE("GPU_UART", "Failed to configure UART parameters: %d", err);
    return false;
  }
  
  err = uart_set_pin(GPU_UART_NUM, GPU_TX_PIN, GPU_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if(err != ESP_OK){
    ESP_LOGE("GPU_UART", "Failed to set UART pins: %d", err);
    return false;
  }
  
  initialized_ = true;
  analytics_.start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  analytics_.last_report_time = analytics_.start_time;
  
  ESP_LOGI("GPU_UART", "UART%d initialized at %d baud (TX=%d, RX=%d)", 
           GPU_UART_NUM, baud_rate, GPU_TX_PIN, GPU_RX_PIN);
  ESP_LOGI("GPU_UART", "Starting communication...");
  
  return true;
}

inline bool GpuUartBidirectional::sendPacket(MessageType type, const uint8_t* payload, uint8_t length){
  if(!initialized_ || length > MAX_PAYLOAD_SIZE){
    return false;
  }
  
  UartPacket packet;
  packet.start_byte = UART_START_BYTE;
  packet.message_type = type;
  packet.payload_length = length;
  
  for(int i = 0; i < length; i++){
    packet.payload[i] = payload[i];
  }
  
  packet.checksum = packet.calculateChecksum();
  packet.end_byte = UART_END_BYTE;
  
  uint8_t tx_buffer[MAX_PAYLOAD_SIZE + 6];
  int tx_index = 0;
  tx_buffer[tx_index++] = packet.start_byte;
  tx_buffer[tx_index++] = static_cast<uint8_t>(packet.message_type);
  tx_buffer[tx_index++] = packet.payload_length;
  
  for(int i = 0; i < packet.payload_length; i++){
    tx_buffer[tx_index++] = packet.payload[i];
  }
  
  tx_buffer[tx_index++] = packet.checksum;
  tx_buffer[tx_index++] = packet.end_byte;
  
  int written = uart_write_bytes(GPU_UART_NUM, tx_buffer, tx_index);
  
  if(written != tx_index){
    return false;
  }
  
  analytics_.total_bytes_sent += tx_index;
  return true;
}

inline bool GpuUartBidirectional::receivePacket(UartPacket& packet){
  if(!initialized_){
    return false;
  }
  
  size_t available_bytes = 0;
  uart_get_buffered_data_len(GPU_UART_NUM, &available_bytes);
  
  if(available_bytes < 4){
    return false;
  }
  
  uint8_t byte;
  bool found_start = false;
  
  while(available_bytes > 0){
    int len = uart_read_bytes(GPU_UART_NUM, &byte, 1, 10 / portTICK_PERIOD_MS);
    if(len > 0 && byte == UART_START_BYTE){
      packet.start_byte = byte;
      found_start = true;
      break;
    }
    available_bytes--;
  }
  
  if(!found_start){
    return false;
  }
  
  uint8_t header[2];
  int len = uart_read_bytes(GPU_UART_NUM, header, 2, 20 / portTICK_PERIOD_MS);
  
  if(len != 2){
    analytics_.timeout_errors++;
    return false;
  }
  
  packet.message_type = static_cast<MessageType>(header[0]);
  packet.payload_length = header[1];
  
  if(packet.payload_length > MAX_PAYLOAD_SIZE){
    return false;
  }
  
  int total_remaining = packet.payload_length + 2;
  uint8_t buffer[MAX_PAYLOAD_SIZE + 2];
  
  len = uart_read_bytes(GPU_UART_NUM, buffer, total_remaining, 
                        20 / portTICK_PERIOD_MS);
  
  if(len != total_remaining){
    analytics_.timeout_errors++;
    return false;
  }
  
  for(int i = 0; i < packet.payload_length; i++){
    packet.payload[i] = buffer[i];
  }
  
  packet.checksum = buffer[packet.payload_length];
  packet.end_byte = buffer[packet.payload_length + 1];
  
  if(!packet.isValid()){
    analytics_.checksum_errors++;
    return false;
  }
  
  analytics_.total_bytes_received += (4 + packet.payload_length + 2);
  return true;
}

inline int GpuUartBidirectional::available(){
  size_t available_bytes = 0;
  uart_get_buffered_data_len(GPU_UART_NUM, &available_bytes);
  return available_bytes;
}

inline bool GpuUartBidirectional::sendPing(){
  uint8_t ping_data = 0xCD;
  return sendPacket(MessageType::PING, &ping_data, 1);
}

inline bool GpuUartBidirectional::sendAck(uint8_t ack_data){
  return sendPacket(MessageType::ACK, &ack_data, 1);
}

inline bool GpuUartBidirectional::sendDataFrame(){
  uint8_t frame_data[GPU_SEND_BYTES];
  
  uint32_t counter = frame_counter_;
  memcpy(frame_data, &counter, sizeof(counter));
  
  for(int i = 4; i < GPU_SEND_BYTES; i++){
    frame_data[i] = (i + frame_counter_) & 0xFF;
  }
  
  bool success = sendPacket(MessageType::DATA_REQUEST, frame_data, GPU_SEND_BYTES);
  if(success){
    analytics_.frames_sent++;
    frame_counter_++;
  }
  return success;
}

inline void GpuUartBidirectional::printAnalytics(){
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  uint32_t elapsed_total = current_time - analytics_.start_time;
  uint32_t elapsed_report = current_time - analytics_.last_report_time;
  float elapsed_sec = elapsed_report / 1000.0f;
  
  // Calculate frames/bytes since last report
  uint32_t frames_sent_delta = analytics_.frames_sent - analytics_.frames_sent_last_report;
  uint32_t frames_received_delta = analytics_.frames_received - analytics_.frames_received_last_report;
  uint32_t packets_dropped_delta = analytics_.packets_dropped - analytics_.packets_dropped_last_report;
  uint32_t bytes_sent_delta = analytics_.total_bytes_sent - analytics_.bytes_sent_last_report;
  uint32_t bytes_received_delta = analytics_.total_bytes_received - analytics_.bytes_received_last_report;
  
  // Calculate averages for this report period
  float send_fps = frames_sent_delta / elapsed_sec;
  float recv_fps = frames_received_delta / elapsed_sec;
  float send_kbps = (bytes_sent_delta * 8.0f) / (elapsed_sec * 1000.0f);
  float recv_kbps = (bytes_received_delta * 8.0f) / (elapsed_sec * 1000.0f);
  
  // Calculate link reliability: we expect to receive at ~60Hz (frames expected in this time period)
  float link_reliability = 100.0f;
  uint32_t expected_frames = (uint32_t)(elapsed_sec * GPU_TARGET_FPS);
  if(expected_frames > 0){
    link_reliability = (frames_received_delta * 100.0f) / expected_frames;
    if(link_reliability > 100.0f) link_reliability = 100.0f;  // Cap at 100%
  }
  
  // Check connection status
  bool is_connected = (current_time - analytics_.last_recv_time) < 1000;
  
  ESP_LOGI("GPU_UART", "========================================");
  ESP_LOGI("GPU_UART", "        GPU ANALYTICS REPORT");
  ESP_LOGI("GPU_UART", "========================================");
  ESP_LOGI("GPU_UART", "Runtime:       %.1f sec (total)", elapsed_total / 1000.0f);
  ESP_LOGI("GPU_UART", "Report Period: %.1f sec", elapsed_sec);
  ESP_LOGI("GPU_UART", "Connection:    %s", is_connected ? "CONNECTED" : "DISCONNECTED");
  ESP_LOGI("GPU_UART", "Frames Sent:   %u total (+%u, %.1f fps)", analytics_.frames_sent, frames_sent_delta, send_fps);
  ESP_LOGI("GPU_UART", "Frames Recv:   %u total (+%u, %.1f fps)", analytics_.frames_received, frames_received_delta, recv_fps);
  ESP_LOGI("GPU_UART", "Link Reliab:   %.2f%% (expected %u)", link_reliability, expected_frames);
  ESP_LOGI("GPU_UART", "Pkts Dropped:  %u total (+%u this period)", analytics_.packets_dropped, packets_dropped_delta);
  ESP_LOGI("GPU_UART", "Checksum Err:  %u", analytics_.checksum_errors);
  ESP_LOGI("GPU_UART", "Timeout Err:   %u", analytics_.timeout_errors);
  ESP_LOGI("GPU_UART", "TX Throughput: %.2f kbps", send_kbps);
  ESP_LOGI("GPU_UART", "RX Throughput: %.2f kbps", recv_kbps);
  
  ESP_LOGI("GPU_UART", "TX Progress:   %u/60", analytics_.frames_sent % 60);
  ESP_LOGI("GPU_UART", "RX Progress:   %u/60", analytics_.frames_received % 60);
  ESP_LOGI("GPU_UART", "========================================");
  
  // Update tracking variables for next report
  analytics_.last_report_time = current_time;
  analytics_.frames_sent_last_report = analytics_.frames_sent;
  analytics_.frames_received_last_report = analytics_.frames_received;
  analytics_.packets_dropped_last_report = analytics_.packets_dropped;
  analytics_.bytes_sent_last_report = analytics_.total_bytes_sent;
  analytics_.bytes_received_last_report = analytics_.total_bytes_received;
}

inline void GpuUartBidirectional::update(){
  UartPacket packet;
  int max_packets_per_cycle = 5;
  int packets_processed = 0;
  
  while(packets_processed < max_packets_per_cycle && receivePacket(packet)){
    handleReceivedPacket(packet);
    packets_processed++;
  }
  
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  if(current_time - last_frame_time_ >= GPU_FRAME_TIME_MS){
    sendDataFrame();
    last_frame_time_ += GPU_FRAME_TIME_MS;  // Maintain consistent timing
    
    // If we've fallen too far behind, resync
    if(current_time - last_frame_time_ > GPU_FRAME_TIME_MS * 2){
      last_frame_time_ = current_time;
    }
  }
  
  // Analytics reporting disabled
  // if(current_time - analytics_.last_report_time >= 2000){
  //   printAnalytics();
  // }
}

inline void GpuUartBidirectional::handleReceivedPacket(const UartPacket& packet){
  analytics_.last_recv_time = xTaskGetTickCount() * portTICK_PERIOD_MS;  // Update connection timestamp
  
  switch(packet.message_type){
    case MessageType::DATA_RESPONSE:
      if(packet.payload_length == GPU_RECV_BYTES){
        uint32_t sequence = 0;
        memcpy(&sequence, packet.payload, sizeof(sequence));
        
        if(analytics_.frames_received > 0 && sequence > analytics_.expected_sequence){
          analytics_.packets_dropped += (sequence - analytics_.expected_sequence);
        }
        analytics_.expected_sequence = sequence + 1;
        
        analytics_.frames_received++;
      }
      break;
      
    default:
      break;
  }
}

} // namespace arcos::communication

#endif // ARCOS_COMMUNICATION_GPU_UART_BIDIRECTIONAL_IMPL_HPP_
