#pragma once

#include "../AnimationTypes.hpp"
#include "../Shaders/GlitchShader.hpp"

namespace AnimationSystem {
namespace Animations {

// ================================================================
// COMPLEX TRANSITION ANIMATION
// Multi-stage animation that demonstrates all effects:
// 1. Rotating square (SDF)
// 2. Glitch transition to triangle
// 3. SDF morph to circle
// 4. Particle dissolution with accelerometer physics
// 5. Square drops from sky to restart
// ================================================================
struct ComplexTransitionAnim {
    enum class Stage {
        SQUARE_ROTATE,      // Stage 0: Rotating square
        GLITCH_TO_TRIANGLE, // Stage 1: Glitching square->triangle
        TRIANGLE_HOLD,      // Stage 2: Triangle rotating
        MORPH_TO_CIRCLE,    // Stage 3: Triangle->circle SDF morph
        CIRCLE_HOLD,        // Stage 4: Circle stable
        DISSOLVE,           // Stage 5: Circle dissolves into sand
        SAND_FALL,          // Stage 6: Sand particles falling
        SQUARE_DROP         // Stage 7: New square drops from sky
    };
    
    Stage currentStage = Stage::SQUARE_ROTATE;
    float stageTime = 0.0f;
    float rotation = 0.0f;
    float morphT = 0.0f;  // Interpolation between shapes
    
    // Shape parameters
    float leftCenterX = 32.0f, leftCenterY = 16.0f;
    float rightCenterX = 96.0f, rightCenterY = 16.0f;
    float shapeSize = 12.0f;
    float rotationSpeed = 0.3f;  // Slow rotation
    
    // Color
    uint8_t colorR = 100, colorG = 200, colorB = 255;
    
    // Glitch shader for transitions
    Shaders::GlitchShader glitchShader;
    
    // Sand particle system - optimized with spatial grid
    static constexpr int MAX_PARTICLES = 256;
    static constexpr int GRID_SIZE = 8;
    static constexpr int GRID_CELLS = (DISPLAY_W / GRID_SIZE) * (DISPLAY_H / GRID_SIZE);
    
    struct Particle {
        float x, y;
        float vx, vy;
        uint8_t r, g, b;
        bool active;
    };
    
    Particle particles[MAX_PARTICLES];
    int activeParticles = 0;
    int particleGrid[GRID_CELLS];  // Head of linked list for each cell
    int particleNext[MAX_PARTICLES];  // Next particle in grid cell
    
    // Accelerometer gravity vector
    float gravityX = 0.0f;
    float gravityY = 1.0f;  // Default downward
    float gravityScale = 100.0f;
    
    // Drop square state
    float dropSquareY = -20.0f;
    float dropSquareVY = 0.0f;
    
    // Timing
    float squareRotateTime = 3.0f;
    float glitchTransitionTime = 1.5f;
    float triangleHoldTime = 3.0f;
    float morphTime = 2.0f;
    float circleHoldTime = 2.0f;
    float dissolveTime = 2.0f;
    float sandFallTime = 3.0f;
    float squareDropTime = 1.5f;
    
    uint32_t randomSeed = 54321;
    
    uint32_t fastRand() {
        randomSeed = randomSeed * 1103515245 + 12345;
        return (randomSeed >> 16) & 0x7FFF;
    }
    
    // Inline SDF functions
    inline float sdfSquare(float x, float y, float cx, float cy, float size, float rot) {
        float dx = x - cx;
        float dy = y - cy;
        float cosA = cosf(-rot);
        float sinA = sinf(-rot);
        float rx = dx * cosA - dy * sinA;
        float ry = dx * sinA + dy * cosA;
        return fmaxf(fabsf(rx) - size, fabsf(ry) - size);
    }
    
    inline float sdfTriangle(float x, float y, float cx, float cy, float size, float rot) {
        float dx = x - cx;
        float dy = y - cy;
        float cosA = cosf(-rot);
        float sinA = sinf(-rot);
        float rx = dx * cosA - dy * sinA;
        float ry = dx * sinA + dy * cosA;
        
        float px = fabsf(rx);
        float py = ry + size * 0.5f;
        float edge = py + px * 1.732f - size * 1.732f;
        return fmaxf(edge / 2.0f, -py - size * 0.5f);
    }
    
    inline float sdfCircle(float x, float y, float cx, float cy, float radius) {
        float dx = x - cx;
        float dy = y - cy;
        return sqrtf(dx * dx + dy * dy) - radius;
    }
    
