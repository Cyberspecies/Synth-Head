/*****************************************************************
 * File:      LEDAnimationManager.hpp
 * Category:  Manager/LED
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Manages LED animations with function caching. Provides ability
 *    to register, store, and execute animation functions without
 *    caching pixel data.
 * 
 * Features:
 *    - Register animation functions by name
 *    - Execute animations with time parameter
 *    - Switch between animations
 *    - Access LED data payload for UART transmission
 * 
 * Hardware:
 *    - NeoPixel RGBW LED strips (4 strips: left fin, tongue, right fin, scale)
 *****************************************************************/

#ifndef LED_ANIMATION_MANAGER_HPP
#define LED_ANIMATION_MANAGER_HPP

#include "Drivers/UART Comms/GpuUartBidirectional.hpp"
#include <functional>
#include <vector>
#include <cmath>

using namespace arcos::communication;

namespace arcos::manager{

/**
 * @brief Animation function type for LED strips
 * Parameters: led_data (reference to LED data payload), time_ms (animation time)
 */
using LEDAnimationFunc = std::function<void(LedDataPayload&, uint32_t)>;

/**
 * @brief Manages LED animations with function caching
 */
class LEDAnimationManager{
public:
  /**
   * @brief Constructor
   * @param led_data Reference to LED data payload
   */
  LEDAnimationManager(LedDataPayload& led_data) 
    : led_data_(led_data), 
      current_animation_index_(0), 
      animation_start_time_(0), 
      use_section_control_(false){}

  /**
   * @brief Initialize LED data payload
   */
  void initialize(){
    led_data_.setAllColor(RgbwColor(0, 0, 0, 0));
    led_data_.fan_speed = 128;  // Default 50% fan speed
  }

  /**
   * @brief Get LED data payload reference
   */
  LedDataPayload& getLedData(){
    return led_data_;
  }

  /**
   * @brief Get const LED data payload reference
   */
  const LedDataPayload& getLedData() const{
    return led_data_;
  }

  /**
   * @brief Set fan speed
   * @param speed Fan speed (0-255)
   */
  void setFanSpeed(uint8_t speed){
    led_data_.fan_speed = speed;
  }

  /**
   * @brief Get current fan speed
   */
  uint8_t getFanSpeed() const{
    return led_data_.fan_speed;
  }

  /**
   * @brief Set all LEDs to a single color
   * @param color RGBW color
   */
  void setAllColor(const RgbwColor& color){
    led_data_.setAllColor(color);
  }

  /**
   * @brief Set left fin LEDs color
   */
  void setLeftFinColor(const RgbwColor& color){
    led_data_.setLeftFinColor(color);
  }

  /**
   * @brief Set tongue LEDs color
   */
  void setTongueColor(const RgbwColor& color){
    led_data_.setTongueColor(color);
  }

  /**
   * @brief Set right fin LEDs color
   */
  void setRightFinColor(const RgbwColor& color){
    led_data_.setRightFinColor(color);
  }

  /**
   * @brief Set scale LEDs color
   */
  void setScaleColor(const RgbwColor& color){
    led_data_.setScaleColor(color);
  }

