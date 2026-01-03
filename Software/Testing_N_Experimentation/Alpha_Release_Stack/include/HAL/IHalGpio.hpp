/*****************************************************************
 * File:      IHalGpio.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPIO Hardware Abstraction Layer interface.
 *    Provides platform-independent GPIO control including
 *    digital I/O, PWM output, and button input handling.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_GPIO_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_GPIO_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// GPIO Interface
// ============================================================

/** GPIO Hardware Abstraction Interface
 * 
 * Provides platform-independent GPIO control for digital pins.
 * All pin operations are abstracted to allow middleware to
 * work without direct hardware access.
 */
class IHalGpio{
public:
  virtual ~IHalGpio() = default;
  
  /** Initialize GPIO subsystem
   * @return HalResult::OK on success
   */
  virtual HalResult init() = 0;
  
  /** Configure pin mode
   * @param pin GPIO pin number
   * @param mode Pin mode (INPUT, OUTPUT, etc.)
   * @return HalResult::OK on success
   */
  virtual HalResult pinMode(gpio_pin_t pin, GpioMode mode) = 0;
  
  /** Read digital pin state
   * @param pin GPIO pin number
   * @return GpioState::HIGH or GpioState::LOW
   */
  virtual GpioState digitalRead(gpio_pin_t pin) = 0;
  
  /** Write digital pin state
   * @param pin GPIO pin number
   * @param state Pin state to write
   * @return HalResult::OK on success
   */
  virtual HalResult digitalWrite(gpio_pin_t pin, GpioState state) = 0;
};

// ============================================================
// PWM Interface
// ============================================================

/** PWM Hardware Abstraction Interface
 * 
 * Provides platform-independent PWM control for outputs
 * like fans, motors, or dimmable LEDs.
 */
class IHalPwm{
public:
  virtual ~IHalPwm() = default;
  
  /** Initialize PWM channel
   * @param pin GPIO pin number
   * @param frequency PWM frequency in Hz
   * @param resolution Resolution in bits (e.g., 8 for 0-255)
   * @return HalResult::OK on success
   */
  virtual HalResult init(gpio_pin_t pin, uint32_t frequency, uint8_t resolution) = 0;
  
  /** Set PWM duty cycle
   * @param pin GPIO pin number
   * @param duty Duty cycle (0 to max based on resolution)
   * @return HalResult::OK on success
   */
  virtual HalResult setDuty(gpio_pin_t pin, uint32_t duty) = 0;
  
  /** Set PWM duty cycle as percentage
   * @param pin GPIO pin number
   * @param percent Duty cycle as percentage (0.0 to 100.0)
   * @return HalResult::OK on success
   */
  virtual HalResult setDutyPercent(gpio_pin_t pin, float percent) = 0;
  
  /** Get current duty cycle
   * @param pin GPIO pin number
   * @return Current duty cycle value
   */
  virtual uint32_t getDuty(gpio_pin_t pin) = 0;
};

// ============================================================
// Button Interface
// ============================================================

/** Button state information */
struct ButtonState{
  bool pressed = false;          // Current pressed state
  bool just_pressed = false;     // True on rising edge (just pressed)
  bool just_released = false;    // True on falling edge (just released)
  timestamp_ms_t press_time = 0; // Time when button was pressed
  uint32_t press_count = 0;      // Total press count
};

/** Button configuration */
struct ButtonConfig{
  gpio_pin_t pin = 0;
  GpioMode mode = GpioMode::GPIO_INPUT_PULLUP;
  bool active_low = true;        // True if button is active low
  uint16_t debounce_ms = 50;     // Debounce time in ms
};

/** Button Hardware Abstraction Interface
 * 
 * Provides platform-independent button handling with
 * debouncing and edge detection.
 */
class IHalButton{
public:
  virtual ~IHalButton() = default;
  
  /** Initialize button
   * @param config Button configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const ButtonConfig& config) = 0;
  
  /** Update button state (call in main loop)
   * @return HalResult::OK on success
   */
  virtual HalResult update() = 0;
  
  /** Get button state
   * @return Current button state
   */
  virtual ButtonState getState() const = 0;
  
  /** Check if button is currently pressed
   * @return true if pressed
   */
  virtual bool isPressed() const = 0;
  
  /** Check if button was just pressed (rising edge)
   * @return true if just pressed this update
   */
  virtual bool justPressed() const = 0;
  
  /** Check if button was just released (falling edge)
   * @return true if just released this update
   */
  virtual bool justReleased() const = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_GPIO_HPP_