    void update(uint32_t deltaMs, float accelX, float accelY, float accelZ) {
        float deltaSeconds = deltaMs * 0.001f;
        stageTime += deltaSeconds;
        
        // Update gravity from accelerometer (simple vector sum)
        // Accel measures opposite of gravity, so negate
        gravityX = -accelX * 0.1f + gravityX * 0.9f;  // Smooth filtering
        gravityY = accelY * 0.1f + gravityY * 0.9f;   // Note: Y is flipped in display coords
        
        // Stage-specific updates
        switch (currentStage) {
            case Stage::SQUARE_ROTATE:
                rotation += rotationSpeed * deltaSeconds;
                if (stageTime > squareRotateTime) {
                    stageTime = 0.0f;
                    currentStage = Stage::GLITCH_TO_TRIANGLE;
                    glitchShader.setEnabled(true);
                    glitchShader.setIntensity(1.0f);
                }
                break;
                
            case Stage::GLITCH_TO_TRIANGLE:
                rotation += rotationSpeed * deltaSeconds;
                glitchShader.update(deltaMs);
                morphT = stageTime / glitchTransitionTime;
                if (morphT > 1.0f) morphT = 1.0f;
                if (stageTime > glitchTransitionTime) {
                    stageTime = 0.0f;
                    currentStage = Stage::TRIANGLE_HOLD;
                    glitchShader.setEnabled(false);
                    morphT = 1.0f;
                }
                break;
                
            case Stage::TRIANGLE_HOLD:
                rotation += rotationSpeed * deltaSeconds;
                if (stageTime > triangleHoldTime) {
                    stageTime = 0.0f;
                    currentStage = Stage::MORPH_TO_CIRCLE;
                    morphT = 0.0f;
                }
                break;
                
            case Stage::MORPH_TO_CIRCLE:
                rotation += rotationSpeed * deltaSeconds;
                morphT = stageTime / morphTime;
                if (morphT > 1.0f) morphT = 1.0f;
                if (stageTime > morphTime) {
                    stageTime = 0.0f;
                    currentStage = Stage::CIRCLE_HOLD;
                    morphT = 1.0f;
                }
                break;
                
            case Stage::CIRCLE_HOLD:
                rotation += rotationSpeed * deltaSeconds * 0.5f;  // Slow down
                if (stageTime > circleHoldTime) {
                    stageTime = 0.0f;
                    currentStage = Stage::DISSOLVE;
                    initializeParticles();
                }
                break;
                
            case Stage::DISSOLVE:
                updateDissolve(deltaSeconds);
                if (stageTime > dissolveTime) {
                    stageTime = 0.0f;
                    currentStage = Stage::SAND_FALL;
                }
                break;
                
            case Stage::SAND_FALL:
                updateSandPhysics(deltaSeconds);
                if (stageTime > sandFallTime) {
                    stageTime = 0.0f;
                    currentStage = Stage::SQUARE_DROP;
                    dropSquareY = -20.0f;
                    dropSquareVY = 0.0f;
                }
                break;
                
            case Stage::SQUARE_DROP:
                dropSquareY += dropSquareVY * deltaSeconds;
                dropSquareVY += gravityY * gravityScale * deltaSeconds;
                if (dropSquareY >= 16.0f) {
                    dropSquareY = 16.0f;
                    stageTime = 0.0f;
                    currentStage = Stage::SQUARE_ROTATE;
                    rotation = 0.0f;
                }
                break;
        }
    }
    
    void initializeParticles() {
        activeParticles = 0;
        
        // Create particles in a circle pattern
        float radius = shapeSize;
        int numRings = 6;
        int particlesPerRing = 12;
        
        for (int ring = numRings - 1; ring >= 0 && activeParticles < MAX_PARTICLES; ring--) {
            float r = radius * ((float)ring / numRings);
            int count = particlesPerRing * (ring + 1) / numRings;
            if (count < 4) count = 4;
            
            for (int i = 0; i < count && activeParticles < MAX_PARTICLES; i++) {
                float angle = (float)i / count * 6.283185f + (fastRand() % 100) * 0.01f;
                float dist = r + (fastRand() % 20) * 0.05f;
                
                Particle& p = particles[activeParticles];
                p.x = leftCenterX + cosf(angle) * dist;
                p.y = leftCenterY + sinf(angle) * dist;
                p.vx = cosf(angle) * 5.0f + (fastRand() % 40 - 20) * 0.1f;
                p.vy = sinf(angle) * 5.0f + (fastRand() % 40 - 20) * 0.1f;
                p.r = colorR;
                p.g = colorG;
                p.b = colorB;
                p.active = true;
                activeParticles++;
                
                // Right eye
                if (activeParticles < MAX_PARTICLES) {
                    Particle& p2 = particles[activeParticles];
                    p2.x = rightCenterX + cosf(angle) * dist;
                    p2.y = rightCenterY + sinf(angle) * dist;
                    p2.vx = p.vx;
                    p2.vy = p.vy;
                    p2.r = colorR;
                    p2.g = colorG;
                    p2.b = colorB;
                    p2.active = true;
                    activeParticles++;
                }
            }
        }
    }
    
