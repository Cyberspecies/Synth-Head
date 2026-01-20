/*****************************************************************
 * @file ShaderTestAnim.hpp
 * @brief Rotating/Moving Square Animation with Shader Support
 * 
 * Two rotating squares (one per eye panel) that orbit around the center.
 * Uses optimized scanline rendering with fillRect instead of drawPixel.
 * Supports optional GlitchShader integration.
 * 
 * Usage:
 *   1. Call update() each frame with deltaMs
 *   2. Call render() with optional shader pointer
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "../AnimationTypes.hpp"
#include "../Shaders/GlitchShader.hpp"
#include <cmath>

namespace AnimationSystem {
namespace Animations {

struct ShaderTestAnim {
    float time = 0.0f;
    float squareSize = 8.0f;
    
    // Per-eye state
    float leftAngle = 0.0f;
    float rightAngle = 0.0f;
    float leftPosX = 32.0f, leftPosY = 16.0f;
    float rightPosX = 32.0f, rightPosY = 16.0f;
    
    float lerp(float a, float b, float t) { return a + (b - a) * t; }
    
    float smoothstep(float edge0, float edge1, float x) {
        float t = (x - edge0) / (edge1 - edge0);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return t * t * (3.0f - 2.0f * t);
    }
    
    // SDF for rotated box
    float sdfRotatedBox(float px, float py, float cx, float cy, float size, float angle) {
        float dx = px - cx;
        float dy = py - cy;
        float cosA = cosf(-angle);
        float sinA = sinf(-angle);
        float rx = dx * cosA - dy * sinA;
        float ry = dx * sinA + dy * cosA;
        float qx = fabsf(rx) - size;
        float qy = fabsf(ry) - size;
        return fmaxf(qx, qy);
    }
    
    void update(uint32_t deltaMs) {
        time += deltaMs * 0.001f;
        
        // Rotate continuously
        leftAngle = time * 1.5f;
        rightAngle = -time * 1.5f;
        
        // Move in a circle around the panel center
        float orbitRadius = 8.0f;
        leftPosX = 32.0f + cosf(time * 0.8f) * orbitRadius;
        leftPosY = 16.0f + sinf(time * 0.8f) * orbitRadius;
        rightPosX = 32.0f + cosf(-time * 0.8f) * orbitRadius;
        rightPosY = 16.0f + sinf(-time * 0.8f) * orbitRadius;
    }
    
    // OPTIMIZED: Render using horizontal spans (fillRect) instead of drawPixel
    // Finds contiguous runs of solid pixels and batches them
    void renderSquareFast(float cx, float cy, float angle, int panelOffsetX,
                          FillRectFunc fillRect, Shaders::GlitchShader* shader) {
        if (!fillRect) return;
        
        int margin = (int)squareSize + 2;
        int startY = (int)cy - margin;
        int endY = (int)cy + margin;
        
        // Clamp to display
        if (startY < 0) startY = 0;
        if (endY >= DISPLAY_H) endY = DISPLAY_H - 1;
        
        // Pre-compute cos/sin for this frame
        float cosA = cosf(-angle);
        float sinA = sinf(-angle);
        
        for (int py = startY; py <= endY; py++) {
            // Get row offset from shader
            int rowOffset = shader ? shader->getRowOffset(py) : 0;
            
            // Check for row tint (skip if fully tinted - overlay handles it)
            uint8_t tr, tg, tb;
            if (shader && shader->getRowTint(py, tr, tg, tb)) {
                continue;  // Let overlay draw this row
            }
            
            // Find the horizontal span where SDF < 0 (inside the square)
            // Start from the approximate left edge of the rotated square
            int scanStartX = (int)cx - margin + panelOffsetX;
            int scanEndX = (int)cx + margin + panelOffsetX;
            
            // Find first pixel inside
            int spanStart = -1;
            int spanEnd = -1;
            
            for (int px = scanStartX; px <= scanEndX; px++) {
                float sampleX = (float)(px - panelOffsetX) + 0.5f;
                float sampleY = (float)py + 0.5f;
                
                // Inline SDF calculation (faster than function call)
                float dx = sampleX - cx;
                float dy = sampleY - cy;
                float rx = dx * cosA - dy * sinA;
                float ry = dx * sinA + dy * cosA;
                float sdf = fmaxf(fabsf(rx) - squareSize, fabsf(ry) - squareSize);
                
                if (sdf < 0.5f) {  // Inside or on edge
                    if (spanStart < 0) spanStart = px;
                    spanEnd = px;
                }
            }
            
            // Draw the span if found
            if (spanStart >= 0) {
                int drawX = spanStart + rowOffset;
                int width = spanEnd - spanStart + 1;
                
                // Clamp to display bounds
                if (drawX < 0) { width += drawX; drawX = 0; }
                if (drawX + width > DISPLAY_W) width = DISPLAY_W - drawX;
                
                if (width > 0) {
                    fillRect(drawX, py, width, 1, 255, 255, 255);
                }
            }
        }
    }
    
    void render(FillRectFunc fillRect, DrawPixelFunc drawPixel, ClearFunc clear, 
                PresentFunc present, Shaders::GlitchShader* shader = nullptr) {
        // Dark background
        clear(10, 10, 20);
        
        // Render using fast scanline method (fillRect instead of drawPixel)
        renderSquareFast(leftPosX, leftPosY, leftAngle, LEFT_EYE_X, fillRect, shader);
        renderSquareFast(rightPosX, rightPosY, rightAngle, RIGHT_EYE_X, fillRect, shader);
        
        // Apply shader overlay (scanlines, tint bands, flashes)
        if (shader) {
            shader->applyOverlay(fillRect);
        }
        
        present();
    }
};

} // namespace Animations
} // namespace AnimationSystem
