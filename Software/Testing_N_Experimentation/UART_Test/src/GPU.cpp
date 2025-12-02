
/*****************************************************************
 * File:      GPU.cpp
 * Category:  testing/experimentation
 * Author:    XCR1793 (Feather Forge)
 *
 * Purpose:
 *    ESP-IDF (GPU) side of UART bidirectional performance test.
 *    Initializes UART2 at 2Mbaud and prints throughput every 3s.
 *****************************************************************/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart_perf_test.hpp"
#include "uart_perf_test_impl.hpp"

using namespace arcos::testing;

#define TEST_DURATION_MS 1000
#define TESTS_PER_CONFIG 5
#define FIXED_BAUD 10000000

extern "C" void app_main(){
  printf("GPU (ESP-IDF) ESP32: 600 Frame Receiver @ 10 Mbps\n");
  printf("UART2: TX=GPIO12, RX=GPIO13\n");
  printf("Connect: GPU-TX(12) -> CPU-RX(11), GPU-RX(13) -> CPU-TX(12)\n\n");
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  
  const uart_port_t uart_num = UART_NUM_2;
  
  // Initialize UART once at 10 Mbps
  uart_config_t uart_config = {};
  uart_config.baud_rate = FIXED_BAUD;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.rx_flow_ctrl_thresh = 122;
  uart_config.source_clk = UART_SCLK_DEFAULT;
  
  uart_param_config(uart_num, &uart_config);
  uart_set_pin(uart_num, 12, 13, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(uart_num, 8192, 8192, 0, NULL, 0);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  uart_flush(uart_num);
  
  printf("========== Waiting for CPU START signal ==========\n");
  
  // Wait for START signal - read it all at once with timeout
  char start_buf[5];
  int total_read = 0;
  int attempts = 0;
  
  while(total_read < 5 && attempts < 100){
    int len = uart_read_bytes(uart_num, (uint8_t*)&start_buf[total_read], 5 - total_read, 100 / portTICK_PERIOD_MS);
    if(len > 0){
      total_read += len;
    }
    attempts++;
  }
  
  if(total_read == 5 && strncmp(start_buf, "START", 5) == 0){
    printf("START signal received! Sending acknowledgment...\n");
    uart_write_bytes(uart_num, "R", 1);
    uart_wait_tx_done(uart_num, 100 / portTICK_PERIOD_MS);
    printf("Ready signal sent.\n\n");
  } else {
    printf("ERROR: Did not receive START signal correctly (got %d bytes)\n", total_read);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
  }
  
  vTaskDelay(500 / portTICK_PERIOD_MS);
  
  // Prepare receive buffer
  const uint16_t PACKET_SIZE = 1024;
  const uint32_t FRAME_SIZE = 81920; // 80KB
  const uint32_t PACKETS_PER_FRAME = FRAME_SIZE / PACKET_SIZE; // 80 packets
  const uint32_t TOTAL_FRAMES = 600;
  
  uint8_t* packet = (uint8_t*)malloc(PACKET_SIZE);
  if(!packet){
    printf("ERROR: Failed to allocate receive buffer!\n");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    esp_restart();
  }
  
  printf("========== Receiving Dual Display Frames ==========\n");
  printf("HUB75: 8KB frames, 60fps target\n");
  printf("OLED: 2KB frames, 15fps target\n");
  printf("Test duration: 10 seconds\n");
  printf("Heap - Free: %lu KB\n", (unsigned long)esp_get_free_heap_size() / 1024);
  printf("===================================================\n\n");
  
  // Allocate receive buffers
  const uint32_t HUB75_FRAME_SIZE = 8192;
  const uint32_t OLED_FRAME_SIZE = 2048;
  const uint32_t MAX_FRAME_SIZE = HUB75_FRAME_SIZE;
  
  uint8_t* frame_buffer = (uint8_t*)malloc(MAX_FRAME_SIZE);
  if(!frame_buffer){
    printf("ERROR: Failed to allocate frame buffer!\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
  }
  
  // Start timing
  uint64_t start_time = esp_timer_get_time();
  uint32_t hub75_received = 0;
  uint32_t oled_received = 0;
  uint32_t total_hub75_bytes = 0;
  uint32_t total_oled_bytes = 0;
  uint32_t total_frames = 0;
  
  printf("Receiving frames...\n");
  
  // Receive frames for 10 seconds (600 HUB75 + 150 OLED = 750 total)
  const uint32_t EXPECTED_TOTAL_FRAMES = 750;
  
  while(total_frames < EXPECTED_TOTAL_FRAMES){
    // Read frame header (type byte)
    uint8_t frame_type;
    int len = uart_read_bytes(uart_num, &frame_type, 1, 1000 / portTICK_PERIOD_MS);
    if(len != 1){
      // Timeout - check if we're done
      uint64_t current_time = esp_timer_get_time();
      if((current_time - start_time) > 15000000){ // 15 second timeout
        printf("Timeout waiting for frames\n");
        break;
      }
      continue;
    }
    
    // Read frame number (2 bytes)
    uint8_t frame_num[2];
    int received = 0;
    while(received < 2){
      len = uart_read_bytes(uart_num, frame_num + received, 2 - received, portMAX_DELAY);
      if(len > 0) received += len;
    }
    uint16_t frame_number = (frame_num[0] << 8) | frame_num[1];
    
    // Receive frame data based on type
    if(frame_type == 'H'){
      // HUB75 frame
      received = 0;
      while(received < HUB75_FRAME_SIZE){
        len = uart_read_bytes(uart_num, frame_buffer + received, HUB75_FRAME_SIZE - received, portMAX_DELAY);
        if(len > 0) received += len;
      }
      total_hub75_bytes += received + 3;
      hub75_received++;
      
      if(hub75_received % 60 == 0){
        printf("HUB75: %lu frames received\n", (unsigned long)hub75_received);
      }
    }
    else if(frame_type == 'O'){
      // OLED frame
      received = 0;
      while(received < OLED_FRAME_SIZE){
        len = uart_read_bytes(uart_num, frame_buffer + received, OLED_FRAME_SIZE - received, portMAX_DELAY);
        if(len > 0) received += len;
      }
      total_oled_bytes += received + 3;
      oled_received++;
      
      if(oled_received % 15 == 0){
        printf("OLED: %lu frames received\n", (unsigned long)oled_received);
      }
    }
    else{
      printf("Warning: Unknown frame type 0x%02X\n", frame_type);
    }
    
    total_frames++;
  }
  
  // Stop timing
  uint64_t end_time = esp_timer_get_time();
  uint64_t elapsed_us = end_time - start_time;
  
  free(frame_buffer);
  
  // Calculate results
  float elapsed_sec = elapsed_us / 1000000.0f;
  uint32_t total_bytes = total_hub75_bytes + total_oled_bytes;
  float total_mbps = (total_bytes * 8.0f) / elapsed_us;
  float hub75_mbps = (total_hub75_bytes * 8.0f) / elapsed_us;
  float oled_mbps = (total_oled_bytes * 8.0f) / elapsed_us;
  
  printf("\n============== RECEPTION COMPLETE =============\n");
  printf("Test duration: %.3f seconds\n", elapsed_sec);
  printf("\nHUB75 Main Display:\n");
  printf("  Frames received: %lu\n", (unsigned long)hub75_received);
  printf("  Data received: %lu KB\n", (unsigned long)(total_hub75_bytes / 1024));
  printf("  Throughput: %.2f Mbps\n", hub75_mbps);
  printf("  Actual FPS: %.2f\n", hub75_received / elapsed_sec);
  printf("\nOLED HUD:\n");
  printf("  Frames received: %lu\n", (unsigned long)oled_received);
  printf("  Data received: %lu KB\n", (unsigned long)(total_oled_bytes / 1024));
  printf("  Throughput: %.2f Mbps\n", oled_mbps);
  printf("  Actual FPS: %.2f\n", oled_received / elapsed_sec);
  printf("\nTotal:\n");
  printf("  Total frames: %lu\n", (unsigned long)total_frames);
  printf("  Total data: %lu KB\n", (unsigned long)(total_bytes / 1024));
  printf("  Total throughput: %.2f Mbps\n", total_mbps);
  printf("Heap after - Free: %lu KB\n", (unsigned long)esp_get_free_heap_size() / 1024);
  printf("===============================================\n\n");
  
  printf("Test complete. Restarting in 10 seconds...\n");
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  esp_restart();
}