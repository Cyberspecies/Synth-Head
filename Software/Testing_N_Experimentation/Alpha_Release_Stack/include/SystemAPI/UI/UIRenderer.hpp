/*****************************************************************
 * @file UIRenderer.hpp
 * @brief UI Framework Renderer - Renders UI to display buffer
 * 
 * The renderer is the bridge between UI elements and the OLED display.
 * It provides drawing primitives and handles all rendering.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UICore.hpp"
#include "UIStyle.hpp"
#include "UIElement.hpp"
#include "UIContainer.hpp"
#include "UIText.hpp"
#include "UIIcon.hpp"
#include "UIButton.hpp"
#include "UIProgressBar.hpp"
#include "UISlider.hpp"
#include "UICheckbox.hpp"
#include "UIDropdown.hpp"
#include "UIMenu.hpp"
#include "UIScrollView.hpp"
#include "UIGrid.hpp"
#include "UINotification.hpp"
#include "UIDialog.hpp"

#include <cstring>
#include <cmath>

namespace SystemAPI {
namespace UI {

/**
 * @brief Display buffer type
 */
enum class BufferFormat : uint8_t {
  MONO_1BPP,     // 1-bit monochrome (1 byte = 8 pixels)
  GRAY_8BPP,     // 8-bit grayscale
  RGB565,        // 16-bit RGB
  RGB888         // 24-bit RGB
};

/**
 * @brief Renderer configuration
 */
struct RendererConfig {
  uint16_t width = 128;
  uint16_t height = 128;
  BufferFormat format = BufferFormat::MONO_1BPP;
  bool doubleBuffer = false;
};

/**
 * @brief UI Renderer - draws UI elements to display buffer
 * 
 * @example
 * ```cpp
 * // Initialize renderer
 * UIRenderer renderer;
 * renderer.init(128, 128, BufferFormat::MONO_1BPP);
 * 
 * // In render loop
 * renderer.beginFrame();
 * renderer.clear();
 * 
 * // Draw UI
 * myContainer->render(renderer);
 * 
 * renderer.endFrame();
 * 
 * // Get buffer for display
 * const uint8_t* buffer = renderer.getBuffer();
 * oled.drawBitmap(0, 0, buffer, 128, 128);
 * ```
 */
class UIRenderer {
public:
  UIRenderer() = default;
  ~UIRenderer() {
    if (buffer_) delete[] buffer_;
    if (backBuffer_) delete[] backBuffer_;
  }
  
  // ---- Initialization ----
  
  bool init(uint16_t width, uint16_t height, BufferFormat format = BufferFormat::MONO_1BPP) {
    config_.width = width;
    config_.height = height;
    config_.format = format;
    
    bufferSize_ = calculateBufferSize();
    buffer_ = new uint8_t[bufferSize_];
    
    if (config_.doubleBuffer) {
      backBuffer_ = new uint8_t[bufferSize_];
    }
    
    clear();
    return true;
  }
  
  bool init(const RendererConfig& config) {
    config_ = config;
    return init(config.width, config.height, config.format);
  }
  
  // ---- Frame Management ----
  
  void beginFrame() {
    if (config_.doubleBuffer && backBuffer_) {
      std::swap(buffer_, backBuffer_);
    }
    frameCount_++;
  }
  
  void endFrame() {
    // Calculate dirty regions, etc.
  }
  
  // ---- Buffer Access ----
  
  uint8_t* getBuffer() { return buffer_; }
  const uint8_t* getBuffer() const { return buffer_; }
  size_t getBufferSize() const { return bufferSize_; }
  
  uint16_t getWidth() const { return config_.width; }
  uint16_t getHeight() const { return config_.height; }
  BufferFormat getFormat() const { return config_.format; }
  
  // ---- Clipping ----
  
  void setClipRect(const Rect& rect) {
    clipRect_ = rect.intersection(Rect(0, 0, config_.width, config_.height));
    clipEnabled_ = true;
  }
  
  void clearClipRect() {
    clipRect_ = Rect(0, 0, config_.width, config_.height);
    clipEnabled_ = false;
  }
  
  const Rect& getClipRect() const { return clipRect_; }
  
  void pushClipRect(const Rect& rect) {
    clipStack_[clipStackPos_++] = clipRect_;
    setClipRect(clipRect_.intersection(rect));
  }
  
  void popClipRect() {
    if (clipStackPos_ > 0) {
      clipRect_ = clipStack_[--clipStackPos_];
    }
  }
  
  // ---- Transform ----
  
  void setTranslation(int16_t x, int16_t y) {
    translateX_ = x;
    translateY_ = y;
  }
  
  void translate(int16_t dx, int16_t dy) {
    translateX_ += dx;
    translateY_ += dy;
  }
  
