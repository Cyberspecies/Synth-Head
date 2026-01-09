/*****************************************************************
 * File:      TextDriver.hpp
 * Category:  include/FrameworkAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side text rendering driver that provides high-level text
 *    operations for both HUB75 and OLED displays. Wraps GpuCommands
 *    text functions with additional features like text formatting,
 *    word wrap, and text boxes.
 * 
 * Usage:
 *    TextDriver text;
 *    text.init(&gpu);
 *    
 *    // Simple text
 *    text.print(Display::HUB75, 0, 0, "Hello");
 *    
 *    // Formatted text
 *    text.printf(Display::OLED, 0, 0, "Value: %d", 42);
 *    
 *    // Text box with word wrap
 *    text.textBox(Display::OLED, 0, 0, 64, 64, "Long text...");
 *****************************************************************/

#ifndef ARCOS_INCLUDE_FRAMEWORKAPI_TEXT_DRIVER_HPP_
#define ARCOS_INCLUDE_FRAMEWORKAPI_TEXT_DRIVER_HPP_

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include "FrameworkTypes.hpp"

// Forward declaration for GpuCommands
class GpuCommands;

namespace arcos::framework {

// ============================================================
// Text Alignment
// ============================================================

/**
 * @brief Horizontal text alignment
 */
enum class HAlign : uint8_t {
  LEFT = 0,
  CENTER,
  RIGHT,
};

/**
 * @brief Vertical text alignment
 */
enum class VAlign : uint8_t {
  TOP = 0,
  MIDDLE,
  BOTTOM,
};

// ============================================================
// Text Style
// ============================================================

/**
 * @brief Text rendering style configuration
 */
struct TextStyle {
  uint8_t scale = 1;          ///< Font scale (1 = 5x7, 2 = 10x14, etc.)
  Color color = {255, 255, 255, 255};  ///< Text color (RGB for HUB75, on/off for OLED)
  HAlign hAlign = HAlign::LEFT;
  VAlign vAlign = VAlign::TOP;
  int16_t lineSpacing = 1;    ///< Extra pixels between lines
  bool wordWrap = false;      ///< Enable word wrapping
  
  /** Create default white style */
  static TextStyle white(uint8_t s = 1){
    return TextStyle{s, {255, 255, 255, 255}, HAlign::LEFT, VAlign::TOP, 1, false};
  }
  
  /** Create colored style */
  static TextStyle colored(uint8_t r, uint8_t g, uint8_t b, uint8_t s = 1){
    return TextStyle{s, {r, g, b, 255}, HAlign::LEFT, VAlign::TOP, 1, false};
  }
  
  /** Create centered style */
  static TextStyle centered(uint8_t s = 1){
    return TextStyle{s, {255, 255, 255, 255}, HAlign::CENTER, VAlign::MIDDLE, 1, false};
  }
};

// ============================================================
// Text Box Configuration
// ============================================================

/**
 * @brief Text box configuration for bounded text rendering
 */
struct TextBox {
  int16_t x = 0;              ///< Box X position
  int16_t y = 0;              ///< Box Y position
  int16_t width = 128;        ///< Box width
  int16_t height = 32;        ///< Box height
  int16_t paddingX = 2;       ///< Horizontal padding
  int16_t paddingY = 2;       ///< Vertical padding
  bool drawBorder = false;    ///< Draw border around box
  bool fillBackground = false; ///< Fill background before text
  Color bgColor = {0, 0, 0, 255};  ///< Background color
};

// ============================================================
// Text Driver Class
// ============================================================

/**
 * @brief High-level text rendering driver for CPU
 * 
 * Provides text rendering capabilities for both HUB75 and OLED displays
 * through the GPU command interface.
 */
class TextDriver {
public:
  // Font metrics (5x7 font)
  static constexpr int CHAR_WIDTH = 5;
  static constexpr int CHAR_HEIGHT = 7;
  static constexpr int CHAR_SPACING = 1;
  static constexpr int LINE_HEIGHT = 8;  // 7 + 1 spacing
  