  /**
   * @brief Register animation function
   * @param name Animation name
   * @param func Animation function
   */
  void registerAnimation(const char* name, LEDAnimationFunc func){
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
   * @brief Get current animation index
   */
  size_t getCurrentAnimationIndex() const{
    return current_animation_index_;
  }

  /**
   * @brief Set current animation by index
   * @param index Animation index
   * @param reset_time Reset animation time (default: true)
   */
  void setCurrentAnimation(size_t index, bool reset_time = true){
    if(index < animations_.size()){
      current_animation_index_ = index;
      if(reset_time){
        animation_start_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
      }
    }
  }

  /**
   * @brief Set current animation by name
   * @param name Animation name
   * @param reset_time Reset animation time (default: true)
   * @return true if animation found and set
   */
  bool setCurrentAnimation(const char* name, bool reset_time = true){
    for(size_t i = 0; i < animations_.size(); i++){
      if(strcmp(animations_[i].name, name) == 0){
        setCurrentAnimation(i, reset_time);
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Execute current animation
   * @param time_ms Current time in milliseconds
   */
  void updateCurrentAnimation(uint32_t time_ms){
    // If section control is active, apply section settings instead of animations
    if(use_section_control_){
      applySectionSettings();
      return;
    }
    
    if(animations_.empty()) return;
    
    uint32_t animation_time = time_ms - animation_start_time_;
    animations_[current_animation_index_].func(led_data_, animation_time);
  }

  /**
   * @brief Execute animation by index
   * @param index Animation index
   * @param time_ms Animation time in milliseconds
   */
  void executeAnimation(size_t index, uint32_t time_ms){
    if(index >= animations_.size()) return;
    animations_[index].func(led_data_, time_ms);
  }

  /**
   * @brief Execute animation by name
   * @param name Animation name
   * @param time_ms Animation time in milliseconds
   * @return true if animation found and executed
   */
  bool executeAnimation(const char* name, uint32_t time_ms){
    for(const auto& anim : animations_){
      if(strcmp(anim.name, name) == 0){
        anim.func(led_data_, time_ms);
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
    current_animation_index_ = 0;
  }

  /**
   * @brief Cycle to next animation
   */
  void nextAnimation(){
    if(animations_.empty()) return;
    current_animation_index_ = (current_animation_index_ + 1) % animations_.size();
    animation_start_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
  }

  /**
   * @brief Cycle to previous animation
   */
  void previousAnimation(){
    if(animations_.empty()) return;
    if(current_animation_index_ == 0){
      current_animation_index_ = animations_.size() - 1;
    }else{
      current_animation_index_--;
    }
    animation_start_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
  }
  
  /**
   * @brief Set LED section settings from web interface
   * @param sections LED section configuration
   */
  void setSectionSettings(const LedSections& sections){
    section_settings_ = sections;
    use_section_control_ = true;
    
    // Apply settings immediately
    applySectionSettings();
  }
  
  /**
   * @brief Disable section control and return to normal animations
   */
  void disableSectionControl(){
    use_section_control_ = false;
  }
  
  /**
   * @brief Check if section control is active
   */
  bool isSectionControlActive() const{
    return use_section_control_;
  }
  
  /**
   * @brief Apply section settings to LED data
   * Called internally and can be called from LED TX task
   */
  void applySectionSettings(){
    if(!use_section_control_) return;
    
    uint32_t time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Apply settings for each body part
    applySectionToLeds(section_settings_.left_fin, led_data_.getLeftFinLeds(), LED_COUNT_LEFT_FIN, time_ms);
    applySectionToLeds(section_settings_.tongue, led_data_.getTongueLeds(), LED_COUNT_TONGUE, time_ms);
    applySectionToLeds(section_settings_.right_fin, led_data_.getRightFinLeds(), LED_COUNT_RIGHT_FIN, time_ms);
    applySectionToLeds(section_settings_.scale, led_data_.getScaleLeds(), LED_COUNT_SCALE, time_ms);
  }

private:
  /**
   * @brief Apply section settings to a specific LED strip
   * @param section Section configuration
   * @param leds Pointer to LED array
   * @param count Number of LEDs
   * @param time_ms Current time for animations
   */
  void applySectionToLeds(const LedSectionData& section, RgbwColor* leds, uint16_t count, uint32_t time_ms){
    switch(section.mode){
      case 0: // Dynamic (HUB75 based) - skip, let normal system handle
        break;
        
      case 1: // Rainbow
        for(uint16_t i = 0; i < count; i++){
          float hue = fmodf((time_ms / 20.0f + i * 360.0f / count), 360.0f);
          RgbwColor color = hsvToRgb(hue, 1.0f, section.brightness / 255.0f);
          leds[i] = color;
        }
        break;
        
      case 2: // Breathing
        {
          float breath = (sinf(time_ms / 1000.0f) + 1.0f) / 2.0f; // 0.0 - 1.0
          uint8_t r = (uint8_t)(section.color_r * breath * section.brightness / 255);
          uint8_t g = (uint8_t)(section.color_g * breath * section.brightness / 255);
          uint8_t b = (uint8_t)(section.color_b * breath * section.brightness / 255);
          for(uint16_t i = 0; i < count; i++){
            leds[i] = RgbwColor(r, g, b, 0);
          }
        }
        break;
        
      case 3: // Solid Color
        {
          uint8_t r = (uint8_t)(section.color_r * section.brightness / 255);
          uint8_t g = (uint8_t)(section.color_g * section.brightness / 255);
          uint8_t b = (uint8_t)(section.color_b * section.brightness / 255);
          for(uint16_t i = 0; i < count; i++){
            leds[i] = RgbwColor(r, g, b, 0);
          }
        }
        break;
        
      case 4: // Off
        for(uint16_t i = 0; i < count; i++){
          leds[i] = RgbwColor(0, 0, 0, 0);
        }
        break;
        
      default:
        break;
    }
  }
  
  /**
   * @brief Convert HSV to RGB
   * @param h Hue (0-360)
   * @param s Saturation (0-1)
   * @param v Value (0-1)
   * @return RGB color
   */
  RgbwColor hsvToRgb(float h, float s, float v){
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r, g, b;
    if(h < 60)       { r = c; g = x; b = 0; }
    else if(h < 120) { r = x; g = c; b = 0; }
    else if(h < 180) { r = 0; g = c; b = x; }
    else if(h < 240) { r = 0; g = x; b = c; }
    else if(h < 300) { r = x; g = 0; b = c; }
    else             { r = c; g = 0; b = x; }
    
    return RgbwColor(
      (uint8_t)((r + m) * 255),
      (uint8_t)((g + m) * 255),
      (uint8_t)((b + m) * 255),
      0
    );
  }
  struct AnimationEntry{
    const char* name;
    LEDAnimationFunc func;
  };

  LedDataPayload& led_data_;
  std::vector<AnimationEntry> animations_;
  size_t current_animation_index_;
  uint32_t animation_start_time_;
  
  // Section control
  LedSections section_settings_;
  bool use_section_control_;
};

} // namespace arcos::manager

#endif // LED_ANIMATION_MANAGER_HPP
