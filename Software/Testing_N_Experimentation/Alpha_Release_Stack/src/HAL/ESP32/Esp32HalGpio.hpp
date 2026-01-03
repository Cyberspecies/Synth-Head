/*****************************************************************
 * File:      Esp32HalGpio.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of HAL GPIO interfaces using
 *    Arduino GPIO functions.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_GPIO_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_GPIO_HPP_

#include "HAL/IHalGpio.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>

namespace arcos::hal::esp32{

/** ESP32 GPIO Implementation */
class Esp32HalGpio : public IHalGpio{
private:
  static constexpr const char* TAG = "GPIO";
  IHalLog* log_ = nullptr;
  bool initialized_ = false;

public:
  Esp32HalGpio(IHalLog* log = nullptr) : log_(log){}
  
  HalResult init() override{
    initialized_ = true;
    if(log_) log_->info(TAG, "GPIO initialized");
    return HalResult::OK;
  }
  
  HalResult pinMode(gpio_pin_t pin, GpioMode mode) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    uint8_t arduino_mode;
    switch(mode){
      case GpioMode::GPIO_INPUT:         arduino_mode = INPUT; break;
      case GpioMode::GPIO_OUTPUT:        arduino_mode = OUTPUT; break;
      case GpioMode::GPIO_INPUT_PULLUP:  arduino_mode = INPUT_PULLUP; break;
      case GpioMode::GPIO_INPUT_PULLDOWN: arduino_mode = INPUT_PULLDOWN; break;
      default: return HalResult::INVALID_PARAM;
    }
    
    ::pinMode(pin, arduino_mode);
    if(log_) log_->debug(TAG, "Pin %d mode set to %d", pin, (int)mode);
    return HalResult::OK;
  }
  
  GpioState digitalRead(gpio_pin_t pin) override{
    return ::digitalRead(pin) ? GpioState::GPIO_HIGH : GpioState::GPIO_LOW;
  }
  
  HalResult digitalWrite(gpio_pin_t pin, GpioState state) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    ::digitalWrite(pin, state == GpioState::GPIO_HIGH ? HIGH : LOW);
    return HalResult::OK;
  }
};

/** ESP32 PWM Implementation */
class Esp32HalPwm : public IHalPwm{
private:
  static constexpr const char* TAG = "PWM";
  static constexpr uint8_t MAX_CHANNELS = 16;
  
  IHalLog* log_ = nullptr;
  
  struct PwmChannel{
    gpio_pin_t pin = 0;
    uint8_t channel = 0;
    uint32_t frequency = 0;
    uint8_t resolution = 0;
    uint32_t duty = 0;
    bool active = false;
  };
  
  PwmChannel channels_[MAX_CHANNELS];
  uint8_t next_channel_ = 0;
  
  int findChannel(gpio_pin_t pin){
    for(int i = 0; i < MAX_CHANNELS; i++){
      if(channels_[i].active && channels_[i].pin == pin){
        return i;
      }
    }
    return -1;
  }

public:
  Esp32HalPwm(IHalLog* log = nullptr) : log_(log){}
  
  HalResult init(gpio_pin_t pin, uint32_t frequency, uint8_t resolution) override{
    if(next_channel_ >= MAX_CHANNELS){
      if(log_) log_->error(TAG, "No free PWM channels");
      return HalResult::ERROR;
    }
    
    int ch = findChannel(pin);
    if(ch < 0){
      ch = next_channel_++;
    }
    
    channels_[ch].pin = pin;
    channels_[ch].channel = ch;
    channels_[ch].frequency = frequency;
    channels_[ch].resolution = resolution;
    channels_[ch].duty = 0;
    channels_[ch].active = true;
    
    ledcSetup(ch, frequency, resolution);
    ledcAttachPin(pin, ch);
    
    if(log_) log_->info(TAG, "PWM ch%d: pin=%d freq=%lu res=%d", 
                        ch, pin, frequency, resolution);
    return HalResult::OK;
  }
  
  HalResult setDuty(gpio_pin_t pin, uint32_t duty) override{
    int ch = findChannel(pin);
    if(ch < 0) return HalResult::NOT_INITIALIZED;
    
    channels_[ch].duty = duty;
    ledcWrite(ch, duty);
    return HalResult::OK;
  }
  
  HalResult setDutyPercent(gpio_pin_t pin, float percent) override{
    int ch = findChannel(pin);
    if(ch < 0) return HalResult::NOT_INITIALIZED;
    
    if(percent < 0.0f) percent = 0.0f;
    if(percent > 100.0f) percent = 100.0f;
    
    uint32_t max_duty = (1 << channels_[ch].resolution) - 1;
    uint32_t duty = (uint32_t)(percent / 100.0f * max_duty);
    
    return setDuty(pin, duty);
  }
  
  uint32_t getDuty(gpio_pin_t pin) override{
    int ch = findChannel(pin);
    if(ch < 0) return 0;
    return channels_[ch].duty;
  }
};

/** ESP32 Button Implementation */
class Esp32HalButton : public IHalButton{
private:
  static constexpr const char* TAG = "BTN";
  
  IHalLog* log_ = nullptr;
  ButtonConfig config_;
  ButtonState state_;
  bool last_raw_ = false;
  timestamp_ms_t last_change_ = 0;
  bool initialized_ = false;

public:
  Esp32HalButton(IHalLog* log = nullptr) : log_(log){}
  
  HalResult init(const ButtonConfig& config) override{
    config_ = config;
    
    uint8_t mode;
    switch(config_.mode){
      case GpioMode::GPIO_INPUT_PULLUP:   mode = INPUT_PULLUP; break;
      case GpioMode::GPIO_INPUT_PULLDOWN: mode = INPUT_PULLDOWN; break;
      default:                            mode = INPUT; break;
    }
    
    ::pinMode(config_.pin, mode);
    
    // Read initial state
    bool raw = ::digitalRead(config_.pin);
    last_raw_ = config_.active_low ? !raw : raw;
    state_.pressed = last_raw_;
    last_change_ = millis();
    
    initialized_ = true;
    if(log_) log_->info(TAG, "Button on pin %d initialized", config_.pin);
    return HalResult::OK;
  }
  
  HalResult update() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    bool raw = ::digitalRead(config_.pin);
    bool current = config_.active_low ? !raw : raw;
    
    timestamp_ms_t now = millis();
    
    // Clear edge flags
    state_.just_pressed = false;
    state_.just_released = false;
    
    // Debounce
    if(current != last_raw_){
      if(now - last_change_ >= config_.debounce_ms){
        last_raw_ = current;
        last_change_ = now;
        
        if(current && !state_.pressed){
          // Rising edge - button pressed
          state_.pressed = true;
          state_.just_pressed = true;
          state_.press_time = now;
          state_.press_count++;
          if(log_) log_->debug(TAG, "Button %d pressed", config_.pin);
        }else if(!current && state_.pressed){
          // Falling edge - button released
          state_.pressed = false;
          state_.just_released = true;
          if(log_) log_->debug(TAG, "Button %d released", config_.pin);
        }
      }
    }else{
      last_change_ = now;
    }
    
    return HalResult::OK;
  }
  
  ButtonState getState() const override{
    return state_;
  }
  
  bool isPressed() const override{
    return state_.pressed;
  }
  
  bool justPressed() const override{
    return state_.just_pressed;
  }
  
  bool justReleased() const override{
    return state_.just_released;
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_GPIO_HPP_
