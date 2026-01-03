/*****************************************************************
 * GPU_CommandRenderer.cpp - Command-Driven GPU Display Renderer
 * 
 * Receives rendering commands from CPU via UART and renders:
 * - HUB75: 128x32 RGB (2x 64x32 panels) with shapes
 * - OLED: 128x128 monochrome
 * 
 * Features:
 * - Dual polygon rendering (one per panel, centered)
 * - CPU command protocol for rendering control
 * - Local GPU-side animation when no commands
 * 
 * Pin Configuration:
 * - HUB75: Standard pinout (see HUB75Config)
 * - OLED I2C: SDA=GPIO2, SCL=GPIO1
 * - UART: RX=GPIO13, TX=GPIO12
 *****************************************************************/

#include <stdio.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/uart.h"

#include "abstraction/hal.hpp"
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

static const char* TAG = "GPU_CMD_RENDER";

// ============================================================
// Display Configuration
// ============================================================
constexpr int HUB75_WIDTH = 128;
constexpr int HUB75_HEIGHT = 32;
constexpr int PANEL_WIDTH = 64;
constexpr int PANEL_HEIGHT = 32;
constexpr int OLED_SIZE = 128;

// Display objects
SimpleHUB75Display hub75;
DRIVER_OLED_SH1107 oled(0x3C, 0);

// ============================================================
// Command Protocol Definitions
// ============================================================
namespace CommandProtocol {
  constexpr uint8_t SYNC_1 = 0xAA;
  constexpr uint8_t SYNC_2 = 0x55;
  constexpr uint8_t SYNC_3 = 0xCC;
  
  enum class CmdType : uint8_t {
    // Control commands
    NOP           = 0x00,
    PING          = 0x01,
    CLEAR_ALL     = 0x02,
    SHOW          = 0x03,  // Flush buffers to displays
    SET_BRIGHTNESS = 0x04,
    
    // HUB75 drawing commands
    HUB75_CLEAR   = 0x10,
    HUB75_PIXEL   = 0x11,
    HUB75_LINE    = 0x12,
    HUB75_RECT    = 0x13,
    HUB75_POLYGON = 0x14,
    HUB75_FILL    = 0x15,
    HUB75_RAW     = 0x16,  // Raw framebuffer upload
    
    // OLED drawing commands
    OLED_CLEAR    = 0x20,
    OLED_PIXEL    = 0x21,
    OLED_LINE     = 0x22,
    OLED_RECT     = 0x23,
    OLED_CIRCLE   = 0x24,
    OLED_TEXT     = 0x25,
    OLED_RAW      = 0x26,  // Raw buffer upload
    
    // Animation commands
    ANIM_ENABLE   = 0x30,
    ANIM_DISABLE  = 0x31,
    ANIM_SET_MODE = 0x32,
    
    // Status/debug
    GET_STATUS    = 0x40,
  };
  
  #pragma pack(push, 1)
  struct CommandHeader {
    uint8_t sync1;
    uint8_t sync2;
    uint8_t sync3;
    uint8_t cmd_type;
    uint16_t payload_len;
    uint16_t checksum;
  };
  #pragma pack(pop)
}

// ============================================================
// Polygon Shape Data (Original User Vertices)
// These vertices define the base shape for panel 0
// ============================================================
constexpr int SHAPE_VERTEX_COUNT = 16;
const int8_t SHAPE_VERTICES[SHAPE_VERTEX_COUNT][2] = {
  {6, 8}, {14, 8}, {20, 11}, {26, 17}, 
  {27, 19}, {28, 22}, {23, 22}, {21, 19},
  {19, 17}, {17, 17}, {16, 19}, {18, 22}, 
  {7, 22}, {4, 20}, {2, 17}, {2, 12}
};

