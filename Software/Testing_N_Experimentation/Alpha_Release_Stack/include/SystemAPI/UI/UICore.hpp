/*****************************************************************
 * @file UICore.hpp
 * @brief UI Framework Core - Fundamental types and utilities
 * 
 * This is the foundation of the ARCOS UI Framework for OLED displays.
 * Provides:
 * - Coordinate system (Point, Rect, Size)
 * - Color system (grayscale and RGB565)
 * - Font system (built-in pixel fonts)
 * - Input events (touch/button)
 * - Alignment and layout enums
 * 
 * The UI framework is designed like HTML/CSS:
 * - Elements are like HTML elements
 * - Styles are like CSS properties
 * - Containers handle layout
 * - UIManager handles the "DOM"
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <algorithm>

namespace SystemAPI {
namespace UI {

// ============================================================
// Forward Declarations
// ============================================================
class UIElement;
class UIContainer;
class UIStyle;

// ============================================================
// Geometry Types
// ============================================================

/**
 * @brief 2D Point
 */
struct Point {
  int16_t x = 0;
  int16_t y = 0;
  
  Point() = default;
  Point(int16_t x, int16_t y) : x(x), y(y) {}
  
  Point operator+(const Point& o) const { return {int16_t(x + o.x), int16_t(y + o.y)}; }
  Point operator-(const Point& o) const { return {int16_t(x - o.x), int16_t(y - o.y)}; }
  Point operator*(float s) const { return {int16_t(x * s), int16_t(y * s)}; }
  bool operator==(const Point& o) const { return x == o.x && y == o.y; }
  bool operator!=(const Point& o) const { return !(*this == o); }
  
  int16_t manhattanDist(const Point& o) const {
    return abs(x - o.x) + abs(y - o.y);
  }
};

/**
 * @brief 2D Size
 */
struct Size {
  uint16_t width = 0;
  uint16_t height = 0;
  
  Size() = default;
  Size(uint16_t w, uint16_t h) : width(w), height(h) {}
  
  bool operator==(const Size& o) const { return width == o.width && height == o.height; }
  bool operator!=(const Size& o) const { return !(*this == o); }
  
  uint32_t area() const { return (uint32_t)width * height; }
  bool isEmpty() const { return width == 0 || height == 0; }
};

/**
 * @brief Rectangle (position + size)
 */
struct Rect {
  int16_t x = 0;
  int16_t y = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  
  Rect() = default;
  Rect(int16_t x, int16_t y, uint16_t w, uint16_t h) : x(x), y(y), width(w), height(h) {}
  Rect(const Point& pos, const Size& size) : x(pos.x), y(pos.y), width(size.width), height(size.height) {}
  
  // Accessors
  Point position() const { return {x, y}; }
  Size size() const { return {width, height}; }
  
  int16_t left() const { return x; }
  int16_t top() const { return y; }
  int16_t right() const { return x + width; }
  int16_t bottom() const { return y + height; }
  Point center() const { return {int16_t(x + width/2), int16_t(y + height/2)}; }
  
  // Containment
  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
  bool contains(const Point& p) const { return contains(p.x, p.y); }
  bool contains(const Rect& r) const {
    return r.x >= x && r.right() <= right() && r.y >= y && r.bottom() <= bottom();
  }
  
  // Intersection
  bool intersects(const Rect& r) const {
    return !(r.x >= right() || r.right() <= x || r.y >= bottom() || r.bottom() <= y);
  }
  
  Rect intersection(const Rect& r) const {
    int16_t nx = std::max(x, r.x);
    int16_t ny = std::max(y, r.y);
    int16_t nr = std::min(right(), r.right());
    int16_t nb = std::min(bottom(), r.bottom());
    if (nr > nx && nb > ny) {
      return Rect(nx, ny, nr - nx, nb - ny);
    }
    return Rect();
  }
  
  // Union
  Rect unionWith(const Rect& r) const {
    if (isEmpty()) return r;
    if (r.isEmpty()) return *this;
    int16_t nx = std::min(x, r.x);
    int16_t ny = std::min(y, r.y);
    return Rect(nx, ny, std::max(right(), r.right()) - nx, std::max(bottom(), r.bottom()) - ny);
  }
  
