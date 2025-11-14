/*****************************************************************
 * File:      MenuRenderer.hpp
 * Category:  UI/Menu
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Rendering implementation for MenuSystem.
 *    Handles mode selector and delegates to appropriate pages.
 * 
 * Usage:
 *    Included by MenuSystem.hpp - implementation detail
 *****************************************************************/

#ifndef MENU_RENDERER_HPP
#define MENU_RENDERER_HPP

#include "UI/Menu/MenuSystem.hpp"
#include "UI/OLED/DebugPages.hpp"

namespace arcos::ui::menu {

// Forward declare for cleaner organization
extern uint32_t g_sensor_fps;
extern uint32_t g_led_fps;
extern uint8_t g_fan_speed;

/**
 * @brief Render mode selector menu with flashing box around selection
 */
inline void renderModeSelector(arcos::manager::OLEDDisplayManager& oled, uint8_t selected_index, uint32_t time_ms) {
  oled.clear();
  
  // Title
  oled.drawText(25, 0, "MODE SELECT", true);
  oled.drawLine(0, 12, 127, 12, true);
  
  const char* mode_names[] = {
    "1.Screen Saver",
    "2.Idle (GPS)",
    "3.Debug Mode",
    "4.Display Faces",
    "5.Display Effects",
    "6.Display Shaders",
    "7.LED Strip Cfg"
  };
  
  // Mode descriptions (unused but kept for future reference)
  // const char* mode_desc[] = {
  //   "Bouncing Text",
  //   "GPS Time",
  //   "Sensor Data",
  //   "Shape Select",
  //   "Effect Select",
  //   "Shader Select",
  //   "LED Config"
  // };
  
  // Draw mode list (very compact for 7 items)
  int start_y = 16;
  int item_height = 14;
  int spacing = 1;
  
  for (uint8_t i = 0; i < static_cast<uint8_t>(TopLevelMode::COUNT); i++) {
    int item_y = start_y + i * (item_height + spacing);
    bool selected = (i == selected_index);
    
    // Draw selection box with flashing border if selected
    if (selected) {
      // Flashing outline (500ms period)
      bool flash_on = (time_ms / 250) % 2 == 0;
      if (flash_on) {
        // Double border for emphasis when flashing
        oled.drawRect(3, item_y - 2, 122, item_height, false, true);
        oled.drawRect(5, item_y, 118, item_height - 4, false, true);
      } else {
        // Single thick border
        oled.drawRect(3, item_y - 2, 122, item_height, false, true);
      }
      
      // Arrow indicator
      oled.drawText(8, item_y + 1, ">", true);
    }
    
    // Draw mode name (always visible, always use true for visibility)
    oled.drawText(18, item_y + 3, mode_names[i], true);
  }
  
  // Instructions at bottom
  oled.drawLine(0, 115, 127, 115, true);
  oled.drawText(2, 118, "UP/DN  SET:OK", true);
  
  oled.show();
}

/**
 * @brief Render debug mode pages
 */
inline void renderDebugMode(arcos::manager::OLEDDisplayManager& oled,
                            DebugPage page,
                            const arcos::communication::SensorDataPayload& sensor_data) {
  switch (page) {
    case DebugPage::IMU_DATA:
      oled::renderImuPage(oled, sensor_data);
      break;
      
    case DebugPage::ENVIRONMENTAL:
      oled::renderEnvironmentalPage(oled, sensor_data);
      break;
      
    case DebugPage::GPS_DATA:
      oled::renderGpsPage(oled, sensor_data);
      break;
      
    case DebugPage::MICROPHONE:
      oled::renderMicrophonePage(oled, sensor_data);
      break;
      
    case DebugPage::SYSTEM_INFO:
      oled::renderSystemInfoPage(oled, sensor_data, g_sensor_fps, g_led_fps, g_fan_speed);
      break;
      
    case DebugPage::WIFI_INFO:
      oled::renderWifiInfoPage(oled, sensor_data);
      break;
      
    default:
      break;
  }
}

/**
 * @brief Render idle GPS mode - Bouncing time display
 */
inline void renderIdleGps(arcos::manager::OLEDDisplayManager& oled,
                          const arcos::communication::SensorDataPayload& sensor_data,
                          uint32_t time_ms) {
  oled.clear();
  
  // Title at top
  oled.drawText(30, 0, "GPS TIME", true);
  oled.drawLine(0, 10, 127, 10, true);
  
  // Format GPS time
  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", 
           sensor_data.gps_hour, sensor_data.gps_minute, sensor_data.gps_second);
  
  // Bouncing position (simple box collision)
  static int pos_x = 30;
  static int pos_y = 50;
  static int vel_x = 1;
  static int vel_y = 1;
  