  void pushTranslation(int16_t x, int16_t y) {
    translateStack_[translateStackPos_] = {translateX_, translateY_};
    translateStackPos_++;
    translate(x, y);
  }
  
  void popTranslation() {
    if (translateStackPos_ > 0) {
      translateStackPos_--;
      translateX_ = translateStack_[translateStackPos_].x;
      translateY_ = translateStack_[translateStackPos_].y;
    }
  }
  
  // ---- Clear ----
  
  void clear(const Color& color = Colors::Black) {
    if (config_.format == BufferFormat::MONO_1BPP) {
      memset(buffer_, color.isOn() ? 0xFF : 0x00, bufferSize_);
    } else {
      // Fill with color
      for (uint16_t y = 0; y < config_.height; y++) {
        for (uint16_t x = 0; x < config_.width; x++) {
          setPixel(x, y, color);
        }
      }
    }
  }
  
  // ---- Pixel Operations ----
  
  void setPixel(int16_t x, int16_t y, const Color& color) {
    x += translateX_;
    y += translateY_;
    
    if (x < 0 || x >= config_.width || y < 0 || y >= config_.height) return;
    if (clipEnabled_ && !clipRect_.contains(x, y)) return;
    
    switch (config_.format) {
      case BufferFormat::MONO_1BPP: {
        size_t byteIdx = (y * config_.width + x) / 8;
        uint8_t bitMask = 0x80 >> (x % 8);
        if (color.isOn()) {
          buffer_[byteIdx] |= bitMask;
        } else {
          buffer_[byteIdx] &= ~bitMask;
        }
        break;
      }
      case BufferFormat::GRAY_8BPP: {
        buffer_[y * config_.width + x] = color.gray();
        break;
      }
      case BufferFormat::RGB565: {
        size_t idx = (y * config_.width + x) * 2;
        uint16_t c = color.toRGB565();
        buffer_[idx] = c >> 8;
        buffer_[idx + 1] = c & 0xFF;
        break;
      }
      case BufferFormat::RGB888: {
        size_t idx = (y * config_.width + x) * 3;
        buffer_[idx] = color.r;
        buffer_[idx + 1] = color.g;
        buffer_[idx + 2] = color.b;
        break;
      }
    }
  }
  
  Color getPixel(int16_t x, int16_t y) const {
    x += translateX_;
    y += translateY_;
    
    if (x < 0 || x >= config_.width || y < 0 || y >= config_.height) {
      return Colors::Black;
    }
    
    switch (config_.format) {
      case BufferFormat::MONO_1BPP: {
        size_t byteIdx = (y * config_.width + x) / 8;
        uint8_t bitMask = 0x80 >> (x % 8);
        return (buffer_[byteIdx] & bitMask) ? Colors::White : Colors::Black;
      }
      case BufferFormat::GRAY_8BPP:
        return Color(buffer_[y * config_.width + x]);
      case BufferFormat::RGB565: {
        size_t idx = (y * config_.width + x) * 2;
        uint16_t c = (buffer_[idx] << 8) | buffer_[idx + 1];
        return Color::fromRGB565(c);
      }
      case BufferFormat::RGB888: {
        size_t idx = (y * config_.width + x) * 3;
        return Color(buffer_[idx], buffer_[idx + 1], buffer_[idx + 2]);
      }
      default:
        return Colors::Black;
    }
  }
  
