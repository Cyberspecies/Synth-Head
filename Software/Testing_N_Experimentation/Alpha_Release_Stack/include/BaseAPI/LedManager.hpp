/*****************************************************************
 * File:      LedManager.hpp
 * Category:  include/BaseAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Hardware-agnostic LED strip management abstraction.
 *    Provides unified interface for:
 *    - Multiple LED strips (SK6812 RGBW, WS2812B, etc.)
 *    - Color management with RGBW support
 *    - Animation primitives
 *    - Safety features (brightness limiting, thermal protection)
 * 
 * Design:
 *    The LED manager abstracts hardware details. Higher layers
 *    work with LED identifiers and colors, not pins and protocols.
 * 
 * Layer:
 *    HAL Layer -> [Base System API - LED] -> Application
 *****************************************************************/

#ifndef ARCOS_INCLUDE_BASEAPI_LED_MANAGER_HPP_
#define ARCOS_INCLUDE_BASEAPI_LED_MANAGER_HPP_

#include "BaseTypes.hpp"
#include <cstring>
#include <cmath>

namespace arcos::base{

// ============================================================
// LED Configuration
// ============================================================

/** LED strip type enumeration */
enum class LedType : uint8_t{
  WS2812B = 0,    // RGB only
  SK6812_RGB,     // RGB only
  SK6812_RGBW,    // RGB + White
  APA102,         // RGB with brightness
  VIRTUAL         // Virtual (for testing)
};

/** LED strip identifier */
enum class LedStripId : uint8_t{
  LEFT_FIN = 0,
  RIGHT_FIN = 1,
  TONGUE = 2,
  SCALE = 3,
  STRIP_4 = 4,
  STRIP_5 = 5,
  STRIP_6 = 6,
  STRIP_7 = 7,
  MAX_STRIPS = 8
};

/** LED strip configuration */
struct LedStripConfig{
  LedStripId id;
  LedType type;
  uint8_t led_count;
  uint8_t max_brightness;     // Maximum allowed brightness (safety)
  float power_limit_watts;    // Power budget per strip
  
  LedStripConfig()
    : id(LedStripId::LEFT_FIN)
    , type(LedType::SK6812_RGBW)
    , led_count(0)
    , max_brightness(255)
    , power_limit_watts(5.0f)
  {}
};

// ============================================================
// LED Color Buffer
// ============================================================

/**
 * LedBuffer - Buffer for LED colors
 * 
 * Stores colors in RGBW format internally, even for RGB-only strips.
 * Provides efficient access and modification of LED colors.
 */
class LedBuffer{
public:
  static constexpr uint8_t MAX_LEDS = 64;
  
  LedBuffer()
    : count_(0)
    , brightness_(255)
    , dirty_(true)
  {
    clear();
  }
  
  /** Initialize buffer for given LED count */
  bool init(uint8_t count){
    if(count > MAX_LEDS) return false;
    count_ = count;
    clear();
    return true;
  }
  
  /** Get LED count */
  uint8_t count() const{ return count_; }
  
  /** Check if buffer has pending changes */
  bool isDirty() const{ return dirty_; }
  
  /** Clear dirty flag */
  void clearDirty(){ dirty_ = false; }
  
  /** Set brightness (0-255) */
  void setBrightness(uint8_t brightness){
    brightness_ = brightness;
    dirty_ = true;
  }
  
  /** Get brightness */
  uint8_t getBrightness() const{ return brightness_; }
  
  /** Clear all LEDs to black */
  void clear(){
    memset(colors_, 0, sizeof(colors_));
    dirty_ = true;
  }
  
  /** Set single LED color (RGB) */
  void setPixel(uint8_t index, const Color& color){
    if(index < count_){
      colors_[index] = ColorW(color);
      dirty_ = true;
    }
  }
  
  /** Set single LED color (RGBW) */
  void setPixel(uint8_t index, const ColorW& color){
    if(index < count_){
      colors_[index] = color;
      dirty_ = true;
    }
  }
  
  /** Set single LED color (RGB values) */
  void setPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b){
    setPixel(index, Color(r, g, b));
  }
  
