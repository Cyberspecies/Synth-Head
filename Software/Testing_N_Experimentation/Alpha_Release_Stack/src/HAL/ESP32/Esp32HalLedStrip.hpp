/*****************************************************************
 * File:      Esp32HalLedStrip.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of HAL LED strip interface using
 *    Adafruit NeoPixel library.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_LED_STRIP_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_LED_STRIP_HPP_

#include "HAL/IHalLedStrip.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

namespace arcos::hal::esp32{

/** ESP32 LED Strip Implementation using NeoPixel */
class Esp32HalLedStrip : public IHalLedStrip{
private:
  static constexpr const char* TAG = "LED";
  
  IHalLog* log_ = nullptr;
  Adafruit_NeoPixel* strip_ = nullptr;
  LedStripConfig config_;
  bool initialized_ = false;
  
  neoPixelType getNeoPixelType(){
    switch(config_.type){
      case LedStripType::WS2812_RGB:
      case LedStripType::WS2812B_RGB:
      case LedStripType::NEOPIXEL_RGB:
        return NEO_GRB + NEO_KHZ800;
      case LedStripType::SK6812_RGB:
        return NEO_GRB + NEO_KHZ800;
      case LedStripType::SK6812_RGBW:
      case LedStripType::NEOPIXEL_RGBW:
        return NEO_GRBW + NEO_KHZ800;
      default:
        return NEO_GRBW + NEO_KHZ800;
    }
  }

public:
  Esp32HalLedStrip(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalLedStrip(){
    if(strip_){
      delete strip_;
    }
  }
  
  HalResult init(const LedStripConfig& config) override{
    config_ = config;
    
    if(config_.led_count == 0){
      if(log_) log_->error(TAG, "LED count cannot be 0");
      return HalResult::INVALID_PARAM;
    }
    
    strip_ = new Adafruit_NeoPixel(config_.led_count, config_.pin, getNeoPixelType());
    strip_->begin();
    strip_->setBrightness(config_.brightness);
    strip_->clear();
    strip_->show();
    
    initialized_ = true;
    if(log_) log_->info(TAG, "LED strip init: pin=%d, count=%d", config_.pin, config_.led_count);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(strip_){
      strip_->clear();
      strip_->show();
      delete strip_;
      strip_ = nullptr;
    }
    initialized_ = false;
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult setPixel(uint16_t index, const RGB& color) override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    if(index >= config_.led_count) return HalResult::INVALID_PARAM;
    
    strip_->setPixelColor(index, strip_->Color(color.r, color.g, color.b));
    return HalResult::OK;
  }
  
  HalResult setPixelRGBW(uint16_t index, const RGBW& color) override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    if(index >= config_.led_count) return HalResult::INVALID_PARAM;
    
    strip_->setPixelColor(index, strip_->Color(color.r, color.g, color.b, color.w));
    return HalResult::OK;
  }
  
  HalResult setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    if(index >= config_.led_count) return HalResult::INVALID_PARAM;
    
    strip_->setPixelColor(index, strip_->Color(r, g, b, w));
    return HalResult::OK;
  }
  
  HalResult fill(const RGB& color) override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    
    strip_->fill(strip_->Color(color.r, color.g, color.b));
    return HalResult::OK;
  }
  
  HalResult fillRGBW(const RGBW& color) override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    
    strip_->fill(strip_->Color(color.r, color.g, color.b, color.w));
    return HalResult::OK;
  }
  
  HalResult fillRange(uint16_t start, uint16_t count, const RGB& color) override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    
    strip_->fill(strip_->Color(color.r, color.g, color.b), start, count);
    return HalResult::OK;
  }
  
  HalResult clear() override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    
    strip_->clear();
    return HalResult::OK;
  }
  
  HalResult show() override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    
    strip_->show();
    return HalResult::OK;
  }
  
  HalResult setBrightness(uint8_t brightness) override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    
    config_.brightness = brightness;
    strip_->setBrightness(brightness);
    return HalResult::OK;
  }
  
  uint8_t getBrightness() const override{
    return config_.brightness;
  }
  
  uint16_t getLedCount() const override{
    return config_.led_count;
  }
  
  HalResult getPixel(uint16_t index, RGBW& color) const override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    if(index >= config_.led_count) return HalResult::INVALID_PARAM;
    
    uint32_t c = strip_->getPixelColor(index);
    color.w = (c >> 24) & 0xFF;
    color.r = (c >> 16) & 0xFF;
    color.g = (c >> 8) & 0xFF;
    color.b = c & 0xFF;
    return HalResult::OK;
  }
  
  HalResult setBuffer(const uint8_t* data, size_t length) override{
    if(!initialized_ || !strip_) return HalResult::NOT_INITIALIZED;
    if(!data) return HalResult::INVALID_PARAM;
    
    size_t bytes_per_led = 4; // RGBW
    size_t leds_to_set = length / bytes_per_led;
    if(leds_to_set > config_.led_count) leds_to_set = config_.led_count;
    
    for(size_t i = 0; i < leds_to_set; i++){
      strip_->setPixelColor(i, strip_->Color(
        data[i * 4 + 0],  // R
        data[i * 4 + 1],  // G
        data[i * 4 + 2],  // B
        data[i * 4 + 3]   // W
      ));
    }
    
    return HalResult::OK;
  }
  
  // Direct access
  Adafruit_NeoPixel* getStrip(){ return strip_; }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_LED_STRIP_HPP_