  // ---- Drawing Primitives ----
  
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const Color& color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = -abs(y1 - y0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    
    while (true) {
      setPixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) break;
      int16_t e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  
  void drawHLine(int16_t x, int16_t y, int16_t w, const Color& color) {
    for (int16_t i = 0; i < w; i++) {
      setPixel(x + i, y, color);
    }
  }
  
  void drawVLine(int16_t x, int16_t y, int16_t h, const Color& color) {
    for (int16_t i = 0; i < h; i++) {
      setPixel(x, y + i, color);
    }
  }
  
  void drawRect(const Rect& rect, const Color& color) {
    drawHLine(rect.x, rect.y, rect.width, color);
    drawHLine(rect.x, rect.y + rect.height - 1, rect.width, color);
    drawVLine(rect.x, rect.y, rect.height, color);
    drawVLine(rect.x + rect.width - 1, rect.y, rect.height, color);
  }
  
  void drawRect(int16_t x, int16_t y, uint16_t w, uint16_t h, const Color& color) {
    drawRect(Rect(x, y, w, h), color);
  }
  
  void fillRect(const Rect& rect, const Color& color) {
    for (int16_t dy = 0; dy < rect.height; dy++) {
      drawHLine(rect.x, rect.y + dy, rect.width, color);
    }
  }
  
  void fillRect(int16_t x, int16_t y, uint16_t w, uint16_t h, const Color& color) {
    fillRect(Rect(x, y, w, h), color);
  }
  
  void drawRoundRect(const Rect& rect, uint8_t radius, const Color& color) {
    int16_t x = rect.x, y = rect.y;
    int16_t w = rect.width, h = rect.height;
    
    // Draw sides
    drawHLine(x + radius, y, w - 2 * radius, color);
    drawHLine(x + radius, y + h - 1, w - 2 * radius, color);
    drawVLine(x, y + radius, h - 2 * radius, color);
    drawVLine(x + w - 1, y + radius, h - 2 * radius, color);
    
    // Draw corners
    drawCircleQuadrant(x + radius, y + radius, radius, 1, color);
    drawCircleQuadrant(x + w - radius - 1, y + radius, radius, 2, color);
    drawCircleQuadrant(x + w - radius - 1, y + h - radius - 1, radius, 4, color);
    drawCircleQuadrant(x + radius, y + h - radius - 1, radius, 8, color);
  }
  
  void fillRoundRect(const Rect& rect, uint8_t radius, const Color& color) {
    int16_t x = rect.x, y = rect.y;
    int16_t w = rect.width, h = rect.height;
    
    // Center rectangle
    fillRect(x + radius, y, w - 2 * radius, h, color);
    
    // Side rectangles
    fillRect(x, y + radius, radius, h - 2 * radius, color);
    fillRect(x + w - radius, y + radius, radius, h - 2 * radius, color);
    
    // Rounded corners
    fillCircleQuadrant(x + radius, y + radius, radius, 1, color);
    fillCircleQuadrant(x + w - radius - 1, y + radius, radius, 2, color);
    fillCircleQuadrant(x + w - radius - 1, y + h - radius - 1, radius, 4, color);
    fillCircleQuadrant(x + radius, y + h - radius - 1, radius, 8, color);
  }
  
  void drawCircle(int16_t cx, int16_t cy, int16_t r, const Color& color) {
    int16_t x = -r, y = 0, err = 2 - 2 * r;
    do {
      setPixel(cx - x, cy + y, color);
      setPixel(cx - y, cy - x, color);
      setPixel(cx + x, cy - y, color);
      setPixel(cx + y, cy + x, color);
      int16_t r2 = err;
      if (r2 <= y) err += ++y * 2 + 1;
      if (r2 > x || err > y) err += ++x * 2 + 1;
    } while (x < 0);
  }
  
  void fillCircle(int16_t cx, int16_t cy, int16_t r, const Color& color) {
    drawVLine(cx, cy - r, 2 * r + 1, color);
    int16_t x = -r, y = 0, err = 2 - 2 * r;
    do {
      int16_t dx = -x;
      drawHLine(cx - dx, cy + y, 2 * dx + 1, color);
      drawHLine(cx - dx, cy - y, 2 * dx + 1, color);
      int16_t r2 = err;
      if (r2 <= y) err += ++y * 2 + 1;
      if (r2 > x || err > y) err += ++x * 2 + 1;
    } while (x < 0);
  }
  
  // ---- Text Rendering ----
  
  void drawChar(int16_t x, int16_t y, char c, const Color& color, FontSize size) {
    // Simple bitmap font rendering (5x7 built-in)
    FontInfo font = getFontInfo(size);
    
    // For now, use a basic 5x7 font
    if (c < 32 || c > 126) return;
    
    // Simple built-in 5x7 font data would go here
    // This is a placeholder - real implementation would have font bitmaps
    for (int row = 0; row < font.charHeight; row++) {
      for (int col = 0; col < font.charWidth; col++) {
        // Placeholder: draw a simple box for each character
        // Real implementation would read from font bitmap data
        setPixel(x + col, y + row, color);
      }
    }
  }
  
  void drawText(int16_t x, int16_t y, const char* text, const Color& color, 
                FontSize size = FontSize::SMALL, HAlign align = HAlign::LEFT) {
    if (!text) return;
    
    FontInfo font = getFontInfo(size);
    uint16_t textW = textWidth(text, size);
    
    // Adjust X for alignment
    switch (align) {
      case HAlign::CENTER: x -= textW / 2; break;
      case HAlign::RIGHT: x -= textW; break;
      default: break;
    }
    
    while (*text) {
      drawChar(x, y, *text, color, size);
      x += font.charWidth + font.spacing;
      text++;
    }
  }
  
  void drawTextInRect(const Rect& rect, const char* text, const Color& color,
                      FontSize size, HAlign hAlign, VAlign vAlign) {
    if (!text) return;
    
    FontInfo font = getFontInfo(size);
    uint16_t textW = textWidth(text, size);
    
    int16_t x = rect.x;
    int16_t y = rect.y;
    
    // Horizontal alignment
    switch (hAlign) {
      case HAlign::CENTER: x = rect.x + (rect.width - textW) / 2; break;
      case HAlign::RIGHT: x = rect.x + rect.width - textW; break;
      default: break;
    }
    
    // Vertical alignment
    switch (vAlign) {
      case VAlign::CENTER: y = rect.y + (rect.height - font.charHeight) / 2; break;
      case VAlign::BOTTOM: y = rect.y + rect.height - font.charHeight; break;
      default: break;
    }
    
    drawText(x, y, text, color, size);
  }
  
  // ---- Icon Rendering ----
  
  void drawIcon(int16_t x, int16_t y, IconType icon, const Color& color, uint8_t scale = 1) {
    const uint8_t* bitmap = getIconBitmap(icon);
    
    for (int row = 0; row < 8; row++) {
      uint8_t rowData = bitmap[row];
      for (int col = 0; col < 8; col++) {
        if (rowData & (0x80 >> col)) {
          if (scale == 1) {
            setPixel(x + col, y + row, color);
          } else {
            fillRect(x + col * scale, y + row * scale, scale, scale, color);
          }
        }
      }
    }
  }
  
  // ---- Bitmap Rendering ----
  
  void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, 
                  uint16_t w, uint16_t h, const Color& color) {
    for (int16_t row = 0; row < h; row++) {
      for (int16_t col = 0; col < w; col++) {
        int byteIdx = (row * w + col) / 8;
        int bitIdx = 7 - (col % 8);
        if (bitmap[byteIdx] & (1 << bitIdx)) {
          setPixel(x + col, y + row, color);
        }
      }
    }
  }
  
