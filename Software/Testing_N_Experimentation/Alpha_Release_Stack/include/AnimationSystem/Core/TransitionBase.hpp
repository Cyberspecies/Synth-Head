/*****************************************************************
 * @file TransitionBase.hpp
 * @brief Base class for animation transitions
 * 
 * Transitions blend between the previous and next animation states.
 * They capture the "from" frame and blend with the "to" frame.
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "../AnimationTypes.hpp"
#include "ShaderBase.hpp"  // For ParamType
#include <string>
#include <map>
#include <functional>
#include <cstdint>
#include <cmath>

namespace AnimationSystem {

/**
 * @brief Transition parameter definition
 */
struct TransitionParamDef {
    std::string id;
    std::string name;
    std::string description;
    ParamType type;
    float defaultVal;
    float minVal;
    float maxVal;
    std::string category;
};

/**
 * @brief Base class for all transitions
 */
class TransitionBase {
public:
    TransitionBase() = default;
    virtual ~TransitionBase() = default;
    
    // ========================================================
    // Identification (required overrides)
    // ========================================================
    
    virtual const char* getTypeId() const = 0;
    virtual const char* getDisplayName() const = 0;
    virtual const char* getDescription() const { return ""; }
    
    // ========================================================
    // Lifecycle
    // ========================================================
    
    virtual void init() {}
    virtual void update(uint32_t deltaMs) = 0;
    virtual void render() = 0;
    
    /**
     * @brief Start the transition
     * @param durationMs Total transition time in milliseconds
     */
    virtual void start(uint32_t durationMs) {
        durationMs_ = durationMs;
        elapsedMs_ = 0;
        active_ = true;
        complete_ = false;
    }
    
    /**
     * @brief Capture the current frame as the "from" state
     */
    void captureFromFrame() {
        // This would be implemented by the pipeline
        // For now, mark as having captured
    }
    
    // ========================================================
    // State
    // ========================================================
    
    bool isActive() const { return active_; }
    bool isComplete() const { return complete_; }
    float getProgress() const {
        if (durationMs_ == 0) return 1.0f;
        return static_cast<float>(elapsedMs_) / static_cast<float>(durationMs_);
    }
    
    // ========================================================
    // Parameters
    // ========================================================
    
    void defineParam(const std::string& id, const std::string& name,
                     const std::string& desc, ParamType type,
                     float defaultVal, float minVal = 0, float maxVal = 1,
                     const std::string& category = "General") {
        TransitionParamDef def;
        def.id = id;
        def.name = name;
        def.description = desc;
        def.type = type;
        def.defaultVal = defaultVal;
        def.minVal = minVal;
        def.maxVal = maxVal;
        def.category = category;
        paramDefs_[id] = def;
        paramValues_[id] = defaultVal;
    }
    
    float getParam(const std::string& id) const {
        auto it = paramValues_.find(id);
        return it != paramValues_.end() ? it->second : 0.0f;
    }
    
    int getParamInt(const std::string& id) const {
        return static_cast<int>(getParam(id));
    }
    
    bool getParamBool(const std::string& id) const {
        return getParam(id) > 0.5f;
    }
    
    void setParam(const std::string& id, float value) {
        auto it = paramDefs_.find(id);
        if (it != paramDefs_.end()) {
            if (value < it->second.minVal) value = it->second.minVal;
            if (value > it->second.maxVal) value = it->second.maxVal;
            paramValues_[id] = value;
        }
    }
    
    const std::map<std::string, TransitionParamDef>& getParamDefs() const {
        return paramDefs_;
    }
    
    // ========================================================
    // Pixel Access (set by pipeline)
    // ========================================================
    
    using PixelCallback = std::function<void(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b)>;
    using DrawCallback = std::function<void(int x, int y, uint8_t r, uint8_t g, uint8_t b)>;
    
    PixelCallback getCapturedPixel = nullptr;
    DrawCallback drawPixel = nullptr;
    
    /**
     * @brief Reset the transition to initial state
     */
    virtual void reset() {
        active_ = false;
        complete_ = false;
        elapsedMs_ = 0;
    }
    
    /**
     * @brief Get elapsed time in ms
     */
    uint32_t getElapsedMs() const { return elapsedMs_; }
    
protected:
    // Elapsed time accessible to subclasses
    uint32_t elapsed_ = 0;  // Alias for backward compat
    
    void updateProgress(uint32_t deltaMs) {
        elapsedMs_ += deltaMs;
        elapsed_ = elapsedMs_;  // Keep alias in sync
        if (elapsedMs_ >= durationMs_) {
            elapsedMs_ = durationMs_;
            elapsed_ = elapsedMs_;
            complete_ = true;
            active_ = false;
        }
    }
    
    // Easing functions
    static float easeIn(float t) { return t * t; }
    static float easeOut(float t) { return t * (2.0f - t); }
    static float easeInOut(float t) {
        return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    }
    
    // Frame capture buffer (optional, can be set by pipeline)
    uint8_t* capturedFrame_ = nullptr;
    int capturedWidth_ = 0;
    int capturedHeight_ = 0;
    
private:
    bool active_ = false;
    bool complete_ = false;
    uint32_t durationMs_ = 500;
    uint32_t elapsedMs_ = 0;
    
    std::map<std::string, TransitionParamDef> paramDefs_;
    std::map<std::string, float> paramValues_;
};

} // namespace AnimationSystem
