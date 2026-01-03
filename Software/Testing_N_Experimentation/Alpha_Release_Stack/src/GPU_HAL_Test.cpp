/*****************************************************************
 * File:      GPU_HAL_Test.cpp
 * Category:  Main Application (HAL Test)
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side HAL test application (ESP-IDF).
 *    Tests HUB75 Display, OLED Display, and UART communication.
 * 
 * Hardware (COM 16):
 *    - ESP32-S3 (GPU)
 *    - UART to CPU: TX=GPIO12, RX=GPIO13
 *    - HUB75 Display: Dual 64x32 panels (128x32 total)
 *    - OLED Display: SH1107 128x128 @ I2C 0x3C
 *    - I2C: SDA=GPIO2, SCL=GPIO1
 * 
 * Usage:
 *    Build with GPU_HAL_Test environment in platformio.ini
 *****************************************************************/

#include <cstdio>
#include <cstring>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

// ARCOS Drivers
#include "abstraction/hal.hpp"  // For ESP32S3_I2C_HAL
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

// ============================================================
// Configuration
// ============================================================

static const char* TAG = "GPU_HAL_TEST";

// UART Configuration (CPU-GPU)
constexpr uint8_t UART_TX_PIN = 12;
constexpr uint8_t UART_RX_PIN = 13;
constexpr uint32_t UART_BAUD = 10000000;  // 10 Mbps
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr size_t UART_BUF_SIZE = 16384;

// I2C Configuration (OLED) - SDA=GPIO2, SCL=GPIO1
constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_2;
constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_1;
constexpr uint32_t I2C_FREQ = 400000;
constexpr uint8_t OLED_ADDRESS = 0x3C;

// ============================================================
// Global State
// ============================================================

SimpleHUB75Display hub75_display;
DRIVER_OLED_SH1107 oled_display;

uint8_t rx_buffer[256];
uint8_t tx_buffer[256];
uint32_t rx_count = 0;
uint32_t tx_count = 0;
uint32_t last_stats_time = 0;
uint32_t last_heartbeat = 0;
uint32_t frame_count = 0;
uint8_t animation_phase = 0;
uint32_t last_animation = 0;

bool hub75_ok = false;
bool oled_ok = false;
bool uart_ok = false;

// ============================================================
// Utility Functions
// ============================================================

/** Print memory stats */
void printMemoryStats(){
  ESP_LOGI(TAG, "=== Memory Stats ===");
  ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free DMA: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA));
  ESP_LOGI(TAG, "Largest DMA block: %lu bytes", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  ESP_LOGI(TAG, "Free internal: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

/** HSV to RGB conversion */
RGB hsvToRgb(uint8_t h, uint8_t s, uint8_t v){
  if(s == 0){
    return RGB(v, v, v);
  }
  
  uint8_t region = h / 43;
  uint8_t remainder = (h - region * 43) * 6;
  
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  
  switch(region){
    case 0:  return RGB(v, t, p);
    case 1:  return RGB(q, v, p);
    case 2:  return RGB(p, v, t);
    case 3:  return RGB(p, q, v);
    case 4:  return RGB(t, p, v);
    default: return RGB(v, p, q);
  }
}

// ============================================================
// HAL Implementation (ESP-IDF)
// ============================================================

/** Initialize UART */
bool initUart(){
  ESP_LOGI(TAG, "=== Initializing UART ===");
  
  uart_config_t config = {
    .baud_rate = (int)UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_APB,
  };
  
  esp_err_t err = uart_param_config(UART_PORT, &config);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
    return false;
  }
  
  err = uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
    return false;
  }
  
  err = uart_driver_install(UART_PORT, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
    return false;
  }
  
  ESP_LOGI(TAG, "UART initialized: TX=%d, RX=%d, baud=%lu", UART_TX_PIN, UART_RX_PIN, UART_BAUD);
  return true;
}

/** Read from UART */
int uartRead(uint8_t* data, size_t max_len, uint32_t timeout_ms){
  return uart_read_bytes(UART_PORT, data, max_len, pdMS_TO_TICKS(timeout_ms));
}

/** Write to UART */
int uartWrite(const uint8_t* data, size_t len){
  return uart_write_bytes(UART_PORT, data, len);
}

/** Get available bytes in UART RX buffer */
size_t uartAvailable(){
  size_t len = 0;
  uart_get_buffered_data_len(UART_PORT, &len);
  return len;
}

