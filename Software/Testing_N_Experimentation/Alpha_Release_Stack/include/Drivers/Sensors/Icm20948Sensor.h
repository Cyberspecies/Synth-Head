/*****************************************************************
 * File:      Icm20948Sensor.h
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ICM20948 9-axis IMU sensor wrapper using ARCOS driver
 *    abstraction with I2C communication for ESP32-S3.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_ICM20948_SENSOR_H_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_ICM20948_SENSOR_H_

#include <Arduino.h>

namespace sensors{

/** ICM20948 sensor data structure for public interface */
struct Icm20948Data{
  // Accelerometer (g)
  float accel_x, accel_y, accel_z;
  
  // Gyroscope (degrees/second)
  float gyro_x, gyro_y, gyro_z;
  
  // Magnetometer (μT)
  float mag_x, mag_y, mag_z;
};

/** ICM20948 9-axis IMU sensor wrapper
 * 
 * Provides simple interface to ICM20948 sensor using ARCOS
 * driver abstraction. Handles I2C initialization and sensor
 * data reading for accelerometer, gyroscope, and magnetometer.
 */
class Icm20948Sensor{
private:
  static constexpr const char* TAG = "ICM20948";
  static constexpr uint8_t DEFAULT_ADDRESS = 0x68;
  
  uint8_t sda_pin_;
  uint8_t scl_pin_;
  uint8_t address_;
  bool initialized_;

public:
  /** Constructor with custom I2C pins
   * @param sda_pin SDA pin number
   * @param scl_pin SCL pin number
   * @param address I2C address (default 0x68)
   */
  Icm20948Sensor(uint8_t sda_pin, uint8_t scl_pin, uint8_t address = DEFAULT_ADDRESS);

  /** Initialize I2C and sensor
   * @return true if initialization successful
   */
  bool init();

  /** Check if sensor is initialized
   * @return true if initialized
   */
  bool isInitialized() const{ return initialized_; }

  /** Read all sensor data
   * @param data Reference to Icm20948Data structure to fill
   * @return true if read successful
   */
  bool readData(Icm20948Data& data);

  /** Print sensor data to Serial
   * @param data Sensor data to print
   */
  void printData(const Icm20948Data& data) const;
};

} // namespace sensors

// Include implementation
#include "Icm20948Sensor.impl.hpp"

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_ICM20948_SENSOR_H_
