/**
 * @file wifi_led_cpu_receiver.cpp
 * @brief CPU WiFi LED Receiver - Receives LED data via WiFi, sends button data
 * 
 * Flow:
 * 1. CPU connects to WiFi
 * 2. CPU sends WiFi config (SSID, password, IP) to GPU via UART
 * 3. CPU receives LED data via UDP
 * 4. CPU sends button state via UDP
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "LedController_new.h"
#include "LedController_new.impl.hpp"
#include "WiFiLedProtocol.h"

// WiFi credentials - CHANGE THESE TO YOUR NETWORK
const char* WIFI_SSID = "YourSSID";
const char* WIFI_PASSWORD = "YourPassword";

// UDP objects
WiFiUDP udp_led;        // Receive LED data
WiFiUDP udp_button;     // Send button data

// LED controller
LedController led_controller;

// Button pins (from PIN_MAPPING_CPU.md)
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define BUTTON_C_PIN 7
#define BUTTON_D_PIN 15

// Statistics
uint32_t frames_received = 0;
uint32_t frames_corrupted = 0;
uint32_t frames_skipped = 0;
uint8_t last_frame_counter = 0;
unsigned long last_stats_print = 0;

// GPU IP address (will be set when we receive first packet)
IPAddress gpu_ip;
bool gpu_ip_known = false;

/**
 * @brief Calculate CRC8 checksum
 */
uint8_t calculateCRC8(const uint8_t* data, size_t length){
  uint8_t crc = 0x00;
  for(size_t i = 0; i < length; i++){
    crc ^= data[i];
    for(int j = 0; j < 8; j++){
      if(crc & 0x80){
        crc = (crc << 1) ^ 0x07;
      }else{
        crc <<= 1;
      }
    }
  }
  return crc;
}

/**
 * @brief Initialize buttons
 */
void initButtons(){
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_C_PIN, INPUT_PULLUP);
  pinMode(BUTTON_D_PIN, INPUT_PULLUP);
  
  Serial.println("Buttons initialized:");
  Serial.printf("  Button A: GPIO %d\n", BUTTON_A_PIN);
  Serial.printf("  Button B: GPIO %d\n", BUTTON_B_PIN);
  Serial.printf("  Button C: GPIO %d\n", BUTTON_C_PIN);
  Serial.printf("  Button D: GPIO %d\n", BUTTON_D_PIN);
}

/**
 * @brief Send WiFi config to GPU via UART
 */
