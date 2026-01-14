/*****************************************************************
 * @file FilterBase.hpp
 * @brief Base classes for value filtering and smoothing
 * 
 * Filters process input values for smoother animations,
 * noise reduction, and special effects like spring physics.
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include <cmath>
#include <algorithm>

namespace AnimationDriver {

// ============================================================
// Filter Type Identifier
// ============================================================

enum class FilterType : uint8_t {
    PASSTHROUGH,
    LOWPASS,
    HIGHPASS,
    MOVING_AVERAGE,
    EXPONENTIAL,
    DEADZONE,
    CLAMP,
    MAP,
    SPRING,
    THRESHOLD
};

// ============================================================
// Base Filter Interface
// ============================================================

template<typename T>
class FilterBase {
public:
    virtual ~FilterBase() = default;
    
    // Process value through filter
    virtual T process(const T& input) = 0;
    
    // Reset filter state
    virtual void reset() {}
    
    // Get filter type
    virtual FilterType getType() const { return FilterType::PASSTHROUGH; }
};

// ============================================================
// Passthrough Filter (no-op)
// ============================================================

template<typename T>
class PassthroughFilter : public FilterBase<T> {
public:
    T process(const T& input) override { return input; }
    FilterType getType() const override { return FilterType::PASSTHROUGH; }
};

// ============================================================
// Low-Pass Filter (Smoothing)
// ============================================================

class LowPassFilter : public FilterBase<float> {
public:
    LowPassFilter(float alpha = 0.1f) : alpha_(alpha), value_(0.0f), initialized_(false) {}
    
    LowPassFilter& setAlpha(float alpha) {
        alpha_ = std::max(0.0f, std::min(1.0f, alpha));
        return *this;
    }
    
    // Set cutoff by time constant (smoother API)
    LowPassFilter& setSmoothingTime(float seconds, float sampleRate = 60.0f) {
        if (seconds > 0.0f) {
            alpha_ = 1.0f - expf(-1.0f / (seconds * sampleRate));
        }
        return *this;
    }
    
    float process(const float& input) override {
        if (!initialized_) {
            value_ = input;
            initialized_ = true;
        } else {
            value_ = value_ + alpha_ * (input - value_);
        }
        return value_;
    }
    
    void reset() override {
        initialized_ = false;
        value_ = 0.0f;
    }
    
    FilterType getType() const override { return FilterType::LOWPASS; }
    
private:
    float alpha_;
    float value_;
    bool initialized_;
};

// ============================================================
// Moving Average Filter
// ============================================================

class MovingAverageFilter : public FilterBase<float> {
public:
    static constexpr int MAX_SAMPLES = 32;
    
    MovingAverageFilter(int windowSize = 5) 
        : windowSize_(std::min(windowSize, MAX_SAMPLES)), count_(0), index_(0), sum_(0.0f) {
        for (int i = 0; i < MAX_SAMPLES; i++) {
            samples_[i] = 0.0f;
        }
    }
    
    MovingAverageFilter& setWindowSize(int size) {
        windowSize_ = std::max(1, std::min(size, MAX_SAMPLES));
        reset();
        return *this;
    }
    
    float process(const float& input) override {
        // Remove oldest sample if buffer is full
        if (count_ >= windowSize_) {
            sum_ -= samples_[index_];
        }
        
        // Add new sample
        samples_[index_] = input;
        sum_ += input;
        
        // Update count and index
        if (count_ < windowSize_) count_++;
        index_ = (index_ + 1) % windowSize_;
        
        return sum_ / count_;
    }
    
    void reset() override {
        count_ = 0;
        index_ = 0;
        sum_ = 0.0f;
        for (int i = 0; i < MAX_SAMPLES; i++) {
            samples_[i] = 0.0f;
        }
    }
    
    FilterType getType() const override { return FilterType::MOVING_AVERAGE; }
    
private:
    float samples_[MAX_SAMPLES];
    int windowSize_;
    int count_;
    int index_;
    float sum_;
};

// ============================================================
// Exponential Moving Average Filter
// ============================================================

class ExponentialFilter : public FilterBase<float> {
public:
    ExponentialFilter(float alpha = 0.2f) : alpha_(alpha), value_(0.0f), initialized_(false) {}
    
    ExponentialFilter& setAlpha(float alpha) {
        alpha_ = std::max(0.0f, std::min(1.0f, alpha));
        return *this;
    }
    
    float process(const float& input) override {
        if (!initialized_) {
            value_ = input;
            initialized_ = true;
        } else {
            value_ = alpha_ * input + (1.0f - alpha_) * value_;
        }
        return value_;
    }
    
    void reset() override {
        initialized_ = false;
        value_ = 0.0f;
    }
    
    FilterType getType() const override { return FilterType::EXPONENTIAL; }
    
private:
    float alpha_;
    float value_;
    bool initialized_;
};

// ============================================================
// Deadzone Filter
// ============================================================

class DeadzoneFilter : public FilterBase<float> {
public:
    DeadzoneFilter(float threshold = 0.1f, float center = 0.0f) 
        : threshold_(threshold), center_(center), smooth_(true) {}
    
    DeadzoneFilter& setThreshold(float threshold) {
        threshold_ = fabsf(threshold);
        return *this;
    }
    
    DeadzoneFilter& setCenter(float center) {
        center_ = center;
        return *this;
    }
    
    DeadzoneFilter& setSmooth(bool smooth) {
        smooth_ = smooth;
        return *this;
    }
    
    float process(const float& input) override {
        float diff = input - center_;
        float absDiff = fabsf(diff);
        
        if (absDiff < threshold_) {
            return center_;
        }
        
        if (smooth_ && threshold_ > 0.0f) {
            // Smooth transition from deadzone
            float sign = (diff > 0) ? 1.0f : -1.0f;
            return center_ + sign * (absDiff - threshold_);
        }
        
        return input;
    }
    
    FilterType getType() const override { return FilterType::DEADZONE; }
    
private:
    float threshold_;
    float center_;
    bool smooth_;
};

// ============================================================
// Clamp Filter
// ============================================================

class ClampFilter : public FilterBase<float> {
public:
    ClampFilter(float min = 0.0f, float max = 1.0f) : min_(min), max_(max) {}
    
    ClampFilter& setRange(float min, float max) {
        min_ = min;
        max_ = max;
        return *this;
    }
    
    float process(const float& input) override {
        return std::max(min_, std::min(max_, input));
    }
    
    FilterType getType() const override { return FilterType::CLAMP; }
    
private:
    float min_;
    float max_;
};

// ============================================================
// Map/Scale Filter
// ============================================================

class MapFilter : public FilterBase<float> {
public:
    MapFilter() : inMin_(0), inMax_(1), outMin_(0), outMax_(1), clamp_(true) {}
    
    MapFilter& setInputRange(float min, float max) {
        inMin_ = min;
        inMax_ = max;
        return *this;
    }
    
    MapFilter& setOutputRange(float min, float max) {
        outMin_ = min;
        outMax_ = max;
        return *this;
    }
    
    MapFilter& setClamp(bool clamp) {
        clamp_ = clamp;
        return *this;
    }
    
    float process(const float& input) override {
        float normalized = (inMax_ != inMin_) 
            ? (input - inMin_) / (inMax_ - inMin_) 
            : 0.0f;
        
        if (clamp_) {
            normalized = std::max(0.0f, std::min(1.0f, normalized));
        }
        
        return outMin_ + normalized * (outMax_ - outMin_);
    }
    
    FilterType getType() const override { return FilterType::MAP; }
    
private:
    float inMin_, inMax_;
    float outMin_, outMax_;
    bool clamp_;
};

// ============================================================
// Threshold Filter (binary output)
// ============================================================

class ThresholdFilter : public FilterBase<float> {
public:
    ThresholdFilter(float threshold = 0.5f, float hysteresis = 0.0f) 
        : threshold_(threshold), hysteresis_(hysteresis), state_(false) {}
    
    ThresholdFilter& setThreshold(float threshold) {
        threshold_ = threshold;
        return *this;
    }
    
    ThresholdFilter& setHysteresis(float hysteresis) {
        hysteresis_ = fabsf(hysteresis);
        return *this;
    }
    
    ThresholdFilter& setOutputValues(float low, float high) {
        lowVal_ = low;
        highVal_ = high;
        return *this;
    }
    
    float process(const float& input) override {
        if (state_) {
            // Currently high, need to go below (threshold - hysteresis) to go low
            if (input < threshold_ - hysteresis_) {
                state_ = false;
            }
        } else {
            // Currently low, need to go above (threshold + hysteresis) to go high
            if (input > threshold_ + hysteresis_) {
                state_ = true;
            }
        }
        
        return state_ ? highVal_ : lowVal_;
    }
    
    void reset() override {
        state_ = false;
    }
    
    FilterType getType() const override { return FilterType::THRESHOLD; }
    
private:
    float threshold_;
    float hysteresis_;
    bool state_;
    float lowVal_ = 0.0f;
    float highVal_ = 1.0f;
};

} // namespace AnimationDriver
