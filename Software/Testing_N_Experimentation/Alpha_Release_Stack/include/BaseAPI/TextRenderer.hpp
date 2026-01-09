/*****************************************************************
 * File:      TextRenderer.hpp
 * Category:  include/BaseAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Text rendering for displays using bitmap fonts.
 *    Provides character and string drawing with various options.
 * 
 * Features:
 *    - Built-in 5x7 pixel font (ASCII 32-127)
 *    - Scalable text (1x, 2x, 3x, etc.)
 *    - Foreground/background colors
 *    - Text alignment options
 *    - Works with FrameBuffer from DisplayManager
 * 
 * Usage:
 *    TextRenderer text;
 *    text.setScale(2);
 *    text.setColor(colors::WHITE, colors::BLACK);
 *    text.drawString(framebuffer, 10, 10, "Hello!");
 *****************************************************************/

#ifndef ARCOS_INCLUDE_BASEAPI_TEXT_RENDERER_HPP_
#define ARCOS_INCLUDE_BASEAPI_TEXT_RENDERER_HPP_

#include "BaseTypes.hpp"
#include "DisplayManager.hpp"
#include <cstring>

namespace arcos::base {

// ============================================================
// Font Data - 5x7 ASCII Font (characters 32-127)
// ============================================================

/**
 * 5x7 bitmap font data
 * Each character is 5 pixels wide, 7 pixels tall
 * Stored as 5 bytes per character (one byte per column)
 */
namespace font5x7 {

// Character width and height
constexpr uint8_t CHAR_WIDTH = 5;
constexpr uint8_t CHAR_HEIGHT = 7;
constexpr uint8_t CHAR_SPACING = 1;  // Pixels between characters

// Font bitmap data - 5 bytes per character, column-major
// Covers ASCII 32 (space) to 127 (DEL)
static const uint8_t DATA[] = {
  // Space (32)
  0x00, 0x00, 0x00, 0x00, 0x00,
  // ! (33)
  0x00, 0x00, 0x5F, 0x00, 0x00,
  // " (34)
  0x00, 0x07, 0x00, 0x07, 0x00,
  // # (35)
  0x14, 0x7F, 0x14, 0x7F, 0x14,
  // $ (36)
  0x24, 0x2A, 0x7F, 0x2A, 0x12,
  // % (37)
  0x23, 0x13, 0x08, 0x64, 0x62,
  // & (38)
  0x36, 0x49, 0x55, 0x22, 0x50,
  // ' (39)
  0x00, 0x05, 0x03, 0x00, 0x00,
  // ( (40)
  0x00, 0x1C, 0x22, 0x41, 0x00,
  // ) (41)
  0x00, 0x41, 0x22, 0x1C, 0x00,
  // * (42)
  0x08, 0x2A, 0x1C, 0x2A, 0x08,
  // + (43)
  0x08, 0x08, 0x3E, 0x08, 0x08,
  // , (44)
  0x00, 0x50, 0x30, 0x00, 0x00,
  // - (45)
  0x08, 0x08, 0x08, 0x08, 0x08,
  // . (46)
  0x00, 0x60, 0x60, 0x00, 0x00,
  // / (47)
  0x20, 0x10, 0x08, 0x04, 0x02,
  // 0 (48)
  0x3E, 0x51, 0x49, 0x45, 0x3E,
  // 1 (49)
  0x00, 0x42, 0x7F, 0x40, 0x00,
  // 2 (50)
  0x42, 0x61, 0x51, 0x49, 0x46,
  // 3 (51)
  0x21, 0x41, 0x45, 0x4B, 0x31,
  // 4 (52)
  0x18, 0x14, 0x12, 0x7F, 0x10,
  // 5 (53)
  0x27, 0x45, 0x45, 0x45, 0x39,
  // 6 (54)
  0x3C, 0x4A, 0x49, 0x49, 0x30,
  // 7 (55)
  0x01, 0x71, 0x09, 0x05, 0x03,
  // 8 (56)
  0x36, 0x49, 0x49, 0x49, 0x36,
  // 9 (57)
  0x06, 0x49, 0x49, 0x29, 0x1E,
  // : (58)
  0x00, 0x36, 0x36, 0x00, 0x00,
  // ; (59)
  0x00, 0x56, 0x36, 0x00, 0x00,
  // < (60)
  0x00, 0x08, 0x14, 0x22, 0x41,
  // = (61)
  0x14, 0x14, 0x14, 0x14, 0x14,
  // > (62)
  0x41, 0x22, 0x14, 0x08, 0x00,
  // ? (63)
  0x02, 0x01, 0x51, 0x09, 0x06,
  // @ (64)
  0x32, 0x49, 0x79, 0x41, 0x3E,
  // A (65)
  0x7E, 0x11, 0x11, 0x11, 0x7E,
  // B (66)
  0x7F, 0x49, 0x49, 0x49, 0x36,
  // C (67)
  0x3E, 0x41, 0x41, 0x41, 0x22,
  // D (68)
  0x7F, 0x41, 0x41, 0x22, 0x1C,
  // E (69)
  0x7F, 0x49, 0x49, 0x49, 0x41,
  // F (70)
  0x7F, 0x09, 0x09, 0x01, 0x01,
  // G (71)
  0x3E, 0x41, 0x41, 0x51, 0x32,
  // H (72)
  0x7F, 0x08, 0x08, 0x08, 0x7F,
  // I (73)
  0x00, 0x41, 0x7F, 0x41, 0x00,
  // J (74)
  0x20, 0x40, 0x41, 0x3F, 0x01,
  // K (75)
  0x7F, 0x08, 0x14, 0x22, 0x41,
  // L (76)
  0x7F, 0x40, 0x40, 0x40, 0x40,
  // M (77)
  0x7F, 0x02, 0x04, 0x02, 0x7F,
  // N (78)
  0x7F, 0x04, 0x08, 0x10, 0x7F,
  // O (79)
  0x3E, 0x41, 0x41, 0x41, 0x3E,
  // P (80)
  0x7F, 0x09, 0x09, 0x09, 0x06,
  // Q (81)
  0x3E, 0x41, 0x51, 0x21, 0x5E,
  // R (82)
  0x7F, 0x09, 0x19, 0x29, 0x46,
  // S (83)
  0x46, 0x49, 0x49, 0x49, 0x31,
  // T (84)
  0x01, 0x01, 0x7F, 0x01, 0x01,
  // U (85)
  0x3F, 0x40, 0x40, 0x40, 0x3F,
  // V (86)
  0x1F, 0x20, 0x40, 0x20, 0x1F,
  // W (87)
  0x7F, 0x20, 0x18, 0x20, 0x7F,
  // X (88)
  0x63, 0x14, 0x08, 0x14, 0x63,
  // Y (89)
  0x03, 0x04, 0x78, 0x04, 0x03,
  // Z (90)
  0x61, 0x51, 0x49, 0x45, 0x43,
  // [ (91)
  0x00, 0x00, 0x7F, 0x41, 0x41,
  // \ (92)
  0x02, 0x04, 0x08, 0x10, 0x20,
  // ] (93)
  0x41, 0x41, 0x7F, 0x00, 0x00,
  // ^ (94)
  0x04, 0x02, 0x01, 0x02, 0x04,
  // _ (95)
  0x40, 0x40, 0x40, 0x40, 0x40,
  // ` (96)
  0x00, 0x01, 0x02, 0x04, 0x00,
  // a (97)
  0x20, 0x54, 0x54, 0x54, 0x78,
  // b (98)
  0x7F, 0x48, 0x44, 0x44, 0x38,
  // c (99)
  0x38, 0x44, 0x44, 0x44, 0x20,
  // d (100)
  0x38, 0x44, 0x44, 0x48, 0x7F,
  // e (101)
  0x38, 0x54, 0x54, 0x54, 0x18,
  // f (102)
  0x08, 0x7E, 0x09, 0x01, 0x02,
  // g (103)
  0x08, 0x14, 0x54, 0x54, 0x3C,
  // h (104)
  0x7F, 0x08, 0x04, 0x04, 0x78,
  // i (105)
  0x00, 0x44, 0x7D, 0x40, 0x00,
  // j (106)
  0x20, 0x40, 0x44, 0x3D, 0x00,
  // k (107)
  0x00, 0x7F, 0x10, 0x28, 0x44,
  // l (108)
  0x00, 0x41, 0x7F, 0x40, 0x00,
  // m (109)
  0x7C, 0x04, 0x18, 0x04, 0x78,
  // n (110)
  0x7C, 0x08, 0x04, 0x04, 0x78,
  // o (111)
  0x38, 0x44, 0x44, 0x44, 0x38,
  // p (112)
  0x7C, 0x14, 0x14, 0x14, 0x08,
  // q (113)
  0x08, 0x14, 0x14, 0x18, 0x7C,
  // r (114)
  0x7C, 0x08, 0x04, 0x04, 0x08,
  // s (115)
  0x48, 0x54, 0x54, 0x54, 0x20,
  // t (116)
  0x04, 0x3F, 0x44, 0x40, 0x20,
  // u (117)
  0x3C, 0x40, 0x40, 0x20, 0x7C,
  // v (118)
  0x1C, 0x20, 0x40, 0x20, 0x1C,
  // w (119)
  0x3C, 0x40, 0x30, 0x40, 0x3C,
  // x (120)
  0x44, 0x28, 0x10, 0x28, 0x44,
  // y (121)
  0x0C, 0x50, 0x50, 0x50, 0x3C,
  // z (122)
  0x44, 0x64, 0x54, 0x4C, 0x44,
  // { (123)
  0x00, 0x08, 0x36, 0x41, 0x00,
  // | (124)
  0x00, 0x00, 0x7F, 0x00, 0x00,
  // } (125)
  0x00, 0x41, 0x36, 0x08, 0x00,
  // ~ (126)
  0x08, 0x08, 0x2A, 0x1C, 0x08,
  // DEL (127) - Block character
  0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
};

/** Get font data for a character */
inline const uint8_t* getCharData(char c) {
  if (c < 32 || c > 127) c = '?';
  return &DATA[(c - 32) * 5];
}

} // namespace font5x7

// ============================================================
// Text Alignment
// ============================================================

enum class TextAlign : uint8_t {
  LEFT = 0,
  CENTER = 1,
  RIGHT = 2
};

enum class TextVAlign : uint8_t {
  TOP = 0,
  MIDDLE = 1,
  BOTTOM = 2
};

// ============================================================
// TextRenderer Class
// ============================================================

/**
 * TextRenderer - Renders text to FrameBuffer
 * 
 * Supports the built-in 5x7 font with scaling and color options.
 */
class TextRenderer {
public:
  TextRenderer()
    : scale_(1)
    , fg_color_(0xFFFF)  // White
    , bg_color_(0x0000)  // Black
    , transparent_bg_(true)
    , wrap_(false)
    , h_align_(TextAlign::LEFT)
    , v_align_(TextVAlign::TOP)
  {}
  
