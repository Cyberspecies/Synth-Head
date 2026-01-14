/*****************************************************************
 * @file BuiltinShaders.hpp
 * @brief Collection of ready-to-use shader implementations
 * 
 * Includes solid color, gradients, rainbow, patterns, and effects.
 *****************************************************************/

#pragma once

#include "ShaderBase.hpp"
#include "../Core/Easing.hpp"
#include <cmath>

namespace AnimationDriver {

// ============================================================
// Solid Color Shader
// ============================================================

class SolidShader : public ShaderBase {
public:
    SolidShader(const RGB& color = RGB::White()) : color_(color) {
        setType(ShaderType::SOLID);
        setName("Solid");
    }
    
    SolidShader& setColor(const RGB& color) {
        color_.set(color);
        return *this;
    }
    
    SolidShader& setColor(const HSV& hsv) {
        color_.set(hsv.toRGB());
        return *this;
    }
    
    SolidShader& cycleHue(float speed = 1.0f, float saturation = 1.0f, float value = 1.0f) {
        color_.cycleHue(speed, saturation, value);
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        return color_.get(ctx.time);
    }
    
private:
    ColorParam color_;
};

// ============================================================
// Horizontal Gradient Shader
// ============================================================

class GradientHShader : public ShaderBase {
public:
    GradientHShader(const RGB& start = RGB::Black(), const RGB& end = RGB::White()) 
        : start_(start), end_(end), easing_(EasingType::LINEAR) {
        setType(ShaderType::GRADIENT_H);
        setName("GradientH");
    }
    
    GradientHShader& setColors(const RGB& start, const RGB& end) {
        start_.set(start);
        end_.set(end);
        return *this;
    }
    
    GradientHShader& setEasing(EasingType easing) {
        easing_ = easing;
        return *this;
    }
    
    GradientHShader& setOffset(float offset) {
        offset_ = offset;
        return *this;
    }
    
    GradientHShader& animate(float speed) {
        animSpeed_ = speed;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        float t = ctx.x + offset_ + ctx.time * animSpeed_;
        t = fmodf(t, 1.0f);
        if (t < 0) t += 1.0f;
        t = Easing::apply(easing_, t);
        
        return ColorBlend::alpha(start_.get(ctx.time), end_.get(ctx.time), t);
    }
    
private:
    ColorParam start_;
    ColorParam end_;
    EasingType easing_;
    float offset_ = 0.0f;
    float animSpeed_ = 0.0f;
};

// ============================================================
// Vertical Gradient Shader
// ============================================================

class GradientVShader : public ShaderBase {
public:
    GradientVShader(const RGB& start = RGB::Black(), const RGB& end = RGB::White())
        : start_(start), end_(end), easing_(EasingType::LINEAR) {
        setType(ShaderType::GRADIENT_V);
        setName("GradientV");
    }
    
    GradientVShader& setColors(const RGB& start, const RGB& end) {
        start_.set(start);
        end_.set(end);
        return *this;
    }
    
    GradientVShader& setEasing(EasingType easing) {
        easing_ = easing;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        float t = Easing::apply(easing_, ctx.y);
        return ColorBlend::alpha(start_.get(ctx.time), end_.get(ctx.time), t);
    }
    
private:
    ColorParam start_;
    ColorParam end_;
    EasingType easing_;
};

// ============================================================
// Radial Gradient Shader
// ============================================================

class GradientRadialShader : public ShaderBase {
public:
    GradientRadialShader(const RGB& center = RGB::White(), const RGB& edge = RGB::Black())
        : center_(center), edge_(edge), centerX_(0.5f), centerY_(0.5f), radius_(0.5f) {
        setType(ShaderType::GRADIENT_RADIAL);
        setName("GradientRadial");
    }
    
    GradientRadialShader& setColors(const RGB& center, const RGB& edge) {
        center_.set(center);
        edge_.set(edge);
        return *this;
    }
    
    GradientRadialShader& setCenter(float x, float y) {
        centerX_ = x;
        centerY_ = y;
        return *this;
    }
    
