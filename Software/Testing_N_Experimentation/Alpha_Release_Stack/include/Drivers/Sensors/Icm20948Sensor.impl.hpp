/*****************************************************************
 * File:      Icm20948Sensor.impl.hpp
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Implementation file for ICM20948 sensor wrapper using
 *    Arduino ICM20948_WE library for I2C communication.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_ICM20948_SENSOR_IMPL_HPP_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_ICM20948_SENSOR_IMPL_HPP_

#include <Wire.h>
#include <ICM20948_WE.h>

namespace sensors{

// Internal ICM20948 driver instance (static to avoid header pollution)
static ICM20948_WE* icm_driver = nullptr;

Icm20948Sensor::Icm20948Sensor(uint8_t sda_pin, uint8_t scl_pin, uint8_t address)
  : sda_pin_(sda_pin),
    scl_pin_(scl_pin),
    address_(address),
    initialized_(false){
}

bool Icm20948Sensor::init(){
  if(initialized_){
    Serial.println("[ICM20948] Already initialized");
    return true;
  }

  // Initialize I2C with custom pins
  Serial.printf("[ICM20948] Initializing I2C: SDA=%d, SCL=%d\n", sda_pin_, scl_pin_);
  Wire.begin(sda_pin_, scl_pin_);
  Wire.setClock(400000); // 400kHz I2C clock
  
  delay(100); // Allow I2C to stabilize

  // Create ICM20948 driver instance
  if(icm_driver == nullptr){
    icm_driver = new ICM20948_WE(&Wire, address_);
  }

  // Initialize sensor
  Serial.println("[ICM20948] Initializing sensor...");
  if(!icm_driver->init()){
    Serial.println("[ICM20948] Failed to initialize sensor");
    return false;
  }

  // Enable accelerometer and gyroscope
  icm_driver->autoOffsets();
  icm_driver->setAccRange(ICM20948_ACC_RANGE_4G);
  icm_driver->setAccDLPF(ICM20948_DLPF_6);
  icm_driver->setGyrRange(ICM20948_GYRO_RANGE_500);
  icm_driver->setGyrDLPF(ICM20948_DLPF_6);
  
  // Enable magnetometer
  icm_driver->setMagOpMode(AK09916_CONT_MODE_100HZ);
  
  delay(100);

  initialized_ = true;
  Serial.println("[ICM20948] Initialization complete");
  return true;
}

bool Icm20948Sensor::readData(Icm20948Data& data){
  if(!initialized_){
    Serial.println("[ICM20948] Not initialized");
    return false;
  }

  // Read sensor data
  icm_driver->readSensor();
  
  // Read accelerometer (g)
  xyzFloat accel;
  icm_driver->getGValues(&accel);
  data.accel_x = accel.x;
  data.accel_y = accel.y;
  data.accel_z = accel.z;
  
  // Read gyroscope (degrees/second)
  xyzFloat gyro;
  icm_driver->getGyrValues(&gyro);
  data.gyro_x = gyro.x;
  data.gyro_y = gyro.y;
  data.gyro_z = gyro.z;
  
  // Read magnetometer (μT)
  xyzFloat mag;
  icm_driver->getMagValues(&mag);
  data.mag_x = mag.x;
  data.mag_y = mag.y;
  data.mag_z = mag.z;

  return true;
}

void Icm20948Sensor::printData(const Icm20948Data& data) const{
  Serial.printf("Accel: X=%7.3f Y=%7.3f Z=%7.3f g | Gyro: X=%7.2f Y=%7.2f Z=%7.2f °/s | Mag: X=%7.2f Y=%7.2f Z=%7.2f μT\n",
                data.accel_x, data.accel_y, data.accel_z,
                data.gyro_x, data.gyro_y, data.gyro_z,
                data.mag_x, data.mag_y, data.mag_z);
}

} // namespace sensors

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_ICM20948_SENSOR_IMPL_HPP_
