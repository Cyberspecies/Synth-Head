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
// ANIMATION 2: GLITCH TV
// Chromatic aberration, static, scan lines, retro glitch
// ================================================================
struct GlitchTVAnim {
    uint32_t frameCount = 0;
    uint32_t glitchTimer = 0;
    int glitchOffsetX = 0;
    int glitchOffsetY = 0;
    int chromaOffset = 2;
    bool heavyGlitch = false;
    int scanlineY = 0;
    
    // Static noise seed
    uint32_t noiseSeed = 12345;
    
    uint32_t fastRand() {
        noiseSeed = noiseSeed * 1103515245 + 12345;
        return (noiseSeed >> 16) & 0x7FFF;
    }
    
    void update(uint32_t deltaMs) {
        frameCount++;
        glitchTimer += deltaMs;
        
        // Random glitch events
        if (glitchTimer > 100 + (fastRand() % 200)) {
            glitchTimer = 0;
            glitchOffsetX = (fastRand() % 8) - 4;
            glitchOffsetY = (fastRand() % 4) - 2;
            heavyGlitch = (fastRand() % 10) < 2;  // 20% chance
            chromaOffset = 1 + (fastRand() % 4);
        }
        
        // Scanline
        scanlineY = (scanlineY + 1) % DISPLAY_H;
    }
    
    void render(FillRectFunc fillRect, DrawPixelFunc drawPixel, ClearFunc clear, PresentFunc present) {
        // Dark background with slight color
        clear(5, 5, 10);
        
        // Draw some "content" that gets glitched
        // Simple test pattern - rectangles
        int baseX = 20 + glitchOffsetX;
        int baseY = 8 + glitchOffsetY;
        
        // Chromatic aberration - offset R, G, B channels
        // Red channel (offset left)
        fillRect(baseX - chromaOffset, baseY, 24, 16, 180, 0, 0);
        fillRect(baseX - chromaOffset + 70, baseY, 24, 16, 180, 0, 0);
        
        // Blue channel (offset right)  
        fillRect(baseX + chromaOffset, baseY, 24, 16, 0, 0, 180);
        fillRect(baseX + chromaOffset + 70, baseY, 24, 16, 0, 0, 180);
        
        // Green channel (center)
        fillRect(baseX, baseY, 24, 16, 0, 180, 0);
        fillRect(baseX + 70, baseY, 24, 16, 0, 180, 0);
        
        // Static noise overlay
        for (int i = 0; i < 50; i++) {
            int x = fastRand() % DISPLAY_W;
            int y = fastRand() % DISPLAY_H;
            uint8_t v = 50 + (fastRand() % 100);
            if (drawPixel) drawPixel(x, y, v, v, v);
        }
        
        // Horizontal glitch lines
        if (heavyGlitch) {
            for (int i = 0; i < 3; i++) {
                int y = fastRand() % DISPLAY_H;
                int shift = (fastRand() % 20) - 10;
                fillRect(shift < 0 ? 0 : shift, y, DISPLAY_W, 1 + (fastRand() % 2), 
                        fastRand() % 255, fastRand() % 255, fastRand() % 255);
            }
        }
        
        // Scanline effect (dark line moving down)
        fillRect(0, scanlineY, DISPLAY_W, 1, 0, 0, 0);
        if (scanlineY > 0) {
            fillRect(0, scanlineY - 1, DISPLAY_W, 1, 20, 20, 30);
        }
        
        // VHS tracking lines at edges
        if (frameCount % 30 < 5) {
            fillRect(0, 0, DISPLAY_W, 2, 40, 40, 50);
            fillRect(0, DISPLAY_H - 2, DISPLAY_W, 2, 40, 40, 50);
        }
        
        present();
    }
};

// ================================================================
// ANIMATION 3: SDF MORPH
// Square → Triangle → Circle using SDF and weighted pixels
// Transitions smoothly between shapes
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
// ================================================================
class SandboxController {
public:
    enum class Animation {
        GYRO_EYES = 0,
        GLITCH_TV = 1,
        SDF_MORPH = 2
    };
    
    Animation currentAnim = Animation::GYRO_EYES;
    uint32_t animTimer = 0;
    bool enabled = false;
    
    // Animation instances
    GyroEyesAnim gyroEyes;
    GlitchTVAnim glitchTV;
    SDFMorphAnim sdfMorph;
    
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
    
    void update(uint32_t deltaMs) {
        if (!enabled) return;
        
        animTimer += deltaMs;
        
        // DISABLED: Auto-cycle animations (debugging gyro eyes only)
        // if (animTimer >= ANIMATION_DURATION_MS) {
        //     animTimer = 0;
        //     int next = ((int)currentAnim + 1) % NUM_ANIMATIONS;
        //     currentAnim = (Animation)next;
        // }
        
        // Force GYRO_EYES for debugging
        currentAnim = Animation::GYRO_EYES;
        
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
        }
    }
    
    void render() {
        if (!enabled || !clear || !fillRect || !present) return;
        
        switch (currentAnim) {
            case Animation::GYRO_EYES:
                gyroEyes.render(fillRect, drawPixel, clear, present, drawCircleF);
                break;
            case Animation::GLITCH_TV:
                glitchTV.render(fillRect, drawPixel, clear, present);
                break;
            case Animation::SDF_MORPH:
                sdfMorph.render(fillRect, drawPixel, clear, present);
                break;
        }
    }
};

// Global instance for easy access
inline SandboxController& getSandbox() {
    static SandboxController instance;
    return instance;
}

} // namespace Sandbox
} // namespace AnimationSystem
