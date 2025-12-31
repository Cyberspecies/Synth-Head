/*****************************************************************
 * File:      OLEDDisplayManager.hpp
 * Category:  Manager/Display
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Manages OLED SH1107 display with animation function caching.
 *    Provides line drawing (normal and antialiased), text rendering,
 *    initialization, and animation management.
 * 
 * Features:
 *    - Initialize and configure OLED display
 *    - Draw lines with antialiasing using brightness levels
 *    - Store and execute animation functions (no pixel caching)
 *    - Text rendering and basic shapes
 * 
 * Hardware:
 *    - OLED SH1107 128x128 display (I2C)
 *****************************************************************/

#ifndef OLED_DISPLAY_MANAGER_HPP
#define OLED_DISPLAY_MANAGER_HPP

#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"
#include "abstraction/platforms/esp32/wroom32s3/module/hal_interface_i2c_module.hpp"
#include <functional>
#include <vector>
#include <cmath>

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

namespace arcos::manager{

/**
 * @brief Animation function type for OLED display
 * Parameters: time_ms (animation time in milliseconds)
 */
using OLEDAnimationFunc = std::function<void(uint32_t)>;

/**
 * @brief Manages OLED display with animation capabilities
 */
class OLEDDisplayManager{
public:
  /**
   * @brief Constructor
   */
  OLEDDisplayManager() : display_(nullptr), width_(128), height_(128), initialized_(false){}

  /**
   * @brief Initialize OLED display
   * @param i2c_bus I2C bus number (default: 0)
   * @param sda_pin SDA GPIO pin (default: 2)
   * @param scl_pin SCL GPIO pin (default: 1)
   * @param freq_hz I2C frequency in Hz (default: 400kHz)
   * @param flip_horizontal Flip display horizontally
   * @param flip_vertical Flip display vertically
   * @param contrast Display contrast (0-255)
   * @return true if successful
   */
  bool initialize(int i2c_bus = 0, int sda_pin = 2, int scl_pin = 1, 
                  uint32_t freq_hz = 400000, bool flip_horizontal = true,
                  bool flip_vertical = true, uint8_t contrast = 0xCF){
    // Initialize I2C bus
    HalResult i2c_result = ESP32S3_I2C_HAL::Initialize(i2c_bus, sda_pin, scl_pin, freq_hz);
    if(i2c_result != HalResult::Success){
      return false;
    }
    
    // Initialize OLED display
    display_ = new DRIVER_OLED_SH1107();
    
    OLEDConfig config;
    config.contrast = contrast;
    config.flip_horizontal = flip_horizontal;
    config.flip_vertical = flip_vertical;
    
    if(!display_->initialize(config)){
      delete display_;
      display_ = nullptr;
      return false;
    }
    
    // Set upside down mode
    display_->setUpsideDown(true);
    
    initialized_ = true;
    clear();
    return true;
  }

  /**
   * @brief Check if display is initialized
   */
  bool isInitialized() const{
    return initialized_;
  }

  /**
   * @brief Get display width
   */
  int getWidth() const{
    return width_;
  }

  /**
   * @brief Get display height
   */
  int getHeight() const{
    return height_;
  }

  /**
   * @brief Clear display buffer
   */
  void clear(){
    if(!initialized_) return;
    display_->clearBuffer();
  }

  /**
   * @brief Update display (flush buffer to screen)
   */
  void show(){
    if(!initialized_) return;
    display_->updateDisplay();
  }

  /**
   * @brief Set pixel
   * @param x X coordinate
   * @param y Y coordinate
   * @param on Pixel state (true = on, false = off)
   */
  void setPixel(int x, int y, bool on = true){
    if(!initialized_) return;
    display_->setPixel(x, y, on);
  }

  /**
   * @brief Draw text string
   * @param x X coordinate
   * @param y Y coordinate
   * @param text Text to draw
   * @param on Text color (true = white, false = black)
   */
  void drawText(int x, int y, const char* text, bool on = true){
    if(!initialized_) return;
    display_->drawString(x, y, text, on);
  }

  /**
   * @brief Draw line (Bresenham's algorithm)
   * @param x0 Start X
   * @param y0 Start Y
   * @param x1 End X
   * @param y1 End Y
   * @param on Line state (true = on, false = off)
   */
  void drawLine(int x0, int y0, int x1, int y1, bool on = true){
    if(!initialized_) return;
    display_->drawLine(x0, y0, x1, y1, on);
  }

