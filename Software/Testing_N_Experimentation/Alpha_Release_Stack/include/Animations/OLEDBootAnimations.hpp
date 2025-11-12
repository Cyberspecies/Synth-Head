/*****************************************************************
 * File:      OLEDBootAnimations.hpp
 * Category:  Animations/Boot
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Boot animations for OLED SH1107 display.
 *    Contains initialization and startup visual effects.
 * 
 * Usage:
 *    Register these functions with OLEDDisplayManager
 *****************************************************************/

#ifndef OLED_BOOT_ANIMATIONS_HPP
#define OLED_BOOT_ANIMATIONS_HPP

#include "Manager/OLEDDisplayManager.hpp"
#include <cmath>

namespace arcos::animations::oled{

/**
 * @brief Boot animation: System initialization text with progress
 * @param manager OLED display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootSystemInit(arcos::manager::OLEDDisplayManager& manager, uint32_t time_ms){
  manager.clear();
  
  // Title
  manager.drawText(15, 10, "SYNTH-HEAD GPU", true);
  manager.drawText(20, 22, "Initializing...", true);
  
  // Progress stages based on time
  if(time_ms > 200){
    manager.drawText(5, 45, "[OK] HUB75 Display", true);
  }
  if(time_ms > 400){
    manager.drawText(5, 57, "[OK] OLED Display", true);
  }
  if(time_ms > 600){
    manager.drawText(5, 69, "[OK] UART Comm", true);
  }
  if(time_ms > 800){
    manager.drawText(5, 81, "[OK] LED System", true);
  }
  if(time_ms > 1000){
    manager.drawText(5, 93, "[OK] Sensors", true);
  }
  
  // Progress bar at bottom
  float progress = (time_ms % 1500) / 1500.0f;
  if(progress > 1.0f) progress = 1.0f;
  
  int bar_width = 100;
  int bar_x = 14;
  int bar_y = 110;
  
  manager.drawRect(bar_x - 1, bar_y - 1, bar_width + 2, 8, false, true);
  int filled = static_cast<int>(bar_width * progress);
  manager.fillRect(bar_x, bar_y, filled, 6, true);
  
  manager.show();
}

/**
 * @brief Boot animation: Expanding circle waves
 * @param manager OLED display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootCircleWaves(arcos::manager::OLEDDisplayManager& manager, uint32_t time_ms){
  manager.clear();
  
  int center_x = 64;
  int center_y = 64;
  float progress = (time_ms % 1500) / 1500.0f;
  
  // Draw multiple expanding circles
  for(int i = 0; i < 5; i++){
    float offset = i * 0.2f;
    float radius_progress = fmodf(progress + offset, 1.0f);
    int radius = static_cast<int>(radius_progress * 80.0f);
    
    manager.drawCircle(center_x, center_y, radius, true);
  }
  
  // Center dot
  manager.fillCircle(center_x, center_y, 3, true);
  
  manager.show();
}

/**
 * @brief Boot animation: Logo/brand display with animation
 * @param manager OLED display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootLogo(arcos::manager::OLEDDisplayManager& manager, uint32_t time_ms){
  manager.clear();
  
  // Draw animated logo frame
  int frame_size = 60 + static_cast<int>(sinf(time_ms / 200.0f) * 5.0f);
  int center_x = 64;
  int center_y = 50;
  
  // Outer frame
  manager.drawRect(center_x - frame_size/2, center_y - frame_size/2, 
                   frame_size, frame_size, false, true);
  
  // Inner design
  int inner_size = frame_size - 20;
  manager.drawCircle(center_x, center_y, inner_size/2, true);
  
  // Text
  manager.drawText(15, 10, "SYNTH-HEAD", true);
  manager.drawText(30, 105, "GPU System", true);
  
  manager.show();
}

/**
 * @brief Boot animation: Scanning lines effect
 * @param manager OLED display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootScanLines(arcos::manager::OLEDDisplayManager& manager, uint32_t time_ms){
  manager.clear();
  
  float progress = (time_ms % 1500) / 1500.0f;
  int scan_y = static_cast<int>(progress * 128.0f);
  
  // Title
  manager.drawText(20, 5, "SYSTEM SCAN", true);
  
  // Draw scan line
  for(int x = 0; x < 128; x++){
    manager.setPixel(x, scan_y, true);
    if(scan_y > 0) manager.setPixel(x, scan_y - 1, true);
    if(scan_y < 127) manager.setPixel(x, scan_y + 1, true);
  }
  
  // Grid pattern
  for(int y = 0; y < scan_y; y += 8){
    for(int x = 0; x < 128; x += 8){
      manager.setPixel(x, y, true);
    }
  }
  
  // Progress text
  char buf[32];
  snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(progress * 100.0f));
  manager.drawText(55, 60, buf, true);
  
  manager.show();
}

/**
 * @brief Register all OLED boot animations
 * @param manager OLED display manager reference
 */
inline void registerBootAnimations(arcos::manager::OLEDDisplayManager& manager){
  manager.registerAnimation("boot_system_init", [&manager](uint32_t time_ms){
    bootSystemInit(manager, time_ms);
  });
  
  manager.registerAnimation("boot_circle_waves", [&manager](uint32_t time_ms){
    bootCircleWaves(manager, time_ms);
  });
  
  manager.registerAnimation("boot_logo", [&manager](uint32_t time_ms){
    bootLogo(manager, time_ms);
  });
  
  manager.registerAnimation("boot_scan_lines", [&manager](uint32_t time_ms){
    bootScanLines(manager, time_ms);
  });
}

} // namespace arcos::animations::oled

#endif // OLED_BOOT_ANIMATIONS_HPP
