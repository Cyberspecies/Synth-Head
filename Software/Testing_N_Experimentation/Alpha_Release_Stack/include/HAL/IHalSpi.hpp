/*****************************************************************
 * File:      IHalSpi.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    SPI Hardware Abstraction Layer interface.
 *    Provides platform-independent SPI master communication
 *    for SD cards and other SPI peripherals.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_SPI_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_SPI_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// SPI Configuration
// ============================================================

/** SPI mode (clock polarity and phase) */
enum class SpiMode : uint8_t{
  MODE_0,  // CPOL=0, CPHA=0
  MODE_1,  // CPOL=0, CPHA=1
  MODE_2,  // CPOL=1, CPHA=0
  MODE_3   // CPOL=1, CPHA=1
};

/** SPI bit order */
enum class SpiBitOrder : uint8_t{
  MSB_FIRST,
  LSB_FIRST
};

/** SPI configuration */
struct SpiConfig{
  spi_bus_t bus = 0;
  gpio_pin_t mosi_pin = 0;
  gpio_pin_t miso_pin = 0;
  gpio_pin_t sck_pin = 0;
  gpio_pin_t cs_pin = 0;           // Chip select (can be managed manually)
  uint32_t frequency = 1000000;    // 1 MHz default
  SpiMode mode = SpiMode::MODE_0;
  SpiBitOrder bit_order = SpiBitOrder::MSB_FIRST;
};

// ============================================================
// SPI Interface
// ============================================================

/** SPI Hardware Abstraction Interface
 * 
 * Provides platform-independent SPI master operations.
 * Used for SD card access and other SPI peripherals.
 */
class IHalSpi{
public:
  virtual ~IHalSpi() = default;
  
  /** Initialize SPI bus
   * @param config SPI configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const SpiConfig& config) = 0;
  
  /** Deinitialize SPI bus
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if SPI is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Begin SPI transaction (assert CS)
   * @return HalResult::OK on success
   */
  virtual HalResult beginTransaction() = 0;
  
  /** End SPI transaction (deassert CS)
   * @return HalResult::OK on success
   */
  virtual HalResult endTransaction() = 0;
  
  /** Transfer single byte (full duplex)
   * @param tx_byte Byte to transmit
   * @param rx_byte Pointer to store received byte (optional)
   * @return HalResult::OK on success
   */
  virtual HalResult transfer(uint8_t tx_byte, uint8_t* rx_byte = nullptr) = 0;
  
  /** Transfer buffer (full duplex)
   * @param tx_buffer Transmit buffer (nullptr for read-only)
   * @param rx_buffer Receive buffer (nullptr for write-only)
   * @param length Number of bytes to transfer
   * @return HalResult::OK on success
   */
  virtual HalResult transferBuffer(const uint8_t* tx_buffer, uint8_t* rx_buffer, size_t length) = 0;
  
  /** Write buffer (transmit only)
   * @param data Data buffer to write
   * @param length Number of bytes to write
   * @return HalResult::OK on success
   */
  virtual HalResult write(const uint8_t* data, size_t length) = 0;
  
  /** Read buffer (receive only, sends 0x00 or 0xFF)
   * @param buffer Buffer to store read data
   * @param length Number of bytes to read
   * @return HalResult::OK on success
   */
  virtual HalResult read(uint8_t* buffer, size_t length) = 0;
  
  /** Set SPI frequency
   * @param frequency Frequency in Hz
   * @return HalResult::OK on success
   */
  virtual HalResult setFrequency(uint32_t frequency) = 0;
  
  /** Get current SPI frequency
   * @return Frequency in Hz
   */
  virtual uint32_t getFrequency() const = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_SPI_HPP_
