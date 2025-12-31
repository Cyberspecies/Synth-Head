/*****************************************************************
 * GPU.cpp - HUB75 Display with UART Frame Reception
 * Receives 128x32 RGB frames from CPU via UART and displays on HUB75
 * 
 * Optimized Memory Usage:
 * - Buffer size: 26,352 samples (105KB DMA for double buffering)
 * - Brightness levels: 16 (0-255 mapped to 0-15)
 * - Colour depth: 5 bits (32 levels per channel)
 *****************************************************************/

#include <stdio.h>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include "Comms/GpuUartHandler.hpp"

using namespace arcos::abstraction::drivers;
using namespace arcos::comms;

static const char* TAG = "GPU_UART_DISPLAY";

SimpleHUB75Display display;
GpuUartHandler uart;

// Frame statistics
struct FrameStats {
  uint32_t frames_received = 0;
  uint32_t frames_displayed = 0;
  uint32_t last_frame_time = 0;
  uint32_t last_display_time = 0;
  uint32_t fps = 0;
  uint32_t display_fps = 0;
  uint32_t min_frame_time = 0xFFFFFFFF;
  uint32_t max_frame_time = 0;
  uint32_t min_display_time = 0xFFFFFFFF;
  uint32_t max_display_time = 0;
  uint32_t frame_requests_sent = 0;
  
  void updateReceiveFps() {
    uint32_t now = esp_timer_get_time() / 1000;
    if(last_frame_time > 0) {
      uint32_t frame_time = now - last_frame_time;
      if(frame_time < min_frame_time) min_frame_time = frame_time;
      if(frame_time > max_frame_time) max_frame_time = frame_time;
      fps = (frame_time > 0) ? (1000 / frame_time) : 0;
    }
    last_frame_time = now;
    frames_received++;
  }
  
  void updateDisplayFps() {
    uint32_t now = esp_timer_get_time() / 1000;
    if(last_display_time > 0) {
      uint32_t display_time = now - last_display_time;
      if(display_time < min_display_time) min_display_time = display_time;
      if(display_time > max_display_time) max_display_time = display_time;
      display_fps = (display_time > 0) ? (1000 / display_time) : 0;
    }
    last_display_time = now;
    frames_displayed++;
  }
};

FrameStats stats;

void printMemoryStats(){
  ESP_LOGI(TAG, "=== Memory Stats ===");
  ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free DMA: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA));
  ESP_LOGI(TAG, "Largest DMA block: %lu bytes", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  ESP_LOGI(TAG, "Free internal: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

extern "C" void app_main(void){
  // 3 second delay to see boot messages
  ESP_LOGI(TAG, "Starting in 3 seconds...");
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  ESP_LOGI(TAG, "==============================================");
  ESP_LOGI(TAG, "= GPU: UART Frame Reception -> HUB75 Display =");
  ESP_LOGI(TAG, "==============================================");
  
  printMemoryStats();
  
  // Configure HUB75 with optimized settings
  HUB75Config config = HUB75Config::getDefault();
  config.colour_depth = 5;  // 5-bit color (32 levels per channel)
  config.colour_buffer_count = 5;
  config.enable_double_buffering = true;
  config.enable_gamma_correction = true;
  config.gamma_value = 2.2f;
  
  ESP_LOGI(TAG, "HUB75 Config: colour_depth=%d, gamma=%.1f", config.colour_depth, config.gamma_value);
  
  // Initialize display with dual OE mode
  if(!display.begin(true, config)){
    ESP_LOGE(TAG, "Failed to initialize HUB75!");
    while(1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  ESP_LOGI(TAG, "HUB75 initialized: %dx%d", display.getWidth(), display.getHeight());
  display.setBrightness(255);  // Maximum brightness
  ESP_LOGI(TAG, "Brightness set to maximum (255)");
  
  printMemoryStats();
  
  // Initialize UART
  GpuUartHandler::Config uart_config;
  uart_config.baud_rate = 10000000;  // 10 Mbps
  
  if(!uart.init(uart_config)){
    ESP_LOGE(TAG, "Failed to initialize UART!");
    while(1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  ESP_LOGI(TAG, "UART initialized at %lu baud", uart_config.baud_rate);
  ESP_LOGI(TAG, "Waiting for frames from CPU...");
  ESP_LOGI(TAG, "Frame request mode: GPU controls frame rate");
  
  // Clear display to black
  display.clear();
  display.show();
  
  // Send initial frame request to start the loop
  ESP_LOGI(TAG, "Sending initial frame request...");
  uart.sendMessage(MsgType::FRAME_REQUEST, nullptr, 0);
  stats.frame_requests_sent++;
  
  uint32_t last_stats_time = 0;
  bool waiting_for_frame = true;
  
  while(1){
    // Process incoming UART data
    uart.process(8192);
    
    // Check if we have a complete frame and we're waiting for one
    if(waiting_for_frame && uart.hasFrame()){
      const UartFrameBuffer* frame = uart.getFrame();
      
      if(frame && frame->complete && 
         frame->width == 128 && frame->height == 32){
        
        // Track frame receive timing
        stats.updateReceiveFps();
        
        // Copy frame data to display using setPixel (writes to back buffer)
        const uint8_t* pixel_data = frame->data;
        for(int y = 0; y < 32; y++){
          for(int x = 0; x < 128; x++){
            int idx = (y * 128 + x) * 3;
            uint8_t r = pixel_data[idx + 0];
            uint8_t g = pixel_data[idx + 1];
            uint8_t b = pixel_data[idx + 2];
            display.setPixel(x, y, RGB(r, g, b));
          }
        }
        
        // Swap buffers and display (presents to front buffer)
        display.show();
        
        // Track display swap timing
        stats.updateDisplayFps();
        
        // Mark frame as consumed
        uart.consumeFrame();
        
        // Request next frame NOW - right after buffer swap
        uart.sendMessage(MsgType::FRAME_REQUEST, nullptr, 0);
        stats.frame_requests_sent++;
        
        // We've sent the request, now wait for next frame
        waiting_for_frame = true;
      }
    }
    
    // Print statistics every second
    uint32_t now = esp_timer_get_time() / 1000;
    if(now - last_stats_time >= 1000){
      uint32_t display_interval = (stats.last_display_time > 0) ? 
                                   (now - stats.last_display_time) : 0;
      
      ESP_LOGI(TAG, "RX: %lu frames @ %lu FPS (recv: %lu-%lu ms) | Display: %lu @ %lu FPS (swap: %lu-%lu ms) | Requests: %lu",
               (unsigned long)stats.frames_received,
               (unsigned long)stats.fps,
               (unsigned long)stats.min_frame_time,
               (unsigned long)stats.max_frame_time,
               (unsigned long)stats.frames_displayed,
               (unsigned long)stats.display_fps,
               (unsigned long)stats.min_display_time,
               (unsigned long)stats.max_display_time,
               (unsigned long)stats.frame_requests_sent);
      last_stats_time = now;
    }
    
    // Small yield to prevent watchdog
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