  // --------------------------------------------------------
  // Configuration
  // --------------------------------------------------------
  
  /** Set text scale (1 = normal, 2 = double, etc.) */
  void setScale(uint8_t scale) {
    scale_ = (scale > 0) ? scale : 1;
  }
  
  uint8_t getScale() const { return scale_; }
  
  /** Set foreground color (RGB565) */
  void setFgColor(uint16_t color) { fg_color_ = color; }
  uint16_t getFgColor() const { return fg_color_; }
  
  /** Set background color (RGB565) */
  void setBgColor(uint16_t color) { bg_color_ = color; }
  uint16_t getBgColor() const { return bg_color_; }
  
  /** Set both colors */
  void setColors(uint16_t fg, uint16_t bg) {
    fg_color_ = fg;
    bg_color_ = bg;
  }
  
  /** Enable/disable transparent background */
  void setTransparent(bool transparent) { transparent_bg_ = transparent; }
  bool isTransparent() const { return transparent_bg_; }
  
  /** Enable/disable text wrapping */
  void setWrap(bool wrap) { wrap_ = wrap; }
  bool isWrap() const { return wrap_; }
  
  /** Set text alignment */
  void setAlign(TextAlign h, TextVAlign v = TextVAlign::TOP) {
    h_align_ = h;
    v_align_ = v;
  }
  
