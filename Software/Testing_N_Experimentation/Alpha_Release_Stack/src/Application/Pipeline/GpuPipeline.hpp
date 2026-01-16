/*****************************************************************
 * File:      GpuPipeline.hpp
 * Category:  Application/Pipeline
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU rendering pipeline that runs on Core 1.
 *    Handles animation compositing and converts frames to
 *    GPU-readable commands sent via UART.
 * 
 *    Pipeline stages:
 *    1. Read current animation state (from Core 0)
 *    2. Evaluate shaders and animations
 *    3. Composite layers into framebuffer
 *    4. Convert framebuffer to GPU commands
 *    5. Send commands via UART to GPU
 * 
 *    Targets 60 FPS with adaptive frame skipping.
 *****************************************************************/

#ifndef ARCOS_APPLICATION_GPU_PIPELINE_HPP_
#define ARCOS_APPLICATION_GPU_PIPELINE_HPP_

#include <stdint.h>
#include <cstring>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"

namespace Application {

// ============================================================
// GPU Protocol Constants
// ============================================================

constexpr uint8_t GPU_SYNC0 = 0xAA;
constexpr uint8_t GPU_SYNC1 = 0x55;

// Command types matching GPU_Programmable protocol
enum class GpuCmd : uint8_t {
  NOP           = 0x00,
  DRAW_PIXEL    = 0x40,
  DRAW_LINE     = 0x41,
  DRAW_RECT     = 0x42,
  DRAW_FILL     = 0x43,
  DRAW_CIRCLE   = 0x44,
  DRAW_POLY     = 0x45,
  BLIT_SPRITE   = 0x46,
  CLEAR         = 0x47,
  DRAW_LINE_F   = 0x48,
  DRAW_CIRCLE_F = 0x49,
  DRAW_RECT_F   = 0x4A,
  SET_TARGET    = 0x50,
  PRESENT       = 0x51,
  RESET         = 0xFF,
};

// ============================================================
// Display Constants
// ============================================================

constexpr int HUB75_WIDTH = 128;
constexpr int HUB75_HEIGHT = 32;
constexpr int FRAMEBUFFER_SIZE = HUB75_WIDTH * HUB75_HEIGHT * 3;  // RGB888

// ============================================================
// Pixel/Color Types
// ============================================================

struct RGB {
  uint8_t r, g, b;
  
  RGB() : r(0), g(0), b(0) {}
  RGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
  
  static RGB fromHSV(float h, float s, float v) {
    // h: 0-360, s: 0-1, v: 0-1
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r1, g1, b1;
    if (h < 60) { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }
    
    return RGB(
      (uint8_t)((r1 + m) * 255.0f),
      (uint8_t)((g1 + m) * 255.0f),
      (uint8_t)((b1 + m) * 255.0f)
    );
  }
  
  RGB blend(const RGB& other, float t) const {
    return RGB(
      (uint8_t)(r + (other.r - r) * t),
      (uint8_t)(g + (other.g - g) * t),
      (uint8_t)(b + (other.b - b) * t)
    );
  }
  
  RGB scale(float s) const {
    return RGB(
      (uint8_t)(r * s),
      (uint8_t)(g * s),
      (uint8_t)(b * s)
    );
  }
};

// ============================================================
// Eye Shape Definition
// ============================================================

struct Point2D {
  int16_t x, y;
  Point2D() : x(0), y(0) {}
  Point2D(int16_t x_, int16_t y_) : x(x_), y(y_) {}
};

struct EyeShape {
  Point2D points[32];
  uint8_t pointCount;
  int16_t offsetX;
  int16_t offsetY;
  float scale;
  
  EyeShape() : pointCount(0), offsetX(0), offsetY(0), scale(1.0f) {}
};

// Default eye shape polygon
constexpr int16_t DEFAULT_EYE_POINTS[][2] = {
  {6, 8}, {14, 8}, {20, 11}, {26, 17}, {27, 19}, {28, 22},
  {23, 22}, {21, 19}, {19, 17}, {17, 17}, {16, 19}, {18, 22},
  {7, 22}, {4, 20}, {2, 17}, {2, 12}
};
constexpr int DEFAULT_EYE_POINT_COUNT = 16;

// ============================================================
// GPU Pipeline Class
// ============================================================

class GpuPipeline {
public:
  static constexpr const char* TAG = "GpuPipe";
  
