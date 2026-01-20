#pragma once

#include "../AnimationTypes.hpp"

namespace AnimationSystem {
namespace Shaders {

// ================================================================
// GLITCH SHADER - Reusable post-processing effect
// Can be applied to any scene for row displacement, scanlines, etc.
// Usage:
//   1. Call update() each frame to evolve glitch state
//   2. When rendering, query getRowOffset(y) for per-row displacement
//   3. After drawing content, call applyOverlay() for scanlines/flashes
// ================================================================
struct GlitchShader {
    uint32_t frameCount = 0;
    uint32_t glitchTimer = 0;
    int chromaOffset = 2;
    int scanlineY = 0;
    bool enabled = true;
    
    // Intensity control (0.0 = off, 1.0 = normal, >1.0 = overdrive for transitions)
    float intensity = 1.0f;
    
    // Row glitch state - each row can have its own offset
    static constexpr int MAX_GLITCH_ROWS = 16;
    struct RowGlitch {
        int y;           // Row Y position
        int height;      // Height of glitch band
        int offsetX;     // Horizontal shift
        int duration;    // Frames remaining
        uint8_t r, g, b; // Optional color tint
        bool colorTint;  // Apply color tint?
    };
    RowGlitch rowGlitches[MAX_GLITCH_ROWS];
    int activeGlitches = 0;
    
    // Per-row random offset for subtle continuous glitching
    int8_t rowOffsets[DISPLAY_H] = {0};
    uint32_t rowOffsetTimer = 0;
    
    // Random seed
    uint32_t noiseSeed = 12345;
    
    uint32_t fastRand() {
        noiseSeed = noiseSeed * 1103515245 + 12345;
        return (noiseSeed >> 16) & 0x7FFF;
    }
    
    void setEnabled(bool en) { enabled = en; }
    void setIntensity(float i) { intensity = (i < 0.0f) ? 0.0f : (i > 2.0f) ? 2.0f : i; }  // Allow overdrive up to 2.0
    
    // Get the current horizontal offset for a given row
    // Animations should use this when drawing to apply row displacement
    int getRowOffset(int y) const {
        if (!enabled || intensity < 0.01f) return 0;
        if (y < 0 || y >= DISPLAY_H) return 0;
        
        // Base per-row jitter
        int offset = rowOffsets[y];
        
        // Add any active glitch offsets for this row
        for (int i = 0; i < activeGlitches; i++) {
            const RowGlitch& g = rowGlitches[i];
            if (y >= g.y && y < g.y + g.height) {
                offset += g.offsetX;
            }
        }
        
        return (int)(offset * intensity);
    }
    
    // Get chromatic aberration offset (for RGB channel separation)
    int getChromaOffset() const {
        if (!enabled || intensity < 0.01f) return 0;
        return (int)(chromaOffset * intensity);
    }
    
    // Check if a row has an active color tint glitch
    // Returns true and fills r,g,b if there's a tint, false otherwise
    bool getRowTint(int y, uint8_t& r, uint8_t& g, uint8_t& b) const {
        if (!enabled || intensity < 0.01f) return false;
        
        for (int i = 0; i < activeGlitches; i++) {
            const RowGlitch& gl = rowGlitches[i];
            if (gl.colorTint && y >= gl.y && y < gl.y + gl.height) {
                r = (uint8_t)(gl.r * intensity);
                g = (uint8_t)(gl.g * intensity);
                b = (uint8_t)(gl.b * intensity);
                return true;
            }
        }
        return false;
    }
    
    void update(uint32_t deltaMs) {
        if (!enabled) return;
        
        frameCount++;
        glitchTimer += deltaMs;
        
        // Update existing glitches
        for (int i = 0; i < activeGlitches; ) {
            rowGlitches[i].duration--;
            if (rowGlitches[i].duration <= 0) {
                // Remove glitch by swapping with last
                rowGlitches[i] = rowGlitches[--activeGlitches];
            } else {
                i++;
            }
        }
        
        // Spawn new row glitches periodically
        int spawnInterval = 20 + (int)((1.0f - intensity) * 80);  // More intense = more frequent
        if (glitchTimer > (uint32_t)spawnInterval + (fastRand() % 80)) {
            glitchTimer = 0;
            
            int maxNew = 2 + (int)(intensity * 3);  // 2-5 based on intensity
            int newGlitches = 2 + (fastRand() % maxNew);
            for (int i = 0; i < newGlitches && activeGlitches < MAX_GLITCH_ROWS; i++) {
                RowGlitch& g = rowGlitches[activeGlitches++];
                g.y = fastRand() % DISPLAY_H;
                g.height = 1 + (fastRand() % 4);
                g.offsetX = -8 + (fastRand() % 17);
                g.duration = 3 + (fastRand() % 8);
                g.colorTint = (fastRand() % 5 == 0);
                if (g.colorTint) {
                    g.r = 50 + (fastRand() % 100);
                    g.g = 50 + (fastRand() % 100);
                    g.b = 80 + (fastRand() % 120);
                }
            }
            
            if (fastRand() % 3 == 0) {
                chromaOffset = 1 + (fastRand() % 4);
            }
        }
        
        // Update per-row random offsets
        rowOffsetTimer += deltaMs;
        if (rowOffsetTimer > 30) {
            rowOffsetTimer = 0;
            int numRowsToJitter = 3 + (fastRand() % 6);
            for (int i = 0; i < numRowsToJitter; i++) {
                int row = fastRand() % DISPLAY_H;
                if (fastRand() % 2 == 0) {
                    rowOffsets[row] = -2 + (fastRand() % 5);
                } else {
                    rowOffsets[row] = 0;
                }
            }
        }
        
        scanlineY = (scanlineY + 1) % DISPLAY_H;
    }
    
    // Apply overlay effects AFTER the main scene is drawn
    // This adds scanlines, edge flashes, and color tint bands
    void applyOverlay(FillRectFunc fillRect) {
        if (!enabled || !fillRect || intensity < 0.01f) return;
        
        // Draw color tint bands (overlay on content)
        for (int i = 0; i < activeGlitches; i++) {
            RowGlitch& g = rowGlitches[i];
            if (g.colorTint) {
                fillRect(0, g.y, DISPLAY_W, g.height, 
                        (uint8_t)(g.r * intensity), 
                        (uint8_t)(g.g * intensity), 
                        (uint8_t)(g.b * intensity));
            }
        }
        
        // Scanline effect
        uint8_t scanAlpha = (uint8_t)(255 * intensity * 0.3f);
        if (scanAlpha > 5) {
            fillRect(0, scanlineY, DISPLAY_W, 1, 0, 0, 0);
        }
        
        // Edge flashes
        if (frameCount % 40 < 3 && intensity > 0.3f) {
            fillRect(0, 0, DISPLAY_W, 1, 100, 100, 120);
        }
        if (frameCount % 30 < 2 && intensity > 0.3f) {
            fillRect(0, DISPLAY_H - 1, DISPLAY_W, 1, 100, 100, 120);
        }
    }
    
    // Reset all glitch state
    void reset() {
        activeGlitches = 0;
        for (int i = 0; i < DISPLAY_H; i++) rowOffsets[i] = 0;
        chromaOffset = 2;
        scanlineY = 0;
        glitchTimer = 0;
        rowOffsetTimer = 0;
    }
};

} // namespace Shaders
} // namespace AnimationSystem
