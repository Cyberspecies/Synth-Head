/*****************************************************************
 * @file Transition.hpp
 * @brief Transition and crossfade between animations/shaders
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include "../Core/Easing.hpp"
#include "../Core/Color.hpp"
#include "AnimationClip.hpp"

namespace AnimationDriver {

// ============================================================
// Transition Type
// ============================================================

enum class TransitionType : uint8_t {
    CUT,            // Instant switch
    CROSSFADE,      // Blend between A and B
    FADE_BLACK,     // Fade to black, then fade in
    FADE_WHITE,     // Fade to white, then fade in
    WIPE_LEFT,      // Wipe from left
    WIPE_RIGHT,     // Wipe from right
    WIPE_UP,        // Wipe from bottom
    WIPE_DOWN,      // Wipe from top
    DISSOLVE        // Random pixel dissolve
};

// ============================================================
// Transition Configuration
// ============================================================

struct TransitionConfig {
    TransitionType type = TransitionType::CROSSFADE;
    float duration = 0.5f;          // Total transition time
    EasingType easing = EasingType::EASE_IN_OUT_CUBIC;
    
    // Factory methods
    static TransitionConfig Cut() {
        TransitionConfig cfg;
        cfg.type = TransitionType::CUT;
        cfg.duration = 0.0f;
        return cfg;
    }
    
    static TransitionConfig Crossfade(float duration = 0.5f) {
        TransitionConfig cfg;
        cfg.type = TransitionType::CROSSFADE;
        cfg.duration = duration;
        return cfg;
    }
    
    static TransitionConfig FadeBlack(float duration = 1.0f) {
        TransitionConfig cfg;
        cfg.type = TransitionType::FADE_BLACK;
        cfg.duration = duration;
        return cfg;
    }
    
    static TransitionConfig WipeLeft(float duration = 0.5f) {
        TransitionConfig cfg;
        cfg.type = TransitionType::WIPE_LEFT;
        cfg.duration = duration;
        return cfg;
    }
};

// ============================================================
// Color Transition - Blend between two colors
// ============================================================

class ColorTransition {
public:
    ColorTransition() : progress_(0.0f), duration_(0.5f), active_(false) {}
    
    // Start transition
    void start(const RGB& from, const RGB& to, float duration = 0.5f, 
               EasingType easing = EasingType::EASE_IN_OUT_CUBIC) {
        fromColor_ = from;
        toColor_ = to;
        duration_ = duration;
        easing_ = easing;
        progress_ = 0.0f;
        active_ = true;
    }
    
    // Update transition
    void update(float deltaTime) {
        if (!active_) return;
        
        progress_ += (duration_ > 0.0f) ? (deltaTime / duration_) : 1.0f;
        
        if (progress_ >= 1.0f) {
            progress_ = 1.0f;
            active_ = false;
        }
    }
    
    // Get current color
    RGB getCurrent() const {
        float t = Easing::apply(easing_, progress_);
        return ColorBlend::alpha(fromColor_, toColor_, t);
    }
    
    // State queries
    bool isActive() const { return active_; }
    bool isComplete() const { return !active_ && progress_ >= 1.0f; }
    float getProgress() const { return progress_; }
    
    // Cancel transition
    void cancel() {
        active_ = false;
        progress_ = 0.0f;
    }
    
    // Jump to end
    void complete() {
        progress_ = 1.0f;
        active_ = false;
    }
    
private:
    RGB fromColor_;
    RGB toColor_;
    float progress_;
    float duration_;
    EasingType easing_ = EasingType::LINEAR;
    bool active_;
};

// ============================================================
// Float Value Transition
// ============================================================

class FloatTransition {
public:
    FloatTransition() : from_(0.0f), to_(0.0f), progress_(0.0f), 
                        duration_(0.5f), active_(false) {}
    
    void start(float from, float to, float duration = 0.5f,
               EasingType easing = EasingType::EASE_IN_OUT_CUBIC) {
        from_ = from;
        to_ = to;
        duration_ = duration;
        easing_ = easing;
        progress_ = 0.0f;
        active_ = true;
    }
    
    // Start from current value
    void transitionTo(float to, float duration = 0.5f,
                      EasingType easing = EasingType::EASE_IN_OUT_CUBIC) {
        start(getCurrent(), to, duration, easing);
    }
    
    void update(float deltaTime) {
        if (!active_) return;
        
        progress_ += (duration_ > 0.0f) ? (deltaTime / duration_) : 1.0f;
        
        if (progress_ >= 1.0f) {
            progress_ = 1.0f;
            active_ = false;
        }
    }
    
    float getCurrent() const {
        float t = Easing::apply(easing_, progress_);
        return from_ + t * (to_ - from_);
    }
    
    bool isActive() const { return active_; }
    bool isComplete() const { return !active_ && progress_ >= 1.0f; }
    float getProgress() const { return progress_; }
    
    void cancel() { active_ = false; progress_ = 0.0f; }
    void complete() { progress_ = 1.0f; active_ = false; }
    
private:
    float from_;
    float to_;
    float progress_;
    float duration_;
    EasingType easing_ = EasingType::LINEAR;
    bool active_;
};

// ============================================================
// Vec2 Transition
// ============================================================

class Vec2Transition {
public:
    Vec2Transition() : progress_(0.0f), duration_(0.5f), active_(false) {}
    
    void start(const Vec2& from, const Vec2& to, float duration = 0.5f,
               EasingType easing = EasingType::EASE_IN_OUT_CUBIC) {
        from_ = from;
        to_ = to;
        duration_ = duration;
        easing_ = easing;
        progress_ = 0.0f;
        active_ = true;
    }
    
    void transitionTo(const Vec2& to, float duration = 0.5f,
                      EasingType easing = EasingType::EASE_IN_OUT_CUBIC) {
        start(getCurrent(), to, duration, easing);
    }
    
    void update(float deltaTime) {
        if (!active_) return;
        
        progress_ += (duration_ > 0.0f) ? (deltaTime / duration_) : 1.0f;
        
        if (progress_ >= 1.0f) {
            progress_ = 1.0f;
            active_ = false;
        }
    }
    
    Vec2 getCurrent() const {
        float t = Easing::apply(easing_, progress_);
        return Vec2(
            from_.x + t * (to_.x - from_.x),
            from_.y + t * (to_.y - from_.y)
        );
    }
    
    bool isActive() const { return active_; }
    bool isComplete() const { return !active_ && progress_ >= 1.0f; }
    
    void cancel() { active_ = false; progress_ = 0.0f; }
    
private:
    Vec2 from_;
    Vec2 to_;
    float progress_;
    float duration_;
    EasingType easing_ = EasingType::LINEAR;
    bool active_;
};

// ============================================================
// Transition Helper - For shader transitions
// ============================================================

namespace TransitionHelper {

// Get blend factor for pixel at position during transition
inline float getTransitionFactor(TransitionType type, float progress, 
                                  float x, float y, float eased) {
    switch (type) {
        case TransitionType::CUT:
            return (progress >= 0.5f) ? 1.0f : 0.0f;
            
        case TransitionType::CROSSFADE:
            return eased;
            
        case TransitionType::FADE_BLACK:
        case TransitionType::FADE_WHITE:
            // First half: fade out, second half: fade in
            if (progress < 0.5f) {
                return 0.0f;  // Still showing A
            } else {
                return 1.0f;  // Showing B
            }
            
        case TransitionType::WIPE_LEFT:
            return (x < eased) ? 1.0f : 0.0f;
            
        case TransitionType::WIPE_RIGHT:
            return (x > (1.0f - eased)) ? 1.0f : 0.0f;
            
        case TransitionType::WIPE_UP:
            return (y < eased) ? 1.0f : 0.0f;
            
        case TransitionType::WIPE_DOWN:
            return (y > (1.0f - eased)) ? 1.0f : 0.0f;
            
        case TransitionType::DISSOLVE: {
            // Simple dissolve based on pixel position hash
            uint32_t h = static_cast<uint32_t>(x * 1000) ^ static_cast<uint32_t>(y * 7919);
            float threshold = static_cast<float>(h % 1000) / 1000.0f;
            return (eased > threshold) ? 1.0f : 0.0f;
        }
            
        default:
            return eased;
    }
}

// Get fade multiplier for fade transitions
inline float getFadeMultiplier(TransitionType type, float progress, float eased) {
    if (type == TransitionType::FADE_BLACK || type == TransitionType::FADE_WHITE) {
        if (progress < 0.5f) {
            // Fading out (0 -> 1 maps to 1 -> 0)
            return 1.0f - (eased * 2.0f);
        } else {
            // Fading in (0.5 -> 1 maps to 0 -> 1)
            return (eased - 0.5f) * 2.0f;
        }
    }
    return 1.0f;
}

} // namespace TransitionHelper

} // namespace AnimationDriver
