/*****************************************************************
 * @file UIStyle.hpp
 * @brief UI Framework Styling - CSS-like styling system
 * 
 * Provides a styling system similar to CSS for UI elements.
 * Supports:
 * - Colors (foreground, background, border)
 * - Spacing (padding, margin)
 * - Borders (width, radius)
 * - Typography (font size, alignment)
 * - State-based styling (normal, focused, pressed, disabled)
 * - Named themes
 * 
 * @example
 * ```cpp
 * // Create a button style
 * UIStyle buttonStyle;
 * buttonStyle
 *   .backgroundColor(Colors::Primary)
 *   .textColor(Colors::White)
 *   .padding(4, 8)
 *   .borderRadius(2)
 *   .font(FontSize::MEDIUM);
 * 
 * // Apply to button
 * button->setStyle(buttonStyle);
 * ```
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UICore.hpp"

namespace SystemAPI {
namespace UI {

// ============================================================
// Style Properties
// ============================================================

/**
 * @brief Element state for styling
 */
enum class StyleState : uint8_t {
  NORMAL = 0,
  FOCUSED = 1,
  PRESSED = 2,
  DISABLED = 3,
  HOVER = 4,    // For touch-enabled displays
  COUNT = 5
};

/**
 * @brief Border style
 */
enum class BorderStyle : uint8_t {
  NONE,
  SOLID,
  DASHED,
  DOTTED
};

/**
 * @brief Text decoration
 */
enum class TextDecoration : uint8_t {
  NONE,
  UNDERLINE,
  STRIKETHROUGH
};

// ============================================================
// UIStyle Class
// ============================================================

/**
 * @brief Complete styling for a UI element
 * 
 * Similar to CSS, this defines visual properties for an element.
 * State-specific styles can override base properties.
 */
class UIStyle {
public:
  // ---- Constructors ----
  
  UIStyle() {
    reset();
  }
  
  /**
   * @brief Reset to default values
   */
  void reset() {
    // Colors
    textColor_ = Colors::White;
    backgroundColor_ = Colors::Transparent;
    borderColor_ = Colors::Gray;
    focusColor_ = Colors::Primary;
    
    // Typography
    fontSize_ = FontSize::SMALL;
    fontStyle_ = FontStyle::NORMAL;
    textAlign_ = HAlign::LEFT;
    textVAlign_ = VAlign::CENTER;
    textDecoration_ = TextDecoration::NONE;
    
    // Spacing
    padding_ = Edges(0);
    margin_ = Edges(0);
    
    // Border
    borderWidth_ = 0;
    borderRadius_ = 0;
    borderStyle_ = BorderStyle::NONE;
    
    // Size
    minWidth_ = 0;
    minHeight_ = 0;
    maxWidth_ = 0xFFFF;
    maxHeight_ = 0xFFFF;
    
    // Layout
    flexGrow_ = 0;
    flexShrink_ = 1;
    
    // State overrides
    for (int i = 0; i < (int)StyleState::COUNT; i++) {
      hasStateTextColor_[i] = false;
      hasStateBackgroundColor_[i] = false;
      hasStateBorderColor_[i] = false;
    }
  }
  
  // ---- Color Setters (Fluent API) ----
  
  UIStyle& textColor(const Color& c) { textColor_ = c; return *this; }
  UIStyle& backgroundColor(const Color& c) { backgroundColor_ = c; return *this; }
  UIStyle& borderColor(const Color& c) { borderColor_ = c; return *this; }
  UIStyle& focusColor(const Color& c) { focusColor_ = c; return *this; }
  
  // State-specific colors
  UIStyle& textColor(StyleState state, const Color& c) {
    stateTextColor_[(int)state] = c;
    hasStateTextColor_[(int)state] = true;
    return *this;
  }
  
  UIStyle& backgroundColor(StyleState state, const Color& c) {
    stateBackgroundColor_[(int)state] = c;
    hasStateBackgroundColor_[(int)state] = true;
    return *this;
  }
  
