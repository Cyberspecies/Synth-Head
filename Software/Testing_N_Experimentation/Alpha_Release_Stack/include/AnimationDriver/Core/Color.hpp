/*****************************************************************
 * @file Color.hpp
 * @brief Color utilities for the Animation Driver
 * 
 * Provides RGB, HSV, and HSL color representations with conversion
 * functions and blending operations.
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace AnimationDriver {

// ============================================================
// RGB Color (8-bit per channel)
// ============================================================

struct RGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    
    constexpr RGB() = default;
    constexpr RGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
    
    // From 32-bit packed (0xRRGGBB)
    static RGB fromPacked(uint32_t packed) {
        return RGB(
            (packed >> 16) & 0xFF,
            (packed >> 8) & 0xFF,
            packed & 0xFF
        );
    }
    
    uint32_t toPacked() const {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    
    // Linear interpolation
    static RGB lerp(const RGB& a, const RGB& b, float t) {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return RGB(
            (uint8_t)(a.r + (b.r - a.r) * t),
            (uint8_t)(a.g + (b.g - a.g) * t),
            (uint8_t)(a.b + (b.b - a.b) * t)
        );
    }
    
    // Brightness adjustment (0.0 - 1.0+)
    RGB brightness(float factor) const {
        return RGB(
            (uint8_t)std::min(255.0f, r * factor),
            (uint8_t)std::min(255.0f, g * factor),
            (uint8_t)std::min(255.0f, b * factor)
        );
    }
    
    // Common colors
    static constexpr RGB Black()   { return RGB(0, 0, 0); }
    static constexpr RGB White()   { return RGB(255, 255, 255); }
    static constexpr RGB Red()     { return RGB(255, 0, 0); }
    static constexpr RGB Green()   { return RGB(0, 255, 0); }
    static constexpr RGB Blue()    { return RGB(0, 0, 255); }
    static constexpr RGB Yellow()  { return RGB(255, 255, 0); }
    static constexpr RGB Cyan()    { return RGB(0, 255, 255); }
    static constexpr RGB Magenta() { return RGB(255, 0, 255); }
    static constexpr RGB Orange()  { return RGB(255, 128, 0); }
    static constexpr RGB Purple()  { return RGB(128, 0, 255); }
};

// ============================================================
// RGB Float (0.0 - 1.0 per channel)
// ============================================================

struct RGBf {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    
    constexpr RGBf() = default;
    constexpr RGBf(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}
    
    RGBf(const RGB& c) : r(c.r/255.0f), g(c.g/255.0f), b(c.b/255.0f) {}
    
    RGB toRGB() const {
        return RGB(
            (uint8_t)(std::min(1.0f, std::max(0.0f, r)) * 255),
            (uint8_t)(std::min(1.0f, std::max(0.0f, g)) * 255),
            (uint8_t)(std::min(1.0f, std::max(0.0f, b)) * 255)
        );
    }
    
    RGBf operator+(const RGBf& o) const { return {r+o.r, g+o.g, b+o.b}; }
    RGBf operator-(const RGBf& o) const { return {r-o.r, g-o.g, b-o.b}; }
    RGBf operator*(float s) const { return {r*s, g*s, b*s}; }
    RGBf operator*(const RGBf& o) const { return {r*o.r, g*o.g, b*o.b}; }
    
    static RGBf lerp(const RGBf& a, const RGBf& b, float t) {
        return {a.r + (b.r-a.r)*t, a.g + (b.g-a.g)*t, a.b + (b.b-a.b)*t};
    }
};

// ============================================================
// HSV Color (Hue 0-360, Sat/Val 0-1)
// ============================================================

struct HSV {
    float h = 0.0f;  // Hue: 0-360 degrees
    float s = 0.0f;  // Saturation: 0-1
    float v = 0.0f;  // Value: 0-1
    
    constexpr HSV() = default;
    constexpr HSV(float h_, float s_, float v_) : h(h_), s(s_), v(v_) {}
    
    // Convert to RGB
    RGB toRGB() const {
        float h_norm = fmodf(h, 360.0f);
        if (h_norm < 0) h_norm += 360.0f;
        
        float c = v * s;
        float x = c * (1.0f - fabsf(fmodf(h_norm / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        
        float r1, g1, b1;
        if (h_norm < 60)       { r1 = c; g1 = x; b1 = 0; }
        else if (h_norm < 120) { r1 = x; g1 = c; b1 = 0; }
        else if (h_norm < 180) { r1 = 0; g1 = c; b1 = x; }
        else if (h_norm < 240) { r1 = 0; g1 = x; b1 = c; }
        else if (h_norm < 300) { r1 = x; g1 = 0; b1 = c; }
        else                   { r1 = c; g1 = 0; b1 = x; }
        
        return RGB(
            (uint8_t)((r1 + m) * 255),
            (uint8_t)((g1 + m) * 255),
            (uint8_t)((b1 + m) * 255)
        );
    }
    
    // Create from RGB
    static HSV fromRGB(const RGB& rgb) {
        float r = rgb.r / 255.0f;
        float g = rgb.g / 255.0f;
        float b = rgb.b / 255.0f;
        
        float cmax = std::max({r, g, b});
        float cmin = std::min({r, g, b});
        float delta = cmax - cmin;
        
        HSV result;
        result.v = cmax;
        result.s = cmax > 0 ? delta / cmax : 0;
        
        if (delta < 0.0001f) {
            result.h = 0;
        } else if (cmax == r) {
            result.h = 60.0f * fmodf((g - b) / delta, 6.0f);
        } else if (cmax == g) {
            result.h = 60.0f * ((b - r) / delta + 2.0f);
        } else {
            result.h = 60.0f * ((r - g) / delta + 4.0f);
        }
        
        if (result.h < 0) result.h += 360.0f;
        return result;
    }
    
    // Hue rotation
    HSV rotateHue(float degrees) const {
        return HSV(fmodf(h + degrees + 360.0f, 360.0f), s, v);
    }
    
    // Linear interpolation (shortest path for hue)
    static HSV lerp(const HSV& a, const HSV& b, float t) {
        float h1 = a.h, h2 = b.h;
        
        // Find shortest path around the hue circle
        float diff = h2 - h1;
        if (diff > 180.0f) h1 += 360.0f;
        else if (diff < -180.0f) h2 += 360.0f;
        
        return HSV(
            fmodf(h1 + (h2 - h1) * t + 360.0f, 360.0f),
            a.s + (b.s - a.s) * t,
            a.v + (b.v - a.v) * t
        );
    }
};

// ============================================================
// Color Palette
// ============================================================

class ColorPalette {
public:
    static constexpr int MAX_COLORS = 16;
    
    ColorPalette() = default;
    
    // Add color to palette
    ColorPalette& add(const RGB& color) {
        if (count_ < MAX_COLORS) {
            colors_[count_++] = color;
        }
        return *this;
    }
    
    // Get color at index (wraps)
    RGB at(int index) const {
        if (count_ == 0) return RGB::Black();
        return colors_[index % count_];
    }
    
    // Sample palette at position (0.0 - 1.0)
    RGB sample(float t) const {
        if (count_ == 0) return RGB::Black();
        if (count_ == 1) return colors_[0];
        
        t = fmodf(t, 1.0f);
        if (t < 0) t += 1.0f;
        
        float pos = t * (count_ - 1);
        int idx = (int)pos;
        float frac = pos - idx;
        
        return RGB::lerp(colors_[idx], colors_[(idx + 1) % count_], frac);
    }
    
    int count() const { return count_; }
    
    // Preset palettes
    static ColorPalette Rainbow() {
        ColorPalette p;
        for (int i = 0; i < 12; i++) {
            p.add(HSV(i * 30.0f, 1.0f, 1.0f).toRGB());
        }
        return p;
    }
    
    static ColorPalette Fire() {
        return ColorPalette()
            .add(RGB(0, 0, 0))
            .add(RGB(128, 0, 0))
            .add(RGB(255, 64, 0))
            .add(RGB(255, 128, 0))
            .add(RGB(255, 200, 100))
            .add(RGB(255, 255, 200));
    }
    
    static ColorPalette Ocean() {
        return ColorPalette()
            .add(RGB(0, 0, 32))
            .add(RGB(0, 32, 128))
            .add(RGB(0, 128, 200))
            .add(RGB(64, 200, 255))
            .add(RGB(200, 255, 255));
    }
    
    static ColorPalette Forest() {
        return ColorPalette()
            .add(RGB(0, 32, 0))
            .add(RGB(32, 64, 16))
            .add(RGB(64, 128, 32))
            .add(RGB(128, 200, 64))
            .add(RGB(200, 255, 128));
    }

private:
    RGB colors_[MAX_COLORS];
    int count_ = 0;
};

// ============================================================
// Color Blending
// ============================================================

namespace ColorBlend {
    
    inline RGB add(const RGB& a, const RGB& b) {
        return RGB(
            std::min(255, (int)a.r + b.r),
            std::min(255, (int)a.g + b.g),
            std::min(255, (int)a.b + b.b)
        );
    }
    
    inline RGB multiply(const RGB& a, const RGB& b) {
        return RGB(
            (uint8_t)((a.r * b.r) / 255),
            (uint8_t)((a.g * b.g) / 255),
            (uint8_t)((a.b * b.b) / 255)
        );
    }
    
    inline RGB screen(const RGB& a, const RGB& b) {
        return RGB(
            255 - (((255 - a.r) * (255 - b.r)) / 255),
            255 - (((255 - a.g) * (255 - b.g)) / 255),
            255 - (((255 - a.b) * (255 - b.b)) / 255)
        );
    }
    
    inline RGB overlay(const RGB& a, const RGB& b) {
        auto overlayChannel = [](uint8_t base, uint8_t blend) -> uint8_t {
            if (base < 128) {
                return (uint8_t)((2 * base * blend) / 255);
            } else {
                return (uint8_t)(255 - ((2 * (255 - base) * (255 - blend)) / 255));
            }
        };
        return RGB(
            overlayChannel(a.r, b.r),
            overlayChannel(a.g, b.g),
            overlayChannel(a.b, b.b)
        );
    }
    
    inline RGB alpha(const RGB& bg, const RGB& fg, uint8_t alpha) {
        float a = alpha / 255.0f;
        return RGB(
            (uint8_t)(bg.r + (fg.r - bg.r) * a),
            (uint8_t)(bg.g + (fg.g - bg.g) * a),
            (uint8_t)(bg.b + (fg.b - bg.b) * a)
        );
    }
    
} // namespace ColorBlend

} // namespace AnimationDriver
