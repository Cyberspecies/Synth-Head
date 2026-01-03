/*****************************************************************
 * File:      GPU_GpuDriver_Example.cpp
 * Category:  Example / GPU Side
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side example that receives commands from CPU and renders.
 *    Uses GpuRenderer to process commands and update displays.
 * 
 * Usage:
 *    1. Upload this file to GPU (COM5)
 *    2. Upload CPU_GpuDriver_Example.cpp to CPU (COM15)
 *    3. Watch the displays for graphics demo
 *****************************************************************/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

// Include the GPU renderer
#include "GpuDriver/GpuRenderer.hpp"

static const char* TAG = "GPU_MAIN";

using namespace gpu;

// ============================================================
// Hardware Configuration
// ============================================================

#define UART_NUM        UART_NUM_1
#define UART_TX_PIN     GPIO_NUM_12
#define UART_RX_PIN     GPIO_NUM_13
#define UART_BUF_SIZE   8192

// ============================================================
// Display Hardware Interface (stub - replace with real drivers)
// ============================================================

// Simulated display buffers
static uint8_t hub75_display[HUB75_WIDTH * HUB75_HEIGHT * 3];
static uint8_t oled_display[OLED_WIDTH * OLED_HEIGHT / 8];

void displayInit() {
  ESP_LOGI(TAG, "Initializing displays...");
  
  // TODO: Initialize actual HUB75 driver
  // hub75_init();
  
  // TODO: Initialize actual OLED driver
  // oled_init();
  
  memset(hub75_display, 0, sizeof(hub75_display));
  memset(oled_display, 0, sizeof(oled_display));
  
  ESP_LOGI(TAG, "Displays initialized (simulated)");
}

void displayRefresh(const uint8_t* hub75_buf, const uint8_t* oled_buf) {
  // Copy to display buffers
  if (hub75_buf) {
    memcpy(hub75_display, hub75_buf, sizeof(hub75_display));
    // TODO: Refresh actual HUB75
    // hub75_refresh(hub75_display);
  }
  
  if (oled_buf) {
    memcpy(oled_display, oled_buf, sizeof(oled_display));
    // TODO: Refresh actual OLED
    // oled_refresh(oled_display);
  }
}

// ============================================================
// UART Configuration
// ============================================================

void uartInit() {
  ESP_LOGI(TAG, "Configuring UART at %lu baud...", (uint32_t)GPU_BAUD_RATE);
  
  uart_config_t uart_config = {
    .baud_rate = GPU_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_DEFAULT,
    .flags = { .allow_pd = 0, .backup_before_sleep = 0 }
  };
  
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, 
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 
                                       0, NULL, 0));
  
  ESP_LOGI(TAG, "UART initialized: TX=%d, RX=%d", UART_TX_PIN, UART_RX_PIN);
}

// ============================================================
// GPU Renderer Instance
// ============================================================

GpuRenderer renderer;
uint8_t rx_buffer[UART_BUF_SIZE];
size_t rx_buffer_pos = 0;

// ============================================================
// UART Handler Functions
// ============================================================

size_t uartRead(uint8_t* buffer, size_t len) {
  return uart_read_bytes(UART_NUM, buffer, len, 0);
}

void uartWrite(const uint8_t* data, size_t len) {
  uart_write_bytes(UART_NUM, data, len);
}

// ============================================================
// Command Processing Task
// ============================================================

void commandTask(void* pvParameters) {
  ESP_LOGI(TAG, "Command processing task started");
  
  uint8_t temp_buf[256];
  
  while (1) {
    // Read available UART data
    int bytes_read = uartRead(temp_buf, sizeof(temp_buf));
    
    if (bytes_read > 0) {
      // Add to receive buffer
      if (rx_buffer_pos + bytes_read < sizeof(rx_buffer)) {
        memcpy(rx_buffer + rx_buffer_pos, temp_buf, bytes_read);
        rx_buffer_pos += bytes_read;
      } else {
        // Buffer overflow - reset
        ESP_LOGW(TAG, "RX buffer overflow, resetting");
        rx_buffer_pos = 0;
      }
      
      // Try to process complete packets
      size_t bytes_consumed = renderer.processCommands(rx_buffer, rx_buffer_pos);
      
      if (bytes_consumed > 0) {
        // Remove processed bytes
        if (bytes_consumed < rx_buffer_pos) {
          memmove(rx_buffer, rx_buffer + bytes_consumed, rx_buffer_pos - bytes_consumed);
        }
        rx_buffer_pos -= bytes_consumed;
      }
    }
    
    // Small delay to prevent tight loop
    vTaskDelay(1);
  }
}