// Calculate shape bounding box for centering
int shapeMinX = 2, shapeMaxX = 28;
int shapeMinY = 8, shapeMaxY = 22;
int shapeWidth = shapeMaxX - shapeMinX;   // 26
int shapeHeight = shapeMaxY - shapeMinY;  // 14
int shapeCenterX = (shapeMinX + shapeMaxX) / 2;  // 15
int shapeCenterY = (shapeMinY + shapeMaxY) / 2;  // 15

// ============================================================
// Framebuffer (local GPU buffer)
// ============================================================
RGB framebuffer[HUB75_WIDTH * HUB75_HEIGHT];

// ============================================================
// Drawing Primitives
// ============================================================

void clearFramebuffer(RGB color = RGB(0, 0, 0)) {
  for (int i = 0; i < HUB75_WIDTH * HUB75_HEIGHT; i++) {
    framebuffer[i] = color;
  }
}

void setPixel(int x, int y, RGB color) {
  if (x >= 0 && x < HUB75_WIDTH && y >= 0 && y < HUB75_HEIGHT) {
    framebuffer[y * HUB75_WIDTH + x] = color;
  }
}

RGB getPixel(int x, int y) {
  if (x >= 0 && x < HUB75_WIDTH && y >= 0 && y < HUB75_HEIGHT) {
    return framebuffer[y * HUB75_WIDTH + x];
  }
  return RGB(0, 0, 0);
}

void drawLine(int x0, int y0, int x1, int y1, RGB color) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  
  while (true) {
    setPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void drawPolygonOutline(const int* vertices, int count, int offsetX, int offsetY, RGB color) {
  for (int i = 0; i < count; i++) {
    int x0 = vertices[i * 2] + offsetX;
    int y0 = vertices[i * 2 + 1] + offsetY;
    int x1 = vertices[((i + 1) % count) * 2] + offsetX;
    int y1 = vertices[((i + 1) % count) * 2 + 1] + offsetY;
    drawLine(x0, y0, x1, y1, color);
  }
}

// Scanline fill for convex/concave polygons
void fillPolygonScanline(const int* vertices, int count, int offsetX, int offsetY, RGB color) {
  if (count < 3) return;
  
  // Find bounding box
  int minY = INT_MAX, maxY = INT_MIN;
  for (int i = 0; i < count; i++) {
    int y = vertices[i * 2 + 1] + offsetY;
    minY = std::min(minY, y);
    maxY = std::max(maxY, y);
  }
  
  minY = std::max(0, minY);
  maxY = std::min(HUB75_HEIGHT - 1, maxY);
  
  // Scanline fill
  for (int y = minY; y <= maxY; y++) {
    // Find intersections with all edges
    int intersections[32];
    int intCount = 0;
    
    for (int i = 0; i < count && intCount < 32; i++) {
      int x0 = vertices[i * 2] + offsetX;
      int y0 = vertices[i * 2 + 1] + offsetY;
      int x1 = vertices[((i + 1) % count) * 2] + offsetX;
      int y1 = vertices[((i + 1) % count) * 2 + 1] + offsetY;
      
      if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
        int dx = x1 - x0;
        int dy = y1 - y0;
        int x = x0 + (y - y0) * dx / dy;
        intersections[intCount++] = x;
      }
    }
    
    // Sort intersections
    for (int i = 0; i < intCount - 1; i++) {
      for (int j = i + 1; j < intCount; j++) {
        if (intersections[i] > intersections[j]) {
          int tmp = intersections[i];
          intersections[i] = intersections[j];
          intersections[j] = tmp;
        }
      }
    }
    
    // Fill between pairs
    for (int i = 0; i + 1 < intCount; i += 2) {
      int xStart = std::max(0, intersections[i]);
      int xEnd = std::min(HUB75_WIDTH - 1, intersections[i + 1]);
      for (int x = xStart; x <= xEnd; x++) {
        setPixel(x, y, color);
      }
    }
  }
}

// ============================================================
// Fast Plasma Shader using Integer Math + LUT
// ============================================================
uint8_t plasma_time = 0;

