/*****************************************************************
 * File:      Esp32HalI2c.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of HAL I2C interface using
 *    Arduino Wire library.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_I2C_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_I2C_HPP_

#include "HAL/IHalI2c.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <Wire.h>

namespace arcos::hal::esp32{

/** ESP32 I2C Implementation */
class Esp32HalI2c : public IHalI2c{
private:
  static constexpr const char* TAG = "I2C";
  
  IHalLog* log_ = nullptr;
  TwoWire* wire_ = nullptr;
  I2cConfig config_;
  bool initialized_ = false;

public:
  Esp32HalI2c(IHalLog* log = nullptr) : log_(log){}
  
  HalResult init(const I2cConfig& config) override{
    config_ = config;
    
    // Select Wire instance based on bus
    if(config.bus == 0){
      wire_ = &Wire;
    }else if(config.bus == 1){
      wire_ = &Wire1;
    }else{
      if(log_) log_->error(TAG, "Invalid I2C bus: %d", config.bus);
      return HalResult::INVALID_PARAM;
    }
    
    // Initialize I2C
    wire_->begin(config.sda_pin, config.scl_pin, config.frequency);
    
    initialized_ = true;
    
    if(log_) log_->info(TAG, "I2C%d init: SDA=%d, SCL=%d, freq=%lu", 
                        config.bus, config.sda_pin, config.scl_pin, config.frequency);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(wire_ && initialized_){
      wire_->end();
    }
    initialized_ = false;
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult probe(i2c_addr_t address) override{
    if(!initialized_ || !wire_) return HalResult::NOT_INITIALIZED;
    
    wire_->beginTransmission(address);
    uint8_t error = wire_->endTransmission();
    
    if(error == 0){
      if(log_) log_->debug(TAG, "Device found at 0x%02X", address);
      return HalResult::OK;
    }else{
      if(log_) log_->debug(TAG, "No device at 0x%02X (err=%d)", address, error);
      return HalResult::NO_DATA;
    }
  }
  
  HalResult write(i2c_addr_t address, const uint8_t* data, size_t length) override{
    if(!initialized_ || !wire_) return HalResult::NOT_INITIALIZED;
    if(!data || length == 0) return HalResult::INVALID_PARAM;
    
    wire_->beginTransmission(address);
    wire_->write(data, length);
    uint8_t error = wire_->endTransmission();
    
    if(error != 0){
      if(log_) log_->error(TAG, "I2C write to 0x%02X failed: %d", address, error);
      return HalResult::ERROR;
    }
    
    return HalResult::OK;
  }
  
  HalResult read(i2c_addr_t address, uint8_t* buffer, size_t length) override{
    if(!initialized_ || !wire_) return HalResult::NOT_INITIALIZED;
    if(!buffer || length == 0) return HalResult::INVALID_PARAM;
    
    size_t received = wire_->requestFrom(address, length);
    
    if(received != length){
      if(log_) log_->error(TAG, "I2C read from 0x%02X: expected %d, got %d", 
                           address, length, received);
      return HalResult::ERROR;
    }
    
    for(size_t i = 0; i < length; i++){
      buffer[i] = wire_->read();
    }
    
    return HalResult::OK;
  }
  
  HalResult writeRead(i2c_addr_t address, 
                      const uint8_t* write_data, size_t write_length,
                      uint8_t* read_buffer, size_t read_length) override{
    if(!initialized_ || !wire_) return HalResult::NOT_INITIALIZED;
    
    // Write phase
    wire_->beginTransmission(address);
    if(write_data && write_length > 0){
      wire_->write(write_data, write_length);
    }
    uint8_t error = wire_->endTransmission(false); // Keep connection open
    
    if(error != 0){
      if(log_) log_->error(TAG, "I2C writeRead to 0x%02X write failed: %d", address, error);
      return HalResult::ERROR;
    }
    
    // Read phase
    if(read_buffer && read_length > 0){
      size_t received = wire_->requestFrom(address, read_length);
      
      if(received != read_length){
        if(log_) log_->error(TAG, "I2C writeRead from 0x%02X read failed");
        return HalResult::ERROR;
      }
      
      for(size_t i = 0; i < read_length; i++){
        read_buffer[i] = wire_->read();
      }
    }
    
    return HalResult::OK;
  }
  
  HalResult writeRegister(i2c_addr_t address, uint8_t reg, const uint8_t* data, size_t length) override{
    if(!initialized_ || !wire_) return HalResult::NOT_INITIALIZED;
    
    wire_->beginTransmission(address);
    wire_->write(reg);
    if(data && length > 0){
      wire_->write(data, length);
    }
    uint8_t error = wire_->endTransmission();
    
    if(error != 0){
      if(log_) log_->error(TAG, "I2C writeReg 0x%02X:0x%02X failed: %d", address, reg, error);
      return HalResult::ERROR;
    }
    
    return HalResult::OK;
  }
  
  HalResult readRegister(i2c_addr_t address, uint8_t reg, uint8_t* buffer, size_t length) override{
    return writeRead(address, &reg, 1, buffer, length);
  }
  
  HalResult writeRegisterByte(i2c_addr_t address, uint8_t reg, uint8_t value) override{
    return writeRegister(address, reg, &value, 1);
  }
  
  HalResult readRegisterByte(i2c_addr_t address, uint8_t reg, uint8_t* value) override{
    return readRegister(address, reg, value, 1);
  }
  
  // Direct access for advanced usage
  TwoWire* getWire(){ return wire_; }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_I2C_HPP_
