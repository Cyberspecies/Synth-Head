/*****************************************************************
 * @file PatternShaders.hpp
 * @brief Pattern-based shaders: checkerboard, stripes, noise, etc.
 *****************************************************************/

#pragma once

#include "ShaderBase.hpp"
#include <cmath>
#include <cstdlib>

namespace AnimationDriver {

// ============================================================
// Checkerboard Pattern Shader
// ============================================================

class CheckerboardShader : public ShaderBase {
public:
    CheckerboardShader() : color1_(RGB::Black()), color2_(RGB::White()), sizeX_(8), sizeY_(8) {
        setType(ShaderType::CHECKERBOARD);
        setName("Checkerboard");
    }
    
    CheckerboardShader& setColors(const RGB& color1, const RGB& color2) {
        color1_.set(color1);
        color2_.set(color2);
        return *this;
    }
    
    CheckerboardShader& setSize(int size) {
        sizeX_ = sizeY_ = size;
        return *this;
    }
    
    CheckerboardShader& setSize(int sizeX, int sizeY) {
        sizeX_ = sizeX;
        sizeY_ = sizeY;
        return *this;
    }
    
    CheckerboardShader& animate(float speed) {
        animSpeed_ = speed;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        int offset = static_cast<int>(ctx.time * animSpeed_);
        int cellX = (ctx.pixelX + offset) / sizeX_;
        int cellY = ctx.pixelY / sizeY_;
        bool isEven = ((cellX + cellY) % 2) == 0;
        
        return isEven ? color1_.get(ctx.time) : color2_.get(ctx.time);
    }
    
private:
    ColorParam color1_;
    ColorParam color2_;
    int sizeX_;
    int sizeY_;
    float animSpeed_ = 0.0f;
};

// ============================================================
// Stripes Pattern Shader
// ============================================================

class StripesShader : public ShaderBase {
public:
    enum class Orientation { HORIZONTAL, VERTICAL, DIAGONAL_LEFT, DIAGONAL_RIGHT };
    
    StripesShader() : color1_(RGB::Black()), color2_(RGB::White()), stripeWidth_(4), 
                      orientation_(Orientation::VERTICAL) {
        setType(ShaderType::STRIPES);
        setName("Stripes");
    }
    
    StripesShader& setColors(const RGB& color1, const RGB& color2) {
        color1_.set(color1);
        color2_.set(color2);
        return *this;
    }
    
    StripesShader& setWidth(int width) {
        stripeWidth_ = width;
        return *this;
    }
    
    StripesShader& setOrientation(Orientation orient) {
        orientation_ = orient;
        return *this;
    }
    
    StripesShader& animate(float speed) {
        animSpeed_ = speed;
        return *this;
    }
    
    StripesShader& setSoftness(float softness) {
        softness_ = softness;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        int pos = 0;
        int offset = static_cast<int>(ctx.time * animSpeed_);
        
        switch (orientation_) {
            case Orientation::HORIZONTAL:
                pos = ctx.pixelY + offset;
                break;
            case Orientation::VERTICAL:
                pos = ctx.pixelX + offset;
                break;
            case Orientation::DIAGONAL_LEFT:
                pos = ctx.pixelX + ctx.pixelY + offset;
                break;
            case Orientation::DIAGONAL_RIGHT:
                pos = ctx.pixelX - ctx.pixelY + offset;
                break;
        }
        
        if (softness_ > 0.0f) {
            // Soft stripes with gradient
            float t = static_cast<float>(pos % (stripeWidth_ * 2)) / (stripeWidth_ * 2);
            t = sinf(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
            return ColorBlend::alpha(color1_.get(ctx.time), color2_.get(ctx.time), t);
        } else {
            // Hard stripes
            int stripe = (pos / stripeWidth_) % 2;
            return (stripe == 0) ? color1_.get(ctx.time) : color2_.get(ctx.time);
        }
    }
    
private:
    ColorParam color1_;
    ColorParam color2_;
    int stripeWidth_;
    Orientation orientation_;
    float animSpeed_ = 0.0f;
    float softness_ = 0.0f;
};

// ============================================================
// Simple Noise Shader
// ============================================================

class NoiseShader : public ShaderBase {
public:
    NoiseShader() : scale_(1.0f), speed_(0.0f), monochrome_(false) {
        setType(ShaderType::NOISE);
        setName("Noise");
    }
    
    NoiseShader& setScale(float scale) {
        scale_ = scale;
        return *this;
    }
    
    NoiseShader& setSpeed(float speed) {
        speed_ = speed;
        return *this;
    }
    
    NoiseShader& setMonochrome(bool mono) {
        monochrome_ = mono;
        return *this;
    }
    
    NoiseShader& setColors(const RGB& base, const RGB& peak) {
        baseColor_.set(base);
        peakColor_.set(peak);
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        // Simple hash-based noise
        uint32_t seed = hash(ctx.pixelX, ctx.pixelY, static_cast<int>(ctx.time * speed_ * 10));
        
        if (monochrome_) {
            float noise = (seed % 256) / 255.0f;
            return ColorBlend::alpha(baseColor_.get(ctx.time), peakColor_.get(ctx.time), noise);
        } else {
            uint8_t r = (seed >> 0) & 0xFF;
            uint8_t g = (seed >> 8) & 0xFF;
            uint8_t b = (seed >> 16) & 0xFF;
            return RGB(r, g, b);
        }
    }
    
private:
    uint32_t hash(int x, int y, int t) const {
        uint32_t h = x * 374761393u + y * 668265263u + t * 1274126177u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }
    