// Pre-computed sine table (256 entries, 0-255 range output)
static const uint8_t SIN_LUT[256] = {
  128,131,134,137,140,143,146,149,152,155,158,161,164,167,170,173,
  176,179,182,185,188,190,193,196,198,201,203,206,208,210,213,215,
  217,219,221,223,225,227,228,230,231,233,234,236,237,238,239,240,
  241,242,243,244,244,245,246,246,247,247,247,248,248,248,248,248,
  248,248,248,248,248,247,247,247,246,246,245,244,244,243,242,241,
  240,239,238,237,236,234,233,231,230,228,227,225,223,221,219,217,
  215,213,210,208,206,203,201,198,196,193,190,188,185,182,179,176,
  173,170,167,164,161,158,155,152,149,146,143,140,137,134,131,128,
  125,122,119,116,113,110,107,104,101,98,95,92,89,86,83,80,
  77,74,71,68,65,63,60,57,55,52,50,47,45,43,40,38,
  36,34,32,30,28,26,25,23,22,20,19,17,16,15,14,13,
  12,11,10,9,9,8,7,7,6,6,6,5,5,5,5,5,
  5,5,5,5,5,6,6,6,7,7,8,9,9,10,11,12,
  13,14,15,16,17,19,20,22,23,25,26,28,30,32,34,36,
  38,40,43,45,47,50,52,55,57,60,63,65,68,71,74,77,
  80,83,86,89,92,95,98,101,104,107,110,113,116,119,122,125
};

// Fast integer plasma calculation
RGB plasmaColorFast(int x, int y, uint8_t time) {
  // Use integer math with phase offsets
  uint8_t v1 = SIN_LUT[(uint8_t)(x * 10 + time * 2)];
  uint8_t v2 = SIN_LUT[(uint8_t)(y * 12 + time)];
  uint8_t v3 = SIN_LUT[(uint8_t)((x + y) * 6 + time * 3)];
  
  // Average the three waves
  uint16_t v = (v1 + v2 + v3) / 3;
  
  // Create RGB from wave value with phase offsets
  uint8_t r = SIN_LUT[(uint8_t)(v)];
  uint8_t g = SIN_LUT[(uint8_t)(v + 85)];   // +1/3 phase
  uint8_t b = SIN_LUT[(uint8_t)(v + 170)];  // +2/3 phase
  
  return RGB(r, g, b);
}

// ============================================================
// Animation State
// ============================================================
bool animation_enabled = true;
uint8_t animation_mode = 0;  // 0=plasma fill, 1=color cycle, 2=pulsing, 3+=static
uint32_t frame_count = 0;
uint32_t last_fps_time = 0;
uint32_t fps = 0;

