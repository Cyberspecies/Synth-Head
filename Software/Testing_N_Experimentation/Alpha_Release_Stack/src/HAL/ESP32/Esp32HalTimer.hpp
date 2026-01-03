/*****************************************************************
 * File:      Esp32HalTimer.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of HAL timer interface using
 *    Arduino timing functions.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_TIMER_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_TIMER_HPP_

#include "HAL/IHalTimer.hpp"
#include <Arduino.h>

namespace arcos::hal::esp32{

/** ESP32 System Timer Implementation */
class Esp32HalSystemTimer : public IHalSystemTimer{
public:
  Esp32HalSystemTimer() = default;
  
  timestamp_ms_t millis() const override{
    return ::millis();
  }
  
  timestamp_us_t micros() const override{
    return ::micros();
  }
  
  void delayMs(uint32_t ms) override{
    ::delay(ms);
  }
  
  void delayUs(uint32_t us) override{
    ::delayMicroseconds(us);
  }
  
  void yield() override{
    ::yield();
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_TIMER_HPP_
