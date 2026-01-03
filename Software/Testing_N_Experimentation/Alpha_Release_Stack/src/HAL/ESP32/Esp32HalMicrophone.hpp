/*****************************************************************
 * File:      Esp32HalMicrophone.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of Microphone HAL interface.
 *    Designed for INMP441 I2S microphone with audio processing.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_MICROPHONE_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_MICROPHONE_HPP_

#include "HAL/IHalMicrophone.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

namespace arcos::hal::esp32{

/** ESP32 INMP441 Microphone Implementation */
class Esp32HalMicrophone : public IHalMicrophone{
private:
  static constexpr const char* TAG = "MIC";
  static constexpr float REFERENCE_LEVEL = 2147483648.0f;  // Max 32-bit signed value
  static constexpr float DB_REFERENCE = 94.0f;  // dB SPL at 1 Pa
  
  IHalLog* log_ = nullptr;
  MicrophoneConfig config_;
  bool initialized_ = false;
  
  // Audio data
  MicrophoneData current_data_;
  float gain_ = 1.0f;
  
  // Internal buffer
  int32_t* sample_buffer_ = nullptr;
  size_t buffer_size_ = 0;
  
  i2s_port_t getPort() const{
    return (config_.i2s_port == 0) ? I2S_NUM_0 : I2S_NUM_1;
  }
  
  void processBuffer(){
    if(!sample_buffer_ || buffer_size_ == 0) return;
    
    int64_t sum_squares = 0;
    int32_t peak = 0;
    
    for(size_t i = 0; i < buffer_size_; i++){
      int32_t sample = sample_buffer_[i];
      
      // Apply gain
      sample = static_cast<int32_t>(sample * gain_);
      
      // Track peak
      int32_t abs_sample = abs(sample);
      if(abs_sample > peak){
        peak = abs_sample;
      }
      
      // Sum squares for RMS
      sum_squares += static_cast<int64_t>(sample) * sample;
    }
    
    current_data_.current_sample = sample_buffer_[buffer_size_ - 1];
    current_data_.peak_amplitude = peak;
    
    // Calculate RMS
    float rms_raw = sqrtf(static_cast<float>(sum_squares) / buffer_size_);
    current_data_.rms_level = rms_raw / REFERENCE_LEVEL;
    
    // Calculate dB level (approximate)
    if(rms_raw > 0){
      current_data_.db_level = DB_REFERENCE + 20.0f * log10f(rms_raw / REFERENCE_LEVEL);
    } else{
      current_data_.db_level = -INFINITY;
    }
    
    // Check for clipping (using 95% of max value)
    current_data_.clipping = (peak > static_cast<int32_t>(REFERENCE_LEVEL * 0.95f));
    current_data_.timestamp = millis();
  }

public:
  explicit Esp32HalMicrophone(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalMicrophone() override{
    deinit();
  }
  
  HalResult init(const MicrophoneConfig& config) override{
    if(initialized_){
      if(log_) log_->warn(TAG, "Microphone already initialized");
      return HalResult::ALREADY_INITIALIZED;
    }
    
    config_ = config;
    buffer_size_ = config.buffer_size;
    
    // Allocate sample buffer
    sample_buffer_ = new int32_t[buffer_size_];
    if(!sample_buffer_){
      if(log_) log_->error(TAG, "Failed to allocate sample buffer");
      return HalResult::NO_MEMORY;
    }
    
    // I2S configuration for INMP441
    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = config_.sample_rate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // INMP441 is mono
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = static_cast<int>(buffer_size_),
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0
    };
    
    // Pin configuration
    i2s_pin_config_t pin_config = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = static_cast<int>(config_.bck_pin),
      .ws_io_num = static_cast<int>(config_.ws_pin),
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = static_cast<int>(config_.data_pin)
    };
    
    // Install I2S driver
    esp_err_t err = i2s_driver_install(getPort(), &i2s_config, 0, nullptr);
    if(err != ESP_OK){
      delete[] sample_buffer_;
      sample_buffer_ = nullptr;
      if(log_) log_->error(TAG, "Failed to install I2S driver: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    err = i2s_set_pin(getPort(), &pin_config);
    if(err != ESP_OK){
      i2s_driver_uninstall(getPort());
      delete[] sample_buffer_;
      sample_buffer_ = nullptr;
      if(log_) log_->error(TAG, "Failed to set I2S pins: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    // Start I2S
    i2s_start(getPort());
    
    initialized_ = true;
    if(log_) log_->info(TAG, "INMP441 microphone initialized: WS=%d, BCK=%d, DATA=%d, rate=%lu Hz",
                        config_.ws_pin, config_.bck_pin, config_.data_pin, config_.sample_rate);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    i2s_stop(getPort());
    i2s_driver_uninstall(getPort());
    
    if(sample_buffer_){
      delete[] sample_buffer_;
      sample_buffer_ = nullptr;
    }
    
    initialized_ = false;
    if(log_) log_->info(TAG, "Microphone deinitialized");
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult update() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(getPort(), sample_buffer_, 
                             buffer_size_ * sizeof(int32_t),
                             &bytes_read, pdMS_TO_TICKS(100));
    
    if(err != ESP_OK || bytes_read == 0){
      return HalResult::TIMEOUT;
    }
    
    buffer_size_ = bytes_read / sizeof(int32_t);
    processBuffer();
    
    return HalResult::OK;
  }
  
  HalResult getData(MicrophoneData& data) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    data = current_data_;
    return HalResult::OK;
  }
  
  HalResult readBuffer(int32_t* buffer, size_t samples, size_t* samples_read) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!buffer || samples == 0) return HalResult::INVALID_PARAM;
    
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(getPort(), buffer, 
                             samples * sizeof(int32_t),
                             &bytes_read, pdMS_TO_TICKS(100));
    
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "Failed to read audio buffer: %d", err);
      return HalResult::HARDWARE_FAULT;
    }
    
    if(samples_read){
      *samples_read = bytes_read / sizeof(int32_t);
    }
    
    return HalResult::OK;
  }
  
  int32_t getCurrentSample() const override{
    return current_data_.current_sample;
  }
  
  int32_t getPeakAmplitude() const override{
    return current_data_.peak_amplitude;
  }
  
  float getRmsLevel() const override{
    return current_data_.rms_level;
  }
  
  float getDbLevel() const override{
    return current_data_.db_level;
  }
  
  bool isClipping() const override{
    return current_data_.clipping;
  }
  
  HalResult setGain(float gain) override{
    if(gain < 0.0f) return HalResult::INVALID_PARAM;
    gain_ = gain;
    if(log_) log_->debug(TAG, "Gain set to %.2f", gain);
    return HalResult::OK;
  }
  
  float getGain() const override{
    return gain_;
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_MICROPHONE_HPP_
