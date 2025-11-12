/*****************************************************************
 * File:      LEDBootAnimations.hpp
 * Category:  Animations/Boot
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Boot animations for NeoPixel RGBW LED strips.
 *    Contains initialization and startup visual effects.
 * 
 * Usage:
 *    Register these functions with LEDAnimationManager
 *****************************************************************/

#ifndef LED_BOOT_ANIMATIONS_HPP
#define LED_BOOT_ANIMATIONS_HPP

#include "Manager/LEDAnimationManager.hpp"
#include <cmath>

namespace arcos::animations::led{

/**
 * @brief Boot animation: Rainbow startup sequence
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootRainbowStartup(LedDataPayload& led_data, uint32_t time_ms){
  float progress = (time_ms % 1500) / 1500.0f;
  
  // Rainbow wave that fills from start to end
  for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
    float led_progress = i / (float)LED_COUNT_TOTAL;
    
    if(led_progress <= progress){
      float hue = fmodf(led_progress + (time_ms / 3000.0f), 1.0f);
      float h = hue * 6.0f;
      int region = static_cast<int>(h);
      float f = h - region;
      
      uint8_t brightness = static_cast<uint8_t>(200);
      uint8_t q = static_cast<uint8_t>(brightness * (1.0f - f));
      uint8_t t = static_cast<uint8_t>(brightness * f);
      
      switch(region % 6){
        case 0: led_data.leds[i] = RgbwColor(brightness, t, 0, 0); break;
        case 1: led_data.leds[i] = RgbwColor(q, brightness, 0, 0); break;
        case 2: led_data.leds[i] = RgbwColor(0, brightness, t, 0); break;
        case 3: led_data.leds[i] = RgbwColor(0, q, brightness, 0); break;
        case 4: led_data.leds[i] = RgbwColor(t, 0, brightness, 0); break;
        case 5: led_data.leds[i] = RgbwColor(brightness, 0, q, 0); break;
      }
    }else{
      led_data.leds[i] = RgbwColor(0, 0, 0, 0);
    }
  }
}

/**
 * @brief Boot animation: Sequential strip activation
 * Activates each LED strip one by one with color indication
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootSequentialActivation(LedDataPayload& led_data, uint32_t time_ms){
  // Clear all first
  led_data.setAllColor(RgbwColor(0, 0, 0, 0));
  
  uint32_t phase = time_ms / 375;  // Each strip gets 375ms
  uint8_t brightness = 200;
  
  // Left fin - Red (phase 0+)
  if(phase >= 0){
    led_data.setLeftFinColor(RgbwColor(brightness, 0, 0, 0));
  }
  // Tongue - Green (phase 1+)
  if(phase >= 1){
    led_data.setTongueColor(RgbwColor(0, brightness, 0, 0));
  }
  // Right fin - Blue (phase 2+)
  if(phase >= 2){
    led_data.setRightFinColor(RgbwColor(0, 0, brightness, 0));
  }
  // Scale - White (phase 3+)
  if(phase >= 3){
    led_data.setScaleColor(RgbwColor(0, 0, 0, brightness));
  }
}

/**
 * @brief Boot animation: Pulsing brightness wave
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootPulseWave(LedDataPayload& led_data, uint32_t time_ms){
  float time_sec = time_ms / 1000.0f;
  uint8_t brightness = static_cast<uint8_t>(127.5f + 127.5f * sinf(time_sec * 4.0f));
  
  // Pulse with different colors per strip
  led_data.setLeftFinColor(RgbwColor(brightness, 0, brightness / 2, 0));
  led_data.setTongueColor(RgbwColor(0, brightness, brightness / 2, 0));
  led_data.setRightFinColor(RgbwColor(brightness / 2, 0, brightness, 0));
  led_data.setScaleColor(RgbwColor(0, 0, 0, brightness));
}

/**
 * @brief Boot animation: Running light chase effect
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootChaseEffect(LedDataPayload& led_data, uint32_t time_ms){
  // Clear all
  led_data.setAllColor(RgbwColor(0, 0, 0, 0));
  
  // Calculate chase position
  uint32_t position = (time_ms / 20) % LED_COUNT_TOTAL;
  
  // Set chase pixels with trailing fade
  for(int i = 0; i < 10; i++){
    int idx = (position - i + LED_COUNT_TOTAL) % LED_COUNT_TOTAL;
    uint8_t brightness = static_cast<uint8_t>(255 - (i * 25));
    
    // Color based on strip
    if(idx < LED_COUNT_LEFT_FIN){
      led_data.leds[idx] = RgbwColor(brightness, 0, 0, 0);
    }else if(idx < LED_COUNT_LEFT_FIN + LED_COUNT_TONGUE){
      led_data.leds[idx] = RgbwColor(0, brightness, 0, 0);
    }else if(idx < LED_COUNT_LEFT_FIN + LED_COUNT_TONGUE + LED_COUNT_RIGHT_FIN){
      led_data.leds[idx] = RgbwColor(0, 0, brightness, 0);
    }else{
      led_data.leds[idx] = RgbwColor(0, 0, 0, brightness);
    }
  }
}

/**
 * @brief Boot animation: Color wipe startup
 * @param led_data LED data payload reference
 * @param time_ms Animation time in milliseconds
 */
inline void bootColorWipe(LedDataPayload& led_data, uint32_t time_ms){
  float progress = (time_ms % 1500) / 1500.0f;
  uint16_t lit_count = static_cast<uint16_t>(LED_COUNT_TOTAL * progress);
  
  for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
    if(i < lit_count){
      // Cyan color for boot
      led_data.leds[i] = RgbwColor(0, 200, 200, 0);
    }else{
      led_data.leds[i] = RgbwColor(0, 0, 0, 0);
    }
  }
}

/**
 * @brief Register all LED boot animations
 * @param manager LED animation manager reference
 */
inline void registerBootAnimations(arcos::manager::LEDAnimationManager& manager){
  manager.registerAnimation("boot_rainbow_startup", bootRainbowStartup);
  manager.registerAnimation("boot_sequential_activation", bootSequentialActivation);
  manager.registerAnimation("boot_pulse_wave", bootPulseWave);
  manager.registerAnimation("boot_chase_effect", bootChaseEffect);
  manager.registerAnimation("boot_color_wipe", bootColorWipe);
}

} // namespace arcos::animations::led

#endif // LED_BOOT_ANIMATIONS_HPP