// ============================================================
// HUB75 Display Tests
// ============================================================

/** Initialize HUB75 display */
bool initHub75(){
  ESP_LOGI(TAG, "=== Initializing HUB75 Display ===");
  
  // Configure HUB75 with dual panel mode
  HUB75Config config = HUB75Config::getDefault();
  config.colour_depth = 5;
  config.colour_buffer_count = 5;
  config.enable_double_buffering = true;
  config.enable_gamma_correction = true;
  config.gamma_value = 2.2f;
  
  ESP_LOGI(TAG, "HUB75 Config: depth=%d, gamma=%.1f, double_buffer=%d",
           config.colour_depth, config.gamma_value, config.enable_double_buffering);
  
  // Initialize with dual OE mode (2 panels)
  if(!hub75_display.begin(true, config)){
    ESP_LOGE(TAG, "HUB75 initialization failed!");
    return false;
  }
  
  ESP_LOGI(TAG, "HUB75 initialized: %dx%d", hub75_display.getWidth(), hub75_display.getHeight());
  hub75_display.setBrightness(200);
  
  return true;
}

/** Test HUB75 with color patterns */
void testHub75Colors(){
  ESP_LOGI(TAG, "HUB75 Test: Color bars");
  
  int width = hub75_display.getWidth();
  int height = hub75_display.getHeight();
  
  // Fill with red
  ESP_LOGI(TAG, "  RED");
  for(int y = 0; y < height; y++){
    for(int x = 0; x < width; x++){
      hub75_display.setPixel(x, y, RGB(255, 0, 0));
    }
  }
  hub75_display.show();
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Fill with green
  ESP_LOGI(TAG, "  GREEN");
  for(int y = 0; y < height; y++){
    for(int x = 0; x < width; x++){
      hub75_display.setPixel(x, y, RGB(0, 255, 0));
    }
  }
  hub75_display.show();
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Fill with blue
  ESP_LOGI(TAG, "  BLUE");
  for(int y = 0; y < height; y++){
    for(int x = 0; x < width; x++){
      hub75_display.setPixel(x, y, RGB(0, 0, 255));
    }
  }
  hub75_display.show();
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Fill with white
  ESP_LOGI(TAG, "  WHITE");
  for(int y = 0; y < height; y++){
    for(int x = 0; x < width; x++){
      hub75_display.setPixel(x, y, RGB(255, 255, 255));
    }
  }
  hub75_display.show();
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Vertical color bars
  ESP_LOGI(TAG, "  Color bars");
  for(int x = 0; x < width; x++){
    RGB color;
    int bar = (x * 8) / width;
    switch(bar){
      case 0: color = RGB(255, 255, 255); break; // White
      case 1: color = RGB(255, 255, 0);   break; // Yellow
      case 2: color = RGB(0, 255, 255);   break; // Cyan
      case 3: color = RGB(0, 255, 0);     break; // Green
      case 4: color = RGB(255, 0, 255);   break; // Magenta
      case 5: color = RGB(255, 0, 0);     break; // Red
      case 6: color = RGB(0, 0, 255);     break; // Blue
      default: color = RGB(0, 0, 0);      break; // Black
    }
    for(int y = 0; y < height; y++){
      hub75_display.setPixel(x, y, color);
    }
  }
  hub75_display.show();
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // Gradient test
  ESP_LOGI(TAG, "  Rainbow gradient");
  for(int x = 0; x < width; x++){
    uint8_t hue = (x * 256) / width;
    for(int y = 0; y < height; y++){
      uint8_t brightness = 128 + (y * 127) / height;
      RGB color = hsvToRgb(hue, 255, brightness);
      hub75_display.setPixel(x, y, color);
    }
  }
  hub75_display.show();
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  ESP_LOGI(TAG, "HUB75 color test complete");
}

/** Animate HUB75 rainbow */
void animateHub75(uint8_t phase){
  int width = hub75_display.getWidth();
  int height = hub75_display.getHeight();
  
  for(int x = 0; x < width; x++){
    for(int y = 0; y < height; y++){
      // Diagonal rainbow with moving animation
      uint8_t hue = (phase + x * 2 + y * 4) % 256;
      uint8_t val = 180 + (int)(sinf((phase + x) * 0.1f) * 75);
      RGB color = hsvToRgb(hue, 255, val);
      hub75_display.setPixel(x, y, color);
    }
  }
  hub75_display.show();
  frame_count++;
}