  /**
   * @brief Draw antialiased line using grayscale simulation
   * Note: OLED is monochrome, so we simulate antialiasing with dithering
   * @param x0 Start X (float for sub-pixel precision)
   * @param y0 Start Y
   * @param x1 End X
   * @param y1 End Y
   */
  void drawLineAntialiased(float x0, float y0, float x1, float y1){
    if(!initialized_) return;
    
    // Xiaolin Wu's line algorithm adapted for monochrome display
    bool steep = fabs(y1 - y0) > fabs(x1 - x0);
    
    if(steep){
      std::swap(x0, y0);
      std::swap(x1, y1);
    }
    if(x0 > x1){
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    
    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = (dx == 0.0f) ? 1.0f : dy / dx;
    
    // Handle first endpoint
    float xend = roundf(x0);
    float yend = y0 + gradient * (xend - x0);
    float xgap = 1.0f - fmodf(x0 + 0.5f, 1.0f);
    int xpxl1 = static_cast<int>(xend);
    int ypxl1 = static_cast<int>(yend);
    
    // For monochrome, use threshold for antialiasing effect
    float brightness1 = (1.0f - fmodf(yend, 1.0f)) * xgap;
    float brightness2 = fmodf(yend, 1.0f) * xgap;
    
    if(steep){
      if(brightness1 > 0.5f) setPixel(ypxl1, xpxl1, true);
      if(brightness2 > 0.5f) setPixel(ypxl1 + 1, xpxl1, true);
    }else{
      if(brightness1 > 0.5f) setPixel(xpxl1, ypxl1, true);
      if(brightness2 > 0.5f) setPixel(xpxl1, ypxl1 + 1, true);
    }
    
    float intery = yend + gradient;
    
    // Handle second endpoint
    xend = roundf(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fmodf(x1 + 0.5f, 1.0f);
    int xpxl2 = static_cast<int>(xend);
    int ypxl2 = static_cast<int>(yend);
    
    brightness1 = (1.0f - fmodf(yend, 1.0f)) * xgap;
    brightness2 = fmodf(yend, 1.0f) * xgap;
    
    if(steep){
      if(brightness1 > 0.5f) setPixel(ypxl2, xpxl2, true);
      if(brightness2 > 0.5f) setPixel(ypxl2 + 1, xpxl2, true);
    }else{
      if(brightness1 > 0.5f) setPixel(xpxl2, ypxl2, true);
      if(brightness2 > 0.5f) setPixel(xpxl2, ypxl2 + 1, true);
    }
    
    // Main loop - always draw main pixels
    if(steep){
      for(int x = xpxl1 + 1; x < xpxl2; x++){
        int ipart = static_cast<int>(intery);
        float fpart = intery - ipart;
        
        setPixel(ipart, x, true);
        // Add antialiasing pixels based on fractional part
        if(fpart > 0.3f){
          setPixel(ipart + 1, x, true);
        }
        
        intery += gradient;
      }
    }else{
      for(int x = xpxl1 + 1; x < xpxl2; x++){
        int ipart = static_cast<int>(intery);
        float fpart = intery - ipart;
        
        setPixel(x, ipart, true);
        // Add antialiasing pixels based on fractional part
        if(fpart > 0.3f){
          setPixel(x, ipart + 1, true);
        }
        
        intery += gradient;
      }
    }
  }

  /**
   * @brief Draw rectangle
   */
  void drawRect(int x, int y, int w, int h, bool fill = false, bool on = true){
    if(!initialized_) return;
    display_->drawRect(x, y, w, h, fill, on);
  }

  /**
   * @brief Fill rectangle
   */
  void fillRect(int x, int y, int w, int h, bool on = true){
    if(!initialized_) return;
    drawRect(x, y, w, h, true, on);
  }

  /**
   * @brief Draw circle
   */
  void drawCircle(int cx, int cy, int radius, bool on = true){
    if(!initialized_) return;
    int x = radius;
    int y = 0;
    int err = 0;
    
    while(x >= y){
      setPixel(cx + x, cy + y, on);
      setPixel(cx + y, cy + x, on);
      setPixel(cx - y, cy + x, on);
      setPixel(cx - x, cy + y, on);
      setPixel(cx - x, cy - y, on);
      setPixel(cx - y, cy - x, on);
      setPixel(cx + y, cy - x, on);
      setPixel(cx + x, cy - y, on);
      
      if(err <= 0){
        y += 1;
        err += 2 * y + 1;
      }
      if(err > 0){
        x -= 1;
        err -= 2 * x + 1;
      }
    }
  }

  /**
   * @brief Fill circle
   */
  void fillCircle(int cx, int cy, int radius, bool on = true){
    if(!initialized_) return;
    for(int y = -radius; y <= radius; y++){
      for(int x = -radius; x <= radius; x++){
        if(x * x + y * y <= radius * radius){
          setPixel(cx + x, cy + y, on);
        }
      }
    }
  }

  /**
   * @brief Register animation function
   * @param name Animation name
   * @param func Animation function
   */
  void registerAnimation(const char* name, OLEDAnimationFunc func){
    animations_.push_back({name, func});
  }

  /**
   * @brief Get animation count
   */
  size_t getAnimationCount() const{
    return animations_.size();
  }

  /**
   * @brief Get animation name by index
   */
  const char* getAnimationName(size_t index) const{
    if(index < animations_.size()){
      return animations_[index].name;
    }
    return nullptr;
  }

  /**
   * @brief Execute animation by index
   * @param index Animation index
   * @param time_ms Animation time in milliseconds
   */
  void executeAnimation(size_t index, uint32_t time_ms){
    if(!initialized_ || index >= animations_.size()) return;
    animations_[index].func(time_ms);
  }

  /**
   * @brief Execute animation by name
   * @param name Animation name
   * @param time_ms Animation time in milliseconds
   * @return true if animation found and executed
   */
  bool executeAnimation(const char* name, uint32_t time_ms){
    if(!initialized_) return false;
    
    for(const auto& anim : animations_){
      if(strcmp(anim.name, name) == 0){
        anim.func(time_ms);
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Clear all registered animations
   */
  void clearAnimations(){
    animations_.clear();
  }

  /**
   * @brief Get direct access to display driver (for advanced usage)
   */
  DRIVER_OLED_SH1107* getDisplay(){
    return display_;
  }

  /**
   * @brief Destructor
   */
  ~OLEDDisplayManager(){
    if(display_){
      delete display_;
    }
  }

private:
  struct AnimationEntry{
    const char* name;
    OLEDAnimationFunc func;
  };

  DRIVER_OLED_SH1107* display_;
  int width_;
  int height_;
  bool initialized_;
  std::vector<AnimationEntry> animations_;
};

} // namespace arcos::manager

#endif // OLED_DISPLAY_MANAGER_HPP
