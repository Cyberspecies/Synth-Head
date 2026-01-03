/*****************************************************************
 * File:      Esp32HalSensors.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of HAL sensor interfaces for
 *    ICM20948 IMU and BME280 environmental sensor.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_SENSORS_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_SENSORS_HPP_

#include "HAL/IHalImu.hpp"
#include "HAL/IHalEnvironmental.hpp"
#include "HAL/IHalI2c.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>

namespace arcos::hal::esp32{

// ============================================================
// ICM20948 IMU Implementation
// ============================================================

/** ESP32 ICM20948 IMU Implementation */
class Esp32HalImu : public IHalImu{
private:
  static constexpr const char* TAG = "IMU";
  
  // ICM20948 Register addresses
  static constexpr uint8_t REG_WHO_AM_I = 0x00;
  static constexpr uint8_t REG_USER_CTRL = 0x03;
  static constexpr uint8_t REG_PWR_MGMT_1 = 0x06;
  static constexpr uint8_t REG_PWR_MGMT_2 = 0x07;
  static constexpr uint8_t REG_ACCEL_XOUT_H = 0x2D;
  static constexpr uint8_t REG_GYRO_XOUT_H = 0x33;
  static constexpr uint8_t REG_TEMP_OUT_H = 0x39;
  static constexpr uint8_t REG_BANK_SEL = 0x7F;
  
  static constexpr uint8_t WHO_AM_I_VALUE = 0xEA;
  
  IHalLog* log_ = nullptr;
  IHalI2c* i2c_ = nullptr;
  ImuConfig config_;
  bool initialized_ = false;
  
  // Scale factors
  float accel_scale_ = 1.0f / 8192.0f;  // ±4g default
  float gyro_scale_ = 1.0f / 65.5f;      // ±500 dps default
  
  HalResult selectBank(uint8_t bank){
    return i2c_->writeRegisterByte(config_.address, REG_BANK_SEL, bank << 4);
  }

public:
  Esp32HalImu(IHalI2c* i2c, IHalLog* log = nullptr) 
    : log_(log), i2c_(i2c){}
  
  HalResult init(const ImuConfig& config) override{
    if(!i2c_ || !i2c_->isInitialized()){
      if(log_) log_->error(TAG, "I2C not initialized");
      return HalResult::NOT_INITIALIZED;
    }
    
    config_ = config;
    
    // Check WHO_AM_I
    uint8_t who_am_i;
    HalResult result = i2c_->readRegisterByte(config_.address, REG_WHO_AM_I, &who_am_i);
    if(result != HalResult::OK){
      if(log_) log_->error(TAG, "Failed to read WHO_AM_I");
      return result;
    }
    
    if(who_am_i != WHO_AM_I_VALUE){
      if(log_) log_->error(TAG, "Wrong WHO_AM_I: 0x%02X (expected 0x%02X)", who_am_i, WHO_AM_I_VALUE);
      return HalResult::HARDWARE_FAULT;
    }
    
    if(log_) log_->info(TAG, "ICM20948 detected (WHO_AM_I=0x%02X)", who_am_i);
    
    // Reset device
    i2c_->writeRegisterByte(config_.address, REG_PWR_MGMT_1, 0x80);
    delay(100);
    
    // Wake up device, auto select clock
    i2c_->writeRegisterByte(config_.address, REG_PWR_MGMT_1, 0x01);
    delay(50);
    
    // Enable accel and gyro
    i2c_->writeRegisterByte(config_.address, REG_PWR_MGMT_2, 0x00);
    
    // Set scale factors based on config
    setAccelRange(config_.accel_range);
    setGyroRange(config_.gyro_range);
    
    initialized_ = true;
    if(log_) log_->info(TAG, "IMU initialized");
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(initialized_ && i2c_){
      // Put device to sleep
      i2c_->writeRegisterByte(config_.address, REG_PWR_MGMT_1, 0x40);
    }
    initialized_ = false;
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult readAll(ImuData& data) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    // Read accel, gyro (14 bytes starting from ACCEL_XOUT_H)
    uint8_t buffer[14];
    HalResult result = i2c_->readRegister(config_.address, REG_ACCEL_XOUT_H, buffer, 14);
    if(result != HalResult::OK){
      if(log_) log_->error(TAG, "Failed to read sensor data");
      return result;
    }
    
    // Parse accelerometer (registers 0-5)
    int16_t ax = (buffer[0] << 8) | buffer[1];
    int16_t ay = (buffer[2] << 8) | buffer[3];
    int16_t az = (buffer[4] << 8) | buffer[5];
    
    // Parse gyroscope (registers 6-11)
    int16_t gx = (buffer[6] << 8) | buffer[7];
    int16_t gy = (buffer[8] << 8) | buffer[9];
    int16_t gz = (buffer[10] << 8) | buffer[11];
    
    // Parse temperature (registers 12-13)
    int16_t temp_raw = (buffer[12] << 8) | buffer[13];
    
    // Apply scale factors
    data.accel.x = ax * accel_scale_;
    data.accel.y = ay * accel_scale_;
    data.accel.z = az * accel_scale_;
    
    data.gyro.x = gx * gyro_scale_;
    data.gyro.y = gy * gyro_scale_;
    data.gyro.z = gz * gyro_scale_;
    
    data.temperature = (temp_raw / 333.87f) + 21.0f;
    
    data.accel_valid = true;
    data.gyro_valid = true;
    data.mag_valid = false; // Magnetometer requires separate AK09916 access
    
    data.timestamp = millis();
    
    return HalResult::OK;
  }
  