  // ---- Style-Based Drawing ----
  
  void drawStyledRect(const Rect& rect, const UIStyle& style, StyleState state) {
    Color bgColor = style.getBackgroundColor(state);
    Color borderColor = style.getBorderColor(state);
    uint8_t borderWidth = style.getBorderWidth();
    uint8_t borderRadius = style.getBorderRadius();
    
    Rect borderRect = style.borderRect(rect);
    
    // Background
    if (bgColor.a > 0) {
      if (borderRadius > 0) {
        fillRoundRect(borderRect, borderRadius, bgColor);
      } else {
        fillRect(borderRect, bgColor);
      }
    }
    
    // Border
    if (borderWidth > 0) {
      if (borderRadius > 0) {
        drawRoundRect(borderRect, borderRadius, borderColor);
      } else {
        drawRect(borderRect, borderColor);
      }
    }
  }
  
  // ---- Statistics ----
  
  uint32_t getFrameCount() const { return frameCount_; }
  
private:
  RendererConfig config_;
  uint8_t* buffer_ = nullptr;
  uint8_t* backBuffer_ = nullptr;
  size_t bufferSize_ = 0;
  uint32_t frameCount_ = 0;
  
  // Clipping
  Rect clipRect_ = {0, 0, 128, 128};
  bool clipEnabled_ = false;
  Rect clipStack_[8];
  int clipStackPos_ = 0;
  
  // Transform
  int16_t translateX_ = 0;
  int16_t translateY_ = 0;
  Point translateStack_[8];
  int translateStackPos_ = 0;
  
  size_t calculateBufferSize() const {
    switch (config_.format) {
      case BufferFormat::MONO_1BPP:
        return (config_.width * config_.height + 7) / 8;
      case BufferFormat::GRAY_8BPP:
        return config_.width * config_.height;
      case BufferFormat::RGB565:
        return config_.width * config_.height * 2;
      case BufferFormat::RGB888:
        return config_.width * config_.height * 3;
      default:
        return 0;
    }
  }
  
  void drawCircleQuadrant(int16_t cx, int16_t cy, int16_t r, uint8_t quadrant, const Color& color) {
    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;
    while (y >= x) {
      if (quadrant & 1) { setPixel(cx - x, cy - y, color); setPixel(cx - y, cy - x, color); }
      if (quadrant & 2) { setPixel(cx + x, cy - y, color); setPixel(cx + y, cy - x, color); }
      if (quadrant & 4) { setPixel(cx + x, cy + y, color); setPixel(cx + y, cy + x, color); }
      if (quadrant & 8) { setPixel(cx - x, cy + y, color); setPixel(cx - y, cy + x, color); }
      x++;
      if (d > 0) { y--; d += 4 * (x - y) + 10; }
      else { d += 4 * x + 6; }
    }
  }
  
