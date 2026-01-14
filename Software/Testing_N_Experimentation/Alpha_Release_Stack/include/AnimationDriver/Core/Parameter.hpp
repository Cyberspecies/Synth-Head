/*****************************************************************
 * @file Parameter.hpp
 * @brief Animation parameters with binding support
 * 
 * Parameters can be static values or bound to external data sources
 * like IMU readings, audio levels, or time-based expressions.
 *****************************************************************/

#pragma once

#include "Types.hpp"
#include "Color.hpp"
#include <functional>
#include <cstring>

namespace AnimationDriver {

// ============================================================
// Parameter Source Type
// ============================================================

enum class ParamSource : uint8_t {
    STATIC,         // Fixed value
    BOUND,          // Bound to external value provider
    EXPRESSION,     // Computed from expression
    OSCILLATOR      // Built-in oscillator
};

// ============================================================
// Oscillator Type
// ============================================================

enum class OscillatorType : uint8_t {
    SINE,
    COSINE,
    TRIANGLE,
    SAWTOOTH,
    SQUARE,
    NOISE,
    PERLIN
};

// ============================================================
// Parameter Class Template
// ============================================================

template<typename T>
class Parameter {
public:
    Parameter() : source_(ParamSource::STATIC), value_(T{}) {}
    Parameter(const T& value) : source_(ParamSource::STATIC), value_(value) {}
    
    // Set static value
    Parameter& set(const T& value) {
        source_ = ParamSource::STATIC;
        value_ = value;
        return *this;
    }
    
    // Bind to value provider
    Parameter& bind(std::function<T()> provider) {
        source_ = ParamSource::BOUND;
        provider_ = provider;
        return *this;
    }
    
    // Get current value
    T get() const {
        switch (source_) {
            case ParamSource::BOUND:
                return provider_ ? provider_() : value_;
            case ParamSource::STATIC:
            default:
                return value_;
        }
    }
    
    // Check if bound
    bool isBound() const { return source_ == ParamSource::BOUND; }
    bool isStatic() const { return source_ == ParamSource::STATIC; }
    
    // Operator overloads for convenience
    operator T() const { return get(); }
    Parameter& operator=(const T& value) { return set(value); }

private:
    ParamSource source_;
    T value_;
    std::function<T()> provider_;
};

// ============================================================
// Float Parameter with Oscillator Support
// ============================================================

class FloatParam {
public:
    FloatParam() : source_(ParamSource::STATIC), value_(0.0f) {}
    FloatParam(float value) : source_(ParamSource::STATIC), value_(value) {}
    
    // Set static value
    FloatParam& set(float value) {
        source_ = ParamSource::STATIC;
        value_ = value;
        return *this;
    }
    
    // Bind to value provider
    FloatParam& bind(ValueProvider provider) {
        source_ = ParamSource::BOUND;
        provider_ = provider;
        return *this;
    }
    
    // Set up oscillator
    FloatParam& oscillate(OscillatorType type, float frequency, float amplitude = 1.0f, float phase = 0.0f) {
        source_ = ParamSource::OSCILLATOR;
        oscType_ = type;
        oscFreq_ = frequency;
        oscAmp_ = amplitude;
        oscPhase_ = phase;
        return *this;
    }
    
    // Sine wave shortcut
    FloatParam& sine(float frequency, float amplitude = 1.0f) {
        return oscillate(OscillatorType::SINE, frequency, amplitude);
    }
    
    // Triangle wave shortcut
    FloatParam& triangle(float frequency, float amplitude = 1.0f) {
        return oscillate(OscillatorType::TRIANGLE, frequency, amplitude);
    }
    
    // Set range for mapping
    FloatParam& range(float min, float max) {
        range_ = ValueRange(min, max);
        return *this;
    }
    
    // Get current value with optional time input
    float get(float time = 0.0f) const {
        float raw = 0.0f;
        
        switch (source_) {
            case ParamSource::BOUND:
                raw = provider_ ? provider_() : value_;
                break;
            case ParamSource::OSCILLATOR:
                raw = computeOscillator(time);
                break;
            case ParamSource::STATIC:
            default:
                raw = value_;
                break;
        }
        
        // Apply range mapping if set
        if (range_.max != range_.min) {
            raw = range_.min + raw * (range_.max - range_.min);
        }
        
        return raw;
    }
    
    operator float() const { return get(); }
    FloatParam& operator=(float value) { return set(value); }

private:
    float computeOscillator(float time) const {
        float phase = time * oscFreq_ * 2.0f * 3.14159f + oscPhase_;
        float val = 0.0f;
        
        switch (oscType_) {
            case OscillatorType::SINE:
                val = sinf(phase);
                break;
            case OscillatorType::COSINE:
                val = cosf(phase);
                break;
            case OscillatorType::TRIANGLE:
                val = 2.0f * fabsf(2.0f * (phase / (2.0f * 3.14159f) - floorf(phase / (2.0f * 3.14159f) + 0.5f))) - 1.0f;
                break;
            case OscillatorType::SAWTOOTH:
                val = 2.0f * (phase / (2.0f * 3.14159f) - floorf(phase / (2.0f * 3.14159f) + 0.5f));
                break;
            case OscillatorType::SQUARE:
                val = sinf(phase) >= 0 ? 1.0f : -1.0f;
                break;
            default:
                val = 0.0f;
        }
        
        return val * oscAmp_;
    }
    
