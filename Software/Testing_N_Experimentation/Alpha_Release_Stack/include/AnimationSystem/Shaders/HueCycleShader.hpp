/*****************************************************************
 * @file HueCycleShader.hpp
 * @brief RGB hue cycling post-processing effect
 * 
 * Applies a cycling hue shift to the entire frame, creating a
 * rainbow color cycling effect over time.
 * 
 * PARAMETERS:
 * - speed: How fast the hue cycles (degrees per second)
 * - saturation: Color saturation (0=grayscale, 1=full color)
 * - brightness: Overall brightness multiplier
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "../Core/ShaderBase.hpp"
#include "../Core/ShaderRegistry.hpp"
#include <cmath>

namespace AnimationSystem {

class HueCycleShader : public ShaderBase {
public:
    HueCycleShader() {
        // Define parameters
        defineParam("speed", "Cycle Speed", "Hue rotation speed in degrees per second",
                    ParamType::FLOAT, 45.0f, 1.0f, 360.0f, "Animation");
        
        defineParam("saturation", "Saturation", "Color saturation (0=grayscale, 1=full)",
                    ParamType::FLOAT, 1.0f, 0.0f, 1.0f, "Color");
        
        defineParam("brightness", "Brightness", "Overall brightness multiplier",
                    ParamType::FLOAT, 1.0f, 0.1f, 2.0f, "Color");
    }
    
    const char* getTypeId() const override { return "hue_cycle"; }
    const char* getDisplayName() const override { return "Hue Cycle"; }
    const char* getDescription() const override { return "RGB hue cycling effect"; }
    
    void update(uint32_t deltaMs) override {
        if (!isEnabled()) return;
        
        float speed = getParam("speed");
        // Update hue offset based on speed (degrees per second)
        hueOffset_ += (speed * deltaMs) / 1000.0f;
        
        // Wrap around at 360 degrees
        while (hueOffset_ >= 360.0f) {
            hueOffset_ -= 360.0f;
        }
    }
    
    void apply() override {
        if (!isEnabled()) return;
        
        float satMult = getParam("saturation") * getIntensity();
        float brightness = getParam("brightness");
        
        for (int y = 0; y < DISPLAY_H; y++) {
            for (int x = 0; x < DISPLAY_W; x++) {
                uint8_t r, g, b;
                getSourcePixel(x, y, r, g, b);
                
                // Skip black pixels (optimization)
                if (r == 0 && g == 0 && b == 0) {
                    if (drawPixel) {
                        drawPixel(x, y, 0, 0, 0);
                    }
                    continue;
                }
                
                // Convert RGB to HSL
                float h, s, l;
                rgbToHsl(r, g, b, h, s, l);
                
                // Apply hue shift
                h += hueOffset_;
                while (h >= 360.0f) h -= 360.0f;
                
                // Apply saturation multiplier
                s *= satMult;
                if (s > 1.0f) s = 1.0f;
                
                // Apply brightness
                l *= brightness;
                if (l > 1.0f) l = 1.0f;
                
                // Convert back to RGB
                uint8_t newR, newG, newB;
                hslToRgb(h, s, l, newR, newG, newB);
                
                if (drawPixel) {
                    drawPixel(x, y, newR, newG, newB);
                }
            }
        }
    }
    
    void reset() override {
        ShaderBase::reset();
        hueOffset_ = 0;
    }
    
private:
    float hueOffset_ = 0;
    void rgbToHsl(uint8_t r, uint8_t g, uint8_t b, float& h, float& s, float& l) {
        float rf = r / 255.0f;
        float gf = g / 255.0f;
        float bf = b / 255.0f;
        
        float maxVal = fmax(fmax(rf, gf), bf);
        float minVal = fmin(fmin(rf, gf), bf);
        float delta = maxVal - minVal;
        
        // Lightness
        l = (maxVal + minVal) / 2.0f;
        
        if (delta < 0.0001f) {
            // Achromatic (gray)
            h = 0;
            s = 0;
        } else {
            // Saturation
            s = (l > 0.5f) ? delta / (2.0f - maxVal - minVal) : delta / (maxVal + minVal);
            
            // Hue
            if (maxVal == rf) {
                h = 60.0f * fmod((gf - bf) / delta, 6.0f);
            } else if (maxVal == gf) {
                h = 60.0f * ((bf - rf) / delta + 2.0f);
            } else {
                h = 60.0f * ((rf - gf) / delta + 4.0f);
            }
            
            if (h < 0) h += 360.0f;
        }
    }
    
    // Convert HSL to RGB
    void hslToRgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (s < 0.0001f) {
            // Achromatic
            uint8_t val = static_cast<uint8_t>(l * 255.0f);
            r = g = b = val;
            return;
        }
        
        float q = (l < 0.5f) ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        
        float hk = h / 360.0f;  // Normalize to 0-1
        
        auto hueToRgb = [](float p, float q, float t) -> float {
            if (t < 0) t += 1.0f;
            if (t > 1) t -= 1.0f;
            if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
            if (t < 1.0f/2.0f) return q;
            if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
            return p;
        };
        
        float rf = hueToRgb(p, q, hk + 1.0f/3.0f);
        float gf = hueToRgb(p, q, hk);
        float bf = hueToRgb(p, q, hk - 1.0f/3.0f);
        
        r = static_cast<uint8_t>(rf * 255.0f);
        g = static_cast<uint8_t>(gf * 255.0f);
        b = static_cast<uint8_t>(bf * 255.0f);
    }
};

// Register the shader
REGISTER_SHADER(HueCycleShader);

} // namespace AnimationSystem