  void fillCircleQuadrant(int16_t cx, int16_t cy, int16_t r, uint8_t quadrant, const Color& color) {
    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;
    while (y >= x) {
      if (quadrant & 1) { drawVLine(cx - x, cy - y, y, color); drawVLine(cx - y, cy - x, x, color); }
      if (quadrant & 2) { drawVLine(cx + x, cy - y, y, color); drawVLine(cx + y, cy - x, x, color); }
      if (quadrant & 4) { drawVLine(cx + x, cy, y + 1, color); drawVLine(cx + y, cy, x + 1, color); }
      if (quadrant & 8) { drawVLine(cx - x, cy, y + 1, color); drawVLine(cx - y, cy, x + 1, color); }
      x++;
      if (d > 0) { y--; d += 4 * (x - y) + 10; }
      else { d += 4 * x + 6; }
    }
  }
};

// ============================================================
// Element Render Implementations
// ============================================================

// UIContainer::render
inline void UIContainer::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  // Draw background
  renderer.drawStyledRect(bounds_, style_, getStyleState());
  
  // Set up clipping and translation for content
  Rect content = style_.contentRect(bounds_);
  if (overflow_ == Overflow::HIDDEN || overflow_ == Overflow::SCROLL) {
    renderer.pushClipRect(content);
  }
  renderer.pushTranslation(content.x - scrollX_, content.y - scrollY_);
  
  // Render children
  for (auto* child : children_) {
    child->render(renderer);
  }
  
  renderer.popTranslation();
  if (overflow_ == Overflow::HIDDEN || overflow_ == Overflow::SCROLL) {
    renderer.popClipRect();
  }
}

// UIText::render
inline void UIText::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  renderer.drawStyledRect(bounds_, style_, getStyleState());
  
  Rect content = style_.contentRect(bounds_);
  renderer.drawTextInRect(
    content, text_,
    style_.getTextColor(getStyleState()),
    style_.getFontSize(),
    style_.getTextAlign(),
    style_.getTextVAlign()
  );
}

// UIIcon::render
inline void UIIcon::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  renderer.drawIcon(
    bounds_.x, bounds_.y,
    icon_,
    style_.getTextColor(getStyleState()),
    scale_
  );
}

// UIButton::render
inline void UIButton::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  StyleState state = getStyleState();
  renderer.drawStyledRect(bounds_, style_, state);
  
  Rect content = style_.contentRect(bounds_);
  Color textColor = style_.getTextColor(state);
  
  int16_t x = content.x;
  int16_t iconW = (icon_ != IconType::NONE) ? 10 : 0;
  int16_t gap = (icon_ != IconType::NONE && text_[0]) ? 4 : 0;
  int16_t textW = text_[0] ? textWidth(text_, style_.getFontSize()) : 0;
  int16_t totalW = iconW + gap + textW;
  
  // Center content
  x = content.x + (content.width - totalW) / 2;
  
  if (icon_ != IconType::NONE) {
    renderer.drawIcon(x, content.y + (content.height - 8) / 2, icon_, textColor);
    x += iconW + gap;
  }
  
  if (text_[0]) {
    renderer.drawText(x, content.y + (content.height - textHeight(style_.getFontSize())) / 2,
                      text_, textColor, style_.getFontSize());
  }
}

// UIProgressBar::render
inline void UIProgressBar::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  renderer.drawStyledRect(bounds_, style_, getStyleState());
  
  Rect content = style_.contentRect(bounds_);
  
  if (indeterminate_) {
    // Animated bar
    int16_t barW = content.width / 3;
    int16_t x = (int16_t)(animPhase_ * (content.width + barW)) - barW;
    renderer.fillRect(content.x + x, content.y, barW, content.height, barColor_);
  } else {
    // Progress bar
    int16_t fillW = (int16_t)(value_ * content.width);
    renderer.fillRect(content.x, content.y, fillW, content.height, barColor_);
  }
}

// UISlider::render
inline void UISlider::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  Rect content = style_.contentRect(bounds_);
  
  // Track
  int16_t trackY = content.y + content.height / 2 - 2;
  renderer.fillRect(content.x, trackY, content.width, 4, trackColor_);
  
  // Fill
  float ratio = getNormalizedValue();
  int16_t fillW = (int16_t)(ratio * content.width);
  renderer.fillRect(content.x, trackY, fillW, 4, fillColor_);
  
  // Knob
  int16_t knobX = content.x + fillW - 4;
  int16_t knobY = content.y + content.height / 2 - 6;
  renderer.fillCircle(knobX + 4, knobY + 6, 5, knobColor_);
  
  // Focus indicator
  if (isFocused()) {
    renderer.drawCircle(knobX + 4, knobY + 6, 7, style_.getFocusColor());
  }
}