// ============================================================
// Render Dual Panel Shapes
// ============================================================
void renderDualPanelShapes() {
  clearFramebuffer();
  
  // Convert shape vertices to flat array for drawing functions
  int flatVertices[SHAPE_VERTEX_COUNT * 2];
  for (int i = 0; i < SHAPE_VERTEX_COUNT; i++) {
    flatVertices[i * 2] = SHAPE_VERTICES[i][0] - shapeCenterX;
    flatVertices[i * 2 + 1] = SHAPE_VERTICES[i][1] - shapeCenterY;
  }
  
  // Panel 0: Left panel (x: 0-63), center at (32, 16)
  int panel0CenterX = 32;
  int panel0CenterY = 16;
  
  // Panel 1: Right panel (x: 64-127), center at (96, 16)
  int panel1CenterX = 96;
  int panel1CenterY = 16;
  
  if (animation_enabled) {
    plasma_time++;
    
    switch (animation_mode) {
      case 0:  // Plasma-filled shapes (no outline)
      {
        // Fill shapes first with placeholder
        fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                           panel0CenterX, panel0CenterY, RGB(255, 255, 255));
        fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                           panel1CenterX, panel1CenterY, RGB(255, 255, 255));
        
        // Apply plasma shader to filled pixels (using fast LUT version)
        for (int y = 0; y < HUB75_HEIGHT; y++) {
          for (int x = 0; x < HUB75_WIDTH; x++) {
            RGB px = getPixel(x, y);
            if (px.r > 0 || px.g > 0 || px.b > 0) {
              setPixel(x, y, plasmaColorFast(x, y, plasma_time));
            }
          }
        }
        // No white outline - just plasma filled shapes
        break;
      }
      
      case 1:  // Color cycling outlines (using LUT)
      {
        uint8_t phase = plasma_time * 4;
        RGB color1(SIN_LUT[phase], SIN_LUT[(uint8_t)(phase + 85)], SIN_LUT[(uint8_t)(phase + 170)]);
        RGB color2(SIN_LUT[(uint8_t)(phase + 128)], SIN_LUT[(uint8_t)(phase + 213)], SIN_LUT[(uint8_t)(phase + 42)]);
        
        // Fill with solid colors
        fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                           panel0CenterX, panel0CenterY, color1);
        fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                           panel1CenterX, panel1CenterY, color2);
        break;
      }
      
      case 2:  // Static with pulsing brightness (using LUT, pure integer)
      {
        // Use plasma_time directly for pulsing - no floats
        uint8_t brightness = SIN_LUT[(uint8_t)(plasma_time * 3)];
        
        fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                           panel0CenterX, panel0CenterY, RGB(brightness, 0, brightness));
        fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                           panel1CenterX, panel1CenterY, RGB(0, brightness, brightness));
        break;
      }
      
      default:  // Solid colored shapes
      {
        fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                           panel0CenterX, panel0CenterY, RGB(255, 100, 50));
        fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                           panel1CenterX, panel1CenterY, RGB(50, 100, 255));
        break;
      }
    }
  } else {
    // Static rendering - solid red/blue
    fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                       panel0CenterX, panel0CenterY, RGB(255, 0, 0));
    fillPolygonScanline(flatVertices, SHAPE_VERTEX_COUNT, 
                       panel1CenterX, panel1CenterY, RGB(0, 0, 255));
  }
}

// ============================================================
// Flush Framebuffer to HUB75
// ============================================================
void flushToHUB75() {
  for (int y = 0; y < HUB75_HEIGHT; y++) {
    for (int x = 0; x < HUB75_WIDTH; x++) {
      hub75.setPixel(x, y, framebuffer[y * HUB75_WIDTH + x]);
    }
  }
  hub75.show();
}

// ============================================================
// OLED Rendering
// ============================================================
void renderOLEDStatus() {
  oled.clearBuffer();
  
  // Draw border
  oled.drawRect(0, 0, 128, 128, false, true);
  
  // Title
  oled.drawString(10, 10, "GPU RENDERER", true);
  
  // Status
  char buf[32];
  snprintf(buf, sizeof(buf), "FPS: %lu", (unsigned long)fps);
  oled.drawString(10, 30, buf, true);
  
  snprintf(buf, sizeof(buf), "Frame: %lu", (unsigned long)frame_count);
  oled.drawString(10, 45, buf, true);
  
  snprintf(buf, sizeof(buf), "Anim: %s", animation_enabled ? "ON" : "OFF");
  oled.drawString(10, 60, buf, true);
  
  snprintf(buf, sizeof(buf), "Mode: %d", animation_mode);
  oled.drawString(10, 75, buf, true);
  
  // Draw mini preview of shape
  int previewX = 64 - 15;  // Center horizontally
  int previewY = 100;
  for (int i = 0; i < SHAPE_VERTEX_COUNT; i++) {
    int x0 = SHAPE_VERTICES[i][0] - shapeCenterX + previewX;
    int y0 = SHAPE_VERTICES[i][1] - shapeCenterY + previewY;
    int x1 = SHAPE_VERTICES[(i + 1) % SHAPE_VERTEX_COUNT][0] - shapeCenterX + previewX;
    int y1 = SHAPE_VERTICES[(i + 1) % SHAPE_VERTEX_COUNT][1] - shapeCenterY + previewY;
    oled.drawLine(x0, y0, x1, y1, true);
  }
  
  oled.updateDisplay();
}

