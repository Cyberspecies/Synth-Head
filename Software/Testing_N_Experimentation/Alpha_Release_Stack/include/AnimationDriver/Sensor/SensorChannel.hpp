/*****************************************************************
 * @file SensorChannel.hpp
 * @brief Generic sensor channel for any data source
 * 
 * A channel represents a single data stream from any sensor type.
 * Includes transformation, filtering, and normalization.
 *****************************************************************/

#pragma once

#include "SensorTypes.hpp"
#include "SensorTransform.hpp"
#include "../Binding/FilterChain.hpp"
#include <functional>
#include <cstring>

namespace AnimationDriver {

// ============================================================
// Raw Value Provider
// ============================================================

using RawValueProvider = std::function<SensorValue()>;
using ScalarProvider = std::function<float()>;
using Vec3Provider = std::function<Vec3()>;

// ============================================================
// Sensor Channel
// ============================================================

class SensorChannel {
public:
    static constexpr int MAX_NAME_LEN = 24;
    
    SensorChannel() : enabled_(true), initialized_(false) {
        name_[0] = '\0';
    }
    
    SensorChannel(const char* name) : SensorChannel() {
        setName(name);
    }
    
    // Naming
    SensorChannel& setName(const char* name) {
        strncpy(name_, name, MAX_NAME_LEN - 1);
        name_[MAX_NAME_LEN - 1] = '\0';
        return *this;
    }
    
    const char* getName() const { return name_; }
    
    // Set info
    SensorChannel& setInfo(const SensorInfo& info) {
        info_ = info;
        return *this;
    }
    
    SensorChannel& setCategory(SensorCategory category) {
        info_.category = category;
        return *this;
    }
    
    SensorChannel& setValueType(SensorValueType type) {
        info_.valueType = type;
        return *this;
    }
    
    SensorChannel& setRange(float min, float max) {
        info_.minValue = min;
        info_.maxValue = max;
        return *this;
    }
    
    // ========================================================
    // Data Source Configuration
    // ========================================================
    
    // Set generic value provider
    SensorChannel& setProvider(RawValueProvider provider) {
        rawProvider_ = provider;
        return *this;
    }
    
    // Set scalar provider (convenience)
    SensorChannel& setScalarProvider(ScalarProvider provider) {
        rawProvider_ = [provider]() {
            return SensorValue(provider());
        };
        info_.valueType = SensorValueType::SCALAR;
        return *this;
    }
    
    // Set Vec3 provider (convenience for IMU, etc.)
    SensorChannel& setVec3Provider(
        ScalarProvider xProv,
        ScalarProvider yProv,
        ScalarProvider zProv
    ) {
        rawProvider_ = [xProv, yProv, zProv]() {
            return SensorValue(xProv(), yProv(), zProv());
        };
        info_.valueType = SensorValueType::VEC3;
        return *this;
    }
    
    // ========================================================
    // Transform Configuration
    // ========================================================
    
    SensorChannel& setTransform(const SensorTransform& transform) {
        transform_ = transform;
        return *this;
    }
    
    SensorTransform& transform() { return transform_; }
    
    // Mounting shortcuts
    SensorChannel& setMounting(float pitch, float roll = 0.0f, float yaw = 0.0f) {
        transform_.setMounting(pitch, roll, yaw);
        return *this;
    }
    
    SensorChannel& setOffset(float x, float y = 0.0f, float z = 0.0f) {
        transform_.setOffset(x, y, z);
        return *this;
    }
    
    // ========================================================
    // Filter Configuration
    // ========================================================
    
    FilterChain& filter() { return filter_; }
    
    // Filter shortcuts
    SensorChannel& smooth(float seconds) {
        filter_.smooth(seconds);
        return *this;
    }
    
    SensorChannel& deadzone(float threshold) {
        filter_.deadzone(threshold);
        return *this;
    }
    
    SensorChannel& clamp(float min, float max) {
        filter_.clamp(min, max);
        return *this;
    }
    
    SensorChannel& spring(const SpringConfig& config = SpringConfig::Smooth()) {
        filter_.spring(config);
        return *this;
    }
    