// UISliderF::render
inline void UISliderF::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  Rect content = style_.contentRect(bounds_);
  
  // Track
  int16_t trackY = content.y + content.height / 2 - 2;
  renderer.fillRect(content.x, trackY, content.width, 4, Color(40));
  
  // Fill
  float ratio = getNormalizedValue();
  int16_t fillW = (int16_t)(ratio * content.width);
  renderer.fillRect(content.x, trackY, fillW, 4, Colors::Primary);
  
  // Knob
  int16_t knobX = content.x + fillW - 4;
  renderer.fillCircle(knobX + 4, content.y + content.height / 2, 5, Colors::White);
}

// UICheckbox::render
inline void UICheckbox::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  StyleState state = getStyleState();
  Rect content = style_.contentRect(bounds_);
  
  // Checkbox box
  Rect boxRect(content.x, content.y + (content.height - boxSize_) / 2, boxSize_, boxSize_);
  renderer.drawRect(boxRect, style_.getTextColor(state));
  
  if (checked_) {
    // Draw checkmark
    renderer.drawIcon(boxRect.x + 1, boxRect.y + 1, IconType::CHECK, checkColor_);
  }
  
  // Focus indicator
  if (isFocused()) {
    renderer.drawRect(boxRect.inset(-2), style_.getFocusColor());
  }
  
  // Label
  if (label_[0]) {
    renderer.drawText(
      content.x + boxSize_ + 6,
      content.y + (content.height - textHeight(style_.getFontSize())) / 2,
      label_,
      style_.getTextColor(state),
      style_.getFontSize()
    );
  }
}

// UIRadioButton::render
inline void UIRadioButton::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  StyleState state = getStyleState();
  Rect content = style_.contentRect(bounds_);
  
  // Radio circle
  int16_t cx = content.x + boxSize_ / 2;
  int16_t cy = content.y + content.height / 2;
  renderer.drawCircle(cx, cy, boxSize_ / 2, style_.getTextColor(state));
  
  if (checked_) {
    renderer.fillCircle(cx, cy, boxSize_ / 2 - 3, checkColor_);
  }
  
  // Label
  if (label_[0]) {
    renderer.drawText(
      content.x + boxSize_ + 6,
      content.y + (content.height - textHeight(style_.getFontSize())) / 2,
      label_,
      style_.getTextColor(state),
      style_.getFontSize()
    );
  }
}

// UIToggle::render
inline void UIToggle::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  Rect content = bounds_;
  int16_t h = content.height;
  int16_t r = h / 2;
  
  // Track
  Color trackColor = on_ ? onColor_ : offColor_;
  renderer.fillRoundRect(content, r, trackColor);
  
  // Knob position (animated)
  int16_t knobX = content.x + 2 + (int16_t)(animPos_ * (content.width - h));
  renderer.fillCircle(knobX + r - 2, content.y + r, r - 2, Colors::White);
}

// UIDropdown::render
inline void UIDropdown::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  StyleState state = getStyleState();
  renderer.drawStyledRect(bounds_, style_, state);
  
  Rect content = style_.contentRect(bounds_);
  
  // Selected text or placeholder
  const char* displayText = selectedIndex_ >= 0 ? items_[selectedIndex_] : placeholder_;
  renderer.drawText(
    content.x, content.y + (content.height - textHeight(style_.getFontSize())) / 2,
    displayText,
    style_.getTextColor(state),
    style_.getFontSize()
  );
  
  // Dropdown arrow
  renderer.drawIcon(
    content.x + content.width - 10,
    content.y + (content.height - 8) / 2,
    open_ ? IconType::CHEVRON_UP : IconType::CHEVRON_DOWN,
    style_.getTextColor(state)
  );
  
  // Dropdown list when open
  if (open_ && itemCount_ > 0) {
    Size openSize = getOpenSize();
    Rect listRect(bounds_.x, bounds_.bottom(), bounds_.width, openSize.height);
    
    renderer.fillRect(listRect, style_.getBackgroundColor());
    renderer.drawRect(listRect, style_.getBorderColor());
    
    FontInfo font = getFontInfo(style_.getFontSize());
    int16_t itemH = font.charHeight + 4;
    int16_t y = listRect.y + 2;
    
    for (int i = 0; i < itemCount_ && i < maxVisibleItems_; i++) {
      if (i == highlightedIndex_) {
        renderer.fillRect(listRect.x + 1, y, listRect.width - 2, itemH, Color(60));
      }
      renderer.drawText(
        listRect.x + 4, y + 2,
        items_[i],
        style_.getTextColor(),
        style_.getFontSize()
      );
      y += itemH;
    }
  }
}

