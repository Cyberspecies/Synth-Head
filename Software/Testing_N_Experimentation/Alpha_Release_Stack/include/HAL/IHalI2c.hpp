/*****************************************************************
 * File:      IHalI2c.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    I2C Hardware Abstraction Layer interface.
 *    Provides platform-independent I2C master communication
 *    for sensors and other I2C peripherals.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_I2C_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_I2C_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// I2C Configuration
// ============================================================

/** I2C configuration */
struct I2cConfig{
  uint8_t bus = 0;
  gpio_pin_t sda_pin = 0;
  gpio_pin_t scl_pin = 0;
  uint32_t frequency = 400000;  // 400 kHz default
};

// ============================================================
// I2C Interface
// ============================================================

/** I2C Hardware Abstraction Interface
 * 
 * Provides platform-independent I2C master operations.
 * All sensor drivers use this interface for communication.
 */
class IHalI2c{
public:
  virtual ~IHalI2c() = default;
  
  /** Initialize I2C bus
   * @param config I2C configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const I2cConfig& config) = 0;
  
  /** Deinitialize I2C bus
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if I2C is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Scan for device on bus
   * @param address 7-bit I2C address
   * @return HalResult::OK if device responds
   */
  virtual HalResult probe(i2c_addr_t address) = 0;
  
  /** Write data to device
   * @param address 7-bit I2C address
   * @param data Data buffer to write
   * @param length Number of bytes to write
   * @return HalResult::OK on success
   */
  virtual HalResult write(i2c_addr_t address, const uint8_t* data, size_t length) = 0;
  
  /** Read data from device
   * @param address 7-bit I2C address
   * @param buffer Buffer to store read data
   * @param length Number of bytes to read
   * @return HalResult::OK on success
   */
  virtual HalResult read(i2c_addr_t address, uint8_t* buffer, size_t length) = 0;
  
  /** Write then read (combined transaction)
   * @param address 7-bit I2C address
   * @param write_data Data buffer to write
   * @param write_length Number of bytes to write
   * @param read_buffer Buffer to store read data
   * @param read_length Number of bytes to read
   * @return HalResult::OK on success
   */
  virtual HalResult writeRead(i2c_addr_t address, 
                              const uint8_t* write_data, size_t write_length,
                              uint8_t* read_buffer, size_t read_length) = 0;
  
  /** Write to register
   * @param address 7-bit I2C address
   * @param reg Register address
   * @param data Data buffer to write
   * @param length Number of bytes to write
   * @return HalResult::OK on success
   */
  virtual HalResult writeRegister(i2c_addr_t address, uint8_t reg, const uint8_t* data, size_t length) = 0;
  
  /** Read from register
   * @param address 7-bit I2C address
   * @param reg Register address
   * @param buffer Buffer to store read data
   * @param length Number of bytes to read
   * @return HalResult::OK on success
   */
  virtual HalResult readRegister(i2c_addr_t address, uint8_t reg, uint8_t* buffer, size_t length) = 0;
  
  /** Write single byte to register
   * @param address 7-bit I2C address
   * @param reg Register address
   * @param value Byte value to write
   * @return HalResult::OK on success
   */
  virtual HalResult writeRegisterByte(i2c_addr_t address, uint8_t reg, uint8_t value) = 0;
  
  /** Read single byte from register
   * @param address 7-bit I2C address
   * @param reg Register address
   * @param value Pointer to store read byte
   * @return HalResult::OK on success
   */
  virtual HalResult readRegisterByte(i2c_addr_t address, uint8_t reg, uint8_t* value) = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_I2C_HPP_
