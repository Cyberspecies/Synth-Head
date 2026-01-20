/*****************************************************************
 * @file GyroEyesAnim.hpp
 * @brief Gyroscope-controlled Eye Animation
 * 
 * Two circular "eyes" that respond to gyroscope input.
 * Uses rolling window average for smooth motion.
 * Supports row-based rendering for glitch shader compatibility.
 * 
 * Usage:
 *   1. Call update() each frame with gyro data
 *   2. Call render() to draw the eyes
 *   3. Use renderRowWithOffset() for glitch-aware rendering
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "../AnimationTypes.hpp"
#include <cmath>

namespace AnimationSystem {
namespace Animations {

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

} // namespace Animations
} // namespace AnimationSystem
