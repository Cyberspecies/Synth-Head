/*****************************************************************
 * File:      MenuSystem.hpp
 * Category:  UI/Menu
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Hierarchical menu system with mode selection.
 *    Top level: Debug Mode, Standard Mode, Screen Saver
 *    Access by holding Mode button, navigate with Up/Down
 * 
 * Usage:
 *    MenuSystem menu;
 *    menu.update(btnMgr, sensor_data);
 *    menu.render(oled_manager);
 *****************************************************************/

#ifndef MENU_SYSTEM_HPP
#define MENU_SYSTEM_HPP

#include <cstdint>
#include "UI/ButtonManager.hpp"
#include "Manager/OLEDDisplayManager.hpp"
#include "Drivers/UART Comms/GpuUartBidirectional.hpp"

namespace arcos::ui::menu {

// Top-level modes
enum class TopLevelMode : uint8_t {
  SCREEN_SAVER = 0,
  IDLE_GPS,
  DEBUG_MODE,
  DISPLAY_FACES,
  DISPLAY_EFFECTS,
  DISPLAY_SHADERS,
  LED_STRIP_CONFIG,
  COUNT
};

// Debug mode pages
enum class DebugPage : uint8_t {
  IMU_DATA = 0,
  ENVIRONMENTAL,
  GPS_DATA,
  MICROPHONE,
  SYSTEM_INFO,
  WIFI_INFO,         // WiFi credentials from CPU
  COUNT
};

// Display Faces options (HUB75 LED matrix shape selection)
enum class DisplayFace : uint8_t {
  CIRCLE = 0,
  SQUARE,
  TRIANGLE,
  HEXAGON,
  STAR,
  PANEL_NUMBER,      // Shows panel numbers (0/1) for dual panel setup
  ORIENTATION,       // Shows orientation arrows for panel alignment
  COUNT
};

// Display Effects options (HUB75 LED matrix animation effects)
enum class DisplayEffect : uint8_t {
  NONE = 0,
  PARTICLES,
  TRAILS,
  GRID,
  WAVE,
  COUNT
};

// Display Shaders options (HUB75 LED matrix post-processing)
enum class DisplayShader : uint8_t {
  RGB_SPLIT = 0,
  SCANLINES,
  PIXELATE,
  INVERT,
  DITHER,
  COUNT
};

// LED Strip Config options
enum class LedStripMode : uint8_t {
  DYNAMIC_DISPLAY = 0,  // Uses HUB75 as base
  RAINBOW,
  BREATHING,
  WAVE,
  FIRE,
  THEATER_CHASE,
  COUNT
};

// Menu states
enum class MenuState : uint8_t {
  MODE_SELECTOR,    // Top-level mode selection (accessed by holding MODE)
  ACTIVE_MODE,      // Currently running selected mode
  SUBMENU           // Inside a mode's submenu
};

class MenuSystem {
public:
  MenuSystem() 
    : m_state(MenuState::ACTIVE_MODE)
    , m_current_mode(TopLevelMode::IDLE_GPS)
    , m_debug_page(DebugPage::IMU_DATA)
    , m_display_face(DisplayFace::CIRCLE)
    , m_display_effect(DisplayEffect::NONE)
    , m_display_shader(DisplayShader::RGB_SPLIT)
    , m_led_strip_mode(LedStripMode::RAINBOW)
    , m_mode_selector_index(0)
    , m_submenu_index(0)
  {}

  /**
   * @brief Update menu system state
   * @param btnMgr Button manager reference
   * @param sensor_data Current sensor data
   * @param current_time_ms Current time in milliseconds
   */
  void update(ButtonManager& btnMgr, 
              const arcos::communication::SensorDataPayload& sensor_data,
              uint32_t current_time_ms) {
    
    m_sensor_data = &sensor_data;
    m_current_time = current_time_ms;
    
    // CRITICAL: MODE hold ALWAYS returns to mode selector from ANY state
    // This provides universal "escape to top" functionality
    if (btnMgr.wasHeld(ButtonID::MODE)) {
      // Cancel whatever is happening and go to mode selector
      enterModeSelector();
      btnMgr.clearFlags();  // Clear all button states to prevent interference
      return;  // Don't process other button events this frame
    }
    
    // Handle current state
    switch (m_state) {
      case MenuState::MODE_SELECTOR:
        updateModeSelector(btnMgr);
        break;
        
      case MenuState::ACTIVE_MODE:
        updateActiveMode(btnMgr);
        break;
        
      case MenuState::SUBMENU:
        updateSubmenu(btnMgr);
        break;
    }
    
    btnMgr.clearFlags();
  }