  // Display dimensions
  static constexpr int HUB75_WIDTH = 128;
  static constexpr int HUB75_HEIGHT = 32;
  static constexpr int OLED_WIDTH = 128;
  static constexpr int OLED_HEIGHT = 128;
  
private:
  GpuCommands* gpu_;
  bool initialized_;
  TextStyle defaultStyle_;
  char formatBuffer_[256];
  
public:
  TextDriver() : gpu_(nullptr), initialized_(false) {}
  
  // ============================================================
  // Initialization
  // ============================================================
  
  /**
   * @brief Initialize with GPU command interface
   * @param gpu Pointer to initialized GpuCommands instance
   * @return true if successful
   */
  bool init(GpuCommands* gpu){
    if(!gpu) return false;
    gpu_ = gpu;
    initialized_ = true;
    return true;
  }
  
  /** Check if initialized */
  bool isInitialized() const { return initialized_; }
  
  /** Set default text style */
  void setDefaultStyle(const TextStyle& style){
    defaultStyle_ = style;
  }
  
  /** Get default text style */
  const TextStyle& getDefaultStyle() const { return defaultStyle_; }
  
  // ============================================================
  // Text Measurement
  // ============================================================
  
  /**
   * @brief Calculate width of text string in pixels
   * @param text Text to measure
   * @param scale Font scale
   * @return Width in pixels
   */
  int measureWidth(const char* text, int scale = 1) const {
    if(!text || !*text) return 0;
    int len = strlen(text);
    return len * (CHAR_WIDTH + CHAR_SPACING) * scale - CHAR_SPACING * scale;
  }
  
  /**
   * @brief Calculate height of text string in pixels
   * @param text Text to measure (counts newlines)
   * @param scale Font scale
   * @param lineSpacing Extra spacing between lines
   * @return Height in pixels
   */
  int measureHeight(const char* text, int scale = 1, int lineSpacing = 1) const {
    if(!text || !*text) return 0;
    
    int lines = 1;
    while(*text){
      if(*text == '\n') lines++;
      text++;
    }
    
    return lines * (CHAR_HEIGHT * scale + lineSpacing) - lineSpacing;
  }
  
  /**
   * @brief Count characters that fit in given width
   * @param text Text to measure
   * @param maxWidth Maximum width in pixels
   * @param scale Font scale
   * @return Number of characters that fit
   */
  int charsThatFit(const char* text, int maxWidth, int scale = 1) const {
    if(!text) return 0;
    
    int charWidth = (CHAR_WIDTH + CHAR_SPACING) * scale;
    int count = 0;
    int width = 0;
    
    while(*text && width + CHAR_WIDTH * scale <= maxWidth){
      width += charWidth;
      count++;
      text++;
    }
    
    return count;
  }
  
  // ============================================================
  // Basic Text Rendering
  // ============================================================
  
  /**
   * @brief Print text to display
   * @param target Target display (HUB75 or OLED)
   * @param x X position
   * @param y Y position
   * @param text Text to print
   * @param style Text style (optional, uses default if not provided)
   */
  void print(Display target, int16_t x, int16_t y, const char* text,
             const TextStyle& style = TextStyle{});
  
  /**
   * @brief Print formatted text (printf-style)
   * @param target Target display
   * @param x X position
   * @param y Y position
   * @param fmt Format string
   * @param ... Format arguments
   */
  void printf(Display target, int16_t x, int16_t y, const char* fmt, ...);
  
  /**
   * @brief Print formatted text with style
   */
  void printfStyled(Display target, int16_t x, int16_t y, 
                    const TextStyle& style, const char* fmt, ...);
  
  // ============================================================
  // Aligned Text
  // ============================================================
  
  /**
   * @brief Print text with horizontal centering
   * @param target Target display
   * @param y Y position
   * @param text Text to print
   * @param style Text style
   */
  void printCentered(Display target, int16_t y, const char* text,
                     const TextStyle& style = TextStyle{});
  
  /**
   * @brief Print text right-aligned
   * @param target Target display
   * @param y Y position
   * @param text Text to print
   * @param style Text style
   */
  void printRight(Display target, int16_t y, const char* text,
                  const TextStyle& style = TextStyle{});
  
  /**
   * @brief Print text in a bounded box with alignment
   * @param target Target display
   * @param box Text box bounds and options
   * @param text Text to print
   * @param style Text style
   */
  void printInBox(Display target, const TextBox& box, const char* text,
                  const TextStyle& style = TextStyle{});
  
