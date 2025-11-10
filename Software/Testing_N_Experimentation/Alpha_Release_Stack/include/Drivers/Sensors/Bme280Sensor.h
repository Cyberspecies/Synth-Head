/*****************************************************************
 * File:      Bme280Sensor.h
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    BME280 environmental sensor wrapper using ARCOS driver
 *    abstraction with I2C communication for ESP32-S3.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_BME280_SENSOR_H_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_BME280_SENSOR_H_

#include <Arduino.h>

namespace sensors{

/** BME280 sensor data structure for public interface */
struct Bme280Data{
  float temperature;  // °C
  float humidity;     // %
  float pressure;     // Pa
};

/** BME280 environmental sensor wrapper
 * 
 * Provides simple interface to BME280 sensor using ARCOS
 * driver abstraction. Handles I2C initialization and sensor
 * data reading for temperature, humidity, and pressure.
 */
class Bme280Sensor{
private:
  static constexpr const char* TAG = "BME280";
  static constexpr uint8_t DEFAULT_ADDRESS = 0x76;
  
  uint8_t sda_pin_;
  uint8_t scl_pin_;
  uint8_t address_;
  bool initialized_;

public:
  /** Constructor with custom I2C pins
   * @param sda_pin SDA pin number
   * @param scl_pin SCL pin number
   * @param address I2C address (default 0x76, alt 0x77)
   */
  Bme280Sensor(uint8_t sda_pin, uint8_t scl_pin, uint8_t address = DEFAULT_ADDRESS);

  /** Initialize I2C and sensor
   * Note: I2C should already be initialized by first sensor
   * @return true if initialization successful
   */
  bool init();

  /** Check if sensor is initialized
   * @return true if initialized
   */
  bool isInitialized() const{ return initialized_; }

  /** Read all sensor data
   * @param data Reference to Bme280Data structure to fill
   * @return true if read successful
   */
  bool readData(Bme280Data& data);

  /** Read temperature only
   * @param temperature Reference to store temperature (°C)
   * @return true if read successful
   */
  bool readTemperature(float& temperature);

  /** Read humidity only
   * @param humidity Reference to store humidity (%)
   * @return true if read successful
   */
  bool readHumidity(float& humidity);

  /** Read pressure only
   * @param pressure Reference to store pressure (Pa)
   * @return true if read successful
   */
  bool readPressure(float& pressure);

  /** Print sensor data to Serial
   * @param data Sensor data to print
   */
  void printData(const Bme280Data& data) const;
};

} // namespace sensors

// Include implementation
#include "Bme280Sensor.impl.hpp"

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_BME280_SENSOR_H_