  HalResult readAccel(Vec3f& accel) override{
    ImuData data;
    HalResult result = readAll(data);
    if(result == HalResult::OK){
      accel = data.accel;
    }
    return result;
  }
  
  HalResult readGyro(Vec3f& gyro) override{
    ImuData data;
    HalResult result = readAll(data);
    if(result == HalResult::OK){
      gyro = data.gyro;
    }
    return result;
  }
  
  HalResult readMag(Vec3f& mag) override{
    // AK09916 magnetometer access - simplified
    mag = Vec3f{0, 0, 0};
    return HalResult::NOT_SUPPORTED;
  }
  
  HalResult readTemperature(float& temperature) override{
    ImuData data;
    HalResult result = readAll(data);
    if(result == HalResult::OK){
      temperature = data.temperature;
    }
    return result;
  }
  
  HalResult calibrateAccel() override{
    if(log_) log_->info(TAG, "Accel calibration not implemented");
    return HalResult::NOT_SUPPORTED;
  }
  
  HalResult calibrateGyro() override{
    if(log_) log_->info(TAG, "Gyro calibration not implemented");
    return HalResult::NOT_SUPPORTED;
  }
  
  HalResult setAccelRange(uint8_t range_g) override{
    switch(range_g){
      case 2:  accel_scale_ = 1.0f / 16384.0f; break;
      case 4:  accel_scale_ = 1.0f / 8192.0f; break;
      case 8:  accel_scale_ = 1.0f / 4096.0f; break;
      case 16: accel_scale_ = 1.0f / 2048.0f; break;
      default: return HalResult::INVALID_PARAM;
    }
    return HalResult::OK;
  }
  
  HalResult setGyroRange(uint16_t range_dps) override{
    switch(range_dps){
      case 250:  gyro_scale_ = 1.0f / 131.0f; break;
      case 500:  gyro_scale_ = 1.0f / 65.5f; break;
      case 1000: gyro_scale_ = 1.0f / 32.8f; break;
      case 2000: gyro_scale_ = 1.0f / 16.4f; break;
      default: return HalResult::INVALID_PARAM;
    }
    return HalResult::OK;
  }
  
  bool dataReady() override{
    return initialized_;
  }
};

// ============================================================
// BME280 Environmental Sensor Implementation
// ============================================================

/** ESP32 BME280 Environmental Sensor Implementation */
class Esp32HalEnvironmental : public IHalEnvironmental{
private:
  static constexpr const char* TAG = "ENV";
  
  // BME280 Register addresses
  static constexpr uint8_t REG_CHIP_ID = 0xD0;
  static constexpr uint8_t REG_RESET = 0xE0;
  static constexpr uint8_t REG_CTRL_HUM = 0xF2;
  static constexpr uint8_t REG_STATUS = 0xF3;
  static constexpr uint8_t REG_CTRL_MEAS = 0xF4;
  static constexpr uint8_t REG_CONFIG = 0xF5;
  static constexpr uint8_t REG_PRESS_MSB = 0xF7;
  static constexpr uint8_t REG_CALIB00 = 0x88;
  static constexpr uint8_t REG_CALIB26 = 0xE1;
  
  static constexpr uint8_t CHIP_ID_BME280 = 0x60;
  static constexpr uint8_t CHIP_ID_BMP280 = 0x58;
  
  IHalLog* log_ = nullptr;
  IHalI2c* i2c_ = nullptr;
  EnvironmentalConfig config_;
  bool initialized_ = false;
  bool is_bme280_ = true;
  
