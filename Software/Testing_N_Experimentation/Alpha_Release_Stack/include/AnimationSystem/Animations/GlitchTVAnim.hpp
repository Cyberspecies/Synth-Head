/*****************************************************************
 * @file GlitchTVAnim.hpp
 * @brief Glitch TV Demo Animation
 * 
 * Demonstrates the GlitchShader effect on a simple scene.
 * Shows chromatic aberration, row displacement, and scanlines.
 * 
 * Usage:
 *   1. Call update() each frame with deltaMs
 *   2. Call render() to draw the scene
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "../AnimationTypes.hpp"
#include "../Shaders/GlitchShader.hpp"

namespace AnimationSystem {
namespace Animations {

struct GlitchTVAnim {
    Shaders::GlitchShader shader;
    
    void update(uint32_t deltaMs) {
        shader.update(deltaMs);
    }
    
    void render(FillRectFunc fillRect, DrawPixelFunc drawPixel, ClearFunc clear, PresentFunc present) {
        // Dark background
        clear(5, 5, 10);
        
        // Draw base content - two "eye" rectangles with shader displacement
        int baseY = 8;
        int chroma = shader.getChromaOffset();
        
        for (int row = 0; row < 16; row++) {
            int y = baseY + row;
            if (y >= DISPLAY_H) break;
            
            // Query shader for this row's offset
            int offset = shader.getRowOffset(y);
            
            // Check if row has a tint (skip normal drawing if so)
            uint8_t tr, tg, tb;
            if (shader.getRowTint(y, tr, tg, tb)) {
                // Row is tinted, just draw the tint color
                continue;  // Let overlay handle it
            }
            
            // Chromatic aberration with row offset
            fillRect(20 - chroma + offset, y, 24, 1, 150, 0, 0);
            fillRect(84 - chroma + offset, y, 24, 1, 150, 0, 0);
            fillRect(20 + chroma + offset, y, 24, 1, 0, 0, 150);
            fillRect(84 + chroma + offset, y, 24, 1, 0, 0, 150);
            fillRect(20 + offset, y, 24, 1, 0, 200, 0);
            fillRect(84 + offset, y, 24, 1, 0, 200, 0);
        }
        
        // Apply shader overlay (scanlines, tint bands, flashes)
        shader.applyOverlay(fillRect);
        
        present();
    }
    
    // Get reference to internal shader for external control
    Shaders::GlitchShader& getShader() { return shader; }
    const Shaders::GlitchShader& getShader() const { return shader; }
};

} // namespace Animations
} // namespace AnimationSystem