  UIStyle& borderColor(StyleState state, const Color& c) {
    stateBorderColor_[(int)state] = c;
    hasStateBorderColor_[(int)state] = true;
    return *this;
  }
  
  // ---- Typography Setters ----
  
  UIStyle& font(FontSize size) { fontSize_ = size; return *this; }
  UIStyle& fontStyle(FontStyle style) { fontStyle_ = style; return *this; }
  UIStyle& textAlign(HAlign align) { textAlign_ = align; return *this; }
  UIStyle& textVAlign(VAlign align) { textVAlign_ = align; return *this; }
  UIStyle& textDecoration(TextDecoration dec) { textDecoration_ = dec; return *this; }
  
  // ---- Spacing Setters ----
  
  UIStyle& padding(uint8_t all) { padding_ = Edges(all); return *this; }
  UIStyle& padding(uint8_t v, uint8_t h) { padding_ = Edges(v, h); return *this; }
  UIStyle& padding(uint8_t t, uint8_t r, uint8_t b, uint8_t l) { 
    padding_ = Edges(t, r, b, l); return *this; 
  }
  
  UIStyle& margin(uint8_t all) { margin_ = Edges(all); return *this; }
  UIStyle& margin(uint8_t v, uint8_t h) { margin_ = Edges(v, h); return *this; }
  UIStyle& margin(uint8_t t, uint8_t r, uint8_t b, uint8_t l) { 
    margin_ = Edges(t, r, b, l); return *this; 
  }
  
  UIStyle& paddingTop(uint8_t v) { padding_.top = v; return *this; }
  UIStyle& paddingRight(uint8_t v) { padding_.right = v; return *this; }
  UIStyle& paddingBottom(uint8_t v) { padding_.bottom = v; return *this; }
  UIStyle& paddingLeft(uint8_t v) { padding_.left = v; return *this; }
  
  UIStyle& marginTop(uint8_t v) { margin_.top = v; return *this; }
  UIStyle& marginRight(uint8_t v) { margin_.right = v; return *this; }
  UIStyle& marginBottom(uint8_t v) { margin_.bottom = v; return *this; }
  UIStyle& marginLeft(uint8_t v) { margin_.left = v; return *this; }
  
  // ---- Border Setters ----
  
  UIStyle& border(uint8_t width, const Color& color = Colors::Gray, BorderStyle style = BorderStyle::SOLID) {
    borderWidth_ = width;
    borderColor_ = color;
    borderStyle_ = style;
    return *this;
  }
  
  UIStyle& borderWidth(uint8_t w) { borderWidth_ = w; return *this; }
  UIStyle& borderRadius(uint8_t r) { borderRadius_ = r; return *this; }
  UIStyle& borderStyle(BorderStyle s) { borderStyle_ = s; return *this; }
  
  // ---- Size Setters ----
  
  UIStyle& minWidth(uint16_t w) { minWidth_ = w; return *this; }
  UIStyle& minHeight(uint16_t h) { minHeight_ = h; return *this; }
  UIStyle& maxWidth(uint16_t w) { maxWidth_ = w; return *this; }
  UIStyle& maxHeight(uint16_t h) { maxHeight_ = h; return *this; }
  UIStyle& size(uint16_t w, uint16_t h) { 
    minWidth_ = maxWidth_ = w; 
    minHeight_ = maxHeight_ = h; 
    return *this; 
  }
  UIStyle& width(uint16_t w) { minWidth_ = maxWidth_ = w; return *this; }
  UIStyle& height(uint16_t h) { minHeight_ = maxHeight_ = h; return *this; }
  
  // ---- Flex Setters ----
  
  UIStyle& flexGrow(float g) { flexGrow_ = g; return *this; }
  UIStyle& flexShrink(float s) { flexShrink_ = s; return *this; }
  UIStyle& flex(float grow, float shrink = 1.0f) { 
    flexGrow_ = grow; 
    flexShrink_ = shrink; 
    return *this; 
  }
  
  // ---- Getters ----
  
  Color getTextColor(StyleState state = StyleState::NORMAL) const {
    if (hasStateTextColor_[(int)state]) return stateTextColor_[(int)state];
    return textColor_;
  }
  