// UIMenuItem::render
inline void UIMenuItem::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  if (type_ == MenuItemType::SEPARATOR) {
    renderer.drawHLine(bounds_.x + 4, bounds_.y + 2, bounds_.width - 8, Colors::DarkGray);
    return;
  }
  
  StyleState state = getStyleState();
  renderer.drawStyledRect(bounds_, style_, state);
  
  Rect content = style_.contentRect(bounds_);
  Color textColor = style_.getTextColor(state);
  int16_t x = content.x;
  
  // Checkbox/Radio
  if (type_ == MenuItemType::CHECKBOX || type_ == MenuItemType::RADIO) {
    if (checked_) {
      renderer.drawIcon(x, content.y, IconType::CHECK, textColor);
    }
    x += 14;
  }
  
  // Icon
  if (icon_ != IconType::NONE) {
    renderer.drawIcon(x, content.y, icon_, textColor);
    x += 12;
  }
  
  // Label
  renderer.drawText(x, content.y, label_, textColor, style_.getFontSize());
  
  // Shortcut
  if (shortcut_[0]) {
    uint16_t shortcutW = textWidth(shortcut_, style_.getFontSize());
    renderer.drawText(
      content.right() - shortcutW - 10, content.y,
      shortcut_, Colors::Gray, style_.getFontSize()
    );
  }
  
  // Submenu arrow
  if (submenu_) {
    renderer.drawIcon(content.right() - 10, content.y, IconType::CHEVRON_RIGHT, textColor);
  }
}

// UIMenu::render
inline void UIMenu::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  renderer.drawStyledRect(bounds_, style_, getStyleState());
  
  Rect content = style_.contentRect(bounds_);
  
  // Title
  if (title_[0]) {
    renderer.drawText(
      content.x + 4, content.y + 2,
      title_, Colors::White, FontSize::MEDIUM
    );
    content.y += 16;
    content.height -= 16;
    renderer.drawHLine(content.x, content.y, content.width, Colors::DarkGray);
    content.y += 4;
  }
  
  // Items
  renderer.pushTranslation(content.x, content.y);
  for (auto* item : items_) {
    item->render(renderer);
  }
  renderer.popTranslation();
  
  // Active submenu
  if (activeSubmenu_ && activeSubmenu_->isVisible()) {
    activeSubmenu_->render(renderer);
  }
}

// UIScrollView::render
inline void UIScrollView::render(UIRenderer& renderer) {
  UIContainer::render(renderer);
  
  // Draw scrollbar
  if (showScrollbar_ && overflow_ == Overflow::SCROLL) {
    Rect content = style_.contentRect(bounds_);
    
    if (scrollDir_ == ScrollDirection::VERTICAL || scrollDir_ == ScrollDirection::BOTH) {
      if (contentSize_.height > content.height) {
        float ratio = (float)content.height / contentSize_.height;
        int16_t barH = std::max((int16_t)(content.height * ratio), (int16_t)10);
        int16_t barY = content.y + (int16_t)(getVerticalScrollRatio() * (content.height - barH));
        
        renderer.fillRect(
          content.right() - scrollbarWidth_, barY,
          scrollbarWidth_, barH,
          Color(100)
        );
      }
    }
  }
}

// UIGrid::render
inline void UIGrid::render(UIRenderer& renderer) {
  UIContainer::render(renderer);
  
  // Selection highlight
  Size cellSize = getCellSize();
  int16_t x = selectedCol_ * (cellSize.width + cellGap_);
  int16_t y = selectedRow_ * (cellSize.height + cellGap_);
  
  Rect selectRect(bounds_.x + x - 1, bounds_.y + y - 1, 
                  cellSize.width + 2, cellSize.height + 2);
  renderer.drawRect(selectRect, Colors::Primary);
}

// UITabs::render
inline void UITabs::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  Rect content = style_.contentRect(bounds_);
  
  // Tab bar background
  renderer.fillRect(content.x, content.y, content.width, 16, Color(40));
  
  // Tab headers
  int16_t x = content.x + 2;
  for (int i = 0; i < (int)tabs_.size(); i++) {
    uint16_t tabW = textWidth(tabs_[i].label, FontSize::SMALL) + 12;
    
    if (i == selectedTab_) {
      renderer.fillRect(x, content.y, tabW, 16, Color(60));
    }
    
    renderer.drawText(
      x + 6, content.y + 4,
      tabs_[i].label,
      i == selectedTab_ ? Colors::White : Colors::Gray,
      FontSize::SMALL
    );
    
    x += tabW + 2;
  }
  
  // Render content
  renderer.pushTranslation(content.x, content.y + 16);
  for (auto& tab : tabs_) {
    if (tab.content->isVisible()) {
      tab.content->render(renderer);
    }
  }
  renderer.popTranslation();
}

