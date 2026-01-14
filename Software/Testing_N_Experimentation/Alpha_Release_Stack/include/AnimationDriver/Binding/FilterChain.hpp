/*****************************************************************
 * @file FilterChain.hpp
 * @brief Chain multiple filters together for complex processing
 *****************************************************************/

#pragma once

#include "FilterBase.hpp"
#include "SpringFilter.hpp"
#include <memory>

namespace AnimationDriver {

// ============================================================
// Filter Chain - Process through multiple filters
// ============================================================

class FilterChain : public FilterBase<float> {
public:
    static constexpr int MAX_FILTERS = 8;
    
    FilterChain() : count_(0) {}
    
    // Add filter to chain (takes ownership)
    template<typename T>
    FilterChain& add(T* filter) {
        if (count_ < MAX_FILTERS && filter) {
            filters_[count_++] = filter;
        }
        return *this;
    }
    
    // Add low-pass filter
    FilterChain& lowPass(float alpha) {
        if (count_ < MAX_FILTERS) {
            lpFilters_[lpCount_].setAlpha(alpha);
            filters_[count_++] = &lpFilters_[lpCount_++];
        }
        return *this;
    }
    
    // Add smoothing by time
    FilterChain& smooth(float seconds) {
        if (count_ < MAX_FILTERS && lpCount_ < 4) {
            lpFilters_[lpCount_].setSmoothingTime(seconds);
            filters_[count_++] = &lpFilters_[lpCount_++];
        }
        return *this;
    }
    
    // Add deadzone filter
    FilterChain& deadzone(float threshold, float center = 0.0f) {
        if (count_ < MAX_FILTERS && dzCount_ < 4) {
            dzFilters_[dzCount_].setThreshold(threshold);
            dzFilters_[dzCount_].setCenter(center);
            filters_[count_++] = &dzFilters_[dzCount_++];
        }
        return *this;
    }
    
    // Add clamp filter
    FilterChain& clamp(float min, float max) {
        if (count_ < MAX_FILTERS && clampCount_ < 4) {
            clampFilters_[clampCount_].setRange(min, max);
            filters_[count_++] = &clampFilters_[clampCount_++];
        }
        return *this;
    }
    
    // Add map/scale filter
    FilterChain& map(float inMin, float inMax, float outMin, float outMax) {
        if (count_ < MAX_FILTERS && mapCount_ < 4) {
            mapFilters_[mapCount_].setInputRange(inMin, inMax);
            mapFilters_[mapCount_].setOutputRange(outMin, outMax);
            filters_[count_++] = &mapFilters_[mapCount_++];
        }
        return *this;
    }
    
    // Add spring filter
    FilterChain& spring(const SpringConfig& config = SpringConfig::Smooth()) {
        if (count_ < MAX_FILTERS && springCount_ < 2) {
            springFilters_[springCount_].setConfig(config);
            filters_[count_++] = &springFilters_[springCount_++];
        }
        return *this;
    }
    
    // Process through all filters
    float process(const float& input) override {
        float value = input;
        for (int i = 0; i < count_; i++) {
            if (filters_[i]) {
                value = filters_[i]->process(value);
            }
        }
        return value;
    }
    
    // Reset all filters
    void reset() override {
        for (int i = 0; i < count_; i++) {
            if (filters_[i]) {
                filters_[i]->reset();
            }
        }
    }
    
    // Get filter count
    int getFilterCount() const { return count_; }
    
    // Clear all filters
    void clear() {
        count_ = 0;
        lpCount_ = 0;
        dzCount_ = 0;
        clampCount_ = 0;
        mapCount_ = 0;
        springCount_ = 0;
    }
    
private:
    FilterBase<float>* filters_[MAX_FILTERS];
    int count_;
    
    // Pre-allocated filter storage
    LowPassFilter lpFilters_[4];
    int lpCount_ = 0;
    
    DeadzoneFilter dzFilters_[4];
    int dzCount_ = 0;
    
    ClampFilter clampFilters_[4];
    int clampCount_ = 0;
    
    MapFilter mapFilters_[4];
    int mapCount_ = 0;
    
    SpringFilter springFilters_[2];
    int springCount_ = 0;
};

// ============================================================
// Filter Factory - Create common filter configurations
// ============================================================

namespace FilterFactory {

// Create a smooth IMU filter chain
inline FilterChain createIMUFilter(float smoothing = 0.1f, float deadzone = 0.02f) {
    FilterChain chain;
    chain.deadzone(deadzone)
         .smooth(smoothing)
         .clamp(-1.0f, 1.0f);
    return chain;
}

// Create a position filter with spring physics
inline FilterChain createPositionFilter(float smoothing = 0.1f) {
    FilterChain chain;
    chain.smooth(smoothing)
         .spring(SpringConfig::Smooth());
    return chain;
}

// Create a velocity filter (for gyro data)
inline FilterChain createVelocityFilter(float smoothing = 0.05f, float deadzone = 5.0f) {
    FilterChain chain;
    chain.deadzone(deadzone)
         .smooth(smoothing);
    return chain;
}

// Create a trigger/threshold filter
inline FilterChain createTriggerFilter(float threshold = 0.5f, float hysteresis = 0.1f) {
    FilterChain chain;
    // Note: Would need to add threshold filter support to chain
    chain.smooth(0.05f);
    return chain;
}

// Create a normalized output filter (0-1 range)
inline FilterChain createNormalizedFilter(float inMin, float inMax) {
    FilterChain chain;
    chain.map(inMin, inMax, 0.0f, 1.0f)
         .clamp(0.0f, 1.0f);
    return chain;
}

} // namespace FilterFactory

} // namespace AnimationDriver
