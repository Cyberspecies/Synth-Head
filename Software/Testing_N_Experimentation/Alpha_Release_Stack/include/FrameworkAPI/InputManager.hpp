/*****************************************************************
 * File:      InputManager.hpp
 * Category:  include/FrameworkAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Abstracts physical inputs (buttons, encoders) into high-level
 *    events. Handles debouncing, long press detection, double-click,
 *    and provides a clean event-based API.
 * 
 * Usage:
 *    InputManager input;
 *    input.init(50, 1000, 300);  // 50ms debounce, 1s long press, 300ms double-click
 *    
 *    // Configure button pins
 *    input.configureButton(ButtonId::BUTTON_A, 5, true);  // GPIO 5, pull-up
 *    
 *    // Register callbacks
 *    input.onEvent(ButtonId::BUTTON_A, InputEvent::CLICK, []() {
 *      printf("Button A clicked!\n");
 *    });
 *    
 *    input.onEvent(ButtonId::BUTTON_A, InputEvent::LONG_PRESS, []() {
 *      printf("Button A long pressed!\n");
 *    });
 *    
 *    // Or handle all events
 *    input.onAnyEvent([](const InputState& state) {
 *      printf("Button %d: event %d\n", state.button, state.event);
 *    });
 *****************************************************************/

#ifndef ARCOS_INCLUDE_FRAMEWORKAPI_INPUT_MANAGER_HPP_
#define ARCOS_INCLUDE_FRAMEWORKAPI_INPUT_MANAGER_HPP_

#include "FrameworkTypes.hpp"
#include <cstring>
#include <functional>

namespace arcos::framework {

/**
 * Simple event callback (no parameters)
 */
using SimpleCallback = std::function<void()>;

/**
 * Internal button state
 */
struct ButtonState {
  uint8_t pin = 0;
  bool configured = false;
  bool pull_up = true;
  bool current_state = false;
  bool last_state = false;
  bool pressed = false;
  uint32_t press_time = 0;
  uint32_t release_time = 0;
  uint32_t last_click_time = 0;
  bool long_press_fired = false;
  bool waiting_double_click = false;
};

/**
 * Event handler entry
 */
struct EventHandler {
  ButtonId button;
  InputEvent event;
  SimpleCallback callback;
  bool active;
};

/**
 * InputManager - Handles physical input abstraction
 * 
 * Converts raw GPIO states into high-level events like
 * click, double-click, long press, etc.
 */
class InputManager {
public:
  static constexpr size_t MAX_HANDLERS = 32;
  
  InputManager() = default;
  
  /**
   * Initialize input manager
   * @param debounce_ms Button debounce time
   * @param long_press_ms Time to trigger long press
   * @param double_click_ms Max time between clicks for double-click
   */
  Result init(uint32_t debounce_ms = 50, 
              uint32_t long_press_ms = 1000,
              uint32_t double_click_ms = 300) {
    debounce_ms_ = debounce_ms;
    long_press_ms_ = long_press_ms;
    double_click_ms_ = double_click_ms;
    handler_count_ = 0;
    
    memset(buttons_, 0, sizeof(buttons_));
    memset(handlers_, 0, sizeof(handlers_));
    
    initialized_ = true;
    return Result::OK;
  }
  
  /**
   * Configure a button pin
   * @param id Button identifier
   * @param pin GPIO pin number
   * @param pull_up True if using internal pull-up (active low)
   */
  Result configureButton(ButtonId id, uint8_t pin, bool pull_up = true) {
    if (!initialized_) return Result::NOT_INITIALIZED;
    if (static_cast<uint8_t>(id) >= static_cast<uint8_t>(ButtonId::MAX_BUTTONS)) {
      return Result::INVALID_PARAMETER;
    }
    
    ButtonState& btn = buttons_[static_cast<uint8_t>(id)];
    btn.pin = pin;
    btn.pull_up = pull_up;
    btn.configured = true;
    btn.current_state = pull_up;  // Default to not pressed
    btn.last_state = pull_up;
    
    // Note: Actual GPIO configuration should be done by HAL layer
    // This just stores the configuration
    
    return Result::OK;
  }
  
  /**
   * Register callback for specific button event
   * @param button Which button
   * @param event Which event type
   * @param callback Function to call
   */
  Result onEvent(ButtonId button, InputEvent event, SimpleCallback callback) {
    if (!initialized_) return Result::NOT_INITIALIZED;
    if (handler_count_ >= MAX_HANDLERS) return Result::BUFFER_FULL;
    
    EventHandler& h = handlers_[handler_count_++];
    h.button = button;
    h.event = event;
    h.callback = callback;
    h.active = true;
    
    return Result::OK;
  }
  
  /**
   * Register callback for any input event
   * @param callback Function to call with InputState
   */
  Result onAnyEvent(InputCallback callback) {
    any_event_callback_ = callback;
    return Result::OK;
  }
  
