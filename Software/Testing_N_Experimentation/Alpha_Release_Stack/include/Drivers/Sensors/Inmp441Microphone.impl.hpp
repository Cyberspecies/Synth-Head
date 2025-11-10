/*****************************************************************
 * File:      Inmp441Microphone.impl.hpp
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Implementation file for INMP441 I2S MEMS microphone
 *    with audio capture and level processing.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_INMP441_MICROPHONE_IMPL_HPP_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_INMP441_MICROPHONE_IMPL_HPP_

#include <cmath>

namespace sensors{

Inmp441Microphone::Inmp441Microphone()
  : initialized_(false),
    smoothed_sample_(0.0f),
    smoothed_db_(-60.0f){
  config_ = Inmp441Config();
  current_data_.current_sample = 0;
  current_data_.raw_sample = 0;
  current_data_.peak_amplitude = 0;
  current_data_.rms_level = 0.0f;
  current_data_.db_level = -60.0f;
  current_data_.db_level_raw = -60.0f;
  current_data_.sample_count = 0;
  current_data_.clipping = false;
}

Inmp441Microphone::Inmp441Microphone(const Inmp441Config& config)
  : config_(config),
    initialized_(false),
    smoothed_sample_(0.0f),
    smoothed_db_(-60.0f){
  current_data_.current_sample = 0;
  current_data_.raw_sample = 0;
  current_data_.peak_amplitude = 0;
  current_data_.rms_level = 0.0f;
  current_data_.db_level = -60.0f;
  current_data_.db_level_raw = -60.0f;
  current_data_.sample_count = 0;
  current_data_.clipping = false;
}

bool Inmp441Microphone::init(){
  if(initialized_){
    Serial.println("[INMP441] Already initialized");
    return true;
  }

  Serial.printf("[INMP441] Initializing I2S microphone\n");
  Serial.printf("  WS Pin: %d\n", config_.ws_pin);
  Serial.printf("  SCK Pin: %d\n", config_.sck_pin);
  Serial.printf("  SD Pin: %d\n", config_.sd_pin);
  Serial.printf("  LR Select Pin: %d\n", config_.lr_select_pin);
  Serial.printf("  Sample Rate: %lu Hz\n", config_.sample_rate);

  // Configure L/R select pin (high for right channel)
  pinMode(config_.lr_select_pin, OUTPUT);
  digitalWrite(config_.lr_select_pin, HIGH);

  // Configure I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = config_.sample_rate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  // Install I2S driver
  esp_err_t err = i2s_driver_install(config_.i2s_port, &i2s_config, 0, nullptr);
  if(err != ESP_OK){
    Serial.printf("[INMP441] Failed to install I2S driver: %d\n", err);
    return false;
  }

  // Configure I2S pins
  i2s_pin_config_t pin_config = {
    .bck_io_num = config_.sck_pin,
    .ws_io_num = config_.ws_pin,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = config_.sd_pin
  };

  err = i2s_set_pin(config_.i2s_port, &pin_config);
  if(err != ESP_OK){
    Serial.printf("[INMP441] Failed to set I2S pins: %d\n", err);
    i2s_driver_uninstall(config_.i2s_port);
    return false;
  }

  // Start I2S
  i2s_start(config_.i2s_port);

  initialized_ = true;
  Serial.println("[INMP441] Initialization complete");
  
  return true;
}

bool Inmp441Microphone::update(){
  if(!initialized_){
    return false;
  }

  // Read single sample from I2S (non-blocking)
  size_t bytes_read = 0;
  int32_t sample_raw = 0;
  esp_err_t result = i2s_read(config_.i2s_port, &sample_raw, 
                               sizeof(int32_t), 
                               &bytes_read, 0);
  
  if(result != ESP_OK || bytes_read == 0){
    return false;
  }

  // Shift from 32-bit to 24-bit
  int32_t sample = sample_raw >> 8;
  
  // Store raw sample
  current_data_.raw_sample = sample;
  
  // Apply exponential smoothing to sample
  smoothed_sample_ = (SMOOTHING_FACTOR * static_cast<float>(sample)) + 
                     ((1.0f - SMOOTHING_FACTOR) * smoothed_sample_);
  current_data_.current_sample = static_cast<int32_t>(smoothed_sample_);
  
  // Update peak if current sample is higher
  int32_t abs_sample = abs(sample);
  if(abs_sample > current_data_.peak_amplitude){
    current_data_.peak_amplitude = abs_sample;
  }
  
  // Check for clipping
  current_data_.clipping = (abs_sample >= MAX_AMPLITUDE * 0.95f);
  
  // Calculate raw instantaneous level (normalized)
  float normalized = static_cast<float>(abs_sample) / static_cast<float>(MAX_AMPLITUDE);
  current_data_.rms_level = normalized;
  current_data_.db_level_raw = rmsToDb(normalized);
  
  // Apply exponential smoothing to dB level
  smoothed_db_ = (SMOOTHING_FACTOR * current_data_.db_level_raw) + 
                 ((1.0f - SMOOTHING_FACTOR) * smoothed_db_);
  current_data_.db_level = smoothed_db_;
  
  current_data_.sample_count++;
  
  return true;
}

float Inmp441Microphone::calculateRms(const int32_t* samples, size_t count){
  if(count == 0) return 0.0f;
  
  float sum = 0.0f;
  for(size_t i = 0; i < count; i++){
    int32_t sample = samples[i] >> 8; // Shift to 24-bit
    float normalized = static_cast<float>(sample) / static_cast<float>(MAX_AMPLITUDE);
    sum += normalized * normalized;
  }
  
  return sqrtf(sum / count);
}

float Inmp441Microphone::rmsToDb(float rms){
  if(rms <= 0.0f) return -100.0f;
  
  // Convert RMS to dB (reference: 1.0 = 0dB)
  float db = 20.0f * log10f(rms);
  
  // Clamp to reasonable range
  if(db < -100.0f) db = -100.0f;
  if(db > 0.0f) db = 0.0f;
  
  return db;
}

void Inmp441Microphone::printData(const Inmp441AudioData& data) const{
  Serial.printf("Mic: Sample=%8d Peak=%7d Level=%6.4f dB=%6.2f %s\n",
                data.current_sample,
                data.peak_amplitude,
                data.rms_level,
                data.db_level,
                data.clipping ? "[CLIP!]" : "      ");
}

} // namespace sensors

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_INMP441_MICROPHONE_IMPL_HPP_
