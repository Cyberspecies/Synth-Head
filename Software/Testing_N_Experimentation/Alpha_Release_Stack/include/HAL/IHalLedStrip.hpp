/*****************************************************************
 * File:      IHalLedStrip.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    LED Strip Hardware Abstraction Layer interface.
 *    Provides platform-independent access to addressable LED
 *    strips (NeoPixel/WS2812/SK6812 RGBW).
 * 
 * Note:
 *    This is an output HAL interface. The middleware layer will
 *    use this to build animation and lighting effects.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_LED_STRIP_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_LED_STRIP_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// LED Strip Configuration
// ============================================================

/** LED strip type */
enum class LedStripType : uint8_t{
  WS2812_RGB,      // WS2812 RGB
  WS2812B_RGB,     // WS2812B RGB
  SK6812_RGB,      // SK6812 RGB
  SK6812_RGBW,     // SK6812 RGBW (with white channel)
  NEOPIXEL_RGB,    // Generic NeoPixel RGB
  NEOPIXEL_RGBW    // Generic NeoPixel RGBW
};

/** LED strip color order */
enum class LedColorOrder : uint8_t{
  RGB,
  GRB,
  BGR,
  RGBW,
  GRBW,
  WRGB
};

/** LED strip configuration */
struct LedStripConfig{
  gpio_pin_t pin = 0;
  uint16_t led_count = 0;
  LedStripType type = LedStripType::SK6812_RGBW;
  LedColorOrder color_order = LedColorOrder::GRBW;
  uint8_t brightness = 255;      // Global brightness (0-255)
};

// ============================================================
// LED Strip Interface
// ============================================================

/** LED Strip Hardware Abstraction Interface
 * 
 * Provides platform-independent access to addressable LED strips.
 * Supports RGB and RGBW LED strips with various color orders.
 */
class IHalLedStrip{
public:
  virtual ~IHalLedStrip() = default;
  
  /** Initialize LED strip
   * @param config Strip configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const LedStripConfig& config) = 0;
  
  /** Deinitialize LED strip
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if strip is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Set single LED color (RGB)
   * @param index LED index (0-based)
   * @param color RGB color
   * @return HalResult::OK on success
   */
  virtual HalResult setPixel(uint16_t index, const RGB& color) = 0;
  
  /** Set single LED color (RGBW)
   * @param index LED index (0-based)
   * @param color RGBW color
   * @return HalResult::OK on success
   */
  virtual HalResult setPixelRGBW(uint16_t index, const RGBW& color) = 0;
  
  /** Set single LED color by components
   * @param index LED index (0-based)
   * @param r Red component (0-255)
   * @param g Green component (0-255)
   * @param b Blue component (0-255)
   * @param w White component (0-255, optional)
   * @return HalResult::OK on success
   */
  virtual HalResult setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) = 0;
  
  /** Fill all LEDs with single color
   * @param color RGB color
   * @return HalResult::OK on success
   */
  virtual HalResult fill(const RGB& color) = 0;
  
  /** Fill all LEDs with single RGBW color
   * @param color RGBW color
   * @return HalResult::OK on success
   */
  virtual HalResult fillRGBW(const RGBW& color) = 0;
  
  /** Fill range of LEDs with color
   * @param start Start index
   * @param count Number of LEDs to fill
   * @param color RGB color
   * @return HalResult::OK on success
   */
  virtual HalResult fillRange(uint16_t start, uint16_t count, const RGB& color) = 0;
  
  /** Clear all LEDs (set to black)
   * @return HalResult::OK on success
   */
  virtual HalResult clear() = 0;
  
  /** Update LED strip (send data to LEDs)
   * @return HalResult::OK on success
   */
  virtual HalResult show() = 0;
  
  /** Set global brightness
   * @param brightness Brightness value (0-255)
   * @return HalResult::OK on success
   */
  virtual HalResult setBrightness(uint8_t brightness) = 0;
  
  /** Get global brightness
   * @return Current brightness (0-255)
   */
  virtual uint8_t getBrightness() const = 0;
  
  /** Get LED count
   * @return Number of LEDs in strip
   */
  virtual uint16_t getLedCount() const = 0;
  
  /** Get pixel color at index
   * @param index LED index
   * @param color Reference to RGBW to fill
   * @return HalResult::OK on success
   */
  virtual HalResult getPixel(uint16_t index, RGBW& color) const = 0;
  
  /** Set raw buffer data
   * @param data RGBW data buffer (4 bytes per LED)
   * @param length Buffer length in bytes
   * @return HalResult::OK on success
   */
  virtual HalResult setBuffer(const uint8_t* data, size_t length) = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_LED_STRIP_HPP_