  Color getBackgroundColor(StyleState state = StyleState::NORMAL) const {
    if (hasStateBackgroundColor_[(int)state]) return stateBackgroundColor_[(int)state];
    return backgroundColor_;
  }
  
  Color getBorderColor(StyleState state = StyleState::NORMAL) const {
    if (hasStateBorderColor_[(int)state]) return stateBorderColor_[(int)state];
    return borderColor_;
  }
  
  Color getFocusColor() const { return focusColor_; }
  
  FontSize getFontSize() const { return fontSize_; }
  FontStyle getFontStyle() const { return fontStyle_; }
  HAlign getTextAlign() const { return textAlign_; }
  VAlign getTextVAlign() const { return textVAlign_; }
  TextDecoration getTextDecoration() const { return textDecoration_; }
  
  const Edges& getPadding() const { return padding_; }
  const Edges& getMargin() const { return margin_; }
  
  uint8_t getBorderWidth() const { return borderWidth_; }
  uint8_t getBorderRadius() const { return borderRadius_; }
  BorderStyle getBorderStyle() const { return borderStyle_; }
  
  uint16_t getMinWidth() const { return minWidth_; }
  uint16_t getMinHeight() const { return minHeight_; }
  uint16_t getMaxWidth() const { return maxWidth_; }
  uint16_t getMaxHeight() const { return maxHeight_; }
  
  float getFlexGrow() const { return flexGrow_; }
  float getFlexShrink() const { return flexShrink_; }
  
  // ---- Utilities ----
  
  /**
   * @brief Get total horizontal space (padding + border + margin)
   */
  uint16_t horizontalSpace() const {
    return padding_.horizontal() + margin_.horizontal() + borderWidth_ * 2;
  }
  
  /**
   * @brief Get total vertical space
   */
  uint16_t verticalSpace() const {
    return padding_.vertical() + margin_.vertical() + borderWidth_ * 2;
  }
  
  /**
   * @brief Calculate content rect from outer rect
   */
  Rect contentRect(const Rect& outer) const {
    int16_t x = outer.x + margin_.left + borderWidth_ + padding_.left;
    int16_t y = outer.y + margin_.top + borderWidth_ + padding_.top;
    uint16_t w = outer.width - horizontalSpace();
    uint16_t h = outer.height - verticalSpace();
    return Rect(x, y, w, h);
  }
  
  /**
   * @brief Calculate border rect from outer rect
   */
  Rect borderRect(const Rect& outer) const {
    return Rect(
      outer.x + margin_.left,
      outer.y + margin_.top,
      outer.width - margin_.horizontal(),
      outer.height - margin_.vertical()
    );
  }
  
private:
  // Colors
  Color textColor_;
  Color backgroundColor_;
  Color borderColor_;
  Color focusColor_;
  
  // State-specific colors
  Color stateTextColor_[(int)StyleState::COUNT];
  Color stateBackgroundColor_[(int)StyleState::COUNT];
  Color stateBorderColor_[(int)StyleState::COUNT];
  bool hasStateTextColor_[(int)StyleState::COUNT];
  bool hasStateBackgroundColor_[(int)StyleState::COUNT];
  bool hasStateBorderColor_[(int)StyleState::COUNT];
  
  // Typography
  FontSize fontSize_;
  FontStyle fontStyle_;
  HAlign textAlign_;
  VAlign textVAlign_;
  TextDecoration textDecoration_;
  
  // Spacing
  Edges padding_;
  Edges margin_;
  
  // Border
  uint8_t borderWidth_;
  uint8_t borderRadius_;
  BorderStyle borderStyle_;
  
  // Size constraints
  uint16_t minWidth_, minHeight_;
  uint16_t maxWidth_, maxHeight_;
  
  // Flex layout
  float flexGrow_;
  float flexShrink_;
};

// ============================================================
// Predefined Styles
// ============================================================