    GradientRadialShader& setRadius(float radius) {
        radius_ = radius;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        float dx = ctx.x - centerX_;
        float dy = ctx.y - centerY_;
        float dist = sqrtf(dx * dx + dy * dy);
        float t = (radius_ > 0) ? fminf(dist / radius_, 1.0f) : 0.0f;
        
        return ColorBlend::alpha(center_.get(ctx.time), edge_.get(ctx.time), t);
    }
    
private:
    ColorParam center_;
    ColorParam edge_;
    float centerX_;
    float centerY_;
    float radius_;
};

// ============================================================
// Horizontal Rainbow Shader
// ============================================================

class RainbowHShader : public ShaderBase {
public:
    RainbowHShader() : speed_(1.0f), saturation_(1.0f), value_(1.0f), scale_(1.0f) {
        setType(ShaderType::RAINBOW_H);
        setName("RainbowH");
    }
    
    RainbowHShader& setSpeed(float speed) {
        speed_ = speed;
        return *this;
    }
    
    RainbowHShader& setScale(float scale) {
        scale_ = scale;
        return *this;
    }
    
    RainbowHShader& setSaturation(float sat) {
        saturation_ = sat;
        return *this;
    }
    
    RainbowHShader& setValue(float val) {
        value_ = val;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        float hue = fmodf((ctx.x * scale_ + ctx.time * speed_) * 360.0f, 360.0f);
        if (hue < 0) hue += 360.0f;
        return HSV(hue, saturation_, value_).toRGB();
    }
    
private:
    float speed_;
    float saturation_;
    float value_;
    float scale_;
};

// ============================================================
// Vertical Rainbow Shader
// ============================================================

class RainbowVShader : public ShaderBase {
public:
    RainbowVShader() : speed_(1.0f), saturation_(1.0f), value_(1.0f), scale_(1.0f) {
        setType(ShaderType::RAINBOW_V);
        setName("RainbowV");
    }
    
    RainbowVShader& setSpeed(float speed) {
        speed_ = speed;
        return *this;
    }
    
    RainbowVShader& setScale(float scale) {
        scale_ = scale;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        float hue = fmodf((ctx.y * scale_ + ctx.time * speed_) * 360.0f, 360.0f);
        if (hue < 0) hue += 360.0f;
        return HSV(hue, saturation_, value_).toRGB();
    }
    
private:
    float speed_;
    float saturation_;
    float value_;
    float scale_;
};

// ============================================================
// Palette-Based Shader
// ============================================================

class PaletteShader : public ShaderBase {
public:
    enum class Direction { HORIZONTAL, VERTICAL, DIAGONAL, RADIAL };
    
    PaletteShader() : direction_(Direction::HORIZONTAL), speed_(0.0f), scale_(1.0f) {
        setName("Palette");
        palette_ = ColorPalette::Rainbow();
    }
    
    PaletteShader& setPalette(const ColorPalette& palette) {
        palette_ = palette;
        return *this;
    }
    
    PaletteShader& setDirection(Direction dir) {
        direction_ = dir;
        return *this;
    }
    
    PaletteShader& setSpeed(float speed) {
        speed_ = speed;
        return *this;
    }
    
    PaletteShader& setScale(float scale) {
        scale_ = scale;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        float t = 0.0f;
        
        switch (direction_) {
            case Direction::HORIZONTAL:
                t = ctx.x;
                break;
            case Direction::VERTICAL:
                t = ctx.y;
                break;
            case Direction::DIAGONAL:
                t = (ctx.x + ctx.y) * 0.5f;
                break;
            case Direction::RADIAL:
                t = sqrtf((ctx.x - 0.5f) * (ctx.x - 0.5f) + (ctx.y - 0.5f) * (ctx.y - 0.5f)) * 2.0f;
                break;
        }
        
        t = fmodf((t * scale_ + ctx.time * speed_), 1.0f);
        if (t < 0) t += 1.0f;
        
        return palette_.sample(t);
    }
    
private:
    ColorPalette palette_;
    Direction direction_;
    float speed_;
    float scale_;
};

} // namespace AnimationDriver
