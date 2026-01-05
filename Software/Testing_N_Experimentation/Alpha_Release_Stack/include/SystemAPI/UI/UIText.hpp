/*****************************************************************
 * @file UIText.hpp
 * @brief UI Framework Text - Text display element
 * 
 * UIText renders text with styling support. Like HTML span/p elements.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIElement.hpp"

namespace SystemAPI {
namespace UI {

/**
 * @brief Text display element
 * 
 * @example
 * ```cpp
 * auto label = new UIText("Hello World");
 * label->setStyle(Styles::heading());
 * 
 * auto info = new UIText("Status: OK");
 * info->setFont(FontSize::SMALL);
 * info->setColor(Colors::Green);
 * ```
 */
class UIText : public UIElement {
public:
  UIText() = default;
  
  UIText(const char* text) {
    setText(text);
  }
  
  UIText(const char* text, const UIStyle& style) {
    setText(text);
    setStyle(style);
  }
  
  const char* getTypeName() const override { return "UIText"; }
  
  // ---- Text Content ----
  
  void setText(const char* text) {
    if (text) {
      strncpy(text_, text, sizeof(text_) - 1);
      text_[sizeof(text_) - 1] = '\0';
    } else {
      text_[0] = '\0';
    }
    markDirty();
  }
  
  const char* getText() const { return text_; }
  
  // ---- Convenience Setters ----
  
  void setFont(FontSize size) {
    style_.font(size);
    markDirty();
  }
  
  void setColor(const Color& color) {
    style_.textColor(color);
    markDirty();
  }
  
  void setAlign(HAlign align) {
    style_.textAlign(align);
    markDirty();
  }
  
  // ---- Wrapping ----
  
  void setWrap(bool wrap) { wrap_ = wrap; markDirty(); }
  bool getWrap() const { return wrap_; }
  
  void setMaxLines(uint8_t maxLines) { maxLines_ = maxLines; markDirty(); }
  uint8_t getMaxLines() const { return maxLines_; }
  
  void setEllipsis(bool ellipsis) { ellipsis_ = ellipsis; markDirty(); }
  bool getEllipsis() const { return ellipsis_; }
  
  // ---- Layout ----
  
  Size getPreferredSize() const override {
    FontInfo font = getFontInfo(style_.getFontSize());
    uint16_t textW = textWidth(text_, style_.getFontSize());
    uint16_t textH = font.charHeight;
    
    if (wrap_ && bounds_.width > 0) {
      // Calculate wrapped height
      int lines = (textW / bounds_.width) + 1;
      if (maxLines_ > 0) lines = std::min(lines, (int)maxLines_);
      textH = lines * (font.charHeight + 2);
    }
    
    return Size(
      std::max(style_.getMinWidth(), (uint16_t)(textW + style_.getPadding().horizontal())),
      std::max(style_.getMinHeight(), (uint16_t)(textH + style_.getPadding().vertical()))
    );
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  char text_[128] = "";
  bool wrap_ = false;
  uint8_t maxLines_ = 0;  // 0 = unlimited
  bool ellipsis_ = true;  // Show ... if truncated
};

/**
 * @brief Label convenience class
 */
class UILabel : public UIText {
public:
  UILabel() = default;
  UILabel(const char* text) : UIText(text) {}
  UILabel(const char* text, const UIStyle& style) : UIText(text, style) {}
  
  const char* getTypeName() const override { return "UILabel"; }
};

} // namespace UI
} // namespace SystemAPI
