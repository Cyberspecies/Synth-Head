/*****************************************************************
 * @file Easing.hpp
 * @brief Easing functions for smooth animations
 * 
 * Provides a comprehensive set of easing functions following
 * standard animation curves (linear, ease-in, ease-out, etc.)
 *****************************************************************/

#pragma once

#include <cmath>
#include <functional>

namespace AnimationDriver {

// ============================================================
// Easing Function Type
// ============================================================

enum class EasingType : uint8_t {
    LINEAR,
    
    // Quadratic
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    
    // Cubic
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    
    // Quartic
    EASE_IN_QUART,
    EASE_OUT_QUART,
    EASE_IN_OUT_QUART,
    
    // Quintic
    EASE_IN_QUINT,
    EASE_OUT_QUINT,
    EASE_IN_OUT_QUINT,
    
    // Sine
    EASE_IN_SINE,
    EASE_OUT_SINE,
    EASE_IN_OUT_SINE,
    
    // Exponential
    EASE_IN_EXPO,
    EASE_OUT_EXPO,
    EASE_IN_OUT_EXPO,
    
    // Circular
    EASE_IN_CIRC,
    EASE_OUT_CIRC,
    EASE_IN_OUT_CIRC,
    
    // Elastic
    EASE_IN_ELASTIC,
    EASE_OUT_ELASTIC,
    EASE_IN_OUT_ELASTIC,
    
    // Back
    EASE_IN_BACK,
    EASE_OUT_BACK,
    EASE_IN_OUT_BACK,
    
    // Bounce
    EASE_IN_BOUNCE,
    EASE_OUT_BOUNCE,
    EASE_IN_OUT_BOUNCE,
    
    // Special
    STEP,           // Step function (0 until 1, then 1)
    SMOOTH_STEP,    // Hermite interpolation
    SMOOTHER_STEP,  // Ken Perlin's smoother step
    
    COUNT
};

// ============================================================
// Easing Functions Namespace
// ============================================================

namespace Easing {

constexpr float PI = 3.14159265358979f;

// Clamp t to [0, 1]
inline float clamp01(float t) {
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

// Linear
inline float linear(float t) { return t; }

// Quadratic
inline float easeInQuad(float t) { return t * t; }
inline float easeOutQuad(float t) { return t * (2.0f - t); }
inline float easeInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

// Cubic
inline float easeInCubic(float t) { return t * t * t; }
inline float easeOutCubic(float t) { float t1 = t - 1.0f; return t1 * t1 * t1 + 1.0f; }
inline float easeInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f + (t - 1.0f) * powf(2.0f * t - 2.0f, 2);
}

// Quartic
inline float easeInQuart(float t) { return t * t * t * t; }
inline float easeOutQuart(float t) { float t1 = t - 1.0f; return 1.0f - t1 * t1 * t1 * t1; }
inline float easeInOutQuart(float t) {
    float t1 = t - 1.0f;
    return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - 8.0f * t1 * t1 * t1 * t1;
}

// Quintic
inline float easeInQuint(float t) { return t * t * t * t * t; }
inline float easeOutQuint(float t) { float t1 = t - 1.0f; return 1.0f + t1 * t1 * t1 * t1 * t1; }
inline float easeInOutQuint(float t) {
    float t1 = t - 1.0f;
    return t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f + 16.0f * t1 * t1 * t1 * t1 * t1;
}

// Sine
inline float easeInSine(float t) { return 1.0f - cosf(t * PI * 0.5f); }
inline float easeOutSine(float t) { return sinf(t * PI * 0.5f); }
inline float easeInOutSine(float t) { return 0.5f * (1.0f - cosf(PI * t)); }

// Exponential
inline float easeInExpo(float t) { return t == 0.0f ? 0.0f : powf(2.0f, 10.0f * (t - 1.0f)); }
inline float easeOutExpo(float t) { return t == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t); }
inline float easeInOutExpo(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return t < 0.5f 
        ? 0.5f * powf(2.0f, 20.0f * t - 10.0f)
        : 1.0f - 0.5f * powf(2.0f, -20.0f * t + 10.0f);
}

// Circular
inline float easeInCirc(float t) { return 1.0f - sqrtf(1.0f - t * t); }
inline float easeOutCirc(float t) { float t1 = t - 1.0f; return sqrtf(1.0f - t1 * t1); }
inline float easeInOutCirc(float t) {
    return t < 0.5f
        ? 0.5f * (1.0f - sqrtf(1.0f - 4.0f * t * t))
        : 0.5f * (sqrtf(1.0f - powf(-2.0f * t + 2.0f, 2)) + 1.0f);
}

// Elastic
inline float easeInElastic(float t) {
    if (t == 0.0f || t == 1.0f) return t;
    return -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * (2.0f * PI / 3.0f));
}
inline float easeOutElastic(float t) {
    if (t == 0.0f || t == 1.0f) return t;
    return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * (2.0f * PI / 3.0f)) + 1.0f;
}
inline float easeInOutElastic(float t) {
    if (t == 0.0f || t == 1.0f) return t;
    return t < 0.5f
        ? -0.5f * powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * (2.0f * PI / 4.5f))
        : 0.5f * powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * (2.0f * PI / 4.5f)) + 1.0f;
}