    void updateDissolve(float dt) {
        // Expand particles outward from circle edge
        for (int i = 0; i < activeParticles; i++) {
            Particle& p = particles[i];
            if (!p.active) continue;
            
            p.vx += gravityX * gravityScale * dt * 0.1f;
            p.vy += gravityY * gravityScale * dt * 0.1f;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
        }
    }
    
    void updateSandPhysics(float dt) {
        // Clear spatial grid
        for (int i = 0; i < GRID_CELLS; i++) {
            particleGrid[i] = -1;
        }
        
        // Update particles and build spatial grid
        for (int i = 0; i < activeParticles; i++) {
            Particle& p = particles[i];
            if (!p.active) continue;
            
            // Apply gravity
            p.vx += gravityX * gravityScale * dt;
            p.vy += gravityY * gravityScale * dt;
            
            // Apply drag
            p.vx *= 0.98f;
            p.vy *= 0.98f;
            
            // Update position
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            
            // Bounce off walls
            if (p.x < 0) { p.x = 0; p.vx = -p.vx * 0.5f; }
            if (p.x >= DISPLAY_W) { p.x = DISPLAY_W - 1; p.vx = -p.vx * 0.5f; }
            if (p.y < 0) { p.y = 0; p.vy = -p.vy * 0.5f; }
            if (p.y >= DISPLAY_H) { p.y = DISPLAY_H - 1; p.vy = -p.vy * 0.5f; }
            
            // Add to spatial grid
            int gridX = (int)(p.x / GRID_SIZE);
            int gridY = (int)(p.y / GRID_SIZE);
            if (gridX >= 0 && gridX < DISPLAY_W / GRID_SIZE && 
                gridY >= 0 && gridY < DISPLAY_H / GRID_SIZE) {
                int cellIndex = gridY * (DISPLAY_W / GRID_SIZE) + gridX;
                particleNext[i] = particleGrid[cellIndex];
                particleGrid[cellIndex] = i;
            }
        }
    }
    
    void render(FillRectFunc fillRect, DrawPixelFunc drawPixel, ClearFunc clear, PresentFunc present) {
        clear(5, 5, 10);
        
        switch (currentStage) {
            case Stage::SQUARE_ROTATE:
                renderShape(leftCenterX, leftCenterY, 0, rotation, fillRect);
                renderShape(rightCenterX, rightCenterY, 0, -rotation, fillRect);
                break;
                
            case Stage::GLITCH_TO_TRIANGLE:
                renderShape(leftCenterX, leftCenterY, morphT, rotation, fillRect);
                renderShape(rightCenterX, rightCenterY, morphT, -rotation, fillRect);
                if (glitchShader.enabled) {
                    glitchShader.applyOverlay(fillRect);
                }
                break;
                
            case Stage::TRIANGLE_HOLD:
                renderShape(leftCenterX, leftCenterY, 1.0f, rotation, fillRect);
                renderShape(rightCenterX, rightCenterY, 1.0f, -rotation, fillRect);
                break;
                
            case Stage::MORPH_TO_CIRCLE:
                renderShapeToCircle(leftCenterX, leftCenterY, morphT, rotation, fillRect);
                renderShapeToCircle(rightCenterX, rightCenterY, morphT, -rotation, fillRect);
                break;
                
            case Stage::CIRCLE_HOLD:
                renderCircle(leftCenterX, leftCenterY, fillRect);
                renderCircle(rightCenterX, rightCenterY, fillRect);
                break;
                
            case Stage::DISSOLVE:
            case Stage::SAND_FALL:
                renderParticles(drawPixel);
                break;
                
            case Stage::SQUARE_DROP:
                // Draw falling square
                renderShape(leftCenterX, dropSquareY, 0, rotation, fillRect);
                renderShape(rightCenterX, dropSquareY, 0, -rotation, fillRect);
                break;
        }
        
        present();
    }
    
