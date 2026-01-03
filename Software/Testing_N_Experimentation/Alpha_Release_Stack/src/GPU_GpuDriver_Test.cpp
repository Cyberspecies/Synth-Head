/*****************************************************************
 * File:      GPU_GpuDriver_Test.cpp
 * Category:  Hardware Integration Test
 * Author:    Generated for GPU Driver Testing
 * 
 * Purpose:
 *    GPU-side test receiver that accepts commands from CPU via UART
 *    and renders them using the GPU Driver system.
 * 
 * Usage:
 *    1. Upload this to GPU (COM5)
 *    2. Upload CPU_GpuDriver_Test.cpp to CPU (COM15)
 *    3. Monitor GPU serial output for test results
 *****************************************************************/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

// Include the ARCOS display drivers
#include "abstraction/hal.hpp"
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

// Include the GpuDriver components
#include "GpuDriver/GpuBaseAPI.hpp"

static const char* TAG = "GPU_TEST";

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;
using namespace gpu;

// ============================================================
// Hardware Configuration
// ============================================================

#define UART_NUM        UART_NUM_1
#define UART_TX_PIN     GPIO_NUM_12
#define UART_RX_PIN     GPIO_NUM_13
#define UART_BUF_SIZE   8192

// ============================================================
// Display Hardware
// ============================================================

SimpleHUB75Display hub75_display;
DRIVER_OLED_SH1107 oled_display(0x3C, 0);

// Statistics
struct Stats {
  uint32_t commands_received = 0;
  uint32_t sync_errors = 0;
  uint32_t checksum_errors = 0;
  uint32_t frames_rendered = 0;
  uint32_t draw_commands = 0;
  uint32_t system_commands = 0;
  uint32_t buffer_commands = 0;
  uint32_t bytes_received = 0;
  uint32_t pongs_sent = 0;
};
Stats stats;

static bool hub75_ok = false;
static bool oled_ok = false;

// ============================================================
// Initialization
// ============================================================

bool initI2C() {
  ESP_LOGI(TAG, "Initializing I2C for OLED...");
  HalResult result = ESP32S3_I2C_HAL::Initialize(0, 2, 1, 400000, 1000);
  if (result != HalResult::Success) {
    ESP_LOGE(TAG, "Failed to initialize I2C!");
    return false;
  }
  ESP_LOGI(TAG, "I2C initialized (SDA=2, SCL=1 @ 400kHz)");
  return true;
}

void initDisplays() {
  ESP_LOGI(TAG, "Initializing displays...");
  
  // Initialize HUB75
  if (hub75_display.begin()) {
    hub75_display.setBrightness(128);
    hub75_ok = true;
    ESP_LOGI(TAG, "HUB75 display initialized: %dx%d", hub75_display.getWidth(), hub75_display.getHeight());
  } else {
    ESP_LOGE(TAG, "Failed to initialize HUB75 display!");
  }
  
  // Initialize OLED
  if (initI2C()) {
    if (oled_display.initialize()) {
      oled_display.clearBuffer();
      oled_display.updateDisplay();
      oled_ok = true;
      ESP_LOGI(TAG, "OLED display initialized: 128x128");
    } else {
      ESP_LOGE(TAG, "Failed to initialize OLED display!");
    }
  }
}

void initUART() {
  ESP_LOGI(TAG, "Initializing UART at %lu baud...", (uint32_t)GPU_BAUD_RATE);
  
  uart_config_t uart_config = {};
  uart_config.baud_rate = GPU_BAUD_RATE;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.rx_flow_ctrl_thresh = 0;
  uart_config.source_clk = UART_SCLK_DEFAULT;
  
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, 
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
  
  ESP_LOGI(TAG, "UART initialized: TX=%d, RX=%d", UART_TX_PIN, UART_RX_PIN);
}

// ============================================================
// Drawing Functions - Use SimpleHUB75Display API
// ============================================================

void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (hub75_ok) {
    hub75_display.setPixel(x, y, RGB(r, g, b));
  }
}

void drawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
  // Bresenham's line algorithm
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  
  while (true) {
    setPixel(x0, y0, r, g, b);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx) { err += dx; y0 += sy; }
  }
}

void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  drawLine(x, y, x + w - 1, y, r, g, b);
  drawLine(x + w - 1, y, x + w - 1, y + h - 1, r, g, b);
  drawLine(x + w - 1, y + h - 1, x, y + h - 1, r, g, b);
  drawLine(x, y + h - 1, x, y, r, g, b);
}

void fillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  for (int py = y; py < y + h; py++) {
    for (int px = x; px < x + w; px++) {
      setPixel(px, py, r, g, b);
    }
  }
}

void drawCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) {
  int x = radius;
  int y = 0;
  int err = 0;
  
  while (x >= y) {
    setPixel(cx + x, cy + y, r, g, b);
    setPixel(cx + y, cy + x, r, g, b);
    setPixel(cx - y, cy + x, r, g, b);
    setPixel(cx - x, cy + y, r, g, b);
    setPixel(cx - x, cy - y, r, g, b);
    setPixel(cx - y, cy - x, r, g, b);
    setPixel(cx + y, cy - x, r, g, b);
    setPixel(cx + x, cy - y, r, g, b);
    
    y++;
    err += 1 + 2 * y;
    if (2 * (err - x) + 1 > 0) {
      x--;
      err += 1 - 2 * x;
    }
  }
}

void fillCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) {
  for (int py = -radius; py <= radius; py++) {
    for (int px = -radius; px <= radius; px++) {
      if (px * px + py * py <= radius * radius) {
        setPixel(cx + px, cy + py, r, g, b);
      }
    }
  }
}

void clearBuffer(uint8_t r, uint8_t g, uint8_t b) {
  if (hub75_ok) {
    hub75_display.fill(RGB(r, g, b));
  }
}

void swapBuffer() {
  if (hub75_ok) {
    hub75_display.show();
  }
  stats.frames_rendered++;
}

// ============================================================
// Command Processing
// ============================================================

