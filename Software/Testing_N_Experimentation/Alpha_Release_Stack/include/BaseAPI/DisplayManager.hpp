/*****************************************************************
 * File:      DisplayManager.hpp
 * Category:  include/BaseAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Hardware-agnostic display management abstraction.
 *    Provides unified interface for:
 *    - HUB75 LED matrix panels
 *    - OLED displays
 *    - Frame buffer management
 *    - Drawing primitives
 * 
 * Design:
 *    The display manager doesn't care about underlying hardware.
 *    It provides a frame buffer interface that can be:
 *    - Rendered locally (GPU side)
 *    - Sent over UART (CPU side, for GPU to render)
 * 
 * Layer:
 *    HAL Layer -> [Base System API - Display] -> Application
 *****************************************************************/

#ifndef ARCOS_INCLUDE_BASEAPI_DISPLAY_MANAGER_HPP_
#define ARCOS_INCLUDE_BASEAPI_DISPLAY_MANAGER_HPP_

#include "BaseTypes.hpp"
#include <cstring>

namespace arcos::base{

// ============================================================
// Display Configuration
// ============================================================

/** Display type enumeration */
enum class DisplayType : uint8_t{
  NONE = 0,
  HUB75_MATRIX,    // HUB75 LED matrix panel
  OLED_SH1107,     // SH1107 OLED (128x128)
  OLED_SSD1306,    // SSD1306 OLED (128x64)
  LCD_ILI9341,     // ILI9341 LCD
  VIRTUAL          // Virtual display (for testing)
};

/** Color format enumeration */
enum class ColorFormat : uint8_t{
  MONO = 0,        // 1-bit monochrome
  RGB565 = 1,      // 16-bit (5-6-5)
  RGB888 = 2,      // 24-bit (8-8-8)
  RGBA8888 = 3     // 32-bit with alpha
};

/** Display configuration */
struct DisplayConfig{
  DisplayType type;
  uint16_t width;
  uint16_t height;
  ColorFormat format;
  uint8_t brightness;    // 0-255
  bool double_buffer;    // Use double buffering
  uint8_t refresh_rate;  // Target refresh rate (Hz)
  
  DisplayConfig()
    : type(DisplayType::NONE)
    , width(0)
    , height(0)
    , format(ColorFormat::RGB565)
    , brightness(128)
    , double_buffer(true)
    , refresh_rate(60)
  {}
};

// ============================================================
// Frame Buffer
// ============================================================

/**
 * FrameBuffer - Generic frame buffer with drawing primitives
 * 
 * Supports RGB565 format (16-bit per pixel) which is efficient
 * for both HUB75 and most displays.
 */
class FrameBuffer{
public:
  FrameBuffer()
    : buffer_(nullptr)
    , width_(0)
    , height_(0)
    , size_(0)
    , owned_(false)
  {}
  
  ~FrameBuffer(){
    if(owned_ && buffer_){
      delete[] buffer_;
    }
  }
  
  /** Allocate frame buffer */
  bool allocate(uint16_t width, uint16_t height){
    if(owned_ && buffer_){
      delete[] buffer_;
    }
    
    width_ = width;
    height_ = height;
    size_ = width * height;
    buffer_ = new uint16_t[size_];
    owned_ = true;
    
    if(!buffer_){
      size_ = 0;
      return false;
    }
    
    clear();
    return true;
  }
  
  /** Use external buffer */
  void setBuffer(uint16_t* buffer, uint16_t width, uint16_t height){
    if(owned_ && buffer_){
      delete[] buffer_;
    }
    
    buffer_ = buffer;
    width_ = width;
    height_ = height;
    size_ = width * height;
    owned_ = false;
  }
  
  /** Get raw buffer pointer */
  uint16_t* data(){ return buffer_; }
  const uint16_t* data() const{ return buffer_; }
  
  /** Get dimensions */
  uint16_t width() const{ return width_; }
  uint16_t height() const{ return height_; }
  size_t size() const{ return size_; }
  size_t sizeBytes() const{ return size_ * 2; }
  
  /** Clear buffer */
  void clear(uint16_t color = 0){
    if(buffer_){
      for(size_t i = 0; i < size_; i++){
        buffer_[i] = color;
      }
    }
  }
  
  /** Set pixel (bounds checked) */
  void setPixel(int16_t x, int16_t y, uint16_t color){
    if(x >= 0 && x < width_ && y >= 0 && y < height_){
      buffer_[y * width_ + x] = color;
    }
  }
  
  /** Set pixel with Color */
  void setPixel(int16_t x, int16_t y, const Color& color){
    setPixel(x, y, color.toRGB565());
  }
  
