/*****************************************************************
 * @file KeyframeTrack.hpp
 * @brief Keyframe-based animation tracks
 * 
 * Define animation sequences with keyframes and interpolation.
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include "../Core/Easing.hpp"
#include "../Core/Color.hpp"
#include <algorithm>

namespace AnimationDriver {

// ============================================================
// Keyframe Structure
// ============================================================

template<typename T>
struct Keyframe {
    float time;         // Time position in seconds
    T value;            // Value at this keyframe
    EasingType easing;  // Easing to use when interpolating TO this keyframe
    
    Keyframe() : time(0.0f), value(T{}), easing(EasingType::LINEAR) {}
    Keyframe(float t, const T& v, EasingType e = EasingType::LINEAR)
        : time(t), value(v), easing(e) {}
    
    bool operator<(const Keyframe& other) const {
        return time < other.time;
    }
};

// ============================================================
// Float Keyframe Track
// ============================================================

class FloatTrack {
public:
    static constexpr int MAX_KEYFRAMES = 32;
    
    FloatTrack() : count_(0), duration_(0.0f) {}
    
    // Add keyframe
    FloatTrack& addKey(float time, float value, EasingType easing = EasingType::LINEAR) {
        if (count_ < MAX_KEYFRAMES) {
            keyframes_[count_++] = Keyframe<float>(time, value, easing);
            sortKeyframes();
            updateDuration();
        }
        return *this;
    }
    
    // Convenience methods
    FloatTrack& addKey(float time, float value) {
        return addKey(time, value, EasingType::LINEAR);
    }
    
    FloatTrack& addKeyEaseIn(float time, float value) {
        return addKey(time, value, EasingType::EASE_IN_CUBIC);
    }
    
    FloatTrack& addKeyEaseOut(float time, float value) {
        return addKey(time, value, EasingType::EASE_OUT_CUBIC);
    }
    
    FloatTrack& addKeyEaseInOut(float time, float value) {
        return addKey(time, value, EasingType::EASE_IN_OUT_CUBIC);
    }
    
    // Evaluate at time
    float evaluate(float time) const {
        if (count_ == 0) return 0.0f;
        if (count_ == 1) return keyframes_[0].value;
        
        // Handle loop modes
        time = processTime(time);
        
        // Find surrounding keyframes
        int nextIdx = 0;
        for (int i = 0; i < count_; i++) {
            if (keyframes_[i].time > time) {
                nextIdx = i;
                break;
            }
            nextIdx = i;
        }
        
        if (nextIdx == 0) {
            return keyframes_[0].value;
        }
        
        int prevIdx = nextIdx - 1;
        
        // Compute interpolation factor
        float segmentDuration = keyframes_[nextIdx].time - keyframes_[prevIdx].time;
        float t = (segmentDuration > 0.0f) 
            ? (time - keyframes_[prevIdx].time) / segmentDuration 
            : 0.0f;
        
        // Apply easing
        t = Easing::apply(keyframes_[nextIdx].easing, t);
        
        // Linear interpolation
        return keyframes_[prevIdx].value + t * (keyframes_[nextIdx].value - keyframes_[prevIdx].value);
    }
    
    // Set loop mode
    FloatTrack& setLoop(LoopMode mode) {
        loopMode_ = mode;
        return *this;
    }
    
    // Get properties
    float getDuration() const { return duration_; }
    int getKeyframeCount() const { return count_; }
    
    // Clear all keyframes
    void clear() {
        count_ = 0;
        duration_ = 0.0f;
    }
    
private:
    void sortKeyframes() {
        std::sort(keyframes_, keyframes_ + count_);
    }
    
    void updateDuration() {
        duration_ = (count_ > 0) ? keyframes_[count_ - 1].time : 0.0f;
    }
    
    float processTime(float time) const {
        if (duration_ <= 0.0f || loopMode_ == LoopMode::ONCE) {
            return std::max(0.0f, std::min(time, duration_));
        }
        
        switch (loopMode_) {
            case LoopMode::LOOP:
                return fmodf(time, duration_);
                
            case LoopMode::PING_PONG: {
                float cycle = fmodf(time, duration_ * 2.0f);
                return (cycle > duration_) ? (duration_ * 2.0f - cycle) : cycle;
            }
            
            case LoopMode::REVERSE:
                return duration_ - fmodf(time, duration_);
                
            default:
                return time;
        }
    }
    
    Keyframe<float> keyframes_[MAX_KEYFRAMES];
    int count_;
    float duration_;
    LoopMode loopMode_ = LoopMode::ONCE;
};

// ============================================================
// Color Keyframe Track
// ============================================================

class ColorTrack {
public:
    static constexpr int MAX_KEYFRAMES = 16;
    
    ColorTrack() : count_(0), duration_(0.0f), useHSV_(false) {}
    
    // Add RGB keyframe
    ColorTrack& addKey(float time, const RGB& color, EasingType easing = EasingType::LINEAR) {
        if (count_ < MAX_KEYFRAMES) {
            keyframes_[count_++] = Keyframe<RGB>(time, color, easing);
            sortKeyframes();
            updateDuration();
        }
        return *this;
    }
    
    // Add HSV keyframe (will interpolate in HSV space)
    ColorTrack& addKeyHSV(float time, const HSV& color, EasingType easing = EasingType::LINEAR) {
        useHSV_ = true;
        if (count_ < MAX_KEYFRAMES) {
            keyframes_[count_] = Keyframe<RGB>(time, color.toRGB(), easing);
            hsvValues_[count_] = color;
            count_++;
            sortKeyframes();
            updateDuration();
        }
        return *this;
    }
    
    // Evaluate at time
    RGB evaluate(float time) const {
        if (count_ == 0) return RGB::Black();
        if (count_ == 1) return keyframes_[0].value;
        
        time = processTime(time);
        
        // Find surrounding keyframes
        int nextIdx = 0;
        for (int i = 0; i < count_; i++) {
            if (keyframes_[i].time > time) {
                nextIdx = i;
                break;
            }
            nextIdx = i;
        }
        
        if (nextIdx == 0) {
            return keyframes_[0].value;
        }
        
        int prevIdx = nextIdx - 1;
        
        float segmentDuration = keyframes_[nextIdx].time - keyframes_[prevIdx].time;
        float t = (segmentDuration > 0.0f) 
            ? (time - keyframes_[prevIdx].time) / segmentDuration 
            : 0.0f;
        
        t = Easing::apply(keyframes_[nextIdx].easing, t);
        
        if (useHSV_) {
            // Interpolate in HSV space
            HSV from = hsvValues_[prevIdx];
            HSV to = hsvValues_[nextIdx];
            
            // Handle hue wraparound
            float hDiff = to.h - from.h;
            if (hDiff > 180.0f) hDiff -= 360.0f;
            if (hDiff < -180.0f) hDiff += 360.0f;
            
            float h = fmodf(from.h + t * hDiff + 360.0f, 360.0f);
            float s = from.s + t * (to.s - from.s);
            float v = from.v + t * (to.v - from.v);
            
            return HSV(h, s, v).toRGB();
        }
        
        // Interpolate in RGB space
        return ColorBlend::alpha(keyframes_[prevIdx].value, keyframes_[nextIdx].value, t);
    }
    
    ColorTrack& setLoop(LoopMode mode) {
        loopMode_ = mode;
        return *this;
    }
    
    float getDuration() const { return duration_; }
    int getKeyframeCount() const { return count_; }
    
    void clear() {
        count_ = 0;
        duration_ = 0.0f;
    }
    
private:
    void sortKeyframes() {
        // Simple bubble sort (small array)
        for (int i = 0; i < count_ - 1; i++) {
            for (int j = 0; j < count_ - i - 1; j++) {
                if (keyframes_[j].time > keyframes_[j + 1].time) {
                    std::swap(keyframes_[j], keyframes_[j + 1]);
                    std::swap(hsvValues_[j], hsvValues_[j + 1]);
                }
            }
        }
    }
    
    void updateDuration() {
        duration_ = (count_ > 0) ? keyframes_[count_ - 1].time : 0.0f;
    }
    
    float processTime(float time) const {
        if (duration_ <= 0.0f || loopMode_ == LoopMode::ONCE) {
            return std::max(0.0f, std::min(time, duration_));
        }
        
        switch (loopMode_) {
            case LoopMode::LOOP:
                return fmodf(time, duration_);
            case LoopMode::PING_PONG: {
                float cycle = fmodf(time, duration_ * 2.0f);
                return (cycle > duration_) ? (duration_ * 2.0f - cycle) : cycle;
            }
            default:
                return time;
        }
    }
    
    Keyframe<RGB> keyframes_[MAX_KEYFRAMES];
    HSV hsvValues_[MAX_KEYFRAMES];
    int count_;
    float duration_;
    LoopMode loopMode_ = LoopMode::ONCE;
    bool useHSV_;
};

// ============================================================
// Vec2 Keyframe Track
// ============================================================

class Vec2Track {
public:
    static constexpr int MAX_KEYFRAMES = 32;
    
    Vec2Track() : count_(0), duration_(0.0f) {}
    
    Vec2Track& addKey(float time, const Vec2& value, EasingType easing = EasingType::LINEAR) {
        if (count_ < MAX_KEYFRAMES) {
            keyframes_[count_++] = Keyframe<Vec2>(time, value, easing);
            sortKeyframes();
            updateDuration();
        }
        return *this;
    }
    
    Vec2Track& addKey(float time, float x, float y, EasingType easing = EasingType::LINEAR) {
        return addKey(time, Vec2(x, y), easing);
    }
    
    Vec2 evaluate(float time) const {
        if (count_ == 0) return Vec2(0, 0);
        if (count_ == 1) return keyframes_[0].value;
        
        time = processTime(time);
        
        int nextIdx = 0;
        for (int i = 0; i < count_; i++) {
            if (keyframes_[i].time > time) {
                nextIdx = i;
                break;
            }
            nextIdx = i;
        }
        
        if (nextIdx == 0) return keyframes_[0].value;
        
        int prevIdx = nextIdx - 1;
        float segmentDuration = keyframes_[nextIdx].time - keyframes_[prevIdx].time;
        float t = (segmentDuration > 0.0f) 
            ? (time - keyframes_[prevIdx].time) / segmentDuration 
            : 0.0f;
        
        t = Easing::apply(keyframes_[nextIdx].easing, t);
        
        return Vec2(
            keyframes_[prevIdx].value.x + t * (keyframes_[nextIdx].value.x - keyframes_[prevIdx].value.x),
            keyframes_[prevIdx].value.y + t * (keyframes_[nextIdx].value.y - keyframes_[prevIdx].value.y)
        );
    }
    
    Vec2Track& setLoop(LoopMode mode) {
        loopMode_ = mode;
        return *this;
    }
    
    float getDuration() const { return duration_; }
    
    void clear() { count_ = 0; duration_ = 0.0f; }
    
private:
    void sortKeyframes() {
        std::sort(keyframes_, keyframes_ + count_);
    }
    
    void updateDuration() {
        duration_ = (count_ > 0) ? keyframes_[count_ - 1].time : 0.0f;
    }
    
    float processTime(float time) const {
        if (duration_ <= 0.0f) return 0.0f;
        switch (loopMode_) {
            case LoopMode::LOOP: return fmodf(time, duration_);
            case LoopMode::PING_PONG: {
                float c = fmodf(time, duration_ * 2.0f);
                return (c > duration_) ? (duration_ * 2.0f - c) : c;
            }
            default: return std::max(0.0f, std::min(time, duration_));
        }
    }
    
    Keyframe<Vec2> keyframes_[MAX_KEYFRAMES];
    int count_;
    float duration_;
    LoopMode loopMode_ = LoopMode::ONCE;
};

} // namespace AnimationDriver
