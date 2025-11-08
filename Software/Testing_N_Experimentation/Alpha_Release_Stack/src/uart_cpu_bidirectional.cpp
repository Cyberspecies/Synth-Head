/*****************************************************************
 * File:      uart_cpu_bidirectional.cpp
 * Category:  communication/examples
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side bidirectional UART communication example for ESP32-S3.
 *    Uses UART2 with RX on GPIO 11 and TX on GPIO 12 (COM15).
 *    Demonstrates sending and receiving messages with the GPU board.
 *****************************************************************/

#include <Arduino.h>
#include <HardwareSerial.h>
#include "../include/UartBidirectionalProtocol.h"

using namespace arcos::communication;

/** CPU UART Configuration */
constexpr int CPU_RX_PIN = 11;
constexpr int CPU_TX_PIN = 12;
constexpr int CPU_UART_NUM = 2;  // UART2

/** CPU-side UART bidirectional implementation */
class CpuUartBidirectional : public IUartBidirectional{
public:
  CpuUartBidirectional()
    : uart_(CPU_UART_NUM)
    , initialized_(false)
    , packet_count_(0)
    , last_ping_time_(0)
  {
  }
  
  bool init(int baud_rate = BAUD_RATE) override{
    Serial.begin(115200);  // USB Serial for debugging
    delay(1000);
    Serial.println("CPU: Initializing UART bidirectional communication...");
    
    // Initialize UART2 with RX=11, TX=12
    uart_.begin(baud_rate, SERIAL_8N1, CPU_RX_PIN, CPU_TX_PIN);
    
    if(!uart_){
      Serial.println("CPU: ERROR - Failed to initialize UART2");
      return false;
    }
    
    initialized_ = true;
    Serial.printf("CPU: UART2 initialized at %d baud (RX=%d, TX=%d)\n", 
                  baud_rate, CPU_RX_PIN, CPU_TX_PIN);
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
    
    // Send packet
    uart_.write(packet.start_byte);
    uart_.write(static_cast<uint8_t>(packet.message_type));
    uart_.write(packet.payload_length);
    uart_.write(packet.payload, packet.payload_length);
    uart_.write(packet.checksum);
    uart_.write(packet.end_byte);
    
    packet_count_++;
    Serial.printf("CPU: Sent packet #%d, type=0x%02X, length=%d\n", 
                  packet_count_, static_cast<uint8_t>(type), length);
    
    return true;
  }
  
  bool receivePacket(UartPacket& packet) override{
    if(!initialized_ || uart_.available() < 4){
      return false;
    }
    
    // Look for start byte
    while(uart_.available() > 0){
      uint8_t byte = uart_.read();
      if(byte == UART_START_BYTE){
        packet.start_byte = byte;
        break;
      }
    }
    
    if(packet.start_byte != UART_START_BYTE){
      return false;
    }
    
    // Wait for complete packet (need at least 3 more bytes: type, length, end)
    unsigned long timeout = millis() + 100;
    while(uart_.available() < 3 && millis() < timeout){
      delay(1);
    }
    
    if(uart_.available() < 3){
      Serial.println("CPU: Timeout waiting for packet");
      return false;
    }
    
    // Read message type and length
    packet.message_type = static_cast<MessageType>(uart_.read());
    packet.payload_length = uart_.read();
    
    // Validate length
    if(packet.payload_length > MAX_PAYLOAD_SIZE){
      Serial.printf("CPU: Invalid payload length: %d\n", packet.payload_length);
      return false;
    }
    
    // Wait for payload + checksum + end byte
    timeout = millis() + 100;
    while(uart_.available() < (packet.payload_length + 2) && millis() < timeout){
      delay(1);
    }
    
    if(uart_.available() < (packet.payload_length + 2)){
      Serial.println("CPU: Timeout waiting for payload");
      return false;
    }
    
    // Read payload
    for(int i = 0; i < packet.payload_length; i++){
      packet.payload[i] = uart_.read();
    }
    
    // Read checksum and end byte
    packet.checksum = uart_.read();
    packet.end_byte = uart_.read();
    
    // Validate packet
    if(!packet.isValid()){
      Serial.println("CPU: Invalid packet received");
      return false;
    }
    
    Serial.printf("CPU: Received valid packet, type=0x%02X, length=%d\n",
                  static_cast<uint8_t>(packet.message_type), packet.payload_length);
    
    return true;
  }
  
  int available() override{
    return uart_.available();
  }
  
  bool sendPing() override{
    uint8_t ping_data = 0xAB;
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
    
    // Send periodic ping every 2 seconds
    unsigned long current_time = millis();
    if(current_time - last_ping_time_ > 2000){
      sendPing();
      last_ping_time_ = current_time;
    }
  }
  
private:
  HardwareSerial uart_;
  bool initialized_;
  int packet_count_;
  unsigned long last_ping_time_;
  
  /** Handle received packet based on type */
  void handleReceivedPacket(const UartPacket& packet){
    switch(packet.message_type){
      case MessageType::PING:
        Serial.println("CPU: Received PING, sending PONG");
        sendPacket(MessageType::PONG, packet.payload, packet.payload_length);
        break;
        
      case MessageType::PONG:
        Serial.println("CPU: Received PONG");
        break;
        
      case MessageType::DATA_REQUEST:
        Serial.println("CPU: Received DATA_REQUEST, sending response");
        {
          uint8_t response_data[] = {0x12, 0x34, 0x56, 0x78};
          sendPacket(MessageType::DATA_RESPONSE, response_data, 4);
        }
        break;
        
      case MessageType::DATA_RESPONSE:
        Serial.printf("CPU: Received DATA_RESPONSE with %d bytes\n", packet.payload_length);
        Serial.print("CPU: Data: ");
        for(int i = 0; i < packet.payload_length; i++){
          Serial.printf("0x%02X ", packet.payload[i]);
        }
        Serial.println();
        break;
        
      case MessageType::COMMAND:
        Serial.printf("CPU: Received COMMAND (0x%02X)\n", 
                      packet.payload_length > 0 ? packet.payload[0] : 0);
        sendAck(1);  // Send ACK
        break;
        
      case MessageType::ACK:
        Serial.println("CPU: Received ACK");
        break;
        
      case MessageType::STATUS:
        Serial.printf("CPU: Received STATUS update\n");
        break;
        
      case MessageType::ERROR:
        Serial.println("CPU: Received ERROR notification");
        break;
        
      default:
        Serial.printf("CPU: Unknown message type: 0x%02X\n", 
                      static_cast<uint8_t>(packet.message_type));
        break;
    }
  }
};

// Global instance
CpuUartBidirectional uart_comm;

void setup(){
  // Initialize UART communication
  if(!uart_comm.init()){
    Serial.println("CPU: Failed to initialize UART communication");
    while(1){
      delay(1000);
    }
  }
  
  Serial.println("CPU: Setup complete - Ready for bidirectional communication");
  
  // Send initial greeting
  uint8_t greeting[] = "CPU Ready";
  uart_comm.sendPacket(MessageType::STATUS, greeting, sizeof(greeting) - 1);
}

void loop(){
  // Process communication
  uart_comm.update();
  
  // Example: Send data request every 5 seconds
  static unsigned long last_request_time = 0;
  unsigned long current_time = millis();
  
  if(current_time - last_request_time > 5000){
    Serial.println("CPU: Sending DATA_REQUEST to GPU");
    uint8_t request_id = 0x42;
    uart_comm.sendPacket(MessageType::DATA_REQUEST, &request_id, 1);
    last_request_time = current_time;
  }
  
  // Small delay to prevent overwhelming the system
  delay(10);
}
