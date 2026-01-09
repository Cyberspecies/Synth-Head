/*****************************************************************
 * GPU.cpp - Dual Display Driver (HUB75 + OLED) with UART Frame Reception
 * Receives frames from CPU via UART and displays on HUB75 and OLED
 * 
 * Display Configuration:
 * - HUB75: 128x32 RGB (dual 64x32 panels) @ 60fps
 * - OLED: SH1107 128x128 monochrome @ 15fps
 * 
 * UART Configuration:
 * - Baud: 20 Mbps
 * - RX: GPIO13 (GPU RX <- CPU TX GPIO12)
 * - TX: GPIO12 (GPU TX -> CPU RX GPIO11)
 * - Frame reception: HUB75 (12,288 bytes) + OLED (2,048 bytes)
 *****************************************************************/

#include <stdio.h>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "abstraction/hal.hpp"  // For ESP32S3_I2C_HAL
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"
#include "Comms/GpuUartHandler.hpp"

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;
using namespace arcos::comms;

static const char* TAG = "GPU_DUAL_DISPLAY";

// Display objects
SimpleHUB75Display hub75_display;
DRIVER_OLED_SH1107 oled_display(0x3C, 0);  // I2C address 0x3C, bus 0

// UART handler
GpuUartHandler uart;

// Frame statistics
struct DisplayStats {
  // HUB75 stats
  uint32_t hub75_frames_received = 0;
  uint32_t hub75_frames_displayed = 0;
  uint32_t hub75_fps = 0;
  uint32_t hub75_display_fps = 0;
  uint32_t hub75_last_rx_time = 0;
  uint32_t hub75_last_display_time = 0;
  
  // OLED stats  
  uint32_t oled_frames_received = 0;
  uint32_t oled_frames_displayed = 0;
  uint32_t oled_fps = 0;
  uint32_t oled_display_fps = 0;
  uint32_t oled_last_rx_time = 0;
  uint32_t oled_last_display_time = 0;
  
  // Error stats
  uint32_t checksum_errors = 0;
  uint32_t sync_errors = 0;
  
  void updateHub75Rx() {
    uint32_t now = esp_timer_get_time() / 1000;
    if(hub75_last_rx_time > 0) {
      uint32_t dt = now - hub75_last_rx_time;
      hub75_fps = (dt > 0) ? (1000 / dt) : 0;
    }
    hub75_last_rx_time = now;
    hub75_frames_received++;
  }
  
  void updateHub75Display() {
    uint32_t now = esp_timer_get_time() / 1000;
    if(hub75_last_display_time > 0) {
      uint32_t dt = now - hub75_last_display_time;
      hub75_display_fps = (dt > 0) ? (1000 / dt) : 0;
    }
    hub75_last_display_time = now;
    hub75_frames_displayed++;
  }
  
  void updateOledRx() {
    uint32_t now = esp_timer_get_time() / 1000;
    if(oled_last_rx_time > 0) {
      uint32_t dt = now - oled_last_rx_time;
      oled_fps = (dt > 0) ? (1000 / dt) : 0;
    }
    oled_last_rx_time = now;
    oled_frames_received++;
  }
  
  void updateOledDisplay() {
    uint32_t now = esp_timer_get_time() / 1000;
    if(oled_last_display_time > 0) {
      uint32_t dt = now - oled_last_display_time;
      oled_display_fps = (dt > 0) ? (1000 / dt) : 0;
    }
    oled_last_display_time = now;
    oled_frames_displayed++;
  }
};

DisplayStats stats;
bool hub75_ok = false;
bool oled_ok = false;