  // Configuration
  struct Config {
    uart_port_t uartPort;
    int txPin;
    int rxPin;
    int baudRate;
    uint32_t targetFps;
    bool mirrorMode;
    
    // Default constructor with default values
    Config()
      : uartPort(UART_NUM_1)
      , txPin(12)
      , rxPin(11)
      , baudRate(10000000)  // 10 Mbps
      , targetFps(60)
      , mirrorMode(true)
    {}
  };
  
  GpuPipeline()
    : initialized_(false)
    , config_()
    , time_(0.0f)
    , frameCount_(0)
  {
    // Clear framebuffer
    memset(framebuffer_, 0, sizeof(framebuffer_));
    
    // Initialize default eye shape
    initDefaultEyeShape();
  }
  
  /** Initialize the GPU pipeline
   * @param config Pipeline configuration
   * @return true if successful
   */
  bool init(const Config& config = Config()) {
    if (initialized_) return true;
    
    config_ = config;
    
    ESP_LOGI(TAG, "Initializing GPU pipeline");
    ESP_LOGI(TAG, "  UART: %d, TX:%d, RX:%d, Baud:%d",
             config_.uartPort, config_.txPin, config_.rxPin, config_.baudRate);
    
    // Configure UART for GPU communication
    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate = config_.baudRate;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.rx_flow_ctrl_thresh = 0;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;
    
    esp_err_t err = uart_param_config(config_.uartPort, &uart_cfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "UART config failed: %d", err);
      return false;
    }
    
    err = uart_set_pin(config_.uartPort, config_.txPin, config_.rxPin, -1, -1);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "UART pin config failed: %d", err);
      return false;
    }
    
    // Check if UART driver is already installed (e.g., by GpuCommands on Core 0)
    if (uart_is_driver_installed(config_.uartPort)) {
      ESP_LOGI(TAG, "UART driver already installed - reusing existing driver");
    } else {
      err = uart_driver_install(config_.uartPort, 1024, 2048, 0, nullptr, 0);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %d", err);
        return false;
      }
    }
    
    initialized_ = true;
    
    // Send reset to GPU
    sendReset();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "GPU pipeline initialized");
    return true;
  }
  
  /** Process one frame
   * @param lookX Eye X position (-1 to 1)
   * @param lookY Eye Y position (-1 to 1)
   * @param blinkProgress Blink progress (0 = open, 1 = closed)
   * @param shaderType Shader type (0=solid, 1=rainbow, etc.)
   * @param shaderSpeed Shader animation speed
   * @param brightness Overall brightness (0-100)
   * @param primaryColor Primary color
   * @param deltaTime Time since last frame in seconds
   */
  void processFrame(float lookX, float lookY, float blinkProgress,
                    uint8_t shaderType, float shaderSpeed, uint8_t brightness,
                    RGB primaryColor, RGB secondaryColor,
                    float deltaTime) {
    if (!initialized_) return;
    
    time_ += deltaTime;
    frameCount_++;
    
    // Clear framebuffer
    clearFramebuffer(RGB(0, 0, 0));
    
    // Calculate eye offset from look position
    int eyeOffsetX = (int)(lookX * 8.0f);
    int eyeOffsetY = (int)(lookY * 4.0f);
    
    // Calculate blink (squish eye vertically)
    float eyeScaleY = 1.0f - blinkProgress * 0.9f;
    
    // Render left eye (first half of display)
    renderEye(0, 0, 64, 32, eyeOffsetX, eyeOffsetY, eyeScaleY,
              shaderType, shaderSpeed, brightness, primaryColor, secondaryColor);
    
    // Render right eye (second half, mirrored if enabled)
    if (config_.mirrorMode) {
      renderEye(64, 0, 64, 32, -eyeOffsetX, eyeOffsetY, eyeScaleY,
                shaderType, shaderSpeed, brightness, primaryColor, secondaryColor);
    } else {
      renderEye(64, 0, 64, 32, eyeOffsetX, eyeOffsetY, eyeScaleY,
                shaderType, shaderSpeed, brightness, primaryColor, secondaryColor);
    }
    
    // Send framebuffer to GPU
    sendFramebuffer();
  }
  
  /** Get frame count */
  uint32_t getFrameCount() const { return frameCount_; }
  
  /** Get time since init */
  float getTime() const { return time_; }
  