  // Text bounds (approximate)
  constexpr int text_width = 48;  // 8 chars * 6 pixels
  constexpr int text_height = 8;
  
  pos_x += vel_x;
  pos_y += vel_y;
  
  // Keep within bounds (below title line)
  if (pos_x <= 0 || pos_x >= 128 - text_width) vel_x = -vel_x;
  if (pos_y <= 12 || pos_y >= 128 - text_height) vel_y = -vel_y;
  
  oled.drawText(pos_x, pos_y, time_str, true);
  oled.show();
}

/**
 * @brief Render display faces mode - Shows current HUB75 shape selection
 * Note: OLED shows configuration menu, HUB75 displays the actual shape
 */
inline void renderDisplayFaces(arcos::manager::OLEDDisplayManager& oled, DisplayFace face) {
  oled.clear();
  oled.drawText(10, 0, "HUB75 FACE CFG", true);
  oled.drawLine(0, 10, 127, 10, true);
  oled.drawText(10, 15, "Press SET to change", true);
  
  const char* face_names[] = {"Custom", "Panel #", "Orient"};
  
  
  char text[32];
  snprintf(text, sizeof(text), "Current: %s", face_names[static_cast<uint8_t>(face)]);
  oled.drawText(10, 40, text, true);
  
  // Draw preview of selected face
  constexpr int cx = 64;
  constexpr int cy = 80;
  
  switch (face) {
    case DisplayFace::CUSTOM_IMAGE:
      // Show custom image indicator
      oled.drawRect(cx - 20, cy - 15, 40, 30, false, true);
      oled.drawText(cx - 20, cy - 5, "IMG", true);
      oled.drawText(cx - 18, cy + 5, "FILE", true);
      break;
    case DisplayFace::PANEL_NUMBER:
      // Show panel indicators
      oled.drawRect(cx - 30, cy - 15, 25, 30, false, true);
      oled.drawText(cx - 22, cy - 4, "0", true);
      oled.drawRect(cx + 5, cy - 15, 25, 30, false, true);
      oled.drawText(cx + 13, cy - 4, "1", true);
      break;
    case DisplayFace::ORIENTATION:
      // Show orientation arrows preview
      oled.drawText(cx - 12, cy - 10, "UP", true);
      oled.drawText(cx - 18, cy + 5, "DOWN", true);
      oled.drawLine(cx - 10, cy - 3, cx + 10, cy - 3, true);
      oled.drawLine(cx - 10, cy + 3, cx + 10, cy + 3, true);
      break;
    default:
      break;
  }
  
  oled.show();
}

/**
 * @brief Render display effects mode - Shows current HUB75 effect selection
 * Note: OLED shows configuration menu, HUB75 displays the actual effect
 */
inline void renderDisplayEffects(arcos::manager::OLEDDisplayManager& oled, DisplayEffect effect) {
  oled.clear();
  oled.drawText(8, 0, "HUB75 EFFECT CFG", true);
  oled.drawLine(0, 10, 127, 10, true);
  oled.drawText(10, 15, "Press SET to change", true);
  
  const char* effect_names[] = {"None", "Particles", "Trails", "Grid", "Wave"};
  
  char text[32];
  snprintf(text, sizeof(text), "Current: %s", effect_names[static_cast<uint8_t>(effect)]);
  oled.drawText(10, 40, text, true);
  
  oled.show();
}

/**
 * @brief Render display shaders mode - Shows current HUB75 shader selection
 * Note: OLED shows configuration menu, HUB75 displays the shader result
 */
inline void renderDisplayShaders(arcos::manager::OLEDDisplayManager& oled, DisplayShader shader) {
  oled.clear();
  oled.drawText(8, 0, "HUB75 SHADER CFG", true);
  oled.drawLine(0, 10, 127, 10, true);
  oled.drawText(10, 15, "Press SET to change", true);
  
  const char* shader_names[] = {
    "None", "Hue Row", "Hue All", "Color", "Breathe", 
    "RGB Split", "Scanlines", "Pixelate", "Invert", "Dither"
  };
  
  char text[32];
  snprintf(text, sizeof(text), "Current: %s", shader_names[static_cast<uint8_t>(shader)]);
  oled.drawText(10, 40, text, true);
  
  oled.show();
}

/**
 * @brief Render LED strip config mode
 */