  /**
   * @brief Render current menu state to OLED
   * @param oled OLED display manager reference
   */
  void render(arcos::manager::OLEDDisplayManager& oled);

  /**
   * @brief Get current top-level mode
   */
  TopLevelMode getCurrentMode() const { return m_current_mode; }

  /**
   * @brief Get current debug page
   */
  DebugPage getDebugPage() const { return m_debug_page; }
  
  /**
   * @brief Get current display face
   */
  DisplayFace getDisplayFace() const { return m_display_face; }
  
  /**
   * @brief Get current display effect
   */
  DisplayEffect getDisplayEffect() const { return m_display_effect; }
  
  /**
   * @brief Get current display shader
   */
  DisplayShader getDisplayShader() const { return m_display_shader; }
  
  /**
   * @brief Get current LED strip mode
   */
  LedStripMode getLedStripMode() const { return m_led_strip_mode; }

  /**
   * @brief Get menu state
   */
  MenuState getMenuState() const { return m_state; }
  
  /**
   * @brief Get submenu index
   */
  uint8_t getSubmenuIndex() const { return m_submenu_index; }

private:
  MenuState m_state;
  TopLevelMode m_current_mode;
  DebugPage m_debug_page;
  DisplayFace m_display_face;
  DisplayEffect m_display_effect;
  DisplayShader m_display_shader;
  LedStripMode m_led_strip_mode;
  uint8_t m_mode_selector_index;
  uint8_t m_submenu_index;
  const arcos::communication::SensorDataPayload* m_sensor_data;
  uint32_t m_current_time;

  void enterModeSelector() {
    m_state = MenuState::MODE_SELECTOR;
    m_mode_selector_index = static_cast<uint8_t>(m_current_mode);
  }

  void exitModeSelector() {
    m_state = MenuState::ACTIVE_MODE;
    m_current_mode = static_cast<TopLevelMode>(m_mode_selector_index);
  }

  void updateModeSelector(ButtonManager& btnMgr) {
    // Navigate with up/down
    if (btnMgr.wasPressed(ButtonID::UP)) {
      if (m_mode_selector_index > 0) {
        m_mode_selector_index--;
      } else {
        m_mode_selector_index = static_cast<uint8_t>(TopLevelMode::COUNT) - 1;
      }
    }
    
    if (btnMgr.wasPressed(ButtonID::DOWN)) {
      m_mode_selector_index++;
      if (m_mode_selector_index >= static_cast<uint8_t>(TopLevelMode::COUNT)) {
        m_mode_selector_index = 0;
      }
    }
    
    // SET button to confirm selection and exit to selected mode
    if (btnMgr.wasPressed(ButtonID::SET)) {
      exitModeSelector();
    }
    
    // Short press MODE button to cancel and return to current mode
    if (btnMgr.wasPressed(ButtonID::MODE)) {
      m_state = MenuState::ACTIVE_MODE;
      // Don't change m_current_mode - cancel selection
    }
  }

  void updateActiveMode(ButtonManager& btnMgr) {
    // SET button enters submenu for modes that have one
    if (btnMgr.wasPressed(ButtonID::SET)) {
      if (modeHasSubmenu(m_current_mode)) {
        m_state = MenuState::SUBMENU;
        m_submenu_index = getCurrentSubmenuSelection();
        return;
      }
    }
    
    switch (m_current_mode) {
      case TopLevelMode::SCREEN_SAVER:
        // Screen saver - no navigation
        break;
        
      case TopLevelMode::IDLE_GPS:
        // Idle GPS - no navigation
        break;
        
      case TopLevelMode::DEBUG_MODE:
        updateDebugMode(btnMgr);
        break;
        
      case TopLevelMode::DISPLAY_FACES:
      case TopLevelMode::DISPLAY_EFFECTS:
      case TopLevelMode::DISPLAY_SHADERS:
      case TopLevelMode::LED_STRIP_CONFIG:
        // These modes show their current selection, press SET to enter submenu
        break;
        
      case TopLevelMode::COUNT:
        // Invalid state
        break;
    }
  }
  
