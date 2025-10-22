/**
 * @file main.cpp
 * @brief UART-based LED controller with button feedback
 * 
 * This application receives LED data via UART and controls 4 LED strips:
 * - Left Fin: 13 RGBW LEDs
 * - Right Fin: 13 RGBW LEDs
 * - Tongue: 9 RGBW LEDs
 * - Scale: 14 RGBW LEDs
 * 
 * Total: 49 LEDs × 4 bytes (RGBW) = 196 bytes per frame
 * 
 * Also reads 4 buttons (A, B, C, D) and sends their state via UART.
 */

#include <Arduino.h>
#include "UartController.h"
#include "LedController_new.h"

// Include implementations
#include "UartController.impl.hpp"
#include "LedController_new.impl.hpp"

// Global objects
UartController uart_controller;
LedController led_controller;

// Timing variables
unsigned long last_update_time = 0;
unsigned long last_button_send_time = 0;
unsigned long last_debug_print_time = 0;
unsigned long last_led_update_time = 0; // Track LED update timing

// Configuration
const unsigned long LED_UPDATE_INTERVAL = 16;     // Update LEDs every 16ms (60 FPS max)
const unsigned long BUTTON_SEND_INTERVAL = 50;    // Send button state every 50ms (20Hz)
const unsigned long DEBUG_PRINT_INTERVAL = 500;   // Debug print every 500ms
const unsigned long UART_DEBUG_INTERVAL = 100;    // UART data debug every 100ms

// Debug mode
bool debug_mode = true;
unsigned long last_uart_debug_time = 0;
int frame_count = 0;

void setup(){
  // Initialize serial for debugging
  Serial.begin(115200);
  while(!Serial && millis() < 3000){
    ; // Wait for serial connection (max 3 seconds)
  }
  
  Serial.println("\n\n========================================");
  Serial.println("  UART LED Controller with Buttons");
  Serial.println("========================================\n");

  // Initialize UART controller
  if(!uart_controller.initialize()){
    Serial.println("FATAL ERROR: Failed to initialize UART Controller");
    while(1){
      delay(1000);
    }
  }
  
  // UART diagnostic - wait a moment for data to arrive
  Serial.println("\n=== UART RX Diagnostic ===");
  for(int i = 0; i < 10; i++){
    delay(100);
    int available = Serial1.available();
    Serial.printf("  [%d] UART RX buffer: %d bytes\n", i, available);
    if(available > 0){
      Serial.println("  ✓ Data detected!");
      break;
    }
  }
  Serial.println("=========================\n");

  // Initialize LED controller
  if(!led_controller.initialize()){
    Serial.println("FATAL ERROR: Failed to initialize LED Controller");
    while(1){
      delay(1000);
    }
  }

  // Run test pattern
  Serial.println("\nRunning LED test pattern...");
  led_controller.testPattern();
  
  Serial.println("\nSystem ready!");
  Serial.println("Waiting for UART data...\n");
  
  // Initialize timing
  last_update_time = millis();
  last_button_send_time = millis();
  last_debug_print_time = millis();
}

void loop(){
  unsigned long current_time = millis();

  // Debug: Check UART buffer status
  static unsigned long last_buffer_check = 0;
  if(current_time - last_buffer_check >= 100){
    int buffer_bytes = Serial1.available();
    if(buffer_bytes > 0 && buffer_bytes < 197){
      Serial.printf("!!! PARTIAL FRAME: %d bytes in buffer (need 197) !!!\n", buffer_bytes);
    }
    last_buffer_check = current_time;
  }

  // Update UART controller (read buttons)
  uart_controller.update();

  // Try to receive full 197-byte LED frame (keep reading, store latest)
  // Read ALL available frames to prevent buffer overflow
  static bool has_new_frame = false;
  static unsigned long last_frame_receive_time = 0;
  static unsigned long frames_this_second = 0;
  static unsigned long last_fps_print = 0;
  
  // Read multiple frames if available (drain the UART buffer)
  int frames_read_this_loop = 0;
  while(uart_controller.receiveData() && frames_read_this_loop < 10){
    frame_count++;
    has_new_frame = true; // Mark that we have new data
    frames_this_second++;
    frames_read_this_loop++;
    last_frame_receive_time = current_time;
  }
  
  // Print actual receive FPS every second
  if(current_time - last_fps_print >= 1000){
    Serial.printf(">>> RECEIVE FPS: %lu frames/sec\n", frames_this_second);
    frames_this_second = 0;
    last_fps_print = current_time;
  }
  
  // Print frame info every 60 frames (approximately once per second at 60 FPS)
  if(frame_count % 60 == 0 && has_new_frame){
    const uint8_t* left_data = uart_controller.getLeftFinData();
    uint32_t total_received = uart_controller.getTotalFramesReceived();
    uint32_t total_skipped = uart_controller.getFramesSkipped();
    uint32_t total_corrupted = uart_controller.getFramesCorrupted();
    uint32_t total_sync_fail = uart_controller.getSyncFailures();
    uint8_t frame_counter = uart_controller.getFrameCounter();
    float skip_rate = total_received > 0 ? (total_skipped * 100.0f / total_received) : 0.0f;
    
    Serial.printf("Frame %d | Counter=%d | LED[0]: R=%d G=%d B=%d W=%d\n",
                  frame_count, frame_counter,
                  left_data[0], left_data[1], left_data[2], left_data[3]);
    Serial.printf("  Skipped=%lu (%.1f%%) | Corrupted=%lu | Sync_Fail=%lu | Buf=%d\n",
                  total_skipped, skip_rate, total_corrupted, total_sync_fail,
                  Serial1.available());
  }
  
  // Update physical LEDs at controlled rate (60 FPS max) to prevent stuttering
  static unsigned long led_updates_this_second = 0;
  static unsigned long last_led_fps_print = 0;
  
  if(has_new_frame && (current_time - last_led_update_time >= LED_UPDATE_INTERVAL)){
    unsigned long led_update_start = micros();
    
    last_led_update_time = current_time;
    has_new_frame = false;
    led_updates_this_second++;
    
    // Update all LED strips with latest received data
    led_controller.updateFromUartData(
      uart_controller.getLeftFinData(),
      uart_controller.getRightFinData(),
      uart_controller.getTongueData(),
      uart_controller.getScaleData()
    );
    
    unsigned long led_update_time = micros() - led_update_start;
    
    // Print LED update FPS and timing every second
    if(current_time - last_led_fps_print >= 1000){
      Serial.printf(">>> LED UPDATE FPS: %lu updates/sec | Last update took: %lu us\n", 
                    led_updates_this_second, led_update_time);
      led_updates_this_second = 0;
      last_led_fps_print = current_time;
    }
  }
  
  // Send button state periodically
  if(current_time - last_button_send_time >= BUTTON_SEND_INTERVAL){
    last_button_send_time = current_time;
    uart_controller.sendButtonState();
  }

  // No delay - process as fast as possible
}