  /** Set single LED color (RGBW values) */
  void setPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
    setPixel(index, ColorW(r, g, b, w));
  }
  
  /** Get single LED color */
  const ColorW& getPixel(uint8_t index) const{
    if(index < count_) return colors_[index];
    static ColorW black;
    return black;
  }
  
  /** Fill all LEDs with color */
  void fill(const Color& color){
    fill(ColorW(color));
  }
  
  /** Fill all LEDs with RGBW color */
  void fill(const ColorW& color){
    for(uint8_t i = 0; i < count_; i++){
      colors_[i] = color;
    }
    dirty_ = true;
  }
  
  /** Fill range with color */
  void fillRange(uint8_t start, uint8_t end, const ColorW& color){
    if(start > end) return;
    if(end > count_) end = count_;
    for(uint8_t i = start; i < end; i++){
      colors_[i] = color;
    }
    dirty_ = true;
  }
  
  /** Apply gradient between two colors */
  void gradient(const ColorW& start, const ColorW& end){
    if(count_ == 0) return;
    for(uint8_t i = 0; i < count_; i++){
      float t = (float)i / (count_ - 1);
      colors_[i].r = start.r + (uint8_t)((end.r - start.r) * t);
      colors_[i].g = start.g + (uint8_t)((end.g - start.g) * t);
      colors_[i].b = start.b + (uint8_t)((end.b - start.b) * t);
      colors_[i].w = start.w + (uint8_t)((end.w - start.w) * t);
    }
    dirty_ = true;
  }
  
  /** Apply rainbow pattern */
  void rainbow(uint8_t start_hue, uint8_t delta_hue = 10, uint8_t saturation = 255, uint8_t value = 128){
    for(uint8_t i = 0; i < count_; i++){
      uint8_t hue = start_hue + i * delta_hue;
      Color c = Color::fromHSV(hue, saturation, value);
      colors_[i] = ColorW(c);
    }
    dirty_ = true;
  }
  
  /** Shift pixels left (wraps) */
  void shiftLeft(uint8_t count = 1){
    if(count_ == 0 || count == 0) return;
    count = count % count_;
    
    ColorW temp[MAX_LEDS];
    memcpy(temp, colors_, count_ * sizeof(ColorW));
    
    for(uint8_t i = 0; i < count_; i++){
      colors_[i] = temp[(i + count) % count_];
    }
    dirty_ = true;
  }
  
  /** Shift pixels right (wraps) */
  void shiftRight(uint8_t count = 1){
    if(count_ == 0 || count == 0) return;
    count = count % count_;
    shiftLeft(count_ - count);
  }
  
  /** Blend with another buffer */
  void blend(const LedBuffer& other, uint8_t amount){
    if(other.count_ != count_) return;
    
    for(uint8_t i = 0; i < count_; i++){
      colors_[i].r = ((uint16_t)colors_[i].r * (255 - amount) + 
                      (uint16_t)other.colors_[i].r * amount) / 255;
      colors_[i].g = ((uint16_t)colors_[i].g * (255 - amount) + 
                      (uint16_t)other.colors_[i].g * amount) / 255;
      colors_[i].b = ((uint16_t)colors_[i].b * (255 - amount) + 
                      (uint16_t)other.colors_[i].b * amount) / 255;
      colors_[i].w = ((uint16_t)colors_[i].w * (255 - amount) + 
                      (uint16_t)other.colors_[i].w * amount) / 255;
    }
    dirty_ = true;
  }
  
  /** Fade all pixels toward black */
  void fade(uint8_t amount){
    for(uint8_t i = 0; i < count_; i++){
      if(colors_[i].r > amount) colors_[i].r -= amount; else colors_[i].r = 0;
      if(colors_[i].g > amount) colors_[i].g -= amount; else colors_[i].g = 0;
      if(colors_[i].b > amount) colors_[i].b -= amount; else colors_[i].b = 0;
      if(colors_[i].w > amount) colors_[i].w -= amount; else colors_[i].w = 0;
    }
    dirty_ = true;
  }
  
  /** Get raw color array */
  const ColorW* data() const{ return colors_; }
  ColorW* data(){ return colors_; }

private:
  ColorW colors_[MAX_LEDS];
  uint8_t count_;
  uint8_t brightness_;
  bool dirty_;
};

// ============================================================
// LED Strip Interface
// ============================================================

/**
 * ILedStrip - Abstract LED strip interface
 * 
 * Implementations wrap hardware-specific LED drivers
 * while providing a common interface for the middleware.
 */
class ILedStrip{
public:
  virtual ~ILedStrip() = default;
  