inline void renderLedStripConfig(arcos::manager::OLEDDisplayManager& oled, LedStripMode mode) {
  oled.clear();
  oled.drawText(10, 0, "LED STRIP CONFIG", true);
  oled.drawLine(0, 10, 127, 10, true);
  oled.drawText(10, 15, "Press SET to change", true);
  
  const char* mode_names[] = {"Dynamic Display", "Rainbow", "Breathing", "Wave", "Fire", "Theater"};
  
  char text[32];
  snprintf(text, sizeof(text), "Mode: %s", mode_names[static_cast<uint8_t>(mode)]);
  oled.drawText(10, 40, text, true);
  
  if (mode == LedStripMode::DYNAMIC_DISPLAY) {
    oled.drawText(10, 55, "Uses HUB75 display", true);
    oled.drawText(10, 65, "as LED source", true);
  }
  
  oled.show();
}

/**
 * @brief Render submenu for selection
 */
inline void renderSubmenu(arcos::manager::OLEDDisplayManager& oled, 
                          TopLevelMode mode, 
                          uint8_t selected_index,
                          uint32_t time_ms) {
  oled.clear();
  
  const char* title = "SUBMENU";
  const char** item_names = nullptr;
  uint8_t item_count = 0;
  
  // Get appropriate items based on mode
  static const char* face_names[] = {"Custom", "Panel #", "Orient"};
  static const char* effect_names[] = {"None", "Particles", "Trails", "Grid", "Wave"};
  static const char* shader_names[] = {
    "None", "Hue Row", "Hue All", "Color", "Breathe", 
    "RGB Split", "Scanlines", "Pixelate", "Invert", "Dither"
  };
  static const char* led_names[] = {"Dynamic", "Rainbow", "Breathing", "Wave", "Fire", "Theater"};
  
  switch (mode) {
    case TopLevelMode::DISPLAY_FACES:
      title = "SELECT FACE";
      item_names = face_names;
      item_count = 3;
      break;
    case TopLevelMode::DISPLAY_EFFECTS:
      title = "SELECT EFFECT";
      item_names = effect_names;
      item_count = 5;
      break;
    case TopLevelMode::DISPLAY_SHADERS:
      title = "SELECT SHADER";
      item_names = shader_names;
      item_count = 10;
      break;
    case TopLevelMode::LED_STRIP_CONFIG:
      title = "SELECT LED MODE";
      item_names = led_names;
      item_count = 6;
      break;
    default:
      break;
  }
  
  oled.drawText(15, 0, title, true);
  oled.drawLine(0, 10, 127, 10, true);
  
  // Vertical carousel - centered selection
  int item_height = 12;
  int visible_items = 8;  // Show up to 8 items (96 pixels from y=14 to y=110)
  int middle_slot = visible_items / 2;  // Middle position (slot 4 of 0-7)
  int start_y = 14;
  
  // Calculate which items to display (carousel wrapping)
  // Selected item should be in the middle slot
  int first_visible_index = selected_index - middle_slot;
  
  // Draw visible items in carousel
  for (int slot = 0; slot < visible_items; slot++) {
    // Calculate actual item index with wrapping
    int item_index = first_visible_index + slot;
    
    // Wrap around for carousel effect
    while (item_index < 0) item_index += item_count;
    while (item_index >= item_count) item_index -= item_count;
    
    int item_y = start_y + slot * item_height;
    bool selected = (slot == middle_slot);
    
    if (selected) {
      // Flashing box for selected item (always in middle)
      bool flash_on = (time_ms / 250) % 2 == 0;
      if (flash_on) {
        oled.drawRect(2, item_y, 124, item_height - 2, false, true);
      }
      oled.drawText(6, item_y + 2, ">", true);
    }
    
    // Always draw item names with true for visibility
    oled.drawText(16, item_y + 2, item_names[item_index], true);
  }
  
  // Scroll indicators (show if list is longer than visible area)
  if (item_count > visible_items) {
    oled.drawText(120, 14, "^", true);   // Up arrow (always show for carousel)
    oled.drawText(120, 108, "v", true);  // Down arrow (always show for carousel)
  }
  
  // Instructions
  oled.drawLine(0, 115, 127, 115, true);
  oled.drawText(2, 118, "UP/DN SET:OK", true);
  
  oled.show();
}

/**
 * @brief Render gyroscope visualization (moved from standard mode)
 */