  // Modification
  Rect inset(int16_t amount) const {
    return Rect(x + amount, y + amount, 
                width > 2*amount ? width - 2*amount : 0,
                height > 2*amount ? height - 2*amount : 0);
  }
  
  Rect offset(int16_t dx, int16_t dy) const {
    return Rect(x + dx, y + dy, width, height);
  }
  
  bool isEmpty() const { return width == 0 || height == 0; }
};

/**
 * @brief Edges/Padding/Margin (top, right, bottom, left - like CSS)
 */
struct Edges {
  uint8_t top = 0;
  uint8_t right = 0;
  uint8_t bottom = 0;
  uint8_t left = 0;
  
  Edges() = default;
  Edges(uint8_t all) : top(all), right(all), bottom(all), left(all) {}
  Edges(uint8_t v, uint8_t h) : top(v), right(h), bottom(v), left(h) {}
  Edges(uint8_t t, uint8_t h, uint8_t b) : top(t), right(h), bottom(b), left(h) {}
  Edges(uint8_t t, uint8_t r, uint8_t b, uint8_t l) : top(t), right(r), bottom(b), left(l) {}
  
  uint16_t horizontal() const { return left + right; }
  uint16_t vertical() const { return top + bottom; }
};

// ============================================================
// Color System
// ============================================================

/**
 * @brief Color (supports both grayscale OLED and RGB displays)
 */
struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;  // Alpha
  
  Color() = default;
  Color(uint8_t gray) : r(gray), g(gray), b(gray), a(255) {}
  Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
  
  // Grayscale for OLED
  uint8_t gray() const { return (r * 77 + g * 150 + b * 29) >> 8; }
  bool isOn() const { return gray() > 127; }  // For 1-bit displays
  
  // RGB565 for color displays
  uint16_t toRGB565() const {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  
  static Color fromRGB565(uint16_t c) {
    return Color(
      ((c >> 11) & 0x1F) << 3,
      ((c >> 5) & 0x3F) << 2,
      (c & 0x1F) << 3
    );
  }
  
  // Alpha blending
  Color blend(const Color& bg) const {
    if (a == 255) return *this;
    if (a == 0) return bg;
    uint16_t invA = 255 - a;
    return Color(
      (r * a + bg.r * invA) >> 8,
      (g * a + bg.g * invA) >> 8,
      (b * a + bg.b * invA) >> 8
    );
  }
  
  // Darken/Lighten
  Color darken(uint8_t amount) const {
    return Color(
      r > amount ? r - amount : 0,
      g > amount ? g - amount : 0,
      b > amount ? b - amount : 0, a);
  }
  
  Color lighten(uint8_t amount) const {
    return Color(
      r + amount < 256 ? r + amount : 255,
      g + amount < 256 ? g + amount : 255,
      b + amount < 256 ? b + amount : 255, a);
  }
  
  Color withAlpha(uint8_t newA) const { return Color(r, g, b, newA); }
  
  bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
};

// Predefined colors
namespace Colors {
  constexpr uint8_t WHITE_V = 255;
  constexpr uint8_t BLACK_V = 0;
  
  static const Color Transparent{0, 0, 0, 0};
  static const Color Black{0, 0, 0, 255};
  static const Color White{255, 255, 255, 255};
  static const Color Gray{128, 128, 128, 255};
  static const Color DarkGray{64, 64, 64, 255};
  static const Color LightGray{192, 192, 192, 255};
  
  static const Color Red{255, 0, 0, 255};
  static const Color Green{0, 255, 0, 255};
  static const Color Blue{0, 0, 255, 255};
  static const Color Yellow{255, 255, 0, 255};
  static const Color Cyan{0, 255, 255, 255};
  static const Color Magenta{255, 0, 255, 255};
  static const Color Orange{255, 165, 0, 255};
  
  // UI semantic colors
  static const Color Primary{66, 133, 244, 255};    // Blue
  static const Color Secondary{128, 128, 128, 255};
  static const Color Success{52, 168, 83, 255};     // Green
  static const Color Warning{251, 188, 4, 255};     // Yellow
  static const Color Danger{234, 67, 53, 255};      // Red
  static const Color Info{66, 133, 244, 255};       // Blue
}

