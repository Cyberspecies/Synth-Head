/*****************************************************************
 * @file UISlider.hpp
 * @brief UI Framework Slider - Value slider control
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIElement.hpp"

namespace SystemAPI {
namespace UI {

/**
 * @brief Value slider element
 * 
 * @example
 * ```cpp
 * auto slider = new UISlider(0, 100);
 * slider->setValue(50);
 * slider->onValueChange([](int value) {
 *   printf("Value: %d\n", value);
 * });
 * 
 * // Float slider
 * auto floatSlider = new UISliderF(0.0f, 1.0f);
 * floatSlider->setValue(0.5f);
 * ```
 */
class UISlider : public UIElement {
public:
  UISlider() {
    focusable_ = true;
    style_ = Styles::slider();
  }
  
  UISlider(int min, int max) : UISlider() {
    setRange(min, max);
  }
  
  UISlider(int min, int max, int value) : UISlider(min, max) {
    setValue(value);
  }
  
  const char* getTypeName() const override { return "UISlider"; }
  bool isInteractive() const override { return true; }
  
  // ---- Value ----
  
  void setValue(int value) {
    int newVal = clamp(value, min_, max_);
    if (newVal != value_) {
      value_ = newVal;
      markDirty();
      if (onValueChange_) onValueChange_(value_);
      if (onChange_) onChange_(this);
    }
  }
  
  int getValue() const { return value_; }
  
  // ---- Range ----
  
  void setRange(int min, int max) {
    min_ = min;
    max_ = max;
    setValue(value_);  // Re-clamp
    markDirty();
  }
  
  int getMin() const { return min_; }
  int getMax() const { return max_; }
  
  // ---- Step ----
  
  void setStep(int step) { step_ = step; }
  int getStep() const { return step_; }
  
  // ---- Appearance ----
  
  void setTrackColor(const Color& color) { trackColor_ = color; markDirty(); }
  void setFillColor(const Color& color) { fillColor_ = color; markDirty(); }
  void setKnobColor(const Color& color) { knobColor_ = color; markDirty(); }
  
  void setShowValue(bool show) { showValue_ = show; markDirty(); }
  bool getShowValue() const { return showValue_; }
  
  void setOrientation(ProgressOrientation orient) { orientation_ = orient; markDirty(); }
  
  // ---- Callback ----
  
  void onValueChange(ValueCallback<int> cb) { onValueChange_ = cb; }
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    if (!isEnabled() || !isVisible() || !isFocused()) return false;
    
    if (event.type == InputEvent::BUTTON) {
      if (event.btn.event == ButtonEvent::PRESSED || event.btn.event == ButtonEvent::REPEATED) {
        if (event.btn.button == Button::RIGHT || event.btn.button == Button::UP ||
            event.btn.button == Button::ENCODER_CW) {
          setValue(value_ + step_);
          event.consumed = true;
          return true;
        } else if (event.btn.button == Button::LEFT || event.btn.button == Button::DOWN ||
                   event.btn.button == Button::ENCODER_CCW) {
          setValue(value_ - step_);
          event.consumed = true;
          return true;
        }
      }
    } else if (event.type == InputEvent::TOUCH) {
      Rect bounds = getScreenBounds();
      if (bounds.contains(event.touch.x, event.touch.y)) {
        if (event.touch.event == TouchEvent::DOWN || event.touch.event == TouchEvent::MOVE) {
          // Calculate value from touch position
          float ratio;
          if (orientation_ == ProgressOrientation::HORIZONTAL) {
            ratio = (float)(event.touch.x - bounds.x) / bounds.width;
          } else {
            ratio = 1.0f - (float)(event.touch.y - bounds.y) / bounds.height;
          }
          ratio = clamp(ratio, 0.0f, 1.0f);
          setValue(min_ + (int)(ratio * (max_ - min_)));
          event.consumed = true;
          return true;
        }
      }
    }
    
    return UIElement::handleInput(event);
  }
  
  // ---- Normalized Value ----
  
  float getNormalizedValue() const {
    if (max_ == min_) return 0.0f;
    return (float)(value_ - min_) / (max_ - min_);
  }
  
  // ---- Layout ----
  
  Size getPreferredSize() const override {
    if (orientation_ == ProgressOrientation::HORIZONTAL) {
      return Size(
        std::max(style_.getMinWidth(), (uint16_t)60),
        std::max(style_.getMinHeight(), (uint16_t)16)
      );
    } else {
      return Size(
        std::max(style_.getMinWidth(), (uint16_t)16),
        std::max(style_.getMinHeight(), (uint16_t)60)
      );
    }
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  int value_ = 0;
  int min_ = 0;
  int max_ = 100;
  int step_ = 1;
  
  Color trackColor_ = Color(40);
  Color fillColor_ = Colors::Primary;
  Color knobColor_ = Colors::White;
  
  bool showValue_ = false;
  ProgressOrientation orientation_ = ProgressOrientation::HORIZONTAL;
  
  ValueCallback<int> onValueChange_;
};

/**
 * @brief Float value slider
 */
class UISliderF : public UIElement {
public:
  UISliderF() {
    focusable_ = true;
    style_ = Styles::slider();
  }
  
  UISliderF(float min, float max) : UISliderF() {
    setRange(min, max);
  }
  
  const char* getTypeName() const override { return "UISliderF"; }
  bool isInteractive() const override { return true; }
  
  void setValue(float value) {
    float newVal = clamp(value, min_, max_);
    if (newVal != value_) {
      value_ = newVal;
      markDirty();
      if (onValueChange_) onValueChange_(value_);
      if (onChange_) onChange_(this);
    }
  }
  
  float getValue() const { return value_; }
  
  void setRange(float min, float max) {
    min_ = min;
    max_ = max;
    setValue(value_);
    markDirty();
  }
  
  float getMin() const { return min_; }
  float getMax() const { return max_; }
  
  void setStep(float step) { step_ = step; }
  float getStep() const { return step_; }
  
  void onValueChange(ValueCallback<float> cb) { onValueChange_ = cb; }
  
  float getNormalizedValue() const {
    if (max_ == min_) return 0.0f;
    return (value_ - min_) / (max_ - min_);
  }
  
  bool handleInput(InputEvent& event) override {
    if (!isEnabled() || !isVisible() || !isFocused()) return false;
    
    if (event.type == InputEvent::BUTTON) {
      if (event.btn.event == ButtonEvent::PRESSED || event.btn.event == ButtonEvent::REPEATED) {
        if (event.btn.button == Button::RIGHT || event.btn.button == Button::ENCODER_CW) {
          setValue(value_ + step_);
          event.consumed = true;
          return true;
        } else if (event.btn.button == Button::LEFT || event.btn.button == Button::ENCODER_CCW) {
          setValue(value_ - step_);
          event.consumed = true;
          return true;
        }
      }
    }
    
    return UIElement::handleInput(event);
  }
  
  Size getPreferredSize() const override {
    return Size(
      std::max(style_.getMinWidth(), (uint16_t)60),
      std::max(style_.getMinHeight(), (uint16_t)16)
    );
  }
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  float value_ = 0.0f;
  float min_ = 0.0f;
  float max_ = 1.0f;
  float step_ = 0.01f;
  
  ValueCallback<float> onValueChange_;
};

} // namespace UI
} // namespace SystemAPI