// ============================================================
// OLED Display Tests
// ============================================================

/** Initialize I2C bus for OLED */
bool initI2C(){
  ESP_LOGI(TAG, "Initializing I2C bus...");
  
  // Initialize I2C bus 0 with the OLED pins
  HalResult result = ESP32S3_I2C_HAL::Initialize(
    0,                             // Bus ID 0
    static_cast<uint8_t>(I2C_SDA_PIN),  // SDA = GPIO2
    static_cast<uint8_t>(I2C_SCL_PIN),  // SCL = GPIO1
    I2C_FREQ,                      // 400kHz
    1000                           // 1000ms timeout
  );
  
  if(result != HalResult::Success){
    ESP_LOGE(TAG, "I2C initialization failed!");
    return false;
  }
  
  ESP_LOGI(TAG, "I2C bus initialized: SDA=%d, SCL=%d @ %lu Hz", 
           I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);
  return true;
}

/** Scan I2C bus for all connected devices */
void scanI2CBus(){
  ESP_LOGI(TAG, "=== I2C Bus Scan ===");
  ESP_LOGI(TAG, "Scanning addresses 0x01-0x7F...");
  
  int devices_found = 0;
  
  for(uint8_t addr = 0x01; addr < 0x78; addr++){
    // Try to probe the device
    HalResult result = ESP32S3_I2C_HAL::ProbeDevice(0, addr);
    
    if(result == HalResult::Success){
      ESP_LOGI(TAG, "  Found device at 0x%02X", addr);
      devices_found++;
      
      // Identify common devices
      if(addr == 0x3C || addr == 0x3D){
        ESP_LOGI(TAG, "    -> Likely OLED display (SH1107/SSD1306)");
      } else if(addr == 0x68 || addr == 0x69){
        ESP_LOGI(TAG, "    -> Likely IMU (ICM20948/MPU6050)");
      } else if(addr == 0x76 || addr == 0x77){
        ESP_LOGI(TAG, "    -> Likely environmental sensor (BME280/BMP280)");
      } else if(addr >= 0x20 && addr <= 0x27){
        ESP_LOGI(TAG, "    -> Likely I/O expander (PCF8574)");
      } else if(addr >= 0x50 && addr <= 0x57){
        ESP_LOGI(TAG, "    -> Likely EEPROM (24LC)");
      }
    }
  }
  
  if(devices_found == 0){
    ESP_LOGW(TAG, "No I2C devices found!");
    ESP_LOGW(TAG, "Check wiring: SDA=GPIO%d, SCL=GPIO%d", I2C_SDA_PIN, I2C_SCL_PIN);
  } else {
    ESP_LOGI(TAG, "Total devices found: %d", devices_found);
  }
  ESP_LOGI(TAG, "=== I2C Scan Complete ===");
}

/** Initialize OLED display */
bool initOled(){
  ESP_LOGI(TAG, "=== Initializing OLED Display ===");
  
  // First, initialize I2C bus
  if(!initI2C()){
    ESP_LOGE(TAG, "Cannot initialize OLED: I2C bus init failed");
    return false;
  }
  
  // Scan I2C bus to find connected devices
  scanI2CBus();
  
  // Configure OLED
  OLEDConfig config;
  config.contrast = 0xCF;  // High contrast
  config.flip_horizontal = true;
  config.flip_vertical = true;
  
  // Initialize OLED driver
  if(!oled_display.initialize(config)){
    ESP_LOGE(TAG, "OLED initialization failed!");
    return false;
  }
  
  ESP_LOGI(TAG, "OLED initialized: 128x128 @ 0x%02X", OLED_ADDRESS);
  
  return true;
}

