/*****************************************************************
 * GPU_Simple_UART.cpp - UART Byte Test
 * 
 * Uses OLD WORKING CODE initialization order:
 * 1. uart_driver_install() FIRST
 * 2. uart_param_config() SECOND
 * 3. uart_set_pin() THIRD
 * 
 * GPU Build (ESP-IDF Framework)
 *****************************************************************/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <cstring>

static const char* TAG = "UART_TEST";

constexpr int GPU_TX_PIN = 12;
constexpr int GPU_RX_PIN = 13;
constexpr int BAUD_RATE = 2000000;
constexpr uart_port_t UART_NUM = UART_NUM_1;

bool initUart() {
  ESP_LOGI(TAG, "Initializing UART using OLD WORKING CODE order...");
  
  // === OLD WORKING CODE INITIALIZATION ORDER ===
  
  // STEP 1: Configure UART parameters struct
  uart_config_t uart_config = {};
  uart_config.baud_rate = BAUD_RATE;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_APB;
  
  // STEP 2: Install UART driver FIRST (same as old code)
  esp_err_t err = uart_driver_install(UART_NUM, 8192, 2048, 0, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
    return false;
  }
  ESP_LOGI(TAG, "uart_driver_install: OK");
  
  // STEP 3: Configure UART parameters SECOND (same as old code)
  err = uart_param_config(UART_NUM, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
    return false;
  }
  ESP_LOGI(TAG, "uart_param_config: OK");
  
  // STEP 4: Set pins THIRD (same as old code)
  err = uart_set_pin(UART_NUM, GPU_TX_PIN, GPU_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
    return false;
  }
  ESP_LOGI(TAG, "uart_set_pin: OK (TX=%d, RX=%d)", GPU_TX_PIN, GPU_RX_PIN);
  
  return true;
}

extern "C" void app_main() {
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "  UART Byte Test - GPU");
  ESP_LOGI(TAG, "  TX=GPIO%d  RX=GPIO%d  Baud=%d", GPU_TX_PIN, GPU_RX_PIN, BAUD_RATE);
  ESP_LOGI(TAG, "========================================");
  
  if (!initUart()) {
    ESP_LOGE(TAG, "UART initialization failed!");
    return;
  }
  
  ESP_LOGI(TAG, "UART initialized successfully!");
  ESP_LOGI(TAG, "Starting TX/RX test...");
  
  uint8_t tx_byte = 0xAA;  // GPU sends 0xAA
  uint8_t rx_buffer[16];
  int cycle = 0;
  
  while (1) {
    cycle++;
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Cycle %d ===", cycle);
    
    // Send byte to CPU
    int sent = uart_write_bytes(UART_NUM, &tx_byte, 1);
    ESP_LOGI(TAG, "GPU TX: Sent 0x%02X (%d bytes written)", tx_byte, sent);
    
    // Wait a bit for response
    vTaskDelay(50 / portTICK_PERIOD_MS);
    
    // Check for received data from CPU
    size_t available = 0;
    uart_get_buffered_data_len(UART_NUM, &available);
    
    if (available > 0) {
      int len = uart_read_bytes(UART_NUM, rx_buffer, 
                                (available < 16 ? available : 16), 
                                10 / portTICK_PERIOD_MS);
      ESP_LOGI(TAG, "GPU RX: Received %d bytes:", len);
      for (int i = 0; i < len; i++) {
        ESP_LOGI(TAG, "  [%d] = 0x%02X", i, rx_buffer[i]);
      }
    } else {
      ESP_LOGW(TAG, "GPU RX: No data from CPU");
    }
    
    vTaskDelay(950 / portTICK_PERIOD_MS);  // ~1 second cycle
  }
}
