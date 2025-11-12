/*****************************************************************
 * File:      HUB75BootAnimations.hpp
 * Category:  Animations/Boot
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Boot animations for HUB75 LED matrix display.
 *    Contains initialization and startup visual effects.
 * 
 * Usage:
 *    Register these functions with HUB75DisplayManager
 *****************************************************************/

#ifndef HUB75_BOOT_ANIMATIONS_HPP
#define HUB75_BOOT_ANIMATIONS_HPP

#include "Manager/HUB75DisplayManager.hpp"
#include <cmath>

namespace arcos::animations::hub75{

/**
 * @brief Boot animation: Spinning loading circles
 * Two counter-rotating circle patterns on dual displays
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootSpinningCircles(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  manager.clear({0, 0, 0});
  
  float angle = (time_ms % 2000) / 2000.0f * 6.28318f;
  const int num_circles = 5;
  const int orbit_radius = 10;
  const int circle_radius = 2;
  
  // Left display - clockwise rotation
  const int center_x1 = 32;
  const int center_y1 = 16;
  
  for(int i = 0; i < num_circles; i++){
    float circle_angle = angle + (i * 6.28318f / num_circles);
    int x = center_x1 + static_cast<int>(cosf(circle_angle) * orbit_radius);
    int y = center_y1 + static_cast<int>(sinf(circle_angle) * orbit_radius * 0.5f);
    
    uint8_t hue = static_cast<uint8_t>((i * 255) / num_circles);
    RGB color = {static_cast<uint8_t>(255 - hue), hue, 255};
    
    manager.fillCircle(x, y, circle_radius, color);
  }
  
  // Center pivot for left display
  manager.fillCircle(center_x1, center_y1, 1, {255, 255, 255});
  
  // Right display - counter-clockwise rotation
  const int center_x2 = 96;
  const int center_y2 = 16;
  
  for(int i = 0; i < num_circles; i++){
    float circle_angle = -angle + (i * 6.28318f / num_circles);
    int x = center_x2 + static_cast<int>(cosf(circle_angle) * orbit_radius);
    int y = center_y2 + static_cast<int>(sinf(circle_angle) * orbit_radius * 0.5f);
    
    uint8_t hue = static_cast<uint8_t>((i * 255) / num_circles);
    RGB color = {hue, static_cast<uint8_t>(255 - hue), 255};
    
    manager.fillCircle(x, y, circle_radius, color);
  }
  
  // Center pivot for right display
  manager.fillCircle(center_x2, center_y2, 1, {255, 255, 255});
  
  manager.show();
}

/**
 * @brief Boot animation: Progress bar with gradient
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootProgressBar(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  manager.clear({0, 0, 0});
  
  // Calculate progress (0-100% over 1500ms)
  float progress = (time_ms % 1500) / 1500.0f;
  if(progress > 1.0f) progress = 1.0f;
  
  const int bar_width = 120;
  const int bar_height = 8;
  const int bar_x = 4;
  const int bar_y = 12;
  
  // Draw border
  manager.drawRect(bar_x - 1, bar_y - 1, bar_width + 2, bar_height + 2, {128, 128, 128});
  
  // Fill progress bar with gradient
  int filled_width = static_cast<int>(bar_width * progress);
  for(int x = 0; x < filled_width; x++){
    uint8_t hue = static_cast<uint8_t>((x * 255) / bar_width);
    RGB color = {static_cast<uint8_t>(255 - hue), hue, 128};
    
    for(int y = 0; y < bar_height; y++){
      manager.setPixel(bar_x + x, bar_y + y, color);
    }
  }
  
  manager.show();
}

/**
 * @brief Boot animation: Expanding ripple effect
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootRipple(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  manager.clear({0, 0, 0});
  
  float progress = (time_ms % 1500) / 1500.0f;
  int center_x = 64;
  int center_y = 16;
  
  // Draw expanding circles
  for(int i = 0; i < 3; i++){
    float offset = i * 0.33f;
    float radius_progress = fmodf(progress + offset, 1.0f);
    int radius = static_cast<int>(radius_progress * 40.0f);
    
    uint8_t brightness = static_cast<uint8_t>((1.0f - radius_progress) * 255.0f);
    RGB color = {0, brightness, brightness};
    
    manager.drawCircle(center_x, center_y, radius, color);
  }
  
  manager.show();
}

/**
 * @brief Register all HUB75 boot animations
 * @param manager HUB75 display manager reference
 */
inline void registerBootAnimations(arcos::manager::HUB75DisplayManager& manager){
  manager.registerAnimation("boot_spinning_circles", [&manager](uint32_t time_ms){
    bootSpinningCircles(manager, time_ms);
  });
  
  manager.registerAnimation("boot_progress_bar", [&manager](uint32_t time_ms){
    bootProgressBar(manager, time_ms);
  });
  
  manager.registerAnimation("boot_ripple", [&manager](uint32_t time_ms){
    bootRipple(manager, time_ms);
  });
}

} // namespace arcos::animations::hub75

#endif // HUB75_BOOT_ANIMATIONS_HPP
