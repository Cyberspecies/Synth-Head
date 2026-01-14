/*****************************************************************
 * @file ValueBinding.hpp
 * @brief Bind external values to animation parameters
 * 
 * Creates connections between data sources (IMU, sensors, time)
 * and animation parameters with optional filtering and mapping.
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include "../Core/Parameter.hpp"
#include "FilterChain.hpp"
#include <functional>

namespace AnimationDriver {

// ============================================================
// Value Source Type
// ============================================================

enum class BindingSource : uint8_t {
    CUSTOM,         // Custom provider function
    TIME,           // Animation time
    FRAME,          // Frame number
    IMU_ACCEL_X,    // Accelerometer X
    IMU_ACCEL_Y,    // Accelerometer Y
    IMU_ACCEL_Z,    // Accelerometer Z
    IMU_GYRO_X,     // Gyroscope X
    IMU_GYRO_Y,     // Gyroscope Y
    IMU_GYRO_Z,     // Gyroscope Z
    IMU_PITCH,      // Computed pitch angle
    IMU_ROLL,       // Computed roll angle
    AUDIO_LEVEL,    // Audio input level
    POTENTIOMETER,  // Analog input
    BUTTON          // Digital input
};

// ============================================================
// Value Binding Class
// ============================================================

class ValueBinding {
public:
    ValueBinding() : source_(BindingSource::CUSTOM), enabled_(true) {}
    
    // Set custom value provider
    ValueBinding& setProvider(ValueProvider provider) {
        provider_ = provider;
        source_ = BindingSource::CUSTOM;
        return *this;
    }
    
    // Set source type
    ValueBinding& setSource(BindingSource source) {
        source_ = source;
        return *this;
    }
    
    // Set offset (e.g., for IMU calibration or mounting angle compensation)
    ValueBinding& setOffset(float offset) {
        offset_ = offset;
        return *this;
    }
    
    // Set scale factor
    ValueBinding& setScale(float scale) {
        scale_ = scale;
        return *this;
    }
    
    // Set input range for normalization
    ValueBinding& setInputRange(float min, float max) {
        inMin_ = min;
        inMax_ = max;
        return *this;
    }
    
    // Set output range
    ValueBinding& setOutputRange(float min, float max) {
        outMin_ = min;
        outMax_ = max;
        return *this;
    }
    
    // Set filter chain
    ValueBinding& setFilter(const FilterChain& filter) {
        filter_ = filter;
        useFilter_ = true;
        return *this;
    }
    
    // Add smoothing
    ValueBinding& smooth(float seconds) {
        filter_.smooth(seconds);
        useFilter_ = true;
        return *this;
    }
    
    // Add deadzone
    ValueBinding& deadzone(float threshold) {
        filter_.deadzone(threshold);
        useFilter_ = true;
        return *this;
    }
    
    // Add spring physics
    ValueBinding& spring(const SpringConfig& config = SpringConfig::Smooth()) {
        filter_.spring(config);
        useFilter_ = true;
        return *this;
    }
    
    // Invert value
    ValueBinding& invert(bool inv = true) {
        inverted_ = inv;
        return *this;
    }
    
    // Enable/disable
    ValueBinding& setEnabled(bool enabled) {
        enabled_ = enabled;
        return *this;
    }
    
    // Get current processed value
    float get() const {
        if (!enabled_ || !provider_) {
            return defaultValue_;
        }
        
        float raw = provider_();
        
        // Apply offset and scale
        float value = (raw + offset_) * scale_;
        
        // Apply input range normalization if set
        if (inMax_ != inMin_) {
            value = (value - inMin_) / (inMax_ - inMin_);
        }
        
        // Apply filter if set
        if (useFilter_) {
            // Filter is mutable for this purpose
            value = const_cast<FilterChain&>(filter_).process(value);
        }
        
        // Apply output range mapping if set
        if (outMax_ != outMin_) {
            value = outMin_ + value * (outMax_ - outMin_);
        }
        
        // Apply inversion
        if (inverted_) {
            value = outMax_ - (value - outMin_);
        }
        
        return value;
    }
    
    // Get raw value (no processing)
    float getRaw() const {
        return provider_ ? provider_() : defaultValue_;
    }
    
    // Set default value when source unavailable
    ValueBinding& setDefault(float value) {
        defaultValue_ = value;
        return *this;
    }
    
    // Reset filter state
    void reset() {
        filter_.reset();
    }
    
    // Check if binding is valid
    bool isValid() const { return provider_ != nullptr; }
    bool isEnabled() const { return enabled_; }
    
    // Get as function object for parameter binding
    ValueProvider asProvider() const {
        return [this]() { return this->get(); };
    }
    
private:
    BindingSource source_;
    ValueProvider provider_;
    FilterChain filter_;
    bool useFilter_ = false;
    bool enabled_;
    bool inverted_ = false;
    
    float offset_ = 0.0f;
    float scale_ = 1.0f;
    float inMin_ = 0.0f;
    float inMax_ = 0.0f;
    float outMin_ = 0.0f;
    float outMax_ = 0.0f;
    float defaultValue_ = 0.0f;
};

// ============================================================
// 2D Value Binding
// ============================================================

class ValueBinding2D {
public:
    ValueBinding2D() = default;
    
    // Set providers for X and Y
    ValueBinding2D& setProviders(ValueProvider xProv, ValueProvider yProv) {
        bindingX_.setProvider(xProv);
        bindingY_.setProvider(yProv);
        return *this;
    }
    
    // Access individual axis bindings for configuration
    ValueBinding& x() { return bindingX_; }
    ValueBinding& y() { return bindingY_; }
    
    // Configure both axes at once
    ValueBinding2D& setOffset(float x, float y) {
        bindingX_.setOffset(x);
        bindingY_.setOffset(y);
        return *this;
    }
    
    ValueBinding2D& setScale(float x, float y) {
        bindingX_.setScale(x);
        bindingY_.setScale(y);
        return *this;
    }
    
    ValueBinding2D& smooth(float seconds) {
        bindingX_.smooth(seconds);
        bindingY_.smooth(seconds);
        return *this;
    }
    
    ValueBinding2D& spring(const SpringConfig& config = SpringConfig::Smooth()) {
        bindingX_.spring(config);
        bindingY_.spring(config);
        return *this;
    }
    
    // Get processed value
    Vec2 get() const {
        return Vec2(bindingX_.get(), bindingY_.get());
    }
    
    Vec2 getRaw() const {
        return Vec2(bindingX_.getRaw(), bindingY_.getRaw());
    }
    
    void reset() {
        bindingX_.reset();
        bindingY_.reset();
    }
    
    Vec2Provider asProvider() const {
        return [this]() { return this->get(); };
    }
    
private:
    ValueBinding bindingX_;
    ValueBinding bindingY_;
};

// ============================================================
// 3D Value Binding
// ============================================================

class ValueBinding3D {
public:
    ValueBinding3D() = default;
    
    ValueBinding3D& setProviders(ValueProvider xProv, ValueProvider yProv, ValueProvider zProv) {
        bindingX_.setProvider(xProv);
        bindingY_.setProvider(yProv);
        bindingZ_.setProvider(zProv);
        return *this;
    }
    
    ValueBinding& x() { return bindingX_; }
    ValueBinding& y() { return bindingY_; }
    ValueBinding& z() { return bindingZ_; }
    
    ValueBinding3D& setOffset(float x, float y, float z) {
        bindingX_.setOffset(x);
        bindingY_.setOffset(y);
        bindingZ_.setOffset(z);
        return *this;
    }
    
    ValueBinding3D& smooth(float seconds) {
        bindingX_.smooth(seconds);
        bindingY_.smooth(seconds);
        bindingZ_.smooth(seconds);
        return *this;
    }
    
    Vec3 get() const {
        return Vec3(bindingX_.get(), bindingY_.get(), bindingZ_.get());
    }
    
    void reset() {
        bindingX_.reset();
        bindingY_.reset();
        bindingZ_.reset();
    }
    
    Vec3Provider asProvider() const {
        return [this]() { return this->get(); };
    }
    
private:
    ValueBinding bindingX_;
    ValueBinding bindingY_;
    ValueBinding bindingZ_;
};

} // namespace AnimationDriver