/** Test OLED display patterns */
void testOled(){
  ESP_LOGI(TAG, "OLED Test: Patterns");
  
  // Clear display
  oled_display.clearBuffer();
  oled_display.updateDisplay();
  vTaskDelay(pdMS_TO_TICKS(200));
  
  // Draw border
  ESP_LOGI(TAG, "  Border");
  oled_display.drawRect(0, 0, 128, 128, false, true);  // Outer border
  oled_display.drawRect(4, 4, 120, 120, false, true);  // Inner border
  oled_display.updateDisplay();
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Draw text
  ESP_LOGI(TAG, "  Text");
  oled_display.clearBuffer();
  oled_display.drawString(10, 10, "ARCOS HAL TEST", true);
  oled_display.drawString(10, 30, "GPU Display", true);
  oled_display.drawString(10, 50, "HUB75 + OLED", true);
  oled_display.drawLine(10, 70, 118, 70, true);
  
  char buf[32];
  uint32_t uptime = esp_timer_get_time() / 1000000;
  snprintf(buf, sizeof(buf), "Uptime: %lus", uptime);
  oled_display.drawString(10, 80, buf, true);
  
  snprintf(buf, sizeof(buf), "Heap: %luKB", (unsigned long)(esp_get_free_heap_size() / 1024));
  oled_display.drawString(10, 100, buf, true);
  
  oled_display.updateDisplay();
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // Draw geometric shapes
  ESP_LOGI(TAG, "  Shapes");
  oled_display.clearBuffer();
  
  // Filled rectangle
  oled_display.drawRect(10, 10, 40, 30, true, true);
  
  // Empty rectangle  
  oled_display.drawRect(60, 10, 40, 30, false, true);
  
  // Lines
  for(int i = 0; i < 10; i++){
    oled_display.drawLine(10 + i * 10, 50, 10 + i * 10, 80, true);
  }
  
  // Circle pattern using lines
  int cx = 64, cy = 100;
  for(int angle = 0; angle < 360; angle += 15){
    float rad = angle * 3.14159f / 180.0f;
    int x1 = cx + (int)(cosf(rad) * 10);
    int y1 = cy + (int)(sinf(rad) * 10);
    int x2 = cx + (int)(cosf(rad) * 20);
    int y2 = cy + (int)(sinf(rad) * 20);
    oled_display.drawLine(x1, y1, x2, y2, true);
  }
  
  oled_display.updateDisplay();
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  ESP_LOGI(TAG, "OLED test complete");
}

/** Update OLED with status info */
void updateOledStatus(){
  oled_display.clearBuffer();
  
  // Title
  oled_display.drawString(10, 5, "GPU HAL Status", true);
  oled_display.drawLine(0, 18, 128, 18, true);
  
  // HUB75 status
  char buf[32];
  oled_display.drawString(5, 25, "HUB75:", true);
  snprintf(buf, sizeof(buf), "%dx%d %s", 
           hub75_ok ? hub75_display.getWidth() : 0,
           hub75_ok ? hub75_display.getHeight() : 0,
           hub75_ok ? "OK" : "FAIL");
  oled_display.drawString(50, 25, buf, true);
  
  // Frame count
  oled_display.drawString(5, 40, "Frames:", true);
  snprintf(buf, sizeof(buf), "%lu", frame_count);
  oled_display.drawString(55, 40, buf, true);
  
  // UART status
  oled_display.drawString(5, 55, "UART:", true);
  snprintf(buf, sizeof(buf), "RX:%lu TX:%lu", rx_count, tx_count);
  oled_display.drawString(45, 55, buf, true);
  
  // Memory
  oled_display.drawString(5, 70, "Heap:", true);
  snprintf(buf, sizeof(buf), "%luKB", (unsigned long)(esp_get_free_heap_size() / 1024));
  oled_display.drawString(45, 70, buf, true);
  
  // Uptime
  oled_display.drawString(5, 85, "Up:", true);
  uint32_t uptime = esp_timer_get_time() / 1000000;
  snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", uptime / 3600, (uptime / 60) % 60, uptime % 60);
  oled_display.drawString(30, 85, buf, true);
  
  // Animation phase bar
  oled_display.drawRect(5, 100, 118, 10, false, true);
  int bar_width = (animation_phase * 116) / 255;
  if(bar_width > 0){
    oled_display.drawRect(6, 101, bar_width, 8, true, true);
  }
  
  oled_display.updateDisplay();
}

// ============================================================
// UART Communication
// ============================================================

