/*****************************************************************
 * File:      CPU.cpp
 * Category:  Main Application
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side main application for dual display transmission.
 *    Sends HUB75 frames at 60fps and OLED frames at 15fps.
 * 
 * Hardware:
 *    - ESP32-S3 (CPU)
 *    - UART to GPU: RX=GPIO11, TX=GPIO12
 * 
 * Display Configuration:
 *    - HUB75: 128x32 RGB at 60fps (min 30fps)
 *    - OLED:  128x128 monochrome at 15fps (min 10fps)
 * 
 * Bandwidth:
 *    - HUB75: 12,288 bytes × 60fps = 5.9 Mbps
 *    - OLED:  2,048 bytes × 15fps = 0.25 Mbps
 *    - Total: ~6.2 Mbps
 *****************************************************************/

#include <Arduino.h>
#include "Comms/CpuUartHandler.hpp"

using namespace arcos::comms;

// ============== Configuration ==============
// HUB75 Display
constexpr uint16_t HUB75_FRAME_WIDTH = 128;
constexpr uint16_t HUB75_FRAME_HEIGHT = 32;
constexpr uint32_t HUB75_FRAME_SIZE = HUB75_FRAME_WIDTH * HUB75_FRAME_HEIGHT * 3;
constexpr uint32_t HUB75_TARGET_FPS_VAL = 30;  // 30fps at 3 Mbps baud
constexpr uint32_t HUB75_MIN_FPS_VAL = 20;
constexpr uint32_t HUB75_FRAME_INTERVAL_US = 1000000 / HUB75_TARGET_FPS_VAL;

// OLED Display (128x128 monochrome, 1-bit per pixel)
constexpr uint16_t OLED_FRAME_WIDTH = 128;
constexpr uint16_t OLED_FRAME_HEIGHT = 128;
constexpr uint32_t OLED_FRAME_SIZE = (OLED_FRAME_WIDTH * OLED_FRAME_HEIGHT) / 8;  // 2048 bytes
constexpr uint32_t OLED_TARGET_FPS_VAL = 15;
constexpr uint32_t OLED_MIN_FPS_VAL = 10;
constexpr uint32_t OLED_FRAME_INTERVAL_US = 1000000 / OLED_TARGET_FPS_VAL;

// ============== Global Objects ==============
CpuUartHandler uart;
uint8_t hub75_buffer[HUB75_FRAME_SIZE];
uint8_t oled_buffer[OLED_FRAME_SIZE];

// Frame counters
uint16_t hub75_frame_count = 0;
uint16_t oled_frame_count = 0;

// Timing
uint32_t last_hub75_frame_time = 0;
uint32_t last_oled_frame_time = 0;
uint32_t last_stats_time = 0;
uint32_t hub75_frames_this_second = 0;
uint32_t oled_frames_this_second = 0;

// ============== Animation State ==============
uint8_t animation_phase = 0;
uint8_t oled_animation_phase = 0;
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
  for(int y = 0; y < HUB75_FRAME_HEIGHT; y++){
    for(int x = 0; x < HUB75_FRAME_WIDTH; x++){
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
      
      uint32_t idx = (y * HUB75_FRAME_WIDTH + x) * 3;
      hub75_buffer[idx] = r;
      hub75_buffer[idx + 1] = g;
      hub75_buffer[idx + 2] = b;
    }
  }
}

/** Generate a plasma-like pattern */
void generatePlasmaPattern(uint8_t phase){
  for(int y = 0; y < HUB75_FRAME_HEIGHT; y++){
    for(int x = 0; x < HUB75_FRAME_WIDTH; x++){
      // Simple plasma calculation
      uint8_t v1 = sin8((x * 8) + phase);
      uint8_t v2 = sin8((y * 8) + phase);
      uint8_t v3 = sin8((x * 4 + y * 4) + phase);
      
      uint8_t r = (v1 + v2) / 2;
      uint8_t g = (v2 + v3) / 2;
      uint8_t b = (v1 + v3) / 2;
      
      uint32_t idx = (y * HUB75_FRAME_WIDTH + x) * 3;
      hub75_buffer[idx] = r;
      hub75_buffer[idx + 1] = g;
      hub75_buffer[idx + 2] = b;
    }
  }
}

