/*****************************************************************
 * @file UICheckbox.hpp
 * @brief UI Framework Checkbox - Toggle checkbox control
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
 * @brief Checkbox element
 * 
 * @example
 * ```cpp
 * auto checkbox = new UICheckbox("Enable Feature");
 * checkbox->setChecked(true);
 * checkbox->onToggle([](bool checked) {
 *   printf("Checked: %d\n", checked);
 * });
 * ```
 */
class UICheckbox : public UIElement {
public:
  UICheckbox() {
    focusable_ = true;
    style_ = Styles::checkbox();
  }
  
  UICheckbox(const char* label) : UICheckbox() {
    setLabel(label);
  }
  
  UICheckbox(const char* label, bool checked) : UICheckbox(label) {
    setChecked(checked);
  }
  
  const char* getTypeName() const override { return "UICheckbox"; }
  bool isInteractive() const override { return true; }
  
  // ---- State ----
  
  void setChecked(bool checked) {
    if (checked_ != checked) {
      checked_ = checked;
      markDirty();
      if (onToggle_) onToggle_(checked_);
      if (onChange_) onChange_(this);
    }
  }
  
  bool isChecked() const { return checked_; }
  
  void toggle() { setChecked(!checked_); }
  
  // ---- Label ----
  
  void setLabel(const char* label) {
    if (label) {
      strncpy(label_, label, sizeof(label_) - 1);
      label_[sizeof(label_) - 1] = '\0';
    } else {
      label_[0] = '\0';
    }
    markDirty();
  }
  
  const char* getLabel() const { return label_; }
  
  // ---- Appearance ----
  
  void setBoxSize(uint8_t size) { boxSize_ = size; markDirty(); }
  uint8_t getBoxSize() const { return boxSize_; }
  
  void setCheckColor(const Color& color) { checkColor_ = color; markDirty(); }
  Color getCheckColor() const { return checkColor_; }
  
  // ---- Callback ----
  
  void onToggle(ValueCallback<bool> cb) { onToggle_ = cb; }
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    if (!isEnabled() || !isVisible()) return false;
    
    if (event.type == InputEvent::BUTTON) {
      if (event.btn.button == Button::SELECT && event.btn.event == ButtonEvent::RELEASED) {
        toggle();
        event.consumed = true;
        return true;
      }
    } else if (event.type == InputEvent::TOUCH) {
      if (event.touch.event == TouchEvent::UP && getScreenBounds().contains(event.touch.x, event.touch.y)) {
        toggle();
        event.consumed = true;
        return true;
      }
    }
    
    return UIElement::handleInput(event);
  }
  
  // ---- Layout ----
  
  Size getPreferredSize() const override {
    FontInfo font = getFontInfo(style_.getFontSize());
    uint16_t textW = label_[0] ? textWidth(label_, style_.getFontSize()) : 0;
    uint16_t gap = label_[0] ? 6 : 0;
    
    return Size(
      std::max(style_.getMinWidth(), (uint16_t)(boxSize_ + gap + textW + style_.horizontalSpace())),
      std::max(style_.getMinHeight(), (uint16_t)(std::max((int)boxSize_, (int)font.charHeight) + style_.verticalSpace()))
    );
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  bool checked_ = false;
  char label_[32] = "";
  uint8_t boxSize_ = 10;
  Color checkColor_ = Colors::Primary;
  
  ValueCallback<bool> onToggle_;
};

/**
 * @brief Radio button (single selection from group)
 */
class UIRadioButton : public UICheckbox {
public:
  UIRadioButton() : UICheckbox() {}
  UIRadioButton(const char* label) : UICheckbox(label) {}
  
  const char* getTypeName() const override { return "UIRadioButton"; }
  
  void setGroup(int group) { group_ = group; }
  int getGroup() const { return group_; }
  
  void render(UIRenderer& renderer) override;  // Renders as circle
  
protected:
  int group_ = 0;
};

/**
 * @brief Toggle switch (iOS-style)
 */
class UIToggle : public UIElement {
public:
  UIToggle() {
    focusable_ = true;
    style_.width(30).height(16);
  }
  
  UIToggle(bool initial) : UIToggle() {
    setOn(initial);
  }
  
  const char* getTypeName() const override { return "UIToggle"; }
  bool isInteractive() const override { return true; }
  
  void setOn(bool on) {
    if (on_ != on) {
      on_ = on;
      markDirty();
      if (onToggle_) onToggle_(on_);
      if (onChange_) onChange_(this);
    }
  }
  
  bool isOn() const { return on_; }
  void toggle() { setOn(!on_); }
  
  void setOnColor(const Color& color) { onColor_ = color; markDirty(); }
  void setOffColor(const Color& color) { offColor_ = color; markDirty(); }
  
  void onToggle(ValueCallback<bool> cb) { onToggle_ = cb; }
  
  bool handleInput(InputEvent& event) override {
    if (!isEnabled() || !isVisible()) return false;
    
    if (event.type == InputEvent::BUTTON) {
      if (event.btn.button == Button::SELECT && event.btn.event == ButtonEvent::RELEASED) {
        toggle();
        event.consumed = true;
        return true;
      }
    } else if (event.type == InputEvent::TOUCH) {
      if (event.touch.event == TouchEvent::UP && getScreenBounds().contains(event.touch.x, event.touch.y)) {
        toggle();
        event.consumed = true;
        return true;
      }
    }
    
    return UIElement::handleInput(event);
  }
  
  // Animate transition
  void update(uint32_t deltaMs) override {
    float target = on_ ? 1.0f : 0.0f;
    float diff = target - animPos_;
    if (abs(diff) > 0.01f) {
      animPos_ += diff * deltaMs * 0.01f;
      animPos_ = clamp(animPos_, 0.0f, 1.0f);
      markDirty();
    }
  }
  
  float getAnimPosition() const { return animPos_; }
  
  Size getPreferredSize() const override {
    return Size(style_.getMinWidth() ?: 30, style_.getMinHeight() ?: 16);
  }
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  bool on_ = false;
  Color onColor_ = Colors::Success;
  Color offColor_ = Color(60);
  float animPos_ = 0.0f;
  
  ValueCallback<bool> onToggle_;
};

} // namespace UI
} // namespace SystemAPI
