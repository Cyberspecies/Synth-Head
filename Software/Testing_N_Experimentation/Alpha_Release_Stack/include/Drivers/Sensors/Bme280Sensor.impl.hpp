/*****************************************************************
 * File:      Bme280Sensor.impl.hpp
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Implementation file for BME280 sensor wrapper using
 *    Adafruit BME280 library for I2C communication.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_BME280_SENSOR_IMPL_HPP_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_BME280_SENSOR_IMPL_HPP_

#include <Wire.h>
#include <Adafruit_BME280.h>

namespace sensors{

// Internal BME280 driver instance (static to avoid header pollution)
static Adafruit_BME280* bme_driver = nullptr;

Bme280Sensor::Bme280Sensor(uint8_t sda_pin, uint8_t scl_pin, uint8_t address)
  : sda_pin_(sda_pin),
    scl_pin_(scl_pin),
    address_(address),
    initialized_(false){
}

bool Bme280Sensor::init(){
  if(initialized_){
    Serial.println("[BME280] Already initialized");
    return true;
  }

  // Note: I2C should already be initialized by ICM20948 sensor
  Serial.printf("[BME280] Using existing I2C: SDA=%d, SCL=%d\n", sda_pin_, scl_pin_);
  
  delay(50); // Brief delay for sensor stability

  // Create BME280 driver instance
  if(bme_driver == nullptr){
    bme_driver = new Adafruit_BME280();
  }

  // Initialize sensor
  Serial.println("[BME280] Initializing sensor...");
  if(!bme_driver->begin(address_, &Wire)){
    Serial.println("[BME280] Failed to initialize sensor");
    return false;
  }

  // Configure sensor settings
  bme_driver->setSampling(
    Adafruit_BME280::MODE_NORMAL,
    Adafruit_BME280::SAMPLING_X1,  // temperature
    Adafruit_BME280::SAMPLING_X1,  // pressure
    Adafruit_BME280::SAMPLING_X1,  // humidity
    Adafruit_BME280::FILTER_OFF
  );

  delay(100);

  initialized_ = true;
  Serial.println("[BME280] Initialization complete");
  return true;
}

bool Bme280Sensor::readData(Bme280Data& data){
  if(!initialized_){
    Serial.println("[BME280] Not initialized");
    return false;
  }

  // Read sensor data
  data.temperature = bme_driver->readTemperature();
  data.humidity = bme_driver->readHumidity();
  data.pressure = bme_driver->readPressure();

  // Check for invalid readings
  if(isnan(data.temperature) || isnan(data.humidity) || isnan(data.pressure)){
    Serial.println("[BME280] Invalid sensor readings");
    return false;
  }

  return true;
}

bool Bme280Sensor::readTemperature(float& temperature){
  if(!initialized_){
    Serial.println("[BME280] Not initialized");
    return false;
  }

  temperature = bme_driver->readTemperature();
  return !isnan(temperature);
}

bool Bme280Sensor::readHumidity(float& humidity){
  if(!initialized_){
    Serial.println("[BME280] Not initialized");
    return false;
  }

  humidity = bme_driver->readHumidity();
  return !isnan(humidity);
}

bool Bme280Sensor::readPressure(float& pressure){
  if(!initialized_){
    Serial.println("[BME280] Not initialized");
    return false;
  }

  pressure = bme_driver->readPressure();
  return !isnan(pressure);
}

void Bme280Sensor::printData(const Bme280Data& data) const{
  Serial.printf("Temp: %6.2f °C | Humidity: %6.2f %% | Pressure: %7.2f hPa\n",
                data.temperature, data.humidity, data.pressure / 100.0f);
}

} // namespace sensors

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_BME280_SENSOR_IMPL_HPP_
