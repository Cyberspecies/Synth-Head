/*****************************************************************
 * @file SpringFilter.hpp
 * @brief Spring physics filter for natural-feeling motion
 * 
 * Simulates a damped spring for smooth, bouncy transitions
 * with configurable stiffness and damping.
 *****************************************************************/

#pragma once

#include "FilterBase.hpp"
#include <cmath>

namespace AnimationDriver {

// ============================================================
// Spring Configuration Presets
// ============================================================

struct SpringConfig {
    float stiffness;    // Spring constant (higher = faster response)
    float damping;      // Damping ratio (1.0 = critical, <1 = bouncy, >1 = sluggish)
    float mass;         // Mass (affects inertia)
    
    SpringConfig(float s = 100.0f, float d = 10.0f, float m = 1.0f)
        : stiffness(s), damping(d), mass(m) {}
    
    // Preset configurations
    static SpringConfig Snappy() { return SpringConfig(400.0f, 20.0f, 1.0f); }
    static SpringConfig Bouncy() { return SpringConfig(200.0f, 8.0f, 1.0f); }
    static SpringConfig Smooth() { return SpringConfig(100.0f, 15.0f, 1.0f); }
    static SpringConfig Gentle() { return SpringConfig(50.0f, 10.0f, 1.0f); }
    static SpringConfig Stiff()  { return SpringConfig(500.0f, 30.0f, 1.0f); }
    static SpringConfig Wobbly() { return SpringConfig(150.0f, 5.0f, 1.0f); }
};

// ============================================================
// Spring Filter (1D)
// ============================================================

class SpringFilter : public FilterBase<float> {
public:
    SpringFilter() : config_(SpringConfig::Smooth()), 
                     position_(0.0f), velocity_(0.0f), 
                     target_(0.0f), initialized_(false) {}
    
    SpringFilter(const SpringConfig& config) : config_(config),
                     position_(0.0f), velocity_(0.0f), 
                     target_(0.0f), initialized_(false) {}
    
    // Configuration
    SpringFilter& setConfig(const SpringConfig& config) {
        config_ = config;
        return *this;
    }
    
    SpringFilter& setStiffness(float stiffness) {
        config_.stiffness = stiffness;
        return *this;
    }
    
    SpringFilter& setDamping(float damping) {
        config_.damping = damping;
        return *this;
    }
    
    SpringFilter& setMass(float mass) {
        config_.mass = mass;
        return *this;
    }
    
    // Process: input is target position, output is current spring position
    float process(const float& input) override {
        target_ = input;
        
        if (!initialized_) {
            position_ = input;
            velocity_ = 0.0f;
            initialized_ = true;
            return position_;
        }
        
        // Use fixed timestep for consistent behavior
        const float dt = 1.0f / 60.0f;
        
        // Spring force: F = -k * x - c * v
        // where x = displacement from target, v = velocity
        float displacement = position_ - target_;
        float springForce = -config_.stiffness * displacement;
        float dampingForce = -config_.damping * velocity_;
        float acceleration = (springForce + dampingForce) / config_.mass;
        
        // Integrate using semi-implicit Euler
        velocity_ += acceleration * dt;
        position_ += velocity_ * dt;
        
        // Settle if very close and slow
        if (fabsf(displacement) < 0.001f && fabsf(velocity_) < 0.001f) {
            position_ = target_;
            velocity_ = 0.0f;
        }
        
        return position_;
    }
    
    // Get current velocity
    float getVelocity() const { return velocity_; }
    
    // Check if spring is at rest
    bool isAtRest(float threshold = 0.01f) const {
        return fabsf(position_ - target_) < threshold && fabsf(velocity_) < threshold;
    }
    
    // Snap to target immediately
    void snap(float value) {
        position_ = value;
        target_ = value;
        velocity_ = 0.0f;
        initialized_ = true;
    }
    
    void reset() override {
        initialized_ = false;
        velocity_ = 0.0f;
    }
    
    FilterType getType() const override { return FilterType::SPRING; }
    
private:
    SpringConfig config_;
    float position_;
    float velocity_;
    float target_;
    bool initialized_;
};

// ============================================================
// 2D Spring Filter
// ============================================================

class Spring2DFilter {
public:
    Spring2DFilter() = default;
    Spring2DFilter(const SpringConfig& config) : springX_(config), springY_(config) {}
    
    Spring2DFilter& setConfig(const SpringConfig& config) {
        springX_.setConfig(config);
        springY_.setConfig(config);
        return *this;
    }
    
    Vec2 process(const Vec2& input) {
        return Vec2(
            springX_.process(input.x),
            springY_.process(input.y)
        );
    }
    
    Vec2 getVelocity() const {
        return Vec2(springX_.getVelocity(), springY_.getVelocity());
    }
    
    bool isAtRest(float threshold = 0.01f) const {
        return springX_.isAtRest(threshold) && springY_.isAtRest(threshold);
    }
    
    void snap(const Vec2& value) {
        springX_.snap(value.x);
        springY_.snap(value.y);
    }
    
    void reset() {
        springX_.reset();
        springY_.reset();
    }
    
private:
    SpringFilter springX_;
    SpringFilter springY_;
};

// ============================================================
// 3D Spring Filter
// ============================================================

class Spring3DFilter {
public:
    Spring3DFilter() = default;
    Spring3DFilter(const SpringConfig& config) 
        : springX_(config), springY_(config), springZ_(config) {}
    
    Spring3DFilter& setConfig(const SpringConfig& config) {
        springX_.setConfig(config);
        springY_.setConfig(config);
        springZ_.setConfig(config);
        return *this;
    }
    
    Vec3 process(const Vec3& input) {
        return Vec3(
            springX_.process(input.x),
            springY_.process(input.y),
            springZ_.process(input.z)
        );
    }
    
    Vec3 getVelocity() const {
        return Vec3(
            springX_.getVelocity(), 
            springY_.getVelocity(), 
            springZ_.getVelocity()
        );
    }
    
    bool isAtRest(float threshold = 0.01f) const {
        return springX_.isAtRest(threshold) && 
               springY_.isAtRest(threshold) && 
               springZ_.isAtRest(threshold);
    }
    
    void snap(const Vec3& value) {
        springX_.snap(value.x);
        springY_.snap(value.y);
        springZ_.snap(value.z);
    }
    
    void reset() {
        springX_.reset();
        springY_.reset();
        springZ_.reset();
    }
    
private:
    SpringFilter springX_;
    SpringFilter springY_;
    SpringFilter springZ_;
};

} // namespace AnimationDriver
