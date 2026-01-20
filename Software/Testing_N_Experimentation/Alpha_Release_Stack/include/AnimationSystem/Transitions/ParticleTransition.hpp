/*****************************************************************
 * @file ParticleTransition.hpp
 * @brief Particle-based Transition Effect
 * 
 * Particles fall OUT from current animation, then fall IN to next animation.
 * Creates a visually satisfying dissolve/reform effect.
 * 
 * Usage:
 *   1. Call capturePixel() to capture the outgoing animation's pixels
 *   2. Call initOutgoing() to start the particles falling
 *   3. Call update() each frame to simulate physics
 *   4. When switching animations, clearCapture(), capture new animation,
 *      then initIncoming() to have particles fall into place
 *   5. Call draw() to render particles
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "../AnimationTypes.hpp"
#include <cstdint>

namespace AnimationSystem {
namespace Transitions {

struct ParticleTransition {
    // Particle states
    static constexpr int MAX_PARTICLES = 256;  // Max active particles
    static constexpr int GRID_STEP = 2;        // Sample every N pixels (for performance)
    
    struct Particle {
        float x, y;             // Current position
        float targetX, targetY; // Target position (for incoming particles)
        float vx, vy;           // Velocity X and Y
        uint8_t r, g, b;        // Color
        bool active;
        bool incoming;          // true = falling into place, false = falling out
    };
    Particle particles[MAX_PARTICLES];
    int numParticles = 0;
    
    // Framebuffer to capture animation pixels
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
    
    // Capture pixels from the current frame
    void capturePixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x % GRID_STEP != 0 || y % GRID_STEP != 0) return;
        int sx = x / GRID_STEP;
        int sy = y / GRID_STEP;
        if (sx < 0 || sx >= SAMPLE_W || sy < 0 || sy >= SAMPLE_H) return;
        
        int idx = sy * SAMPLE_W + sx;
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
                p.targetY = DISPLAY_H + 10;
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
        
        // Spread outward from center as they fall
        float centerX = DISPLAY_W / 2.0f;
        for (int i = 0; i < numParticles; i++) {
            Particle& p = particles[i];
            float distFromCenter = (p.x - centerX) / centerX;
            p.vx = distFromCenter * 40.0f + (randFloat() - 0.5f) * 30.0f;
            p.vy = -1.0f - randFloat() * 2.0f;
        }
        
        initialized = true;
        outgoingDone = false;
    }
    
    // Initialize incoming particles
    void initIncoming() {
        numParticles = 0;
        
        float centerX = DISPLAY_W / 2.0f;
        
        for (int sy = 0; sy < SAMPLE_H && numParticles < MAX_PARTICLES; sy++) {
            for (int sx = 0; sx < SAMPLE_W && numParticles < MAX_PARTICLES; sx++) {
                int idx = sy * SAMPLE_W + sx;
                if (!pixelActive[idx]) continue;
                
                Particle& p = particles[numParticles];
                p.targetX = (float)(sx * GRID_STEP);
                p.targetY = (float)(sy * GRID_STEP);
                
                float distFromCenter = (p.targetX - centerX) / centerX;
                p.x = p.targetX + distFromCenter * 40.0f + (randFloat() - 0.5f) * 20.0f;
                p.y = -10.0f - randFloat() * 40.0f;
                
                p.vx = 0.0f;
                p.vy = 2.0f + randFloat() * 2.0f;
                p.r = capturedR[idx];
                p.g = capturedG[idx];
                p.b = capturedB[idx];
                p.active = true;
                p.incoming = true;
                numParticles++;
            }
        }
    }
    
    void clearCapture() {
        for (int i = 0; i < SAMPLE_W * SAMPLE_H; i++) {
            pixelActive[i] = false;
        }
    }
    
    void update(uint32_t deltaMs) {
        float dt = deltaMs * 0.001f;
        float gravity = 120.0f;
        
        int stillActive = 0;
        
        for (int i = 0; i < numParticles; i++) {
            Particle& p = particles[i];
            if (!p.active) continue;
            
            if (p.incoming) {
                p.vy += gravity * dt;
                p.y += p.vy * dt;
                
                float dx = p.targetX - p.x;
                p.vx += dx * 8.0f * dt;
                p.vx *= 0.95f;
                p.x += p.vx * dt;
                
                if (p.y >= p.targetY) {
                    p.y = p.targetY;
                    p.x = p.targetX;
                    p.vy = 0;
                    p.vx = 0;
                }
                stillActive++;
            } else {
                p.vy += gravity * dt;
                p.y += p.vy * dt;
                p.x += p.vx * dt;
                p.vx *= 0.99f;
                
                if (p.y > DISPLAY_H + 5 || p.x < -20 || p.x > DISPLAY_W + 20) {
                    p.active = false;
                } else {
                    stillActive++;
                }
            }
        }
        
        if (!outgoingDone && stillActive == 0) {
            outgoingDone = true;
        }
    }
    
    bool isOutgoingComplete() const {
        if (numParticles == 0) return true;
        for (int i = 0; i < numParticles; i++) {
            if (particles[i].active && !particles[i].incoming) return false;
        }
        return true;
    }
    
    bool isIncomingComplete() const {
        for (int i = 0; i < numParticles; i++) {
            if (particles[i].active && particles[i].incoming) {
                if (particles[i].y < particles[i].targetY - 0.5f) return false;
            }
        }
        return true;
    }
    
    void draw(DrawPixelFunc drawPixel, FillRectFunc fillRect) {
        if (!drawPixel && !fillRect) return;
        
        for (int i = 0; i < numParticles; i++) {
            Particle& p = particles[i];
            if (!p.active) continue;
            
            int px = (int)(p.x + 0.5f);
            int py = (int)(p.y + 0.5f);
            
            if (py >= 0 && py < DISPLAY_H && px >= 0 && px < DISPLAY_W) {
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

} // namespace Transitions
} // namespace AnimationSystem