  // --------------------------------------------------------
  // Metrics
  // --------------------------------------------------------
  
  /** Get character width in pixels (with current scale) */
  uint16_t charWidth() const {
    return font5x7::CHAR_WIDTH * scale_;
  }
  
  /** Get character height in pixels (with current scale) */
  uint16_t charHeight() const {
    return font5x7::CHAR_HEIGHT * scale_;
  }
  
  /** Get spacing between characters */
  uint16_t charSpacing() const {
    return font5x7::CHAR_SPACING * scale_;
  }
  
  /** Get width of a string in pixels */
  uint16_t stringWidth(const char* str) const {
    if (!str) return 0;
    size_t len = strlen(str);
    if (len == 0) return 0;
    return len * (charWidth() + charSpacing()) - charSpacing();
  }
  
  /** Get height of a string (single line) */
  uint16_t stringHeight() const {
    return charHeight();
  }
  
  // --------------------------------------------------------
  // Drawing - Character
  // --------------------------------------------------------
  
  /**
   * Draw a single character
   * @param fb Target framebuffer
   * @param x X position (left edge)
   * @param y Y position (top edge)
   * @param c Character to draw
   * @return Width of character drawn (including spacing)
   */
  uint16_t drawChar(FrameBuffer& fb, int16_t x, int16_t y, char c) {
    const uint8_t* charData = font5x7::getCharData(c);
    
    // Draw each column of the character
    for (uint8_t col = 0; col < font5x7::CHAR_WIDTH; col++) {
      uint8_t colData = charData[col];
      
      // Draw each row in the column
      for (uint8_t row = 0; row < font5x7::CHAR_HEIGHT; row++) {
        bool pixel = (colData >> row) & 0x01;
        
        if (pixel) {
          // Draw foreground pixel (scaled)
          drawScaledPixel(fb, x + col * scale_, y + row * scale_, fg_color_);
        } else if (!transparent_bg_) {
          // Draw background pixel (scaled)
          drawScaledPixel(fb, x + col * scale_, y + row * scale_, bg_color_);
        }
      }
    }
    
    // Draw spacing background if not transparent
    if (!transparent_bg_) {
      for (uint8_t s = 0; s < font5x7::CHAR_SPACING; s++) {
        for (uint8_t row = 0; row < font5x7::CHAR_HEIGHT; row++) {
          drawScaledPixel(fb, x + (font5x7::CHAR_WIDTH + s) * scale_, 
                         y + row * scale_, bg_color_);
        }
      }
    }
    
    return charWidth() + charSpacing();
  }
  
