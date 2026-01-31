/*****************************************************************
 * @file HueGradientShader.hpp
 * @brief RGB Hue Gradient Cycling Shader
 * 
 * Creates a smooth rainbow hue gradient that cycles across the display.
 * Can overlay on top of existing animations or replace the frame entirely.
 * 
 * EFFECT MODES:
 * - Overlay: Blends hue colors with existing pixels (multiplicative)
 * - Replace: Completely replaces frame with gradient
 * - Additive: Adds gradient colors to existing pixels
 * 
 * PARAMETERS:
 * - cycle_speed: Speed of hue cycling (degrees per second, 0-360)
 * - saturation: Color saturation (0-1, 0=grayscale, 1=vivid)
 * - brightness: Color brightness (0-1)
 * - gradient_scale: How many hue cycles fit across display (0.5-4)
 * - direction: Gradient direction (0=horizontal, 1=vertical, 2=diagonal, 3=radial)
 * - blend_mode: How to blend with existing frame (0=overlay, 1=replace, 2=additive)
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "../Core/ShaderBase.hpp"
#include "../Core/ShaderRegistry.hpp"
#include <cmath>
#include <algorithm>

namespace AnimationSystem {

/**
 * @brief RGB Hue Gradient Shader - Creates animated rainbow gradients
 * 
 * This shader generates a smooth HSV-based rainbow gradient that
 * cycles over time. It can be applied as an overlay, replacement,
 * or additive blend to the current animation frame.
 */
class HueGradientShader : public ShaderBase {
public:
    HueGradientShader() {
        // ============================================================
        // PARAMETER DEFINITIONS
        // ============================================================
        
        // --- Animation Speed ---
        defineParam("cycle_speed", "Cycle Speed", 
                    "Hue cycling speed in degrees per second (0-360)",
                    ParamType::FLOAT, 45.0f, 0.0f, 360.0f, "Animation");
        
        // --- Color Properties ---
        defineParam("saturation", "Saturation", 
                    "Color saturation (0=grayscale, 1=vivid)",
                    ParamType::FLOAT, 1.0f, 0.0f, 1.0f, "Color");
        
        defineParam("brightness", "Brightness", 
                    "Color brightness/value",
                    ParamType::FLOAT, 1.0f, 0.0f, 1.0f, "Color");
        
        // --- Gradient Shape ---
        defineParam("gradient_scale", "Gradient Scale", 
                    "Number of hue cycles across display (0.5=wide, 4=dense)",
                    ParamType::FLOAT, 1.0f, 0.25f, 4.0f, "Gradient");
        
        defineParam("direction", "Direction", 
                    "Gradient direction (0=horizontal, 1=vertical, 2=diagonal, 3=radial)",
                    ParamType::INT, 0.0f, 0.0f, 3.0f, "Gradient");
        
        // --- Blending ---
        defineParam("blend_mode", "Blend Mode", 
                    "How to blend with frame (0=overlay, 1=replace, 2=additive)",
                    ParamType::INT, 1.0f, 0.0f, 2.0f, "Blending");
        
        defineParam("blend_amount", "Blend Amount", 
                    "Blend strength (0=source only, 1=full effect)",
                    ParamType::FLOAT, 1.0f, 0.0f, 1.0f, "Blending");
    }
    
    // ============================================================
    // SHADER METADATA
    // ============================================================
    
    const char* getTypeId() const override { return "hue_gradient"; }
    const char* getDisplayName() const override { return "Hue Gradient"; }
    const char* getDescription() const override { 
        return "Animated rainbow hue gradient cycling effect";
    }
    
    // ============================================================
    // SHADER LOGIC
    // ============================================================
    
    void update(uint32_t deltaMs) override {
        if (!isEnabled()) return;
        
        time_ += deltaMs;
        
        // Update hue offset based on cycle speed
        float speed = getParam("cycle_speed");
        hueOffset_ += (speed * deltaMs) / 1000.0f;
        
        // Keep hue in 0-360 range
        while (hueOffset_ >= 360.0f) {
            hueOffset_ -= 360.0f;
        }
        while (hueOffset_ < 0.0f) {
            hueOffset_ += 360.0f;
        }
    }
    
