#include "FanController.h"

// Constructor
FanController::FanController(uint8_t pin, uint8_t pwm_channel){
  pin_ = pin;
  pwm_channel_ = pwm_channel;
  current_speed_ = 0;
  ramping_ = false;
  ramp_start_speed_ = 0;
  ramp_target_speed_ = 0;
  ramp_start_time_ = 0;
  ramp_duration_ = 0;
}

// Initialize the fan with PWM
void FanController::begin(){
  // Configure PWM channel
  ledcSetup(pwm_channel_, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
  
  // Attach PWM channel to pin
  ledcAttachPin(pin_, pwm_channel_);
  
  // Start with fan off
  ledcWrite(pwm_channel_, 0);
  current_speed_ = 0;
}

// Set fan speed as percentage (0-100%)
void FanController::setSpeed(uint8_t percentage){
  // Clamp to 0-100
  if(percentage > 100) percentage = 100;
  
  // Convert percentage to 8-bit PWM value (0-255)
  uint8_t pwm_value = (percentage * 255) / 100;
  
  setSpeedRaw(pwm_value);
}

// Set fan speed as raw PWM value (0-255)
void FanController::setSpeedRaw(uint8_t pwm_value){
  current_speed_ = pwm_value;
  ledcWrite(pwm_channel_, pwm_value);
  
  // Cancel any ongoing ramp
  ramping_ = false;
}

// Get current speed percentage
uint8_t FanController::getSpeed(){
  return (current_speed_ * 100) / 255;
}

// Get current raw PWM value
uint8_t FanController::getSpeedRaw(){
  return current_speed_;
}

// Turn fan on at specified speed (default 100%)
void FanController::on(uint8_t percentage){
  setSpeed(percentage);
}

// Turn fan off
void FanController::off(){
  setSpeedRaw(0);
}

// Check if fan is running
bool FanController::isRunning(){
  return current_speed_ > 0;
}

// Ramp speed gradually from current to target over duration_ms
void FanController::rampTo(uint8_t target_percentage, unsigned long duration_ms){
  // Clamp target to 0-100
  if(target_percentage > 100) target_percentage = 100;
  
  // Convert to PWM value
  uint8_t target_pwm = (target_percentage * 255) / 100;
  
  // Set up ramping
  ramping_ = true;
  ramp_start_speed_ = current_speed_;
  ramp_target_speed_ = target_pwm;
  ramp_start_time_ = millis();
  ramp_duration_ = duration_ms;
  
  // If duration is 0, just set immediately
  if(duration_ms == 0){
    setSpeedRaw(target_pwm);
    ramping_ = false;
  }
}

// Update function for ramping (call this in loop if using ramp)
void FanController::update(){
  if(!ramping_) return;
  
  unsigned long elapsed = millis() - ramp_start_time_;
  
  // Check if ramp is complete
  if(elapsed >= ramp_duration_){
    setSpeedRaw(ramp_target_speed_);
    ramping_ = false;
    return;
  }
  
  // Calculate current speed based on linear interpolation
  float progress = (float)elapsed / (float)ramp_duration_;
  int speed_diff = (int)ramp_target_speed_ - (int)ramp_start_speed_;
  uint8_t new_speed = ramp_start_speed_ + (uint8_t)(speed_diff * progress);
  
  // Update PWM directly without canceling ramp
  current_speed_ = new_speed;
  ledcWrite(pwm_channel_, new_speed);
}