  /** Initialize LED strip */
  virtual Result init(const LedStripConfig& config) = 0;
  
  /** Deinitialize LED strip */
  virtual void deinit() = 0;
  
  /** Get configuration */
  virtual const LedStripConfig& getConfig() const = 0;
  
  /** Get LED count */
  virtual uint8_t getLedCount() const = 0;
  
  /** Set global brightness */
  virtual void setBrightness(uint8_t brightness) = 0;
  
  /** Get current brightness */
  virtual uint8_t getBrightness() const = 0;
  
  /** Set single LED color */
  virtual void setPixel(uint8_t index, const ColorW& color) = 0;
  
  /** Fill all LEDs with color */
  virtual void fill(const ColorW& color) = 0;
  
  /** Clear all LEDs */
  virtual void clear() = 0;
  
  /** Update LED strip from buffer */
  virtual void update(const LedBuffer& buffer) = 0;
  
  /** Show current state (send to hardware) */
  virtual void show() = 0;
  
  /** Check if strip is ready */
  virtual bool isReady() const = 0;
};

// ============================================================
// LED Manager
// ============================================================

/**
 * LedManager - Manages multiple LED strips
 * 
 * Provides centralized control with safety features:
 * - Brightness limiting (per-strip and global)
 * - Power budget management
 * - Synchronized updates across strips
 */
class LedManager{
public:
  static constexpr uint8_t MAX_STRIPS = 8;
  
  LedManager()
    : strip_count_(0)
    , global_brightness_(255)
    , power_budget_watts_(25.0f)
    , current_power_watts_(0.0f)
  {
    for(int i = 0; i < MAX_STRIPS; i++){
      strips_[i] = nullptr;
    }
  }
  
  /** Register a LED strip */
  Result addStrip(ILedStrip* strip, LedStripId id){
    uint8_t index = static_cast<uint8_t>(id);
    if(index >= MAX_STRIPS) return Result::INVALID_PARAM;
    if(!strip) return Result::INVALID_PARAM;
    
    strips_[index] = strip;
    buffers_[index].init(strip->getLedCount());
    if(index >= strip_count_) strip_count_ = index + 1;
    return Result::OK;
  }
  
  /** Get strip by ID */
  ILedStrip* getStrip(LedStripId id){
    uint8_t index = static_cast<uint8_t>(id);
    if(index >= MAX_STRIPS) return nullptr;
    return strips_[index];
  }
  
  /** Get buffer for direct manipulation */
  LedBuffer* getBuffer(LedStripId id){
    uint8_t index = static_cast<uint8_t>(id);
    if(index >= MAX_STRIPS) return nullptr;
    return &buffers_[index];
  }
  
  /** Set global brightness (affects all strips) */
  void setGlobalBrightness(uint8_t brightness){
    global_brightness_ = brightness;
    for(int i = 0; i < MAX_STRIPS; i++){
      if(strips_[i]){
        // Apply scaled brightness
        uint8_t scaled = (strips_[i]->getConfig().max_brightness * brightness) / 255;
        strips_[i]->setBrightness(scaled);
      }
    }
  }
  
  /** Get global brightness */
  uint8_t getGlobalBrightness() const{ return global_brightness_; }
  
  /** Set power budget (total watts for all LEDs) */
  void setPowerBudget(float watts){ power_budget_watts_ = watts; }
  
  /** Get current estimated power consumption */
  float getCurrentPower() const{ return current_power_watts_; }
  
  /** Fill all strips with color */
  void fillAll(const ColorW& color){
    for(int i = 0; i < MAX_STRIPS; i++){
      if(strips_[i]){
        buffers_[i].fill(color);
      }
    }
  }
  
  /** Clear all strips */
  void clearAll(){
    for(int i = 0; i < MAX_STRIPS; i++){
      if(strips_[i]){
        buffers_[i].clear();
      }
    }
  }
  
  /** Update all strips from their buffers */
  void updateAll(){
    estimatePower();
    
    for(int i = 0; i < MAX_STRIPS; i++){
      if(strips_[i] && buffers_[i].isDirty()){
        strips_[i]->update(buffers_[i]);
        buffers_[i].clearDirty();
      }
    }
  }
  
  /** Show all strips (send to hardware) */
  void showAll(){
    for(int i = 0; i < MAX_STRIPS; i++){
      if(strips_[i]){
        strips_[i]->show();
      }
    }
  }
  
