/*****************************************************************
 * File:      Esp32HalSpi.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of SPI Hardware Abstraction interface.
 *    Uses Arduino SPI library for master mode communication.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_SPI_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_SPI_HPP_

#include "HAL/IHalSpi.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <SPI.h>

namespace arcos::hal::esp32{

/** ESP32 SPI Implementation using Arduino SPI library */
class Esp32HalSpi : public IHalSpi{
private:
  static constexpr const char* TAG = "SPI";
  
  IHalLog* log_ = nullptr;
  SpiConfig config_;
  SPIClass* spi_ = nullptr;
  SPISettings settings_;
  bool initialized_ = false;
  bool in_transaction_ = false;
  
  uint8_t convertMode(SpiMode mode) const{
    switch(mode){
      case SpiMode::MODE_0: return SPI_MODE0;
      case SpiMode::MODE_1: return SPI_MODE1;
      case SpiMode::MODE_2: return SPI_MODE2;
      case SpiMode::MODE_3: return SPI_MODE3;
      default: return SPI_MODE0;
    }
  }

public:
  explicit Esp32HalSpi(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalSpi() override{
    deinit();
  }
  
  HalResult init(const SpiConfig& config) override{
    if(initialized_){
      if(log_) log_->warn(TAG, "SPI already initialized");
      return HalResult::ALREADY_INITIALIZED;
    }
    
    config_ = config;
    
    // Use VSPI (default) or HSPI based on bus number
    if(config_.bus == 0){
      spi_ = &SPI;  // VSPI
    } else{
      // Create HSPI instance
      spi_ = new SPIClass(HSPI);
    }
    
    // Initialize SPI with custom pins
    spi_->begin(config_.sck_pin, config_.miso_pin, config_.mosi_pin, config_.cs_pin);
    
    // Create settings
    uint8_t bit_order = (config_.bit_order == SpiBitOrder::MSB_FIRST) ? MSBFIRST : LSBFIRST;
    uint8_t spi_mode = convertMode(config_.mode);
    settings_ = SPISettings(config_.frequency, bit_order, spi_mode);
    
    // Setup CS pin as output
    if(config_.cs_pin > 0){
      pinMode(config_.cs_pin, OUTPUT);
      digitalWrite(config_.cs_pin, HIGH);  // Deassert
    }
    
    initialized_ = true;
    if(log_) log_->info(TAG, "SPI bus %d initialized: SCK=%d, MOSI=%d, MISO=%d, CS=%d, freq=%lu Hz",
                        config_.bus, config_.sck_pin, config_.mosi_pin, config_.miso_pin,
                        config_.cs_pin, config_.frequency);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    if(in_transaction_){
      endTransaction();
    }
    
    spi_->end();
    
    // Delete HSPI instance if we created it
    if(config_.bus != 0 && spi_ != &SPI){
      delete spi_;
    }
    spi_ = nullptr;
    
    initialized_ = false;
    if(log_) log_->info(TAG, "SPI deinitialized");
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult beginTransaction() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(in_transaction_) return HalResult::BUSY;
    
    spi_->beginTransaction(settings_);
    
    // Assert CS
    if(config_.cs_pin > 0){
      digitalWrite(config_.cs_pin, LOW);
    }
    
    in_transaction_ = true;
    return HalResult::OK;
  }
  
  HalResult endTransaction() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!in_transaction_) return HalResult::INVALID_STATE;
    
    // Deassert CS
    if(config_.cs_pin > 0){
      digitalWrite(config_.cs_pin, HIGH);
    }
    
    spi_->endTransaction();
    in_transaction_ = false;
    return HalResult::OK;
  }
  
  HalResult transfer(uint8_t tx_byte, uint8_t* rx_byte) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    uint8_t received = spi_->transfer(tx_byte);
    if(rx_byte){
      *rx_byte = received;
    }
    return HalResult::OK;
  }
  
  HalResult transferBuffer(const uint8_t* tx_buffer, uint8_t* rx_buffer, size_t length) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(length == 0) return HalResult::INVALID_PARAM;
    
    if(tx_buffer && rx_buffer){
      // Full duplex transfer
      for(size_t i = 0; i < length; i++){
        rx_buffer[i] = spi_->transfer(tx_buffer[i]);
      }
    } else if(tx_buffer){
      // Write only
      for(size_t i = 0; i < length; i++){
        spi_->transfer(tx_buffer[i]);
      }
    } else if(rx_buffer){
      // Read only
      for(size_t i = 0; i < length; i++){
        rx_buffer[i] = spi_->transfer(0xFF);
      }
    } else{
      return HalResult::INVALID_PARAM;
    }
    
    return HalResult::OK;
  }
  
  HalResult write(const uint8_t* data, size_t length) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!data || length == 0) return HalResult::INVALID_PARAM;
    
    spi_->writeBytes(data, length);
    return HalResult::OK;
  }
  
  HalResult read(uint8_t* buffer, size_t length) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!buffer || length == 0) return HalResult::INVALID_PARAM;
    
    // ESP32 SPI library doesn't have a direct read, so we transfer
    for(size_t i = 0; i < length; i++){
      buffer[i] = spi_->transfer(0xFF);
    }
    return HalResult::OK;
  }
  
  HalResult setFrequency(uint32_t frequency) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    config_.frequency = frequency;
    uint8_t bit_order = (config_.bit_order == SpiBitOrder::MSB_FIRST) ? MSBFIRST : LSBFIRST;
    uint8_t spi_mode = convertMode(config_.mode);
    settings_ = SPISettings(frequency, bit_order, spi_mode);
    
    if(log_) log_->debug(TAG, "SPI frequency set to %lu Hz", frequency);
    return HalResult::OK;
  }
  
  uint32_t getFrequency() const override{
    return config_.frequency;
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_SPI_HPP_