/** Generate scrolling bars pattern */
void generateBarsPattern(uint8_t offset){
  for(int y = 0; y < HUB75_FRAME_HEIGHT; y++){
    for(int x = 0; x < HUB75_FRAME_WIDTH; x++){
      uint8_t bar = ((x + offset) / 16) % 3;
      
      uint8_t r = (bar == 0) ? 255 : 0;
      uint8_t g = (bar == 1) ? 255 : 0;
      uint8_t b = (bar == 2) ? 255 : 0;
      
      uint32_t idx = (y * HUB75_FRAME_WIDTH + x) * 3;
      hub75_buffer[idx] = r;
      hub75_buffer[idx + 1] = g;
      hub75_buffer[idx + 2] = b;
    }
  }
}

/** Generate the current HUB75 animation frame - FAST scrolling rainbow */
void generateHub75Frame(){
  // Fast scrolling rainbow - optimized for 60fps
  for(int y = 0; y < HUB75_FRAME_HEIGHT; y++){
    for(int x = 0; x < HUB75_FRAME_WIDTH; x++){
      uint8_t hue = (x * 2 + animation_phase) & 0xFF;
      
      // Fast HSV to RGB (6 regions)
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
      
      uint32_t idx = (y * HUB75_FRAME_WIDTH + x) * 3;
      hub75_buffer[idx] = r;
      hub75_buffer[idx + 1] = g;
      hub75_buffer[idx + 2] = b;
    }
  }
  
  animation_phase += 3;  // Smooth scrolling speed
}

/** Generate OLED monochrome pattern (1-bit per pixel, packed into bytes) */
void generateOledFrame(){
  // Clear buffer first
  memset(oled_buffer, 0, OLED_FRAME_SIZE);
  
  // Simple moving pattern - faster than sqrt circles
  uint8_t offset = oled_animation_phase;
  
  for(int y = 0; y < OLED_FRAME_HEIGHT; y++){
    for(int x = 0; x < OLED_FRAME_WIDTH; x++){
      bool pixel_on = false;
      
      // Diagonal stripes pattern (very fast)
      if(((x + y + offset) / 8) % 2 == 0){
        pixel_on = true;
      }
      
      // Add a border rectangle
      if(x < 4 || x >= OLED_FRAME_WIDTH - 4 || y < 4 || y >= OLED_FRAME_HEIGHT - 4){
        pixel_on = true;
      }
      
      // Add crosshair in center
      if((x == 64 && y > 54 && y < 74) || (y == 64 && x > 54 && x < 74)){
        pixel_on = true;
      }
      
      // Pack pixel into byte (8 pixels per byte, MSB first)
      if(pixel_on){
        uint32_t byte_idx = (y * OLED_FRAME_WIDTH + x) / 8;
        uint8_t bit_idx = 7 - ((y * OLED_FRAME_WIDTH + x) % 8);  // MSB first
        oled_buffer[byte_idx] |= (1 << bit_idx);
      }
    }
  }
  
  oled_animation_phase += 2;
}
// ============================================================
// Setup
// ============================================================

