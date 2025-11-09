/*****************************************************************
 * File:      CpuUartBidirectional.cpp
 * Category:  communication/implementations
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side UART bidirectional communication implementation.
 *****************************************************************/

#include <Arduino.h>
#include <cstring>
#include "../include/CpuUartBidirectional.h"

using namespace arcos::communication;

CpuUartBidirectional::CpuUartBidirectional()
  : initialized_(false)
  , frame_counter_(0)
  , last_frame_time_(0)
{
  memset(&analytics_, 0, sizeof(analytics_));
}

bool CpuUartBidirectional::init(int baud_rate){
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
  
  esp_err_t err = uart_driver_install(CPU_UART_NUM, 2048, 2048, 0, NULL, 0);
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
  
  Serial.printf("CPU: UART2 initialized at %d baud (RX=%d, TX=%d)\n", 
                baud_rate, CPU_RX_PIN, CPU_TX_PIN);
  Serial.println("Starting communication...\n");
  return true;
}

bool CpuUartBidirectional::sendPacket(MessageType type, const uint8_t* payload, uint8_t length){
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

bool CpuUartBidirectional::receivePacket(UartPacket& packet){
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

int CpuUartBidirectional::available(){
  size_t available_bytes = 0;
  uart_get_buffered_data_len(CPU_UART_NUM, &available_bytes);
  return available_bytes;
}

bool CpuUartBidirectional::sendPing(){
  uint8_t ping_data = 0xAB;
  return sendPacket(MessageType::PING, &ping_data, 1);
}

bool CpuUartBidirectional::sendAck(uint8_t ack_data){
  return sendPacket(MessageType::ACK, &ack_data, 1);
}

bool CpuUartBidirectional::sendDataFrame(){
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

void CpuUartBidirectional::printAnalytics(){
  unsigned long current_time = millis();
  unsigned long elapsed = current_time - analytics_.start_time;
  float elapsed_sec = elapsed / 1000.0f;
  
  float send_fps = analytics_.frames_sent / elapsed_sec;
  float recv_fps = analytics_.frames_received / elapsed_sec;
  float send_kbps = (analytics_.total_bytes_sent * 8.0f) / (elapsed_sec * 1000.0f);
  float recv_kbps = (analytics_.total_bytes_received * 8.0f) / (elapsed_sec * 1000.0f);
  float packet_loss = 0.0f;
  
  if(analytics_.frames_sent > 0){
    packet_loss = (analytics_.packets_lost * 100.0f) / analytics_.frames_sent;
  }
  
  Serial.println("\n========================================");
  Serial.println("        CPU ANALYTICS REPORT");
  Serial.println("========================================");
  Serial.printf("Runtime:       %.1f sec\n", elapsed_sec);
  Serial.printf("Frames Sent:   %u (%.1f fps)\n", analytics_.frames_sent, send_fps);
  Serial.printf("Frames Recv:   %u (%.1f fps)\n", analytics_.frames_received, recv_fps);
  Serial.printf("Packet Loss:   %u (%.2f%%)\n", analytics_.packets_lost, packet_loss);
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
  
  analytics_.last_report_time = current_time;
}

void CpuUartBidirectional::update(){
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
    last_frame_time_ = current_time;
  }
  
  if(current_time - analytics_.last_report_time >= 2000){
    printAnalytics();
  }
}

void CpuUartBidirectional::handleReceivedPacket(const UartPacket& packet){
  switch(packet.message_type){
    case MessageType::DATA_REQUEST:
      if(packet.payload_length == CPU_RECV_BYTES){
        uint32_t sequence = 0;
        memcpy(&sequence, packet.payload, sizeof(sequence));
        
        if(analytics_.frames_received > 0 && sequence > analytics_.expected_sequence){
          analytics_.packets_lost += (sequence - analytics_.expected_sequence);
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