    // ========================================================
    // Normalization
    // ========================================================
    
    SensorChannel& setNormalize(bool enable) {
        normalize_ = enable;
        return *this;
    }
    
    SensorChannel& setNormalizeRange(float outMin, float outMax) {
        normalizeOutMin_ = outMin;
        normalizeOutMax_ = outMax;
        normalize_ = true;
        return *this;
    }
    
    // ========================================================
    // Reading Values
    // ========================================================
    
    // Update channel (call each frame)
    void update() {
        if (!enabled_ || !rawProvider_) return;
        
        // Get raw value
        rawValue_ = rawProvider_();
        
        // Apply transform
        transformedValue_ = transform_.transform(rawValue_);
        
        // Apply filter (for scalar values)
        if (transformedValue_.type == SensorValueType::SCALAR) {
            float filtered = filter_.process(transformedValue_.scalar);
            transformedValue_ = SensorValue(filtered);
        }
        
        // Apply normalization
        if (normalize_) {
            processedValue_ = normalizeValue(transformedValue_);
        } else {
            processedValue_ = transformedValue_;
        }
        
        initialized_ = true;
    }
    
    // Get raw value (before transform)
    SensorValue getRaw() const { return rawValue_; }
    
    // Get transformed value (after transform, before filter)
    SensorValue getTransformed() const { return transformedValue_; }
    
    // Get processed value (final output)
    SensorValue get() const { return processedValue_; }
    
    // Convenience getters
    float getScalar() const { return processedValue_.asScalar(); }
    Vec2 getVec2() const { return processedValue_.asVec2(); }
    Vec3 getVec3() const { return processedValue_.asVec3(); }
    float getMagnitude() const { return processedValue_.magnitude(); }
    
    // Get as provider function (for binding)
    ValueProvider asScalarProvider() const {
        return [this]() { return this->getScalar(); };
    }
    
    Vec2Provider asVec2Provider() const {
        return [this]() { return this->getVec2(); };
    }
    
    Vec3Provider asVec3Provider() const {
        return [this]() { return this->getVec3(); };
    }
    
    // ========================================================
    // State
    // ========================================================
    
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    bool isInitialized() const { return initialized_; }
    
    void reset() {
        filter_.reset();
        initialized_ = false;
    }
    
private:
    SensorValue normalizeValue(const SensorValue& input) const {
        float range = info_.maxValue - info_.minValue;
        if (range <= 0.0f) return input;
        
        float outRange = normalizeOutMax_ - normalizeOutMin_;
        
        switch (input.type) {
            case SensorValueType::SCALAR: {
                float norm = (input.scalar - info_.minValue) / range;
                return SensorValue(normalizeOutMin_ + norm * outRange);
            }
            case SensorValueType::VEC2: {
                float nx = (input.vec2.x - info_.minValue) / range;
                float ny = (input.vec2.y - info_.minValue) / range;
                return SensorValue(
                    normalizeOutMin_ + nx * outRange,
                    normalizeOutMin_ + ny * outRange
                );
            }
            case SensorValueType::VEC3: {
                float nx = (input.vec3.x - info_.minValue) / range;
                float ny = (input.vec3.y - info_.minValue) / range;
                float nz = (input.vec3.z - info_.minValue) / range;
                return SensorValue(
                    normalizeOutMin_ + nx * outRange,
                    normalizeOutMin_ + ny * outRange,
                    normalizeOutMin_ + nz * outRange
                );
            }
            default:
                return input;
        }
    }
    
    char name_[MAX_NAME_LEN];
    SensorInfo info_;
    
    RawValueProvider rawProvider_;
    SensorTransform transform_;
    FilterChain filter_;
    
    SensorValue rawValue_;
    SensorValue transformedValue_;
    SensorValue processedValue_;
    
    bool enabled_;
    bool initialized_;
    bool normalize_ = false;
    float normalizeOutMin_ = 0.0f;
    float normalizeOutMax_ = 1.0f;
};

} // namespace AnimationDriver
