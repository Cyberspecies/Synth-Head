/*****************************************************************
 * @file ShaderBase.hpp
 * @brief Base class for all shaders in the animation system
 * 
 * Shaders generate color values for each pixel based on
 * position, time, and configurable parameters.
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include "../Core/Color.hpp"
#include "../Core/Parameter.hpp"
#include <cstring>

namespace AnimationDriver {

// ============================================================
// Shader Context - passed to shader render function
// ============================================================

struct ShaderContext {
    // Current pixel position (0-1 normalized)
    float x;
    float y;
    
    // Absolute pixel position
    int pixelX;
    int pixelY;
    
    // Display dimensions
    int width;
    int height;
    
    // Time in seconds since animation start
    float time;
    
    // Delta time since last frame
    float deltaTime;
    
    // Frame number
    uint32_t frame;
    
    // Constructor
    ShaderContext() : x(0), y(0), pixelX(0), pixelY(0), 
                      width(128), height(32), time(0), 
                      deltaTime(0.016f), frame(0) {}
    
    // Helper to set pixel position (also computes normalized)
    void setPixel(int px, int py, int w, int h) {
        pixelX = px;
        pixelY = py;
        width = w;
        height = h;
        x = (w > 1) ? static_cast<float>(px) / (w - 1) : 0.0f;
        y = (h > 1) ? static_cast<float>(py) / (h - 1) : 0.0f;
    }
};

// ============================================================
// Shader Type Identifier
// ============================================================

enum class ShaderType : uint8_t {
    CUSTOM,
    SOLID,
    GRADIENT_H,
    GRADIENT_V,
    GRADIENT_RADIAL,
    RAINBOW_H,
    RAINBOW_V,
    RAINBOW_RADIAL,
    CHECKERBOARD,
    STRIPES,
    NOISE,
    PLASMA,
    WAVE,
    FIRE,
    SPARKLE
};

// ============================================================
// Shader Base Class
// ============================================================

class ShaderBase {
public:
    ShaderBase() : type_(ShaderType::CUSTOM), enabled_(true) {
        name_[0] = '\0';
    }
    
    virtual ~ShaderBase() = default;
    
    // Main render function - must be implemented by derived shaders
    virtual RGB render(const ShaderContext& ctx) = 0;
    
    // Optional update function called once per frame
    virtual void update(float deltaTime) { (void)deltaTime; }
    
    // Optional reset function
    virtual void reset() {}
    
    // Shader identification
    ShaderType getType() const { return type_; }
    const char* getName() const { return name_; }
    
    // Enable/disable
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    
    // Opacity for blending
    void setOpacity(float opacity) { opacity_ = opacity; }
    float getOpacity() const { return opacity_; }

protected:
    void setType(ShaderType type) { type_ = type; }
    void setName(const char* name) {
        strncpy(name_, name, sizeof(name_) - 1);
        name_[sizeof(name_) - 1] = '\0';
    }
    
    ShaderType type_;
    char name_[24];
    bool enabled_;
    float opacity_ = 1.0f;
};

// ============================================================
// Shader Function Pointer Type (for lightweight shaders)
// ============================================================

using ShaderFunc = RGB(*)(const ShaderContext& ctx, void* userData);

// ============================================================
// Function-Based Shader Wrapper
// ============================================================

class FunctionShader : public ShaderBase {
public:
    FunctionShader(ShaderFunc func, void* userData = nullptr) 
        : func_(func), userData_(userData) {
        setName("FunctionShader");
    }
    
    RGB render(const ShaderContext& ctx) override {
        return func_ ? func_(ctx, userData_) : RGB::Black();
    }
    
    void setUserData(void* data) { userData_ = data; }
    void* getUserData() const { return userData_; }

private:
    ShaderFunc func_;
    void* userData_;
};

// ============================================================
// Shader Blend Helper
// ============================================================

namespace ShaderBlend {
    
inline RGB blend(const RGB& base, const RGB& overlay, BlendMode mode, float opacity = 1.0f) {
    RGB result;
    
    switch (mode) {
        case BlendMode::REPLACE:
            result = overlay;
            break;
            
        case BlendMode::ADD:
            result = ColorBlend::add(base, overlay);
            break;
            
        case BlendMode::MULTIPLY:
            result = ColorBlend::multiply(base, overlay);
            break;
            
        case BlendMode::SCREEN:
            result = ColorBlend::screen(base, overlay);
            break;
            
        case BlendMode::OVERLAY:
            result = ColorBlend::overlay(base, overlay);
            break;
            
        case BlendMode::AVERAGE:
            result = RGB(
                (base.r + overlay.r) / 2,
                (base.g + overlay.g) / 2,
                (base.b + overlay.b) / 2
            );
            break;
            
        case BlendMode::DIFFERENCE:
            result = RGB(
                static_cast<uint8_t>(abs(base.r - overlay.r)),
                static_cast<uint8_t>(abs(base.g - overlay.g)),
                static_cast<uint8_t>(abs(base.b - overlay.b))
            );
            break;
            
        default:
            result = overlay;
            break;
    }
    
    // Apply opacity
    if (opacity < 1.0f) {
        return ColorBlend::alpha(base, result, opacity);
    }
    
    return result;
}

} // namespace ShaderBlend

} // namespace AnimationDriver