void sendWiFiConfigToGPU(){
  WiFiConfig config;
  config.sync1 = WIFI_CONFIG_SYNC_1;
  config.sync2 = WIFI_CONFIG_SYNC_2;
  
  strncpy(config.ssid, WIFI_SSID, WIFI_SSID_MAX_LEN - 1);
  config.ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
  
  strncpy(config.password, WIFI_PASSWORD, WIFI_PASSWORD_MAX_LEN - 1);
  config.password[WIFI_PASSWORD_MAX_LEN - 1] = '\0';
  
  config.cpu_ip = (uint32_t)WiFi.localIP();
  config.led_port = DEFAULT_LED_PORT;
  config.button_port = DEFAULT_BUTTON_PORT;
  
  // Calculate CRC (exclude CRC byte itself)
  config.crc = calculateCRC8((uint8_t*)&config, sizeof(WiFiConfig) - 1);
  
  // Send via UART (Serial1: TX=GPIO12, RX=GPIO11)
  Serial1.begin(921600, SERIAL_8N1, 11, 12);
  Serial1.write((uint8_t*)&config, sizeof(WiFiConfig));
  
  Serial.println("\n=== WiFi Config Sent to GPU ===");
  Serial.printf("SSID: %s\n", config.ssid);
  Serial.printf("CPU IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("LED Port: %d\n", config.led_port);
  Serial.printf("Button Port: %d\n", config.button_port);
  Serial.println("===============================\n");
}

/**
 * @brief Connect to WiFi
 */
void connectWiFi(){
  Serial.println("\n=== Connecting to WiFi ===");
  Serial.printf("SSID: %s\n", WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20){
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\n✓ WiFi Connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
    Serial.println("==========================\n");
  }else{
    Serial.println("\n✗ WiFi Connection Failed!");
    Serial.println("Check your SSID and password");
    while(1) delay(1000);
  }
}

/**
 * @brief Process received LED packet
 */
void processLedPacket(LedDataPacket* packet, size_t length, IPAddress from_ip){
  // Validate packet size
  if(length != sizeof(LedDataPacket)){
    Serial.printf("Invalid packet size: %d (expected %d)\n", length, sizeof(LedDataPacket));
    return;
  }
  
  // Validate magic number
  if(packet->magic != LED_PACKET_MAGIC){
    Serial.printf("Invalid magic: 0x%04X (expected 0x%04X)\n", packet->magic, LED_PACKET_MAGIC);
    return;
  }
  
  // Validate CRC
  uint8_t calculated_crc = calculateCRC8((uint8_t*)packet, sizeof(LedDataPacket) - 1);
  if(packet->crc != calculated_crc){
    frames_corrupted++;
    return;
  }
  
  // Remember GPU IP from first valid packet
  if(!gpu_ip_known){
    gpu_ip = from_ip;
    gpu_ip_known = true;
    Serial.printf("GPU IP detected: %s\n", gpu_ip.toString().c_str());
  }
  
  // Detect frame skipping
  if(frames_received > 0){
    uint8_t expected = last_frame_counter + 1;
    if(expected > 60) expected = 1;
    
    if(packet->frame_counter != expected){
      int skipped = (packet->frame_counter > expected) 
                    ? (packet->frame_counter - expected)
                    : ((60 - expected) + packet->frame_counter);
      frames_skipped += skipped;
    }
  }
  
  last_frame_counter = packet->frame_counter;
  frames_received++;
  
  // Update LEDs
  // LED data layout: [Left Fin 13×4][Right Fin 13×4][Tongue 9×4][Scale 14×4]
  led_controller.updateFromUartData(
    &packet->led_data[0],                                          // Left Fin
    &packet->led_data[LEFT_FIN_COUNT * 4],                        // Right Fin
    &packet->led_data[(LEFT_FIN_COUNT + RIGHT_FIN_COUNT) * 4],    // Tongue
    &packet->led_data[(LEFT_FIN_COUNT + RIGHT_FIN_COUNT + TONGUE_COUNT) * 4] // Scale
  );
}

/**
 * @brief Send button state to GPU
 */
void sendButtonState(){
  if(!gpu_ip_known){
    return; // Don't know where to send yet
  }
  
  ButtonDataPacket packet;
  packet.magic = BUTTON_PACKET_MAGIC;
  packet.button_a = digitalRead(BUTTON_A_PIN) ? 0x00 : 0x01; // Active LOW
  packet.button_b = digitalRead(BUTTON_B_PIN) ? 0x00 : 0x01;
  packet.button_c = digitalRead(BUTTON_C_PIN) ? 0x00 : 0x01;
  packet.button_d = digitalRead(BUTTON_D_PIN) ? 0x00 : 0x01;
  packet.crc = calculateCRC8((uint8_t*)&packet, sizeof(ButtonDataPacket) - 1);
  
  udp_button.beginPacket(gpu_ip, DEFAULT_BUTTON_PORT);
  udp_button.write((uint8_t*)&packet, sizeof(ButtonDataPacket));
  udp_button.endPacket();
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("  WiFi LED Controller - CPU Receiver");
  Serial.println("========================================\n");
  
  // Initialize buttons
  initButtons();
  
  // Initialize LED controller
  if(!led_controller.initialize()){
    Serial.println("FATAL ERROR: Failed to initialize LED Controller");
    while(1) delay(1000);
  }
  
  // Test pattern
  Serial.println("\nRunning LED test pattern...");
  led_controller.testPattern();
  delay(1000);
  
  // Connect to WiFi
  connectWiFi();
  
  // Start UDP listeners
  if(udp_led.begin(DEFAULT_LED_PORT)){
    Serial.printf("✓ UDP LED listener started on port %d\n", DEFAULT_LED_PORT);
  }else{
    Serial.println("✗ Failed to start UDP LED listener");
  }
  
  if(udp_button.begin(DEFAULT_BUTTON_PORT + 1)){ // Different port for sending
    Serial.printf("✓ UDP Button sender initialized\n");
  }
  
  // Send WiFi config to GPU via UART
  delay(500);
  sendWiFiConfigToGPU();
  
  Serial.println("\n✓ System ready - waiting for LED data...\n");
  last_stats_print = millis();
}

void loop(){
  unsigned long current_time = millis();
  
  // Receive LED packets
  int packet_size = udp_led.parsePacket();
  if(packet_size > 0){
    LedDataPacket packet;
    int len = udp_led.read((uint8_t*)&packet, sizeof(LedDataPacket));
    
    if(len > 0){
      IPAddress from_ip = udp_led.remoteIP();
      processLedPacket(&packet, len, from_ip);
    }
  }
  
  // Send button state every 50ms
  static unsigned long last_button_send = 0;
  if(current_time - last_button_send >= 50){
    last_button_send = current_time;
    sendButtonState();
  }
  
  // Print statistics every second
  if(current_time - last_stats_print >= 1000){
    last_stats_print = current_time;
    
    static uint32_t last_frame_count = 0;
    uint32_t fps = frames_received - last_frame_count;
    last_frame_count = frames_received;
    
    float skip_rate = frames_received > 0 ? (frames_skipped * 100.0f / frames_received) : 0.0f;
    
    Serial.printf("FPS: %lu | Total: %lu | Skipped: %lu (%.1f%%) | Corrupted: %lu | WiFi: %d dBm\n",
                  fps, frames_received, frames_skipped, skip_rate, 
                  frames_corrupted, WiFi.RSSI());
  }
}
