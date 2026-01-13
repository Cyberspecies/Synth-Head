/*****************************************************************
 * File:      HalTypes.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Common type definitions and data structures for the HAL API.
 *    These types are platform-independent and used throughout
 *    the HAL layer to provide consistent interfaces.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_HAL_TYPES_HPP_
#define ARCOS_INCLUDE_HAL_HAL_TYPES_HPP_

#include <stdint.h>
#include <stddef.h>

namespace arcos::hal{

// ============================================================
// Result Types
// ============================================================

/** HAL operation result codes */
enum class HalResult : uint8_t{
  OK = 0,              // Operation successful
  ERROR,               // Generic error
  TIMEOUT,             // Operation timed out
  BUSY,                // Resource is busy
  INVALID_PARAM,       // Invalid parameter
  NOT_INITIALIZED,     // Module not initialized
  NOT_SUPPORTED,       // Feature not supported
  BUFFER_FULL,         // Buffer is full
  BUFFER_EMPTY,        // Buffer is empty
  KEY_NOT_FOUND,       // Key/data not found in storage
  HARDWARE_FAULT,      // Hardware fault detected
  ALREADY_INITIALIZED, // Already initialized
  INVALID_STATE,       // Invalid state for operation
  NO_MEMORY,           // Memory allocation failed
  DEVICE_NOT_FOUND,    // Device not found
  READ_FAILED,         // Read operation failed
  WRITE_FAILED         // Write operation failed
};

// ============================================================
// Color Types
// ============================================================

/** RGB color structure (8-bit per channel) */
struct RGB{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  
  RGB() = default;
  RGB(uint8_t red, uint8_t green, uint8_t blue)
    : r(red), g(green), b(blue){}
};

/** RGBW color structure (8-bit per channel with white) */
struct RGBW{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t w = 0;
  
  RGBW() = default;
  RGBW(uint8_t red, uint8_t green, uint8_t blue, uint8_t white = 0)
    : r(red), g(green), b(blue), w(white){}
  
  /** Convert to RGB (ignoring white channel) */
  RGB toRGB() const{ return RGB{r, g, b}; }
};

// ============================================================
// Vector Types (for sensor data)
// ============================================================

/** 3D float vector */
struct Vec3f{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  
  Vec3f() = default;
  Vec3f(float x_, float y_, float z_)
    : x(x_), y(y_), z(z_){}
};

/** 3D integer vector */
struct Vec3i{
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
  
  Vec3i() = default;
  Vec3i(int32_t x_, int32_t y_, int32_t z_)
    : x(x_), y(y_), z(z_){}
};

// ============================================================
// Time Types
// ============================================================

/** Timestamp in milliseconds since boot */
using timestamp_ms_t = uint32_t;

/** Timestamp in microseconds since boot */
using timestamp_us_t = uint64_t;

// ============================================================
// GPIO Types
// ============================================================

/** GPIO pin number type */
using gpio_pin_t = uint8_t;

/** GPIO pin mode */
enum class GpioMode : uint8_t{
  GPIO_INPUT,
  GPIO_OUTPUT,
  GPIO_INPUT_PULLUP,
  GPIO_INPUT_PULLDOWN,
  GPIO_ANALOG
};

/** GPIO pin state */
enum class GpioState : uint8_t{
  GPIO_LOW = 0,
  GPIO_HIGH = 1
};

// ============================================================
// Communication Types
// ============================================================

/** I2C address type (7-bit) */
using i2c_addr_t = uint8_t;

/** UART port number */
using uart_port_t = uint8_t;

/** SPI bus number */
using spi_bus_t = uint8_t;

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_HAL_TYPES_HPP_
