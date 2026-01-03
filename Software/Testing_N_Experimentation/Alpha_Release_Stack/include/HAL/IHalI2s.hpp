/*****************************************************************
 * File:      IHalI2s.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    I2S Hardware Abstraction Layer interface.
 *    Provides platform-independent I2S audio input/output
 *    for microphones and audio devices.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_I2S_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_I2S_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// I2S Configuration
// ============================================================

/** I2S channel mode */
enum class I2sChannelMode : uint8_t{
  MONO_LEFT,
  MONO_RIGHT,
  STEREO
};

/** I2S data format */
enum class I2sDataFormat : uint8_t{
  FORMAT_16BIT,
  FORMAT_24BIT,
  FORMAT_32BIT
};

/** I2S mode (master/slave) */
enum class I2sMode : uint8_t{
  MASTER_TX,
  MASTER_RX,
  SLAVE_TX,
  SLAVE_RX
};

/** I2S configuration */
struct I2sConfig{
  uint8_t port = 0;
  gpio_pin_t bck_pin = 0;     // Bit clock (BCLK/SCK)
  gpio_pin_t ws_pin = 0;      // Word select (LRCLK)
  gpio_pin_t data_pin = 0;    // Data pin (SD/DOUT/DIN)
  uint32_t sample_rate = 16000;
  I2sMode mode = I2sMode::MASTER_RX;
  I2sChannelMode channel_mode = I2sChannelMode::MONO_LEFT;
  I2sDataFormat data_format = I2sDataFormat::FORMAT_32BIT;
  size_t buffer_size = 512;
};

// ============================================================
// I2S Interface
// ============================================================

/** I2S Hardware Abstraction Interface
 * 
 * Provides platform-independent I2S audio operations.
 * Used for microphone input and audio output.
 */
class IHalI2s{
public:
  virtual ~IHalI2s() = default;
  
  /** Initialize I2S peripheral
   * @param config I2S configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const I2sConfig& config) = 0;
  
  /** Deinitialize I2S peripheral
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if I2S is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Start I2S streaming
   * @return HalResult::OK on success
   */
  virtual HalResult start() = 0;
  
  /** Stop I2S streaming
   * @return HalResult::OK on success
   */
  virtual HalResult stop() = 0;
  
  /** Read audio samples (blocking)
   * @param buffer Buffer to store samples
   * @param samples Number of samples to read
   * @param samples_read Pointer to store actual samples read
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return HalResult::OK on success
   */
  virtual HalResult read(int32_t* buffer, size_t samples, size_t* samples_read, uint32_t timeout_ms = 0) = 0;
  
  /** Write audio samples (blocking)
   * @param buffer Buffer containing samples to write
   * @param samples Number of samples to write
   * @param samples_written Pointer to store actual samples written
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return HalResult::OK on success
   */
  virtual HalResult write(const int32_t* buffer, size_t samples, size_t* samples_written, uint32_t timeout_ms = 0) = 0;
  
  /** Get current sample rate
   * @return Sample rate in Hz
   */
  virtual uint32_t getSampleRate() const = 0;
  
  /** Set sample rate
   * @param sample_rate Sample rate in Hz
   * @return HalResult::OK on success
   */
  virtual HalResult setSampleRate(uint32_t sample_rate) = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_I2S_HPP_
