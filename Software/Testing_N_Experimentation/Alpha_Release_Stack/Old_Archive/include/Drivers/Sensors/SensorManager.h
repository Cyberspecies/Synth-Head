/*****************************************************************
 * File:      SensorManager.h
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Unified sensor manager for registering and accessing sensors:
 *    ICM20948 (IMU), BME280 (Environmental), NEO-8M (GPS).
 *    Centralizes initialization and provides easy value access.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_SENSOR_MANAGER_H_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_SENSOR_MANAGER_H_

#include <Arduino.h>
#include "Icm20948Sensor.h"
#include "Bme280Sensor.h"
#include "Neo8mGps.h"
#include "Inmp441Microphone.h"

namespace sensors{

/** Sensor manager configuration */
struct SensorManagerConfig{
  // I2C configuration
  uint8_t i2c_sda_pin;
  uint8_t i2c_scl_pin;
  uint8_t icm20948_address;
  uint8_t bme280_address;
  
  // UART configuration (GPS)
  uint8_t gps_tx_pin;
  uint8_t gps_rx_pin;
  
  // I2S configuration (Microphone)
  uint8_t mic_ws_pin;
  uint8_t mic_sck_pin;
  uint8_t mic_sd_pin;
  uint8_t mic_lr_pin;
  
  // Default configuration
  SensorManagerConfig()
    : i2c_sda_pin(9),
      i2c_scl_pin(10),
      icm20948_address(0x68),
      bme280_address(0x76),
      gps_tx_pin(43),
      gps_rx_pin(44),
      mic_ws_pin(42),
      mic_sck_pin(40),
      mic_sd_pin(2),
      mic_lr_pin(41){}
};

/** Unified sensor manager
 * 
 * Registers and initializes sensors, caches current values,
 * and provides easy access to all sensor data for main application.
 */
class SensorManager{
private:
  static constexpr const char* TAG = "SensorManager";
  
  // Sensor instances
  Icm20948Sensor* imu_sensor_;
  Bme280Sensor* env_sensor_;
  Neo8mGps* gps_sensor_;
  Inmp441Microphone* mic_sensor_;
  
  // Cached sensor values
  Icm20948Data imu_data_;
  Bme280Data env_data_;
  Neo8mGpsData gps_data_;
  Inmp441AudioData mic_data_;
  
  // Validity flags
  bool imu_valid_;
  bool env_valid_;
  bool gps_valid_;
  bool mic_valid_;
  
  SensorManagerConfig config_;
  bool initialized_;

public:
  /** Constructor */
  SensorManager();
  
  /** Destructor */
  ~SensorManager();

  /** Initialize and register all sensors with default configuration
   * @return true if all sensors initialized successfully
   */
  bool init();

  /** Initialize and register all sensors with custom configuration
   * @param config Custom sensor configuration
   * @return true if all sensors initialized successfully
   */
  bool init(const SensorManagerConfig& config);

  /** Check if sensor manager is initialized
   * @return true if initialized
   */
  bool isInitialized() const{ return initialized_; }

  /** Update sensor manager (processes GPS UART, updates cached values)
   * Call this frequently in main loop
   */
  void update();

  /** Get direct access to IMU sensor
   * @return Pointer to ICM20948 sensor (or nullptr if not registered)
   */
  Icm20948Sensor* imu(){ return imu_sensor_; }

  /** Get direct access to environmental sensor
   * @return Pointer to BME280 sensor (or nullptr if not registered)
   */
  Bme280Sensor* environmental(){ return env_sensor_; }

  /** Get direct access to GPS sensor
   * @return Pointer to NEO-8M GPS (or nullptr if not registered)
   */
  Neo8mGps* gps(){ return gps_sensor_; }

  /** Get direct access to microphone
   * @return Pointer to INMP441 microphone (or nullptr if not registered)
   */
  Inmp441Microphone* microphone(){ return mic_sensor_; }

  /** Get cached IMU data
   * @return Reference to cached IMU data
   */
  const Icm20948Data& getImuData() const{ return imu_data_; }

  /** Get cached environmental data
   * @return Reference to cached environmental data
   */
  const Bme280Data& getEnvironmentalData() const{ return env_data_; }

  /** Get cached GPS data
   * @return Reference to cached GPS data
   */
  const Neo8mGpsData& getGpsData() const{ return gps_data_; }

  /** Get cached microphone data
   * @return Reference to cached audio data
   */
  const Inmp441AudioData& getMicrophoneData() const{ return mic_data_; }

  /** Check if IMU data is valid
   * @return true if IMU has valid cached data
   */
  bool isImuValid() const{ return imu_valid_; }

  /** Check if environmental data is valid
   * @return true if environmental sensor has valid cached data
   */
  bool isEnvironmentalValid() const{ return env_valid_; }

  /** Check if GPS data is valid
   * @return true if GPS has valid cached data
   */
  bool isGpsValid() const{ return gps_valid_; }

  /** Check if microphone data is valid
   * @return true if microphone has valid cached data
   */
  bool isMicrophoneValid() const{ return mic_valid_; }
};

} // namespace sensors

// Include implementation
#include "SensorManager.impl.hpp"

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_SENSOR_MANAGER_H_
