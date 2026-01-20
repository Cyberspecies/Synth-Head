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
    
    // Render a single row of the eye circles with horizontal offset (for glitch effects)
    void renderRowWithOffset(int py, int offset, FillRectFunc fillRect) {
        // Calculate final eye positions
        float leftX = LEFT_EYE_X + leftCircleX;
        float leftY = leftCircleY;
        float rightX = RIGHT_EYE_X + rightCircleX;
        float rightY = rightCircleY;
        int r = circleRadius;
        
        // Check if this row intersects left eye circle
        {
            int dy = py - (int)(leftY + 0.5f);
            if (abs(dy) <= r) {
                int x = (int)sqrt(r*r - dy*dy);
                int cx = (int)(leftX + 0.5f);
                int startX = cx - x + offset;
                int endX = cx + x + offset;
                if (startX < 0) startX = 0;
                if (endX >= DISPLAY_W) endX = DISPLAY_W - 1;
                if (startX <= endX) {
                    fillRect(startX, py, endX - startX + 1, 1, eyeR, eyeG, eyeB);
                }
            }
        }
        
        // Check if this row intersects right eye circle
        {
            int dy = py - (int)(rightY + 0.5f);
            if (abs(dy) <= r) {
                int x = (int)sqrt(r*r - dy*dy);
                int cx = (int)(rightX + 0.5f);
                int startX = cx - x + offset;
                int endX = cx + x + offset;
                if (startX < 0) startX = 0;
                if (endX >= DISPLAY_W) endX = DISPLAY_W - 1;
                if (startX <= endX) {
                    fillRect(startX, py, endX - startX + 1, 1, eyeR, eyeG, eyeB);
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
    
    // Intensity control (0.0 = off, 1.0 = full, can go >1 for overdrive)
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
    void setIntensity(float i) { intensity = (i < 0.0f) ? 0.0f : (i > 2.0f) ? 2.0f : i; }  // Allow overdrive to 2.0
    
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
        
        float clampedIntensity = (intensity > 1.0f) ? 1.0f : intensity;
        return (int)(offset * clampedIntensity);
    }
    
    // Get chromatic aberration offset (for RGB channel separation)
    int getChromaOffset() const {
        if (!enabled || intensity < 0.01f) return 0;
        float clampedIntensity = (intensity > 1.0f) ? 1.0f : intensity;
        return (int)(chromaOffset * clampedIntensity);
    }
    
    // Check if a row has an active color tint glitch
    // Returns true and fills r,g,b if there's a tint, false otherwise
    bool getRowTint(int y, uint8_t& r, uint8_t& g, uint8_t& b) const {
        if (!enabled || intensity < 0.01f) return false;
        
        for (int i = 0; i < activeGlitches; i++) {
            const RowGlitch& gl = rowGlitches[i];
            if (gl.colorTint && y >= gl.y && y < gl.y + gl.height) {
                float clampedIntensity = (intensity > 1.0f) ? 1.0f : intensity;
                r = (uint8_t)(gl.r * clampedIntensity);
                g = (uint8_t)(gl.g * clampedIntensity);
                b = (uint8_t)(gl.b * clampedIntensity);
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
        
        // Spawn new row glitches - faster and more intense based on intensity
        // When intensity > 1.0, spawn much more aggressively
        int spawnInterval = (intensity > 1.0f) ? 15 : (30 + (int)((1.0f - intensity) * 60));
        if (glitchTimer > (uint32_t)spawnInterval + (fastRand() % 40)) {
            glitchTimer = 0;
            
            // More glitches when intensity is high
            int maxNew = (intensity > 1.0f) ? 4 + (int)(intensity * 2) : 2 + (int)(intensity * 2);
            int newGlitches = 1 + (fastRand() % maxNew);
            for (int i = 0; i < newGlitches && activeGlitches < MAX_GLITCH_ROWS; i++) {
                RowGlitch& g = rowGlitches[activeGlitches];
                g.y = fastRand() % DISPLAY_H;
                g.height = 1 + (fastRand() % 4);
                // Bigger offsets when intensity > 1
                int maxOffset = (intensity > 1.0f) ? 60 : 40;
                g.offsetX = (fastRand() % maxOffset) - (maxOffset / 2);
                g.duration = 2 + (fastRand() % 8);
                // More color tints at high intensity
                int tintChance = (intensity > 1.0f) ? 50 : (int)(30 * intensity);
                g.colorTint = (fastRand() % 100) < tintChance;
                if (g.colorTint) {
                    int colorChoice = fastRand() % 7;
                    g.r = (colorChoice & 0x04) ? 255 : 30;
                    g.g = (colorChoice & 0x02) ? 255 : 30;
                    g.b = (colorChoice & 0x01) ? 255 : 30;
                }
                activeGlitches++;
            }
            
            // More frequent chroma changes
            if (fastRand() % 2 == 0) {
                chromaOffset = 2 + (fastRand() % 8);
            }
        }
        
        // Update per-row random offsets - more aggressive at high intensity
        rowOffsetTimer += deltaMs;
        int jitterInterval = (intensity > 1.0f) ? 20 : 40;
        if (rowOffsetTimer > (uint32_t)jitterInterval) {
            rowOffsetTimer = 0;
            int numRowsToJitter = (intensity > 1.0f) ? 5 + (fastRand() % 6) : 3 + (fastRand() % 4);
            for (int i = 0; i < numRowsToJitter; i++) {
                int row = fastRand() % DISPLAY_H;
                if (fastRand() % 4 == 0) {
                    rowOffsets[row] = 0;
                } else {
                    int maxJitter = (intensity > 1.0f) ? 6 : 4;
                    rowOffsets[row] = (int8_t)((fastRand() % (maxJitter * 2 + 1)) - maxJitter);
                }
            }
        }
        
        scanlineY = (scanlineY + 2) % DISPLAY_H;  // Faster scanline
    }
    
    // Apply overlay effects AFTER the main scene is drawn
    // This adds scanlines, edge flashes, and color tint bands
    void applyOverlay(FillRectFunc fillRect) {
        if (!enabled || !fillRect || intensity < 0.01f) return;
        
        float clampedIntensity = (intensity > 1.0f) ? 1.0f : intensity;
        
        // Draw color tint bands (overlay on content)
        for (int i = 0; i < activeGlitches; i++) {
            RowGlitch& g = rowGlitches[i];
            if (g.colorTint) {
                fillRect(0, g.y, DISPLAY_W, g.height, 
                        (uint8_t)(g.r * clampedIntensity), 
                        (uint8_t)(g.g * clampedIntensity), 
                        (uint8_t)(g.b * clampedIntensity));
            }
        }
        
        // Scanline effect - thicker at high intensity
        uint8_t scanAlpha = (uint8_t)(255 * clampedIntensity * 0.4f);
        if (scanAlpha > 5) {
            int scanHeight = (intensity > 1.0f) ? 2 : 1;
            fillRect(0, scanlineY, DISPLAY_W, scanHeight, 0, 0, 0);
        }
        
        // More frequent edge flashes at high intensity
        int flashMod = (intensity > 1.0f) ? 15 : 30;
        if (frameCount % flashMod < 3 && clampedIntensity > 0.2f) {
            fillRect(0, 0, DISPLAY_W, 1, 120, 120, 150);
        }
        if (frameCount % (flashMod + 5) < 3 && clampedIntensity > 0.2f) {
            fillRect(0, DISPLAY_H - 1, DISPLAY_W, 1, 120, 120, 150);
        }
        
        // At high intensity, add random full-width flash bars
        if (intensity > 1.2f && (fastRand() % 10) < 3) {
            int flashY = fastRand() % DISPLAY_H;
            fillRect(0, flashY, DISPLAY_W, 1, 200, 200, 255);
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
// PARTICLE TRANSITION SYSTEM - Pixel-based falling particle effect
// Particles fall OUT from current animation, then fall IN to next animation
// ================================================================
struct ParticleTransition {
    // Particle states
    static constexpr int MAX_PARTICLES = 256;  // Max active particles
    static constexpr int GRID_STEP = 2;        // Sample every N pixels (for performance)
    
    struct Particle {
        float x, y;           // Current position
        float targetX, targetY; // Target position (for incoming particles)
        float vx, vy;         // Velocity X and Y
        uint8_t r, g, b;      // Color
        bool active;
        bool incoming;        // true = falling into place, false = falling out
    };
    Particle particles[MAX_PARTICLES];
    int numParticles = 0;
    
    // Framebuffer to capture animation pixels (1 bit per pixel for memory)
    // We'll sample sparsely instead of full framebuffer
    static constexpr int SAMPLE_W = DISPLAY_W / GRID_STEP;
    static constexpr int SAMPLE_H = DISPLAY_H / GRID_STEP;
    uint8_t capturedR[SAMPLE_W * SAMPLE_H];
    uint8_t capturedG[SAMPLE_W * SAMPLE_H];
    uint8_t capturedB[SAMPLE_W * SAMPLE_H];
    bool pixelActive[SAMPLE_W * SAMPLE_H];
    
    // State
    bool initialized = false;
    bool outgoingDone = false;
    float progress = 0.0f;
    
    // Random
    uint32_t seed = 12345;
    uint32_t fastRand() {
        seed = seed * 1103515245 + 12345;
        return (seed >> 16) & 0x7FFF;
    }
    float randFloat() { return (float)(fastRand() % 1000) / 1000.0f; }
    
    void reset() {
        numParticles = 0;
        initialized = false;
        outgoingDone = false;
        progress = 0.0f;
        for (int i = 0; i < SAMPLE_W * SAMPLE_H; i++) {
            pixelActive[i] = false;
        }
    }
    
    // Capture pixels from the current frame (call with drawPixel hooked)
    // We sample a grid pattern for performance
    void capturePixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        // Only capture on grid points
        if (x % GRID_STEP != 0 || y % GRID_STEP != 0) return;
        int sx = x / GRID_STEP;
        int sy = y / GRID_STEP;
        if (sx < 0 || sx >= SAMPLE_W || sy < 0 || sy >= SAMPLE_H) return;
        
        int idx = sy * SAMPLE_W + sx;
        // Only capture if pixel is bright enough (not background)
        if (r > 10 || g > 10 || b > 10) {
            capturedR[idx] = r;
            capturedG[idx] = g;
            capturedB[idx] = b;
            pixelActive[idx] = true;
        }
    }
    
    // Initialize outgoing particles from captured pixels
    void initOutgoing() {
        numParticles = 0;
        
        for (int sy = 0; sy < SAMPLE_H && numParticles < MAX_PARTICLES; sy++) {
            for (int sx = 0; sx < SAMPLE_W && numParticles < MAX_PARTICLES; sx++) {
                int idx = sy * SAMPLE_W + sx;
                if (!pixelActive[idx]) continue;
                
                Particle& p = particles[numParticles];
                p.x = (float)(sx * GRID_STEP);
                p.y = (float)(sy * GRID_STEP);
                p.targetX = p.x;
                p.targetY = DISPLAY_H + 10;  // Fall off screen
                p.vx = 0.0f;
                p.vy = 0.0f;
                p.r = capturedR[idx];
                p.g = capturedG[idx];
                p.b = capturedB[idx];
                p.active = true;
                p.incoming = false;
                numParticles++;
            }
        }
        
        // Randomize velocities - spread outward from center as they fall
        float centerX = DISPLAY_W / 2.0f;
        for (int i = 0; i < numParticles; i++) {
            Particle& p = particles[i];
            // Horizontal spread based on distance from center
            float distFromCenter = (p.x - centerX) / centerX;  // -1 to 1
            p.vx = distFromCenter * 40.0f + (randFloat() - 0.5f) * 30.0f;  // Spread outward + random
            p.vy = -1.0f - randFloat() * 2.0f;  // Start with small upward pop
        }
        
        initialized = true;
        outgoingDone = false;
    }
    
    // Initialize incoming particles (call after capturing new animation)
    void initIncoming() {
        numParticles = 0;
        
        float centerX = DISPLAY_W / 2.0f;
        
        for (int sy = 0; sy < SAMPLE_H && numParticles < MAX_PARTICLES; sy++) {
            for (int sx = 0; sx < SAMPLE_W && numParticles < MAX_PARTICLES; sx++) {
                int idx = sy * SAMPLE_W + sx;
                if (!pixelActive[idx]) continue;
                
                Particle& p = particles[numParticles];
                p.targetX = (float)(sx * GRID_STEP);  // Final X position
                p.targetY = (float)(sy * GRID_STEP);  // Final Y position
                
                // Start spread out from target, above screen
                float distFromCenter = (p.targetX - centerX) / centerX;
                p.x = p.targetX + distFromCenter * 40.0f + (randFloat() - 0.5f) * 20.0f;
                p.y = -10.0f - randFloat() * 40.0f;   // Start above screen, staggered
                
                p.vx = 0.0f;  // Will be calculated each frame to home in
                p.vy = 2.0f + randFloat() * 2.0f;     // Fall down
                p.r = capturedR[idx];
                p.g = capturedG[idx];
                p.b = capturedB[idx];
                p.active = true;
                p.incoming = true;
                numParticles++;
            }
        }
    }
    
    // Clear captured pixels for new capture
    void clearCapture() {
        for (int i = 0; i < SAMPLE_W * SAMPLE_H; i++) {
            pixelActive[i] = false;
        }
    }
    
    // Update particle physics
    void update(uint32_t deltaMs) {
        float dt = deltaMs * 0.001f;  // Convert to seconds
        float gravity = 120.0f;       // Pixels per second^2 (slower)
        
        int stillActive = 0;
        
        for (int i = 0; i < numParticles; i++) {
            Particle& p = particles[i];
            if (!p.active) continue;
            
            if (p.incoming) {
                // Incoming: fall and home in on target X
                p.vy += gravity * dt;
                p.y += p.vy * dt;
                
                // Gradually steer X toward target (homing)
                float dx = p.targetX - p.x;
                p.vx += dx * 8.0f * dt;  // Spring force toward target
                p.vx *= 0.95f;  // Damping
                p.x += p.vx * dt;
                
                // Stop at target Y
                if (p.y >= p.targetY) {
                    p.y = p.targetY;
                    p.x = p.targetX;  // Snap to exact position
                    p.vy = 0;
                    p.vx = 0;
                }
                stillActive++;
            } else {
                // Outgoing: fall down with gravity and spread
                p.vy += gravity * dt;
                p.y += p.vy * dt;
                p.x += p.vx * dt;
                
                // Slight drag on horizontal
                p.vx *= 0.99f;
                
                // Deactivate when off screen
                if (p.y > DISPLAY_H + 5 || p.x < -20 || p.x > DISPLAY_W + 20) {
                    p.active = false;
                } else {
                    stillActive++;
                }
            }
        }
        
        // Check if outgoing phase is done
        if (!outgoingDone && stillActive == 0) {
            outgoingDone = true;
        }
    }
    
    // Check if all outgoing particles have fallen off
    bool isOutgoingComplete() const {
        if (numParticles == 0) return true;
        for (int i = 0; i < numParticles; i++) {
            if (particles[i].active && !particles[i].incoming) return false;
        }
        return true;
    }
    
    // Check if all incoming particles have landed
    bool isIncomingComplete() const {
        for (int i = 0; i < numParticles; i++) {
            if (particles[i].active && particles[i].incoming) {
                if (particles[i].y < particles[i].targetY - 0.5f) return false;
            }
        }
        return true;
    }
    
    // Draw all active particles
    void draw(DrawPixelFunc drawPixel, FillRectFunc fillRect) {
        if (!drawPixel && !fillRect) return;
        
        for (int i = 0; i < numParticles; i++) {
            Particle& p = particles[i];
            if (!p.active) continue;
            
            int px = (int)(p.x + 0.5f);
            int py = (int)(p.y + 0.5f);
            
            if (py >= 0 && py < DISPLAY_H && px >= 0 && px < DISPLAY_W) {
                // Draw as small blocks for visibility
                if (fillRect) {
                    int size = GRID_STEP;
                    if (px + size > DISPLAY_W) size = DISPLAY_W - px;
                    if (py + size > DISPLAY_H) size = DISPLAY_H - py;
                    if (size > 0) {
                        fillRect(px, py, size, size, p.r, p.g, p.b);
                    }
                } else if (drawPixel) {
                    drawPixel(px, py, p.r, p.g, p.b);
                }
            }
        }
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
// ANIMATION 3: SDF MORPH - OPTIMIZED
// Uses scanline rendering with fillRect instead of per-pixel drawPixel
// Binary search to find shape edges, then batch draw horizontal spans
// ================================================================
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

// ================================================================
// MASTER SANDBOX CONTROLLER
// Auto-cycles through animations every 5 seconds
// Provides shared GlitchShader that can be applied to any animation
// Features separate transition types: Glitch OR Particle Dissolve
// ================================================================
class SandboxController {
public:
    enum class Animation {
        GYRO_EYES = 0,
        GLITCH_TV = 1,
        SDF_MORPH = 2,
        SHADER_TEST = 3
    };
    
    // Transition types - cycle through these
    enum class TransitionType {
        GLITCH = 0,      // Glitch effect with row displacement and color bands
        PARTICLE = 1,    // Particle dissolve with falling pixels
        COUNT = 2
    };
    
    Animation currentAnim = Animation::SDF_MORPH;
    Animation nextAnim = Animation::SDF_MORPH;
    uint32_t animTimer = 0;
    bool enabled = false;
    
    // Animation cycling
    static constexpr uint32_t ANIMATION_DURATION_MS = 8000;   // 8 seconds per animation
    static constexpr uint32_t TRANSITION_DURATION_MS = 1500;  // 1.5 second transition
    bool inTransition = false;
    uint32_t transitionTimer = 0;
    float transitionProgress = 0.0f;  // 0 to 1
    
    // Current transition type
    TransitionType currentTransition = TransitionType::GLITCH;
    
    // Glitch intensity for glitch transitions
    float glitchIntensity = 0.0f;
    
    // Shared glitch shader
    GlitchShader glitchShader;
    bool applyGlitchToAll = false;
    
    // Particle transition system
    ParticleTransition particleFX;
    int particlePhase = 0;  // 0 = outgoing, 1 = incoming, 2 = done
    Animation outgoingAnim;  // Remember which anim was outgoing
    
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
        
        // Handle transitions - different behavior based on transition type
        if (inTransition) {
            transitionTimer += deltaMs;
            transitionProgress = (float)transitionTimer / TRANSITION_DURATION_MS;
            if (transitionProgress > 1.0f) transitionProgress = 1.0f;
            
            // Update based on transition type
            if (currentTransition == TransitionType::GLITCH) {
                // Switch animation at 50% progress for glitch
                if (transitionProgress >= 0.5f && currentAnim != nextAnim) {
                    currentAnim = nextAnim;
                }
                
                // Glitch transition: intensity peaks in middle, fades at ends
                float glitchCurve;
                if (transitionProgress < 0.5f) {
                    glitchCurve = transitionProgress * 3.0f;  // peaks at 1.5
                } else {
                    glitchCurve = (1.0f - transitionProgress) * 3.0f;
                }
                glitchIntensity = glitchCurve;
                glitchShader.setIntensity(glitchIntensity);
                glitchShader.update(deltaMs);
            }
            else if (currentTransition == TransitionType::PARTICLE) {
                // Particle transition has 2 phases:
                // Phase 0: Outgoing particles fall off screen (first half)
                // Phase 1: Incoming particles fall into place (second half)
                
                particleFX.update(deltaMs);
                
                // Check phase transitions
                if (particlePhase == 0 && transitionProgress >= 0.5f) {
                    // Switch to incoming phase
                    particlePhase = 1;
                    currentAnim = nextAnim;  // Switch animation
                    
                    // Capture the new animation's pixels
                    particleFX.clearCapture();
                    captureAnimationPixels(currentAnim);
                    particleFX.initIncoming();
                }
            }
            
            // End transition
            if (transitionTimer >= TRANSITION_DURATION_MS) {
                inTransition = false;
                applyGlitchToAll = false;
                glitchShader.setEnabled(false);
                glitchIntensity = 0.0f;
                particleFX.reset();
                particlePhase = 0;
                animTimer = 0;
            }
        }
        // Auto-cycle animations with alternating transition types
        else if (animTimer >= ANIMATION_DURATION_MS) {
            // Start transition to next animation
            int next = ((int)currentAnim + 1);
            // Skip GLITCH_TV (it's just a demo)
            if (next == (int)Animation::GLITCH_TV) next++;
            if (next > (int)Animation::SHADER_TEST) next = (int)Animation::SDF_MORPH;
            
            nextAnim = (Animation)next;
            outgoingAnim = currentAnim;  // Remember for particle capture
            inTransition = true;
            transitionTimer = 0;
            transitionProgress = 0.0f;
            
            // Cycle to next transition type (GLITCH <-> PARTICLE)
            currentTransition = (TransitionType)(((int)currentTransition + 1) % (int)TransitionType::COUNT);
            
            // Setup based on transition type
            if (currentTransition == TransitionType::GLITCH) {
                applyGlitchToAll = true;
                glitchShader.setEnabled(true);
                glitchShader.reset();
                glitchIntensity = 0.0f;
            }
            else if (currentTransition == TransitionType::PARTICLE) {
                // Capture current animation's pixels, then start outgoing
                particleFX.reset();
                particlePhase = 0;
                captureAnimationPixels(currentAnim);
                particleFX.initOutgoing();
            }
        }
        
        // Update glitch shader if active (outside of transition too)
        if (applyGlitchToAll && !inTransition) {
            glitchShader.update(deltaMs);
        }
        
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
        
        // Also update next animation during transition so it's ready
        if (inTransition && nextAnim != currentAnim) {
            switch (nextAnim) {
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
    }
    
    void render() {
        if (!enabled || !clear || !fillRect || !present) return;
        
        // During transition, render based on transition type
        if (inTransition) {
            if (currentTransition == TransitionType::GLITCH) {
                renderGlitchTransition();
            } else if (currentTransition == TransitionType::PARTICLE) {
                renderParticleTransition();
            }
            return;
        }
        
        // Normal rendering
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
    // Determine which animation to render as "outgoing" vs "incoming"
    Animation getOutgoingAnim() const {
        // In first half, current is outgoing; in second half, next was outgoing
        return (transitionProgress < 0.5f) ? currentAnim : nextAnim;
    }
    
    Animation getIncomingAnim() const {
        return (transitionProgress < 0.5f) ? nextAnim : currentAnim;
    }
    
    // GLITCH TRANSITION: Row displacement, color bands, scanlines - no particles
    void renderGlitchTransition() {
        clear(5, 5, 15);  // Dark background
        
        // First half: outgoing animation with increasing glitch
        // Second half: incoming animation with decreasing glitch
        Animation animToRender = (transitionProgress < 0.5f) ? currentAnim : nextAnim;
        
        // Render the current animation with heavy glitch
        renderAnimWithGlitch(animToRender, &glitchShader);
        
        // Apply glitch overlay effects (scanlines, color bands)
        glitchShader.applyOverlay(fillRect);
        
        present();
    }
    
    // Helper to render any animation with glitch shader applied
    void renderAnimWithGlitch(Animation anim, GlitchShader* shader) {
        switch (anim) {
            case Animation::SDF_MORPH:
                renderSdfMorphGlitched(shader);
                break;
            case Animation::SHADER_TEST:
                renderShaderTestGlitched(shader);
                break;
            case Animation::GYRO_EYES:
                renderGyroEyesGlitched(shader);
                break;
            default:
                break;
        }
    }
    
    // Render SDF morph with full glitch (row offsets applied)
    void renderSdfMorphGlitched(GlitchShader* shader) {
        if (!shader) return;
        uint8_t r = sdfMorph.colorR;
        uint8_t g = sdfMorph.colorG;
        uint8_t b = sdfMorph.colorB;
        
        for (int py = 0; py < DISPLAY_H; py++) {
            int offset = shader->getRowOffset(py);
            renderSdfRowWithOffset(sdfMorph.leftCenterX, sdfMorph.leftCenterY, 
                                   sdfMorph.leftRotation, py, offset, r, g, b);
            renderSdfRowWithOffset(sdfMorph.rightCenterX, sdfMorph.rightCenterY, 
                                   sdfMorph.rightRotation, py, offset, r, g, b);
        }
    }
    
    // Render shader test with glitch
    void renderShaderTestGlitched(GlitchShader* shader) {
        // ShaderTest has its own glitch-aware render
        shaderTest.render(fillRect, drawPixel, clear, present, shader);
    }
    
    // Render gyro eyes with glitch
    void renderGyroEyesGlitched(GlitchShader* shader) {
        if (!shader) return;
        // Render eyes row by row with offsets
        for (int py = 0; py < DISPLAY_H; py++) {
            int offset = shader->getRowOffset(py);
            gyroEyes.renderRowWithOffset(py, offset, fillRect);
        }
    }
    
    // Capture pixels from an animation for particle transition
    void captureAnimationPixels(Animation anim) {
        // We need to "render" the animation but capture the pixels instead of displaying
        // Use a temporary capture callback
        switch (anim) {
            case Animation::SDF_MORPH:
                captureSdfMorph();
                break;
            case Animation::SHADER_TEST:
                captureShaderTest();
                break;
            case Animation::GYRO_EYES:
                captureGyroEyes();
                break;
            default:
                break;
        }
    }
    
    // Capture SDF morph pixels
    void captureSdfMorph() {
        uint8_t r = sdfMorph.colorR;
        uint8_t g = sdfMorph.colorG;
        uint8_t b = sdfMorph.colorB;
        
        // Sample at GRID_STEP intervals
        for (int py = 0; py < DISPLAY_H; py += ParticleTransition::GRID_STEP) {
            for (int px = 0; px < DISPLAY_W; px += ParticleTransition::GRID_STEP) {
                // Check if pixel is inside either shape
                if (isPixelInSdfShape(px, py, sdfMorph.leftCenterX, sdfMorph.leftCenterY, sdfMorph.leftRotation) ||
                    isPixelInSdfShape(px, py, sdfMorph.rightCenterX, sdfMorph.rightCenterY, sdfMorph.rightRotation)) {
                    particleFX.capturePixel(px, py, r, g, b);
                }
            }
        }
    }
    
    bool isPixelInSdfShape(int px, int py, float cx, float cy, float rotation) {
        float x = px + 0.5f;
        float y = py + 0.5f;
        float dx = x - cx;
        float dy = y - cy;
        
        float cosA = cosf(-rotation);
        float sinA = sinf(-rotation);
        float rx = dx * cosA - dy * sinA + cx;
        float ry = dx * sinA + dy * cosA + cy;
        
        float sdf1, sdf2;
        switch (sdfMorph.currentShape) {
            case 0: sdf1 = sdfMorph.sdfSquareInline(rx, ry, cx, cy, sdfMorph.shapeSize); 
                    sdf2 = sdfMorph.sdfTriangleInline(rx, ry, cx, cy, sdfMorph.shapeSize); break;
            case 1: sdf1 = sdfMorph.sdfTriangleInline(rx, ry, cx, cy, sdfMorph.shapeSize); 
                    sdf2 = sdfMorph.sdfCircleInline(rx, ry, cx, cy, sdfMorph.shapeSize); break;
            default: sdf1 = sdfMorph.sdfCircleInline(rx, ry, cx, cy, sdfMorph.shapeSize); 
                     sdf2 = sdfMorph.sdfSquareInline(rx, ry, cx, cy, sdfMorph.shapeSize); break;
        }
        float sdf = sdf1 + (sdf2 - sdf1) * sdfMorph.t;
        return sdf < 0.5f;
    }
    
    // Capture shader test pixels - uses white squares
    void captureShaderTest() {
        float leftX = shaderTest.leftPosX + LEFT_EYE_X;
        float leftY = shaderTest.leftPosY;
        float rightX = shaderTest.rightPosX + RIGHT_EYE_X;
        float rightY = shaderTest.rightPosY;
        float size = shaderTest.squareSize;
        
        // ShaderTest uses white (255, 255, 255) for its rotating squares
        uint8_t r = 255, g = 255, b = 255;
        
        for (int py = 0; py < DISPLAY_H; py += ParticleTransition::GRID_STEP) {
            for (int px = 0; px < DISPLAY_W; px += ParticleTransition::GRID_STEP) {
                // Check left square (using SDF for rotation)
                float sdfLeft = shaderTest.sdfRotatedBox((float)px, (float)py, 
                    shaderTest.leftPosX + LEFT_EYE_X, shaderTest.leftPosY, size, shaderTest.leftAngle);
                if (sdfLeft < 0.5f) {
                    particleFX.capturePixel(px, py, r, g, b);
                    continue;
                }
                
                // Check right square
                float sdfRight = shaderTest.sdfRotatedBox((float)px, (float)py,
                    shaderTest.rightPosX + RIGHT_EYE_X, shaderTest.rightPosY, size, shaderTest.rightAngle);
                if (sdfRight < 0.5f) {
                    particleFX.capturePixel(px, py, r, g, b);
                }
            }
        }
    }
    
    // Capture gyro eyes pixels
    void captureGyroEyes() {
        float leftX = LEFT_EYE_X + gyroEyes.leftCircleX;
        float leftY = gyroEyes.leftCircleY;
        float rightX = RIGHT_EYE_X + gyroEyes.rightCircleX;
        float rightY = gyroEyes.rightCircleY;
        int r = gyroEyes.circleRadius;
        
        for (int py = 0; py < DISPLAY_H; py += ParticleTransition::GRID_STEP) {
            for (int px = 0; px < DISPLAY_W; px += ParticleTransition::GRID_STEP) {
                float dx1 = px - leftX;
                float dy1 = py - leftY;
                float dx2 = px - rightX;
                float dy2 = py - rightY;
                
                if ((dx1*dx1 + dy1*dy1) <= r*r || (dx2*dx2 + dy2*dy2) <= r*r) {
                    particleFX.capturePixel(px, py, gyroEyes.eyeR, gyroEyes.eyeG, gyroEyes.eyeB);
                }
            }
        }
    }
    
    // PARTICLE TRANSITION: Particles fall out, then fall in
    void renderParticleTransition() {
        clear(0, 0, 0);  // Black background
        
        // Just draw all the particles
        particleFX.draw(drawPixel, fillRect);
        
        present();
    }
    
    // Render a single row of SDF shape with offset
    void renderSdfRowWithOffset(float cx, float cy, float rotation, int py, int offset, 
                                 uint8_t r, uint8_t g, uint8_t b) {
        float y = py + 0.5f;
        float dy = y - cy;
        if (fabsf(dy) > sdfMorph.shapeSize + 2) return;  // Quick bounds check
        
        float cosA = cosf(-rotation);
        float sinA = sinf(-rotation);
        
        int scanLeft = (int)cx - (int)sdfMorph.shapeSize - 2;
        int scanRight = (int)cx + (int)sdfMorph.shapeSize + 2;
        if (scanLeft < 0) scanLeft = 0;
        if (scanRight >= DISPLAY_W) scanRight = DISPLAY_W - 1;
        
        int leftEdge = -1, rightEdge = -1;
        
        // Find edges
        for (int px = scanLeft; px <= scanRight; px++) {
            float dx = px + 0.5f - cx;
            float rx = dx * cosA - dy * sinA + cx;
            float ry = dx * sinA + dy * cosA + cy;
            
            float sdf1, sdf2;
            switch (sdfMorph.currentShape) {
                case 0: sdf1 = sdfMorph.sdfSquareInline(rx, ry, cx, cy, sdfMorph.shapeSize); 
                        sdf2 = sdfMorph.sdfTriangleInline(rx, ry, cx, cy, sdfMorph.shapeSize); break;
                case 1: sdf1 = sdfMorph.sdfTriangleInline(rx, ry, cx, cy, sdfMorph.shapeSize); 
                        sdf2 = sdfMorph.sdfCircleInline(rx, ry, cx, cy, sdfMorph.shapeSize); break;
                default: sdf1 = sdfMorph.sdfCircleInline(rx, ry, cx, cy, sdfMorph.shapeSize); 
                         sdf2 = sdfMorph.sdfSquareInline(rx, ry, cx, cy, sdfMorph.shapeSize); break;
            }
            float sdf = sdf1 + (sdf2 - sdf1) * sdfMorph.t;
            
            if (sdf < 0.5f) {
                if (leftEdge < 0) leftEdge = px;
                rightEdge = px;
            }
        }
        
        if (leftEdge >= 0 && rightEdge >= leftEdge) {
            int drawX = leftEdge + offset;
            int width = rightEdge - leftEdge + 1;
            if (drawX < 0) { width += drawX; drawX = 0; }
            if (drawX + width > DISPLAY_W) width = DISPLAY_W - drawX;
            if (width > 0) {
                fillRect(drawX, py, width, 1, r, g, b);
            }
        }
    }
    
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
        sdfMorph.renderShapeFast(sdfMorph.leftCenterX, sdfMorph.leftCenterY, sdfMorph.leftRotation, fillRect);
        sdfMorph.renderShapeFast(sdfMorph.rightCenterX, sdfMorph.rightCenterY, sdfMorph.rightRotation, fillRect);
        
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
