#include <Arduino.h>
#include "LedController.h"
#include "LedController.impl.hpp"

LedController led_controller;

void setup(){
  Serial.begin(115200);
  while(!Serial){
    delay(10);
  }
  
  Serial.println("=== ARCOS LED Rainbow Hue Cycle Test ===");
  Serial.println("");
  Serial.println("This test demonstrates a smooth rainbow hue cycle");
  Serial.println("across all connected LED strips:");
  Serial.println("- Left Fin (GPIO 18): 13 LEDs");
  Serial.println("- Tongue (GPIO 5): 9 LEDs");
  Serial.println("- Right Fin (GPIO 38): 13 LEDs");
  Serial.println("- Scale LEDs (GPIO 37): 14 LEDs");
  Serial.println("");
  
  if(!led_controller.initialize()){
    Serial.println("ERROR: Failed to initialize LED controller!");
    Serial.println("Check your wiring and connections!");
    while(1){
      delay(1000);
    }
  }
  
  Serial.println("✓ LED Controller initialized successfully!");
  Serial.println("✓ Starting rainbow hue cycle...");
  Serial.println("");
  
  // Configure rainbow effect
  led_controller.setRainbowSpeed(1.5f);     // Smooth, medium speed
  led_controller.setUpdateInterval(40);     // 25 FPS for smooth animation
  
  Serial.println("Press Ctrl+C to stop the test");
  Serial.println("You should see a smooth rainbow cycling through all colors!");
}

void loop(){
  // Update the rainbow effect
  led_controller.update();
  
  // Optional: Print status every 10 seconds
  static unsigned long last_status = 0;
  if(millis() - last_status >= 10000){
    Serial.println("Rainbow hue cycle active... 🌈");
    last_status = millis();
  }
  
  // Small delay to maintain timing
  delay(15);
}