    void renderShape(float cx, float cy, float triangleT, float rot, FillRectFunc fillRect) {
        if (!fillRect) return;
        
        int margin = (int)shapeSize + 3;
        int startY = (int)cy - margin;
        int endY = (int)cy + margin;
        
        if (startY < 0) startY = 0;
        if (endY >= DISPLAY_H) endY = DISPLAY_H - 1;
        
        // Smoothstep interpolation
        float t = triangleT * triangleT * (3.0f - 2.0f * triangleT);
        
        for (int py = startY; py <= endY; py++) {
            int leftEdge = -1, rightEdge = -1;
            
            int scanLeft = (int)cx - margin;
            int scanRight = (int)cx + margin;
            if (scanLeft < 0) scanLeft = 0;
            if (scanRight >= DISPLAY_W) scanRight = DISPLAY_W - 1;
            
            for (int px = scanLeft; px <= scanRight; px++) {
                float sdfSq = sdfSquare(px + 0.5f, py + 0.5f, cx, cy, shapeSize, rot);
                float sdfTri = sdfTriangle(px + 0.5f, py + 0.5f, cx, cy, shapeSize, rot);
                float sdf = sdfSq + (sdfTri - sdfSq) * t;
                
                if (sdf < 0.5f) {
                    if (leftEdge < 0) leftEdge = px;
                    rightEdge = px;
                }
            }
            
            if (leftEdge >= 0 && rightEdge >= leftEdge) {
                int width = rightEdge - leftEdge + 1;
                fillRect(leftEdge, py, width, 1, colorR, colorG, colorB);
            }
        }
    }
    
    void renderShapeToCircle(float cx, float cy, float circleT, float rot, FillRectFunc fillRect) {
        if (!fillRect) return;
        
        int margin = (int)shapeSize + 3;
        int startY = (int)cy - margin;
        int endY = (int)cy + margin;
        
        if (startY < 0) startY = 0;
        if (endY >= DISPLAY_H) endY = DISPLAY_H - 1;
        
        // Smoothstep
        float t = circleT * circleT * (3.0f - 2.0f * circleT);
        
        for (int py = startY; py <= endY; py++) {
            int leftEdge = -1, rightEdge = -1;
            
            int scanLeft = (int)cx - margin;
            int scanRight = (int)cx + margin;
            if (scanLeft < 0) scanLeft = 0;
            if (scanRight >= DISPLAY_W) scanRight = DISPLAY_W - 1;
            
            for (int px = scanLeft; px <= scanRight; px++) {
                float sdfTri = sdfTriangle(px + 0.5f, py + 0.5f, cx, cy, shapeSize, rot);
                float sdfCir = sdfCircle(px + 0.5f, py + 0.5f, cx, cy, shapeSize);
                float sdf = sdfTri + (sdfCir - sdfTri) * t;
                
                if (sdf < 0.5f) {
                    if (leftEdge < 0) leftEdge = px;
                    rightEdge = px;
                }
            }
            
            if (leftEdge >= 0 && rightEdge >= leftEdge) {
                int width = rightEdge - leftEdge + 1;
                fillRect(leftEdge, py, width, 1, colorR, colorG, colorB);
            }
        }
    }
    
    void renderCircle(float cx, float cy, FillRectFunc fillRect) {
        if (!fillRect) return;
        
        int r = (int)shapeSize;
        for (int py = -r; py <= r; py++) {
            int y = (int)cy + py;
            if (y < 0 || y >= DISPLAY_H) continue;
            
            float dy = py;
            float dx = sqrtf((float)(r * r) - dy * dy);
            int x1 = (int)(cx - dx);
            int x2 = (int)(cx + dx);
            
            if (x1 < 0) x1 = 0;
            if (x2 >= DISPLAY_W) x2 = DISPLAY_W - 1;
            
            if (x1 <= x2) {
                fillRect(x1, y, x2 - x1 + 1, 1, colorR, colorG, colorB);
            }
        }
    }
    
    void renderParticles(DrawPixelFunc drawPixel) {
        if (!drawPixel) return;
        
        for (int i = 0; i < activeParticles; i++) {
            const Particle& p = particles[i];
            if (!p.active) continue;
            
            int px = (int)(p.x + 0.5f);
            int py = (int)(p.y + 0.5f);
            
            if (px >= 0 && px < DISPLAY_W && py >= 0 && py < DISPLAY_H) {
                drawPixel(px, py, p.r, p.g, p.b);
            }
        }
    }
};

} // namespace Animations
} // namespace AnimationSystem
