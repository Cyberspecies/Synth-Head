/*****************************************************************
 * @file GestureDetector.hpp
 * @brief Detect gestures and patterns from sensor data
 * 
 * Converts raw sensor readings into gesture-like events:
 * shake, tilt, tap, swipe, hover, etc.
 *****************************************************************/

#pragma once

#include "SensorTypes.hpp"
#include "SensorChannel.hpp"
#include "../Binding/FilterChain.hpp"
#include <cstring>
#include <cmath>

namespace AnimationDriver {

// ============================================================
// Gesture Types
// ============================================================

enum class GestureType : uint8_t {
    NONE,
    
    // Motion gestures
    SHAKE,          // Rapid movement in any direction
    TAP,            // Quick impact
    DOUBLE_TAP,     // Two quick taps
    TILT_LEFT,      // Tilting left
    TILT_RIGHT,     // Tilting right
    TILT_FORWARD,   // Tilting forward
    TILT_BACKWARD,  // Tilting backward
    FLIP,           // 180° rotation
    
    // Threshold gestures
    THRESHOLD_HIGH, // Value exceeds high threshold
    THRESHOLD_LOW,  // Value drops below low threshold
    RISING,         // Value increasing
    FALLING,        // Value decreasing
    
    // Pattern gestures
    PULSE,          // Rhythmic pattern
    WAVE,           // Oscillating pattern
    
    // Custom
    CUSTOM
};

// ============================================================
// Gesture State
// ============================================================

struct GestureState {
    GestureType type = GestureType::NONE;
    float intensity = 0.0f;         // 0-1 gesture strength
    float progress = 0.0f;          // For multi-stage gestures
    float duration = 0.0f;          // How long gesture has been active
    bool active = false;            // Currently in gesture
    bool triggered = false;         // Just triggered this frame
    bool released = false;          // Just released this frame
    
    void reset() {
        intensity = 0.0f;
        progress = 0.0f;
        duration = 0.0f;
        active = false;
        triggered = false;
        released = false;
    }
};

// ============================================================
// Gesture Configuration
// ============================================================

struct GestureConfig {
    GestureType type = GestureType::NONE;
    
    // Thresholds
    float activateThreshold = 0.5f;     // Threshold to start gesture
    float deactivateThreshold = 0.3f;   // Threshold to end gesture
    float intensityScale = 1.0f;        // Scale factor for intensity
    
    // Timing
    float minDuration = 0.0f;           // Minimum time to hold
    float maxDuration = 0.0f;           // Maximum duration (0 = unlimited)
    float cooldown = 0.1f;              // Time between triggers
    
    // For motion gestures
    float requiredMagnitude = 1.5f;     // Required acceleration magnitude
    float sustainedTime = 0.1f;         // Time to sustain for detection
    
    // Factory methods
    static GestureConfig Shake(float threshold = 1.5f) {
        GestureConfig cfg;
        cfg.type = GestureType::SHAKE;
        cfg.activateThreshold = threshold;
        cfg.deactivateThreshold = threshold * 0.6f;
        cfg.requiredMagnitude = threshold;
        return cfg;
    }
    
    static GestureConfig Tap(float threshold = 2.0f) {
        GestureConfig cfg;
        cfg.type = GestureType::TAP;
        cfg.activateThreshold = threshold;
        cfg.maxDuration = 0.2f;
        cfg.cooldown = 0.3f;
        return cfg;
    }
    
    static GestureConfig Tilt(GestureType direction, float threshold = 0.3f) {
        GestureConfig cfg;
        cfg.type = direction;
        cfg.activateThreshold = threshold;
        cfg.deactivateThreshold = threshold * 0.7f;
        return cfg;
    }
    
    static GestureConfig Threshold(float high, float low = 0.0f) {
        GestureConfig cfg;
        cfg.type = GestureType::THRESHOLD_HIGH;
        cfg.activateThreshold = high;
        cfg.deactivateThreshold = (low > 0.0f) ? low : high * 0.8f;
        return cfg;
    }
};

// ============================================================
// Single Gesture Detector
// ============================================================

class GestureDetector {
public:
    static constexpr int MAX_NAME_LEN = 24;
    
    GestureDetector() : state_(), cooldownTimer_(0.0f) {
        name_[0] = '\0';
    }
    
    GestureDetector(const char* name, const GestureConfig& config) : GestureDetector() {
        setName(name);
        setConfig(config);
    }
    
    // Configuration
    GestureDetector& setName(const char* name) {
        strncpy(name_, name, MAX_NAME_LEN - 1);
        name_[MAX_NAME_LEN - 1] = '\0';
        return *this;
    }
    
    GestureDetector& setConfig(const GestureConfig& config) {
        config_ = config;
        return *this;
    }
    
    // Update with scalar input
    void update(float value, float deltaTime) {
        // Clear frame-specific flags
        state_.triggered = false;
        state_.released = false;
        
        // Update cooldown
        if (cooldownTimer_ > 0.0f) {
            cooldownTimer_ -= deltaTime;
        }
        
        // Check for activation
        bool shouldActivate = evaluateActivation(value);
        
        if (!state_.active && shouldActivate && cooldownTimer_ <= 0.0f) {
            // Start gesture
            state_.active = true;
            state_.triggered = true;
            state_.duration = 0.0f;
        } else if (state_.active) {
            state_.duration += deltaTime;
            
            // Check for deactivation
            bool shouldDeactivate = evaluateDeactivation(value);
            bool timedOut = (config_.maxDuration > 0.0f && 
                            state_.duration > config_.maxDuration);
            
            if (shouldDeactivate || timedOut) {
                state_.active = false;
                state_.released = true;
                cooldownTimer_ = config_.cooldown;
            }
        }
        
        // Update intensity
        if (state_.active) {
            state_.intensity = calculateIntensity(value);
        } else {
            state_.intensity = 0.0f;
        }
        
        lastValue_ = value;
    }
    