// ============================================================
// UART Command Handling
// ============================================================
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int UART_RX_PIN = 13;
constexpr int UART_TX_PIN = 12;
constexpr int UART_BAUD = 10000000;  // 10 Mbps
constexpr int UART_BUF_SIZE = 4096;

uint8_t uart_rx_buffer[UART_BUF_SIZE];

bool initUART() {
  uart_config_t uart_config = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_APB,
    .flags = {0}
  };
  
  esp_err_t err = uart_param_config(UART_PORT, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART config failed: %d", err);
    return false;
  }
  
  err = uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART set pin failed: %d", err);
    return false;
  }
  
  err = uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, UART_BUF_SIZE, 0, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART driver install failed: %d", err);
    return false;
  }
  
  ESP_LOGI(TAG, "UART initialized: %d baud, RX=%d, TX=%d", 
           UART_BAUD, UART_RX_PIN, UART_TX_PIN);
  return true;
}

void processUARTCommands() {
  int len = uart_read_bytes(UART_PORT, uart_rx_buffer, UART_BUF_SIZE, 0);
  if (len <= 0) return;
  
  // Parse commands from buffer
  int pos = 0;
  while (pos + sizeof(CommandProtocol::CommandHeader) <= (size_t)len) {
    // Look for sync bytes
    if (uart_rx_buffer[pos] != CommandProtocol::SYNC_1) {
      pos++;
      continue;
    }
    if (pos + 1 >= len || uart_rx_buffer[pos + 1] != CommandProtocol::SYNC_2) {
      pos++;
      continue;
    }
    if (pos + 2 >= len || uart_rx_buffer[pos + 2] != CommandProtocol::SYNC_3) {
      pos++;
      continue;
    }
    
    // Found sync, parse header
    auto* hdr = reinterpret_cast<CommandProtocol::CommandHeader*>(&uart_rx_buffer[pos]);
    
    if (pos + sizeof(CommandProtocol::CommandHeader) + hdr->payload_len > (size_t)len) {
      break;  // Incomplete packet
    }
    
    uint8_t* payload = &uart_rx_buffer[pos + sizeof(CommandProtocol::CommandHeader)];
    
    // Process command
    switch (static_cast<CommandProtocol::CmdType>(hdr->cmd_type)) {
      case CommandProtocol::CmdType::PING:
        ESP_LOGI(TAG, "PING received");
        // TODO: Send PONG response
        break;
        
      case CommandProtocol::CmdType::CLEAR_ALL:
        clearFramebuffer();
        oled.clearBuffer();
        ESP_LOGI(TAG, "CLEAR_ALL");
        break;
        
      case CommandProtocol::CmdType::SHOW:
        flushToHUB75();
        oled.updateDisplay();
        ESP_LOGI(TAG, "SHOW");
        break;
        
      case CommandProtocol::CmdType::SET_BRIGHTNESS:
        if (hdr->payload_len >= 1) {
          hub75.setBrightness(payload[0]);
          ESP_LOGI(TAG, "SET_BRIGHTNESS: %d", payload[0]);
        }
        break;
        
      case CommandProtocol::CmdType::HUB75_CLEAR:
        clearFramebuffer();
        break;
        
      case CommandProtocol::CmdType::HUB75_PIXEL:
        if (hdr->payload_len >= 5) {
          int x = payload[0];
          int y = payload[1];
          RGB color(payload[2], payload[3], payload[4]);
          setPixel(x, y, color);
        }
        break;
        
      case CommandProtocol::CmdType::HUB75_RAW:
        if (hdr->payload_len >= HUB75_WIDTH * HUB75_HEIGHT * 3) {
          memcpy(framebuffer, payload, HUB75_WIDTH * HUB75_HEIGHT * 3);
          ESP_LOGI(TAG, "HUB75_RAW frame received");
        }
        break;
        
      case CommandProtocol::CmdType::OLED_CLEAR:
        oled.clearBuffer();
        break;
        
      case CommandProtocol::CmdType::OLED_PIXEL:
        if (hdr->payload_len >= 3) {
          oled.setPixel(payload[0], payload[1], payload[2] > 0);
        }
        break;
        
      case CommandProtocol::CmdType::OLED_LINE:
        if (hdr->payload_len >= 5) {
          oled.drawLine(payload[0], payload[1], payload[2], payload[3], payload[4] > 0);
        }
        break;
        
      case CommandProtocol::CmdType::OLED_RECT:
        if (hdr->payload_len >= 6) {
          oled.drawRect(payload[0], payload[1], payload[2], payload[3], 
                       payload[4] > 0, payload[5] > 0);
        }
        break;
        
      case CommandProtocol::CmdType::OLED_CIRCLE:
        if (hdr->payload_len >= 5) {
          oled.drawCircle(payload[0], payload[1], payload[2], 
                         payload[3] > 0, payload[4] > 0);
        }
        break;
        
      case CommandProtocol::CmdType::OLED_TEXT:
        if (hdr->payload_len >= 3) {
          int x = payload[0];
          int y = payload[1];
          const char* text = reinterpret_cast<const char*>(&payload[2]);
          oled.drawString(x, y, text, true);
        }
        break;
        
      case CommandProtocol::CmdType::OLED_RAW:
        if (hdr->payload_len >= 2048) {
          memcpy(oled.getBuffer(), payload, 2048);
          ESP_LOGI(TAG, "OLED_RAW buffer received");
        }
        break;
        
      case CommandProtocol::CmdType::ANIM_ENABLE:
        animation_enabled = true;
        ESP_LOGI(TAG, "Animation ENABLED");
        break;
        
      case CommandProtocol::CmdType::ANIM_DISABLE:
        animation_enabled = false;
        ESP_LOGI(TAG, "Animation DISABLED");
        break;
        
      case CommandProtocol::CmdType::ANIM_SET_MODE:
        if (hdr->payload_len >= 1) {
          animation_mode = payload[0];
          ESP_LOGI(TAG, "Animation mode: %d", animation_mode);
        }
        break;
        
      default:
        ESP_LOGW(TAG, "Unknown command: 0x%02X", hdr->cmd_type);
        break;
    }
    
    pos += sizeof(CommandProtocol::CommandHeader) + hdr->payload_len;
  }
}

