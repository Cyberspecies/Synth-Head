/*****************************************************************
 * File:      CPU.cpp
 * Category:  Main Application
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side main application for image transmission test.
 *    Sends animated frames to GPU at 60fps via UART.
 * 
 * Hardware:
 *    - ESP32-S3 (CPU)
 *    - UART to GPU: RX=GPIO11, TX=GPIO12
 * 
 * Test Pattern:
 *    - Generates animated color patterns
 *    - Sends 128x32 RGB frames at 60fps
 *    - Reports statistics every second
 *****************************************************************/

#include <Arduino.h>
#include "Comms/CpuUartHandler.hpp"

using namespace arcos::comms;

// ============== Configuration ==============
constexpr uint16_t FRAME_WIDTH = 128;
constexpr uint16_t FRAME_HEIGHT = 32;
constexpr uint32_t FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3;
constexpr uint32_t TARGET_FPS = 60;
constexpr uint32_t FRAME_INTERVAL_US = 1000000 / TARGET_FPS;

// ============== Global Objects ==============
CpuUartHandler uart;
uint8_t frame_buffer[FRAME_SIZE];
uint16_t frame_count = 0;
uint32_t last_frame_time = 0;
uint32_t last_stats_time = 0;
uint32_t frames_this_second = 0;

// ============== Animation State ==============
uint8_t animation_phase = 0;
uint8_t pattern_type = 0;

// Simple sin8 approximation
uint8_t sin8(uint8_t x){
  static const uint8_t sin_table[256] = {
    128,131,134,137,140,143,146,149,152,155,158,162,165,167,170,173,
    176,179,182,185,188,190,193,196,198,201,203,206,208,211,213,215,
    218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,
    245,246,248,249,250,250,251,252,253,253,254,254,254,255,255,255,
    255,255,255,255,254,254,254,253,253,252,251,250,250,249,248,246,
    245,244,243,241,240,238,237,235,234,232,230,228,226,224,222,220,
    218,215,213,211,208,206,203,201,198,196,193,190,188,185,182,179,
    176,173,170,167,165,162,158,155,152,149,146,143,140,137,134,131,
    128,124,121,118,115,112,109,106,103,100,97,93,90,88,85,82,
    79,76,73,70,67,65,62,59,57,54,52,49,47,44,42,40,
    37,35,33,31,29,27,25,23,21,20,18,17,15,14,12,11,
    10,9,7,6,5,5,4,3,2,2,1,1,1,0,0,0,
    0,0,0,0,1,1,1,2,2,3,4,5,5,6,7,9,
    10,11,12,14,15,17,18,20,21,23,25,27,29,31,33,35,
    37,40,42,44,47,49,52,54,57,59,62,65,67,70,73,76,
    79,82,85,88,90,93,97,100,103,106,109,112,115,118,121,124
  };
  return sin_table[x];
}

/** Generate a rainbow gradient pattern */
void generateRainbowPattern(uint8_t offset){
  for(int y = 0; y < FRAME_HEIGHT; y++){
    for(int x = 0; x < FRAME_WIDTH; x++){
      uint8_t hue = (x * 2 + offset) % 256;
      
      // HSV to RGB conversion (simplified)
      uint8_t region = hue / 43;
      uint8_t remainder = (hue - region * 43) * 6;
      
      uint8_t r, g, b;
      switch(region){
        case 0: r = 255; g = remainder; b = 0; break;
        case 1: r = 255 - remainder; g = 255; b = 0; break;
        case 2: r = 0; g = 255; b = remainder; break;
        case 3: r = 0; g = 255 - remainder; b = 255; break;
        case 4: r = remainder; g = 0; b = 255; break;
        default: r = 255; g = 0; b = 255 - remainder; break;
      }
      
      uint32_t idx = (y * FRAME_WIDTH + x) * 3;
      frame_buffer[idx] = r;
      frame_buffer[idx + 1] = g;
      frame_buffer[idx + 2] = b;
    }
  }
}

