



/*****************************************************************
 * File:      CPU.cpp
 * Category:  testing/experimentation
 * Author:    XCR1793 (Feather Forge)
 *
 * Purpose:
 *    Arduino (CPU) side of UART bidirectional performance test.
 *    Initializes UART2 at 2Mbaud and prints throughput every 3s.
 *****************************************************************/

#include <Arduino.h>
#include "uart_perf_test.hpp"
#include "uart_perf_test_impl.hpp"

using namespace arcos::testing;

constexpr uint32_t TEST_DURATION_MS = 1000;
constexpr int TESTS_PER_CONFIG = 5;
constexpr uint32_t FIXED_BAUD = 10000000; // Test at 10 Mbps

struct PacketSizeResult{
  uint16_t packet_size;
  float cpu_to_gpu_mbps;
  float gpu_to_cpu_mbps;
  uint32_t cpu_sent;
  uint32_t gpu_received;
};

PacketSizeResult results[8]; // 64, 128, 256, 512, 1024, 2048, 4096, 8192

void setup(){
  Serial.begin(115200);
  while(!Serial){}
  Serial.println("CPU (Arduino) ESP32: UART Progressive Baud Test");
  Serial.println("UART2: TX=GPIO12, RX=GPIO11");
  Serial.println("Connect: CPU-TX(12) -> GPU-RX(13), CPU-RX(11) -> GPU-TX(12)");
  Serial.println("Testing from 1.0 Mbps to 10.0 Mbps in 0.5 Mbps steps");
  Serial.println();
  delay(3000); // Wait for GPU to be ready
}