  // ============================================================
  // Multi-line Text
  // ============================================================
  
  /**
   * @brief Print multi-line text with word wrapping
   * @param target Target display
   * @param x X position
   * @param y Y position
   * @param maxWidth Maximum width before wrapping
   * @param text Text to print (can contain \n)
   * @param style Text style
   * @return Number of lines rendered
   */
  int printWrapped(Display target, int16_t x, int16_t y, int16_t maxWidth,
                   const char* text, const TextStyle& style = TextStyle{});
  
  // ============================================================
  // Special Text Elements
  // ============================================================
  
  /**
   * @brief Print a title with underline
   * @param target Target display
   * @param y Y position
   * @param text Title text
   * @param style Text style
   */
  void printTitle(Display target, int16_t y, const char* text,
                  const TextStyle& style = TextStyle{});
  
  /**
   * @brief Print a label: value pair
   * @param target Target display
   * @param x X position
   * @param y Y position
   * @param label Label text
   * @param value Value text
   * @param style Text style
   */
  void printLabelValue(Display target, int16_t x, int16_t y,
                       const char* label, const char* value,
                       const TextStyle& style = TextStyle{});
  
  /**
   * @brief Print integer value with label
   */
  void printLabelInt(Display target, int16_t x, int16_t y,
                     const char* label, int value,
                     const TextStyle& style = TextStyle{});
  
  /**
   * @brief Print float value with label
   */
  void printLabelFloat(Display target, int16_t x, int16_t y,
                       const char* label, float value, int decimals = 2,
                       const TextStyle& style = TextStyle{});
  
  // ============================================================
  // Status Display Helpers
  // ============================================================
  
  /**
   * @brief Print a status line (label + OK/FAIL indicator)
   * @param target Target display
   * @param y Y position
   * @param label Status label
   * @param ok True for OK, false for FAIL
   */
  void printStatus(Display target, int16_t y, const char* label, bool ok);
  
  /**
   * @brief Print a progress indicator
   * @param target Target display
   * @param y Y position
   * @param label Label text
   * @param current Current value
   * @param total Total value
   */
  void printProgress(Display target, int16_t y, const char* label,
                     int current, int total);
  
private:
  /** Get display width */
  int getDisplayWidth(Display target) const {
    return (target == Display::OLED) ? OLED_WIDTH : HUB75_WIDTH;
  }
  
  /** Get display height */
  int getDisplayHeight(Display target) const {
    return (target == Display::OLED) ? OLED_HEIGHT : HUB75_HEIGHT;
  }
  
