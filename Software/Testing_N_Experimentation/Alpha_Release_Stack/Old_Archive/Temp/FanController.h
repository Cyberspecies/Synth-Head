#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <Arduino.h>

// PWM Configuration
#define FAN_PWM_FREQ 25000     // 25 kHz PWM frequency (typical for PC fans)
#define FAN_PWM_RESOLUTION 8   // 8-bit resolution (0-255)

class FanController{
public:
  // Constructor
  FanController(uint8_t pin, uint8_t pwm_channel);
  
  // Initialize the fan with PWM
  void begin();
  
  // Set fan speed as percentage (0-100%)
  void setSpeed(uint8_t percentage);
  
  // Set fan speed as raw PWM value (0-255)
  void setSpeedRaw(uint8_t pwm_value);
  
  // Get current speed percentage
  uint8_t getSpeed();
  
  // Get current raw PWM value
  uint8_t getSpeedRaw();
  
  // Turn fan on at specified speed (default 100%)
  void on(uint8_t percentage = 100);
  
  // Turn fan off
  void off();
  
  // Check if fan is running
  bool isRunning();
  
  // Ramp speed gradually from current to target over duration_ms
  void rampTo(uint8_t target_percentage, unsigned long duration_ms);
  
  // Update function for ramping (call this in loop if using ramp)
  void update();

private:
  uint8_t pin_;
  uint8_t pwm_channel_;
  uint8_t current_speed_;  // 0-255
  
  // Ramping variables
  bool ramping_;
  uint8_t ramp_start_speed_;
  uint8_t ramp_target_speed_;
  unsigned long ramp_start_time_;
  unsigned long ramp_duration_;
};

#include "FanController.impl.hpp"

#endif // FAN_CONTROLLER_H
