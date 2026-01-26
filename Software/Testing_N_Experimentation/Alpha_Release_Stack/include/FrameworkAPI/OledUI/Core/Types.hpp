/**
 * @file Types.hpp
 * @brief Core types and enums for OLED UI framework
 * 
 * This file defines the fundamental types used throughout the UI system,
 * similar to CSS units, alignments, and display properties.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace OledUI {

//=============================================================================
// Display Constants
//=============================================================================
constexpr int16_t OLED_WIDTH = 128;
constexpr int16_t OLED_HEIGHT = 128;

//=============================================================================
// Alignment (like CSS text-align, align-items)
//=============================================================================
enum class Align : uint8_t {
    START,      // Left/Top
    CENTER,     // Center
    END,        // Right/Bottom
    STRETCH,    // Fill available space
};

//=============================================================================
// Flex Direction (like CSS flex-direction)
//=============================================================================
enum class FlexDirection : uint8_t {
    ROW,        // Horizontal layout
    COLUMN,     // Vertical layout
};

//=============================================================================
// Justify Content (like CSS justify-content)
//=============================================================================
enum class Justify : uint8_t {
    START,          // Pack items at start
    CENTER,         // Pack items at center
    END,            // Pack items at end
    SPACE_BETWEEN,  // Distribute with space between
    SPACE_AROUND,   // Distribute with space around
    SPACE_EVENLY,   // Distribute with equal space
};

//=============================================================================
// Text Size (predefined font sizes)
//=============================================================================
enum class TextSize : uint8_t {
    SMALL = 1,      // 5x7 font
    MEDIUM = 2,     // 10x14 (2x scale)
    LARGE = 3,      // 15x21 (3x scale)
};

//=============================================================================
// Icon Set (built-in icons)
//=============================================================================
enum class Icon : uint8_t {
    NONE = 0,
    HOME,
    BACK,
    SETTINGS,
    WIFI,
    WIFI_OFF,
    BLUETOOTH,
    BATTERY_FULL,
    BATTERY_HALF,
    BATTERY_LOW,
    BATTERY_EMPTY,
    GPS,
    GPS_OFF,
    SPEAKER,
    SPEAKER_OFF,
    MIC,
    MIC_OFF,
    CHECK,
    CROSS,
    ARROW_UP,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    MENU,
    EDIT,
    SAVE,
    DELETE,
    PLAY,
    PAUSE,
    STOP,
    REFRESH,
    INFO,
    WARNING,
    ERROR,
    HEART,
    STAR,
    EYE,
};

//=============================================================================
// Input Events
//=============================================================================
enum class InputEvent : uint8_t {
    NONE = 0,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    SELECT,     // Center button / confirm
    BACK,       // Back button / cancel
    MENU,       // Menu button
};

//=============================================================================
// Transition Types (like CSS transitions)
//=============================================================================
enum class Transition : uint8_t {
    NONE,           // Instant change
    FADE,           // Fade in/out
    SLIDE_LEFT,     // Slide from right
    SLIDE_RIGHT,    // Slide from left
    SLIDE_UP,       // Slide from bottom
    SLIDE_DOWN,     // Slide from top
};

//=============================================================================
// Rect - Basic rectangle structure
//=============================================================================
struct Rect {
    int16_t x = 0;
    int16_t y = 0;
    int16_t width = 0;
    int16_t height = 0;
    
    Rect() = default;
    Rect(int16_t x_, int16_t y_, int16_t w_, int16_t h_) 
        : x(x_), y(y_), width(w_), height(h_) {}
    
    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
    
    bool intersects(const Rect& other) const {
        return !(x >= other.x + other.width || x + width <= other.x ||
                 y >= other.y + other.height || y + height <= other.y);
    }
};

//=============================================================================
// Spacing - Margin/Padding (like CSS margin/padding)
//=============================================================================
struct Spacing {
    int16_t top = 0;
    int16_t right = 0;
    int16_t bottom = 0;
    int16_t left = 0;
    
    Spacing() = default;
    Spacing(int16_t all) : top(all), right(all), bottom(all), left(all) {}
    Spacing(int16_t vertical, int16_t horizontal) 
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    Spacing(int16_t t, int16_t r, int16_t b, int16_t l) 
        : top(t), right(r), bottom(b), left(l) {}
    
    int16_t horizontal() const { return left + right; }
    int16_t vertical() const { return top + bottom; }
};

//=============================================================================
// Color - Monochrome color (OLED specific)
//=============================================================================
struct OledColor {
    bool white = true;      // true = white pixel, false = black
    bool inverted = false;  // Invert foreground/background
    
    OledColor() = default;
    OledColor(bool w, bool inv = false) : white(w), inverted(inv) {}
    
    static OledColor White() { return OledColor(true, false); }
    static OledColor Black() { return OledColor(false, false); }
    static OledColor Inverted() { return OledColor(true, true); }
};

//=============================================================================
// Event Callback Types
//=============================================================================
using Callback = std::function<void()>;  // Generic callback alias
using OnClickCallback = std::function<void()>;
using OnFocusCallback = std::function<void(bool focused)>;
using OnValueChangeCallback = std::function<void(int value)>;
using OnTextChangeCallback = std::function<void(const std::string& text)>;

//=============================================================================
// Element ID type
//=============================================================================
using ElementId = uint16_t;
constexpr ElementId INVALID_ELEMENT_ID = 0xFFFF;

} // namespace OledUI