// ============================================================
// Font System
// ============================================================
/**
 * @brief Font sizes (built-in pixel fonts)
 */
enum class FontSize : uint8_t {
  TINY = 0,     // 4x6  - Minimal
  SMALL = 1,    // 5x7  - Default small
  MEDIUM = 2,   // 6x8  - Standard
  LARGE = 3,    // 8x12 - Headers
  XLARGE = 4    // 12x16 - Titles
};

/**
 * @brief Font styles
 */
enum class FontStyle : uint8_t {
  NORMAL = 0,
  BOLD = 1,
  ITALIC = 2,
  BOLD_ITALIC = 3
};

/**
 * @brief Font information
 */
struct FontInfo {
  uint8_t charWidth;
  uint8_t charHeight;
  uint8_t spacing;
  const uint8_t* data;  // Bitmap font data
};

/**
 * @brief Get font metrics
 */
inline FontInfo getFontInfo(FontSize size) {
  switch (size) {
    case FontSize::TINY:   return {4, 6, 1, nullptr};
    case FontSize::SMALL:  return {5, 7, 1, nullptr};
    case FontSize::MEDIUM: return {6, 8, 1, nullptr};
    case FontSize::LARGE:  return {8, 12, 1, nullptr};
    case FontSize::XLARGE: return {12, 16, 2, nullptr};
    default: return {5, 7, 1, nullptr};
  }
}

// ============================================================
// Alignment & Layout
// ============================================================

/**
 * @brief Horizontal alignment
 */
enum class HAlign : uint8_t {
  LEFT,
  CENTER,
  RIGHT
};

/**
 * @brief Vertical alignment
 */
enum class VAlign : uint8_t {
  TOP,
  CENTER,
  BOTTOM
};

/**
 * @brief Combined alignment
 */
struct Alignment {
  HAlign horizontal = HAlign::LEFT;
  VAlign vertical = VAlign::TOP;
  
  Alignment() = default;
  Alignment(HAlign h, VAlign v) : horizontal(h), vertical(v) {}
  
  static Alignment topLeft() { return {HAlign::LEFT, VAlign::TOP}; }
  static Alignment topCenter() { return {HAlign::CENTER, VAlign::TOP}; }
  static Alignment topRight() { return {HAlign::RIGHT, VAlign::TOP}; }
  static Alignment centerLeft() { return {HAlign::LEFT, VAlign::CENTER}; }
  static Alignment center() { return {HAlign::CENTER, VAlign::CENTER}; }
  static Alignment centerRight() { return {HAlign::RIGHT, VAlign::CENTER}; }
  static Alignment bottomLeft() { return {HAlign::LEFT, VAlign::BOTTOM}; }
  static Alignment bottomCenter() { return {HAlign::CENTER, VAlign::BOTTOM}; }
  static Alignment bottomRight() { return {HAlign::RIGHT, VAlign::BOTTOM}; }
};

/**
 * @brief Flex direction (for layout containers)
 */
enum class FlexDirection : uint8_t {
  ROW,          // Horizontal, left to right
  ROW_REVERSE,  // Horizontal, right to left
  COLUMN,       // Vertical, top to bottom
  COLUMN_REVERSE // Vertical, bottom to top
};

/**
 * @brief Flex justify content (main axis)
 */
enum class JustifyContent : uint8_t {
  START,
  END,
  CENTER,
  SPACE_BETWEEN,
  SPACE_AROUND,
  SPACE_EVENLY
};

/**
 * @brief Flex align items (cross axis)
 */
enum class AlignItems : uint8_t {
  START,
  END,
  CENTER,
  STRETCH
};

/**
 * @brief Overflow handling
 */
enum class Overflow : uint8_t {
  VISIBLE,   // Show content outside bounds
  HIDDEN,    // Clip content at bounds
  SCROLL     // Enable scrolling
};

// ============================================================
// Input Events
// ============================================================

/**
 * @brief Input button IDs
 */
enum class Button : uint8_t {
  NONE = 0,
  UP,
  DOWN,
  LEFT,
  RIGHT,
  SELECT,    // Center/OK
  BACK,      // Back/Cancel
  MENU,      // Menu
  BUTTON_A,  // Generic A
  BUTTON_B,  // Generic B
  ENCODER_CW,   // Rotary encoder clockwise
  ENCODER_CCW   // Rotary encoder counter-clockwise
};

