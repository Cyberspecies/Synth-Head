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
}

} // namespace arcos::animations::hub75

#endif // HUB75_TEST_ANIMATIONS_HPP
