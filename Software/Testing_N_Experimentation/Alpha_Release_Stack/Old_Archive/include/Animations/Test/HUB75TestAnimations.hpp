/*****************************************************************
 * File:      HUB75TestAnimations.hpp
 * Category:  Animations/Test
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Test and demonstration animations for HUB75 LED matrix.
 *    Various visual effects for testing and demonstration.
 * 
 * Usage:
 *    Register these functions with HUB75DisplayManager
 *****************************************************************/

#ifndef HUB75_TEST_ANIMATIONS_HPP
#define HUB75_TEST_ANIMATIONS_HPP

#include "Manager/HUB75DisplayManager.hpp"
#include <cmath>

namespace arcos::animations::hub75{

/**
 * @brief Test animation: Rainbow wave
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void testRainbowWave(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  manager.clear({0, 0, 0});
  
  for(int x = 0; x < manager.getWidth(); x++){
    float hue = fmodf((x / (float)manager.getWidth()) + (time_ms / 2000.0f), 1.0f);
    float h = hue * 6.0f;
    int region = static_cast<int>(h);
    float f = h - region;
    
    uint8_t q = static_cast<uint8_t>(255 * (1.0f - f));
    uint8_t t = static_cast<uint8_t>(255 * f);
    
    RGB color;
    switch(region % 6){
      case 0: color = {255, t, 0}; break;
      case 1: color = {q, 255, 0}; break;
      case 2: color = {0, 255, t}; break;
      case 3: color = {0, q, 255}; break;
      case 4: color = {t, 0, 255}; break;
      case 5: color = {255, 0, q}; break;
    }
    
    for(int y = 0; y < manager.getHeight(); y++){
      manager.setPixel(x, y, color);
    }
  }
  // NO show() - called in main loop
}

/**
 * @brief Test animation: Plasma effect
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void testPlasma(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  manager.clear({0, 0, 0});
  
  float time_sec = time_ms / 1000.0f;
  
  for(int y = 0; y < manager.getHeight(); y++){
    for(int x = 0; x < manager.getWidth(); x++){
      float value = sinf(x / 8.0f + time_sec) +
                    sinf(y / 6.0f + time_sec * 1.5f) +
                    sinf((x + y) / 10.0f + time_sec * 2.0f);
      
      value = (value + 3.0f) / 6.0f;  // Normalize to 0-1
      
      uint8_t r = static_cast<uint8_t>(127.5f + 127.5f * sinf(value * 6.28318f));
      uint8_t g = static_cast<uint8_t>(127.5f + 127.5f * sinf(value * 6.28318f + 2.094f));
      uint8_t b = static_cast<uint8_t>(127.5f + 127.5f * sinf(value * 6.28318f + 4.189f));
      
      manager.setPixel(x, y, {r, g, b});
    }
  }
  // NO show() - called in main loop
}

/**
 * @brief Test animation: RGB cycle (solid colors for 60fps)
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 * NOTE: show() is called by the main loop, not here!
 */
inline void testRGBCycle(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  // Cycle through solid colors every 2 seconds
  uint32_t cycle_pos = (time_ms / 2000) % 3;
  
  RGB color;
  switch(cycle_pos){
    case 0: color = {255, 0, 0}; break;    // Red
    case 1: color = {0, 255, 0}; break;    // Green
    case 2: color = {0, 0, 255}; break;    // Blue
  }
  
  // Fill entire display with one color - simple and fast
  manager.clear(color);
  // NO show() call here - called once per frame in main loop!
}

/**
 * @brief Test animation: Simple scrolling bars (optimized for 60fps)
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 * NOTE: show() is called by the main loop, not here!
 */
inline void testScrollingBars(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  static bool first_frame = true;
  
  // Only clear on first frame
  if(first_frame){
    manager.clear({0, 0, 0});
    first_frame = false;
  }
  
  // Scroll effect - shift columns
  int offset = (time_ms / 50) % manager.getWidth();
  
  // Draw 3 colored bars
  for(int y = 0; y < manager.getHeight(); y++){
    int x = (offset + y) % manager.getWidth();
    
    RGB color;
    if(y < 10){
      color = {255, 0, 0};  // Red
    }else if(y < 21){
      color = {0, 255, 0};  // Green  
    }else{
      color = {0, 0, 255};  // Blue
    }
    
    manager.setPixel(x, y, color);
    
    // Fade previous column
    int prev_x = (x - 1 + manager.getWidth()) % manager.getWidth();
    manager.setPixel(prev_x, y, {0, 0, 0});
  }
  // NO show() call here - called once per frame in main loop!
}

/**
 * @brief Test animation: Bouncing ball (original, slower)
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void testBouncingBall(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  manager.clear({0, 0, 0});
  
  float time_sec = time_ms / 1000.0f;
  
  // Ball physics
  int ball_x = static_cast<int>(64.0f + 50.0f * sinf(time_sec * 2.0f));
  int ball_y = static_cast<int>(16.0f + 12.0f * fabs(sinf(time_sec * 3.0f)));
  int ball_radius = 3;
  
  // Draw ball with trail
  manager.fillCircle(ball_x, ball_y, ball_radius, {255, 100, 0});
  
  // Trail effect
  for(int i = 1; i < 5; i++){
    float trail_time = time_sec - (i * 0.05f);
    int trail_x = static_cast<int>(64.0f + 50.0f * sinf(trail_time * 2.0f));
    int trail_y = static_cast<int>(16.0f + 12.0f * fabs(sinf(trail_time * 3.0f)));
    uint8_t brightness = 255 - (i * 50);
    
    manager.fillCircle(trail_x, trail_y, ball_radius - 1, 
                      {static_cast<uint8_t>(brightness / 2), 
                       static_cast<uint8_t>(brightness / 4), 
                       0});
  }
  // NO show() - called in main loop
}

/**
 * @brief Test animation: Starfield
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void testStarfield(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  static uint32_t last_time = 0;
  static int star_x[50];
  static int star_y[50];
  static uint8_t star_brightness[50];
  static bool initialized = false;
  
  if(!initialized){
    for(int i = 0; i < 50; i++){
      star_x[i] = rand() % manager.getWidth();
      star_y[i] = rand() % manager.getHeight();
      star_brightness[i] = 128 + (rand() % 128);
    }
    initialized = true;
  }
  
  manager.clear({0, 0, 5});  // Dark blue background
  
  // Update and draw stars
  uint32_t dt = time_ms - last_time;
  for(int i = 0; i < 50; i++){
    // Move stars
    star_x[i] += (dt / 50);
    if(star_x[i] >= manager.getWidth()){
      star_x[i] = 0;
      star_y[i] = rand() % manager.getHeight();
    }
    
    // Twinkle effect
    star_brightness[i] = 128 + static_cast<uint8_t>(127.0f * sinf(time_ms / 200.0f + i));
    
    // Draw star
    manager.setPixel(star_x[i], star_y[i], {star_brightness[i], star_brightness[i], star_brightness[i]});
  }
  
  last_time = time_ms;
  // NO show() - called in main loop
}

/**
 * @brief Test animation: Panel axes and rotation indicators
 * Shows X+, Y+, and clockwise direction arrows on each panel
 * @param manager HUB75 display manager reference
 * @param time_ms Animation time in milliseconds
 */
inline void testPanelAxes(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  manager.clear({0, 0, 0});
  
  // Panel dimensions (assuming dual 64x32 panels = 128x32 total)
  const int panel_width = 64;
  const int panel_height = 32;
  const int total_width = manager.getWidth();
  
  // Colors for axes
  const RGB x_color = {255, 0, 0};      // Red for X+
  const RGB y_color = {0, 255, 0};      // Green for Y+
  const RGB cw_color = {0, 128, 255};   // Cyan for Clockwise
  const RGB label_color = {255, 255, 255}; // White for labels
  
  // Draw axes for each panel
  for(int panel = 0; panel < 2; panel++){
    int panel_x_offset = panel * panel_width;
    
    // Center of panel
    int center_x = panel_x_offset + panel_width / 2;
    int center_y = panel_height / 2;
    
    // X+ Arrow (horizontal, pointing right)
    // Arrow shaft
    for(int i = 0; i < 20; i++){
      manager.setPixel(center_x + i, center_y, x_color);
    }
    // Arrowhead
    manager.setPixel(center_x + 20, center_y, x_color);
    manager.setPixel(center_x + 19, center_y - 1, x_color);
    manager.setPixel(center_x + 19, center_y + 1, x_color);
    manager.setPixel(center_x + 18, center_y - 2, x_color);
    manager.setPixel(center_x + 18, center_y + 2, x_color);
    
    // Label "X+"
    int x_label_x = center_x + 22;
    int x_label_y = center_y - 5;
    manager.setPixel(x_label_x, x_label_y, label_color);
    manager.setPixel(x_label_x + 1, x_label_y + 1, label_color);
    manager.setPixel(x_label_x + 2, x_label_y + 2, label_color);
    manager.setPixel(x_label_x + 1, x_label_y + 3, label_color);
    manager.setPixel(x_label_x, x_label_y + 4, label_color);
    manager.setPixel(x_label_x + 4, x_label_y + 1, label_color);
    manager.setPixel(x_label_x + 5, x_label_y + 2, label_color);
    manager.setPixel(x_label_x + 4, x_label_y + 3, label_color);
    
    // Y+ Arrow (vertical, pointing down - standard screen coordinates)
    // Arrow shaft
    for(int i = 0; i < 10; i++){
      manager.setPixel(center_x, center_y + i, y_color);
    }
    // Arrowhead
    manager.setPixel(center_x, center_y + 10, y_color);
    manager.setPixel(center_x - 1, center_y + 9, y_color);
    manager.setPixel(center_x + 1, center_y + 9, y_color);
    manager.setPixel(center_x - 2, center_y + 8, y_color);
    manager.setPixel(center_x + 2, center_y + 8, y_color);
    
    // Label "Y+"
    int y_label_x = center_x + 4;
    int y_label_y = center_y + 8;
    manager.setPixel(y_label_x, y_label_y, label_color);
    manager.setPixel(y_label_x + 1, y_label_y + 1, label_color);
    manager.setPixel(y_label_x + 2, y_label_y + 2, label_color);
    manager.setPixel(y_label_x + 3, y_label_y + 1, label_color);
    manager.setPixel(y_label_x + 4, y_label_y, label_color);
    manager.setPixel(y_label_x + 6, y_label_y + 1, label_color);
    manager.setPixel(y_label_x + 7, y_label_y + 2, label_color);
    manager.setPixel(y_label_x + 6, y_label_y + 3, label_color);
    
    // Clockwise indicator (circular arc in top-left quadrant)
    float arc_radius = 12.0f;
    float start_angle = -3.14159f;  // PI (left)
    float end_angle = -1.5708f;     // PI/2 (up)
    
    // Draw arc
    for(float angle = start_angle; angle < end_angle; angle += 0.1f){
      int arc_x = center_x + static_cast<int>(arc_radius * cosf(angle));
      int arc_y = center_y + static_cast<int>(arc_radius * sinf(angle));
      manager.setPixel(arc_x, arc_y, cw_color);
    }
    
    // Clockwise arrow at end of arc
    int arrow_x = center_x + static_cast<int>(arc_radius * cosf(end_angle));
    int arrow_y = center_y + static_cast<int>(arc_radius * sinf(end_angle));
    manager.setPixel(arrow_x + 1, arrow_y, cw_color);
    manager.setPixel(arrow_x + 2, arrow_y + 1, cw_color);
    manager.setPixel(arrow_x, arrow_y + 1, cw_color);
    manager.setPixel(arrow_x + 1, arrow_y + 2, cw_color);
    
    // Label "CW" above arc
    int cw_label_x = center_x - 16;
    int cw_label_y = center_y - 14;
    // C
    manager.setPixel(cw_label_x + 1, cw_label_y, label_color);
    manager.setPixel(cw_label_x + 2, cw_label_y, label_color);
    manager.setPixel(cw_label_x, cw_label_y + 1, label_color);
    manager.setPixel(cw_label_x, cw_label_y + 2, label_color);
    manager.setPixel(cw_label_x, cw_label_y + 3, label_color);
    manager.setPixel(cw_label_x + 1, cw_label_y + 4, label_color);
    manager.setPixel(cw_label_x + 2, cw_label_y + 4, label_color);
    // W
    manager.setPixel(cw_label_x + 4, cw_label_y, label_color);
    manager.setPixel(cw_label_x + 4, cw_label_y + 1, label_color);
    manager.setPixel(cw_label_x + 4, cw_label_y + 2, label_color);
    manager.setPixel(cw_label_x + 4, cw_label_y + 3, label_color);
    manager.setPixel(cw_label_x + 4, cw_label_y + 4, label_color);
    manager.setPixel(cw_label_x + 5, cw_label_y + 3, label_color);
    manager.setPixel(cw_label_x + 6, cw_label_y + 2, label_color);
    manager.setPixel(cw_label_x + 7, cw_label_y + 3, label_color);
    manager.setPixel(cw_label_x + 8, cw_label_y, label_color);
    manager.setPixel(cw_label_x + 8, cw_label_y + 1, label_color);
    manager.setPixel(cw_label_x + 8, cw_label_y + 2, label_color);
    manager.setPixel(cw_label_x + 8, cw_label_y + 3, label_color);
    manager.setPixel(cw_label_x + 8, cw_label_y + 4, label_color);
    
    // Panel label
    const char* panel_label = (panel == 0) ? "PANEL 0" : "PANEL 1";
    int label_x = panel_x_offset + 2;
    int label_y = 2;
    
    // Simple text rendering for panel number
    for(int i = 0; i < 7; i++){
      manager.setPixel(label_x + i, label_y, label_color);
    }
    manager.setPixel(label_x + 6, label_y + 1, label_color);
  }
  
  // NO show() - called in main loop
}

/**
 * @brief Register all HUB75 test animations
 * @param manager HUB75 display manager reference
 */
inline void registerTestAnimations(arcos::manager::HUB75DisplayManager& manager){
  manager.registerAnimation("test_rgb_cycle", [&manager](uint32_t time_ms){
    testRGBCycle(manager, time_ms);
  });
  
  manager.registerAnimation("test_scrolling_bars", [&manager](uint32_t time_ms){
    testScrollingBars(manager, time_ms);
  });
  
  manager.registerAnimation("test_rainbow_wave", [&manager](uint32_t time_ms){
    testRainbowWave(manager, time_ms);
  });
  
  manager.registerAnimation("test_plasma", [&manager](uint32_t time_ms){
    testPlasma(manager, time_ms);
  });
  
  manager.registerAnimation("test_bouncing_ball", [&manager](uint32_t time_ms){
    testBouncingBall(manager, time_ms);
  });
  
  manager.registerAnimation("test_starfield", [&manager](uint32_t time_ms){
    testStarfield(manager, time_ms);
  });
  
  manager.registerAnimation("test_panel_axes", [&manager](uint32_t time_ms){
    testPanelAxes(manager, time_ms);
  });
}

} // namespace arcos::animations::hub75

#endif // HUB75_TEST_ANIMATIONS_HPP