void processPacket(const uint8_t* data, size_t len) {
  if (len < sizeof(PacketHeader)) return;
  
  const PacketHeader* hdr = (const PacketHeader*)data;
  const uint8_t* payload = data + sizeof(PacketHeader);
  uint16_t payload_len = hdr->payload_len;
  
  CmdCategory category = static_cast<CmdCategory>(hdr->category);
  
  ESP_LOGD(TAG, "CMD: cat=0x%02X cmd=0x%02X len=%d", hdr->category, hdr->command, payload_len);
  
  switch (category) {
    case CmdCategory::SYSTEM:
      stats.system_commands++;
      switch (hdr->command) {
        case 0x06:  // PING - Send PONG response
          {
            ESP_LOGI(TAG, "PING received - sending PONG");
            // Build PONG response packet
            uint8_t pong[sizeof(PacketHeader) + sizeof(PacketFooter)];
            PacketHeader* pong_hdr = (PacketHeader*)pong;
            pong_hdr->sync1 = SYNC_BYTE_1;
            pong_hdr->sync2 = SYNC_BYTE_2;
            pong_hdr->sync3 = SYNC_BYTE_3;
            pong_hdr->version = PROTOCOL_VERSION;
            pong_hdr->category = static_cast<uint8_t>(CmdCategory::SYSTEM);
            pong_hdr->command = 0x07;  // PONG
            pong_hdr->display = hdr->display;
            pong_hdr->flags = 0;
            pong_hdr->payload_len = 0;
            pong_hdr->seq_num = hdr->seq_num;
            
            // Calculate checksum
            uint16_t chk = 0;
            for (size_t i = 0; i < sizeof(PacketHeader); i++) {
              chk += pong[i];
            }
            PacketFooter* pong_ftr = (PacketFooter*)(pong + sizeof(PacketHeader));
            pong_ftr->checksum = chk;
            pong_ftr->end_byte = SYNC_BYTE_2;
            
            uart_write_bytes(UART_NUM, pong, sizeof(pong));
            stats.pongs_sent++;
          }
          break;
        case 0x03:  // STATUS
          ESP_LOGI(TAG, "STATUS: cmds=%lu draws=%lu frames=%lu", 
                   stats.commands_received, stats.draw_commands, stats.frames_rendered);
          break;
        case 0x04:  // SET_BRIGHTNESS
          if (payload_len >= 1 && hub75_ok) {
            hub75_display.setBrightness(payload[0]);
            ESP_LOGI(TAG, "Brightness set to %d", payload[0]);
          }
          break;
      }
      break;
      
    case CmdCategory::DRAW:
      stats.draw_commands++;
      switch (hdr->command) {
        case 0x10:  // PIXEL
          if (payload_len >= 7) {
            int16_t x = *(int16_t*)payload;
            int16_t y = *(int16_t*)(payload + 2);
            uint8_t r = payload[4], g = payload[5], b = payload[6];
            setPixel(x, y, r, g, b);
            ESP_LOGD(TAG, "PIXEL (%d,%d) RGB(%d,%d,%d)", x, y, r, g, b);
          }
          break;
        case 0x11:  // LINE
          if (payload_len >= 12) {
            int16_t x0 = *(int16_t*)payload;
            int16_t y0 = *(int16_t*)(payload + 2);
            int16_t x1 = *(int16_t*)(payload + 4);
            int16_t y1 = *(int16_t*)(payload + 6);
            uint8_t r = payload[8], g = payload[9], b = payload[10];
            drawLine(x0, y0, x1, y1, r, g, b);
            ESP_LOGD(TAG, "LINE (%d,%d)-(%d,%d)", x0, y0, x1, y1);
          }
          break;
        case 0x12:  // RECT
          if (payload_len >= 12) {
            int16_t x = *(int16_t*)payload;
            int16_t y = *(int16_t*)(payload + 2);
            uint16_t w = *(uint16_t*)(payload + 4);
            uint16_t h = *(uint16_t*)(payload + 6);
            uint8_t r = payload[8], g = payload[9], b = payload[10];
            drawRect(x, y, w, h, r, g, b);
            ESP_LOGD(TAG, "RECT (%d,%d) %dx%d", x, y, w, h);
          }
          break;
        case 0x13:  // RECT_FILL
          if (payload_len >= 12) {
            int16_t x = *(int16_t*)payload;
            int16_t y = *(int16_t*)(payload + 2);
            uint16_t w = *(uint16_t*)(payload + 4);
            uint16_t h = *(uint16_t*)(payload + 6);
            uint8_t r = payload[8], g = payload[9], b = payload[10];
            fillRect(x, y, w, h, r, g, b);
            ESP_LOGD(TAG, "RECT_FILL (%d,%d) %dx%d", x, y, w, h);
          }
          break;
        case 0x14:  // CIRCLE
          if (payload_len >= 10) {
            int16_t cx = *(int16_t*)payload;
            int16_t cy = *(int16_t*)(payload + 2);
            uint16_t radius = *(uint16_t*)(payload + 4);
            uint8_t r = payload[6], g = payload[7], b = payload[8];
            drawCircle(cx, cy, radius, r, g, b);
            ESP_LOGD(TAG, "CIRCLE (%d,%d) r=%d", cx, cy, radius);
          }
          break;
        case 0x15:  // CIRCLE_FILL
          if (payload_len >= 10) {
            int16_t cx = *(int16_t*)payload;
            int16_t cy = *(int16_t*)(payload + 2);
            uint16_t radius = *(uint16_t*)(payload + 4);
            uint8_t r = payload[6], g = payload[7], b = payload[8];
            fillCircle(cx, cy, radius, r, g, b);
            ESP_LOGD(TAG, "CIRCLE_FILL (%d,%d) r=%d", cx, cy, radius);
          }
          break;
      }
      break;
      
    case CmdCategory::BUFFER:
      stats.buffer_commands++;
      switch (hdr->command) {
        case 0x70:  // CLEAR
          if (payload_len >= 3) {
            clearBuffer(payload[0], payload[1], payload[2]);
          } else {
            clearBuffer(0, 0, 0);
          }
          ESP_LOGD(TAG, "CLEAR");
          break;
        case 0x71:  // SWAP
          swapBuffer();
          ESP_LOGD(TAG, "SWAP");
          break;
      }
      break;
      
    case CmdCategory::EFFECT:
      ESP_LOGI(TAG, "EFFECT cmd=0x%02X", hdr->command);
      break;
      
    default:
      ESP_LOGW(TAG, "Unknown category: 0x%02X", hdr->category);
      break;
  }
  
  stats.commands_received++;
}

// ============================================================
// UART Receiver Task
// ============================================================

static uint8_t rx_buffer[UART_BUF_SIZE];
static size_t rx_pos = 0;

