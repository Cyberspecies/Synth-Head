#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

class LedController{
private:
  // LED strip pin definitions from PIN_MAPPING_CPU.md
  static constexpr int LED_STRIP_0_PIN = 16;  // Not used in hardware
  static constexpr int LED_STRIP_1_PIN = 18;  // Left Fin - 13 LEDs
  static constexpr int LED_STRIP_2_PIN = 8;   // Tongue - 9 LEDs
  static constexpr int LED_STRIP_3_PIN = 39;  // Not used in hardware
  static constexpr int LED_STRIP_4_PIN = 38;  // Right Fin - 13 LEDs
  static constexpr int LED_STRIP_5_PIN = 37;  // Scale LEDs - 14 LEDs

  // LED counts per strip
  static constexpr int LEFT_FIN_LED_COUNT = 13;
  static constexpr int TONGUE_LED_COUNT = 9;
  static constexpr int RIGHT_FIN_LED_COUNT = 13;
  static constexpr int SCALE_LED_COUNT = 14;

  // NeoPixel strip objects
  Adafruit_NeoPixel* left_fin_strip;
  Adafruit_NeoPixel* tongue_strip;
  Adafruit_NeoPixel* right_fin_strip;
  Adafruit_NeoPixel* scale_strip;

  // Rainbow effect parameters
  float hue_offset;
  float hue_speed;
  unsigned long last_update_time;
  unsigned long update_interval_ms;

  // Helper methods
  uint32_t hsvToRgb(float hue, float saturation, float value);
  uint32_t hsvToWrgb(float hue, float saturation, float value);
  uint32_t hsvToWrgbNoWhite(float hue, float saturation, float value); // RGB only, no white channel
  void updateRainbowEffect();

public:
  LedController();
  ~LedController();

  // Initialization
  bool initialize();

  // Main update loop
  void update();

  // Rainbow effect controls
  void setRainbowSpeed(float speed);
  void setUpdateInterval(unsigned long interval_ms);

  // Individual strip controls
  void setLeftFinColor(uint32_t color);
  void setTongueColor(uint32_t color);
  void setRightFinColor(uint32_t color);
  void setScaleColor(uint32_t color);

  // Utility methods
  void setAllStripsColor(uint32_t color);
  void clearAllStrips();
  void showAllStrips();

  // Test patterns
  void runRainbowCycle();
  void runChaseEffect(uint32_t color, int delay_ms);
  void runBreathingEffect(uint32_t color, float breathing_speed);
};

#endif // LED_CONTROLLER_H