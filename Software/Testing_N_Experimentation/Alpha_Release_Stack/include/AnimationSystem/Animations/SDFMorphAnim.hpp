/*****************************************************************
 * @file SDFMorphAnim.hpp
 * @brief SDF Shape Morphing Animation
 * 
 * Square → Triangle → Circle morphing using Signed Distance Fields.
 * Uses weighted pixel blending and smooth color transitions.
 * Optimized with scanline rendering using fillRect.
 * 
 * Usage:
 *   1. Call update() each frame with deltaMs
 *   2. Call render() to draw the morphing shapes
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "../AnimationTypes.hpp"
#include <cmath>

// Forward declare vTaskDelay if needed (ESP-IDF)
extern "C" {
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
}

namespace AnimationSystem {
namespace Animations {

struct SDFMorphAnim {
    float morphTime = 0.0f;
    float morphSpeed = 0.0008f;  // Slightly faster cycle
    int currentShape = 0;  // 0=square, 1=triangle, 2=circle
    float morphProgress = 0.0f;  // 0-1 between shapes
    
    // Shape centers
    float leftCenterX = 32.0f, leftCenterY = 16.0f;
    float rightCenterX = 96.0f, rightCenterY = 16.0f;
    float shapeSize = 12.0f;
    
    // Rotation
    float leftRotation = 0.0f;
    float rightRotation = 0.0f;
    float rotationSpeed = 1.2f;  // radians per second
    
    // Pre-computed interpolation value
    float t = 0.0f;
    
    // Pre-computed colors
    uint8_t colorR = 0, colorG = 255, colorB = 255;
    
    // Inline SDF functions - no function call overhead
    inline float sdfSquareInline(float x, float y, float cx, float cy, float size) {
        return fmaxf(fabsf(x - cx) - size, fabsf(y - cy) - size);
    }
    
    inline float sdfCircleInline(float x, float y, float cx, float cy, float radius) {
        float dx = x - cx;
        float dy = y - cy;
        return sqrtf(dx * dx + dy * dy) - radius;
    }
    
    // Simplified triangle SDF (faster approximation)
    inline float sdfTriangleInline(float x, float y, float cx, float cy, float size) {
        float px = fabsf(x - cx);
        float py = y - cy + size * 0.5f;
        // Approximate equilateral triangle with linear edges
        float slope = size * 1.732f;  // sqrt(3)
        float edge = py + px * 1.732f - size * 1.732f;
        return fmaxf(edge / 2.0f, -py - size * 0.5f);
    }
    
    // Get interpolated SDF at a point with rotation
    inline float getSDF(float x, float y, float cx, float cy, float rotation) {
        // Rotate point around center
        float dx = x - cx;
        float dy = y - cy;
        float cosA = cosf(-rotation);
        float sinA = sinf(-rotation);
        float rx = dx * cosA - dy * sinA + cx;
        float ry = dx * sinA + dy * cosA + cy;
        
        float sdf1, sdf2;
        switch (currentShape) {
            case 0:  // Square → Triangle
                sdf1 = sdfSquareInline(rx, ry, cx, cy, shapeSize);
                sdf2 = sdfTriangleInline(rx, ry, cx, cy, shapeSize);
                break;
            case 1:  // Triangle → Circle
                sdf1 = sdfTriangleInline(rx, ry, cx, cy, shapeSize);
                sdf2 = sdfCircleInline(rx, ry, cx, cy, shapeSize);
                break;
            default:  // Circle → Square
                sdf1 = sdfCircleInline(rx, ry, cx, cy, shapeSize);
                sdf2 = sdfSquareInline(rx, ry, cx, cy, shapeSize);
                break;
        }
        return sdf1 + (sdf2 - sdf1) * t;  // lerp
    }
    
    void update(uint32_t deltaMs) {
        morphTime += deltaMs * morphSpeed;
        
        float cycle = fmodf(morphTime, 3.0f);
        currentShape = (int)cycle;
        morphProgress = cycle - currentShape;
        
        // Pre-compute smoothstep interpolation
        float st = morphProgress;
        t = st * st * (3.0f - 2.0f * st);
        
        // Update rotation (opposite directions for left/right)
        float deltaSeconds = deltaMs * 0.001f;
        leftRotation += rotationSpeed * deltaSeconds;
        rightRotation -= rotationSpeed * deltaSeconds;
        
        // Pre-compute blended color
        uint8_t r1, g1, b1, r2, g2, b2;
        switch (currentShape) {
            case 0: r1 = 0; g1 = 255; b1 = 255; r2 = 255; g2 = 0; b2 = 255; break;
            case 1: r1 = 255; g1 = 0; b1 = 255; r2 = 255; g2 = 255; b2 = 0; break;
            default: r1 = 255; g1 = 255; b1 = 0; r2 = 0; g2 = 255; b2 = 255; break;
        }
        colorR = (uint8_t)(r1 + (r2 - r1) * t);
        colorG = (uint8_t)(g1 + (g2 - g1) * t);
        colorB = (uint8_t)(b1 + (b2 - b1) * t);
    }
    
    void render(FillRectFunc fillRect, DrawPixelFunc drawPixel, ClearFunc clear, PresentFunc present) {
        clear(10, 5, 20);
        
        // Render both shapes using optimized scanline method
        renderShapeFast(leftCenterX, leftCenterY, leftRotation, fillRect);
        vTaskDelay(pdMS_TO_TICKS(1));  // Yield between eyes
        renderShapeFast(rightCenterX, rightCenterY, rightRotation, fillRect);
        
        present();
    }
    
    // OPTIMIZED: Scanline rendering - finds horizontal spans and draws with fillRect
    void renderShapeFast(float cx, float cy, float rotation, FillRectFunc fillRect) {
        if (!fillRect) return;
        
        int margin = (int)shapeSize + 1;  // Reduced margin
        int startY = (int)cy - margin;
        int endY = (int)cy + margin;
        
        if (startY < 0) startY = 0;
        if (endY >= DISPLAY_H) endY = DISPLAY_H - 1;
        
        // Pre-compute rotation
        float cosA = cosf(-rotation);
        float sinA = sinf(-rotation);
        
        int rowCount = 0;
        for (int py = startY; py <= endY; py++) {
            // Yield every 3 rows (more frequent)
            if (++rowCount % 3 == 0) {
                vTaskDelay(0);  // Yield to watchdog
            }
            
            float y = py + 0.5f;
            
            // Find left edge using binary search for efficiency
            int scanLeft = (int)cx - margin;
            int scanRight = (int)cx + margin;
            if (scanLeft < 0) scanLeft = 0;
            if (scanRight >= DISPLAY_W) scanRight = DISPLAY_W - 1;
            
            int leftEdge = -1;
            int rightEdge = -1;
            
            // Linear scan from approximate center (faster for small shapes)
            int centerX = (int)cx;
            
            // Tighter scan range based on row distance
            float rowDistY = fabsf(y - cy);
            int maxSearchX = (int)(sqrtf(fmaxf(0.0f, (shapeSize + 2) * (shapeSize + 2) - rowDistY * rowDistY)));
            int searchLeft = centerX - maxSearchX;
            int searchRight = centerX + maxSearchX;
            if (searchLeft < scanLeft) searchLeft = scanLeft;
            if (searchRight > scanRight) searchRight = scanRight;
            
            // Search left from center (with early exit optimization)
            for (int px = centerX; px >= scanLeft; px--) {
                float dx = px + 0.5f - cx;
                float dy = y - cy;
                
                // Quick distance check before expensive SDF
                float distSq = dx * dx + dy * dy;
                if (distSq > (shapeSize + 3) * (shapeSize + 3)) {
                    if (leftEdge >= 0) break;  // Past the shape
                    continue;
                }
                
                float rx = dx * cosA - dy * sinA + cx;
                float ry = dx * sinA + dy * cosA + cy;
                
                float sdf1, sdf2;
                switch (currentShape) {
                    case 0: sdf1 = sdfSquareInline(rx, ry, cx, cy, shapeSize); sdf2 = sdfTriangleInline(rx, ry, cx, cy, shapeSize); break;
                    case 1: sdf1 = sdfTriangleInline(rx, ry, cx, cy, shapeSize); sdf2 = sdfCircleInline(rx, ry, cx, cy, shapeSize); break;
                    default: sdf1 = sdfCircleInline(rx, ry, cx, cy, shapeSize); sdf2 = sdfSquareInline(rx, ry, cx, cy, shapeSize); break;
                }
                float sdf = sdf1 + (sdf2 - sdf1) * t;
                
                if (sdf < 0.5f) leftEdge = px;
                else if (leftEdge >= 0) break;  // Found edge, stop searching
            }
            
            // Search right from center (with early exit optimization)
            for (int px = centerX; px <= scanRight; px++) {
                float dx = px + 0.5f - cx;
                float dy = y - cy;
                
                // Quick distance check before expensive SDF
                float distSq = dx * dx + dy * dy;
                if (distSq > (shapeSize + 3) * (shapeSize + 3)) {
                    if (rightEdge >= 0) break;  // Past the shape
                    continue;
                }
                
                float rx = dx * cosA - dy * sinA + cx;
                float ry = dx * sinA + dy * cosA + cy;
                
                float sdf1, sdf2;
                switch (currentShape) {
                    case 0: sdf1 = sdfSquareInline(rx, ry, cx, cy, shapeSize); sdf2 = sdfTriangleInline(rx, ry, cx, cy, shapeSize); break;
                    case 1: sdf1 = sdfTriangleInline(rx, ry, cx, cy, shapeSize); sdf2 = sdfCircleInline(rx, ry, cx, cy, shapeSize); break;
                    default: sdf1 = sdfCircleInline(rx, ry, cx, cy, shapeSize); sdf2 = sdfSquareInline(rx, ry, cx, cy, shapeSize); break;
                }
                float sdf = sdf1 + (sdf2 - sdf1) * t;
                
                if (sdf < 0.5f) rightEdge = px;
                else if (rightEdge >= 0) break;  // Found edge, stop searching
            }
            
            // Draw the span
            if (leftEdge >= 0 && rightEdge >= leftEdge) {
                int width = rightEdge - leftEdge + 1;
                fillRect(leftEdge, py, width, 1, colorR, colorG, colorB);
            }
        }
    }
};

} // namespace Animations
} // namespace AnimationSystem