void setup(){
  Serial.begin(115200);
  delay(3000); // 3 second delay for user observation

  Serial.println();
  Serial.println("========================================");
  Serial.println("  CPU Dual Display Transmission Test");
  Serial.println("========================================");
  Serial.println("  HUB75 Display:");
  Serial.printf("    - Resolution: %dx%d\n", HUB75_FRAME_WIDTH, HUB75_FRAME_HEIGHT);
  Serial.printf("    - Frame size: %lu bytes (RGB)\n", HUB75_FRAME_SIZE);
  Serial.printf("    - Target FPS: %lu (min %lu)\n", HUB75_TARGET_FPS_VAL, HUB75_MIN_FPS_VAL);
  Serial.println();
  Serial.println("  OLED Display:");
  Serial.printf("    - Resolution: %dx%d\n", OLED_FRAME_WIDTH, OLED_FRAME_HEIGHT);
  Serial.printf("    - Frame size: %lu bytes (1-bit mono)\n", OLED_FRAME_SIZE);
  Serial.printf("    - Target FPS: %lu (min %lu)\n", OLED_TARGET_FPS_VAL, OLED_MIN_FPS_VAL);
  Serial.println();
  Serial.printf("  UART baud: %lu (%.1f Mbps)\n", (uint32_t)UART_BAUD_RATE, UART_BAUD_RATE / 1000000.0);
  
  // Bandwidth calculation
  float hub75_bw = (HUB75_FRAME_SIZE + 16) * HUB75_TARGET_FPS_VAL * 8 / 1000000.0;
  float oled_bw = (OLED_FRAME_SIZE + 16) * OLED_TARGET_FPS_VAL * 8 / 1000000.0;
  Serial.printf("  Bandwidth: HUB75=%.2f Mbps, OLED=%.2f Mbps\n", hub75_bw, oled_bw);
  Serial.printf("  Total: %.2f Mbps\n", hub75_bw + oled_bw);
  Serial.println("========================================\n");
  
  // Initialize UART handler
  if(!uart.init()){
    Serial.println("[CPU] ERROR: Failed to initialize UART!");
    while(1) delay(1000);
  }
  Serial.println("[CPU] UART initialized");
  
  // Clear frame buffers
  memset(hub75_buffer, 0, HUB75_FRAME_SIZE);
  memset(oled_buffer, 0, OLED_FRAME_SIZE);
  
  last_hub75_frame_time = micros();
  last_oled_frame_time = micros();
  last_stats_time = millis();
  
  Serial.println("[CPU] Starting dual display transmission...\n");
}

// ============================================================
// Main Loop
// ============================================================

void loop(){
  uint32_t now_us = micros();
  uint32_t now_ms = millis();
  
  // ============ HUB75: Send at 60fps (independent timer) ============
  uint32_t hub75_elapsed = now_us - last_hub75_frame_time;
  if(hub75_elapsed >= HUB75_FRAME_INTERVAL_US){
    // Adjust for timing drift
    last_hub75_frame_time = now_us - (hub75_elapsed % HUB75_FRAME_INTERVAL_US);
    
    // Generate animation frame
    generateHub75Frame();
    
    // Send frame to GPU
    uart.sendFrame(hub75_buffer, HUB75_FRAME_WIDTH, HUB75_FRAME_HEIGHT, hub75_frame_count);
    
    hub75_frame_count++;
    hub75_frames_this_second++;
  }
  
  // ============ OLED: Send at 15fps (independent timer) ============
  uint32_t oled_elapsed = now_us - last_oled_frame_time;
  if(oled_elapsed >= OLED_FRAME_INTERVAL_US){
    // Adjust for timing drift
    last_oled_frame_time = now_us - (oled_elapsed % OLED_FRAME_INTERVAL_US);
    
    // Generate OLED animation frame
    generateOledFrame();
    
    // Send OLED frame to GPU
    uart.sendOledFrame(oled_buffer, oled_frame_count);
    
    oled_frame_count++;
    oled_frames_this_second++;
  }
  
  // Process incoming UART data (non-blocking)
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
    
    Serial.printf("[CPU] HUB75: %lu fps | OLED: %lu fps | TX: %lu KB | RTT: %lu us\n",
                  hub75_frames_this_second,
                  oled_frames_this_second,
                  stats.tx_bytes / 1024,
                  stats.last_rtt_us);
    
    hub75_frames_this_second = 0;
    oled_frames_this_second = 0;
    last_stats_time = now_ms;
    
    // Send periodic ping for latency measurement
    uart.sendPing(hub75_frame_count);
  }
}