void uartTask(void* pvParameters) {
  ESP_LOGI(TAG, "UART receiver task started");
  
  uint8_t temp_buf[256];
  
  while (1) {
    // Read available bytes in bulk for efficiency
    int len = uart_read_bytes(UART_NUM, temp_buf, sizeof(temp_buf), pdMS_TO_TICKS(5));
    
    if (len > 0) {
      stats.bytes_received += len;
      
      // Add to buffer
      for (int i = 0; i < len; i++) {
        if (rx_pos < sizeof(rx_buffer)) {
          rx_buffer[rx_pos++] = temp_buf[i];
        } else {
          // Buffer overflow - reset
          rx_pos = 0;
          stats.sync_errors++;
          break;
        }
      }
      
      // Process all complete packets in buffer
      while (rx_pos >= 3) {
        // Check if we have sync at the start
        if (rx_buffer[0] != SYNC_BYTE_1 || 
            rx_buffer[1] != SYNC_BYTE_2 || 
            rx_buffer[2] != SYNC_BYTE_3) {
          // No sync - find it
          size_t shift = 1;
          bool found = false;
          for (size_t j = 1; j <= rx_pos - 3; j++) {
            if (rx_buffer[j] == SYNC_BYTE_1 && 
                rx_buffer[j+1] == SYNC_BYTE_2 && 
                rx_buffer[j+2] == SYNC_BYTE_3) {
              shift = j;
              found = true;
              break;
            }
          }
          if (!found) {
            // No sync found - keep last 2 bytes in case partial sync
            if (rx_pos > 2) {
              rx_buffer[0] = rx_buffer[rx_pos-2];
              rx_buffer[1] = rx_buffer[rx_pos-1];
              stats.sync_errors += (rx_pos - 2);  // Count discarded bytes
              rx_pos = 2;
            }
            break;
          }
          memmove(rx_buffer, rx_buffer + shift, rx_pos - shift);
          stats.sync_errors += shift;  // Count discarded bytes
          rx_pos -= shift;
          continue;
        }
        
        // We have sync at start - check if header complete
        if (rx_pos < sizeof(PacketHeader)) break;
      
        // Check packet length
        PacketHeader* hdr = (PacketHeader*)rx_buffer;
        size_t packet_len = sizeof(PacketHeader) + hdr->payload_len + sizeof(PacketFooter);
        
        // Sanity check payload length
        if (hdr->payload_len > 4096) {
          // Invalid packet - skip sync bytes
          memmove(rx_buffer, rx_buffer + 3, rx_pos - 3);
          rx_pos -= 3;
          stats.sync_errors += 3;
          continue;
        }
        
        if (rx_pos < packet_len) break;  // Need more data
        
        // Have complete packet - process it
        processPacket(rx_buffer, packet_len);
        
        // Remove packet from buffer
        if (rx_pos > packet_len) {
          memmove(rx_buffer, rx_buffer + packet_len, rx_pos - packet_len);
        }
        rx_pos -= packet_len;
      }
    } else {
      // No data - small yield
      vTaskDelay(1);
    }
  }
}

// ============================================================
// Status Task
// ============================================================

void statusTask(void* pvParameters) {
  ESP_LOGI(TAG, "Status task started");
  
  while (1) {
    ESP_LOGI(TAG, "Stats: cmds=%lu sys=%lu draw=%lu buf=%lu frames=%lu bytes=%lu sync_err=%lu pongs=%lu",
             stats.commands_received,
             stats.system_commands,
             stats.draw_commands,
             stats.buffer_commands,
             stats.frames_rendered,
             stats.bytes_received,
             stats.sync_errors,
             stats.pongs_sent);
    
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// ============================================================
// Main Entry
// ============================================================

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "  GPU DRIVER TEST RECEIVER");
  ESP_LOGI(TAG, "  Waiting for commands from CPU...");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "");
  
  // Initialize systems
  initDisplays();
  initUART();
  
  // Clear display to show we're ready
  if (hub75_ok) {
    hub75_display.fill(RGB(0, 32, 0));  // Dark green = ready
    hub75_display.show();
  }
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "GPU ready - displays initialized");
  ESP_LOGI(TAG, "  HUB75: %s", hub75_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "  OLED:  %s", oled_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "");
  
  // Create tasks
  xTaskCreatePinnedToCore(uartTask, "UARTTask", 8192, NULL, 5, NULL, 0);
  xTaskCreate(statusTask, "StatusTask", 4096, NULL, 1, NULL);
  
  ESP_LOGI(TAG, "Tasks started - ready for tests!");
}
