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
   */
  LEDAnimationManager() : current_animation_index_(0), animation_start_time_(0){}

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

private:
  struct AnimationEntry{
    const char* name;
    LEDAnimationFunc func;
  };

  LedDataPayload led_data_;
  std::vector<AnimationEntry> animations_;
  size_t current_animation_index_;
  uint32_t animation_start_time_;
};

} // namespace arcos::manager

#endif // LED_ANIMATION_MANAGER_HPP