  void updateSubmenu(ButtonManager& btnMgr) {
    uint8_t max_items = getSubmenuItemCount(m_current_mode);
    
    // Navigate with up/down
    if (btnMgr.wasPressed(ButtonID::UP)) {
      if (m_submenu_index > 0) {
        m_submenu_index--;
      } else {
        m_submenu_index = max_items - 1;
      }
    }
    
    if (btnMgr.wasPressed(ButtonID::DOWN)) {
      m_submenu_index++;
      if (m_submenu_index >= max_items) {
        m_submenu_index = 0;
      }
    }
    
    // SET to confirm selection and return to active mode
    if (btnMgr.wasPressed(ButtonID::SET)) {
      applySubmenuSelection();
      m_state = MenuState::ACTIVE_MODE;
    }
    
    // MODE short press to cancel and return
    if (btnMgr.wasPressed(ButtonID::MODE)) {
      m_state = MenuState::ACTIVE_MODE;
    }
  }
  
  bool modeHasSubmenu(TopLevelMode mode) const {
    switch (mode) {
      case TopLevelMode::DISPLAY_FACES:
      case TopLevelMode::DISPLAY_EFFECTS:
      case TopLevelMode::DISPLAY_SHADERS:
      case TopLevelMode::LED_STRIP_CONFIG:
        return true;
      default:
        return false;
    }
  }
  
  uint8_t getSubmenuItemCount(TopLevelMode mode) const {
    switch (mode) {
      case TopLevelMode::DISPLAY_FACES:
        return static_cast<uint8_t>(DisplayFace::COUNT);
      case TopLevelMode::DISPLAY_EFFECTS:
        return static_cast<uint8_t>(DisplayEffect::COUNT);
      case TopLevelMode::DISPLAY_SHADERS:
        return static_cast<uint8_t>(DisplayShader::COUNT);
      case TopLevelMode::LED_STRIP_CONFIG:
        return static_cast<uint8_t>(LedStripMode::COUNT);
      default:
        return 0;
    }
  }
  
  uint8_t getCurrentSubmenuSelection() const {
    switch (m_current_mode) {
      case TopLevelMode::DISPLAY_FACES:
        return static_cast<uint8_t>(m_display_face);
      case TopLevelMode::DISPLAY_EFFECTS:
        return static_cast<uint8_t>(m_display_effect);
      case TopLevelMode::DISPLAY_SHADERS:
        return static_cast<uint8_t>(m_display_shader);
      case TopLevelMode::LED_STRIP_CONFIG:
        return static_cast<uint8_t>(m_led_strip_mode);
      default:
        return 0;
    }
  }
  
  void applySubmenuSelection() {
    switch (m_current_mode) {
      case TopLevelMode::DISPLAY_FACES:
        m_display_face = static_cast<DisplayFace>(m_submenu_index);
        break;
      case TopLevelMode::DISPLAY_EFFECTS:
        m_display_effect = static_cast<DisplayEffect>(m_submenu_index);
        break;
      case TopLevelMode::DISPLAY_SHADERS:
        m_display_shader = static_cast<DisplayShader>(m_submenu_index);
        break;
      case TopLevelMode::LED_STRIP_CONFIG:
        m_led_strip_mode = static_cast<LedStripMode>(m_submenu_index);
        break;
      default:
        break;
    }
  }

  void updateDebugMode(ButtonManager& btnMgr) {
    // Navigate between debug pages with up/down
    if (btnMgr.wasPressed(ButtonID::UP)) {
      if (m_debug_page == DebugPage::IMU_DATA) {
        m_debug_page = static_cast<DebugPage>(static_cast<uint8_t>(DebugPage::COUNT) - 1);
      } else {
        m_debug_page = static_cast<DebugPage>(static_cast<uint8_t>(m_debug_page) - 1);
      }
    }
    
    if (btnMgr.wasPressed(ButtonID::DOWN)) {
      m_debug_page = static_cast<DebugPage>((static_cast<uint8_t>(m_debug_page) + 1) % 
                                            static_cast<uint8_t>(DebugPage::COUNT));
    }
  }


};

} // namespace arcos::ui::menu

#endif // MENU_SYSTEM_HPP
