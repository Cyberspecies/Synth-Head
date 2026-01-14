/*****************************************************************
 * @file DisplayTypes.hpp
 * @brief Types for multi-display system configuration
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include <cstdint>

namespace AnimationDriver {

// ============================================================
// Display Identifier
// ============================================================

enum class DisplayId : uint8_t {
    HUB75_LEFT = 0,     // Left HUB75 panel
    HUB75_RIGHT = 1,    // Right HUB75 panel
    HUB75_COMBINED = 2, // Both HUB75 as single virtual display
    OLED_PRIMARY = 3,   // Primary OLED display
    OLED_SECONDARY = 4, // Secondary OLED (if present)
    CUSTOM_0 = 10,
    CUSTOM_1 = 11,
    CUSTOM_2 = 12,
    CUSTOM_3 = 13
};

// ============================================================
// Display Type
// ============================================================

enum class DisplayType : uint8_t {
    HUB75_RGB,          // RGB LED matrix
    OLED_MONO,          // Monochrome OLED
    OLED_RGB,           // RGB OLED
    LCD_RGB,            // LCD display
    WS2812,             // Addressable LED strip
    VIRTUAL             // Virtual/combined display
};

// ============================================================
// Display Configuration
// ============================================================

struct DisplayConfig {
    DisplayId id;
    DisplayType type;
    
    // Physical dimensions
    int width;
    int height;
    
    // Position in global coordinate space (for combined displays)
    int globalX;
    int globalY;
    
    // Rotation/transformation
    int rotation;       // 0, 90, 180, 270 degrees
    bool flipX;
    bool flipY;
    
    // Enabled state
    bool enabled;
    
    DisplayConfig() 
        : id(DisplayId::HUB75_LEFT), type(DisplayType::HUB75_RGB),
          width(64), height(32), globalX(0), globalY(0),
          rotation(0), flipX(false), flipY(false), enabled(true) {}
    
    // Factory methods for common configurations
    static DisplayConfig Hub75Left() {
        DisplayConfig cfg;
        cfg.id = DisplayId::HUB75_LEFT;
        cfg.type = DisplayType::HUB75_RGB;
        cfg.width = 64;
        cfg.height = 32;
        cfg.globalX = 0;
        cfg.globalY = 0;
        return cfg;
    }
    
    static DisplayConfig Hub75Right() {
        DisplayConfig cfg;
        cfg.id = DisplayId::HUB75_RIGHT;
        cfg.type = DisplayType::HUB75_RGB;
        cfg.width = 64;
        cfg.height = 32;
        cfg.globalX = 64;
        cfg.globalY = 0;
        return cfg;
    }
    
    static DisplayConfig Hub75Combined() {
        DisplayConfig cfg;
        cfg.id = DisplayId::HUB75_COMBINED;
        cfg.type = DisplayType::VIRTUAL;
        cfg.width = 128;
        cfg.height = 32;
        cfg.globalX = 0;
        cfg.globalY = 0;
        return cfg;
    }
    
    static DisplayConfig Oled128x128() {
        DisplayConfig cfg;
        cfg.id = DisplayId::OLED_PRIMARY;
        cfg.type = DisplayType::OLED_MONO;
        cfg.width = 128;
        cfg.height = 128;
        cfg.globalX = 0;
        cfg.globalY = 0;
        return cfg;
    }
};

// ============================================================
// Coordinate Mapping
// ============================================================

struct PixelCoord {
    int x;
    int y;
    DisplayId display;
    
    PixelCoord() : x(0), y(0), display(DisplayId::HUB75_LEFT) {}
    PixelCoord(int px, int py, DisplayId d = DisplayId::HUB75_LEFT)
        : x(px), y(py), display(d) {}
};

// ============================================================
// Display Region (for partial updates)
// ============================================================

struct DisplayRegion {
    int x1, y1;     // Top-left corner
    int x2, y2;     // Bottom-right corner (exclusive)
    
    DisplayRegion() : x1(0), y1(0), x2(0), y2(0) {}
    DisplayRegion(int px1, int py1, int px2, int py2)
        : x1(px1), y1(py1), x2(px2), y2(py2) {}
    
    int width() const { return x2 - x1; }
    int height() const { return y2 - y1; }
    bool contains(int x, int y) const {
        return x >= x1 && x < x2 && y >= y1 && y < y2;
    }
    
    // Intersect with another region
    DisplayRegion intersect(const DisplayRegion& other) const {
        return DisplayRegion(
            (x1 > other.x1) ? x1 : other.x1,
            (y1 > other.y1) ? y1 : other.y1,
            (x2 < other.x2) ? x2 : other.x2,
            (y2 < other.y2) ? y2 : other.y2
        );
    }
};

} // namespace AnimationDriver