inline void renderGyroVisualization(arcos::manager::OLEDDisplayManager& oled,
                               const arcos::communication::SensorDataPayload& sensor_data,
                               uint32_t time_ms) {
  oled.clear();
  
  // Title
  oled.drawText(25, 0, "STANDARD MODE", true);
  
  // Get gyroscope data (in degrees per second)
  float gyro_x = sensor_data.gyro_x;
  float gyro_y = sensor_data.gyro_y;
  float gyro_z = sensor_data.gyro_z;
  
  // Center point for visualization
  constexpr int CENTER_X = 64;
  constexpr int CENTER_Y = 64;
  constexpr int RADIUS = 35;
  
  // Draw circular border
  oled.drawCircle(CENTER_X, CENTER_Y, RADIUS, true);
  oled.drawCircle(CENTER_X, CENTER_Y, RADIUS - 1, true);
  
  // Draw center crosshair
  oled.drawLine(CENTER_X - 5, CENTER_Y, CENTER_X + 5, CENTER_Y, true);
  oled.drawLine(CENTER_X, CENTER_Y - 5, CENTER_X, CENTER_Y + 5, true);
  
  // Map gyroscope to position (scale: ±500 dps to radius)
  constexpr float SCALE = RADIUS / 500.0f;
  int gyro_dot_x = CENTER_X + static_cast<int>(gyro_x * SCALE);
  int gyro_dot_y = CENTER_Y + static_cast<int>(gyro_y * SCALE);
  
  // Clamp to circle
  int dx = gyro_dot_x - CENTER_X;
  int dy = gyro_dot_y - CENTER_Y;
  float dist = sqrtf(dx * dx + dy * dy);
  if (dist > RADIUS - 3) {
    float scale = (RADIUS - 3) / dist;
    gyro_dot_x = CENTER_X + static_cast<int>(dx * scale);
    gyro_dot_y = CENTER_Y + static_cast<int>(dy * scale);
  }
  
  // Draw gyroscope position indicator (filled circle)
  oled.drawCircle(gyro_dot_x, gyro_dot_y, 3, true);
  oled.fillRect(gyro_dot_x - 2, gyro_dot_y - 2, 4, 4, true);
  
  // Draw line from center to gyro position
  oled.drawLine(CENTER_X, CENTER_Y, gyro_dot_x, gyro_dot_y, true);
  
  // Display numeric values at bottom
  char text[32];
  snprintf(text, sizeof(text), "X:%.0f", gyro_x);
  oled.drawText(5, 105, text, true);
  
  snprintf(text, sizeof(text), "Y:%.0f", gyro_y);
  oled.drawText(48, 105, text, true);
  
  snprintf(text, sizeof(text), "Z:%.0f", gyro_z);
  oled.drawText(91, 105, text, true);
  
  // Pulsing indicator based on time (shows it's live)
  bool pulse_on = (time_ms / 250) % 2 == 0;
  if (pulse_on) {
    oled.fillRect(123, 1, 3, 3, true);
  }
  
  oled.show();
}

/**
 * @brief Render screen saver
 */
inline void renderScreenSaver(arcos::manager::OLEDDisplayManager& oled, uint32_t time_ms) {
  oled.clear();
  
  // Title bar
  oled.drawText(20, 0, "SCREEN SAVER", true);
  oled.drawLine(0, 10, 127, 10, true);
  
  // Simple bouncing text
  int x = static_cast<int>((time_ms / 20) % 100);
  int y = static_cast<int>((time_ms / 30) % 90) + 15;
  
  oled.drawText(x, y, "SYNTH-HEAD", true);
  oled.show();
}

// MenuSystem render implementation
inline void MenuSystem::render(arcos::manager::OLEDDisplayManager& oled) {
  switch (m_state) {
    case MenuState::MODE_SELECTOR:
      renderModeSelector(oled, m_mode_selector_index, m_current_time);
      break;
      
    case MenuState::ACTIVE_MODE:
      switch (m_current_mode) {
        case TopLevelMode::SCREEN_SAVER:
          renderScreenSaver(oled, m_current_time);
          break;
          
        case TopLevelMode::IDLE_GPS:
          renderIdleGps(oled, *m_sensor_data, m_current_time);
          break;
          
        case TopLevelMode::DEBUG_MODE:
          renderDebugMode(oled, m_debug_page, *m_sensor_data);
          break;
          
        case TopLevelMode::DISPLAY_FACES:
          renderDisplayFaces(oled, m_display_face);
          break;
          
        case TopLevelMode::DISPLAY_EFFECTS:
          renderDisplayEffects(oled, m_display_effect);
          break;
          
        case TopLevelMode::DISPLAY_SHADERS:
          renderDisplayShaders(oled, m_display_shader);
          break;
          
        case TopLevelMode::LED_STRIP_CONFIG:
          renderLedStripConfig(oled, m_led_strip_mode);
          break;
          
        case TopLevelMode::COUNT:
          // Invalid state
          break;
      }
      break;
      
    case MenuState::SUBMENU:
      renderSubmenu(oled, m_current_mode, m_submenu_index, m_current_time);
      break;
  }
}

} // namespace arcos::ui::menu

#endif // MENU_RENDERER_HPP
