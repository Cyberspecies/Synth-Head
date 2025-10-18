#include <Arduino.h>
#include "LedController.h"
#include "LedController.impl.hpp"

// Global LED controller instance
LedController led_controller;

// Test mode enumeration
enum TestMode{
  RAINBOW_CYCLE = 0,
  CHASE_EFFECT,
  BREATHING_EFFECT,
  SOLID_COLORS,
  MODE_COUNT
};

TestMode current_mode = RAINBOW_CYCLE;
unsigned long mode_switch_time = 0;
const unsigned long MODE_DURATION_MS = 10000; // 10 seconds per mode

void setup(){
  Serial.begin(115200);
  while(!Serial){
    delay(10);
  }
  
  Serial.println("=== ARCOS LED Advanced Test Suite ===");
  Serial.println("Multiple LED effects will cycle automatically");
  Serial.println("");
  
  if(!led_controller.initialize()){
    Serial.println("ERROR: Failed to initialize LED controller!");
    while(1){
      delay(1000);
    }
  }
  
  Serial.println("LED Controller initialized successfully!");
  printModeInfo();
  
  mode_switch_time = millis();
}

void loop(){
  // Check if it's time to switch modes
  if(millis() - mode_switch_time >= MODE_DURATION_MS){
    switchToNextMode();
    mode_switch_time = millis();
  }
  
  // Run current effect
  switch(current_mode){
    case RAINBOW_CYCLE:
      led_controller.update(); // Uses the built-in rainbow effect
      break;
      
    case CHASE_EFFECT:
      led_controller.runChaseEffect(0xFF4500, 100); // Orange chase
      break;
      
    case BREATHING_EFFECT:
      led_controller.runBreathingEffect(0x0080FF, 0.05f); // Blue breathing
      break;
      
    case SOLID_COLORS:
      runSolidColorTest();
      break;
      
    default:
      current_mode = RAINBOW_CYCLE;
      break;
  }
  
  delay(20);
}

void switchToNextMode(){
  current_mode = (TestMode)((current_mode + 1) % MODE_COUNT);
  printModeInfo();
  
  // Reset any mode-specific parameters
  if(current_mode == RAINBOW_CYCLE){
    led_controller.setRainbowSpeed(2.0f);
    led_controller.setUpdateInterval(50);
  }
}

void printModeInfo(){
  Serial.println("----------------------------------------");
  switch(current_mode){
    case RAINBOW_CYCLE:
      Serial.println("Mode: RAINBOW CYCLE");
      Serial.println("Smooth hue cycling across all LED strips");
      break;
      
    case CHASE_EFFECT:
      Serial.println("Mode: CHASE EFFECT");
      Serial.println("Orange light chasing across all strips");
      break;
      
    case BREATHING_EFFECT:
      Serial.println("Mode: BREATHING EFFECT");
      Serial.println("Blue breathing effect on all strips");
      break;
      
    case SOLID_COLORS:
      Serial.println("Mode: SOLID COLORS");
      Serial.println("Cycling through solid colors");
      break;
      
    default:
      Serial.println("Mode: UNKNOWN");
      break;
  }
  Serial.println("----------------------------------------");
}

void runSolidColorTest(){
  static unsigned long color_change_time = 0;
  static int color_index = 0;
  const unsigned long COLOR_DURATION_MS = 2000; // 2 seconds per color
  
  // Array of test colors
  uint32_t test_colors[] = {
    0xFF0000, // Red
    0x00FF00, // Green
    0x0000FF, // Blue
    0xFFFF00, // Yellow
    0xFF00FF, // Magenta
    0x00FFFF, // Cyan
    0xFFFFFF, // White
    0xFF8000  // Orange
  };
  const int color_count = sizeof(test_colors) / sizeof(test_colors[0]);
  
  if(millis() - color_change_time >= COLOR_DURATION_MS){
    led_controller.setAllStripsColor(test_colors[color_index]);
    color_index = (color_index + 1) % color_count;
    color_change_time = millis();
  }
}