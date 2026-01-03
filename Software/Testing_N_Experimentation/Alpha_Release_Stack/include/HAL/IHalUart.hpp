/*****************************************************************
 * File:      IHalUart.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    UART Hardware Abstraction Layer interface.
 *    Provides platform-independent UART/serial communication
 *    for inter-processor communication (CPU-GPU) and peripherals.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_UART_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_UART_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// UART Configuration
// ============================================================

/** UART parity options */
enum class UartParity : uint8_t{
  NONE,
  EVEN,
  ODD
};

/** UART stop bits */
enum class UartStopBits : uint8_t{
  ONE,
  ONE_HALF,
  TWO
};

/** UART configuration */
struct UartConfig{
  uart_port_t port = 0;
  gpio_pin_t tx_pin = 0;
  gpio_pin_t rx_pin = 0;
  uint32_t baud_rate = 115200;
  uint8_t data_bits = 8;
  UartParity parity = UartParity::NONE;
  UartStopBits stop_bits = UartStopBits::ONE;
  size_t tx_buffer_size = 1024;
  size_t rx_buffer_size = 1024;
};

// ============================================================
// UART Interface
// ============================================================

/** UART Hardware Abstraction Interface
 * 
 * Provides platform-independent UART communication.
 * Supports both blocking and non-blocking operations.
 */
class IHalUart{
public:
  virtual ~IHalUart() = default;
  
  /** Initialize UART with configuration
   * @param config UART configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const UartConfig& config) = 0;
  
  /** Deinitialize UART
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if UART is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Get number of bytes available to read
   * @return Number of bytes in receive buffer
   */
  virtual size_t available() const = 0;
  
  /** Read single byte (blocking)
   * @param byte Pointer to store read byte
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return HalResult::OK on success
   */
  virtual HalResult readByte(uint8_t* byte, uint32_t timeout_ms = 0) = 0;
  
  /** Read multiple bytes (blocking)
   * @param buffer Buffer to store read data
   * @param length Number of bytes to read
   * @param bytes_read Pointer to store actual bytes read
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return HalResult::OK on success
   */
  virtual HalResult read(uint8_t* buffer, size_t length, size_t* bytes_read, uint32_t timeout_ms = 0) = 0;
  
  /** Write single byte
   * @param byte Byte to write
   * @return HalResult::OK on success
   */
  virtual HalResult writeByte(uint8_t byte) = 0;
  
  /** Write multiple bytes
   * @param data Data buffer to write
   * @param length Number of bytes to write
   * @param bytes_written Pointer to store actual bytes written (optional)
   * @return HalResult::OK on success
   */
  virtual HalResult write(const uint8_t* data, size_t length, size_t* bytes_written = nullptr) = 0;
  
  /** Flush transmit buffer (wait for all data to be sent)
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return HalResult::OK on success
   */
  virtual HalResult flush(uint32_t timeout_ms = 0) = 0;
  
  /** Clear receive buffer
   * @return HalResult::OK on success
   */
  virtual HalResult clearRxBuffer() = 0;
  
  /** Clear transmit buffer
   * @return HalResult::OK on success
   */
  virtual HalResult clearTxBuffer() = 0;
  
  /** Get current baud rate
   * @return Baud rate in bps
   */
  virtual uint32_t getBaudRate() const = 0;
  
  /** Set baud rate (runtime change)
   * @param baud_rate New baud rate in bps
   * @return HalResult::OK on success
   */
  virtual HalResult setBaudRate(uint32_t baud_rate) = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_UART_HPP_
