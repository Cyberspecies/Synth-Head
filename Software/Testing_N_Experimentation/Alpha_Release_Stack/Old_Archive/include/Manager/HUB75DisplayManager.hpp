/*****************************************************************
 * File:      HUB75DisplayManager.hpp
 * Category:  Manager/Display
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Manages HUB75 LED matrix display with animation function
 *    caching. Provides line drawing (normal and antialiased),
 *    initialization, and animation management.
 * 
 * Features:
 *    - Initialize and configure HUB75 display
 *    - Draw lines with antialiasing using alpha blending
 *    - Store and execute animation functions (no pixel caching)
 *    - Clear, fill, and update display buffer
 * 
 * Hardware:
 *    - HUB75 Dual LED Matrix (128x32)
 *****************************************************************/

#ifndef HUB75_DISPLAY_MANAGER_HPP
#define HUB75_DISPLAY_MANAGER_HPP

#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include <functional>
#include <vector>
#include <cmath>

using namespace arcos::abstraction::drivers;

namespace arcos::manager{

/**
 * @brief Animation function type for HUB75 display
 * Parameters: time_ms (animation time in milliseconds)
 */
using HUB75AnimationFunc = std::function<void(uint32_t)>;

/**
 * @brief Manages HUB75 display with animation capabilities
 */
class HUB75DisplayManager{
public:
  /**
   * @brief Constructor
   */
  HUB75DisplayManager() : display_(nullptr), width_(0), height_(0), initialized_(false){}

