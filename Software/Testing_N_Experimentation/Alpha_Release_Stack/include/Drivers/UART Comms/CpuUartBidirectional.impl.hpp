/*****************************************************************
 * File:      CpuUartBidirectional.impl.hpp
 * Category:  communication/implementations
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side UART bidirectional communication implementation.
 *    Header-only implementation file.
 *****************************************************************/

#ifndef ARCOS_COMMUNICATION_CPU_UART_BIDIRECTIONAL_IMPL_HPP_
#define ARCOS_COMMUNICATION_CPU_UART_BIDIRECTIONAL_IMPL_HPP_

#include <Arduino.h>
#include <cstring>
#include "CpuUartBidirectional.hpp"

namespace arcos::communication{

inline CpuUartBidirectional::CpuUartBidirectional()
  : initialized_(false)
  , frame_counter_(0)
  , last_frame_time_(0)
{
  memset(&analytics_, 0, sizeof(analytics_));
}

inline bool CpuUartBidirectional::init(int baud_rate){
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("CPU: High-Throughput UART System");
  Serial.println("========================================");
  Serial.printf("TX: %d bytes (316 bits) @ 60Hz\n", CPU_SEND_BYTES);
  Serial.printf("RX: %d bytes (1568 bits) @ 60Hz\n", CPU_RECV_BYTES);
  Serial.println("========================================\n");
  
  uart_config_t uart_config = {};
  uart_config.baud_rate = baud_rate;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_APB;
  
  // Increased RX buffer to 8KB to handle large file transfers
  esp_err_t err = uart_driver_install(CPU_UART_NUM, 8192, 2048, 0, NULL, 0);
  if(err != ESP_OK){
    Serial.printf("CPU: ERROR - Failed to install UART driver: %d\n", err);
    return false;
  }
  
  err = uart_param_config(CPU_UART_NUM, &uart_config);
  if(err != ESP_OK){
    Serial.printf("CPU: ERROR - Failed to configure UART: %d\n", err);
    return false;
  }
  
  err = uart_set_pin(CPU_UART_NUM, CPU_TX_PIN, CPU_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if(err != ESP_OK){
    Serial.printf("CPU: ERROR - Failed to set UART pins: %d\n", err);
    return false;
  }
  
  initialized_ = true;
  analytics_.start_time = millis();
  analytics_.last_report_time = millis();
  
  Serial.printf("CPU: UART1 initialized at %d baud (RX=%d, TX=%d)\n", 
                baud_rate, CPU_RX_PIN, CPU_TX_PIN);
  Serial.println("Starting communication...\n");
  return true;
}

inline bool CpuUartBidirectional::sendPacket(MessageType type, const uint8_t* payload, uint8_t length){
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
  
  int written = uart_write_bytes(CPU_UART_NUM, tx_buffer, tx_index);
  
  if(written != tx_index){
    return false;
  }
  
  analytics_.total_bytes_sent += tx_index;
  return true;
}

inline bool CpuUartBidirectional::receivePacket(UartPacket& packet){
  if(!initialized_){
    return false;
  }
  
  size_t available_bytes = 0;
  uart_get_buffered_data_len(CPU_UART_NUM, &available_bytes);
  
  if(available_bytes < 4){
    return false;
  }
  
  uint8_t byte;
  bool found_start = false;
  
  while(available_bytes > 0){
    int len = uart_read_bytes(CPU_UART_NUM, &byte, 1, 10 / portTICK_PERIOD_MS);
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
  int len = uart_read_bytes(CPU_UART_NUM, header, 2, 20 / portTICK_PERIOD_MS);
  
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
  
  len = uart_read_bytes(CPU_UART_NUM, buffer, total_remaining, 
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

inline int CpuUartBidirectional::available(){
  size_t available_bytes = 0;
  uart_get_buffered_data_len(CPU_UART_NUM, &available_bytes);
  return available_bytes;
}

inline bool CpuUartBidirectional::sendPing(){
  uint8_t ping_data = 0xAB;
  return sendPacket(MessageType::PING, &ping_data, 1);
}

inline bool CpuUartBidirectional::sendAck(uint8_t ack_data){
  return sendPacket(MessageType::ACK, &ack_data, 1);
}

inline bool CpuUartBidirectional::sendDataFrame(){
  uint8_t frame_data[CPU_SEND_BYTES];
  
  uint32_t counter = frame_counter_;
  memcpy(frame_data, &counter, sizeof(counter));
  
  for(int i = 4; i < CPU_SEND_BYTES; i++){
    frame_data[i] = (i + frame_counter_) & 0xFF;
  }
  
  bool success = sendPacket(MessageType::DATA_RESPONSE, frame_data, CPU_SEND_BYTES);
  if(success){
    analytics_.frames_sent++;
    frame_counter_++;
  }
  return success;
}

inline void CpuUartBidirectional::printAnalytics(){
  unsigned long current_time = millis();
  unsigned long elapsed_total = current_time - analytics_.start_time;
  unsigned long elapsed_report = current_time - analytics_.last_report_time;
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
  uint32_t expected_frames = (uint32_t)(elapsed_sec * TARGET_FPS);
  if(expected_frames > 0){
    link_reliability = (frames_received_delta * 100.0f) / expected_frames;
    if(link_reliability > 100.0f) link_reliability = 100.0f;  // Cap at 100%
  }
  
  // Check connection status
  bool is_connected = (current_time - analytics_.last_recv_time) < 1000;
  
  Serial.println("\n========================================");
  Serial.println("        CPU ANALYTICS REPORT");
  Serial.println("========================================");
  Serial.printf("Runtime:       %.1f sec (total)\n", elapsed_total / 1000.0f);
  Serial.printf("Report Period: %.1f sec\n", elapsed_sec);
  Serial.printf("Connection:    %s\n", is_connected ? "CONNECTED" : "DISCONNECTED");
  Serial.printf("Frames Sent:   %u total (+%u, %.1f fps)\n", analytics_.frames_sent, frames_sent_delta, send_fps);
  Serial.printf("Frames Recv:   %u total (+%u, %.1f fps)\n", analytics_.frames_received, frames_received_delta, recv_fps);
  Serial.printf("Link Reliab:   %.2f%% (recv:%u / exp:%u)\n", link_reliability, frames_received_delta, expected_frames);
  Serial.printf("Pkts Dropped:  %u total (+%u this period)\n", analytics_.packets_dropped, packets_dropped_delta);
  Serial.printf("Checksum Err:  %u\n", analytics_.checksum_errors);
  Serial.printf("Timeout Err:   %u\n", analytics_.timeout_errors);
  Serial.printf("TX Throughput: %.2f kbps\n", send_kbps);
  Serial.printf("RX Throughput: %.2f kbps\n", recv_kbps);
  
  Serial.print("TX Progress:   [");
  int bar_width = 30;
  int filled = (analytics_.frames_sent % 60) * bar_width / 60;
  for(int i = 0; i < bar_width; i++){
    Serial.print(i < filled ? "=" : " ");
  }
  Serial.printf("] %u\n", analytics_.frames_sent % 60);
  
  Serial.print("RX Progress:   [");
  filled = (analytics_.frames_received % 60) * bar_width / 60;
  for(int i = 0; i < bar_width; i++){
    Serial.print(i < filled ? "=" : " ");
  }
  Serial.printf("] %u\n", analytics_.frames_received % 60);
  
  Serial.println("========================================\n");
  
  // Update tracking variables for next report
  analytics_.last_report_time = current_time;
  analytics_.frames_sent_last_report = analytics_.frames_sent;
  analytics_.frames_received_last_report = analytics_.frames_received;
  analytics_.packets_dropped_last_report = analytics_.packets_dropped;
  analytics_.bytes_sent_last_report = analytics_.total_bytes_sent;
  analytics_.bytes_received_last_report = analytics_.total_bytes_received;
}

inline void CpuUartBidirectional::update(){
  UartPacket packet;
  int max_packets_per_cycle = 5;
  int packets_processed = 0;
  
  while(packets_processed < max_packets_per_cycle && receivePacket(packet)){
    handleReceivedPacket(packet);
    packets_processed++;
  }
  
  unsigned long current_time = millis();
  if(current_time - last_frame_time_ >= FRAME_TIME_MS){
    sendDataFrame();
    last_frame_time_ += FRAME_TIME_MS;  // Maintain consistent timing
    
    // If we've fallen too far behind, resync
    if(current_time - last_frame_time_ > FRAME_TIME_MS * 2){
      last_frame_time_ = current_time;
    }
  }
  
  // Analytics reporting disabled
  // if(current_time - analytics_.last_report_time >= 2000){
  //   printAnalytics();
  // }
}

inline void CpuUartBidirectional::handleReceivedPacket(const UartPacket& packet){
  analytics_.last_recv_time = millis();  // Update connection timestamp
  
  switch(packet.message_type){
    case MessageType::DATA_REQUEST:
      if(packet.payload_length == CPU_RECV_BYTES){
        uint32_t sequence = 0;
        memcpy(&sequence, packet.payload, sizeof(sequence));
        
        if(analytics_.frames_received > 0 && sequence > analytics_.expected_sequence){
          analytics_.packets_dropped += (sequence - analytics_.expected_sequence);
        }
        analytics_.expected_sequence = sequence + 1;
        
        analytics_.frames_received++;
      }
      break;
      
    case MessageType::ACK:
    case MessageType::PONG:
      break;
      
    default:
      break;
  }
}

} // namespace arcos::communication

#endif // ARCOS_COMMUNICATION_CPU_UART_BIDIRECTIONAL_IMPL_HPP_
