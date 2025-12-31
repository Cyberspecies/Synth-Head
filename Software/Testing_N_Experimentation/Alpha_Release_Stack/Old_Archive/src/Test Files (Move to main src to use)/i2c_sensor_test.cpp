/*****************************************************************
 * File:      i2c_sensor_test.cpp
 * Category:  testing/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Test application using unified sensor manager to register
 *    and access ICM20948 (IMU), BME280 (Environmental), and
 *    NEO-8M (GPS) sensors. Demonstrates easy value access pattern.
 *****************************************************************/

#include <Arduino.h>
#include "Drivers/Sensors/SensorManager.h"

using namespace sensors;

// Update rate
constexpr uint32_t UPDATE_INTERVAL_MS = 1; // 1000Hz

// Global sensor manager (registers and caches sensor values)
SensorManager sensor_manager;

void setup(){
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize sensor manager with default configuration
  // (SDA 9, SCL 10, ICM20948 0x68, BME280 0x76, GPS TX 43, RX 44)
  if(!sensor_manager.init()){
    Serial.println("\n[FATAL ERROR] Sensor manager initialization failed!");
    Serial.println("System halted. Check wiring and configuration.");
    while(1){
      delay(1000);
    }
  }

  Serial.println("Starting continuous sensor readings");
  Serial.println("Update Rate: 10Hz (100ms interval)\n");
}

void loop(){
  static uint32_t last_print = 0;
  static uint32_t sample_count = 0;
  
  uint32_t current_time = millis();
  
  // Update sensor manager (processes GPS UART, updates cached values)
  sensor_manager.update();
  
  // Print sensor data at 10Hz
  if(current_time - last_print >= UPDATE_INTERVAL_MS){
    last_print = current_time;
    sample_count++;
    
    // Serial.printf("\n[#%lu @ %.2fs] ", sample_count, current_time / 1000.0f);

    // // Access IMU data from manager's cache
    // if(sensor_manager.isImuValid()){
    //   const Icm20948Data& imu_data = sensor_manager.getImuData();
    //   sensor_manager.imu()->printData(imu_data);
    // }else{
    //   Serial.println("IMU: No data");
    // }

    // Serial.print("              ");

    // // Access environmental data from manager's cache
    // if(sensor_manager.isEnvironmentalValid()){
    //   const Bme280Data& env_data = sensor_manager.getEnvironmentalData();
    //   sensor_manager.environmental()->printData(env_data);
    // }else{
    //   Serial.println("Environmental: No data");
    // }

    // Serial.print("              ");

    // // Access GPS data from manager's cache
    // if(sensor_manager.isGpsValid()){
    //   const Neo8mGpsData& gps_data = sensor_manager.getGpsData();
    //   sensor_manager.gps()->printData(gps_data);
    // }else{
    //   Serial.println("GPS: Waiting for fix...");
    // }

    // Serial.print("              ");

    // Access microphone data from manager's cache
    if(sensor_manager.isMicrophoneValid()){
      const Inmp441AudioData& mic_data = sensor_manager.getMicrophoneData();
      
      // Create visual dB meter
      // dB range typically -60 to 0, map to 0-50 characters
      float db = mic_data.db_level;
      int bar_length = 50;
      
      // Clamp dB to -60 to 0 range
      if(db < -60.0f) db = -60.0f;
      if(db > 0.0f) db = 0.0f;
      
      // Map dB to bar position (0 to bar_length)
      int position = static_cast<int>((db + 60.0f) / 60.0f * bar_length);
      
      // Print sample and peak info
      Serial.printf("Mic: Sample=%8d Peak=%7d | ", 
                    mic_data.current_sample, 
                    mic_data.peak_amplitude);
      
      // Print visual meter
      for(int i = 0; i < bar_length; i++){
        if(i == position){
          Serial.print("|");
        }else if(i < position){
          Serial.print("=");
        }else{
          Serial.print(" ");
        }
      }
      
      // Print dB value and clipping warning
      Serial.printf(" %.1fdB %s\n", 
                    mic_data.db_level,
                    mic_data.clipping ? "[CLIP!]" : "");
    }else{
      Serial.println("Mic: No data");
    }
  }
}
