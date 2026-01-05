/*****************************************************************
 * @file UIButton.hpp
 * @brief UI Framework Button - Interactive button element
 * 
 * Buttons for user interaction. Supports text, icons, or both.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIElement.hpp"
#include "UIIcon.hpp"

namespace SystemAPI {
namespace UI {

/**
 * @brief Interactive button element
 * 
 * @example
 * ```cpp
 * // Text button
 * auto btn = new UIButton("Click Me");
 * btn->onClick([](UIElement* e) {
 *   printf("Clicked!\n");
 * });
 * 
 * // Icon button
 * auto iconBtn = new UIButton();
 * iconBtn->setIcon(IconType::SETTINGS);
 * 
 * // Icon + Text
 * auto combo = new UIButton("Settings", IconType::SETTINGS);
 * ```
 */
class UIButton : public UIElement {
public:
  UIButton() {
    focusable_ = true;
    style_ = Styles::buttonPrimary();
  }
  
  UIButton(const char* text) : UIButton() {
    setText(text);
  }
  
  UIButton(const char* text, IconType icon) : UIButton() {
    setText(text);
    setIcon(icon);
  }
  
  UIButton(IconType icon) : UIButton() {
    setIcon(icon);
  }
  
  const char* getTypeName() const override { return "UIButton"; }
  bool isInteractive() const override { return true; }
  
  // ---- Text ----
  
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
  
  // ---- Icon ----
  
  void setIcon(IconType icon) { icon_ = icon; markDirty(); }
  IconType getIcon() const { return icon_; }
  
  void setIconPosition(HAlign pos) { iconPosition_ = pos; markDirty(); }
  HAlign getIconPosition() const { return iconPosition_; }
  
  // ---- Toggle Mode ----
  
  void setToggle(bool isToggle) { isToggle_ = isToggle; }
  bool isToggle() const { return isToggle_; }
  
  void setToggled(bool toggled) { 
    if (toggled_ != toggled) {
      toggled_ = toggled; 
      markDirty();
      if (onChange_) onChange_(this);
    }
  }
  bool isToggled() const { return toggled_; }
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    if (!isEnabled() || !isVisible()) return false;
    
    bool handled = UIElement::handleInput(event);
    
    // Handle toggle on release
    if (handled && event.btn.event == ButtonEvent::RELEASED && isToggle_) {
      setToggled(!toggled_);
    }
    
    return handled;
  }
  
  // ---- Layout ----
  
  Size getPreferredSize() const override {
    FontInfo font = getFontInfo(style_.getFontSize());
    
    uint16_t textW = text_[0] ? textWidth(text_, style_.getFontSize()) : 0;
    uint16_t iconW = (icon_ != IconType::NONE) ? 8 : 0;
    uint16_t gap = (text_[0] && icon_ != IconType::NONE) ? 4 : 0;
    
    uint16_t contentW = textW + iconW + gap;
    uint16_t contentH = std::max(font.charHeight, iconW);
    
    return Size(
      std::max(style_.getMinWidth(), (uint16_t)(contentW + style_.horizontalSpace())),
      std::max(style_.getMinHeight(), (uint16_t)(contentH + style_.verticalSpace()))
    );
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  char text_[32] = "";
  IconType icon_ = IconType::NONE;
  HAlign iconPosition_ = HAlign::LEFT;
  bool isToggle_ = false;
  bool toggled_ = false;
};

/**
 * @brief Icon-only button
 */
class UIIconButton : public UIButton {
public:
  UIIconButton() {
    style_.padding(4);
  }
  
  UIIconButton(IconType icon) : UIIconButton() {
    setIcon(icon);
  }
  
  const char* getTypeName() const override { return "UIIconButton"; }
};

} // namespace UI
} // namespace SystemAPI
