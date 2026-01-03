/*****************************************************************
 * File:      Esp32Hal.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Master header for all ESP32 HAL implementations.
 *    Include this to get all ESP32-specific HAL classes.
 *    This provides the "Base System API" middle layer.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_HPP_

// ============================================================
// Core HAL Implementations
// ============================================================
#include "Esp32HalLog.hpp"
#include "Esp32HalTimer.hpp"
#include "Esp32HalGpio.hpp"

// ============================================================
// Communication HAL Implementations
// ============================================================
#include "Esp32HalUart.hpp"
#include "Esp32HalI2c.hpp"
#include "Esp32HalSpi.hpp"
#include "Esp32HalI2s.hpp"

// ============================================================
// Sensor HAL Implementations
// ============================================================
#include "Esp32HalSensors.hpp"      // IMU & Environmental
#include "Esp32HalGps.hpp"
#include "Esp32HalMicrophone.hpp"

// ============================================================
// Output HAL Implementations
// ============================================================
#include "Esp32HalLedStrip.hpp"
#include "Esp32HalDisplay.hpp"      // HUB75 & OLED

// ============================================================
// Storage HAL Implementations
// ============================================================
#include "Esp32HalStorage.hpp"

namespace arcos::hal::esp32{

/** ESP32 HAL Factory - creates all HAL instances for a device
 * 
 * This provides a convenient factory for creating and initializing
 * all HAL components for a complete ESP32 system.
 */
struct Esp32HalFactory{
  // Core
  Esp32HalLog log;
  Esp32HalErrorHandler error_handler;
  Esp32HalSystemTimer timer;
  Esp32HalGpio gpio;
  Esp32HalPwm pwm;
  
  // Communication
  Esp32HalI2c i2c;
  Esp32HalUart uart1;
  Esp32HalUart uart2;
  Esp32HalSpi spi;
  Esp32HalI2s i2s;
  
  // Sensors
  Esp32HalImu imu;
  Esp32HalEnvironmental env;
  Esp32HalGps gps;
  Esp32HalMicrophone mic;
  
  // Storage
  Esp32HalStorage storage;
  
  Esp32HalFactory() 
    : error_handler(&log)
    , gpio(&log)
    , pwm(&log)
    , i2c(&log)
    , uart1(&log)
    , uart2(&log)
    , spi(&log)
    , i2s(&log)
    , imu(&i2c, &log)
    , env(&i2c, &log)
    , gps(&log)
    , mic(&log)
    , storage(&log)
  {}
  
  /** Initialize core HAL components */
  HalResult initCore(){
    HalResult result = log.init(LogLevel::DEBUG);
    if(result != HalResult::OK) return result;
    
    result = error_handler.init();
    if(result != HalResult::OK) return result;
    
    result = gpio.init();
    if(result != HalResult::OK) return result;
    
    return HalResult::OK;
  }
  
  /** Initialize I2C bus with config */
  HalResult initI2c(const I2cConfig& config){
    return i2c.init(config);
  }
  
  /** Initialize UART port */
  HalResult initUart(uint8_t port, const UartConfig& config){
    if(port == 1) return uart1.init(config);
    if(port == 2) return uart2.init(config);
    return HalResult::INVALID_PARAM;
  }
  
  /** Initialize sensors (requires I2C to be initialized first) */
  HalResult initSensors(const ImuConfig& imu_config, const EnvironmentalConfig& env_config){
    HalResult result = imu.init(imu_config);
    if(result != HalResult::OK){
      log.warn("HAL", "IMU init failed: %d", (int)result);
    }
    
    result = env.init(env_config);
    if(result != HalResult::OK){
      log.warn("HAL", "ENV init failed: %d", (int)result);
    }
    
    return HalResult::OK;
  }
};

/** Convenience type aliases */
using HalFactory = Esp32HalFactory;
using HalLog = Esp32HalLog;
using HalTimer = Esp32HalSystemTimer;
using HalGpio = Esp32HalGpio;
using HalPwm = Esp32HalPwm;
using HalI2c = Esp32HalI2c;
using HalUart = Esp32HalUart;
using HalSpi = Esp32HalSpi;
using HalI2s = Esp32HalI2s;
using HalImu = Esp32HalImu;
using HalEnvironmental = Esp32HalEnvironmental;
using HalGps = Esp32HalGps;
using HalMicrophone = Esp32HalMicrophone;
using HalStorage = Esp32HalStorage;
using HalFile = Esp32HalFile;
using HalHub75Display = Esp32HalHub75Display;
using HalOledDisplay = Esp32HalOledDisplay;
using HalLedStrip = Esp32HalLedStrip;
using HalButton = Esp32HalButton;

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_HPP_
