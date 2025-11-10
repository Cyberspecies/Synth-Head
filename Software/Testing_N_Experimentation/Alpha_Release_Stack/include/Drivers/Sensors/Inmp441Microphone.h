/*****************************************************************
 * File:      Inmp441Microphone.h
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    INMP441 I2S MEMS microphone driver with audio sampling
 *    and basic processing for ESP32-S3.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_INMP441_MICROPHONE_H_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_INMP441_MICROPHONE_H_

#include <Arduino.h>
#include <driver/i2s.h>

namespace sensors{

/** INMP441 microphone configuration */
struct Inmp441Config{
  uint8_t ws_pin;       // Word Select (LRCLK)
  uint8_t sck_pin;      // Serial Clock (BCLK)
  uint8_t sd_pin;       // Serial Data (DOUT)
  uint8_t lr_select_pin; // L/R Select (tie high for right, low for left)
  
  uint32_t sample_rate;  // Sample rate in Hz (default 16kHz)
  i2s_port_t i2s_port;   // I2S port number
  
  // Default configuration
  Inmp441Config()
    : ws_pin(42),
      sck_pin(40),
      sd_pin(2),
      lr_select_pin(41),
      sample_rate(16000),
      i2s_port(I2S_NUM_0){}
};

/** INMP441 audio data structure */
struct Inmp441AudioData{
  int32_t current_sample;      // Current audio sample value (smoothed)
  int32_t raw_sample;          // Raw unsmoothed sample
  int32_t peak_amplitude;      // Peak amplitude in current buffer
  float rms_level;             // RMS level (root mean square)
  float db_level;              // Decibel level (dB SPL approximation, smoothed)
  float db_level_raw;          // Raw unsmoothed dB level
  uint32_t sample_count;       // Number of samples processed
  bool clipping;               // True if clipping detected
};

/** INMP441 I2S MEMS microphone driver
 * 
 * Captures audio data from INMP441 microphone via I2S interface.
 * Provides basic audio level measurements and peak detection.
 */
class Inmp441Microphone{
private:
  static constexpr const char* TAG = "INMP441";
  static constexpr size_t BUFFER_SIZE = 512;
  static constexpr int32_t MAX_AMPLITUDE = 8388607; // 24-bit max
  static constexpr float SMOOTHING_FACTOR = 0.15f; // 15% new, 85% old (adjust for more/less smoothing)
  
  Inmp441Config config_;
  bool initialized_;
  
  int32_t buffer_[BUFFER_SIZE];
  Inmp441AudioData current_data_;
  
  // Smoothing state
  float smoothed_sample_;
  float smoothed_db_;
  
  /** Calculate RMS level from audio buffer
   * @param samples Audio sample buffer
   * @param count Number of samples
   * @return RMS level
   */
  float calculateRms(const int32_t* samples, size_t count);
  
  /** Convert RMS to decibels
   * @param rms RMS value
   * @return Approximate dB SPL
   */
  float rmsToDb(float rms);

public:
  /** Constructor with default configuration */
  Inmp441Microphone();
  
  /** Constructor with custom configuration
   * @param config Microphone configuration
   */
  Inmp441Microphone(const Inmp441Config& config);

  /** Initialize I2S microphone
   * @return true if initialization successful
   */
  bool init();

  /** Check if microphone is initialized
   * @return true if initialized
   */
  bool isInitialized() const{ return initialized_; }

  /** Update audio data (reads and processes audio buffer)
   * Call this frequently to keep data current
   * @return true if new data was processed
   */
  bool update();

  /** Get current audio data
   * @return Reference to current audio data
   */
  const Inmp441AudioData& getAudioData() const{ return current_data_; }

  /** Get current sample value
   * @return Current audio sample
   */
  int32_t getCurrentSample() const{ return current_data_.current_sample; }

  /** Get peak amplitude
   * @return Peak amplitude value
   */
  int32_t getPeakAmplitude() const{ return current_data_.peak_amplitude; }

  /** Get RMS level
   * @return RMS level
   */
  float getRmsLevel() const{ return current_data_.rms_level; }

  /** Get dB level
   * @return Approximate dB SPL level
   */
  float getDbLevel() const{ return current_data_.db_level; }

  /** Check if clipping detected
   * @return true if clipping occurred
   */
  bool isClipping() const{ return current_data_.clipping; }

  /** Print audio data to Serial
   * @param data Audio data to print
   */
  void printData(const Inmp441AudioData& data) const;
};

} // namespace sensors

// Include implementation
#include "Inmp441Microphone.impl.hpp"

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_INMP441_MICROPHONE_H_
