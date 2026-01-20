/*****************************************************************
 * @file AnimationSandbox.hpp
 * @brief TEMPORARY - Animation Testing & Experimentation
 * 
 * ⚠️ SPAGHETTI CODE ZONE ⚠️
 * This file is for rapid prototyping and testing animation ideas.
 * Code here is intentionally messy for quick iteration.
 * DO NOT use in production - refactor working concepts into proper files.
 * 
 * Current Animations (5 second loop each):
 * 1. GYRO_EYES - Polygon eyes with gyro-tracking pupils
 * 2. GLITCH_TV - Chromatic aberration, static, retro glitch
 * 3. SDF_MORPH - Square → Triangle → Circle SDF transitions
 * 
 * @author ARCOS
 * @version 0.2 (experimental)
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <functional>
#include "esp_timer.h"

namespace AnimationSystem {
namespace Sandbox {

// ================================================================
// GPU CALLBACK TYPES
// ================================================================
using ClearFunc = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;
using BlitSpriteFunc = std::function<void(int id, float x, float y)>;
using BlitSpriteRotatedFunc = std::function<void(int id, float x, float y, float angle)>;
using FillCircleFunc = std::function<void(int cx, int cy, int r, uint8_t red, uint8_t green, uint8_t blue)>;
using DrawCircleFFunc = std::function<void(float x, float y, float radius, uint8_t r, uint8_t g, uint8_t b)>;
using FillRectFunc = std::function<void(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)>;
using DrawLineFunc = std::function<void(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b)>;
using DrawPixelFunc = std::function<void(int x, int y, uint8_t r, uint8_t g, uint8_t b)>;
using PresentFunc = std::function<void()>;

// ================================================================
// DISPLAY CONSTANTS (128x32 HUB75, split into 2 x 64x32 eyes)
// ================================================================
constexpr int DISPLAY_W = 128;
constexpr int DISPLAY_H = 32;
constexpr int EYE_W = 64;
constexpr int EYE_H = 32;
constexpr int LEFT_EYE_X = 0;
constexpr int RIGHT_EYE_X = 64;

// Animation timing
constexpr uint32_t ANIMATION_DURATION_MS = 5000;  // 5 seconds per animation
constexpr int NUM_ANIMATIONS = 3;

// ================================================================
// ANIMATION 1: GYRO EYES
// Non-blocking rolling window average + sin() mapping for smooth edges
// Every sample is recorded, average is always up-to-date
// Uses drawCircleF for GPU-side AA rendering
// ================================================================
struct GyroEyesAnim {
    // Eye polygon points (relative to eye origin 0,0)
    // These define the eye white shape
    static constexpr int NUM_POINTS = 16;
    const int eyePointsX[NUM_POINTS] = {6, 14, 20, 26, 27, 28, 23, 21, 19, 17, 16, 18, 7, 4, 2, 2};
    const int eyePointsY[NUM_POINTS] = {8, 8, 11, 17, 19, 22, 22, 20, 17, 12};
    
    // Circle center positions (output to render)
    float leftCircleX = 32.0f;
    float leftCircleY = 16.0f;
    float rightCircleX = 32.0f;  
    float rightCircleY = 16.0f;
    
    // Config - tune these for feel
    float gyroMax = 150.0f;       // Gyro value that maps to ±π (deg/s)
    float movementScale = 10.0f;  // Pixels of movement at sin() = ±1
    int circleRadius = 12;
    
    // Colors
    uint8_t bgR = 0, bgG = 0, bgB = 0;
    uint8_t eyeR = 255, eyeG = 255, eyeB = 255;
    uint8_t pupilR = 20, pupilG = 20, pupilB = 80;
    
    // Rolling window for gyro smoothing - records EVERY sample
    static constexpr int WINDOW_SIZE = 5;
    float gyroXHistory[WINDOW_SIZE] = {0};
    float gyroYHistory[WINDOW_SIZE] = {0};
    float gyroZHistory[WINDOW_SIZE] = {0};
    int windowIndex = 0;
    
    // Running sums for O(1) average calculation
    float sumX = 0, sumY = 0, sumZ = 0;
    
    void update(float gyroX, float gyroY, float gyroZ, uint32_t deltaMs) {
        // Non-blocking rolling window: record EVERY sample
        // Subtract old value from sum, add new value
        sumX -= gyroXHistory[windowIndex];
        sumY -= gyroYHistory[windowIndex];
        sumZ -= gyroZHistory[windowIndex];
        
        // Store new value
        gyroXHistory[windowIndex] = gyroX;
        gyroYHistory[windowIndex] = gyroY;
        gyroZHistory[windowIndex] = gyroZ;
        
        // Add new value to sum
        sumX += gyroX;
        sumY += gyroY;
        sumZ += gyroZ;
        
        // Advance index
        windowIndex = (windowIndex + 1) % WINDOW_SIZE;
        
        // Calculate average (O(1) - just divide the running sum)
        float avgX = sumX / WINDOW_SIZE;
        float avgY = sumY / WINDOW_SIZE;
        float avgZ = sumZ / WINDOW_SIZE;
        
        // Clamp averaged gyro to ±gyroMax
        float clampedX = fmaxf(-gyroMax, fminf(gyroMax, avgX));
        float clampedY = fmaxf(-gyroMax, fminf(gyroMax, avgY));
        float clampedZ = fmaxf(-gyroMax, fminf(gyroMax, avgZ));
        
        // Map to -π to π
        float angleX = (clampedX / gyroMax) * M_PI;
        float angleY = (clampedY / gyroMax) * M_PI;
        float angleZ = (clampedZ / gyroMax) * M_PI;
        
        // Apply sin() for smooth -1 to 1 output
        float sinX = sinf(angleX);
        float sinY = sinf(angleY);
        float sinZ = sinf(angleZ);
        
        // Map sin output to eye positions
        // Panel 0 (left eye) - MIRRORED horizontally:
        // Device Gyro +Z → Panel 0 left
        // Device Gyro -Y → Panel 0 down, +X → Panel 0 down
        float leftOffsetX = sinZ * movementScale;
        float leftOffsetY = (-sinY + sinX) * movementScale;
        
        // Panel 1 (right eye):
        float rightOffsetX = -sinZ * movementScale;
        float rightOffsetY = (sinY + sinX) * movementScale;
        
        // Calculate final positions
        leftCircleX = 32.0f + leftOffsetX;
        leftCircleY = 16.0f + leftOffsetY;
        rightCircleX = 32.0f + rightOffsetX;
        rightCircleY = 16.0f + rightOffsetY;
        
        // Simple bounds clamp (hard edges)
        if (leftCircleX < circleRadius) leftCircleX = circleRadius;
        if (leftCircleX > EYE_W - circleRadius) leftCircleX = EYE_W - circleRadius;
        if (leftCircleY < circleRadius) leftCircleY = circleRadius;
        if (leftCircleY > EYE_H - circleRadius) leftCircleY = EYE_H - circleRadius;
        
        if (rightCircleX < circleRadius) rightCircleX = circleRadius;
        if (rightCircleX > EYE_W - circleRadius) rightCircleX = EYE_W - circleRadius;
        if (rightCircleY < circleRadius) rightCircleY = circleRadius;
        if (rightCircleY > EYE_H - circleRadius) rightCircleY = EYE_H - circleRadius;
    }
    
    // Soft clamp using tanh (hyperbolic tangent) for smooth exponential-like boundaries
    // Maps input smoothly to output range with natural slowdown at edges
    float softClamp(float value, float minVal, float maxVal) {
        float center = (minVal + maxVal) / 2.0f;
        float range = (maxVal - minVal) / 2.0f;
        
        // Normalize input to -inf to +inf range centered on 0
        float normalized = (value - center) / range;
        
        // Apply tanh for smooth S-curve (exponential slowdown at edges)
        // tanh approaches -1 and +1 asymptotically, never quite reaching them
        // Scale factor controls how quickly it saturates (higher = more range/strength)
        float scaleFactor = 1.5f;  // Increased strength for more range
        float curved = tanh(normalized * scaleFactor);
        
        // Map back to output range
        return center + curved * range;
    }
    
    // Debug frame counter
    uint32_t frameCount = 0;
    
    void render(FillRectFunc fillRect, DrawPixelFunc drawPixel, ClearFunc clear, PresentFunc present,
                 DrawCircleFFunc drawCircleF = nullptr) {
        // Clear to black background
        clear(bgR, bgG, bgB);
        
        frameCount++;
        
        // Calculate final positions
        float leftX = LEFT_EYE_X + leftCircleX;
        float leftY = leftCircleY;
        float rightX = RIGHT_EYE_X + rightCircleX;
        float rightY = rightCircleY;
        
        // Use AA circle rendering if available
        if (drawCircleF) {
            // GPU-side AA circles with sub-pixel positioning
            drawCircleF(leftX, leftY, (float)circleRadius, eyeR, eyeG, eyeB);
            drawCircleF(rightX, rightY, (float)circleRadius, eyeR, eyeG, eyeB);
        } else {
            // Fallback: software circle rendering (no AA)
            drawCircle((int)(leftX + 0.5f), (int)(leftY + 0.5f), circleRadius, fillRect, drawPixel, eyeR, eyeG, eyeB);
            drawCircle((int)(rightX + 0.5f), (int)(rightY + 0.5f), circleRadius, fillRect, drawPixel, eyeR, eyeG, eyeB);
        }
        
        present();
    }
    
    // Simple circle drawing using midpoint algorithm
    void drawCircle(int cx, int cy, int r, FillRectFunc fillRect, DrawPixelFunc drawPixel, uint8_t red, uint8_t green, uint8_t blue) {
        // Draw filled circle using horizontal lines
        for (int y = -r; y <= r; y++) {
            int x = (int)sqrt(r*r - y*y);
            int drawY = cy + y;
            if (drawY >= 0 && drawY < DISPLAY_H) {
                int startX = cx - x;
                int endX = cx + x;
                if (startX < 0) startX = 0;
                if (endX >= DISPLAY_W) endX = DISPLAY_W - 1;
                if (startX <= endX) {
                    fillRect(startX, drawY, endX - startX + 1, 1, red, green, blue);
                }
            }
        }
    }
};

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
    
    // Intensity control (0.0 = off, 1.0 = full)
    float intensity = 1.0f;
    
    // Row glitch state - each row can have its own offset
    static constexpr int MAX_GLITCH_ROWS = 12;
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
    void setIntensity(float i) { intensity = (i < 0.0f) ? 0.0f : (i > 1.0f) ? 1.0f : i; }
    
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
            if (!g.colorTint && y >= g.y && y < g.y + g.height) {
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
                rowGlitches[i] = rowGlitches[activeGlitches - 1];
                activeGlitches--;
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
                RowGlitch& g = rowGlitches[activeGlitches];
                g.y = fastRand() % DISPLAY_H;
                g.height = 1 + (fastRand() % 3);
                g.offsetX = (fastRand() % 50) - 25;
                g.duration = 1 + (fastRand() % 6);
                g.colorTint = (fastRand() % 100) < (int)(25 * intensity);
                if (g.colorTint) {
                    int colorChoice = fastRand() % 7;
                    g.r = (colorChoice & 0x04) ? 255 : 30;
                    g.g = (colorChoice & 0x02) ? 255 : 30;
                    g.b = (colorChoice & 0x01) ? 255 : 30;
                }
                activeGlitches++;
            }
            
            if (fastRand() % 3 == 0) {
                chromaOffset = 1 + (fastRand() % 6);
            }
        }
        
        // Update per-row random offsets
        rowOffsetTimer += deltaMs;
        if (rowOffsetTimer > 30) {
            rowOffsetTimer = 0;
            int numRowsToJitter = 3 + (fastRand() % 6);
            for (int i = 0; i < numRowsToJitter; i++) {
                int row = fastRand() % DISPLAY_H;
                if (fastRand() % 3 == 0) {
                    rowOffsets[row] = 0;
                } else {
                    rowOffsets[row] = (int8_t)((fastRand() % 7) - 3);
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

// ================================================================
// ANIMATION 2: GLITCH TV (Demo scene using GlitchShader)
// Shows how to use the GlitchShader on a simple scene
// ================================================================
struct GlitchTVAnim {
    GlitchShader shader;
    
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
};

// ================================================================
// ANIMATION 3: SDF MORPH
// Square → Triangle → Circle using SDF and weighted pixels
// Transitions smoothly between shapes
// ================================================================
// ANIMATION 4: SHADER TEST - Rotating/Moving AA Square
// Tests the GlitchShader with a simple animated square on each panel
// OPTIMIZED: Uses scanline fillRect instead of individual drawPixel
// ================================================================
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
                          FillRectFunc fillRect, GlitchShader* shader) {
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
                PresentFunc present, GlitchShader* shader) {
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

// ================================================================
struct SDFMorphAnim {
    float morphTime = 0.0f;
    float morphSpeed = 0.0006f;  // Complete cycle in ~5 seconds
    int currentShape = 0;  // 0=square, 1=triangle, 2=circle
    float morphProgress = 0.0f;  // 0-1 between shapes
    
    // Shape centers
    float leftCenterX = 32.0f, leftCenterY = 16.0f;
    float rightCenterX = 96.0f, rightCenterY = 16.0f;
    float shapeSize = 12.0f;
    
    // SDF functions return distance to shape edge (negative = inside)
    float sdfSquare(float x, float y, float cx, float cy, float size) {
        float dx = fabsf(x - cx) - size;
        float dy = fabsf(y - cy) - size;
        return fmaxf(dx, dy);
    }
    
    float sdfCircle(float x, float y, float cx, float cy, float radius) {
        float dx = x - cx;
        float dy = y - cy;
        return sqrtf(dx * dx + dy * dy) - radius;
    }
    
    float sdfTriangle(float x, float y, float cx, float cy, float size) {
        // Equilateral triangle pointing up
        float px = x - cx;
        float py = y - cy + size * 0.3f;  // Shift down a bit
        
        // Rotate and fold into first quadrant
        float k = sqrtf(3.0f);
        px = fabsf(px) - size;
        py = py + size / k;
        if (px + k * py > 0.0f) {
            float newPx = (px - k * py) / 2.0f;
            float newPy = (-k * px - py) / 2.0f;
            px = newPx;
            py = newPy;
        }
        px -= fminf(fmaxf(px, -2.0f * size), 0.0f);
        return -sqrtf(px * px + py * py) * ((py > 0.0f) ? 1.0f : -1.0f);
    }
    
    float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    
    // Smooth interpolation
    float smoothstep(float edge0, float edge1, float x) {
        float t = (x - edge0) / (edge1 - edge0);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return t * t * (3.0f - 2.0f * t);
    }
    
    void update(uint32_t deltaMs) {
        morphTime += deltaMs * morphSpeed;
        
        // Calculate which transition we're in
        // 0.0-1.0 = square→triangle, 1.0-2.0 = triangle→circle, 2.0-3.0 = circle→square
        float cycle = fmodf(morphTime, 3.0f);
        currentShape = (int)cycle;
        morphProgress = cycle - currentShape;
    }
    
    void render(FillRectFunc fillRect, DrawPixelFunc drawPixel, ClearFunc clear, PresentFunc present) {
        clear(10, 5, 20);  // Dark purple background
        
        // Render both eyes
        renderShape(leftCenterX, leftCenterY, drawPixel);
        renderShape(rightCenterX, rightCenterY, drawPixel);
        
        present();
    }
    
    void renderShape(float cx, float cy, DrawPixelFunc drawPixel) {
        if (!drawPixel) return;
        
        // Render area around center
        int startX = (int)(cx - shapeSize - 2);
        int endX = (int)(cx + shapeSize + 2);
        int startY = (int)(cy - shapeSize - 2);
        int endY = (int)(cy + shapeSize + 2);
        
        for (int py = startY; py <= endY; py++) {
            for (int px = startX; px <= endX; px++) {
                if (px < 0 || px >= DISPLAY_W || py < 0 || py >= DISPLAY_H) continue;
                
                float x = px + 0.5f;
                float y = py + 0.5f;
                
                // Get SDF for current and next shape
                float sdf1, sdf2;
                
                switch (currentShape) {
                    case 0:  // Square → Triangle
                        sdf1 = sdfSquare(x, y, cx, cy, shapeSize);
                        sdf2 = sdfTriangle(x, y, cx, cy, shapeSize);
                        break;
                    case 1:  // Triangle → Circle
                        sdf1 = sdfTriangle(x, y, cx, cy, shapeSize);
                        sdf2 = sdfCircle(x, y, cx, cy, shapeSize);
                        break;
                    case 2:  // Circle → Square
                    default:
                        sdf1 = sdfCircle(x, y, cx, cy, shapeSize);
                        sdf2 = sdfSquare(x, y, cx, cy, shapeSize);
                        break;
                }
                
                // Interpolate between shapes
                float t = smoothstep(0.0f, 1.0f, morphProgress);
                float sdf = lerp(sdf1, sdf2, t);
                
                // Anti-aliased edge (weighted pixels)
                // sdf < 0 means inside, > 0 means outside
                // Use a soft edge of about 1.5 pixels
                float alpha = 1.0f - smoothstep(-0.5f, 1.0f, sdf);
                
                if (alpha > 0.01f) {
                    // Color based on shape type (blend colors too)
                    uint8_t r1, g1, b1, r2, g2, b2;
                    switch (currentShape) {
                        case 0:  // Square (cyan) → Triangle (magenta)
                            r1 = 0; g1 = 255; b1 = 255;
                            r2 = 255; g2 = 0; b2 = 255;
                            break;
                        case 1:  // Triangle (magenta) → Circle (yellow)
                            r1 = 255; g1 = 0; b1 = 255;
                            r2 = 255; g2 = 255; b2 = 0;
                            break;
                        case 2:  // Circle (yellow) → Square (cyan)
                        default:
                            r1 = 255; g1 = 255; b1 = 0;
                            r2 = 0; g2 = 255; b2 = 255;
                            break;
                    }
                    
                    uint8_t r = (uint8_t)(lerp(r1, r2, t) * alpha);
                    uint8_t g = (uint8_t)(lerp(g1, g2, t) * alpha);
                    uint8_t b = (uint8_t)(lerp(b1, b2, t) * alpha);
                    
                    drawPixel(px, py, r, g, b);
                }
            }
        }
    }
};

// ================================================================
// MASTER SANDBOX CONTROLLER
// Auto-cycles through animations every 5 seconds
// Provides shared GlitchShader that can be applied to any animation
// ================================================================
class SandboxController {
public:
    enum class Animation {
        GYRO_EYES = 0,
        GLITCH_TV = 1,
        SDF_MORPH = 2,
        SHADER_TEST = 3
    };
    
    Animation currentAnim = Animation::GYRO_EYES;
    uint32_t animTimer = 0;
    bool enabled = false;
    
    // Shared glitch shader - can be applied to any animation
    GlitchShader glitchShader;
    bool applyGlitchToAll = false;  // When true, applies glitch shader to all animations
    
    // Animation instances
    GyroEyesAnim gyroEyes;
    GlitchTVAnim glitchTV;
    SDFMorphAnim sdfMorph;
    ShaderTestAnim shaderTest;
    
    // GPU callbacks (set these before use)
    ClearFunc clear;
    BlitSpriteFunc blitSprite;
    BlitSpriteRotatedFunc blitSpriteRotated;
    DrawCircleFFunc drawCircleF;
    FillCircleFunc fillCircle;
    FillRectFunc fillRect;
    DrawLineFunc drawLine;
    DrawPixelFunc drawPixel;
    PresentFunc present;
    
    // Sensor inputs (update these each frame)
    float gyroX = 0, gyroY = 0, gyroZ = 0;
    float audioLevel = 0;
    
    void setEnabled(bool en) { enabled = en; }
    bool isEnabled() const { return enabled; }
    
    void setAnimation(Animation anim) {
        currentAnim = anim;
        animTimer = 0;
    }
    
    // Enable/disable glitch shader overlay on all animations
    void setGlitchEnabled(bool en) { 
        glitchShader.setEnabled(en); 
        applyGlitchToAll = en;
    }
    void setGlitchIntensity(float intensity) { glitchShader.setIntensity(intensity); }
    GlitchShader& getGlitchShader() { return glitchShader; }
    
    void update(uint32_t deltaMs) {
        if (!enabled) return;
        
        animTimer += deltaMs;
        
        // Update glitch shader (always, so it's ready when needed)
        if (applyGlitchToAll || currentAnim == Animation::SHADER_TEST) {
            glitchShader.update(deltaMs);
        }
        
        // DISABLED: Auto-cycle animations (debugging gyro eyes only)
        // if (animTimer >= ANIMATION_DURATION_MS) {
        //     animTimer = 0;
        //     int next = ((int)currentAnim + 1) % NUM_ANIMATIONS;
        //     currentAnim = (Animation)next;
        // }
        
        // Force SHADER_TEST for debugging glitch shader
        currentAnim = Animation::SHADER_TEST;
        
        // Update current animation
        switch (currentAnim) {
            case Animation::GYRO_EYES:
                gyroEyes.update(gyroX, gyroY, gyroZ, deltaMs);
                break;
            case Animation::GLITCH_TV:
                glitchTV.update(deltaMs);
                break;
            case Animation::SDF_MORPH:
                sdfMorph.update(deltaMs);
                break;
            case Animation::SHADER_TEST:
                shaderTest.update(deltaMs);
                break;
        }
    }
    
    void render() {
        if (!enabled || !clear || !fillRect || !present) return;
        
        switch (currentAnim) {
            case Animation::GYRO_EYES:
                renderGyroEyesWithGlitch();
                break;
            case Animation::GLITCH_TV:
                glitchTV.render(fillRect, drawPixel, clear, present);
                break;
            case Animation::SDF_MORPH:
                renderSdfMorphWithGlitch();
                break;
            case Animation::SHADER_TEST:
                shaderTest.render(fillRect, drawPixel, clear, present, &glitchShader);
                break;
        }
    }
    
private:
    // Render gyro eyes with optional glitch shader applied
    void renderGyroEyesWithGlitch() {
        clear(gyroEyes.bgR, gyroEyes.bgG, gyroEyes.bgB);
        
        // Calculate final circle positions
        float leftX = LEFT_EYE_X + gyroEyes.leftCircleX;
        float leftY = gyroEyes.leftCircleY;
        float rightX = RIGHT_EYE_X + gyroEyes.rightCircleX;
        float rightY = gyroEyes.rightCircleY;
        
        if (applyGlitchToAll && glitchShader.enabled) {
            // Draw circles row-by-row with glitch displacement
            int r = gyroEyes.circleRadius;
            
            for (int row = 0; row < DISPLAY_H; row++) {
                int offset = glitchShader.getRowOffset(row);
                
                // Check for color tint override
                uint8_t tr, tg, tb;
                if (glitchShader.getRowTint(row, tr, tg, tb)) {
                    // Full row tint - draw overlay later
                    continue;
                }
                
                // Draw circle scanlines with offset
                // Left eye
                float dy1 = row - leftY;
                if (fabsf(dy1) <= r) {
                    float dx = sqrtf((float)(r * r) - dy1 * dy1);
                    int x1 = (int)(leftX - dx + offset);
                    int x2 = (int)(leftX + dx + offset);
                    if (x1 < 0) x1 = 0;
                    if (x2 >= DISPLAY_W) x2 = DISPLAY_W - 1;
                    if (x1 <= x2) {
                        fillRect(x1, row, x2 - x1 + 1, 1, gyroEyes.eyeR, gyroEyes.eyeG, gyroEyes.eyeB);
                    }
                }
                
                // Right eye
                float dy2 = row - rightY;
                if (fabsf(dy2) <= r) {
                    float dx = sqrtf((float)(r * r) - dy2 * dy2);
                    int x1 = (int)(rightX - dx + offset);
                    int x2 = (int)(rightX + dx + offset);
                    if (x1 < 0) x1 = 0;
                    if (x2 >= DISPLAY_W) x2 = DISPLAY_W - 1;
                    if (x1 <= x2) {
                        fillRect(x1, row, x2 - x1 + 1, 1, gyroEyes.eyeR, gyroEyes.eyeG, gyroEyes.eyeB);
                    }
                }
            }
            
            // Apply glitch overlay
            glitchShader.applyOverlay(fillRect);
        } else {
            // Normal rendering (no glitch)
            if (drawCircleF) {
                drawCircleF(leftX, leftY, (float)gyroEyes.circleRadius, gyroEyes.eyeR, gyroEyes.eyeG, gyroEyes.eyeB);
                drawCircleF(rightX, rightY, (float)gyroEyes.circleRadius, gyroEyes.eyeR, gyroEyes.eyeG, gyroEyes.eyeB);
            } else {
                gyroEyes.drawCircle((int)(leftX + 0.5f), (int)(leftY + 0.5f), gyroEyes.circleRadius, fillRect, drawPixel, gyroEyes.eyeR, gyroEyes.eyeG, gyroEyes.eyeB);
                gyroEyes.drawCircle((int)(rightX + 0.5f), (int)(rightY + 0.5f), gyroEyes.circleRadius, fillRect, drawPixel, gyroEyes.eyeR, gyroEyes.eyeG, gyroEyes.eyeB);
            }
        }
        
        present();
    }
    
    // Render SDF morph with optional glitch shader applied
    void renderSdfMorphWithGlitch() {
        clear(10, 5, 20);
        
        // For now, render normally and just apply overlay
        sdfMorph.renderShape(sdfMorph.leftCenterX, sdfMorph.leftCenterY, drawPixel);
        sdfMorph.renderShape(sdfMorph.rightCenterX, sdfMorph.rightCenterY, drawPixel);
        
        if (applyGlitchToAll && glitchShader.enabled) {
            glitchShader.applyOverlay(fillRect);
        }
        
        present();
    }
};

// Global instance for easy access
inline SandboxController& getSandbox() {
    static SandboxController instance;
    return instance;
}

} // namespace Sandbox
} // namespace AnimationSystem