  /** Calculate X position based on alignment */
  int16_t calcAlignedX(Display target, int16_t x, int textWidth, HAlign align) const {
    switch(align){
      case HAlign::CENTER:
        return (getDisplayWidth(target) - textWidth) / 2;
      case HAlign::RIGHT:
        return getDisplayWidth(target) - textWidth - x;
      default:
        return x;
    }
  }
};

// ============================================================
// Implementation
// ============================================================

inline void TextDriver::print(Display target, int16_t x, int16_t y, 
                              const char* text, const TextStyle& style){
  if(!initialized_ || !gpu_ || !text) return;
  
  TextStyle s = (style.scale == 0) ? defaultStyle_ : style;
  
  // Calculate aligned position
  if(s.hAlign != HAlign::LEFT){
    int w = measureWidth(text, s.scale);
    x = calcAlignedX(target, x, w, s.hAlign);
  }
  
  if(target == Display::OLED){
    gpu_->oledText(x, y, text, s.scale, s.color.r > 127);
  }else{
    gpu_->hub75Text(x, y, text, s.color.r, s.color.g, s.color.b, s.scale);
  }
}

inline void TextDriver::printf(Display target, int16_t x, int16_t y, 
                               const char* fmt, ...){
  if(!initialized_ || !gpu_ || !fmt) return;
  
  va_list args;
  va_start(args, fmt);
  vsnprintf(formatBuffer_, sizeof(formatBuffer_), fmt, args);
  va_end(args);
  
  print(target, x, y, formatBuffer_, defaultStyle_);
}

inline void TextDriver::printfStyled(Display target, int16_t x, int16_t y,
                                     const TextStyle& style, const char* fmt, ...){
  if(!initialized_ || !gpu_ || !fmt) return;
  
  va_list args;
  va_start(args, fmt);
  vsnprintf(formatBuffer_, sizeof(formatBuffer_), fmt, args);
  va_end(args);
  
  print(target, x, y, formatBuffer_, style);
}

inline void TextDriver::printCentered(Display target, int16_t y, const char* text,
                                      const TextStyle& style){
  TextStyle s = style;
  s.hAlign = HAlign::CENTER;
  print(target, 0, y, text, s);
}

inline void TextDriver::printRight(Display target, int16_t y, const char* text,
                                   const TextStyle& style){
  TextStyle s = style;
  s.hAlign = HAlign::RIGHT;
  print(target, 0, y, text, s);
}

inline void TextDriver::printInBox(Display target, const TextBox& box, 
                                   const char* text, const TextStyle& style){
  if(!initialized_ || !gpu_ || !text) return;
  
  // Draw background if requested
  if(box.fillBackground){
    if(target == Display::OLED){
      gpu_->oledFill(box.x, box.y, box.width, box.height, false);
    }else{
      gpu_->hub75Fill(box.x, box.y, box.width, box.height,
                      box.bgColor.r, box.bgColor.g, box.bgColor.b);
    }
  }
  
  // Draw border if requested
  if(box.drawBorder){
    if(target == Display::OLED){
      gpu_->oledRect(box.x, box.y, box.width, box.height, true);
    }else{
      gpu_->hub75Rect(box.x, box.y, box.width, box.height,
                      style.color.r, style.color.g, style.color.b);
    }
  }
  
  // Calculate text position within box
  int textW = measureWidth(text, style.scale);
  int textH = measureHeight(text, style.scale, style.lineSpacing);
  
  int16_t textX = box.x + box.paddingX;
  int16_t textY = box.y + box.paddingY;
  
  // Horizontal alignment
  int innerW = box.width - 2 * box.paddingX;
  switch(style.hAlign){
    case HAlign::CENTER:
      textX = box.x + (box.width - textW) / 2;
      break;
    case HAlign::RIGHT:
      textX = box.x + box.width - box.paddingX - textW;
      break;
    default:
      break;
  }
  
  // Vertical alignment
  int innerH = box.height - 2 * box.paddingY;
  switch(style.vAlign){
    case VAlign::MIDDLE:
      textY = box.y + (box.height - textH) / 2;
      break;
    case VAlign::BOTTOM:
      textY = box.y + box.height - box.paddingY - textH;
      break;
    default:
      break;
  }
  
  // If word wrap enabled, use wrapped printing
  if(style.wordWrap){
    printWrapped(target, textX, textY, innerW, text, style);
  }else{
    print(target, textX, textY, text, style);
  }
}

inline int TextDriver::printWrapped(Display target, int16_t x, int16_t y,
                                    int16_t maxWidth, const char* text,
                                    const TextStyle& style){
  if(!initialized_ || !gpu_ || !text) return 0;
  
  int lineCount = 0;
  int cursorY = y;
  int charWidth = (CHAR_WIDTH + CHAR_SPACING) * style.scale;
  int lineHeight = CHAR_HEIGHT * style.scale + style.lineSpacing;
  
  const char* lineStart = text;
  const char* wordStart = text;
  const char* ptr = text;
  int lineWidth = 0;
  
  while(*ptr){
    if(*ptr == '\n'){
      // Render current line
      int len = ptr - lineStart;
      if(len > 0){
        char buf[128];
        int copyLen = (len < 127) ? len : 127;
        strncpy(buf, lineStart, copyLen);
        buf[copyLen] = '\0';
        print(target, x, cursorY, buf, style);
      }
      
      lineCount++;
      cursorY += lineHeight;
      lineStart = ptr + 1;
      wordStart = lineStart;
      lineWidth = 0;
      ptr++;
      continue;
    }
    
    if(*ptr == ' '){
      wordStart = ptr + 1;
    }
    
    lineWidth += charWidth;
    
    // Check if we need to wrap
    if(lineWidth > maxWidth && lineStart != ptr){
      // Find wrap point
      const char* wrapAt = (wordStart > lineStart) ? wordStart - 1 : ptr;
      
      // Render line up to wrap point
      int len = wrapAt - lineStart;
      if(len > 0){
        char buf[128];
        int copyLen = (len < 127) ? len : 127;
        strncpy(buf, lineStart, copyLen);
        buf[copyLen] = '\0';
        print(target, x, cursorY, buf, style);
      }
      
      lineCount++;
      cursorY += lineHeight;
      lineStart = (wordStart > lineStart) ? wordStart : ptr;
      wordStart = lineStart;
      lineWidth = 0;
      continue;
    }
    
    ptr++;
  }
  
  // Render remaining text
  if(lineStart < ptr){
    print(target, x, cursorY, lineStart, style);
    lineCount++;
  }
  
  return lineCount;
}

inline void TextDriver::printTitle(Display target, int16_t y, const char* text,
                                   const TextStyle& style){
  if(!initialized_ || !gpu_ || !text) return;
  
  // Print centered text
  printCentered(target, y, text, style);
  
  // Draw underline
  int textW = measureWidth(text, style.scale);
  int displayW = getDisplayWidth(target);
  int lineX = (displayW - textW) / 2;
  int lineY = y + CHAR_HEIGHT * style.scale + 2;
  
  if(target == Display::OLED){
    gpu_->oledHLine(lineX, lineY, textW, true);
  }else{
    gpu_->hub75Line(lineX, lineY, lineX + textW - 1, lineY,
                    style.color.r, style.color.g, style.color.b);
  }
}

inline void TextDriver::printLabelValue(Display target, int16_t x, int16_t y,
                                        const char* label, const char* value,
                                        const TextStyle& style){
  if(!initialized_ || !gpu_) return;
  
  snprintf(formatBuffer_, sizeof(formatBuffer_), "%s: %s", 
           label ? label : "", value ? value : "");
  print(target, x, y, formatBuffer_, style);
}

inline void TextDriver::printLabelInt(Display target, int16_t x, int16_t y,
                                      const char* label, int value,
                                      const TextStyle& style){
  if(!initialized_ || !gpu_) return;
  
  snprintf(formatBuffer_, sizeof(formatBuffer_), "%s: %d", 
           label ? label : "", value);
  print(target, x, y, formatBuffer_, style);
}

inline void TextDriver::printLabelFloat(Display target, int16_t x, int16_t y,
                                        const char* label, float value, int decimals,
                                        const TextStyle& style){
  if(!initialized_ || !gpu_) return;
  
  char fmtStr[16];
  snprintf(fmtStr, sizeof(fmtStr), "%%s: %%.%df", decimals);
  snprintf(formatBuffer_, sizeof(formatBuffer_), fmtStr, 
           label ? label : "", value);
  print(target, x, y, formatBuffer_, style);
}

inline void TextDriver::printStatus(Display target, int16_t y, 
                                    const char* label, bool ok){
  if(!initialized_ || !gpu_) return;
  
  int displayW = getDisplayWidth(target);
  
  // Print label on left
  TextStyle labelStyle = defaultStyle_;
  print(target, 2, y, label, labelStyle);
  
  // Print status on right
  const char* statusText = ok ? "OK" : "FAIL";
  int statusW = measureWidth(statusText, 1);
  
  if(target == Display::OLED){
    gpu_->oledText(displayW - statusW - 2, y, statusText, 1, true);
  }else{
    // Green for OK, Red for FAIL
    uint8_t r = ok ? 0 : 255;
    uint8_t g = ok ? 255 : 0;
    gpu_->hub75Text(displayW - statusW - 2, y, statusText, r, g, 0, 1);
  }
}

inline void TextDriver::printProgress(Display target, int16_t y, 
                                      const char* label, int current, int total){
  if(!initialized_ || !gpu_) return;
  
  snprintf(formatBuffer_, sizeof(formatBuffer_), "%s: %d/%d", 
           label ? label : "", current, total);
  print(target, 2, y, formatBuffer_, defaultStyle_);
}

} // namespace arcos::framework

#endif // ARCOS_INCLUDE_FRAMEWORKAPI_TEXT_DRIVER_HPP_
