/*****************************************************************
 * File:      GPU_BaudTest.cpp
 * Purpose:   Test UART baud rates with smaller packets (512B, 1KB, 2KB)
 * 
 * Responds to test packets with ACK at each baud rate
 *****************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "GPU_BAUD_TEST";

// Test baud rates (in order) - must match CPU
const uint32_t BAUD_RATES[] = {
  2000000,   // 2 Mbps
  3000000,   // 3 Mbps
  4000000,   // 4 Mbps
  5000000,   // 5 Mbps
  6000000,   // 6 Mbps
  8000000,   // 8 Mbps
  10000000,  // 10 Mbps
  12000000,  // 12 Mbps
  15000000,  // 15 Mbps
  20000000,  // 20 Mbps
};
const int NUM_BAUDS = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

// Test packet structure - must match CPU
const uint8_t SYNC_PATTERN[] = {0xAA, 0x55, 0xCC, 0x33};
const uint8_t TEST_512B_CMD = 0x01;
const uint8_t TEST_1KB_CMD = 0x02;
const uint8_t TEST_2KB_CMD = 0x03;
const uint8_t TEST_4KB_CMD = 0x04;
const uint8_t ACK_CMD = 0x05;

// Packet sizes
const int PACKET_512B = 512;
const int PACKET_1KB = 1024;
const int PACKET_2KB = 2048;
const int PACKET_4KB = 4096;
const int MAX_PACKET_SIZE = PACKET_4KB + 16;

// Buffer sizes
const int RX_BUF_SIZE = 16384;

// Pin configuration (GPU side)
const int UART_RX_PIN = 13;  // GPU RX <- CPU TX (GPIO12)
const int UART_TX_PIN = 12;  // GPU TX -> CPU RX (GPIO11)
const uart_port_t UART_NUM = UART_NUM_1;

// Buffers
uint8_t* rx_buffer = nullptr;
uint8_t tx_packet[8];
int rx_idx = 0;

// Statistics per baud rate
struct SizeStats {
  uint32_t rx;
  uint32_t ack;
};

struct BaudStats {
  uint32_t baud;
  SizeStats p512;
  SizeStats p1k;
  SizeStats p2k;
  SizeStats p4k;
  uint32_t sync_errors;
};
BaudStats stats[NUM_BAUDS];
int current_baud_idx = 0;

// Timing
uint32_t last_packet_time = 0;
uint32_t baud_start_time = 0;

void init_uart(uint32_t baud) {
  uart_driver_delete(UART_NUM);
  
  uart_config_t uart_config = {};
  uart_config.baud_rate = baud;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_DEFAULT;
  
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM, RX_BUF_SIZE, 1024, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  
  uart_flush(UART_NUM);
  rx_idx = 0;
}

void send_ack(uint8_t seq) {
  memcpy(tx_packet, SYNC_PATTERN, 4);
  tx_packet[4] = ACK_CMD;
  tx_packet[5] = seq;
  uart_write_bytes(UART_NUM, tx_packet, 6);
}

void switch_baud(int idx) {
  if (idx >= NUM_BAUDS) idx = 0;
  
  current_baud_idx = idx;
  uint32_t baud = BAUD_RATES[idx];
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "════════════════════════════════════════════════════════════");
  ESP_LOGI(TAG, "Switching to %lu baud (%lu Mbps)", baud, baud / 1000000);
  ESP_LOGI(TAG, "════════════════════════════════════════════════════════════");
  
  init_uart(baud);
  
  stats[idx].baud = baud;
  stats[idx].p512 = {0, 0};
  stats[idx].p1k = {0, 0};
  stats[idx].p2k = {0, 0};
  stats[idx].p4k = {0, 0};
  stats[idx].sync_errors = 0;
  
  baud_start_time = esp_timer_get_time() / 1000;
  last_packet_time = baud_start_time;
}

void print_all_stats() {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║                         GPU BAUD RATE TEST RESULTS (Small Packets)                            ║");
  ESP_LOGI(TAG, "╠══════════════╦═══════════════════╦═══════════════════╦═══════════════════╦════════════════════╣");
  ESP_LOGI(TAG, "║   Baud Rate  ║    512B Packet    ║     1KB Packet    ║     2KB Packet    ║    Sync Errors     ║");
  ESP_LOGI(TAG, "╠══════════════╬═══════════════════╬═══════════════════╬═══════════════════╬════════════════════╣");
  
  for (int i = 0; i < NUM_BAUDS; i++) {
    float p512_pct = stats[i].p512.rx > 0 ? 100.0f * stats[i].p512.ack / stats[i].p512.rx : 0;
    float p1k_pct = stats[i].p1k.rx > 0 ? 100.0f * stats[i].p1k.ack / stats[i].p1k.rx : 0;
    float p2k_pct = stats[i].p2k.rx > 0 ? 100.0f * stats[i].p2k.ack / stats[i].p2k.rx : 0;
    
    ESP_LOGI(TAG, "║ %4lu Mbps    ║  %2lu/%2lu (%5.1f%%)  ║  %2lu/%2lu (%5.1f%%)  ║  %2lu/%2lu (%5.1f%%)  ║      %8lu      ║",
             stats[i].baud / 1000000,
             stats[i].p512.ack, stats[i].p512.rx, p512_pct,
             stats[i].p1k.ack, stats[i].p1k.rx, p1k_pct,
             stats[i].p2k.ack, stats[i].p2k.rx, p2k_pct,
             stats[i].sync_errors);
  }
  
  ESP_LOGI(TAG, "╚══════════════╩═══════════════════╩═══════════════════╩═══════════════════╩════════════════════╝");
  ESP_LOGI(TAG, "");
}

void process_uart() {
  size_t available = 0;
  uart_get_buffered_data_len(UART_NUM, &available);
  
  if (available == 0) return;
  
  // Read available data
  int to_read = (available < (size_t)(MAX_PACKET_SIZE - rx_idx)) ? available : (MAX_PACKET_SIZE - rx_idx);
  int read = uart_read_bytes(UART_NUM, rx_buffer + rx_idx, to_read, 0);
  if (read > 0) rx_idx += read;
  
  // Process buffer
  while (rx_idx >= 6) {  // Minimum: SYNC(4) + CMD(1) + SEQ(1)
    // Look for sync pattern
    if (memcmp(rx_buffer, SYNC_PATTERN, 4) != 0) {
      memmove(rx_buffer, rx_buffer + 1, rx_idx - 1);
      rx_idx--;
      stats[current_baud_idx].sync_errors++;
      continue;
    }
    
    // Determine packet size from command
    uint8_t cmd = rx_buffer[4];
    int packet_size = 0;
    SizeStats* size_stats = nullptr;
    const char* size_name = "";
    
    if (cmd == TEST_512B_CMD) {
      packet_size = PACKET_512B + 6;
      size_stats = &stats[current_baud_idx].p512;
      size_name = "512B";
    } else if (cmd == TEST_1KB_CMD) {
      packet_size = PACKET_1KB + 6;
      size_stats = &stats[current_baud_idx].p1k;
      size_name = "1KB";
    } else if (cmd == TEST_2KB_CMD) {
      packet_size = PACKET_2KB + 6;
      size_stats = &stats[current_baud_idx].p2k;
      size_name = "2KB";
    } else if (cmd == TEST_4KB_CMD) {
      packet_size = PACKET_4KB + 6;
      size_stats = &stats[current_baud_idx].p4k;
      size_name = "4KB";
    } else {
      // Unknown command, skip sync
      memmove(rx_buffer, rx_buffer + 1, rx_idx - 1);
      rx_idx--;
      continue;
    }
    
    // Wait for full packet
    if (rx_idx < packet_size) {
      break;
    }
    
    // Full packet received
    uint8_t seq = rx_buffer[5];
    size_stats->rx++;
    send_ack(seq);
    size_stats->ack++;
    
    last_packet_time = esp_timer_get_time() / 1000;
    
    if (size_stats->rx % 10 == 0) {
      ESP_LOGI(TAG, "  [%lu Mbps] %s: %lu rx, %lu ack",
               BAUD_RATES[current_baud_idx] / 1000000,
               size_name,
               size_stats->rx, size_stats->ack);
    }
    
    // Remove processed packet
    if (rx_idx > packet_size) {
      memmove(rx_buffer, rx_buffer + packet_size, rx_idx - packet_size);
    }
    rx_idx -= packet_size;
  }
}

extern "C" void app_main() {
  rx_buffer = (uint8_t*)malloc(MAX_PACKET_SIZE);
  if (!rx_buffer) {
    ESP_LOGE(TAG, "Failed to allocate RX buffer!");
    return;
  }
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║              GPU UART BAUD RATE TEST (Small Packets: 512B, 1KB, 2KB)                          ║");
  ESP_LOGI(TAG, "╠═══════════════════════════════════════════════════════════════════════════════════════════════╣");
  ESP_LOGI(TAG, "║  RX: GPIO13  <-  CPU TX: GPIO12                                                               ║");
  ESP_LOGI(TAG, "║  TX: GPIO12  ->  CPU RX: GPIO11                                                               ║");
  ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════════════════════════════════════════════╝");
  ESP_LOGI(TAG, "");
  
  // Initialize statistics
  for (int i = 0; i < NUM_BAUDS; i++) {
    stats[i].baud = BAUD_RATES[i];
    stats[i].p512 = {0, 0};
    stats[i].p1k = {0, 0};
    stats[i].p2k = {0, 0};
    stats[i].sync_errors = 0;
  }
  
  switch_baud(0);
  
  while (1) {
    process_uart();
    
    uint32_t now = esp_timer_get_time() / 1000;
    
    // If no packets for 800ms, switch to next baud
    if (now - last_packet_time > 800) {
      ESP_LOGI(TAG, "  [%lu Mbps] Timeout - 512B: %lu/%lu, 1KB: %lu/%lu, 2KB: %lu/%lu, Err: %lu",
               BAUD_RATES[current_baud_idx] / 1000000,
               stats[current_baud_idx].p512.ack, stats[current_baud_idx].p512.rx,
               stats[current_baud_idx].p1k.ack, stats[current_baud_idx].p1k.rx,
               stats[current_baud_idx].p2k.ack, stats[current_baud_idx].p2k.rx,
               stats[current_baud_idx].sync_errors);
      
      current_baud_idx++;
      if (current_baud_idx >= NUM_BAUDS) {
        print_all_stats();
        ESP_LOGI(TAG, "Test complete! Restarting in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        current_baud_idx = 0;
      }
      switch_baud(current_baud_idx);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
