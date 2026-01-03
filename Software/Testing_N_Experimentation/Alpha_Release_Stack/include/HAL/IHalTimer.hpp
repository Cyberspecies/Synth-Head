/*****************************************************************
 * File:      IHalTimer.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Timer Hardware Abstraction Layer interface.
 *    Provides platform-independent system timing functions
 *    including delays, timestamps, and periodic callbacks.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_TIMER_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_TIMER_HPP_

#include "HalTypes.hpp"
#include <functional>

namespace arcos::hal{

// ============================================================
// Timer Callback Type
// ============================================================

/** Timer callback function type */
using TimerCallback = std::function<void()>;

// ============================================================
// System Timer Interface
// ============================================================

/** System Timer Hardware Abstraction Interface
 * 
 * Provides platform-independent timing functions.
 * Used for delays, timestamps, and scheduling.
 */
class IHalSystemTimer{
public:
  virtual ~IHalSystemTimer() = default;
  
  /** Get milliseconds since boot
   * @return Milliseconds since system start
   */
  virtual timestamp_ms_t millis() const = 0;
  
  /** Get microseconds since boot
   * @return Microseconds since system start
   */
  virtual timestamp_us_t micros() const = 0;
  
  /** Delay for specified milliseconds (blocking)
   * @param ms Milliseconds to delay
   */
  virtual void delayMs(uint32_t ms) = 0;
  
  /** Delay for specified microseconds (blocking)
   * @param us Microseconds to delay
   */
  virtual void delayUs(uint32_t us) = 0;
  
  /** Yield to other tasks (RTOS aware)
   */
  virtual void yield() = 0;
};

// ============================================================
// Hardware Timer Interface
// ============================================================

/** Hardware Timer Hardware Abstraction Interface
 * 
 * Provides platform-independent hardware timer functions
 * for periodic interrupts and precise timing.
 */
class IHalHardwareTimer{
public:
  virtual ~IHalHardwareTimer() = default;
  
  /** Initialize hardware timer
   * @param timer_id Timer peripheral ID
   * @param period_us Timer period in microseconds
   * @return HalResult::OK on success
   */
  virtual HalResult init(uint8_t timer_id, uint32_t period_us) = 0;
  
  /** Deinitialize hardware timer
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Start timer
   * @return HalResult::OK on success
   */
  virtual HalResult start() = 0;
  
  /** Stop timer
   * @return HalResult::OK on success
   */
  virtual HalResult stop() = 0;
  
  /** Set timer callback
   * @param callback Function to call on timer event
   * @return HalResult::OK on success
   */
  virtual HalResult setCallback(TimerCallback callback) = 0;
  
  /** Set timer period
   * @param period_us Period in microseconds
   * @return HalResult::OK on success
   */
  virtual HalResult setPeriod(uint32_t period_us) = 0;
  
  /** Get timer period
   * @return Period in microseconds
   */
  virtual uint32_t getPeriod() const = 0;
  
  /** Check if timer is running
   * @return true if running
   */
  virtual bool isRunning() const = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_TIMER_HPP_