    ParamSource source_;
    float value_ = 0.0f;
    ValueProvider provider_;
    
    // Oscillator params
    OscillatorType oscType_ = OscillatorType::SINE;
    float oscFreq_ = 1.0f;
    float oscAmp_ = 1.0f;
    float oscPhase_ = 0.0f;
    
    // Range mapping
    ValueRange range_{0.0f, 0.0f};  // 0,0 = no mapping
};

// ============================================================
// Color Parameter
// ============================================================

class ColorParam {
public:
    ColorParam() : source_(ParamSource::STATIC), color_(RGB::White()) {}
    ColorParam(const RGB& color) : source_(ParamSource::STATIC), color_(color) {}
    ColorParam(const HSV& hsv) : source_(ParamSource::STATIC), color_(hsv.toRGB()), hsv_(hsv), useHsv_(true) {}
    
    // Set static color
    ColorParam& set(const RGB& color) {
        source_ = ParamSource::STATIC;
        color_ = color;
        useHsv_ = false;
        return *this;
    }
    
    ColorParam& set(const HSV& hsv) {
        source_ = ParamSource::STATIC;
        hsv_ = hsv;
        color_ = hsv.toRGB();
        useHsv_ = true;
        return *this;
    }
    
    // Bind to provider
    ColorParam& bind(std::function<RGB()> provider) {
        source_ = ParamSource::BOUND;
        rgbProvider_ = provider;
        return *this;
    }
    
    // Cycling hue (rainbow effect)
    ColorParam& cycleHue(float speed = 1.0f, float saturation = 1.0f, float value = 1.0f) {
        source_ = ParamSource::OSCILLATOR;
        hueSpeed_ = speed;
        hsv_ = HSV(0.0f, saturation, value);
        return *this;
    }
    
    // Get current color
    RGB get(float time = 0.0f) const {
        switch (source_) {
            case ParamSource::BOUND:
                return rgbProvider_ ? rgbProvider_() : color_;
            case ParamSource::OSCILLATOR:
                return HSV(fmodf(time * hueSpeed_ * 360.0f, 360.0f), hsv_.s, hsv_.v).toRGB();
            case ParamSource::STATIC:
            default:
                return color_;
        }
    }
    
    operator RGB() const { return get(); }

private:
    ParamSource source_;
    RGB color_;
    HSV hsv_;
    bool useHsv_ = false;
    float hueSpeed_ = 1.0f;
    std::function<RGB()> rgbProvider_;
};

// ============================================================
// Named Parameter Collection
// ============================================================

class ParameterSet {
public:
    static constexpr int MAX_PARAMS = 16;
    static constexpr int MAX_NAME_LEN = 16;
    
    ParameterSet() : count_(0) {}
    
    // Add/set float parameter
    ParameterSet& setFloat(const char* name, float value) {
        int idx = findOrCreate(name);
        if (idx >= 0) {
            params_[idx].floatVal = value;
            params_[idx].type = 0;  // float
        }
        return *this;
    }
    
    // Add/set int parameter
    ParameterSet& setInt(const char* name, int value) {
        int idx = findOrCreate(name);
        if (idx >= 0) {
            params_[idx].intVal = value;
            params_[idx].type = 1;  // int
        }
        return *this;
    }
    
    // Get float parameter
    float getFloat(const char* name, float defaultVal = 0.0f) const {
        int idx = find(name);
        return (idx >= 0 && params_[idx].type == 0) ? params_[idx].floatVal : defaultVal;
    }
    
    // Get int parameter
    int getInt(const char* name, int defaultVal = 0) const {
        int idx = find(name);
        return (idx >= 0 && params_[idx].type == 1) ? params_[idx].intVal : defaultVal;
    }
    
private:
    struct Param {
        char name[MAX_NAME_LEN];
        uint8_t type;  // 0=float, 1=int, 2=color
        union {
            float floatVal;
            int intVal;
            uint32_t colorVal;
        };
    };
    
    Param params_[MAX_PARAMS];
    int count_;
    
    int find(const char* name) const {
        for (int i = 0; i < count_; i++) {
            if (strncmp(params_[i].name, name, MAX_NAME_LEN) == 0) {
                return i;
            }
        }
        return -1;
    }
    
    int findOrCreate(const char* name) {
        int idx = find(name);
        if (idx >= 0) return idx;
        if (count_ >= MAX_PARAMS) return -1;
        
        strncpy(params_[count_].name, name, MAX_NAME_LEN - 1);
        params_[count_].name[MAX_NAME_LEN - 1] = '\0';
        return count_++;
    }
};

} // namespace AnimationDriver
