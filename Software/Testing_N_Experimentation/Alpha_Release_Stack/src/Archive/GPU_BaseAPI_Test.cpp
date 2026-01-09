/*****************************************************************
 * File:      GPU_BaseAPI_Test.cpp
 * Category:  Main Application (Base API Test)
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side Base System API test application.
 *    Based on working GPU_HAL_Test.cpp with protocol layer added.
 * 
 * Hardware (COM 3):
 *    - ESP32-S3 (GPU)
 *    - UART from CPU: TX=GPIO12, RX=GPIO13 @ 10Mbps
 *    - HUB75 Display: Dual 64x32 panels (128x32 total)
 *    - OLED Display: SH1107 128x128 @ I2C 0x3C
 *    - I2C: SDA=GPIO2, SCL=GPIO1
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
#include "abstraction/hal.hpp"
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

// Base System API - only protocol components
#include "BaseAPI/BaseTypes.hpp"
#include "BaseAPI/Telemetry.hpp"
#include "BaseAPI/CommProtocol.hpp"

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;
using namespace arcos::base;

// ============================================================
// Configuration
// ============================================================

static const char* TAG = "GPU_BASE_TEST";

constexpr uint8_t UART_TX_PIN = 12;
constexpr uint8_t UART_RX_PIN = 13;
constexpr uint32_t UART_BAUD = 2000000;  // 2 Mbps - matches old working code
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr size_t UART_BUF_SIZE = 16384;

constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_2;
constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_1;
constexpr uint32_t I2C_FREQ = 400000;
constexpr uint8_t OLED_ADDRESS = 0x3C;

// ============================================================
// Global Hardware
// ============================================================

SimpleHUB75Display hub75_display;
DRIVER_OLED_SH1107 oled_display;

// ============================================================
// Protocol Components
// ============================================================

uint8_t rx_packet_buffer[512];
uint8_t tx_packet_buffer[256];
PacketParser packet_parser(rx_packet_buffer, sizeof(rx_packet_buffer));
PacketBuilder packet_builder(tx_packet_buffer, sizeof(tx_packet_buffer));

TelemetryData received_telemetry;
bool telemetry_valid = false;
uint32_t telemetry_count = 0;
uint32_t last_telemetry_time = 0;

// ============================================================
// State
// ============================================================

uint32_t frame_count = 0;
uint32_t rx_count = 0;
uint32_t tx_count = 0;
uint8_t animation_phase = 0;

bool hub75_ok = false;
bool oled_ok = false;
bool uart_ok = false;

// ============================================================
// Utility
// ============================================================

uint32_t getTimeMs(){
  return esp_timer_get_time() / 1000;
}

void printMemoryStats(){
  ESP_LOGI(TAG, "Free heap: %lu, Free DMA: %lu", 
           (unsigned long)esp_get_free_heap_size(),
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA));
}

RGB hsvToRgb(uint8_t h, uint8_t s, uint8_t v){
  if(s == 0) return RGB(v, v, v);
  uint8_t region = h / 43;
  uint8_t remainder = (h - (region * 43)) * 6;
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
// Initialization
// ============================================================

bool initUART(){
  ESP_LOGI(TAG, "Initializing UART...");
  
  uart_config_t config = {
    .baud_rate = (int)UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_APB,
  };
  
  if(uart_param_config(UART_PORT, &config) != ESP_OK) return false;
  if(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, 
                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) return false;
  if(uart_driver_install(UART_PORT, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0) != ESP_OK) return false;
  
  ESP_LOGI(TAG, "UART OK: TX=%d RX=%d @ %lu baud", UART_TX_PIN, UART_RX_PIN, UART_BAUD);
  return true;
}

bool initI2C(){
  ESP_LOGI(TAG, "Initializing I2C...");
  
  HalResult result = ESP32S3_I2C_HAL::Initialize(
    0, (uint8_t)I2C_SDA_PIN, (uint8_t)I2C_SCL_PIN, I2C_FREQ, 1000
  );
  
  if(result != HalResult::Success){
    ESP_LOGE(TAG, "I2C init failed");
    return false;
  }
  
  ESP_LOGI(TAG, "I2C OK: SDA=%d SCL=%d", I2C_SDA_PIN, I2C_SCL_PIN);
  return true;
}

bool initHUB75(){
  ESP_LOGI(TAG, "Initializing HUB75...");
  printMemoryStats();
  
  if(!hub75_display.begin(true)){
    ESP_LOGE(TAG, "HUB75 init failed");
    return false;
  }
  
  hub75_display.clear();
  hub75_display.show();
  
  ESP_LOGI(TAG, "HUB75 OK: %dx%d", hub75_display.getWidth(), hub75_display.getHeight());
  printMemoryStats();
  return true;
}

bool initOLED(){
  ESP_LOGI(TAG, "Initializing OLED...");
  
  HalResult result = ESP32S3_I2C_HAL::ProbeDevice(0, OLED_ADDRESS);
  if(result != HalResult::Success){
    ESP_LOGE(TAG, "OLED not found at 0x%02X", OLED_ADDRESS);
    return false;
  }
  
  OLEDConfig config;
  config.contrast = 0xCF;
  config.flip_horizontal = true;
  config.flip_vertical = true;
  
  if(!oled_display.initialize(config)){
    ESP_LOGE(TAG, "OLED init failed");
    return false;
  }
  
  oled_display.clearBuffer();
  oled_display.updateDisplay();
  
  ESP_LOGI(TAG, "OLED OK: 128x128");
  return true;
}

// ============================================================
// Display Functions
// ============================================================

void drawTelemetryOnHUB75(){
  hub75_display.clear();
  
  int w = hub75_display.getWidth();
  int h = hub75_display.getHeight();
  int cx = w / 2;
  int cy = h / 2;
  
  float roll = 0.0f, pitch = 0.0f;
  if(telemetry_valid){
    roll = received_telemetry.motion.euler.x;
    pitch = received_telemetry.motion.euler.y;
  }
  
  // Horizon line (moves with pitch, rotates with roll)
  float sin_roll = sinf(roll);
  int pitch_offset = (int)(pitch * 20.0f);
  
  for(int i = -50; i <= 50; i++){
    int hx = cx + i;
    int hy = cy + pitch_offset + (int)(i * sin_roll / 3.0f);
    if(hx >= 0 && hx < w && hy >= 0 && hy < h){
      hub75_display.setPixel(hx, hy, RGB(0, 255, 0));
    }
  }
  
  // Aircraft symbol
  hub75_display.setPixel(cx, cy, RGB(255, 255, 0));
  hub75_display.setPixel(cx - 5, cy, RGB(255, 255, 0));
  hub75_display.setPixel(cx + 5, cy, RGB(255, 255, 0));
  hub75_display.setPixel(cx, cy - 3, RGB(255, 255, 0));
  
  // Status indicator
  if(telemetry_valid){
    hub75_display.setPixel(2, h - 2, RGB(0, 255, 0));
    hub75_display.setPixel(3, h - 2, RGB(0, 255, 0));
  }else{
    if((frame_count / 30) % 2){
      hub75_display.setPixel(2, h - 2, RGB(255, 0, 0));
    }
  }
  
  // Packet count indicator
  int bar = (telemetry_count % 16) + 1;
  for(int i = 0; i < bar; i++){
    hub75_display.setPixel(w - 2, h - 1 - i, RGB(0, 128, 255));
  }
  
  hub75_display.show();
  frame_count++;
}

void drawTelemetryOnOLED(){
  oled_display.clearBuffer();
  
  // Title
  oled_display.drawString(0, 0, "ARCOS GPU v0.1", true);
  oled_display.drawLine(0, 12, 127, 12, true);
  
  char buf[32];
  
  if(telemetry_valid){
    oled_display.drawString(0, 16, "CPU: CONNECTED", true);
    
    // Orientation
    float r = received_telemetry.motion.euler.x * math::RAD_TO_DEG;
    float p = received_telemetry.motion.euler.y * math::RAD_TO_DEG;
    float y = received_telemetry.motion.euler.z * math::RAD_TO_DEG;
    snprintf(buf, sizeof(buf), "R:%.1f P:%.1f Y:%.1f", r, p, y);
    oled_display.drawString(0, 28, buf, true);
    
    // Environment
    snprintf(buf, sizeof(buf), "T:%.1fC H:%d%%", 
             received_telemetry.environment.temperature,
             (int)received_telemetry.environment.humidity);
    oled_display.drawString(0, 44, buf, true);
    
    snprintf(buf, sizeof(buf), "P:%.0f hPa", received_telemetry.environment.pressure);
    oled_display.drawString(0, 56, buf, true);
    
    // Stats
    oled_display.drawLine(0, 70, 127, 70, true);
    snprintf(buf, sizeof(buf), "Packets: %lu", telemetry_count);
    oled_display.drawString(0, 74, buf, true);
    
    snprintf(buf, sizeof(buf), "Age: %lu ms", getTimeMs() - last_telemetry_time);
    oled_display.drawString(0, 86, buf, true);
    
    oled_display.drawString(0, 100, received_telemetry.motion.is_stable ? "STABLE" : "MOTION", true);
  }else{
    oled_display.drawString(0, 40, "Waiting for CPU...", true);
    snprintf(buf, sizeof(buf), "RX: %lu bytes", rx_count);
    oled_display.drawString(0, 56, buf, true);
  }
  
  // Frame count
  snprintf(buf, sizeof(buf), "F:%lu", frame_count);
  oled_display.drawString(0, 116, buf, true);
  
  oled_display.updateDisplay();
}

// ============================================================
// Communication
// ============================================================

void processUART(){
  uint8_t byte;
  while(uart_read_bytes(UART_PORT, &byte, 1, 0) > 0){
    rx_count++;
    
    if(packet_parser.feed(byte)){
      PacketType type = packet_parser.getType();
      
      if(type == PacketType::TELEMETRY){
        if(packet_parser.parseTelemetry(received_telemetry)){
          telemetry_valid = true;
          telemetry_count++;
          last_telemetry_time = getTimeMs();
        }
      }
      
      packet_parser.reset();
    }
  }
}

void sendHeartbeat(){
  // Simple raw heartbeat - just send status byte
  uint8_t status = 0x80;  // GPU alive
  if(hub75_ok) status |= 0x01;
  if(oled_ok) status |= 0x02;
  
  uart_write_bytes(UART_PORT, &status, 1);
  tx_count++;
}

// ============================================================
// Test Protocol
// ============================================================

bool testProtocol(){
  ESP_LOGI(TAG, "Testing protocol...");
  
  // Build a test telemetry packet
  PacketBuilder builder(tx_packet_buffer, sizeof(tx_packet_buffer));
  builder.begin(PacketType::TELEMETRY);
  
  TelemetryData test;
  test.timestamp = 12345;
  test.motion.euler = Vec3(0.5f, 0.1f, 1.0f);
  test.environment.temperature = 25.0f;
  test.environment.humidity = 50.0f;
  test.environment.pressure = 1013.0f;
  test.imu_ok = true;
  
  if(!builder.addTelemetry(test)){
    ESP_LOGE(TAG, "addTelemetry failed");
    return false;
  }
  
  size_t len = builder.finalize();
  const uint8_t* pkt = builder.data();
  
  ESP_LOGI(TAG, "Built %d byte packet", (int)len);
  
  // Parse it back
  PacketParser parser(rx_packet_buffer, sizeof(rx_packet_buffer));
  for(size_t i = 0; i < len; i++){
    if(parser.feed(pkt[i])){
      TelemetryData parsed;
      if(parser.parseTelemetry(parsed)){
        float r1 = test.motion.euler.x * math::RAD_TO_DEG;
        float r2 = parsed.motion.euler.x * math::RAD_TO_DEG;
        ESP_LOGI(TAG, "Roll: sent=%.2f parsed=%.2f", r1, r2);
        
        if(fabsf(r1 - r2) < 1.0f){
          ESP_LOGI(TAG, "Protocol test PASSED");
          return true;
        }
      }
    }
  }
  
  ESP_LOGE(TAG, "Protocol test FAILED");
  return false;
}

// ============================================================
// Main
// ============================================================

extern "C" void app_main(){
  ESP_LOGI(TAG, "=== GPU Base API Test ===");
  
  uart_ok = initUART();
  bool i2c_ok = initI2C();
  hub75_ok = initHUB75();
  if(i2c_ok) oled_ok = initOLED();
  
  // Run protocol test
  bool protocol_ok = testProtocol();
  
  ESP_LOGI(TAG, "\n=== TEST RESULTS ===");
  ESP_LOGI(TAG, "Protocol: %s", protocol_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "UART:     %s", uart_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "HUB75:    %s", hub75_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "OLED:     %s", oled_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "====================\n");
  
  ESP_LOGI(TAG, "Entering main loop...");
  
  uint32_t last_heartbeat = 0;
  uint32_t last_display = 0;
  uint32_t last_status = 0;
  
  while(true){
    uint32_t now = getTimeMs();
    
    if(uart_ok) processUART();
    
    if(uart_ok && (now - last_heartbeat >= 500)){
      sendHeartbeat();
      last_heartbeat = now;
    }
    
    if(now - last_display >= 33){
      if(hub75_ok) drawTelemetryOnHUB75();
      
      static uint32_t last_oled = 0;
      if(oled_ok && (now - last_oled >= 100)){
        drawTelemetryOnOLED();
        last_oled = now;
      }
      
      last_display = now;
    }
    
    if(now - last_status >= 5000){
      ESP_LOGI(TAG, "Status: HUB75=%s OLED=%s RX=%lu Telem=%lu Frames=%lu",
               hub75_ok ? "OK" : "FAIL", oled_ok ? "OK" : "FAIL",
               rx_count, telemetry_count, frame_count);
      if(telemetry_valid){
        ESP_LOGI(TAG, "  R=%.1f P=%.1f Y=%.1f",
                 received_telemetry.motion.euler.x * math::RAD_TO_DEG,
                 received_telemetry.motion.euler.y * math::RAD_TO_DEG,
                 received_telemetry.motion.euler.z * math::RAD_TO_DEG);
      }
      printMemoryStats();
      last_status = now;
    }
    
    vTaskDelay(1);
  }
}
