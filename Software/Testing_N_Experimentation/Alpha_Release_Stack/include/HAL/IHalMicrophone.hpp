/*****************************************************************
 * File:      IHalMicrophone.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Microphone Sensor Hardware Abstraction Layer interface.
 *    Provides platform-independent access to I2S microphones
 *    like INMP441 for audio input and level monitoring.
 * 
 * Note:
 *    This is a sensor HAL interface. The middleware layer will
 *    use this to build higher-level audio processing services.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_MICROPHONE_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_MICROPHONE_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// Microphone Data Structures
// ============================================================

/** Microphone audio data */
struct MicrophoneData{
  int32_t current_sample = 0;     // Current audio sample value
  int32_t peak_amplitude = 0;     // Peak amplitude in buffer
  float rms_level = 0.0f;         // RMS level (0.0 to 1.0)
  float db_level = 0.0f;          // Decibel level (dB SPL approx)
  bool clipping = false;          // True if clipping detected
  
  // Timestamp when data was read
  timestamp_ms_t timestamp = 0;
};

/** Microphone configuration */
struct MicrophoneConfig{
  gpio_pin_t ws_pin = 0;          // Word select (LRCLK)
  gpio_pin_t bck_pin = 0;         // Bit clock (BCLK)
  gpio_pin_t data_pin = 0;        // Data pin (DOUT)
  uint32_t sample_rate = 16000;   // Sample rate in Hz
  uint8_t i2s_port = 0;           // I2S port number
  size_t buffer_size = 512;       // Buffer size in samples
};

// ============================================================
// Microphone Interface
// ============================================================

/** Microphone Hardware Abstraction Interface
 * 
 * Provides platform-independent access to I2S microphones.
 * Supports audio sampling and level monitoring.
 */
class IHalMicrophone{
public:
  virtual ~IHalMicrophone() = default;
  
  /** Initialize microphone
   * @param config Microphone configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const MicrophoneConfig& config) = 0;
  
  /** Deinitialize microphone
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if microphone is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Update audio data (call frequently)
   * @return HalResult::OK if new data available
   */
  virtual HalResult update() = 0;
  
  /** Get current audio data
   * @param data Reference to MicrophoneData to fill
   * @return HalResult::OK on success
   */
  virtual HalResult getData(MicrophoneData& data) = 0;
  
  /** Read raw audio buffer
   * @param buffer Buffer to store samples
   * @param samples Number of samples to read
   * @param samples_read Pointer to store actual samples read
   * @return HalResult::OK on success
   */
  virtual HalResult readBuffer(int32_t* buffer, size_t samples, size_t* samples_read) = 0;
  
  /** Get current sample value
   * @return Current audio sample
   */
  virtual int32_t getCurrentSample() const = 0;
  
  /** Get peak amplitude
   * @return Peak amplitude value
   */
  virtual int32_t getPeakAmplitude() const = 0;
  
  /** Get RMS level
   * @return RMS level (0.0 to 1.0)
   */
  virtual float getRmsLevel() const = 0;
  
  /** Get dB level
   * @return Approximate dB SPL level
   */
  virtual float getDbLevel() const = 0;
  
  /** Check if clipping detected
   * @return true if clipping
   */
  virtual bool isClipping() const = 0;
  
  /** Set gain/volume
   * @param gain Gain multiplier (1.0 = unity)
   * @return HalResult::OK on success
   */
  virtual HalResult setGain(float gain) = 0;
  
  /** Get current gain
   * @return Current gain multiplier
   */
  virtual float getGain() const = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_MICROPHONE_HPP_
