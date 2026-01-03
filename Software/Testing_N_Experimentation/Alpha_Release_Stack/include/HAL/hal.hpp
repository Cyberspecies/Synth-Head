/*****************************************************************
 * File:      Hal.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Master HAL header that includes all HAL interfaces.
 *    Include this single header to access all HAL functionality.
 * 
 * Architecture:
 *    HAL Layer (this) -> Middleware Layer -> Application Layer
 *    
 *    The HAL layer provides platform-independent interfaces that
 *    abstract hardware access. Middleware MUST NOT use platform-
 *    specific code or directly access registers/sensors.
 *    
 *    Hardware implementations of these interfaces will be in
 *    platform-specific directories and injected at runtime.
 * 
 * Usage:
 *    #include "HAL/Hal.hpp"
 *    
 *    // Use interfaces via dependency injection:
 *    void initSystem(arcos::hal::IHalGpio* gpio,
 *                    arcos::hal::IHalI2c* i2c,
 *                    arcos::hal::IHalUart* uart);
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_HAL_HPP_
#define ARCOS_INCLUDE_HAL_HAL_HPP_

// ============================================================
// Core Type Definitions
// ============================================================
#include "HalTypes.hpp"

// ============================================================
// Logging and Error Handling
// ============================================================
#include "IHalLog.hpp"       // Logging and error handling

// ============================================================
// Communication Interfaces
// ============================================================
#include "IHalGpio.hpp"      // GPIO, PWM, Button
#include "IHalUart.hpp"      // UART serial communication
#include "IHalI2c.hpp"       // I2C communication
#include "IHalSpi.hpp"       // SPI communication
#include "IHalI2s.hpp"       // I2S audio interface

// ============================================================
// System Interfaces
// ============================================================
#include "IHalTimer.hpp"     // System timers and delays

// ============================================================
// Sensor Interfaces
// ============================================================
#include "IHalImu.hpp"           // IMU (accelerometer, gyro, mag)
#include "IHalEnvironmental.hpp" // Environmental (temp, humidity, pressure)
#include "IHalGps.hpp"           // GPS positioning
#include "IHalMicrophone.hpp"    // Microphone audio input

// ============================================================
// Output Interfaces
// ============================================================
#include "IHalLedStrip.hpp"  // Addressable LED strips
#include "IHalDisplay.hpp"   // Displays (HUB75, OLED)

// ============================================================
// Storage Interfaces
// ============================================================
#include "IHalStorage.hpp"   // SD card, flash storage

// ============================================================
// Pin Mapping Constants
// ============================================================

namespace arcos::hal::pins{

/** CPU Pin Definitions (COM 15) */
namespace cpu{
  // I2C Bus
  constexpr gpio_pin_t I2C_SDA = 9;
  constexpr gpio_pin_t I2C_SCL = 10;
  
  // LED Strips
  constexpr gpio_pin_t LED_STRIP_0 = 16;
  constexpr gpio_pin_t LED_LEFT_FIN = 18;
  constexpr gpio_pin_t LED_TONGUE = 8;
  constexpr gpio_pin_t LED_STRIP_3 = 39;
  constexpr gpio_pin_t LED_RIGHT_FIN = 38;
  constexpr gpio_pin_t LED_SCALE = 37;
  
  // LED Strip Counts
  constexpr uint16_t LED_LEFT_FIN_COUNT = 13;
  constexpr uint16_t LED_RIGHT_FIN_COUNT = 13;
  constexpr uint16_t LED_TONGUE_COUNT = 9;
  constexpr uint16_t LED_SCALE_COUNT = 14;
  
  // Buttons
  constexpr gpio_pin_t BUTTON_A = 5;
  constexpr gpio_pin_t BUTTON_B = 6;
  constexpr gpio_pin_t BUTTON_C = 7;
  constexpr gpio_pin_t BUTTON_D = 15;
  
  // Fans (PWM)
  constexpr gpio_pin_t FAN_1 = 17;
  constexpr gpio_pin_t FAN_2 = 36;
  
  // SD Card (SPI)
  constexpr gpio_pin_t SD_MISO = 14;
  constexpr gpio_pin_t SD_MOSI = 47;
  constexpr gpio_pin_t SD_CLK = 21;
  constexpr gpio_pin_t SD_CS = 48;
  
  // GPS (UART)
  constexpr gpio_pin_t GPS_TX = 43;
  constexpr gpio_pin_t GPS_RX = 44;
  
  // CPU-GPU UART
  constexpr gpio_pin_t UART_RX = 11;
  constexpr gpio_pin_t UART_TX = 12;
  
  // I2S Microphone
  constexpr gpio_pin_t MIC_DOUT = 2;
  constexpr gpio_pin_t MIC_CLK = 40;
  constexpr gpio_pin_t MIC_LR_SEL = 41;
  constexpr gpio_pin_t MIC_WS = 42;
}

/** GPU Pin Definitions (COM 16) */
namespace gpu{
  // CPU-GPU UART (reversed from CPU perspective)
  constexpr gpio_pin_t UART_TX = 12;
  constexpr gpio_pin_t UART_RX = 13;
  
  // HUB75 display pins defined in platform-specific implementation
}

/** I2C Device Addresses */
namespace i2c_addr{
  constexpr i2c_addr_t ICM20948 = 0x68;  // IMU
  constexpr i2c_addr_t BME280 = 0x76;    // Environmental (alt: 0x77)
  constexpr i2c_addr_t OLED_SH1107 = 0x3C;
}

/** Default Communication Settings */
namespace defaults{
  constexpr uint32_t CPU_GPU_BAUD = 10000000;  // 10 Mbps
  constexpr uint32_t GPS_BAUD = 9600;
  constexpr uint32_t I2C_FREQ = 400000;        // 400 kHz
}

} // namespace arcos::hal::pins

#endif // ARCOS_INCLUDE_HAL_HAL_HPP_