void printMemoryStats(){
  ESP_LOGI(TAG, "=== Memory Stats ===");
  ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free DMA: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA));
  ESP_LOGI(TAG, "Largest DMA block: %lu bytes", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  ESP_LOGI(TAG, "Free internal: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

/** Initialize I2C bus for OLED */
bool initI2C(){
  ESP_LOGI(TAG, "Initializing I2C bus for OLED...");
  
  // Initialize I2C bus 0 with SDA=2, SCL=1 using ESP32S3_I2C_HAL
  HalResult result = ESP32S3_I2C_HAL::Initialize(
    0,      // Bus ID 0
    2,      // SDA = GPIO2
    1,      // SCL = GPIO1
    400000, // 400kHz
    1000    // 1000ms timeout
  );
  
  if(result != HalResult::Success){
    ESP_LOGE(TAG, "Failed to initialize I2C bus!");
    return false;
  }
  
  ESP_LOGI(TAG, "I2C bus initialized (SDA=2, SCL=1 @ 400kHz)");
  return true;
}

extern "C" void app_main(void){
  // 3 second delay to see boot messages
  ESP_LOGI(TAG, "Starting in 3 seconds...");
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  ESP_LOGI(TAG, "================================================");
  ESP_LOGI(TAG, "= GPU: Dual Display Driver (HUB75 + OLED)      =");
  ESP_LOGI(TAG, "= HUB75: 128x32 RGB @ 60fps                    =");
  ESP_LOGI(TAG, "= OLED:  128x128 Mono @ 15fps                  =");
  ESP_LOGI(TAG, "================================================");
  
  printMemoryStats();
  
  // ============ Initialize HUB75 Display ============
  ESP_LOGI(TAG, "--- Initializing HUB75 Display ---");
  
  HUB75Config config = HUB75Config::getDefault();
  config.colour_depth = 5;  // 5-bit color (32 levels per channel)
  config.colour_buffer_count = 5;
  config.enable_double_buffering = true;
  config.enable_gamma_correction = true;
  config.gamma_value = 2.2f;
  
  if(!hub75_display.begin(true, config)){
    ESP_LOGE(TAG, "Failed to initialize HUB75!");
  } else {
    hub75_ok = true;
    ESP_LOGI(TAG, "HUB75 initialized: %dx%d", hub75_display.getWidth(), hub75_display.getHeight());
    hub75_display.setBrightness(255);
    hub75_display.clear();
    hub75_display.show();
  }
  
  // ============ Initialize I2C and OLED ============
  ESP_LOGI(TAG, "--- Initializing OLED Display ---");
  
  if(!initI2C()){
    ESP_LOGW(TAG, "I2C init failed, OLED disabled");
  } else {
    OLEDConfig oled_config;
    oled_config.contrast = 0xFF;  // Maximum contrast
    
    if(!oled_display.initialize(oled_config)){
      ESP_LOGE(TAG, "Failed to initialize OLED!");
    } else {
      oled_ok = true;
      ESP_LOGI(TAG, "OLED initialized: 128x128 @ 0x3C");
      oled_display.clearBuffer();
      oled_display.updateDisplay();
    }
  }
  
  printMemoryStats();
  
  // ============ Initialize UART ============
  ESP_LOGI(TAG, "--- Initializing UART ---");
  
  GpuUartHandler::Config uart_config;
  uart_config.baud_rate = UART_BAUD_RATE;  // 20 Mbps from protocol
  uart_config.rx_pin = 13;  // GPU RX <- CPU TX (GPIO12)
  uart_config.tx_pin = 12;  // GPU TX -> CPU RX (GPIO11)
  uart_config.rx_buffer_size = 16384;
  
  if(!uart.init(uart_config)){
    ESP_LOGE(TAG, "Failed to initialize UART!");
    while(1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  ESP_LOGI(TAG, "UART initialized: %lu baud (%.1f Mbps)", 
           (unsigned long)uart_config.baud_rate,
           uart_config.baud_rate / 1000000.0f);
  
  ESP_LOGI(TAG, "Waiting for frames from CPU...");
  ESP_LOGI(TAG, "  HUB75: %s", hub75_ok ? "READY" : "DISABLED");
  ESP_LOGI(TAG, "  OLED:  %s", oled_ok ? "READY" : "DISABLED");
  
  uint32_t last_stats_time = 0;
  
  // ============ Main Loop ============
  while(1){
    // Process incoming UART data
    uart.process(16384);
    
    // ============ Handle HUB75 Frame ============
    if(hub75_ok && uart.hasFrame()){
      const UartFrameBuffer* frame = uart.getFrame();
      
      if(frame && frame->complete && 
         frame->width == 128 && frame->height == 32){
        
        stats.updateHub75Rx();
        
        // Copy frame data to display
        const uint8_t* pixel_data = frame->data;
        for(int y = 0; y < 32; y++){
          for(int x = 0; x < 128; x++){
            int idx = (y * 128 + x) * 3;
            uint8_t r = pixel_data[idx + 0];
            uint8_t g = pixel_data[idx + 1];
            uint8_t b = pixel_data[idx + 2];
            hub75_display.setPixel(x, y, RGB(r, g, b));
          }
        }
        
        // Swap buffers
        hub75_display.show();
        stats.updateHub75Display();
        
        // Mark frame as consumed
        uart.consumeFrame();
      }
    }
    
    // ============ Handle OLED Frame ============
    if(oled_ok && uart.hasOledFrame()){
      const UartFrameBuffer* frame = uart.getOledFrame();
      
      if(frame && frame->complete){
        stats.updateOledRx();
        
        // Copy packed monochrome data directly to OLED buffer
        // The CPU sends 1-bit packed data (MSB first)
        uint8_t* oled_buffer = oled_display.getBuffer();
        if(oled_buffer){
          memcpy(oled_buffer, frame->data, 2048);
          
          // Update OLED display
          oled_display.updateDisplay();
          stats.updateOledDisplay();
        }
        
        // Mark frame as consumed
        uart.consumeOledFrame();
      }
    }
    
    // ============ Print Statistics ============
    uint32_t now = esp_timer_get_time() / 1000;
    if(now - last_stats_time >= 1000){
      const auto& uart_stats = uart.getStats();
      
      ESP_LOGI(TAG, "HUB75: RX %lu @ %lu fps, Display %lu @ %lu fps | OLED: RX %lu @ %lu fps, Display %lu @ %lu fps | Errors: %lu",
               (unsigned long)stats.hub75_frames_received,
               (unsigned long)stats.hub75_fps,
               (unsigned long)stats.hub75_frames_displayed,
               (unsigned long)stats.hub75_display_fps,
               (unsigned long)stats.oled_frames_received,
               (unsigned long)stats.oled_fps,
               (unsigned long)stats.oled_frames_displayed,
               (unsigned long)stats.oled_display_fps,
               (unsigned long)(uart_stats.checksum_errors + uart_stats.sync_errors));
      
      // Send status back to CPU periodically
      uart.sendStatus();
      
      last_stats_time = now;
    }
    
    // Small yield to prevent watchdog
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