// UINotification::render
inline void UINotification::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  // Apply animation (slide in/out)
  float scale = animProgress_;
  int16_t offsetY = (int16_t)((1.0f - scale) * -20);
  
  renderer.pushTranslation(0, offsetY);
  renderer.drawStyledRect(bounds_, style_, getStyleState());
  
  Rect content = style_.contentRect(bounds_);
  
  // Icon
  if (icon_ != IconType::NONE) {
    Color iconColor;
    switch (type_) {
      case NotificationType::SUCCESS: iconColor = Colors::Success; break;
      case NotificationType::WARNING: iconColor = Colors::Warning; break;
      case NotificationType::ERROR: iconColor = Colors::Danger; break;
      default: iconColor = Colors::Primary; break;
    }
    renderer.drawIcon(content.x, content.y + 2, icon_, iconColor);
  }
  
  // Title and message
  int16_t textX = content.x + (icon_ != IconType::NONE ? 14 : 0);
  int16_t textY = content.y;
  
  if (title_[0]) {
    renderer.drawText(textX, textY, title_, Colors::White, FontSize::SMALL);
    textY += 10;
  }
  
  renderer.drawText(textX, textY, message_, Colors::LightGray, FontSize::TINY);
  
  renderer.popTranslation();
}

// UIDialog::render
inline void UIDialog::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  // Overlay
  if (showOverlay_) {
    renderer.fillRect(0, 0, screenW_, screenH_, overlayColor_);
  }
  
  // Dialog box
  renderer.drawStyledRect(bounds_, style_, getStyleState());
  
  Rect content = style_.contentRect(bounds_);
  
  // Title
  if (title_[0]) {
    renderer.drawText(
      content.x + (content.width - textWidth(title_, FontSize::MEDIUM)) / 2,
      content.y,
      title_, Colors::White, FontSize::MEDIUM
    );
  }
  
  // Message
  if (message_[0]) {
    renderer.drawTextInRect(
      Rect(content.x, content.y + 16, content.width, content.height - 36),
      message_,
      Colors::LightGray,
      FontSize::SMALL,
      HAlign::CENTER, VAlign::TOP
    );
  }
  
  // Buttons
  int16_t buttonY = content.y + content.height - 20;
  int16_t totalW = buttonElements_.size() * 40 + (buttonElements_.size() - 1) * 8;
  int16_t buttonX = content.x + (content.width - totalW) / 2;
  
  for (size_t i = 0; i < buttonElements_.size(); i++) {
    buttonElements_[i]->setPosition(buttonX, buttonY);
    buttonElements_[i]->setSize(40, 16);
    buttonElements_[i]->render(renderer);
    buttonX += 48;
  }
}

// UIProgressDialog::render  
inline void UIProgressDialog::render(UIRenderer& renderer) {
  UIDialog::render(renderer);
  
  Rect content = style_.contentRect(bounds_);
  
  // Progress bar
  Rect barRect(content.x + 10, content.y + content.height - 36, content.width - 20, 8);
  renderer.fillRect(barRect, Color(40));
  
  if (indeterminate_) {
    int16_t barW = barRect.width / 3;
    int16_t x = (int16_t)(animPhase_ * (barRect.width + barW)) - barW;
    renderer.fillRect(barRect.x + x, barRect.y, barW, barRect.height, Colors::Primary);
  } else {
    int16_t fillW = (int16_t)(progress_ * barRect.width);
    renderer.fillRect(barRect.x, barRect.y, fillW, barRect.height, Colors::Primary);
  }
}

// UICircularProgress::render
inline void UICircularProgress::render(UIRenderer& renderer) {
  if (!isVisible()) return;
  
  int16_t cx = bounds_.x + bounds_.width / 2;
  int16_t cy = bounds_.y + bounds_.height / 2;
  int16_t r = std::min(bounds_.width, bounds_.height) / 2 - 2;
  
  // Background circle
  renderer.drawCircle(cx, cy, r, Color(40));
  
  // Progress arc (simplified - just draw filled portion)
  if (!indeterminate_) {
    // Draw progress as arc segments
    float endAngle = value_ * 360.0f;
    // Simplified: draw a pie slice
    for (int angle = -90; angle < (int)endAngle - 90; angle++) {
      float rad = angle * 3.14159f / 180.0f;
      int16_t x = cx + (int16_t)(r * cos(rad));
      int16_t y = cy + (int16_t)(r * sin(rad));
      renderer.setPixel(x, y, color_);
    }
  } else {
    // Spinning indicator
    float startAngle = animPhase_ * 360.0f;
    for (int i = 0; i < 90; i++) {
      float rad = (startAngle + i - 90) * 3.14159f / 180.0f;
      int16_t x = cx + (int16_t)(r * cos(rad));
      int16_t y = cy + (int16_t)(r * sin(rad));
      renderer.setPixel(x, y, color_);
    }
  }
}

} // namespace UI
} // namespace SystemAPI
