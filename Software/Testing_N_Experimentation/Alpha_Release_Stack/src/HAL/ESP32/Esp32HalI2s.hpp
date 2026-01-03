/*****************************************************************
 * File:      Esp32HalI2s.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of I2S Hardware Abstraction interface.
 *    Uses ESP32 I2S driver for audio input/output.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_I2S_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_I2S_HPP_

#include "HAL/IHalI2s.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <driver/i2s.h>

namespace arcos::hal::esp32{

/** ESP32 I2S Implementation using ESP-IDF I2S driver */
class Esp32HalI2s : public IHalI2s{
private:
  static constexpr const char* TAG = "I2S";
  
  IHalLog* log_ = nullptr;
  I2sConfig config_;
  bool initialized_ = false;
  bool streaming_ = false;
  
  i2s_port_t getPort() const{
    return (config_.port == 0) ? I2S_NUM_0 : I2S_NUM_1;
  }
  
  i2s_bits_per_sample_t getBits() const{
    switch(config_.data_format){
      case I2sDataFormat::FORMAT_16BIT: return I2S_BITS_PER_SAMPLE_16BIT;
      case I2sDataFormat::FORMAT_24BIT: return I2S_BITS_PER_SAMPLE_24BIT;
      case I2sDataFormat::FORMAT_32BIT: return I2S_BITS_PER_SAMPLE_32BIT;
      default: return I2S_BITS_PER_SAMPLE_32BIT;
    }
  }
  
  i2s_channel_fmt_t getChannelFormat() const{
    switch(config_.channel_mode){
      case I2sChannelMode::MONO_LEFT: return I2S_CHANNEL_FMT_ONLY_LEFT;
      case I2sChannelMode::MONO_RIGHT: return I2S_CHANNEL_FMT_ONLY_RIGHT;
      case I2sChannelMode::STEREO: return I2S_CHANNEL_FMT_RIGHT_LEFT;
      default: return I2S_CHANNEL_FMT_ONLY_LEFT;
    }
  }

public:
  explicit Esp32HalI2s(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalI2s() override{
    deinit();
  }
  
  HalResult init(const I2sConfig& config) override{
    if(initialized_){
      if(log_) log_->warn(TAG, "I2S already initialized");
      return HalResult::ALREADY_INITIALIZED;
    }
    
    config_ = config;
    
    // Determine mode
    i2s_mode_t i2s_mode;
    switch(config_.mode){
      case I2sMode::MASTER_TX:
        i2s_mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        break;
      case I2sMode::MASTER_RX:
        i2s_mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
        break;
      case I2sMode::SLAVE_TX:
        i2s_mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_TX);
        break;
      case I2sMode::SLAVE_RX:
        i2s_mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX);
        break;
      default:
        i2s_mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    }
    
    // I2S configuration
    i2s_config_t i2s_config = {
      .mode = i2s_mode,
      .sample_rate = config_.sample_rate,
      .bits_per_sample = getBits(),
      .channel_format = getChannelFormat(),
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = static_cast<int>(config_.buffer_size),
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0
    };
    
    // Pin configuration
    i2s_pin_config_t pin_config = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = static_cast<int>(config_.bck_pin),
      .ws_io_num = static_cast<int>(config_.ws_pin),
      .data_out_num = (config_.mode == I2sMode::MASTER_TX || config_.mode == I2sMode::SLAVE_TX) 
                      ? static_cast<int>(config_.data_pin) : I2S_PIN_NO_CHANGE,
      .data_in_num = (config_.mode == I2sMode::MASTER_RX || config_.mode == I2sMode::SLAVE_RX) 
                     ? static_cast<int>(config_.data_pin) : I2S_PIN_NO_CHANGE
    };
    
    // Install and start I2S driver
    esp_err_t err = i2s_driver_install(getPort(), &i2s_config, 0, nullptr);
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "Failed to install I2S driver: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    err = i2s_set_pin(getPort(), &pin_config);
    if(err != ESP_OK){
      i2s_driver_uninstall(getPort());
      if(log_) log_->error(TAG, "Failed to set I2S pins: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    initialized_ = true;
    if(log_) log_->info(TAG, "I2S port %d initialized: BCK=%d, WS=%d, DATA=%d, rate=%lu Hz",
                        config_.port, config_.bck_pin, config_.ws_pin, config_.data_pin,
                        config_.sample_rate);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    if(streaming_){
      stop();
    }
    
    i2s_driver_uninstall(getPort());
    
    initialized_ = false;
    if(log_) log_->info(TAG, "I2S deinitialized");
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult start() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(streaming_) return HalResult::ALREADY_INITIALIZED;
    
    esp_err_t err = i2s_start(getPort());
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "Failed to start I2S: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    streaming_ = true;
    if(log_) log_->debug(TAG, "I2S streaming started");
    return HalResult::OK;
  }
  
  HalResult stop() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!streaming_) return HalResult::INVALID_STATE;
    
    esp_err_t err = i2s_stop(getPort());
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "Failed to stop I2S: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    streaming_ = false;
    if(log_) log_->debug(TAG, "I2S streaming stopped");
    return HalResult::OK;
  }
  
  HalResult read(int32_t* buffer, size_t samples, size_t* samples_read, uint32_t timeout_ms) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!buffer || samples == 0) return HalResult::INVALID_PARAM;
    
    size_t bytes_read = 0;
    size_t bytes_to_read = samples * sizeof(int32_t);
    TickType_t timeout = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    
    esp_err_t err = i2s_read(getPort(), buffer, bytes_to_read, &bytes_read, timeout);
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "I2S read failed: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    if(samples_read){
      *samples_read = bytes_read / sizeof(int32_t);
    }
    
    return HalResult::OK;
  }
  
  HalResult write(const int32_t* buffer, size_t samples, size_t* samples_written, uint32_t timeout_ms) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!buffer || samples == 0) return HalResult::INVALID_PARAM;
    
    size_t bytes_written = 0;
    size_t bytes_to_write = samples * sizeof(int32_t);
    TickType_t timeout = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    
    esp_err_t err = i2s_write(getPort(), buffer, bytes_to_write, &bytes_written, timeout);
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "I2S write failed: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    if(samples_written){
      *samples_written = bytes_written / sizeof(int32_t);
    }
    
    return HalResult::OK;
  }
  
  uint32_t getSampleRate() const override{
    return config_.sample_rate;
  }
  
  HalResult setSampleRate(uint32_t sample_rate) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    esp_err_t err = i2s_set_sample_rates(getPort(), sample_rate);
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "Failed to set sample rate: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    config_.sample_rate = sample_rate;
    if(log_) log_->debug(TAG, "Sample rate set to %lu Hz", sample_rate);
    return HalResult::OK;
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_I2S_HPP_