    float scale_;
    float speed_;
    bool monochrome_;
    ColorParam baseColor_{RGB::Black()};
    ColorParam peakColor_{RGB::White()};
};

// ============================================================
// Wave Pattern Shader
// ============================================================

class WaveShader : public ShaderBase {
public:
    WaveShader() : amplitude_(0.3f), frequency_(2.0f), speed_(1.0f), thickness_(0.1f) {
        setType(ShaderType::WAVE);
        setName("Wave");
    }
    
    WaveShader& setWaveColor(const RGB& color) {
        waveColor_.set(color);
        return *this;
    }
    
    WaveShader& setBackgroundColor(const RGB& color) {
        bgColor_.set(color);
        return *this;
    }
    
    WaveShader& setAmplitude(float amplitude) {
        amplitude_ = amplitude;
        return *this;
    }
    
    WaveShader& setFrequency(float frequency) {
        frequency_ = frequency;
        return *this;
    }
    
    WaveShader& setSpeed(float speed) {
        speed_ = speed;
        return *this;
    }
    
    WaveShader& setThickness(float thickness) {
        thickness_ = thickness;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        float wave = 0.5f + amplitude_ * sinf((ctx.x * frequency_ + ctx.time * speed_) * 3.14159f * 2.0f);
        float dist = fabsf(ctx.y - wave);
        
        if (dist < thickness_) {
            float alpha = 1.0f - (dist / thickness_);
            return ColorBlend::alpha(bgColor_.get(ctx.time), waveColor_.get(ctx.time), alpha);
        }
        
        return bgColor_.get(ctx.time);
    }
    
private:
    ColorParam waveColor_{RGB::White()};
    ColorParam bgColor_{RGB::Black()};
    float amplitude_;
    float frequency_;
    float speed_;
    float thickness_;
};

// ============================================================
// Plasma Effect Shader
// ============================================================

class PlasmaShader : public ShaderBase {
public:
    PlasmaShader() : speed_(1.0f), scale_(1.0f), saturation_(1.0f), value_(1.0f) {
        setType(ShaderType::PLASMA);
        setName("Plasma");
    }
    
    PlasmaShader& setSpeed(float speed) {
        speed_ = speed;
        return *this;
    }
    
    PlasmaShader& setScale(float scale) {
        scale_ = scale;
        return *this;
    }
    
    PlasmaShader& setSaturation(float sat) {
        saturation_ = sat;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        float t = ctx.time * speed_;
        float x = ctx.x * scale_ * 10.0f;
        float y = ctx.y * scale_ * 10.0f;
        
        // Classic plasma formula
        float v1 = sinf(x + t);
        float v2 = sinf(y + t);
        float v3 = sinf(x + y + t);
        float v4 = sinf(sqrtf(x * x + y * y) + t);
        
        float v = (v1 + v2 + v3 + v4) / 4.0f;
        float hue = fmodf((v + 1.0f) * 180.0f, 360.0f);
        
        return HSV(hue, saturation_, value_).toRGB();
    }
    
private:
    float speed_;
    float scale_;
    float saturation_;
    float value_;
};

// ============================================================
// Sparkle/Twinkle Effect Shader
// ============================================================

class SparkleShader : public ShaderBase {
public:
    SparkleShader() : density_(0.05f), speed_(2.0f), baseColor_(RGB::Black()), sparkleColor_(RGB::White()) {
        setType(ShaderType::SPARKLE);
        setName("Sparkle");
    }
    
    SparkleShader& setDensity(float density) {
        density_ = density;
        return *this;
    }
    
    SparkleShader& setSpeed(float speed) {
        speed_ = speed;
        return *this;
    }
    
    SparkleShader& setBaseColor(const RGB& color) {
        baseColor_.set(color);
        return *this;
    }
    
    SparkleShader& setSparkleColor(const RGB& color) {
        sparkleColor_.set(color);
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        // Create deterministic but varying sparkle pattern
        uint32_t h = hash(ctx.pixelX, ctx.pixelY);
        float threshold = static_cast<float>(h & 0xFFFF) / 65535.0f;
        
        if (threshold < density_) {
            // This pixel can sparkle
            float phase = static_cast<float>((h >> 16) & 0xFFFF) / 65535.0f;
            float brightness = sinf((ctx.time * speed_ + phase * 6.28f)) * 0.5f + 0.5f;
            
            return ColorBlend::alpha(baseColor_.get(ctx.time), sparkleColor_.get(ctx.time), brightness);
        }
        
        return baseColor_.get(ctx.time);
    }
    
private:
    uint32_t hash(int x, int y) const {
        uint32_t h = x * 374761393u + y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }
    
    float density_;
    float speed_;
    ColorParam baseColor_;
    ColorParam sparkleColor_;
};

} // namespace AnimationDriver
