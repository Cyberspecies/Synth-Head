/*****************************************************************
 * File:      uart_gpu_bidirectional.cpp
 * Category:  communication/examples
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side bidirectional UART communication example for ESP32-S3.
 *    Uses UART1 with TX on GPIO 12 and RX on GPIO 13 (COM16).
 *    Demonstrates sending and receiving messages with the CPU board.
 *****************************************************************/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <cstring>
#include "../include/UartBidirectionalProtocol.h"

using namespace arcos::communication;

static const char* TAG = "GPU_UART";

/** GPU UART Configuration */
constexpr int GPU_TX_PIN = 12;
constexpr int GPU_RX_PIN = 13;
constexpr uart_port_t GPU_UART_NUM = UART_NUM_1;

/** GPU-side UART bidirectional implementation */
class GpuUartBidirectional : public IUartBidirectional{
public:
  GpuUartBidirectional()
    : initialized_(false)
    , packet_count_(0)
    , last_ping_time_(0)
  {
  }
  
  bool init(int baud_rate = BAUD_RATE) override{
    ESP_LOGI(TAG, "Initializing UART bidirectional communication...");
    
    // Configure UART parameters
    uart_config_t uart_config = {};
    uart_config.baud_rate = baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_APB;
    
    // Install UART driver
    esp_err_t err = uart_driver_install(GPU_UART_NUM, 1024, 1024, 0, NULL, 0);
    if(err != ESP_OK){
      ESP_LOGE(TAG, "Failed to install UART driver: %d", err);
      return false;
    }
    
    // Configure UART parameters
    err = uart_param_config(GPU_UART_NUM, &uart_config);
    if(err != ESP_OK){
      ESP_LOGE(TAG, "Failed to configure UART parameters: %d", err);
      return false;
    }
    
    // Set UART pins (TX=12, RX=13)
    err = uart_set_pin(GPU_UART_NUM, GPU_TX_PIN, GPU_RX_PIN, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if(err != ESP_OK){
      ESP_LOGE(TAG, "Failed to set UART pins: %d", err);
      return false;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "UART%d initialized at %d baud (TX=%d, RX=%d)", 
             GPU_UART_NUM, baud_rate, GPU_TX_PIN, GPU_RX_PIN);
    
    return true;
  }
  
  bool sendPacket(MessageType type, const uint8_t* payload, uint8_t length) override{
    if(!initialized_ || length > MAX_PAYLOAD_SIZE){
      return false;
    }
    
    UartPacket packet;
    packet.start_byte = UART_START_BYTE;
    packet.message_type = type;
    packet.payload_length = length;
    
    // Copy payload
    for(int i = 0; i < length; i++){
      packet.payload[i] = payload[i];
    }
    
    // Calculate checksum
    packet.checksum = packet.calculateChecksum();
    packet.end_byte = UART_END_BYTE;
    
    // Build transmit buffer
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
    
    // Send packet
    int written = uart_write_bytes(GPU_UART_NUM, tx_buffer, tx_index);
    
    if(written != tx_index){
      ESP_LOGE(TAG, "Failed to send complete packet");
      return false;
    }
    
    packet_count_++;
    ESP_LOGI(TAG, "Sent packet #%d, type=0x%02X, length=%d", 
             packet_count_, static_cast<uint8_t>(type), length);
    
    return true;
  }
  
  bool receivePacket(UartPacket& packet) override{
    if(!initialized_){
      return false;
    }
    
    // Check if data available
    size_t available_bytes = 0;
    uart_get_buffered_data_len(GPU_UART_NUM, &available_bytes);
    
    if(available_bytes < 4){
      return false;
    }
    
    // Look for start byte
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
    
    // Read message type and length (with timeout)
    uint8_t header[2];
    int len = uart_read_bytes(GPU_UART_NUM, header, 2, 100 / portTICK_PERIOD_MS);
    
    if(len != 2){
      ESP_LOGW(TAG, "Timeout reading packet header");
      return false;
    }
    
    packet.message_type = static_cast<MessageType>(header[0]);
    packet.payload_length = header[1];
    
    // Validate length
    if(packet.payload_length > MAX_PAYLOAD_SIZE){
      ESP_LOGE(TAG, "Invalid payload length: %d", packet.payload_length);
      return false;
    }
    
    // Read payload + checksum + end byte
    int total_remaining = packet.payload_length + 2;
    uint8_t buffer[MAX_PAYLOAD_SIZE + 2];
    
    len = uart_read_bytes(GPU_UART_NUM, buffer, total_remaining, 
                          100 / portTICK_PERIOD_MS);
    
    if(len != total_remaining){
      ESP_LOGW(TAG, "Timeout reading payload (got %d, expected %d)", len, total_remaining);
      return false;
    }
    
    // Extract payload
    for(int i = 0; i < packet.payload_length; i++){
      packet.payload[i] = buffer[i];
    }
    
    // Extract checksum and end byte
    packet.checksum = buffer[packet.payload_length];
    packet.end_byte = buffer[packet.payload_length + 1];
    
    // Validate packet
    if(!packet.isValid()){
      ESP_LOGE(TAG, "Invalid packet (checksum or framing error)");
      return false;
    }
    
    ESP_LOGI(TAG, "Received valid packet, type=0x%02X, length=%d",
             static_cast<uint8_t>(packet.message_type), packet.payload_length);
    
    return true;
  }
  
  int available() override{
    size_t available_bytes = 0;
    uart_get_buffered_data_len(GPU_UART_NUM, &available_bytes);
    return available_bytes;
  }
  
  bool sendPing() override{
    uint8_t ping_data = 0xCD;
    return sendPacket(MessageType::PING, &ping_data, 1);
  }
  
  bool sendAck(uint8_t ack_data = 0) override{
    return sendPacket(MessageType::ACK, &ack_data, 1);
  }
  
  void update() override{
    // Check for incoming packets
    UartPacket packet;
    if(receivePacket(packet)){
      handleReceivedPacket(packet);
    }
    
    // Send periodic ping every 3 seconds
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if(current_time - last_ping_time_ > 3000){
      sendPing();
      last_ping_time_ = current_time;
    }
  }
  
private:
  bool initialized_;
  int packet_count_;
  uint32_t last_ping_time_;
  
  /** Handle received packet based on type */
  void handleReceivedPacket(const UartPacket& packet){
    switch(packet.message_type){
      case MessageType::PING:
        ESP_LOGI(TAG, "Received PING, sending PONG");
        sendPacket(MessageType::PONG, packet.payload, packet.payload_length);
        break;
        
      case MessageType::PONG:
        ESP_LOGI(TAG, "Received PONG");
        break;
        
      case MessageType::DATA_REQUEST:
        ESP_LOGI(TAG, "Received DATA_REQUEST, sending response");
        {
          uint8_t response_data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
          sendPacket(MessageType::DATA_RESPONSE, response_data, 6);
        }
        break;
        
      case MessageType::DATA_RESPONSE:
        ESP_LOGI(TAG, "Received DATA_RESPONSE with %d bytes", packet.payload_length);
        ESP_LOG_BUFFER_HEX(TAG, packet.payload, packet.payload_length);
        break;
        
      case MessageType::COMMAND:
        ESP_LOGI(TAG, "Received COMMAND (0x%02X)", 
                 packet.payload_length > 0 ? packet.payload[0] : 0);
        sendAck(1);  // Send ACK
        break;
        
      case MessageType::ACK:
        ESP_LOGI(TAG, "Received ACK");
        break;
        
      case MessageType::STATUS:
        ESP_LOGI(TAG, "Received STATUS update");
        if(packet.payload_length > 0){
          ESP_LOGI(TAG, "Status message: %.*s", packet.payload_length, packet.payload);
        }
        break;
        
      case MessageType::ERROR:
        ESP_LOGW(TAG, "Received ERROR notification");
        break;
        
      default:
        ESP_LOGW(TAG, "Unknown message type: 0x%02X", 
                 static_cast<uint8_t>(packet.message_type));
        break;
    }
  }
};

// Global instance
static GpuUartBidirectional uart_comm;

/** Communication task */
void uart_communication_task(void* pvParameters){
  ESP_LOGI(TAG, "Communication task started");
  
  // Send initial greeting
  uint8_t greeting[] = "GPU Ready";
  uart_comm.sendPacket(MessageType::STATUS, greeting, sizeof(greeting) - 1);
  
  uint32_t last_command_time = 0;
  
  while(true){
    // Process communication
    uart_comm.update();
    
    // Example: Send command every 7 seconds
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if(current_time - last_command_time > 7000){
      ESP_LOGI(TAG, "Sending COMMAND to CPU");
      uint8_t command_code = 0x99;
      uart_comm.sendPacket(MessageType::COMMAND, &command_code, 1);
      last_command_time = current_time;
    }
    
    // Yield to other tasks
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

extern "C" void app_main(void){
  ESP_LOGI(TAG, "GPU Bidirectional UART Example");
  ESP_LOGI(TAG, "Initializing...");
  
  // Initialize UART communication
  if(!uart_comm.init()){
    ESP_LOGE(TAG, "Failed to initialize UART communication");
    return;
  }
  
  ESP_LOGI(TAG, "Setup complete - Ready for bidirectional communication");
  
  // Create communication task
  xTaskCreate(uart_communication_task, "uart_comm", 4096, NULL, 5, NULL);
}