  // --------------------------------------------------------
  // Drawing - String
  // --------------------------------------------------------
  
  /**
   * Draw a string
   * @param fb Target framebuffer
   * @param x X position
   * @param y Y position
   * @param str String to draw
   * @return Total width of text drawn
   */
  uint16_t drawString(FrameBuffer& fb, int16_t x, int16_t y, const char* str) {
    if (!str) return 0;
    
    // Apply alignment
    x = applyHAlign(x, str, fb.width());
    y = applyVAlign(y, fb.height());
    
    int16_t startX = x;
    int16_t cursorX = x;
    int16_t cursorY = y;
    
    while (*str) {
      char c = *str++;
      
      // Handle newline
      if (c == '\n') {
        cursorX = startX;
        cursorY += charHeight() + charSpacing();
        continue;
      }
      
      // Handle carriage return
      if (c == '\r') {
        cursorX = startX;
        continue;
      }
      
      // Handle tab (4 spaces)
      if (c == '\t') {
        cursorX += (charWidth() + charSpacing()) * 4;
        continue;
      }
      
      // Check for wrap
      if (wrap_ && cursorX + charWidth() > fb.width()) {
        cursorX = startX;
        cursorY += charHeight() + charSpacing();
      }
      
      // Draw character
      drawChar(fb, cursorX, cursorY, c);
      cursorX += charWidth() + charSpacing();
    }
    
    return cursorX - x;
  }
  