namespace Styles {

/**
 * @brief Default text style
 */
inline UIStyle text() {
  return UIStyle()
    .textColor(Colors::White)
    .font(FontSize::SMALL);
}

/**
 * @brief Heading style
 */
inline UIStyle heading() {
  return UIStyle()
    .textColor(Colors::White)
    .font(FontSize::LARGE)
    .fontStyle(FontStyle::BOLD)
    .margin(0, 0, 4, 0);
}

/**
 * @brief Subheading style
 */
inline UIStyle subheading() {
  return UIStyle()
    .textColor(Colors::LightGray)
    .font(FontSize::MEDIUM)
    .margin(0, 0, 2, 0);
}

/**
 * @brief Primary button style
 */
inline UIStyle buttonPrimary() {
  return UIStyle()
    .backgroundColor(Colors::Primary)
    .textColor(Colors::White)
    .padding(4, 8)
    .borderRadius(2)
    .font(FontSize::SMALL)
    .textAlign(HAlign::CENTER)
    .backgroundColor(StyleState::FOCUSED, Colors::Primary.lighten(30))
    .backgroundColor(StyleState::PRESSED, Colors::Primary.darken(30))
    .backgroundColor(StyleState::DISABLED, Colors::DarkGray);
}

/**
 * @brief Secondary button style
 */
inline UIStyle buttonSecondary() {
  return UIStyle()
    .backgroundColor(Colors::Transparent)
    .textColor(Colors::White)
    .border(1, Colors::White)
    .padding(4, 8)
    .borderRadius(2)
    .font(FontSize::SMALL)
    .textAlign(HAlign::CENTER)
    .backgroundColor(StyleState::FOCUSED, Colors::White.withAlpha(30))
    .backgroundColor(StyleState::PRESSED, Colors::White.withAlpha(60));
}

/**
 * @brief Danger button style
 */
inline UIStyle buttonDanger() {
  return UIStyle()
    .backgroundColor(Colors::Danger)
    .textColor(Colors::White)
    .padding(4, 8)
    .borderRadius(2)
    .font(FontSize::SMALL)
    .textAlign(HAlign::CENTER)
    .backgroundColor(StyleState::FOCUSED, Colors::Danger.lighten(30))
    .backgroundColor(StyleState::PRESSED, Colors::Danger.darken(30));
}

/**
 * @brief Card/panel style
 */
inline UIStyle card() {
  return UIStyle()
    .backgroundColor(Color(30))
    .border(1, Colors::DarkGray)
    .borderRadius(4)
    .padding(8);
}

/**
 * @brief Menu item style
 */
inline UIStyle menuItem() {
  return UIStyle()
    .backgroundColor(Colors::Transparent)
    .textColor(Colors::White)
    .padding(4, 8)
    .font(FontSize::SMALL)
    .backgroundColor(StyleState::FOCUSED, Color(60))
    .backgroundColor(StyleState::PRESSED, Color(80));
}

/**
 * @brief Menu header style
 */
inline UIStyle menuHeader() {
  return UIStyle()
    .backgroundColor(Color(40))
    .textColor(Colors::White)
    .padding(4, 8)
    .font(FontSize::MEDIUM)
    .fontStyle(FontStyle::BOLD);
}

/**
 * @brief Input field style
 */
inline UIStyle input() {
  return UIStyle()
    .backgroundColor(Color(20))
    .textColor(Colors::White)
    .border(1, Colors::Gray)
    .padding(4, 6)
    .font(FontSize::SMALL)
    .borderColor(StyleState::FOCUSED, Colors::Primary);
}

/**
 * @brief Checkbox style
 */
inline UIStyle checkbox() {
  return UIStyle()
    .textColor(Colors::White)
    .font(FontSize::SMALL)
    .padding(2);
}

/**
 * @brief Progress bar style
 */
inline UIStyle progressBar() {
  return UIStyle()
    .backgroundColor(Color(40))
    .borderRadius(2)
    .height(8);
}

/**
 * @brief Slider style
 */
inline UIStyle slider() {
  return UIStyle()
    .backgroundColor(Color(40))
    .borderRadius(2)
    .height(4)
    .padding(8, 0);
}

/**
 * @brief Notification/toast style
 */
inline UIStyle notification() {
  return UIStyle()
    .backgroundColor(Color(50))
    .textColor(Colors::White)
    .border(1, Colors::Primary)
    .borderRadius(4)
    .padding(8, 12)
    .font(FontSize::SMALL);
}

/**
 * @brief Dialog/modal style
 */
inline UIStyle dialog() {
  return UIStyle()
    .backgroundColor(Color(30))
    .border(1, Colors::Gray)
    .borderRadius(4)
    .padding(12);
}

/**
 * @brief Scrollbar style
 */
inline UIStyle scrollbar() {
  return UIStyle()
    .backgroundColor(Color(40))
    .width(4)
    .borderRadius(2);
}

/**
 * @brief Dropdown style
 */
inline UIStyle dropdown() {
  return UIStyle()
    .backgroundColor(Color(30))
    .textColor(Colors::White)
    .border(1, Colors::Gray)
    .padding(4, 8)
    .font(FontSize::SMALL)
    .borderColor(StyleState::FOCUSED, Colors::Primary);
}

/**
 * @brief Status bar style
 */
inline UIStyle statusBar() {
  return UIStyle()
    .backgroundColor(Color(20))
    .textColor(Colors::LightGray)
    .padding(2, 4)
    .font(FontSize::TINY);
}

/**
 * @brief Icon style
 */
inline UIStyle icon() {
  return UIStyle()
    .textColor(Colors::White)
    .padding(2);
}

} // namespace Styles