  /**
   * @brief Initialize HUB75 display
   * @param dual_oe_mode Use dual OE pin mode (true for dual panels)
   * @return true if successful
   */
  bool initialize(bool dual_oe_mode = true){
    display_ = new SimpleHUB75Display();
    if(!display_->begin(dual_oe_mode)){
      delete display_;
      display_ = nullptr;
      return false;
    }
    
    width_ = display_->getWidth();
    height_ = display_->getHeight();
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
   * @param color Fill color (default: black)
   */
  void clear(const RGB& color = {0, 0, 0}){
    if(!initialized_) return;
    display_->fill(color);
  }

  /**
   * @brief Update display (show buffer)
   */
  void show(){
    if(!initialized_) return;
    display_->show();
  }

  /**
   * @brief Set pixel color
   * @param x X coordinate
   * @param y Y coordinate
   * @param color RGB color
   */
  void setPixel(int x, int y, const RGB& color){
    if(!initialized_) return;
    display_->setPixel(x, y, color);
  }

  /**
   * @brief Set pixel with alpha blending
   * @param x X coordinate
   * @param y Y coordinate
   * @param color RGB color
   * @param alpha Alpha value (0.0 = transparent, 1.0 = opaque)
   */
  void setPixelAlpha(int x, int y, const RGB& color, float alpha){
    if(!initialized_ || x < 0 || x >= width_ || y < 0 || y >= height_) return;
    
    // Clamp alpha
    if(alpha < 0.0f) alpha = 0.0f;
    if(alpha > 1.0f) alpha = 1.0f;
    
    // For antialiasing, we blend with black (assumes black background)
    // For better results, would need to read existing pixel
    RGB blended;
    blended.r = static_cast<uint8_t>(color.r * alpha);
    blended.g = static_cast<uint8_t>(color.g * alpha);
    blended.b = static_cast<uint8_t>(color.b * alpha);
    
    display_->setPixel(x, y, blended);
  }

  /**
   * @brief Draw line (Bresenham's algorithm)
   * @param x0 Start X
   * @param y0 Start Y
   * @param x1 End X
   * @param y1 End Y
   * @param color Line color
   */
  void drawLine(int x0, int y0, int x1, int y1, const RGB& color){
    if(!initialized_) return;
    
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while(true){
      setPixel(x0, y0, color);
      
      if(x0 == x1 && y0 == y1) break;
      
      int e2 = 2 * err;
      if(e2 > -dy){
        err -= dy;
        x0 += sx;
      }
      if(e2 < dx){
        err += dx;
        y0 += sy;
      }
    }
  }

  /**
   * @brief Draw antialiased line (Xiaolin Wu's algorithm)
   * @param x0 Start X (float for sub-pixel precision)
   * @param y0 Start Y
   * @param x1 End X
   * @param y1 End Y
   * @param color Line color
   */
  void drawLineAntialiased(float x0, float y0, float x1, float y1, const RGB& color){
    if(!initialized_) return;
    
    // Xiaolin Wu's line algorithm
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
    
    if(steep){
      setPixelAlpha(ypxl1, xpxl1, color, (1.0f - fmodf(yend, 1.0f)) * xgap);
      setPixelAlpha(ypxl1 + 1, xpxl1, color, fmodf(yend, 1.0f) * xgap);
    }else{
      setPixelAlpha(xpxl1, ypxl1, color, (1.0f - fmodf(yend, 1.0f)) * xgap);
      setPixelAlpha(xpxl1, ypxl1 + 1, color, fmodf(yend, 1.0f) * xgap);
    }
    
    float intery = yend + gradient;
    
    // Handle second endpoint
    xend = roundf(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fmodf(x1 + 0.5f, 1.0f);
    int xpxl2 = static_cast<int>(xend);
    int ypxl2 = static_cast<int>(yend);
    
    if(steep){
      setPixelAlpha(ypxl2, xpxl2, color, (1.0f - fmodf(yend, 1.0f)) * xgap);
      setPixelAlpha(ypxl2 + 1, xpxl2, color, fmodf(yend, 1.0f) * xgap);
    }else{
      setPixelAlpha(xpxl2, ypxl2, color, (1.0f - fmodf(yend, 1.0f)) * xgap);
      setPixelAlpha(xpxl2, ypxl2 + 1, color, fmodf(yend, 1.0f) * xgap);
    }
    
    // Main loop
    if(steep){
      for(int x = xpxl1 + 1; x < xpxl2; x++){
        int ipart = static_cast<int>(intery);
        float fpart = intery - ipart;
        setPixelAlpha(ipart, x, color, 1.0f - fpart);
        setPixelAlpha(ipart + 1, x, color, fpart);
        intery += gradient;
      }
    }else{
      for(int x = xpxl1 + 1; x < xpxl2; x++){
        int ipart = static_cast<int>(intery);
        float fpart = intery - ipart;
        setPixelAlpha(x, ipart, color, 1.0f - fpart);
        setPixelAlpha(x, ipart + 1, color, fpart);
        intery += gradient;
      }
    }
  }

  /**
   * @brief Draw rectangle
   */
  void drawRect(int x, int y, int w, int h, const RGB& color){
    if(!initialized_) return;
    drawLine(x, y, x + w - 1, y, color);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
    drawLine(x + w - 1, y + h - 1, x, y + h - 1, color);
    drawLine(x, y + h - 1, x, y, color);
  }

  /**
   * @brief Fill rectangle
   */
  void fillRect(int x, int y, int w, int h, const RGB& color){
    if(!initialized_) return;
    for(int j = y; j < y + h; j++){
      for(int i = x; i < x + w; i++){
        if(i >= 0 && i < width_ && j >= 0 && j < height_){
          setPixel(i, j, color);
        }
      }
    }
  }

  /**
   * @brief Draw circle
   */
  void drawCircle(int cx, int cy, int radius, const RGB& color){
    if(!initialized_) return;
    int x = radius;
    int y = 0;
    int err = 0;
    
    while(x >= y){
      setPixel(cx + x, cy + y, color);
      setPixel(cx + y, cy + x, color);
      setPixel(cx - y, cy + x, color);
      setPixel(cx - x, cy + y, color);
      setPixel(cx - x, cy - y, color);
      setPixel(cx - y, cy - x, color);
      setPixel(cx + y, cy - x, color);
      setPixel(cx + x, cy - y, color);
      
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
  void fillCircle(int cx, int cy, int radius, const RGB& color){
    if(!initialized_) return;
    for(int y = -radius; y <= radius; y++){
      for(int x = -radius; x <= radius; x++){
        if(x * x + y * y <= radius * radius){
          setPixel(cx + x, cy + y, color);
        }
      }
    }
  }

  /**
   * @brief Register animation function
   * @param name Animation name
   * @param func Animation function
   */
  void registerAnimation(const char* name, HUB75AnimationFunc func){
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
  SimpleHUB75Display* getDisplay(){
    return display_;
  }

  /**
   * @brief Destructor
   */
  ~HUB75DisplayManager(){
    if(display_){
      delete display_;
    }
  }

private:
  struct AnimationEntry{
    const char* name;
    HUB75AnimationFunc func;
  };

  SimpleHUB75Display* display_;
  int width_;
  int height_;
  bool initialized_;
  std::vector<AnimationEntry> animations_;
};

} // namespace arcos::manager

#endif // HUB75_DISPLAY_MANAGER_HPP