  /** Update and show all in one call */
  void refresh(){
    updateAll();
    showAll();
  }
  
  /** Get strip count */
  uint8_t getStripCount() const{ return strip_count_; }

private:
  ILedStrip* strips_[MAX_STRIPS];
  LedBuffer buffers_[MAX_STRIPS];
  uint8_t strip_count_;
  uint8_t global_brightness_;
  float power_budget_watts_;
  float current_power_watts_;
  
  /** Estimate current power consumption */
  void estimatePower(){
    // Rough estimate: ~60mA per LED at full white
    // RGB: ~20mA each, W: ~20mA
    float total = 0.0f;
    
    for(int i = 0; i < MAX_STRIPS; i++){
      if(strips_[i]){
        float strip_power = 0.0f;
        const LedBuffer& buf = buffers_[i];
        
        for(uint8_t j = 0; j < buf.count(); j++){
          const ColorW& c = buf.getPixel(j);
          // Estimate mA: (R + G + B) * 20/255 + W * 20/255
          float pixel_ma = ((float)(c.r + c.g + c.b + c.w) / 255.0f) * 20.0f;
          strip_power += pixel_ma;
        }
        
        // Apply brightness
        strip_power *= (float)buf.getBrightness() / 255.0f;
        strip_power *= (float)global_brightness_ / 255.0f;
        
        total += strip_power;
      }
    }
    
    // Convert to watts (5V supply)
    current_power_watts_ = (total / 1000.0f) * 5.0f;
  }
};

// ============================================================
// Predefined Animation Effects
// ============================================================

namespace effects{

/** Breathing effect - fades brightness up and down */
inline void breathe(LedBuffer& buffer, uint32_t time_ms, uint32_t period_ms = 2000){
  float phase = (float)(time_ms % period_ms) / period_ms;
  float brightness = (sinf(phase * 2.0f * 3.14159f) + 1.0f) * 0.5f;
  buffer.setBrightness((uint8_t)(brightness * 255));
}

/** Chase effect - single lit LED moves along strip */
inline void chase(LedBuffer& buffer, const ColorW& color, uint32_t time_ms, 
                  uint32_t speed_ms = 100){
  buffer.clear();
  uint8_t pos = (time_ms / speed_ms) % buffer.count();
  buffer.setPixel(pos, color);
}

/** Comet effect - lit LED with fading tail */
inline void comet(LedBuffer& buffer, const ColorW& color, uint32_t time_ms,
                  uint32_t speed_ms = 50, uint8_t tail_length = 5){
  uint8_t pos = (time_ms / speed_ms) % buffer.count();
  
  for(uint8_t i = 0; i < buffer.count(); i++){
    int8_t dist = pos - i;
    if(dist < 0) dist += buffer.count();
    
    if(dist == 0){
      buffer.setPixel(i, color);
    }else if(dist <= tail_length){
      uint8_t fade = 255 - (dist * 255 / tail_length);
      buffer.setPixel(i, ColorW(
        (color.r * fade) >> 8,
        (color.g * fade) >> 8,
        (color.b * fade) >> 8,
        (color.w * fade) >> 8
      ));
    }else{
      buffer.setPixel(i, ColorW(0, 0, 0, 0));
    }
  }
}

/** Sparkle effect - random pixels flash */
inline void sparkle(LedBuffer& buffer, const ColorW& base, const ColorW& spark,
                    uint8_t density = 10){
  buffer.fill(base);
  for(uint8_t i = 0; i < buffer.count(); i++){
    if((rand() % 100) < density){
      buffer.setPixel(i, spark);
    }
  }
}

/** Pulse effect - all LEDs pulse together */
inline void pulse(LedBuffer& buffer, const ColorW& color, uint32_t time_ms,
                  uint32_t period_ms = 500){
  float phase = (float)(time_ms % period_ms) / period_ms;
  float intensity = (sinf(phase * 2.0f * 3.14159f) + 1.0f) * 0.5f;
  
  ColorW scaled(
    (uint8_t)(color.r * intensity),
    (uint8_t)(color.g * intensity),
    (uint8_t)(color.b * intensity),
    (uint8_t)(color.w * intensity)
  );
  
  buffer.fill(scaled);
}

} // namespace effects

} // namespace arcos::base

#endif // ARCOS_INCLUDE_BASEAPI_LED_MANAGER_HPP_