// ============================================================
// Render Task
// ============================================================

void renderTask(void* pvParameters) {
  ESP_LOGI(TAG, "Render task started");
  
  uint64_t frame_count = 0;
  uint64_t last_fps_time = esp_timer_get_time();
  uint64_t fps_frame_count = 0;
  
  while (1) {
    // Update animations and effects
    renderer.update();
    
    // Render to buffers
    renderer.render();
    
    // Get buffers and send to displays
    const uint8_t* hub75_buf = renderer.getHUB75Buffer();
    const uint8_t* oled_buf = renderer.getOLEDBuffer();
    
    displayRefresh(hub75_buf, oled_buf);
    
    frame_count++;
    fps_frame_count++;
    
    // Calculate FPS every second
    uint64_t now = esp_timer_get_time();
    if (now - last_fps_time >= 1000000) {  // 1 second
      float fps = (float)fps_frame_count * 1000000.0f / (float)(now - last_fps_time);
      ESP_LOGI(TAG, "FPS: %.1f | Frames: %llu | Commands: %lu",
               fps, frame_count, renderer.getStats().commands_processed);
      
      last_fps_time = now;
      fps_frame_count = 0;
    }
    
    // Target ~60fps (16.67ms per frame)
    vTaskDelay(pdMS_TO_TICKS(16));
  }
}

// ============================================================
// Status LED Task
// ============================================================

void statusTask(void* pvParameters) {
  // Configure status LED (if available)
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << GPIO_NUM_2),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&io_conf);
  
  ESP_LOGI(TAG, "Status LED task started");
  
  int led_state = 0;
  
  while (1) {
    // Blink LED based on activity
    const auto& stats = renderer.getStats();
    
    if (stats.commands_processed > 0) {
      // Fast blink when processing
      led_state = !led_state;
      gpio_set_level(GPIO_NUM_2, led_state);
      vTaskDelay(pdMS_TO_TICKS(100));
    } else {
      // Slow heartbeat
      led_state = !led_state;
      gpio_set_level(GPIO_NUM_2, led_state);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}

// ============================================================
// Main Entry Point
// ============================================================

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║           GPU Driver Demo - GPU Side                       ║");
  ESP_LOGI(TAG, "║           Command Receiver and Renderer                    ║");
  ESP_LOGI(TAG, "╚════════════════════════════════════════════════════════════╝");
  ESP_LOGI(TAG, "");
  
  // Initialize hardware
  displayInit();
  uartInit();
  
  // Initialize renderer
  GpuRenderer::Config config;
  config.enable_double_buffer = true;
  config.auto_swap_hub75 = false;  // Manual swap from CPU commands
  config.auto_swap_oled = false;
  config.enable_effects = true;
  config.max_animations = 16;
  config.max_sprites = 32;
  
  if (!renderer.init(config)) {
    ESP_LOGE(TAG, "Failed to initialize renderer!");
    return;
  }
  
  ESP_LOGI(TAG, "Renderer initialized");
  ESP_LOGI(TAG, "  HUB75: %dx%d RGB", HUB75_WIDTH, HUB75_HEIGHT);
  ESP_LOGI(TAG, "  OLED:  %dx%d Mono", OLED_WIDTH, OLED_HEIGHT);
  
  // Create tasks
  xTaskCreatePinnedToCore(
    commandTask,
    "CommandTask",
    8192,
    NULL,
    5,  // Higher priority for command processing
    NULL,
    0   // Core 0
  );
  
  xTaskCreatePinnedToCore(
    renderTask,
    "RenderTask",
    8192,
    NULL,
    4,
    NULL,
    1   // Core 1
  );
  
  xTaskCreate(
    statusTask,
    "StatusTask",
    2048,
    NULL,
    1,
    NULL
  );
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "GPU ready - waiting for commands from CPU...");
  ESP_LOGI(TAG, "");
}
