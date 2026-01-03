/*****************************************************************
 * File:      Esp32HalUart.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of HAL UART interface using
 *    Arduino HardwareSerial.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_UART_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_UART_HPP_

#include "HAL/IHalUart.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <HardwareSerial.h>

namespace arcos::hal::esp32{

/** ESP32 UART Implementation
 *  Note: Uses HardwareSerial (UART1/2), not USB CDC (Serial)
 */
class Esp32HalUart : public IHalUart{
private:
  static constexpr const char* TAG = "UART";
  
  IHalLog* log_ = nullptr;
  HardwareSerial* serial_ = nullptr;
  UartConfig config_;
  bool initialized_ = false;

public:
  Esp32HalUart(IHalLog* log = nullptr) : log_(log){}
  
  HalResult init(const UartConfig& config) override{
    config_ = config;
    
    // Use HardwareSerial ports (not USB CDC Serial)
    // Port 0 is typically used by USB CDC on S3, so we skip it
    if(config.port == 1){
      serial_ = &Serial1;
    }else if(config.port == 2){
      serial_ = &Serial2;
    }else{
      if(log_) log_->error(TAG, "Invalid UART port: %d (use 1 or 2)", config.port);
      return HalResult::INVALID_PARAM;
    }
    
    // Configure serial
    serial_->setRxBufferSize(config.rx_buffer_size);
    serial_->setTxBufferSize(config.tx_buffer_size);
    serial_->begin(config.baud_rate, SERIAL_8N1, config.rx_pin, config.tx_pin);
    
    initialized_ = true;
    
    if(log_) log_->info(TAG, "UART%d init: %lu baud, TX=%d, RX=%d", 
                        config.port, config.baud_rate, config.tx_pin, config.rx_pin);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(serial_ && initialized_){
      serial_->end();
    }
    initialized_ = false;
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  size_t available() const override{
    if(!initialized_ || !serial_) return 0;
    return serial_->available();
  }
  
  HalResult readByte(uint8_t* byte, uint32_t timeout_ms) override{
    if(!initialized_ || !serial_) return HalResult::NOT_INITIALIZED;
    if(!byte) return HalResult::INVALID_PARAM;
    
    if(timeout_ms > 0){
      serial_->setTimeout(timeout_ms);
    }
    
    unsigned long start = millis();
    while(serial_->available() == 0){
      if(timeout_ms > 0 && (millis() - start) >= timeout_ms){
        return HalResult::TIMEOUT;
      }
      yield();
    }
    
    *byte = serial_->read();
    return HalResult::OK;
  }
  
  HalResult read(uint8_t* buffer, size_t length, size_t* bytes_read, uint32_t timeout_ms) override{
    if(!initialized_ || !serial_) return HalResult::NOT_INITIALIZED;
    if(!buffer || length == 0) return HalResult::INVALID_PARAM;
    
    if(timeout_ms > 0){
      serial_->setTimeout(timeout_ms);
    }
    
    size_t read_count = serial_->readBytes(buffer, length);
    
    if(bytes_read){
      *bytes_read = read_count;
    }
    
    if(read_count == 0 && timeout_ms > 0){
      return HalResult::TIMEOUT;
    }
    
    return HalResult::OK;
  }
  
  HalResult writeByte(uint8_t byte) override{
    if(!initialized_ || !serial_) return HalResult::NOT_INITIALIZED;
    serial_->write(byte);
    return HalResult::OK;
  }
  
  HalResult write(const uint8_t* data, size_t length, size_t* bytes_written) override{
    if(!initialized_ || !serial_) return HalResult::NOT_INITIALIZED;
    if(!data || length == 0) return HalResult::INVALID_PARAM;
    
    size_t written = serial_->write(data, length);
    
    if(bytes_written){
      *bytes_written = written;
    }
    
    return HalResult::OK;
  }
  
  HalResult flush(uint32_t timeout_ms) override{
    if(!initialized_ || !serial_) return HalResult::NOT_INITIALIZED;
    serial_->flush();
    return HalResult::OK;
  }
  
  HalResult clearRxBuffer() override{
    if(!initialized_ || !serial_) return HalResult::NOT_INITIALIZED;
    while(serial_->available()){
      serial_->read();
    }
    return HalResult::OK;
  }
  
  HalResult clearTxBuffer() override{
    if(!initialized_ || !serial_) return HalResult::NOT_INITIALIZED;
    // Arduino doesn't provide direct TX buffer clear
    serial_->flush();
    return HalResult::OK;
  }
  
  uint32_t getBaudRate() const override{
    return config_.baud_rate;
  }
  
  HalResult setBaudRate(uint32_t baud_rate) override{
    if(!initialized_ || !serial_) return HalResult::NOT_INITIALIZED;
    
    config_.baud_rate = baud_rate;
    serial_->updateBaudRate(baud_rate);
    
    if(log_) log_->info(TAG, "UART%d baud changed to %lu", config_.port, baud_rate);
    return HalResult::OK;
  }
  
  // Direct access for advanced usage
  HardwareSerial* getSerial(){ return serial_; }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_UART_HPP_