    void apply() override {
        if (!isEnabled() || !drawPixel) return;
        
        // Get parameters
        float saturation = getParam("saturation");
        float brightness = getParam("brightness") * getIntensity();
        float gradientScale = getParam("gradient_scale");
        int direction = getParamInt("direction");
        int blendMode = getParamInt("blend_mode");
        float blendAmount = getParam("blend_amount");
        
        // Display dimensions
        const float centerX = DISPLAY_W / 2.0f;
        const float centerY = DISPLAY_H / 2.0f;
        const float maxDist = std::sqrt(centerX * centerX + centerY * centerY);
        
        // Process each pixel
        for (int y = 0; y < DISPLAY_H; y++) {
            for (int x = 0; x < DISPLAY_W; x++) {
                // Calculate position factor (0-1) based on direction
                float posFactor = 0.0f;
                
                switch (direction) {
                    case 0: // Horizontal
                        posFactor = static_cast<float>(x) / static_cast<float>(DISPLAY_W);
                        break;
                        
                    case 1: // Vertical
                        posFactor = static_cast<float>(y) / static_cast<float>(DISPLAY_H);
                        break;
                        
                    case 2: // Diagonal
                        posFactor = (static_cast<float>(x) / DISPLAY_W + 
                                    static_cast<float>(y) / DISPLAY_H) * 0.5f;
                        break;
                        
                    case 3: // Radial (from center)
                        {
                            float dx = x - centerX;
                            float dy = y - centerY;
                            float dist = std::sqrt(dx * dx + dy * dy);
                            posFactor = dist / maxDist;
                        }
                        break;
                        
                    default:
                        posFactor = static_cast<float>(x) / static_cast<float>(DISPLAY_W);
                        break;
                }
                
                // Calculate hue for this pixel
                float hue = hueOffset_ + (posFactor * 360.0f * gradientScale);
                while (hue >= 360.0f) hue -= 360.0f;
                while (hue < 0.0f) hue += 360.0f;
                
                // Convert HSV to RGB
                uint8_t gradR, gradG, gradB;
                hsvToRgb(hue, saturation, brightness, gradR, gradG, gradB);
                
                // Get source pixel
                uint8_t srcR, srcG, srcB;
                getSourcePixel(x, y, srcR, srcG, srcB);
                
                // Apply blend mode
                uint8_t outR, outG, outB;
                
                switch (blendMode) {
                    case 0: // Overlay (multiply)
                        outR = blendMultiply(srcR, gradR, blendAmount);
                        outG = blendMultiply(srcG, gradG, blendAmount);
                        outB = blendMultiply(srcB, gradB, blendAmount);
                        break;
                        
                    case 1: // Replace
                        outR = blendLerp(srcR, gradR, blendAmount);
                        outG = blendLerp(srcG, gradG, blendAmount);
                        outB = blendLerp(srcB, gradB, blendAmount);
                        break;
                        
                    case 2: // Additive
                        outR = blendAdd(srcR, gradR, blendAmount);
                        outG = blendAdd(srcG, gradG, blendAmount);
                        outB = blendAdd(srcB, gradB, blendAmount);
                        break;
                        
                    default:
                        outR = gradR;
                        outG = gradG;
                        outB = gradB;
                        break;
                }
                
                // Draw final pixel
                drawPixel(x, y, outR, outG, outB);
            }
        }
    }
    
    void reset() override {
        ShaderBase::reset();
        hueOffset_ = 0.0f;
    }
    
private:
    float hueOffset_ = 0.0f;
    
    // ============================================================
    // COLOR CONVERSION
    // ============================================================
    
    /**
     * @brief Convert HSV to RGB
     * @param h Hue (0-360)
     * @param s Saturation (0-1)
     * @param v Value/Brightness (0-1)
     * @param r Output red (0-255)
     * @param g Output green (0-255)
     * @param b Output blue (0-255)
     */
    static void hsvToRgb(float h, float s, float v, 
                         uint8_t& r, uint8_t& g, uint8_t& b) {
        // Normalize hue to 0-360
        while (h >= 360.0f) h -= 360.0f;
        while (h < 0.0f) h += 360.0f;
        
        // Clamp s and v
        s = std::max(0.0f, std::min(1.0f, s));
        v = std::max(0.0f, std::min(1.0f, v));
        
        float c = v * s;  // Chroma
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        
        float rf, gf, bf;
        
        if (h < 60.0f) {
            rf = c; gf = x; bf = 0;
        } else if (h < 120.0f) {
            rf = x; gf = c; bf = 0;
        } else if (h < 180.0f) {
            rf = 0; gf = c; bf = x;
        } else if (h < 240.0f) {
            rf = 0; gf = x; bf = c;
        } else if (h < 300.0f) {
            rf = x; gf = 0; bf = c;
        } else {
            rf = c; gf = 0; bf = x;
        }
        
        r = static_cast<uint8_t>((rf + m) * 255.0f);
        g = static_cast<uint8_t>((gf + m) * 255.0f);
        b = static_cast<uint8_t>((bf + m) * 255.0f);
    }
    
    // ============================================================
    // BLENDING HELPERS
    // ============================================================
    
    /**
     * @brief Linear interpolation blend
     */
    static uint8_t blendLerp(uint8_t src, uint8_t dst, float amount) {
        float result = src * (1.0f - amount) + dst * amount;
        return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, result)));
    }
    
    /**
     * @brief Multiplicative blend
     */
    static uint8_t blendMultiply(uint8_t src, uint8_t grad, float amount) {
        float srcNorm = src / 255.0f;
        float gradNorm = grad / 255.0f;
        float multiplied = srcNorm * gradNorm;
        float result = (srcNorm * (1.0f - amount) + multiplied * amount) * 255.0f;
        return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, result)));
    }
    
    /**
     * @brief Additive blend (clamped)
     */
    static uint8_t blendAdd(uint8_t src, uint8_t grad, float amount) {
        float result = src + (grad * amount);
        return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, result)));
    }
};

// Auto-register the shader
REGISTER_SHADER(HueGradientShader)

} // namespace AnimationSystem