/** Process incoming messages */
void processUartMessages(){
  size_t available = uartAvailable();
  if(available > 0){
    int read = uartRead(rx_buffer, available < sizeof(rx_buffer) ? available : sizeof(rx_buffer), 10);
    if(read > 0){
      rx_buffer[read] = '\0';
      rx_count++;
      
      ESP_LOGI(TAG, "RX[%lu]: %s", rx_count, rx_buffer);
      
      // Check for specific messages
      if(strncmp((char*)rx_buffer, "CPU_PING", 8) == 0){
        // Respond to ping
        const char* response = "GPU_PONG";
        uartWrite((const uint8_t*)response, strlen(response));
        tx_count++;
        ESP_LOGI(TAG, "Responded with: %s", response);
      }
      else if(strncmp((char*)rx_buffer, "CPU:", 4) == 0){
        // CPU heartbeat - respond with GPU heartbeat
        char response[64];
        uint32_t now = esp_timer_get_time() / 1000;
        snprintf(response, sizeof(response), "GPU:%lu HUB75:%s OLED:%s Frames:%lu",
                 now, hub75_ok ? "OK" : "FAIL", oled_ok ? "OK" : "FAIL", frame_count);
        uartWrite((const uint8_t*)response, strlen(response));
        tx_count++;
      }
    }
  }
}

/** Send GPU heartbeat */
void sendHeartbeat(){
  uint32_t now = esp_timer_get_time() / 1000;
  if(now - last_heartbeat >= 1000){
    last_heartbeat = now;
    
    char msg[64];
    snprintf(msg, sizeof(msg), "GPU_HB:%lu F:%lu H:%d O:%d", 
             now, frame_count, hub75_ok ? 1 : 0, oled_ok ? 1 : 0);
    uartWrite((const uint8_t*)msg, strlen(msg));
    tx_count++;
  }
}

/** Print statistics */
void printStats(){
  uint32_t now = esp_timer_get_time() / 1000;
  if(now - last_stats_time >= 5000){
    last_stats_time = now;
    
    ESP_LOGI(TAG, "=== Status ===");
    ESP_LOGI(TAG, "HUB75: %s (%dx%d)", hub75_ok ? "OK" : "FAIL",
             hub75_ok ? hub75_display.getWidth() : 0,
             hub75_ok ? hub75_display.getHeight() : 0);
    ESP_LOGI(TAG, "OLED:  %s (128x128)", oled_ok ? "OK" : "FAIL");
    ESP_LOGI(TAG, "UART:  RX=%lu TX=%lu", rx_count, tx_count);
    ESP_LOGI(TAG, "Frames: %lu", frame_count);
    printMemoryStats();
  }
}

// ============================================================
// Main Application
// ============================================================

extern "C" void app_main(void){
  // Startup delay
  ESP_LOGI(TAG, "Starting GPU HAL Test in 3 seconds...");
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "  ARCOS HAL Test - GPU (COM 16)");
  ESP_LOGI(TAG, "  Testing HUB75 + OLED + UART");
  ESP_LOGI(TAG, "========================================");
  
  printMemoryStats();
  
  // Initialize all subsystems
  ESP_LOGI(TAG, "\n--- Display Tests ---");
  
  hub75_ok = initHub75();
  if(hub75_ok){
    testHub75Colors();
  }
  
  oled_ok = initOled();
  if(oled_ok){
    testOled();
  }
  
  ESP_LOGI(TAG, "\n--- Communication Tests ---");
  uart_ok = initUart();
  
  // Print test summary
  ESP_LOGI(TAG, "\n============ TEST SUMMARY ============");
  ESP_LOGI(TAG, "HUB75 Display: %s", hub75_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "OLED Display:  %s", oled_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "UART:          %s", uart_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "======================================\n");
  
  // Send startup message
  if(uart_ok){
    const char* startup_msg = "GPU_READY HUB75:OK OLED:OK";
    uartWrite((const uint8_t*)startup_msg, strlen(startup_msg));
    ESP_LOGI(TAG, "Sent: %s", startup_msg);
  }
  
  ESP_LOGI(TAG, "GPU HAL Test running...");
  ESP_LOGI(TAG, "HUB75: Rainbow animation");
  ESP_LOGI(TAG, "OLED: Status display");
  
  // Main loop
  uint32_t last_oled_update = 0;
  
  while(1){
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Process UART messages
    if(uart_ok){
      processUartMessages();
      sendHeartbeat();
    }
    
    // Animate HUB75 at ~30 FPS
    if(hub75_ok && (now - last_animation >= 33)){
      last_animation = now;
      animation_phase += 2;
      animateHub75(animation_phase);
    }
    
    // Update OLED status every 500ms
    if(oled_ok && (now - last_oled_update >= 500)){
      last_oled_update = now;
      updateOledStatus();
    }
    
    // Print stats periodically
    printStats();
    
    // Small yield to prevent watchdog
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
