#ifndef LED_CONTROLLER_NEW_H
#define LED_CONTROLLER_NEW_H

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

class LedController{
private:
  // LED strip pin definitions from PIN_MAPPING_CPU.md
  static constexpr int LEFT_FIN_PIN = 18;    // GPIO 18 - Left Fin
  static constexpr int RIGHT_FIN_PIN = 38;   // GPIO 38 - Right Fin
  static constexpr int TONGUE_PIN = 8;       // GPIO 8 - Tongue
  static constexpr int SCALE_PIN = 37;       // GPIO 37 - Scale LEDs

  // LED counts per strip
  static constexpr int LEFT_FIN_LED_COUNT = 13;
  static constexpr int RIGHT_FIN_LED_COUNT = 13;
  static constexpr int TONGUE_LED_COUNT = 9;
  static constexpr int SCALE_LED_COUNT = 14;

  // NeoPixel strip objects (RGBW format: NEO_GRBW)
  Adafruit_NeoPixel* left_fin_strip;
  Adafruit_NeoPixel* right_fin_strip;
  Adafruit_NeoPixel* tongue_strip;
  Adafruit_NeoPixel* scale_strip;

  // Initialization state
  bool is_initialized;

public:
  LedController();
  ~LedController();

  // Initialization
  bool initialize();

  // Update all LED strips from UART data
  void updateFromUartData(const uint8_t* left_fin_data, 
                          const uint8_t* right_fin_data,
                          const uint8_t* tongue_data, 
                          const uint8_t* scale_data);

  // Update individual strips from RGBW data
  void updateLeftFin(const uint8_t* rgbw_data);
  void updateRightFin(const uint8_t* rgbw_data);
  void updateTongue(const uint8_t* rgbw_data);
  void updateScale(const uint8_t* rgbw_data);

  // Set individual LED by index and RGBW values
  void setLeftFinLed(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  void setRightFinLed(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  void setTongueLed(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  void setScaleLed(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

  // Set entire strip to single color
  void setLeftFinColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  void setRightFinColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  void setTongueColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  void setScaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

  // Show all strips (apply changes)
  void showAll();

  // Clear all strips
  void clearAll();

  // Get strip pointers (for advanced usage)
  Adafruit_NeoPixel* getLeftFinStrip() { return left_fin_strip; }
  Adafruit_NeoPixel* getRightFinStrip() { return right_fin_strip; }
  Adafruit_NeoPixel* getTongueStrip() { return tongue_strip; }
  Adafruit_NeoPixel* getScaleStrip() { return scale_strip; }

  // Test patterns
  void testPattern();
  void rainbowCycle(uint8_t wait);
};

#endif // LED_CONTROLLER_NEW_H
