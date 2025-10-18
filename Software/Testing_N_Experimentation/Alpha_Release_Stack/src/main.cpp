#include <Arduino.h>
#include "LedController.h"
#include "LedController.impl.hpp"

// Global LED controller instance
LedController led_controller;

void testIndividualStrips();

void setup(){
  // Initialize serial communication
  Serial.begin(115200);
  // while(!Serial){
  //   delay(10);
  // }
  
  Serial.println("=== ARCOS LED Manual Test ===");
  Serial.println("Testing individual strips then rainbow effect");
  Serial.println("Using WRGB LEDs with white channel support");
  Serial.println("");
  
  // Initialize LED controller
  if(!led_controller.initialize()){
    Serial.println("ERROR: Failed to initialize LED controller!");
    while(1){
      delay(1000);
    }
  }
  
  Serial.println("LED Controller initialized successfully!");
  Serial.println("");
  Serial.println("WRGB LED Strip Configuration:");
  Serial.println("- Left Fin (GPIO 18): 13 WRGB LEDs (RGB only - no white during cycling)");
  Serial.println("- Tongue (GPIO 8): 9 WRGB LEDs (full WRGB with white channel)");  
  Serial.println("- Right Fin (GPIO 38): 13 WRGB LEDs (RGB only - no white during cycling)");
  Serial.println("- Scale LEDs (GPIO 37): 14 WRGB LEDs (RGB only - no white during cycling)");
  Serial.println("");
  
  // Test each strip individually first
  testIndividualStrips();
  
  Serial.println("Now starting rainbow hue cycle effect...");
  Serial.println("Only the tongue will use white channel for enhanced colors!");
  
  // Set rainbow effect parameters
  led_controller.setRainbowSpeed(2.0f);    // Moderate speed
  led_controller.setUpdateInterval(50);    // 20 FPS update rate
}

void loop(){
  // Main rainbow effect update
  led_controller.update();
  
  // Optional: Add some status output every 5 seconds
  static unsigned long last_status_time = 0;
  if(millis() - last_status_time >= 5000){
    Serial.println("Rainbow effect running...");
    last_status_time = millis();
  }
  
  // Small delay to prevent overwhelming the processor
  delay(10);
}

void testIndividualStrips(){
  Serial.println("=== Testing Individual LED Strips ===");
  uint32_t test_color = 0x00FF0000; // Bright red (WRGB format)
  
  Serial.println("1. Testing Left Fin (GPIO 18)...");
  led_controller.setLeftFinColor(test_color);
  delay(3000);
  led_controller.clearAllStrips();
  led_controller.showAllStrips();
  delay(1000);
  
  Serial.println("2. Testing Tongue (GPIO 8)...");
  led_controller.setTongueColor(test_color);
  delay(3000);
  led_controller.clearAllStrips();
  led_controller.showAllStrips();
  delay(1000);
  
  Serial.println("3. Testing Right Fin (GPIO 38)...");
  led_controller.setRightFinColor(test_color);
  delay(3000);
  led_controller.clearAllStrips();
  led_controller.showAllStrips();
  delay(1000);
  
  Serial.println("4. Testing Scale LEDs (GPIO 37)...");
  led_controller.setScaleColor(test_color);
  delay(3000);
  led_controller.clearAllStrips();
  led_controller.showAllStrips();
  delay(1000);
  
  Serial.println("=== Individual Strip Test Complete ===");
  Serial.println("Each strip should have lit up red for 3 seconds");
  Serial.println("If any strip didn't light up, check wiring and power");
  Serial.println("");
}