// ============================================================
// Initialization
// ============================================================
bool initI2C() {
  ESP_LOGI(TAG, "Initializing I2C for OLED...");
  HalResult result = ESP32S3_I2C_HAL::Initialize(0, 2, 1, 400000, 1000);
  if (result != HalResult::Success) {
    ESP_LOGE(TAG, "I2C init failed!");
    return false;
  }
  ESP_LOGI(TAG, "I2C OK (SDA=2, SCL=1, 400kHz)");
  return true;
}

void printMemory() {
  ESP_LOGI(TAG, "=== Memory ===");
  ESP_LOGI(TAG, "Free heap: %lu", (unsigned long)esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free DMA: %lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA));
}

// ============================================================
// Main Entry Point
// ============================================================
extern "C" void app_main(void) {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║   GPU Command Renderer - Dual Panel Demo     ║");
  ESP_LOGI(TAG, "║   HUB75: 128x32 (2x64x32) | OLED: 128x128   ║");
  ESP_LOGI(TAG, "╚══════════════════════════════════════════════╝");
  ESP_LOGI(TAG, "");
  
  vTaskDelay(pdMS_TO_TICKS(2000));
  printMemory();
  
  // ============ Initialize HUB75 ============
  ESP_LOGI(TAG, "--- Initializing HUB75 ---");
  
  HUB75Config config = HUB75Config::getDefault();
  config.colour_depth = 5;
  config.colour_buffer_count = 5;
  config.enable_double_buffering = true;
  config.enable_gamma_correction = true;
  config.gamma_value = 2.2f;
  
  bool hub75_ok = hub75.begin(true, config);
  if (!hub75_ok) {
    ESP_LOGE(TAG, "HUB75 FAILED!");
  } else {
    ESP_LOGI(TAG, "HUB75 OK: %dx%d", hub75.getWidth(), hub75.getHeight());
    hub75.setBrightness(200);
    hub75.clear();
    hub75.show();
  }
  
  // ============ Initialize I2C + OLED ============
  ESP_LOGI(TAG, "--- Initializing OLED ---");
  bool oled_ok = false;
  
  if (initI2C()) {
    OLEDConfig oled_cfg;
    oled_cfg.contrast = 0xFF;
    
    oled_ok = oled.initialize(oled_cfg);
    if (!oled_ok) {
      ESP_LOGE(TAG, "OLED FAILED!");
    } else {
      ESP_LOGI(TAG, "OLED OK: 128x128 mono");
      oled.clearBuffer();
      oled.drawString(20, 56, "GPU READY", true);
      oled.updateDisplay();
    }
  }
  
  // ============ Initialize UART ============
  ESP_LOGI(TAG, "--- Initializing UART ---");
  bool uart_ok = initUART();
  
  // ============ Print Summary ============
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== Initialization Summary ===");
  ESP_LOGI(TAG, "  HUB75: %s", hub75_ok ? "READY" : "FAILED");
  ESP_LOGI(TAG, "  OLED:  %s", oled_ok ? "READY" : "FAILED");
  ESP_LOGI(TAG, "  UART:  %s", uart_ok ? "READY" : "FAILED");
  ESP_LOGI(TAG, "");
  
  if (!hub75_ok) {
    ESP_LOGE(TAG, "Cannot continue without HUB75!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  printMemory();
  
  ESP_LOGI(TAG, "Starting render loop...");
  ESP_LOGI(TAG, "Shape vertices: 16 points (centered on each panel)");
  ESP_LOGI(TAG, "Panel 0 center: (32, 16) | Panel 1 center: (96, 16)");
  ESP_LOGI(TAG, "");
  
  uint32_t last_oled_update = 0;
  uint32_t frame_time_start;
  
  // ============ Main Render Loop ============
  while (1) {
    frame_time_start = esp_timer_get_time();
    
    // Process any incoming CPU commands
    processUARTCommands();
    
    // Render the dual panel shapes
    renderDualPanelShapes();
    
    // Flush to HUB75
    flushToHUB75();
    
    // Update frame counter
    frame_count++;
    
    // Calculate FPS
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_fps_time >= 1000) {
      fps = frame_count * 1000 / (now - last_fps_time);
      frame_count = 0;
      last_fps_time = now;
      
      ESP_LOGI(TAG, "FPS: %lu | Anim: %s | Mode: %d", 
               (unsigned long)fps,
               animation_enabled ? "ON" : "OFF",
               animation_mode);
    }
    
    // Update OLED at lower rate (10fps)
    if (oled_ok && now - last_oled_update >= 100) {
      renderOLEDStatus();
      last_oled_update = now;
    }
    
    // Calculate remaining time for 60fps target
    uint32_t elapsed_us = esp_timer_get_time() - frame_time_start;
    uint32_t target_frame_us = 1000000 / 60;  // 16.67ms for 60fps
    
    if (elapsed_us < target_frame_us) {
      uint32_t delay_us = target_frame_us - elapsed_us;
      if (delay_us > 1000) {
        vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
      }
    }
  }
}
