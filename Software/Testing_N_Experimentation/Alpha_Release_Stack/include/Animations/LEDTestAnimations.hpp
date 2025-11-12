/*****************************************************************
 * File:      LEDTestAnimations.hpp
 * Category:  Animations/Test
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Test and demonstration animations for NeoPixel RGBW LEDs.
 *    Various visual effects for runtime operation.
 * 
 * Usage:
 *    Register these functions with LEDAnimationManager
 *****************************************************************/

#ifndef LED_TEST_ANIMATIONS_HPP
#define LED_TEST_ANIMATIONS_HPP

#include "Manager/LEDAnimationManager.hpp"
#include <cmath>

namespace arcos::animations::led{

/**
 * @brief Test animation: Rainbow wave
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void testRainbow(LedDataPayload& led_data, uint32_t time_ms){
  float time_sec = time_ms / 1000.0f;
  
  for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
    float hue = fmodf((i / (float)LED_COUNT_TOTAL) + (time_sec * 0.2f), 1.0f);
    float h = hue * 6.0f;
    int region = static_cast<int>(h);
    float f = h - region;
    
    uint8_t q = static_cast<uint8_t>(255 * (1.0f - f));
    uint8_t t = static_cast<uint8_t>(255 * f);
    
    switch(region % 6){
      case 0: led_data.leds[i] = RgbwColor(255, t, 0, 0); break;
      case 1: led_data.leds[i] = RgbwColor(q, 255, 0, 0); break;
      case 2: led_data.leds[i] = RgbwColor(0, 255, t, 0); break;
      case 3: led_data.leds[i] = RgbwColor(0, q, 255, 0); break;
      case 4: led_data.leds[i] = RgbwColor(t, 0, 255, 0); break;
      case 5: led_data.leds[i] = RgbwColor(255, 0, q, 0); break;
    }
  }
}

/**
 * @brief Test animation: Breathing effect with different colors per strip
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void testBreathing(LedDataPayload& led_data, uint32_t time_ms){
  float time_sec = time_ms / 1000.0f;
  uint8_t brightness = static_cast<uint8_t>(127.5f + 127.5f * sinf(time_sec * 2.0f));
  
  led_data.setLeftFinColor(RgbwColor(brightness, 0, 0, 0));
  led_data.setTongueColor(RgbwColor(0, brightness, 0, 0));
  led_data.setRightFinColor(RgbwColor(0, 0, brightness, 0));
  led_data.setScaleColor(RgbwColor(0, 0, 0, brightness));
}

/**
 * @brief Test animation: Wave effect across all strips
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void testWave(LedDataPayload& led_data, uint32_t time_ms){
  float time_sec = time_ms / 1000.0f;
  
  for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
    float wave = sinf((i * 0.3f) + (time_sec * 3.0f));
    uint8_t brightness = static_cast<uint8_t>(127.5f + 127.5f * wave);
    led_data.leds[i] = RgbwColor(brightness, brightness / 2, 0, 0);
  }
}

/**
 * @brief Test animation: Alternating strip colors
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void testAlternating(LedDataPayload& led_data, uint32_t time_ms){
  uint32_t phase = (time_ms / 500) % 4;
  
  switch(phase){
    case 0:
      led_data.setLeftFinColor(RgbwColor(255, 0, 0, 0));
      led_data.setTongueColor(RgbwColor(0, 0, 0, 0));
      led_data.setRightFinColor(RgbwColor(255, 0, 0, 0));
      led_data.setScaleColor(RgbwColor(0, 0, 0, 0));
      break;
    case 1:
      led_data.setLeftFinColor(RgbwColor(0, 0, 0, 0));
      led_data.setTongueColor(RgbwColor(0, 255, 0, 0));
      led_data.setRightFinColor(RgbwColor(0, 0, 0, 0));
      led_data.setScaleColor(RgbwColor(0, 255, 0, 0));
      break;
    case 2:
      led_data.setLeftFinColor(RgbwColor(0, 0, 255, 0));
      led_data.setTongueColor(RgbwColor(0, 0, 0, 0));
      led_data.setRightFinColor(RgbwColor(0, 0, 255, 0));
      led_data.setScaleColor(RgbwColor(0, 0, 0, 0));
      break;
    case 3:
      led_data.setLeftFinColor(RgbwColor(0, 0, 0, 0));
      led_data.setTongueColor(RgbwColor(0, 0, 0, 255));
      led_data.setRightFinColor(RgbwColor(0, 0, 0, 0));
      led_data.setScaleColor(RgbwColor(0, 0, 0, 255));
      break;
  }
}

/**
 * @brief Test animation: Fire effect
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void testFire(LedDataPayload& led_data, uint32_t time_ms){
  for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
    // Randomized fire colors
    float flicker = sinf((time_ms / 50.0f) + (i * 0.5f));
    uint8_t red = 255;
    uint8_t green = static_cast<uint8_t>(100 + 100 * flicker);
    uint8_t blue = 0;
    
    led_data.leds[i] = RgbwColor(red, green, blue, 0);
  }
}

/**
 * @brief Test animation: Theater chase
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void testTheaterChase(LedDataPayload& led_data, uint32_t time_ms){
  uint32_t position = (time_ms / 100) % 3;
  
  for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
    if(i % 3 == position){
      led_data.leds[i] = RgbwColor(255, 255, 255, 0);
    }else{
      led_data.leds[i] = RgbwColor(0, 0, 0, 0);
    }
  }
}

/**
 * @brief Test animation: Color fade through strips
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void testColorFade(LedDataPayload& led_data, uint32_t time_ms){
  float time_sec = time_ms / 1000.0f;
  float hue = fmodf(time_sec * 0.1f, 1.0f);
  float h = hue * 6.0f;
  int region = static_cast<int>(h);
  float f = h - region;
  
  uint8_t brightness = 200;
  uint8_t q = static_cast<uint8_t>(brightness * (1.0f - f));
  uint8_t t = static_cast<uint8_t>(brightness * f);
  
  RgbwColor color;
  switch(region % 6){
    case 0: color = RgbwColor(brightness, t, 0, 0); break;
    case 1: color = RgbwColor(q, brightness, 0, 0); break;
    case 2: color = RgbwColor(0, brightness, t, 0); break;
    case 3: color = RgbwColor(0, q, brightness, 0); break;
    case 4: color = RgbwColor(t, 0, brightness, 0); break;
    case 5: color = RgbwColor(brightness, 0, q, 0); break;
  }
  
  led_data.setAllColor(color);
}

/**
 * @brief Register all LED test animations
 * @param manager LED animation manager reference
 */
inline void registerTestAnimations(arcos::manager::LEDAnimationManager& manager){
  manager.registerAnimation("test_rainbow", testRainbow);
  manager.registerAnimation("test_breathing", testBreathing);
  manager.registerAnimation("test_wave", testWave);
  manager.registerAnimation("test_alternating", testAlternating);
  manager.registerAnimation("test_fire", testFire);
  manager.registerAnimation("test_theater_chase", testTheaterChase);
  manager.registerAnimation("test_color_fade", testColorFade);
}

} // namespace arcos::animations::led

#endif // LED_TEST_ANIMATIONS_HPP