// Back (overshoots)
inline float easeInBack(float t) {
    constexpr float c = 1.70158f;
    return (c + 1.0f) * t * t * t - c * t * t;
}
inline float easeOutBack(float t) {
    constexpr float c = 1.70158f;
    float t1 = t - 1.0f;
    return 1.0f + (c + 1.0f) * t1 * t1 * t1 + c * t1 * t1;
}
inline float easeInOutBack(float t) {
    constexpr float c = 1.70158f * 1.525f;
    return t < 0.5f
        ? 0.5f * (4.0f * t * t * ((c + 1.0f) * 2.0f * t - c))
        : 0.5f * ((2.0f * t - 2.0f) * (2.0f * t - 2.0f) * ((c + 1.0f) * (t * 2.0f - 2.0f) + c) + 2.0f);
}

// Bounce
inline float easeOutBounce(float t) {
    constexpr float n = 7.5625f;
    constexpr float d = 2.75f;
    if (t < 1.0f / d) {
        return n * t * t;
    } else if (t < 2.0f / d) {
        t -= 1.5f / d;
        return n * t * t + 0.75f;
    } else if (t < 2.5f / d) {
        t -= 2.25f / d;
        return n * t * t + 0.9375f;
    } else {
        t -= 2.625f / d;
        return n * t * t + 0.984375f;
    }
}
inline float easeInBounce(float t) { return 1.0f - easeOutBounce(1.0f - t); }
inline float easeInOutBounce(float t) {
    return t < 0.5f
        ? 0.5f * (1.0f - easeOutBounce(1.0f - 2.0f * t))
        : 0.5f * (1.0f + easeOutBounce(2.0f * t - 1.0f));
}

// Step functions
inline float step(float t) { return t < 1.0f ? 0.0f : 1.0f; }
inline float smoothStep(float t) { return t * t * (3.0f - 2.0f * t); }
inline float smootherStep(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

// ============================================================
// Apply Easing by Type
// ============================================================

inline float apply(EasingType type, float t) {
    t = clamp01(t);
    
    switch (type) {
        case EasingType::LINEAR:            return linear(t);
        
        case EasingType::EASE_IN_QUAD:      return easeInQuad(t);
        case EasingType::EASE_OUT_QUAD:     return easeOutQuad(t);
        case EasingType::EASE_IN_OUT_QUAD:  return easeInOutQuad(t);
        
        case EasingType::EASE_IN_CUBIC:     return easeInCubic(t);
        case EasingType::EASE_OUT_CUBIC:    return easeOutCubic(t);
        case EasingType::EASE_IN_OUT_CUBIC: return easeInOutCubic(t);
        
        case EasingType::EASE_IN_QUART:     return easeInQuart(t);
        case EasingType::EASE_OUT_QUART:    return easeOutQuart(t);
        case EasingType::EASE_IN_OUT_QUART: return easeInOutQuart(t);
        
        case EasingType::EASE_IN_QUINT:     return easeInQuint(t);
        case EasingType::EASE_OUT_QUINT:    return easeOutQuint(t);
        case EasingType::EASE_IN_OUT_QUINT: return easeInOutQuint(t);
        
        case EasingType::EASE_IN_SINE:      return easeInSine(t);
        case EasingType::EASE_OUT_SINE:     return easeOutSine(t);
        case EasingType::EASE_IN_OUT_SINE:  return easeInOutSine(t);
        
        case EasingType::EASE_IN_EXPO:      return easeInExpo(t);
        case EasingType::EASE_OUT_EXPO:     return easeOutExpo(t);
        case EasingType::EASE_IN_OUT_EXPO:  return easeInOutExpo(t);
        
        case EasingType::EASE_IN_CIRC:      return easeInCirc(t);
        case EasingType::EASE_OUT_CIRC:     return easeOutCirc(t);
        case EasingType::EASE_IN_OUT_CIRC:  return easeInOutCirc(t);
        
        case EasingType::EASE_IN_ELASTIC:   return easeInElastic(t);
        case EasingType::EASE_OUT_ELASTIC:  return easeOutElastic(t);
        case EasingType::EASE_IN_OUT_ELASTIC: return easeInOutElastic(t);
        
        case EasingType::EASE_IN_BACK:      return easeInBack(t);
        case EasingType::EASE_OUT_BACK:     return easeOutBack(t);
        case EasingType::EASE_IN_OUT_BACK:  return easeInOutBack(t);
        
        case EasingType::EASE_IN_BOUNCE:    return easeInBounce(t);
        case EasingType::EASE_OUT_BOUNCE:   return easeOutBounce(t);
        case EasingType::EASE_IN_OUT_BOUNCE: return easeInOutBounce(t);
        
        case EasingType::STEP:              return step(t);
        case EasingType::SMOOTH_STEP:       return smoothStep(t);
        case EasingType::SMOOTHER_STEP:     return smootherStep(t);
        
        default: return t;
    }
}

// Get function pointer for type
using EasingFunc = float(*)(float);

inline EasingFunc getFunction(EasingType type) {
    switch (type) {
        case EasingType::LINEAR:            return linear;
        case EasingType::EASE_IN_QUAD:      return easeInQuad;
        case EasingType::EASE_OUT_QUAD:     return easeOutQuad;
        case EasingType::EASE_IN_OUT_QUAD:  return easeInOutQuad;
        case EasingType::EASE_IN_CUBIC:     return easeInCubic;
        case EasingType::EASE_OUT_CUBIC:    return easeOutCubic;
        case EasingType::EASE_IN_OUT_CUBIC: return easeInOutCubic;
        case EasingType::SMOOTH_STEP:       return smoothStep;
        case EasingType::SMOOTHER_STEP:     return smootherStep;
        default: return linear;
    }
}

} // namespace Easing

} // namespace AnimationDriver
