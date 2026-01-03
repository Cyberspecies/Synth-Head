/*****************************************************************
 * File:      IHalDisplay.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Display Hardware Abstraction Layer interface.
 *    Provides platform-independent access to various displays
 *    including HUB75 LED matrices and OLED displays.
 * 
 * Note:
 *    This is an output HAL interface. The middleware layer will
 *    use this to build graphics and UI services.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_DISPLAY_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_DISPLAY_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// Display Types
// ============================================================

/** Display type enumeration */
enum class DisplayType : uint8_t{
  HUB75_MATRIX,     // HUB75 LED matrix
  OLED_SH1107,      // OLED SH1107 128x128
  OLED_SSD1306,     // OLED SSD1306 128x64
  LCD_ILI9341,      // TFT LCD ILI9341
  GENERIC           // Generic display
};

/** Display rotation */
enum class DisplayRotation : uint8_t{
  ROTATE_0 = 0,
  ROTATE_90 = 1,
  ROTATE_180 = 2,
  ROTATE_270 = 3
};

// ============================================================
// HUB75 Display Interface
// ============================================================

/** HUB75 configuration */
struct Hub75Config{
  uint16_t width = 64;
  uint16_t height = 32;
  uint8_t chain_length = 1;       // Number of panels chained
  bool double_buffer = true;
  DisplayRotation rotation = DisplayRotation::ROTATE_0;
};

/** HUB75 Display Hardware Abstraction Interface */
class IHalHub75Display{
public:
  virtual ~IHalHub75Display() = default;
  
  /** Initialize HUB75 display
   * @param config Display configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const Hub75Config& config) = 0;
  
  /** Deinitialize display
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if display is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Set pixel color
   * @param x X coordinate
   * @param y Y coordinate
   * @param color RGB color
   * @return HalResult::OK on success
   */
  virtual HalResult setPixel(int16_t x, int16_t y, const RGB& color) = 0;
  
  /** Get pixel color
   * @param x X coordinate
   * @param y Y coordinate
   * @param color Reference to store color
   * @return HalResult::OK on success
   */
  virtual HalResult getPixel(int16_t x, int16_t y, RGB& color) const = 0;
  
  /** Fill entire display with color
   * @param color RGB color
   * @return HalResult::OK on success
   */
  virtual HalResult fill(const RGB& color) = 0;
  
  /** Clear display (fill with black)
   * @return HalResult::OK on success
   */
  virtual HalResult clear() = 0;
  
  /** Update display (flip buffers / refresh)
   * @return HalResult::OK on success
   */
  virtual HalResult show() = 0;
  
  /** Get display width
   * @return Width in pixels
   */
  virtual uint16_t getWidth() const = 0;
  
  /** Get display height
   * @return Height in pixels
   */
  virtual uint16_t getHeight() const = 0;
  
  /** Set brightness
   * @param brightness Brightness (0-255)
   * @return HalResult::OK on success
   */
  virtual HalResult setBrightness(uint8_t brightness) = 0;
  
  /** Get brightness
   * @return Current brightness
   */
  virtual uint8_t getBrightness() const = 0;
};

// ============================================================
// OLED Display Interface
// ============================================================

/** OLED configuration */
struct OledConfig{
  uint16_t width = 128;
  uint16_t height = 128;
  i2c_addr_t address = 0x3C;
  uint8_t contrast = 0xCF;
  bool flip_horizontal = false;
  bool flip_vertical = false;
  DisplayRotation rotation = DisplayRotation::ROTATE_0;
};

/** OLED Display Hardware Abstraction Interface */
class IHalOledDisplay{
public:
  virtual ~IHalOledDisplay() = default;
  
  /** Initialize OLED display
   * @param config Display configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const OledConfig& config) = 0;
  
  /** Deinitialize display
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if display is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Set pixel state
   * @param x X coordinate
   * @param y Y coordinate
   * @param on Pixel state (true = on)
   * @return HalResult::OK on success
   */
  virtual HalResult setPixel(int16_t x, int16_t y, bool on) = 0;
  
  /** Get pixel state
   * @param x X coordinate
   * @param y Y coordinate
   * @return true if pixel is on
   */
  virtual bool getPixel(int16_t x, int16_t y) const = 0;
  
  /** Fill entire display
   * @param on Fill state (true = all on, false = all off)
   * @return HalResult::OK on success
   */
  virtual HalResult fill(bool on) = 0;
  
  /** Clear display (all pixels off)
   * @return HalResult::OK on success
   */
  virtual HalResult clear() = 0;
  
  /** Update display (flush buffer)
   * @return HalResult::OK on success
   */
  virtual HalResult show() = 0;
  
  /** Get display width
   * @return Width in pixels
   */
  virtual uint16_t getWidth() const = 0;
  
  /** Get display height
   * @return Height in pixels
   */
  virtual uint16_t getHeight() const = 0;
  
  /** Set contrast
   * @param contrast Contrast value (0-255)
   * @return HalResult::OK on success
   */
  virtual HalResult setContrast(uint8_t contrast) = 0;
  
  /** Get contrast
   * @return Current contrast
   */
  virtual uint8_t getContrast() const = 0;
  
  /** Turn display on/off
   * @param on Display state
   * @return HalResult::OK on success
   */
  virtual HalResult setDisplayOn(bool on) = 0;
  
  /** Invert display colors
   * @param invert Invert state
   * @return HalResult::OK on success
   */
  virtual HalResult setInverted(bool invert) = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_DISPLAY_HPP_