  /** Get pixel */
  uint16_t getPixel(int16_t x, int16_t y) const{
    if(x >= 0 && x < width_ && y >= 0 && y < height_){
      return buffer_[y * width_ + x];
    }
    return 0;
  }
  
  /** Draw horizontal line */
  void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color){
    if(y < 0 || y >= height_) return;
    if(x < 0){ w += x; x = 0; }
    if(x + w > width_) w = width_ - x;
    if(w <= 0) return;
    
    uint16_t* ptr = buffer_ + y * width_ + x;
    while(w--) *ptr++ = color;
  }
  
  /** Draw vertical line */
  void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color){
    if(x < 0 || x >= width_) return;
    if(y < 0){ h += y; y = 0; }
    if(y + h > height_) h = height_ - y;
    if(h <= 0) return;
    
    uint16_t* ptr = buffer_ + y * width_ + x;
    while(h--){
      *ptr = color;
      ptr += width_;
    }
  }
  
  /** Draw line (Bresenham) */
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color){
    int16_t dx = abs(x1 - x0);
    int16_t dy = -abs(y1 - y0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    
    while(true){
      setPixel(x0, y0, color);
      if(x0 == x1 && y0 == y1) break;
      int16_t e2 = 2 * err;
      if(e2 >= dy){ err += dy; x0 += sx; }
      if(e2 <= dx){ err += dx; y0 += sy; }
    }
  }
  
  /** Draw rectangle outline */
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
    drawHLine(x, y, w, color);
    drawHLine(x, y + h - 1, w, color);
    drawVLine(x, y, h, color);
    drawVLine(x + w - 1, y, h, color);
  }
  
  /** Draw filled rectangle */
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
    for(int16_t i = 0; i < h; i++){
      drawHLine(x, y + i, w, color);
    }
  }
  
  /** Draw circle outline (Midpoint) */
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color){
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    setPixel(x0, y0 + r, color);
    setPixel(x0, y0 - r, color);
    setPixel(x0 + r, y0, color);
    setPixel(x0 - r, y0, color);
    
    while(x < y){
      if(f >= 0){
        y--;
        ddF_y += 2;
        f += ddF_y;
      }
      x++;
      ddF_x += 2;
      f += ddF_x;
      
      setPixel(x0 + x, y0 + y, color);
      setPixel(x0 - x, y0 + y, color);
      setPixel(x0 + x, y0 - y, color);
      setPixel(x0 - x, y0 - y, color);
      setPixel(x0 + y, y0 + x, color);
      setPixel(x0 - y, y0 + x, color);
      setPixel(x0 + y, y0 - x, color);
      setPixel(x0 - y, y0 - x, color);
    }
  }
  
  /** Draw filled circle */
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color){
    drawVLine(x0, y0 - r, 2 * r + 1, color);
    
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    while(x < y){
      if(f >= 0){
        y--;
        ddF_y += 2;
        f += ddF_y;
      }
      x++;
      ddF_x += 2;
      f += ddF_x;
      
      drawVLine(x0 + x, y0 - y, 2 * y + 1, color);
      drawVLine(x0 - x, y0 - y, 2 * y + 1, color);
      drawVLine(x0 + y, y0 - x, 2 * x + 1, color);
      drawVLine(x0 - y, y0 - x, 2 * x + 1, color);
    }
  }
  
  /** Copy from another framebuffer (with offset) */
  void blit(const FrameBuffer& src, int16_t dx, int16_t dy){
    for(int16_t y = 0; y < src.height_; y++){
      for(int16_t x = 0; x < src.width_; x++){
        setPixel(dx + x, dy + y, src.getPixel(x, y));
      }
    }
  }
  
  /** Copy to raw buffer */
  void copyTo(uint16_t* dest) const{
    if(buffer_ && dest){
      memcpy(dest, buffer_, size_ * 2);
    }
  }
  
  /** Copy from raw buffer */
  void copyFrom(const uint16_t* src){
    if(buffer_ && src){
      memcpy(buffer_, src, size_ * 2);
    }
  }

private:
  uint16_t* buffer_;
  uint16_t width_;
  uint16_t height_;
  size_t size_;
  bool owned_;
};

// ============================================================
// Display Interface
// ============================================================

/**
 * IDisplay - Abstract display interface
 * 
 * Implementations wrap hardware-specific display drivers
 * while providing a common interface for the middleware.
 */
class IDisplay{
public:
  virtual ~IDisplay() = default;
  
  /** Initialize display */
  virtual Result init(const DisplayConfig& config) = 0;
  