/**
 * @brief Button event types
 */
enum class ButtonEvent : uint8_t {
  PRESSED,
  RELEASED,
  HELD,
  REPEATED,
  LONG_PRESS
};

/**
 * @brief Touch event types
 */
enum class TouchEvent : uint8_t {
  DOWN,
  UP,
  MOVE,
  LONG_PRESS
};

/**
 * @brief Input event (button or touch)
 */
struct InputEvent {
  enum Type : uint8_t { BUTTON, TOUCH } type;
  
  union {
    struct {
      Button button;
      ButtonEvent event;
    } btn;
    struct {
      TouchEvent event;
      int16_t x, y;
    } touch;
  };
  
  uint32_t timestamp = 0;
  bool consumed = false;
  
  static InputEvent buttonPress(Button b) {
    InputEvent e;
    e.type = BUTTON;
    e.btn.button = b;
    e.btn.event = ButtonEvent::PRESSED;
    return e;
  }
  
  static InputEvent buttonRelease(Button b) {
    InputEvent e;
    e.type = BUTTON;
    e.btn.button = b;
    e.btn.event = ButtonEvent::RELEASED;
    return e;
  }
  
  static InputEvent touchDown(int16_t x, int16_t y) {
    InputEvent e;
    e.type = TOUCH;
    e.touch.event = TouchEvent::DOWN;
    e.touch.x = x;
    e.touch.y = y;
    return e;
  }
  
  static InputEvent touchUp(int16_t x, int16_t y) {
    InputEvent e;
    e.type = TOUCH;
    e.touch.event = TouchEvent::UP;
    e.touch.x = x;
    e.touch.y = y;
    return e;
  }
  
  static InputEvent touchMove(int16_t x, int16_t y) {
    InputEvent e;
    e.type = TOUCH;
    e.touch.event = TouchEvent::MOVE;
    e.touch.x = x;
    e.touch.y = y;
    return e;
  }
};

// ============================================================
// Callback Types
// ============================================================

/**
 * @brief Element callback (for events)
 */
using ElementCallback = std::function<void(UIElement*)>;

/**
 * @brief Value change callback
 */
template<typename T>
using ValueCallback = std::function<void(T)>;

/**
 * @brief Selection callback
 */
using SelectCallback = std::function<void(int index, const char* item)>;

// ============================================================
// Element State
// ============================================================

/**
 * @brief Element visibility state
 */
enum class Visibility : uint8_t {
  VISIBLE,    // Rendered and takes space
  HIDDEN,     // Not rendered but takes space
  GONE        // Not rendered and no space
};

/**
 * @brief Focus state
 */
enum class FocusState : uint8_t {
  NONE,       // Not focusable
  UNFOCUSED,  // Focusable but not focused
  FOCUSED,    // Currently focused
  ACTIVE      // Focused and being interacted with
};

// ============================================================
// ID System
// ============================================================

/**
 * @brief Element ID type
 */
using ElementID = uint16_t;

constexpr ElementID INVALID_ID = 0;

/**
 * @brief Generate unique element ID
 */
inline ElementID generateElementID() {
  static ElementID nextID = 1;
  return nextID++;
}

// ============================================================
// Utility Functions
// ============================================================

/**
 * @brief Clamp value to range
 */
template<typename T>
inline T clamp(T value, T min, T max) {
  return value < min ? min : (value > max ? max : value);
}

/**
 * @brief Linear interpolation
 */
inline float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

/**
 * @brief Map value from one range to another
 */
inline float mapRange(float value, float inMin, float inMax, float outMin, float outMax) {
  return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

/**
 * @brief Calculate text width
 */
inline uint16_t textWidth(const char* text, FontSize font) {
  if (!text) return 0;
  FontInfo info = getFontInfo(font);
  return strlen(text) * (info.charWidth + info.spacing);
}

/**
 * @brief Calculate text height
 */
inline uint16_t textHeight(FontSize font) {
  return getFontInfo(font).charHeight;
}

} // namespace UI
} // namespace SystemAPI
