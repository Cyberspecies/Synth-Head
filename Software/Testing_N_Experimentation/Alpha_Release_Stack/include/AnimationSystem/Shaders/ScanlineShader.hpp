/*****************************************************************
 * @file ScanlineShader.hpp
 * @brief Retro scanline post-processing effect
 * 
 * Adds horizontal scanlines for a CRT/retro monitor look.
 * 
 * PARAMETERS:
 * - line_spacing: Pixels between scanlines
 * - line_darkness: How dark the scanlines are (0-1)
 * - animate: Whether scanlines scroll
 * - scroll_speed: Scroll speed in pixels per second
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "../Core/ShaderBase.hpp"
#include "../Core/ShaderRegistry.hpp"

namespace AnimationSystem {

class ScanlineShader : public ShaderBase {
public:
    ScanlineShader() {
        // Define parameters
        defineParam("line_spacing", "Line Spacing", "Pixels between scanlines",
                    ParamType::INT, 2.0f, 1.0f, 8.0f, "Appearance");
        
        defineParam("line_darkness", "Darkness", "How dark the scanlines are",
                    ParamType::FLOAT, 0.5f, 0.0f, 1.0f, "Appearance");
        
        defineParam("animate", "Animate", "Whether scanlines scroll",
                    ParamType::BOOL, 0.0f, 0.0f, 1.0f, "Animation");
        
        defineParam("scroll_speed", "Scroll Speed", "Scroll speed in pixels per second",
                    ParamType::FLOAT, 30.0f, 0.0f, 200.0f, "Animation");
    }
    
    const char* getTypeId() const override { return "scanline"; }
    const char* getDisplayName() const override { return "Scanlines"; }
    const char* getDescription() const override { return "Retro CRT scanline effect"; }
    
    void update(uint32_t deltaMs) override {
        if (!isEnabled()) return;
        
        time_ += deltaMs;
        
        if (getParamBool("animate")) {
            float speed = getParam("scroll_speed");
            scrollOffset_ += (speed * deltaMs) / 1000.0f;
            
            int spacing = getParamInt("line_spacing");
            if (scrollOffset_ >= spacing) {
                scrollOffset_ -= spacing;
            }
        } else {
            scrollOffset_ = 0;
        }
    }
    
    void apply() override {
        if (!isEnabled()) return;
        
        int spacing = getParamInt("line_spacing");
        float darkness = getParam("line_darkness") * getIntensity();
        int offset = static_cast<int>(scrollOffset_);
        
        for (int y = 0; y < DISPLAY_H; y++) {
            // Check if this row is a scanline
            bool isScanline = ((y + offset) % spacing) == 0;
            
            for (int x = 0; x < DISPLAY_W; x++) {
                uint8_t r, g, b;
                getSourcePixel(x, y, r, g, b);
                
                if (isScanline) {
                    // Darken the scanline
                    float factor = 1.0f - darkness;
                    r = static_cast<uint8_t>(r * factor);
                    g = static_cast<uint8_t>(g * factor);
                    b = static_cast<uint8_t>(b * factor);
                }
                
                if (drawPixel) {
                    drawPixel(x, y, r, g, b);
                }
            }
        }
    }
    
    void reset() override {
        ShaderBase::reset();
        scrollOffset_ = 0;
    }
    
private:
    float scrollOffset_ = 0;
};

// Auto-register the shader
REGISTER_SHADER(ScanlineShader, "scanline", "Scanlines", "Retro CRT scanline effect");

} // namespace AnimationSystem