  /** Deinitialize display */
  virtual void deinit() = 0;
  
  /** Get display configuration */
  virtual const DisplayConfig& getConfig() const = 0;
  
  /** Get display dimensions */
  virtual uint16_t getWidth() const = 0;
  virtual uint16_t getHeight() const = 0;
  
  /** Set brightness (0-255) */
  virtual void setBrightness(uint8_t brightness) = 0;
  
  /** Get current brightness */
  virtual uint8_t getBrightness() const = 0;
  
  /** Clear display */
  virtual void clear() = 0;
  
  /** Display a frame buffer */
  virtual void display(const FrameBuffer& frame) = 0;
  
  /** Display raw RGB565 data */
  virtual void display(const uint16_t* data, uint16_t width, uint16_t height) = 0;
  
  /** Swap buffers (if double-buffered) */
  virtual void swap() = 0;
  
  /** Check if display is ready */
  virtual bool isReady() const = 0;
  
  /** Get frame rate (FPS) */
  virtual float getFPS() const = 0;
};

// ============================================================
// Display Manager
// ============================================================

/**
 * DisplayManager - Manages multiple displays
 * 
 * Provides a unified interface for managing displays on either
 * CPU or GPU side. On CPU, frames are sent over UART. On GPU,
 * frames are rendered directly to hardware.
 */
class DisplayManager{
public:
  static constexpr uint8_t MAX_DISPLAYS = 4;
  
  DisplayManager()
    : display_count_(0)
    , active_display_(0)
  {
    for(int i = 0; i < MAX_DISPLAYS; i++){
      displays_[i] = nullptr;
    }
  }
  
  /** Register a display */
  Result addDisplay(IDisplay* display, uint8_t index){
    if(index >= MAX_DISPLAYS) return Result::INVALID_PARAM;
    if(!display) return Result::INVALID_PARAM;
    
    displays_[index] = display;
    if(index >= display_count_) display_count_ = index + 1;
    return Result::OK;
  }
  
  /** Get display by index */
  IDisplay* getDisplay(uint8_t index){
    if(index >= MAX_DISPLAYS) return nullptr;
    return displays_[index];
  }
  
  /** Get display count */
  uint8_t getDisplayCount() const{ return display_count_; }
  
  /** Set active display (for unified operations) */
  void setActiveDisplay(uint8_t index){
    if(index < MAX_DISPLAYS){
      active_display_ = index;
    }
  }
  
  /** Display frame to active display */
  void display(const FrameBuffer& frame){
    if(displays_[active_display_]){
      displays_[active_display_]->display(frame);
    }
  }
  
  /** Display frame to specific display */
  void displayTo(uint8_t index, const FrameBuffer& frame){
    if(index < MAX_DISPLAYS && displays_[index]){
      displays_[index]->display(frame);
    }
  }
  
  /** Clear all displays */
  void clearAll(){
    for(int i = 0; i < MAX_DISPLAYS; i++){
      if(displays_[i]){
        displays_[i]->clear();
      }
    }
  }
  
  /** Set brightness for all displays */
  void setBrightnessAll(uint8_t brightness){
    for(int i = 0; i < MAX_DISPLAYS; i++){
      if(displays_[i]){
        displays_[i]->setBrightness(brightness);
      }
    }
  }

private:
  IDisplay* displays_[MAX_DISPLAYS];
  uint8_t display_count_;
  uint8_t active_display_;
};

// ============================================================
// Convenience Color Definitions
// ============================================================

namespace colors{
  constexpr uint16_t BLACK   = 0x0000;
  constexpr uint16_t WHITE   = 0xFFFF;
  constexpr uint16_t RED     = 0xF800;
  constexpr uint16_t GREEN   = 0x07E0;
  constexpr uint16_t BLUE    = 0x001F;
  constexpr uint16_t YELLOW  = 0xFFE0;
  constexpr uint16_t CYAN    = 0x07FF;
  constexpr uint16_t MAGENTA = 0xF81F;
  constexpr uint16_t ORANGE  = 0xFD20;
  constexpr uint16_t PURPLE  = 0x8010;
  
  /** Convert RGB888 to RGB565 */
  inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b){
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  }
  
  /** Create color from HSV (h: 0-255, s: 0-255, v: 0-255) */
  inline uint16_t hsv(uint8_t h, uint8_t s, uint8_t v){
    Color c = Color::fromHSV(h, s, v);
    return c.toRGB565();
  }
}

} // namespace arcos::base

#endif // ARCOS_INCLUDE_BASEAPI_DISPLAY_MANAGER_HPP_