  /**
   * Draw centered string
   * @param fb Target framebuffer
   * @param y Y position
   * @param str String to draw
   */
  void drawCentered(FrameBuffer& fb, int16_t y, const char* str) {
    int16_t x = (fb.width() - stringWidth(str)) / 2;
    drawString(fb, x, y, str);
  }
  
  /**
   * Draw right-aligned string
   * @param fb Target framebuffer
   * @param x Right edge X position
   * @param y Y position
   * @param str String to draw
   */
  void drawRightAligned(FrameBuffer& fb, int16_t x, int16_t y, const char* str) {
    int16_t startX = x - stringWidth(str);
    drawString(fb, startX, y, str);
  }
  
  /**
   * Draw string in a box (with alignment)
   * @param fb Target framebuffer
   * @param x Box X position
   * @param y Box Y position
   * @param w Box width
   * @param h Box height
   * @param str String to draw
   */
  void drawInBox(FrameBuffer& fb, int16_t x, int16_t y, 
                 uint16_t w, uint16_t h, const char* str) {
    uint16_t strW = stringWidth(str);
    uint16_t strH = stringHeight();
    
    int16_t textX = x;
    int16_t textY = y;
    
    // Horizontal alignment
    switch (h_align_) {
      case TextAlign::CENTER:
        textX = x + (w - strW) / 2;
        break;
      case TextAlign::RIGHT:
        textX = x + w - strW;
        break;
      default:
        break;
    }
    
    // Vertical alignment
    switch (v_align_) {
      case TextVAlign::MIDDLE:
        textY = y + (h - strH) / 2;
        break;
      case TextVAlign::BOTTOM:
        textY = y + h - strH;
        break;
      default:
        break;
    }
    
    drawString(fb, textX, textY, str);
  }
  
  // --------------------------------------------------------
  // Drawing - Formatted
  // --------------------------------------------------------
  
  /**
   * Draw formatted integer
   * @param fb Target framebuffer
   * @param x X position
   * @param y Y position
   * @param value Integer value
   * @param minDigits Minimum digits (zero-padded)
   * @return Width of text drawn
   */
  uint16_t drawInt(FrameBuffer& fb, int16_t x, int16_t y, 
                   int32_t value, uint8_t minDigits = 1) {
    char buf[12];
    intToStr(value, buf, minDigits);
    return drawString(fb, x, y, buf);
  }
  
  /**
   * Draw formatted float
   * @param fb Target framebuffer
   * @param x X position
   * @param y Y position
   * @param value Float value
   * @param decimals Decimal places
   * @return Width of text drawn
   */
  uint16_t drawFloat(FrameBuffer& fb, int16_t x, int16_t y, 
                     float value, uint8_t decimals = 2) {
    char buf[16];
    floatToStr(value, buf, decimals);
    return drawString(fb, x, y, buf);
  }
  
private:
  uint8_t scale_;
  uint16_t fg_color_;
  uint16_t bg_color_;
  bool transparent_bg_;
  bool wrap_;
  TextAlign h_align_;
  TextVAlign v_align_;
  