void loop(){
  // Initialize UART once at 10 Mbps
  Serial2.setRxBufferSize(8192);
  Serial2.begin(FIXED_BAUD, SERIAL_8N1, 11, 12);
  delay(200);
  
  // Clear buffers
  uart_flush(UART_NUM_2);
  uint8_t dummy;
  while(uart_read_bytes(UART_NUM_2, &dummy, 1, 0) > 0);
  
  Serial.println("\n========== Dual Display Frame Test @ 10 Mbps ==========");
  Serial.println("HUB75 Main: 64x32x2 displays, RGB565 (5-bit), 60fps");
  Serial.println("  Frame size: 8192 bytes (8KB)");
  Serial.println("  Bandwidth: 3.93 Mbps");
  Serial.println("OLED HUD: 128x128, 1-bit mono, 15fps");
  Serial.println("  Frame size: 2048 bytes (2KB)");
  Serial.println("  Bandwidth: 0.25 Mbps");
  Serial.println("Total bandwidth: 4.18 Mbps");
  Serial.println("Test duration: 10 seconds");
  Serial.print("Heap - Total: ");
  Serial.print(ESP.getHeapSize() / 1024);
  Serial.print(" KB, Free: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  Serial.println("=======================================================\n");
  
  // Handshake with GPU
  Serial.println("Sending START signal to GPU...");
  uart_write_bytes(UART_NUM_2, "START", 5);
  uart_wait_tx_done(UART_NUM_2, 100 / portTICK_PERIOD_MS);
  
  delay(200); // Give GPU time to receive and process
  
  Serial.println("Waiting for GPU acknowledgment...");
  uint8_t ack;
  int timeout = 0;
  while(timeout < 50){
    if(uart_read_bytes(UART_NUM_2, &ack, 1, 100 / portTICK_PERIOD_MS) == 1 && ack == 'R'){
      Serial.println("GPU acknowledged. Starting transfer...\n");
      break;
    }
    timeout++;
  }
  
  if(timeout >= 50){
    Serial.println("ERROR: GPU did not respond!");
    delay(5000);
    return;
  }
  
  delay(500);
  
  // Display frame specifications
  const uint32_t HUB75_FRAME_SIZE = 8192;  // 64x32x2 RGB565 5-bit = 8KB
  const uint32_t OLED_FRAME_SIZE = 2048;   // 128x128 1-bit = 2KB
  const uint8_t HUB75_FPS = 60;
  const uint8_t OLED_FPS = 15;
  const uint32_t TEST_DURATION_SEC = 10;
  
  // Calculate total frames to send
  const uint32_t HUB75_FRAMES = HUB75_FPS * TEST_DURATION_SEC;  // 600 frames
  const uint32_t OLED_FRAMES = OLED_FPS * TEST_DURATION_SEC;    // 150 frames
  
  // Allocate frame buffers
  uint8_t* hub75_frame = (uint8_t*)malloc(HUB75_FRAME_SIZE);
  uint8_t* oled_frame = (uint8_t*)malloc(OLED_FRAME_SIZE);
  
  if(!hub75_frame || !oled_frame){
    Serial.println("ERROR: Failed to allocate frame buffers!");
    if(hub75_frame) free(hub75_frame);
    if(oled_frame) free(oled_frame);
    delay(10000);
    return;
  }
  
  // Fill with test patterns
  for(uint32_t i = 0; i < HUB75_FRAME_SIZE; i++){
    hub75_frame[i] = (i * 123) & 0xFF; // RGB test pattern
  }
  for(uint32_t i = 0; i < OLED_FRAME_SIZE; i++){
    oled_frame[i] = (i & 0x80) ? 0xFF : 0x00; // Checkerboard pattern
  }
  
  Serial.println("Starting transfer...\n");
  
  // Start timing
  uint64_t start_time = esp_timer_get_time();
  uint32_t total_hub75_bytes = 0;
  uint32_t total_oled_bytes = 0;
  
  // Calculate frame timing (in microseconds)
  const uint32_t HUB75_FRAME_INTERVAL_US = 1000000 / HUB75_FPS;  // 16667 us
  const uint32_t OLED_FRAME_INTERVAL_US = 1000000 / OLED_FPS;    // 66667 us
  
  uint64_t next_hub75_time = start_time;
  uint64_t next_oled_time = start_time;
  uint32_t hub75_sent = 0;
  uint32_t oled_sent = 0;
  
  // Send frames for 10 seconds, maintaining proper frame timing
  while(hub75_sent < HUB75_FRAMES || oled_sent < OLED_FRAMES){
    uint64_t current_time = esp_timer_get_time();
    
    // Send HUB75 frame if it's time
    if(hub75_sent < HUB75_FRAMES && current_time >= next_hub75_time){
      // Frame header: 'H' + frame number (2 bytes)
      uart_write_bytes(UART_NUM_2, "H", 1);
      uint8_t frame_num[2] = {(uint8_t)(hub75_sent >> 8), (uint8_t)(hub75_sent & 0xFF)};
      uart_write_bytes(UART_NUM_2, (const char*)frame_num, 2);
      // Frame data
      uart_write_bytes(UART_NUM_2, (const char*)hub75_frame, HUB75_FRAME_SIZE);
      
      total_hub75_bytes += HUB75_FRAME_SIZE + 3;
      hub75_sent++;
      next_hub75_time += HUB75_FRAME_INTERVAL_US;
      
      if(hub75_sent % 60 == 0){
        Serial.print("HUB75: ");
        Serial.print(hub75_sent);
        Serial.println(" frames sent");
      }
    }
    
    // Send OLED frame if it's time
    if(oled_sent < OLED_FRAMES && current_time >= next_oled_time){
      // Frame header: 'O' + frame number (2 bytes)
      uart_write_bytes(UART_NUM_2, "O", 1);
      uint8_t frame_num[2] = {(uint8_t)(oled_sent >> 8), (uint8_t)(oled_sent & 0xFF)};
      uart_write_bytes(UART_NUM_2, (const char*)frame_num, 2);
      // Frame data
      uart_write_bytes(UART_NUM_2, (const char*)oled_frame, OLED_FRAME_SIZE);
      
      total_oled_bytes += OLED_FRAME_SIZE + 3;
      oled_sent++;
      next_oled_time += OLED_FRAME_INTERVAL_US;
      
      if(oled_sent % 15 == 0){
        Serial.print("OLED: ");
        Serial.print(oled_sent);
        Serial.println(" frames sent");
      }
    }
    
    // Small delay to prevent busy-waiting
    delayMicroseconds(100);
  }
  
  // Wait for all data to be transmitted
  uart_wait_tx_done(UART_NUM_2, portMAX_DELAY);
  
  // Stop timing
  uint64_t end_time = esp_timer_get_time();
  uint64_t elapsed_us = end_time - start_time;
  
  free(hub75_frame);
  free(oled_frame);
  
  // Calculate results
  uint32_t total_bytes = total_hub75_bytes + total_oled_bytes;
  float elapsed_sec = elapsed_us / 1000000.0f;
  float total_mbps = (total_bytes * 8.0f) / elapsed_us;
  float hub75_mbps = (total_hub75_bytes * 8.0f) / elapsed_us;
  float oled_mbps = (total_oled_bytes * 8.0f) / elapsed_us;
  
  Serial.println("\n============== TRANSFER COMPLETE ==============");
  Serial.print("Test duration: ");
  Serial.print(elapsed_sec, 3);
  Serial.println(" seconds");
  Serial.println("\nHUB75 Main Display:");
  Serial.print("  Frames sent: ");
  Serial.println(hub75_sent);
  Serial.print("  Data sent: ");
  Serial.print(total_hub75_bytes / 1024);
  Serial.println(" KB");
  Serial.print("  Throughput: ");
  Serial.print(hub75_mbps, 2);
  Serial.println(" Mbps");
  Serial.print("  Actual FPS: ");
  Serial.println(hub75_sent / elapsed_sec, 2);
  Serial.println("\nOLED HUD:");
  Serial.print("  Frames sent: ");
  Serial.println(oled_sent);
  Serial.print("  Data sent: ");
  Serial.print(total_oled_bytes / 1024);
  Serial.println(" KB");
  Serial.print("  Throughput: ");
  Serial.print(oled_mbps, 2);
  Serial.println(" Mbps");
  Serial.print("  Actual FPS: ");
  Serial.println(oled_sent / elapsed_sec, 2);
  Serial.println("\nTotal:");
  Serial.print("  Total data: ");
  Serial.print(total_bytes / 1024);
  Serial.println(" KB");
  Serial.print("  Total throughput: ");
  Serial.print(total_mbps, 2);
  Serial.println(" Mbps");
  Serial.print("Heap after - Free: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  Serial.println("===============================================\n");
  
  Serial.println("Test complete. Restarting in 10 seconds...");
  delay(10000);
}