private:
  bool initialized_;
  Config config_;
  float time_;
  uint32_t frameCount_;
  
  // Framebuffer (RGB888)
  uint8_t framebuffer_[FRAMEBUFFER_SIZE];
  
  // Eye shapes
  EyeShape defaultEyeShape_;
  
  // ========================================================
  // Eye Shape Initialization
  // ========================================================
  
  void initDefaultEyeShape() {
    defaultEyeShape_.pointCount = DEFAULT_EYE_POINT_COUNT;
    for (int i = 0; i < DEFAULT_EYE_POINT_COUNT; i++) {
      defaultEyeShape_.points[i] = Point2D(DEFAULT_EYE_POINTS[i][0], DEFAULT_EYE_POINTS[i][1]);
    }
    defaultEyeShape_.offsetX = 0;
    defaultEyeShape_.offsetY = 0;
    defaultEyeShape_.scale = 1.0f;
  }
  
  // ========================================================
  // Framebuffer Operations
  // ========================================================
  
  void clearFramebuffer(RGB color) {
    for (int i = 0; i < HUB75_WIDTH * HUB75_HEIGHT; i++) {
      framebuffer_[i * 3 + 0] = color.r;
      framebuffer_[i * 3 + 1] = color.g;
      framebuffer_[i * 3 + 2] = color.b;
    }
  }
  
  void setPixel(int x, int y, RGB color) {
    if (x < 0 || x >= HUB75_WIDTH || y < 0 || y >= HUB75_HEIGHT) return;
    int idx = (y * HUB75_WIDTH + x) * 3;
    framebuffer_[idx + 0] = color.r;
    framebuffer_[idx + 1] = color.g;
    framebuffer_[idx + 2] = color.b;
  }
  
  RGB getShaderColor(int x, int y, uint8_t shaderType, float speed,
                     RGB primary, RGB secondary) {
    switch (shaderType) {
      case 0:  // Solid
        return primary;
        
      case 1: {  // Rainbow horizontal
        float hue = fmodf((float)x / HUB75_WIDTH * 360.0f + time_ * speed * 100.0f, 360.0f);
        return RGB::fromHSV(hue, 1.0f, 1.0f);
      }
      
      case 2: {  // Gradient vertical
        float t = (float)y / HUB75_HEIGHT;
        return primary.blend(secondary, t);
      }
      
      case 3: {  // Pulse
        float pulse = (sinf(time_ * speed * 3.14159f * 2.0f) + 1.0f) * 0.5f;
        return primary.scale(0.3f + pulse * 0.7f);
      }
      
      case 4: {  // Plasma
        float px = (float)x / HUB75_WIDTH * 3.14159f * 2.0f;
        float py = (float)y / HUB75_HEIGHT * 3.14159f * 2.0f;
        float t = time_ * speed;
        float v = sinf(px + t) + sinf(py + t) + sinf(px + py + t);
        v = (v + 3.0f) / 6.0f;  // Normalize to 0-1
        float hue = fmodf(v * 360.0f + t * 50.0f, 360.0f);
        return RGB::fromHSV(hue, 1.0f, 1.0f);
      }
      
      default:
        return primary;
    }
  }
  
  // ========================================================
  // Eye Rendering
  // ========================================================
  
  bool pointInPolygon(int px, int py, const Point2D* points, int count,
                      int offsetX, int offsetY, float scaleY) {
    // Ray casting algorithm
    bool inside = false;
    int j = count - 1;
    
    for (int i = 0; i < count; i++) {
      // Apply offset and vertical scale for blink
      int xi = points[i].x + offsetX;
      int yi = (int)((points[i].y - 16) * scaleY) + 16 + offsetY;  // Scale from center
      int xj = points[j].x + offsetX;
      int yj = (int)((points[j].y - 16) * scaleY) + 16 + offsetY;
      
      if (((yi > py) != (yj > py)) &&
          (px < (xj - xi) * (py - yi) / (yj - yi + 0.001f) + xi)) {
        inside = !inside;
      }
      j = i;
    }
    
    return inside;
  }
  
  void renderEye(int startX, int startY, int width, int height,
                 int eyeOffsetX, int eyeOffsetY, float eyeScaleY,
                 uint8_t shaderType, float shaderSpeed, uint8_t brightness,
                 RGB primary, RGB secondary) {
    float brightScale = brightness / 100.0f;
    
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        // Check if pixel is inside eye polygon
        // Map x to eye space (0-30 for each eye)
        int eyeX = x * 30 / width;
        int eyeY = y * 30 / height;
        
        if (pointInPolygon(eyeX, eyeY, defaultEyeShape_.points,
                          defaultEyeShape_.pointCount,
                          eyeOffsetX, eyeOffsetY, eyeScaleY)) {
          // Get shader color for this pixel
          RGB color = getShaderColor(startX + x, startY + y,
                                     shaderType, shaderSpeed, primary, secondary);
          
          // Apply brightness
          color = color.scale(brightScale);
          
          // Set pixel
          setPixel(startX + x, startY + y, color);
        }
      }
    }
  }
  
  // ========================================================
  // GPU Communication
  // ========================================================
  
  void sendCommand(GpuCmd cmd, const uint8_t* payload, uint16_t len) {
    uint8_t header[5] = {
      GPU_SYNC0, GPU_SYNC1,
      static_cast<uint8_t>(cmd),
      static_cast<uint8_t>(len & 0xFF),
      static_cast<uint8_t>((len >> 8) & 0xFF)
    };
    
    uart_write_bytes(config_.uartPort, header, 5);
    if (len > 0 && payload) {
      uart_write_bytes(config_.uartPort, payload, len);
    }
    uart_wait_tx_done(config_.uartPort, pdMS_TO_TICKS(50));
  }
  
  void sendReset() {
    sendCommand(GpuCmd::RESET, nullptr, 0);
  }
  
  void sendClear(RGB color) {
    uint8_t payload[3] = { color.r, color.g, color.b };
    sendCommand(GpuCmd::CLEAR, payload, 3);
  }
  
  void sendPixel(int16_t x, int16_t y, RGB color) {
    uint8_t payload[7];
    payload[0] = x & 0xFF;
    payload[1] = (x >> 8) & 0xFF;
    payload[2] = y & 0xFF;
    payload[3] = (y >> 8) & 0xFF;
    payload[4] = color.r;
    payload[5] = color.g;
    payload[6] = color.b;
    sendCommand(GpuCmd::DRAW_PIXEL, payload, 7);
  }
  
  void sendPresent() {
    sendCommand(GpuCmd::PRESENT, nullptr, 0);
  }
  
  void sendFramebuffer() {
    // Send clear first
    sendClear(RGB(0, 0, 0));
    
    // Send non-black pixels
    for (int y = 0; y < HUB75_HEIGHT; y++) {
      for (int x = 0; x < HUB75_WIDTH; x++) {
        int idx = (y * HUB75_WIDTH + x) * 3;
        uint8_t r = framebuffer_[idx + 0];
        uint8_t g = framebuffer_[idx + 1];
        uint8_t b = framebuffer_[idx + 2];
        
        // Skip black pixels
        if (r > 0 || g > 0 || b > 0) {
          sendPixel(x, y, RGB(r, g, b));
        }
      }
    }
    
    // Present frame
    sendPresent();
  }
};

} // namespace Application

#endif // ARCOS_APPLICATION_GPU_PIPELINE_HPP_
