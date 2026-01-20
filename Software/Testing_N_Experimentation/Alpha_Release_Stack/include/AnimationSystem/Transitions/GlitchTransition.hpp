/*****************************************************************
 * @file GlitchTransition.hpp
 * @brief Glitch-based Transition Effect
 * 
 * Uses the GlitchShader to create a glitchy transition between animations.
 * Intensity peaks in the middle of the transition when animations swap.
 * 
 * Usage:
 *   1. Call init() when starting transition
 *   2. Call update() each frame with deltaMs and progress (0-1)
 *   3. Use getShader() to apply glitch effects during rendering
 *   4. Call reset() when transition ends
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "../Shaders/GlitchShader.hpp"
#include <cstdint>

namespace AnimationSystem {
namespace Transitions {

struct GlitchTransition {
    Shaders::GlitchShader shader;
    float intensity = 0.0f;
    bool active = false;
    
    void init() {
        shader.reset();
        shader.setEnabled(true);
        intensity = 0.0f;
        active = true;
    }
    
    // Update with transition progress (0.0 to 1.0)
    void update(uint32_t deltaMs, float progress) {
        if (!active) return;
        
        // Intensity curve: peaks at 50% progress
        if (progress < 0.5f) {
            intensity = progress * 3.0f;  // 0 -> 1.5
        } else {
            intensity = (1.0f - progress) * 3.0f;  // 1.5 -> 0
        }
        
        shader.setIntensity(intensity);
        shader.update(deltaMs);
    }
    
    Shaders::GlitchShader& getShader() { return shader; }
    const Shaders::GlitchShader& getShader() const { return shader; }
    
    float getIntensity() const { return intensity; }
    bool isActive() const { return active; }
    
    // Returns true if we should switch animations (at 50% progress)
    bool shouldSwapAnimation(float progress) const {
        return progress >= 0.5f;
    }
    
    void reset() {
        shader.setEnabled(false);
        shader.reset();
        intensity = 0.0f;
        active = false;
    }
};

} // namespace Transitions
} // namespace AnimationSystem
