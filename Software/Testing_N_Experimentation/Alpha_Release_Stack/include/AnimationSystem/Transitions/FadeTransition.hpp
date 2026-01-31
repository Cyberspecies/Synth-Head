/*****************************************************************
 * @file FadeTransition.hpp
 * @brief Simple crossfade transition between animations
 * 
 * Fades out the old animation while fading in the new one.
 * 
 * PARAMETERS:
 * - duration_ms: Transition duration in milliseconds
 * - easing: Easing function (0=Linear, 1=EaseIn, 2=EaseOut, 3=EaseInOut)
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "../Core/TransitionBase.hpp"
#include "../Core/TransitionRegistry.hpp"

namespace AnimationSystem {

class FadeTransition : public TransitionBase {
public:
    FadeTransition() {
        // Define parameters
        defineParam("duration_ms", "Duration", "Transition duration in milliseconds",
                    ParamType::FLOAT, 500.0f, 100.0f, 5000.0f, "Timing");
        
        defineParam("easing", "Easing", "Easing function type",
                    ParamType::INT, 3.0f, 0.0f, 3.0f, "Timing");
    }
    
    const char* getTypeId() const override { return "fade"; }
    const char* getDisplayName() const override { return "Fade"; }
    const char* getDescription() const override { return "Crossfade transition"; }
    
    void update(uint32_t deltaMs) override {
        updateProgress(deltaMs);
    }
    
    void start(uint32_t durationMs = 0) override {
        // Use parameter or override
        TransitionBase::start(durationMs > 0 ? durationMs : 
                              static_cast<uint32_t>(getParam("duration_ms")));
    }
    
    void render() override {
        // Skip if complete (no blending needed)
        if (isComplete()) return;
        
        // Get easing type
        int easingType = getParamInt("easing");
        float progress = getProgress();
        
        // Apply easing
        switch (easingType) {
            case 0: break; // Linear
            case 1: progress = easeIn(progress); break;
            case 2: progress = easeOut(progress); break;
            case 3: progress = easeInOut(progress); break;
        }
        
        // Blend captured "from" frame with current output
        // The captured frame is the "old" state
        for (int y = 0; y < DISPLAY_H; y++) {
            for (int x = 0; x < DISPLAY_W; x++) {
                uint8_t oldR, oldG, oldB;
                getCapturedPixel(x, y, oldR, oldG, oldB);
                
                // Fade from captured to transparent (let new animation show through)
                uint8_t r = static_cast<uint8_t>(oldR * (1.0f - progress));
                uint8_t g = static_cast<uint8_t>(oldG * (1.0f - progress));
                uint8_t b = static_cast<uint8_t>(oldB * (1.0f - progress));
                
                if (drawPixel) {
                    drawPixel(x, y, r, g, b);
                }
            }
        }
    }
};

// Auto-register the transition with fade icon
REGISTER_TRANSITION_WITH_ICON(FadeTransition, "&#x25D0;")

} // namespace AnimationSystem
