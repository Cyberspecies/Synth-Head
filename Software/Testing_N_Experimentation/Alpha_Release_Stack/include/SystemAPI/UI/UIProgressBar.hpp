/*****************************************************************
 * @file UIProgressBar.hpp
 * @brief UI Framework Progress Bar - Progress indicators
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIElement.hpp"

namespace SystemAPI {
namespace UI {

/**
 * @brief Progress bar orientation
 */
enum class ProgressOrientation : uint8_t {
  HORIZONTAL,
  VERTICAL
};

/**
 * @brief Progress bar element
 * 
 * @example
 * ```cpp
 * auto progress = new UIProgressBar();
 * progress->setValue(0.5f);  // 50%
 * progress->setColor(Colors::Success);
 * 
 * // Indeterminate (loading)
 * auto loading = new UIProgressBar();
 * loading->setIndeterminate(true);
 * ```
 */
class UIProgressBar : public UIElement {
public:
  UIProgressBar() {
    style_ = Styles::progressBar();
  }
  
  const char* getTypeName() const override { return "UIProgressBar"; }
  
  // ---- Value ----
  
  void setValue(float value) { 
    value_ = clamp(value, 0.0f, 1.0f); 
    markDirty(); 
  }
  
  float getValue() const { return value_; }
  
  void setPercent(int percent) { setValue(percent / 100.0f); }
  int getPercent() const { return (int)(value_ * 100); }
  
  // ---- Appearance ----
  
  void setBarColor(const Color& color) { barColor_ = color; markDirty(); }
  Color getBarColor() const { return barColor_; }
  
  void setOrientation(ProgressOrientation orient) { orientation_ = orient; markDirty(); }
  ProgressOrientation getOrientation() const { return orientation_; }
  
  void setShowLabel(bool show) { showLabel_ = show; markDirty(); }
  bool getShowLabel() const { return showLabel_; }
  
  // ---- Indeterminate Mode ----
  
  void setIndeterminate(bool ind) { indeterminate_ = ind; markDirty(); }
  bool isIndeterminate() const { return indeterminate_; }
  
  // ---- Animation ----
  
  void update(uint32_t deltaMs) override {
    if (indeterminate_) {
      animPhase_ += deltaMs * 0.002f;
      if (animPhase_ > 1.0f) animPhase_ -= 1.0f;
      markDirty();
    }
  }
  
  float getAnimPhase() const { return animPhase_; }
  
  // ---- Layout ----
  
  Size getPreferredSize() const override {
    if (orientation_ == ProgressOrientation::HORIZONTAL) {
      return Size(
        std::max(style_.getMinWidth(), (uint16_t)50),
        std::max(style_.getMinHeight(), (uint16_t)8)
      );
    } else {
      return Size(
        std::max(style_.getMinWidth(), (uint16_t)8),
        std::max(style_.getMinHeight(), (uint16_t)50)
      );
    }
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  float value_ = 0.0f;
  Color barColor_ = Colors::Primary;
  ProgressOrientation orientation_ = ProgressOrientation::HORIZONTAL;
  bool showLabel_ = false;
  bool indeterminate_ = false;
  float animPhase_ = 0.0f;
};

/**
 * @brief Circular progress indicator
 */
class UICircularProgress : public UIElement {
public:
  UICircularProgress() {
    style_.width(24).height(24);
  }
  
  const char* getTypeName() const override { return "UICircularProgress"; }
  
  void setValue(float value) { value_ = clamp(value, 0.0f, 1.0f); markDirty(); }
  float getValue() const { return value_; }
  
  void setColor(const Color& color) { color_ = color; markDirty(); }
  Color getColor() const { return color_; }
  
  void setThickness(uint8_t t) { thickness_ = t; markDirty(); }
  uint8_t getThickness() const { return thickness_; }
  
  void setIndeterminate(bool ind) { indeterminate_ = ind; markDirty(); }
  bool isIndeterminate() const { return indeterminate_; }
  
  void update(uint32_t deltaMs) override {
    if (indeterminate_) {
      animPhase_ += deltaMs * 0.003f;
      if (animPhase_ > 1.0f) animPhase_ -= 1.0f;
      markDirty();
    }
  }
  
  float getAnimPhase() const { return animPhase_; }
  
  Size getPreferredSize() const override {
    return Size(style_.getMinWidth() ?: 24, style_.getMinHeight() ?: 24);
  }
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  float value_ = 0.0f;
  Color color_ = Colors::Primary;
  uint8_t thickness_ = 2;
  bool indeterminate_ = false;
  float animPhase_ = 0.0f;
};

} // namespace UI
} // namespace SystemAPI