/** Generate a plasma-like pattern */
void generatePlasmaPattern(uint8_t phase){
  for(int y = 0; y < FRAME_HEIGHT; y++){
    for(int x = 0; x < FRAME_WIDTH; x++){
      // Simple plasma calculation
      uint8_t v1 = sin8((x * 8) + phase);
      uint8_t v2 = sin8((y * 8) + phase);
      uint8_t v3 = sin8((x * 4 + y * 4) + phase);
      
      uint8_t r = (v1 + v2) / 2;
      uint8_t g = (v2 + v3) / 2;
      uint8_t b = (v1 + v3) / 2;
      
      uint32_t idx = (y * FRAME_WIDTH + x) * 3;
      frame_buffer[idx] = r;
      frame_buffer[idx + 1] = g;
      frame_buffer[idx + 2] = b;
    }
  }
}

/** Generate scrolling bars pattern */
void generateBarsPattern(uint8_t offset){
  for(int y = 0; y < FRAME_HEIGHT; y++){
    for(int x = 0; x < FRAME_WIDTH; x++){
      uint8_t bar = ((x + offset) / 16) % 3;
      
      uint8_t r = (bar == 0) ? 255 : 0;
      uint8_t g = (bar == 1) ? 255 : 0;
      uint8_t b = (bar == 2) ? 255 : 0;
      
      uint32_t idx = (y * FRAME_WIDTH + x) * 3;
      frame_buffer[idx] = r;
      frame_buffer[idx + 1] = g;
      frame_buffer[idx + 2] = b;
    }
  }
}

/** Generate the current animation frame */
void generateFrame(){
  switch(pattern_type){
    case 0:
      generateRainbowPattern(animation_phase);
      break;
    case 1:
      generatePlasmaPattern(animation_phase);
      break;
    case 2:
      generateBarsPattern(animation_phase);
      break;
  }
  
  animation_phase += 2;
  
  // Switch patterns every 5 seconds
  static uint32_t last_pattern_switch = 0;
  if(millis() - last_pattern_switch > 5000){
    pattern_type = (pattern_type + 1) % 3;
    last_pattern_switch = millis();
    Serial.printf("[CPU] Switching to pattern %d\n", pattern_type);
  }
}

// ============================================================
// Setup
// ============================================================

void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("    CPU Image Transmission Test");
  Serial.println("========================================");
  Serial.printf("  Frame: %dx%d @ %lu fps\n", FRAME_WIDTH, FRAME_HEIGHT, TARGET_FPS);
  Serial.printf("  Frame size: %lu bytes\n", FRAME_SIZE);
  Serial.printf("  UART baud: %lu (%.1f Mbps)\n", (uint32_t)UART_BAUD_RATE, UART_BAUD_RATE / 1000000.0);
  Serial.println("========================================\n");
  
  // Initialize UART handler
  if(!uart.init()){
    Serial.println("[CPU] ERROR: Failed to initialize UART!");
    while(1) delay(1000);
  }
  Serial.println("[CPU] UART initialized");
  
  // Clear frame buffer
  memset(frame_buffer, 0, FRAME_SIZE);
  
  last_frame_time = micros();
  last_stats_time = millis();
  
  Serial.println("[CPU] Starting frame transmission...\n");
}

// ============================================================
// Main Loop
// ============================================================

void loop(){
  uint32_t now_us = micros();
  uint32_t now_ms = millis();
  
  // Send frame at target FPS
  if(now_us - last_frame_time >= FRAME_INTERVAL_US){
    last_frame_time = now_us;
    
    // Generate animation frame
    generateFrame();
    
    // Send frame to GPU
    uart.sendFrame(frame_buffer, FRAME_WIDTH, FRAME_HEIGHT, frame_count);
    
    frame_count++;
    frames_this_second++;
  }
  
  // Process incoming UART data
  uart.process();
  
  // Handle received messages
  if(uart.hasMessage()){
    MsgType type = uart.getLastMessageType();
    
    if(type == MsgType::PONG){
      // PONG received - RTT is already calculated in handler
    }
    
    uart.clearMessage();
  }
  
  // Print statistics every second
  if(now_ms - last_stats_time >= 1000){
    const auto& stats = uart.getStats();
    
    Serial.printf("[CPU] FPS: %lu | TX: %lu KB | RX: %lu B | Frames: %u | RTT: %lu us\n",
                  frames_this_second,
                  stats.tx_bytes / 1024,
                  stats.rx_bytes,
                  frame_count,
                  stats.last_rtt_us);
    
    frames_this_second = 0;
    last_stats_time = now_ms;
    
    // Send periodic ping for latency measurement
    uart.sendPing(frame_count);
  }
}