// ============================================================
// Theme System
// ============================================================

/**
 * @brief Theme definition
 */
struct Theme {
  const char* name;
  
  // Primary colors
  Color primary;
  Color secondary;
  Color background;
  Color surface;
  Color error;
  
  // Text colors
  Color textPrimary;
  Color textSecondary;
  Color textDisabled;
  
  // Other
  Color divider;
  Color overlay;
};

namespace Themes {

/**
 * @brief Dark theme (default)
 */
inline Theme dark() {
  Theme t;
  t.name = "dark";
  t.primary = Colors::Primary;
  t.secondary = Color(100, 100, 100);
  t.background = Color(0);
  t.surface = Color(30);
  t.error = Colors::Danger;
  t.textPrimary = Colors::White;
  t.textSecondary = Colors::LightGray;
  t.textDisabled = Colors::DarkGray;
  t.divider = Color(50);
  t.overlay = Color(0, 0, 0, 180);
  return t;
}

/**
 * @brief Light theme
 */
inline Theme light() {
  Theme t;
  t.name = "light";
  t.primary = Colors::Primary;
  t.secondary = Color(150, 150, 150);
  t.background = Color(240);
  t.surface = Color(255);
  t.error = Colors::Danger;
  t.textPrimary = Color(30);
  t.textSecondary = Color(100);
  t.textDisabled = Color(180);
  t.divider = Color(200);
  t.overlay = Color(0, 0, 0, 100);
  return t;
}

/**
 * @brief High contrast theme (for accessibility)
 */
inline Theme highContrast() {
  Theme t;
  t.name = "highContrast";
  t.primary = Colors::Yellow;
  t.secondary = Colors::Cyan;
  t.background = Color(0);
  t.surface = Color(0);
  t.error = Colors::Red;
  t.textPrimary = Colors::White;
  t.textSecondary = Colors::Yellow;
  t.textDisabled = Colors::Gray;
  t.divider = Colors::White;
  t.overlay = Color(0, 0, 0, 200);
  return t;
}

/**
 * @brief OLED-optimized theme (minimal white pixels)
 */
inline Theme oled() {
  Theme t;
  t.name = "oled";
  t.primary = Color(0, 100, 200);
  t.secondary = Color(80, 80, 80);
  t.background = Color(0);
  t.surface = Color(0);
  t.error = Color(200, 50, 50);
  t.textPrimary = Color(200);
  t.textSecondary = Color(120);
  t.textDisabled = Color(60);
  t.divider = Color(40);
  t.overlay = Color(0, 0, 0, 200);
  return t;
}

} // namespace Themes

} // namespace UI
} // namespace SystemAPI
