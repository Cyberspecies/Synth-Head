/*****************************************************************
 * File:      ButtonManager.hpp
 * Category:  UI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Button state management with press and hold detection.
 *    Handles 4 buttons: Up/Down (2/3), Set (1), Mode (4)
 * 
 * Usage:
 *    ButtonManager btnMgr;
 *    btnMgr.update(sensor_data);
 *    if(btnMgr.wasPressed(ButtonID::MODE)) { ... }
 *****************************************************************/

#ifndef BUTTON_MANAGER_HPP
#define BUTTON_MANAGER_HPP

#include <cstdint>
#include "Drivers/UART Comms/GpuUartBidirectional.hpp"

namespace arcos::ui {

enum class ButtonID : uint8_t {
  SET = 0,      // Button A (Button 1) - Set/Enter (not used in debug mode)
  UP = 1,       // Button B (Button 2) - Navigate up/previous
  DOWN = 2,     // Button C (Button 3) - Navigate down/next
  MODE = 3      // Button D (Button 4) - Mode switching
};

enum class ButtonEvent {
  NONE,
  PRESSED,      // Short press (released before hold threshold)
  HOLD_START,   // Hold threshold reached (still pressed)
  HOLD_REPEAT,  // Repeated while holding (for continuous scroll)
  RELEASED      // Released after any press
};

struct ButtonState {
  bool current = false;
  bool previous = false;
  uint32_t press_start_time = 0;
  bool hold_triggered = false;
  bool was_pressed_flag = false;
  bool was_held_flag = false;
};

class ButtonManager {
public:
  // Timing constants
  static constexpr uint32_t HOLD_THRESHOLD_MS = 2000;   // Time to trigger hold (2 seconds)
  static constexpr uint32_t HOLD_REPEAT_MS = 150;       // Repeat rate during hold
  static constexpr uint32_t DEBOUNCE_MS = 50;           // Debounce time

  /**
   * @brief Update button states from sensor data
   * @param data Sensor data payload containing button states
   * @param current_time_ms Current time in milliseconds
   */
  void update(const arcos::communication::SensorDataPayload& data, uint32_t current_time_ms) {
    m_current_time = current_time_ms;
    
    // Read button states (A=Set, B=Up, C=Down, D=Mode)
    updateButton(ButtonID::SET, data.getButtonA());
    updateButton(ButtonID::UP, data.getButtonB());
    updateButton(ButtonID::DOWN, data.getButtonC());
    updateButton(ButtonID::MODE, data.getButtonD());
  }

  /**
   * @brief Check if button was pressed (short press, released)
   * @param btn Button to check
   * @return true if button was pressed this frame
   */
  bool wasPressed(ButtonID btn) {
    return m_buttons[static_cast<uint8_t>(btn)].was_pressed_flag;
  }

  /**
   * @brief Check if button hold just started
   * @param btn Button to check
   * @return true if button just reached hold threshold
   */
  bool wasHeld(ButtonID btn) {
    return m_buttons[static_cast<uint8_t>(btn)].was_held_flag;
  }

  /**
   * @brief Check if button is currently being held down
   * @param btn Button to check
   * @return true if button is currently held
   */
  bool isHeld(ButtonID btn) {
    ButtonState& state = m_buttons[static_cast<uint8_t>(btn)];
    return state.current && state.hold_triggered;
  }

  /**
   * @brief Check if button is currently pressed (any duration)
   * @param btn Button to check
   * @return true if button is currently down
   */
  bool isPressed(ButtonID btn) {
    return m_buttons[static_cast<uint8_t>(btn)].current;
  }

  /**
   * @brief Get hold duration for currently held button
   * @param btn Button to check
   * @return milliseconds held (0 if not held)
   */
  uint32_t getHoldDuration(ButtonID btn) {
    ButtonState& state = m_buttons[static_cast<uint8_t>(btn)];
    if (state.current && state.press_start_time > 0) {
      return m_current_time - state.press_start_time;
    }
    return 0;
  }

  /**
   * @brief Check if hold repeat event should trigger (for continuous actions)
   * @param btn Button to check
   * @return true if repeat threshold reached
   */
  bool shouldRepeat(ButtonID btn) {
    ButtonState& state = m_buttons[static_cast<uint8_t>(btn)];
    if (!state.hold_triggered || !state.current) return false;
    
    uint32_t hold_duration = m_current_time - state.press_start_time - HOLD_THRESHOLD_MS;
    return (hold_duration % HOLD_REPEAT_MS) == 0;
  }

  /**
   * @brief Clear all button flags (call after processing events)
   */
  void clearFlags() {
    for (auto& btn : m_buttons) {
      btn.was_pressed_flag = false;
      btn.was_held_flag = false;
    }
  }

private:
  ButtonState m_buttons[4];
  uint32_t m_current_time = 0;

  void updateButton(ButtonID id, bool current_state) {
    ButtonState& state = m_buttons[static_cast<uint8_t>(id)];
    
    // Update state
    state.previous = state.current;
    state.current = current_state;
    
    // Rising edge - button just pressed
    if (state.current && !state.previous) {
      state.press_start_time = m_current_time;
      state.hold_triggered = false;
    }
    
    // Button held - check if hold threshold reached
    if (state.current && !state.hold_triggered) {
      uint32_t press_duration = m_current_time - state.press_start_time;
      if (press_duration >= HOLD_THRESHOLD_MS) {
        state.hold_triggered = true;
        state.was_held_flag = true;
      }
    }
    
    // Falling edge - button released
    if (!state.current && state.previous) {
      // Short press if released before hold threshold
      if (!state.hold_triggered && 
          (m_current_time - state.press_start_time) >= DEBOUNCE_MS) {
        state.was_pressed_flag = true;
      }
      
      // Reset state
      state.press_start_time = 0;
      state.hold_triggered = false;
    }
  }
};

} // namespace arcos::ui

#endif // BUTTON_MANAGER_HPP