  // Calibration data
  uint16_t dig_T1_;
  int16_t dig_T2_, dig_T3_;
  uint16_t dig_P1_;
  int16_t dig_P2_, dig_P3_, dig_P4_, dig_P5_, dig_P6_, dig_P7_, dig_P8_, dig_P9_;
  uint8_t dig_H1_, dig_H3_;
  int16_t dig_H2_, dig_H4_, dig_H5_;
  int8_t dig_H6_;
  
  int32_t t_fine_;

public:
  Esp32HalEnvironmental(IHalI2c* i2c, IHalLog* log = nullptr)
    : log_(log), i2c_(i2c){}
  
  HalResult init(const EnvironmentalConfig& config) override{
    if(!i2c_ || !i2c_->isInitialized()){
      if(log_) log_->error(TAG, "I2C not initialized");
      return HalResult::NOT_INITIALIZED;
    }
    
    config_ = config;
    
    // Check chip ID
    uint8_t chip_id;
    HalResult result = i2c_->readRegisterByte(config_.address, REG_CHIP_ID, &chip_id);
    if(result != HalResult::OK){
      if(log_) log_->error(TAG, "Failed to read chip ID");
      return result;
    }
    
    if(chip_id == CHIP_ID_BME280){
      is_bme280_ = true;
      if(log_) log_->info(TAG, "BME280 detected");
    }else if(chip_id == CHIP_ID_BMP280){
      is_bme280_ = false;
      if(log_) log_->info(TAG, "BMP280 detected (no humidity)");
    }else{
      if(log_) log_->error(TAG, "Unknown chip ID: 0x%02X", chip_id);
      return HalResult::HARDWARE_FAULT;
    }
    
    // Soft reset
    i2c_->writeRegisterByte(config_.address, REG_RESET, 0xB6);
    delay(10);
    
    // Read calibration data
    uint8_t calib[26];
    i2c_->readRegister(config_.address, REG_CALIB00, calib, 26);
    
    dig_T1_ = (calib[1] << 8) | calib[0];
    dig_T2_ = (calib[3] << 8) | calib[2];
    dig_T3_ = (calib[5] << 8) | calib[4];
    dig_P1_ = (calib[7] << 8) | calib[6];
    dig_P2_ = (calib[9] << 8) | calib[8];
    dig_P3_ = (calib[11] << 8) | calib[10];
    dig_P4_ = (calib[13] << 8) | calib[12];
    dig_P5_ = (calib[15] << 8) | calib[14];
    dig_P6_ = (calib[17] << 8) | calib[16];
    dig_P7_ = (calib[19] << 8) | calib[18];
    dig_P8_ = (calib[21] << 8) | calib[20];
    dig_P9_ = (calib[23] << 8) | calib[22];
    dig_H1_ = calib[25];
    
    if(is_bme280_){
      uint8_t calib_h[7];
      i2c_->readRegister(config_.address, REG_CALIB26, calib_h, 7);
      dig_H2_ = (calib_h[1] << 8) | calib_h[0];
      dig_H3_ = calib_h[2];
      dig_H4_ = (calib_h[3] << 4) | (calib_h[4] & 0x0F);
      dig_H5_ = (calib_h[5] << 4) | ((calib_h[4] >> 4) & 0x0F);
      dig_H6_ = calib_h[6];
    }
    
    // Configure sensor
    if(is_bme280_){
      i2c_->writeRegisterByte(config_.address, REG_CTRL_HUM, config_.humidity_oversampling);
    }
    
    uint8_t ctrl_meas = (config_.temp_oversampling << 5) | 
                        (config_.pressure_oversampling << 2) | 
                        config_.mode;
    i2c_->writeRegisterByte(config_.address, REG_CTRL_MEAS, ctrl_meas);
    
    initialized_ = true;
    if(log_) log_->info(TAG, "Environmental sensor initialized");
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(initialized_ && i2c_){
      // Put to sleep mode
      i2c_->writeRegisterByte(config_.address, REG_CTRL_MEAS, 0x00);
    }
    initialized_ = false;
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult readAll(EnvironmentalData& data) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    // Read raw data (8 bytes: press[3], temp[3], hum[2])
    uint8_t buffer[8];
    HalResult result = i2c_->readRegister(config_.address, REG_PRESS_MSB, buffer, 8);
    if(result != HalResult::OK){
      if(log_) log_->error(TAG, "Failed to read sensor data");
      return result;
    }
    
    int32_t adc_P = ((uint32_t)buffer[0] << 12) | ((uint32_t)buffer[1] << 4) | (buffer[2] >> 4);
    int32_t adc_T = ((uint32_t)buffer[3] << 12) | ((uint32_t)buffer[4] << 4) | (buffer[5] >> 4);
    int32_t adc_H = ((uint32_t)buffer[6] << 8) | buffer[7];
    
    // Compensate temperature
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1_ << 1))) * ((int32_t)dig_T2_)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1_)) * ((adc_T >> 4) - ((int32_t)dig_T1_))) >> 12) * ((int32_t)dig_T3_)) >> 14;
    t_fine_ = var1 + var2;
    data.temperature = (t_fine_ * 5 + 128) >> 8;
    data.temperature /= 100.0f;
    data.temperature_valid = true;
    
    // Compensate pressure
    int64_t var1_64 = ((int64_t)t_fine_) - 128000;
    int64_t var2_64 = var1_64 * var1_64 * (int64_t)dig_P6_;
    var2_64 = var2_64 + ((var1_64 * (int64_t)dig_P5_) << 17);
    var2_64 = var2_64 + (((int64_t)dig_P4_) << 35);
    var1_64 = ((var1_64 * var1_64 * (int64_t)dig_P3_) >> 8) + ((var1_64 * (int64_t)dig_P2_) << 12);
    var1_64 = (((((int64_t)1) << 47) + var1_64)) * ((int64_t)dig_P1_) >> 33;
    
    if(var1_64 == 0){
      data.pressure = 0;
    }else{
      int64_t p = 1048576 - adc_P;
      p = (((p << 31) - var2_64) * 3125) / var1_64;
      var1_64 = (((int64_t)dig_P9_) * (p >> 13) * (p >> 13)) >> 25;
      var2_64 = (((int64_t)dig_P8_) * p) >> 19;
      p = ((p + var1_64 + var2_64) >> 8) + (((int64_t)dig_P7_) << 4);
      data.pressure = (float)p / 256.0f;
    }
    data.pressure_valid = true;
    
    // Compensate humidity (BME280 only)
    if(is_bme280_){
      int32_t v_x1_u32r = (t_fine_ - ((int32_t)76800));
      v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4_) << 20) - (((int32_t)dig_H5_) * v_x1_u32r)) +
                ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6_)) >> 10) *
                (((v_x1_u32r * ((int32_t)dig_H3_)) >> 11) + ((int32_t)32768))) >> 10) +
                ((int32_t)2097152)) * ((int32_t)dig_H2_) + 8192) >> 14));
      v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1_)) >> 4));
      v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
      v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
      data.humidity = (float)(v_x1_u32r >> 12) / 1024.0f;
      data.humidity_valid = true;
    }else{
      data.humidity = 0;
      data.humidity_valid = false;
    }
    
    data.timestamp = millis();
    return HalResult::OK;
  }
  
  HalResult readTemperature(float& temperature) override{
    EnvironmentalData data;
    HalResult result = readAll(data);
    if(result == HalResult::OK){
      temperature = data.temperature;
    }
    return result;
  }
  
  HalResult readHumidity(float& humidity) override{
    EnvironmentalData data;
    HalResult result = readAll(data);
    if(result == HalResult::OK){
      humidity = data.humidity;
    }
    return result;
  }
  
  HalResult readPressure(float& pressure) override{
    EnvironmentalData data;
    HalResult result = readAll(data);
    if(result == HalResult::OK){
      pressure = data.pressure;
    }
    return result;
  }
  
  float calculateAltitude(float sea_level_pressure) override{
    EnvironmentalData data;
    if(readAll(data) != HalResult::OK) return 0;
    return 44330.0f * (1.0f - pow(data.pressure / sea_level_pressure, 0.1903f));
  }
  
  HalResult triggerMeasurement() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    // Force measurement in forced mode
    uint8_t ctrl_meas = (config_.temp_oversampling << 5) | 
                        (config_.pressure_oversampling << 2) | 
                        0x01; // Forced mode
    return i2c_->writeRegisterByte(config_.address, REG_CTRL_MEAS, ctrl_meas);
  }
  
  bool dataReady() override{
    if(!initialized_) return false;
    
    uint8_t status;
    if(i2c_->readRegisterByte(config_.address, REG_STATUS, &status) != HalResult::OK){
      return false;
    }
    return (status & 0x08) == 0; // Not measuring
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_SENSORS_HPP_