    // Update with SensorValue input
    void update(const SensorValue& value, float deltaTime) {
        update(value.magnitude(), deltaTime);
    }
    
    // Update with Vec3 input (for motion gestures)
    void updateMotion(const Vec3& accel, float deltaTime) {
        float mag = sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
        update(mag, deltaTime);
    }
    
    // State queries
    const GestureState& getState() const { return state_; }
    bool isActive() const { return state_.active; }
    bool wasTriggered() const { return state_.triggered; }
    bool wasReleased() const { return state_.released; }
    float getIntensity() const { return state_.intensity; }
    float getDuration() const { return state_.duration; }
    const char* getName() const { return name_; }
    GestureType getType() const { return config_.type; }
    
    // Reset
    void reset() {
        state_.reset();
        cooldownTimer_ = 0.0f;
    }
    
private:
    bool evaluateActivation(float value) const {
        switch (config_.type) {
            case GestureType::SHAKE:
            case GestureType::TAP:
            case GestureType::THRESHOLD_HIGH:
                return value >= config_.activateThreshold;
                
            case GestureType::THRESHOLD_LOW:
                return value <= config_.activateThreshold;
                
            case GestureType::RISING:
                return value > lastValue_ + config_.activateThreshold;
                
            case GestureType::FALLING:
                return value < lastValue_ - config_.activateThreshold;
                
            default:
                return value >= config_.activateThreshold;
        }
    }
    
    bool evaluateDeactivation(float value) const {
        switch (config_.type) {
            case GestureType::SHAKE:
            case GestureType::TAP:
            case GestureType::THRESHOLD_HIGH:
                return value < config_.deactivateThreshold;
                
            case GestureType::THRESHOLD_LOW:
                return value > config_.deactivateThreshold;
                
            default:
                return value < config_.deactivateThreshold;
        }
    }
    
    float calculateIntensity(float value) const {
        float range = config_.activateThreshold - config_.deactivateThreshold;
        if (range <= 0.0f) return 1.0f;
        
        float intensity = (value - config_.deactivateThreshold) / range;
        intensity = fmaxf(0.0f, fminf(1.0f, intensity));
        return intensity * config_.intensityScale;
    }
    
    char name_[MAX_NAME_LEN];
    GestureConfig config_;
    GestureState state_;
    float cooldownTimer_;
    float lastValue_ = 0.0f;
};

// ============================================================
// Orientation Detector (Computes pitch/roll from accelerometer)
// ============================================================

class OrientationDetector {
public:
    OrientationDetector() = default;
    
    // Update with accelerometer data
    void update(const Vec3& accel) {
        // Compute pitch and roll from accelerometer
        // Pitch: rotation around X axis (forward/backward tilt)
        rawPitch_ = atan2f(accel.y, sqrtf(accel.x * accel.x + accel.z * accel.z));
        // Roll: rotation around Y axis (left/right tilt)
        rawRoll_ = atan2f(-accel.x, accel.z);
        
        // Convert to degrees
        pitch_ = rawPitch_ * 180.0f / 3.14159f;
        roll_ = rawRoll_ * 180.0f / 3.14159f;
        
        // Apply smoothing
        if (initialized_) {
            smoothedPitch_ = smoothedPitch_ + smoothAlpha_ * (pitch_ - smoothedPitch_);
            smoothedRoll_ = smoothedRoll_ + smoothAlpha_ * (roll_ - smoothedRoll_);
        } else {
            smoothedPitch_ = pitch_;
            smoothedRoll_ = roll_;
            initialized_ = true;
        }
    }
    
    // Set smoothing (0 = no smoothing, 1 = instant)
    void setSmoothingAlpha(float alpha) {
        smoothAlpha_ = fmaxf(0.0f, fminf(1.0f, alpha));
    }
    
    // Get orientation in degrees
    float getPitch() const { return smoothedPitch_; }
    float getRoll() const { return smoothedRoll_; }
    float getRawPitch() const { return pitch_; }
    float getRawRoll() const { return roll_; }
    
    // Get normalized orientation (-1 to 1, mapped from -90 to 90)
    float getNormalizedPitch() const { return smoothedPitch_ / 90.0f; }
    float getNormalizedRoll() const { return smoothedRoll_ / 90.0f; }
    
    // As providers
    ValueProvider pitchProvider() const {
        return [this]() { return this->getNormalizedPitch(); };
    }
    
    ValueProvider rollProvider() const {
        return [this]() { return this->getNormalizedRoll(); };
    }
    
    void reset() {
        initialized_ = false;
        pitch_ = roll_ = 0.0f;
        smoothedPitch_ = smoothedRoll_ = 0.0f;
    }
    
private:
    float rawPitch_ = 0.0f;
    float rawRoll_ = 0.0f;
    float pitch_ = 0.0f;
    float roll_ = 0.0f;
    float smoothedPitch_ = 0.0f;
    float smoothedRoll_ = 0.0f;
    float smoothAlpha_ = 0.1f;
    bool initialized_ = false;
};

} // namespace AnimationDriver
