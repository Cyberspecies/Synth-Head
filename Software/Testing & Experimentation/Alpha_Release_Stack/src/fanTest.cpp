#include <Arduino.h>
#include "FanController.h"

// Fan pin definitions (from PIN_MAPPING_CPU.md)
#define FAN1_PIN 17
#define FAN2_PIN 36

// PWM channels (ESP32 has 16 PWM channels, 0-15)
#define FAN1_PWM_CHANNEL 0
#define FAN2_PWM_CHANNEL 1

// Create fan controller instances
FanController fan1(FAN1_PIN, FAN1_PWM_CHANNEL);
FanController fan2(FAN2_PIN, FAN2_PWM_CHANNEL);

void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Fan Controller Test Program ===");
  Serial.println("Testing PWM-controlled fans on GPIO 17 and GPIO 36\n");
  
  // Initialize fans
  Serial.println("Initializing fans...");
  fan1.begin();
  fan2.begin();
  Serial.println("Fans initialized!\n");
  
  Serial.println("Starting fan test sequence...");
  Serial.println("Watch for speed changes every few seconds\n");
}

void loop(){
  static unsigned long last_update = 0;
  static int test_phase = 0;
  
  // Update fan ramping
  fan1.update();
  fan2.update();
  
  // Run test sequence every 3 seconds
  if(millis() - last_update >= 3000){
    last_update = millis();
    
    switch(test_phase){
      case 0:
        Serial.println("=================================");
        Serial.println("Phase 0: Both fans OFF");
        fan1.off();
        fan2.off();
        break;
        
      case 1:
        Serial.println("=================================");
        Serial.println("Phase 1: Fan 1 @ 25%, Fan 2 OFF");
        fan1.setSpeed(25);
        fan2.off();
        break;
        
      case 2:
        Serial.println("=================================");
        Serial.println("Phase 2: Fan 1 @ 50%, Fan 2 @ 25%");
        fan1.setSpeed(50);
        fan2.setSpeed(25);
        break;
        
      case 3:
        Serial.println("=================================");
        Serial.println("Phase 3: Fan 1 @ 75%, Fan 2 @ 50%");
        fan1.setSpeed(75);
        fan2.setSpeed(50);
        break;
        
      case 4:
        Serial.println("=================================");
        Serial.println("Phase 4: Both fans @ 100%");
        fan1.setSpeed(100);
        fan2.setSpeed(100);
        break;
        
      case 5:
        Serial.println("=================================");
        Serial.println("Phase 5: Ramping Fan 1 from 100% to 0% over 5 seconds");
        Serial.println("         Fan 2 stays at 100%");
        fan1.rampTo(0, 5000);
        break;
        
      case 6:
        Serial.println("=================================");
        Serial.println("Phase 6: Ramping Fan 2 from 100% to 0% over 5 seconds");
        Serial.println("         Fan 1 stays at 0%");
        fan2.rampTo(0, 5000);
        break;
        
      case 7:
        Serial.println("=================================");
        Serial.println("Phase 7: Ramping both fans from 0% to 100% over 5 seconds");
        fan1.rampTo(100, 5000);
        fan2.rampTo(100, 5000);
        break;
        
      case 8:
        Serial.println("=================================");
        Serial.println("Phase 8: Both fans OFF");
        fan1.off();
        fan2.off();
        Serial.println("\nTest sequence complete. Restarting in 5 seconds...\n");
        break;
        
      default:
        test_phase = -1;  // Will increment to 0
        break;
    }
    
    test_phase++;
    if(test_phase > 8){
      test_phase = 0;
      delay(5000);  // Pause before restarting
    }
  }
  
  // Print fan status every 500ms
  static unsigned long last_status = 0;
  if(millis() - last_status >= 500){
    last_status = millis();
    
    Serial.printf("Fan 1: %3d%% (PWM:%3d) %s | Fan 2: %3d%% (PWM:%3d) %s\n",
                  fan1.getSpeed(), fan1.getSpeedRaw(), fan1.isRunning() ? "ON " : "OFF",
                  fan2.getSpeed(), fan2.getSpeedRaw(), fan2.isRunning() ? "ON " : "OFF");
  }
  
  delay(10);
}