  /**
   * Update input states - call this regularly
   * @param dt_ms Time since last update
   * @param gpio_reader Function to read GPIO state (returns true if HIGH)
   */
  void update(uint32_t dt_ms, 
              std::function<bool(uint8_t)> gpio_reader = nullptr) {
    if (!initialized_) return;
    
    current_time_ += dt_ms;
    
    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonId::MAX_BUTTONS); i++) {
      ButtonState& btn = buttons_[i];
      if (!btn.configured) continue;
      
      // Read GPIO state (or simulate for testing)
      bool raw_state = gpio_reader ? gpio_reader(btn.pin) : btn.current_state;
      bool pressed = btn.pull_up ? !raw_state : raw_state;
      
      // Debounce
      if (pressed != btn.last_state) {
        btn.last_state = pressed;
        btn.press_time = current_time_;  // Start debounce timer
        continue;  // Wait for debounce
      }
      
      // Check if debounce period passed
      if (current_time_ - btn.press_time < debounce_ms_) {
        continue;
      }
      
      ButtonId id = static_cast<ButtonId>(i);
      
      // State changed after debounce
      if (pressed != btn.pressed) {
        btn.pressed = pressed;
        
        if (pressed) {
          // Button pressed
          btn.press_time = current_time_;
          btn.long_press_fired = false;
          fireEvent(id, InputEvent::PRESS);
        } else {
          // Button released
          uint32_t hold_duration = current_time_ - btn.press_time;
          btn.release_time = current_time_;
          
          fireEvent(id, InputEvent::RELEASE, hold_duration);
          
          // Check for click vs long press
          if (!btn.long_press_fired) {
            // Check for double-click
            if (btn.waiting_double_click && 
                (current_time_ - btn.last_click_time) < double_click_ms_) {
              fireEvent(id, InputEvent::DOUBLE_CLICK);
              btn.waiting_double_click = false;
            } else {
              // Single click - wait to see if double-click follows
              btn.waiting_double_click = true;
              btn.last_click_time = current_time_;
            }
          }
        }
      }
      
      // Check for pending single click (double-click timeout)
      if (btn.waiting_double_click && !btn.pressed &&
          (current_time_ - btn.last_click_time) >= double_click_ms_) {
        fireEvent(id, InputEvent::CLICK);
        btn.waiting_double_click = false;
      }
      
      // Check for long press while held
      if (btn.pressed && !btn.long_press_fired) {
        uint32_t hold_duration = current_time_ - btn.press_time;
        if (hold_duration >= long_press_ms_) {
          fireEvent(id, InputEvent::LONG_PRESS, hold_duration);
          btn.long_press_fired = true;
        }
      }
      
      // Fire HOLD events periodically while held after long press
      if (btn.pressed && btn.long_press_fired) {
        // Could add periodic HOLD events here if needed
      }
    }
  }
  
  /**
   * Manually inject a button state (for testing or virtual buttons)
   */
  void injectState(ButtonId id, bool pressed) {
    if (static_cast<uint8_t>(id) >= static_cast<uint8_t>(ButtonId::MAX_BUTTONS)) {
      return;
    }
    buttons_[static_cast<uint8_t>(id)].current_state = 
      buttons_[static_cast<uint8_t>(id)].pull_up ? !pressed : pressed;
  }
  
  /**
   * Check if a button is currently pressed
   */
  bool isPressed(ButtonId id) const {
    if (static_cast<uint8_t>(id) >= static_cast<uint8_t>(ButtonId::MAX_BUTTONS)) {
      return false;
    }
    return buttons_[static_cast<uint8_t>(id)].pressed;
  }
  
  /**
   * Get how long a button has been held (0 if not pressed)
   */
  uint32_t getHoldDuration(ButtonId id) const {
    if (static_cast<uint8_t>(id) >= static_cast<uint8_t>(ButtonId::MAX_BUTTONS)) {
      return 0;
    }
    const ButtonState& btn = buttons_[static_cast<uint8_t>(id)];
    if (!btn.pressed) return 0;
    return current_time_ - btn.press_time;
  }
  
private:
  void fireEvent(ButtonId button, InputEvent event, uint32_t duration = 0) {
    InputState state;
    state.button = button;
    state.event = event;
    state.timestamp = current_time_;
    state.duration_ms = duration;
    state.encoder_delta = 0;
    
    // Fire specific handlers
    for (size_t i = 0; i < handler_count_; i++) {
      EventHandler& h = handlers_[i];
      if (h.active && h.button == button && h.event == event) {
        h.callback();
      }
    }
    
    // Fire any-event handler
    if (any_event_callback_) {
      any_event_callback_(state);
    }
  }
  
  bool initialized_ = false;
  uint32_t debounce_ms_ = 50;
  uint32_t long_press_ms_ = 1000;
  uint32_t double_click_ms_ = 300;
  uint32_t current_time_ = 0;
  
  ButtonState buttons_[static_cast<uint8_t>(ButtonId::MAX_BUTTONS)];
  EventHandler handlers_[MAX_HANDLERS];
  size_t handler_count_ = 0;
  InputCallback any_event_callback_;
};

} // namespace arcos::framework

#endif // ARCOS_INCLUDE_FRAMEWORKAPI_INPUT_MANAGER_HPP_