  /** Draw a scaled pixel (scale x scale block) */
  void drawScaledPixel(FrameBuffer& fb, int16_t x, int16_t y, uint16_t color) {
    for (uint8_t dy = 0; dy < scale_; dy++) {
      for (uint8_t dx = 0; dx < scale_; dx++) {
        fb.setPixel(x + dx, y + dy, color);
      }
    }
  }
  
  /** Apply horizontal alignment */
  int16_t applyHAlign(int16_t x, const char* str, uint16_t displayWidth) {
    if (h_align_ == TextAlign::CENTER) {
      return x + (displayWidth - x - stringWidth(str)) / 2;
    } else if (h_align_ == TextAlign::RIGHT) {
      return displayWidth - stringWidth(str);
    }
    return x;
  }
  
  /** Apply vertical alignment */
  int16_t applyVAlign(int16_t y, uint16_t displayHeight) {
    if (v_align_ == TextVAlign::MIDDLE) {
      return y + (displayHeight - y - charHeight()) / 2;
    } else if (v_align_ == TextVAlign::BOTTOM) {
      return displayHeight - charHeight();
    }
    return y;
  }
  
  /** Convert integer to string */
  void intToStr(int32_t value, char* buf, uint8_t minDigits) {
    bool negative = value < 0;
    if (negative) value = -value;
    
    char tmp[12];
    int i = 0;
    
    do {
      tmp[i++] = '0' + (value % 10);
      value /= 10;
    } while (value > 0);
    
    // Pad with zeros
    while (i < minDigits) {
      tmp[i++] = '0';
    }
    
    // Build output string
    int j = 0;
    if (negative) buf[j++] = '-';
    while (i > 0) {
      buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
  }
  
  /** Convert float to string */
  void floatToStr(float value, char* buf, uint8_t decimals) {
    bool negative = value < 0;
    if (negative) value = -value;
    
    // Integer part
    int32_t intPart = (int32_t)value;
    float fracPart = value - intPart;
    
    // Convert integer part
    int j = 0;
    if (negative) buf[j++] = '-';
    
    char tmp[12];
    intToStr(intPart, tmp, 1);
    for (int i = 0; tmp[i]; i++) {
      buf[j++] = tmp[i];
    }
    
    // Decimal point and fraction
    if (decimals > 0) {
      buf[j++] = '.';
      
      for (uint8_t d = 0; d < decimals; d++) {
        fracPart *= 10;
        int digit = (int)fracPart;
        buf[j++] = '0' + digit;
        fracPart -= digit;
      }
    }
    
    buf[j] = '\0';
  }
};

// ============================================================
// Convenience Functions
// ============================================================

namespace text {

/** Quick text draw with default settings */
inline void draw(FrameBuffer& fb, int16_t x, int16_t y, 
                 const char* str, uint16_t color = 0xFFFF) {
  TextRenderer renderer;
  renderer.setFgColor(color);
  renderer.drawString(fb, x, y, str);
}

/** Quick centered text draw */
inline void drawCentered(FrameBuffer& fb, int16_t y, 
                         const char* str, uint16_t color = 0xFFFF) {
  TextRenderer renderer;
  renderer.setFgColor(color);
  renderer.drawCentered(fb, y, str);
}

/** Quick scaled text draw */
inline void drawScaled(FrameBuffer& fb, int16_t x, int16_t y, 
                       const char* str, uint8_t scale, uint16_t color = 0xFFFF) {
  TextRenderer renderer;
  renderer.setScale(scale);
  renderer.setFgColor(color);
  renderer.drawString(fb, x, y, str);
}

} // namespace text

} // namespace arcos::base

#endif // ARCOS_INCLUDE_BASEAPI_TEXT_RENDERER_HPP_
