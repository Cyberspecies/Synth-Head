/*****************************************************************
 * @file ShaderBase.hpp
 * @brief Base class for post-processing shaders
 * 
 * Shaders are effects applied after animation rendering.
 * They process the frame buffer and output modified pixels.
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "../AnimationTypes.hpp"
#include <string>
#include <map>
#include <functional>
#include <cstdint>

namespace AnimationSystem {

// Define guard for enum
#ifndef ANIMSYS_PARAMTYPE_DEFINED
enum class ParamType {
    FLOAT,
    INT,
    BOOL,
    STRING
};
#define ANIMSYS_PARAMTYPE_DEFINED
#endif

/**
 * @brief Shader parameter definition
 */
struct ShaderParamDef {
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
 * @brief Base class for all post-processing shaders
 */
class ShaderBase {
public:
    ShaderBase() = default;
    virtual ~ShaderBase() = default;
    
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
    virtual void update(uint32_t deltaMs) {}
    virtual void apply() = 0;
    virtual void reset() { time_ = 0; }
    
    // ========================================================
    // Enable/Disable
    // ========================================================
    
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    float getIntensity() const { return intensity_; }
    void setIntensity(float intensity) { intensity_ = intensity; }
    
    // ========================================================
    // Parameters
    // ========================================================
    
    void defineParam(const std::string& id, const std::string& name,
                     const std::string& desc, ParamType type,
                     float defaultVal, float minVal = 0, float maxVal = 1,
                     const std::string& category = "General") {
        ShaderParamDef def;
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
            // Clamp to range
            if (value < it->second.minVal) value = it->second.minVal;
            if (value > it->second.maxVal) value = it->second.maxVal;
            paramValues_[id] = value;
        }
    }
    
    const std::map<std::string, ShaderParamDef>& getParamDefs() const {
        return paramDefs_;
    }
    
    // ========================================================
    // Pixel Access (set by pipeline)
    // ========================================================
    
    using PixelCallback = std::function<void(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b)>;
    using DrawCallback = std::function<void(int x, int y, uint8_t r, uint8_t g, uint8_t b)>;
    
    PixelCallback getSourcePixel = nullptr;
    DrawCallback drawPixel = nullptr;
    
protected:
    // ========================================================
    // Color Conversion Utilities
    // ========================================================
    
    static void rgbToHsl(uint8_t r, uint8_t g, uint8_t b, float& h, float& s, float& l) {
        float rf = r / 255.0f;
        float gf = g / 255.0f;
        float bf = b / 255.0f;
        
        float maxC = (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
        float minC = (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
        float delta = maxC - minC;
        
        l = (maxC + minC) / 2.0f;
        
        if (delta < 0.001f) {
            h = 0;
            s = 0;
        } else {
            s = (l > 0.5f) ? delta / (2.0f - maxC - minC) : delta / (maxC + minC);
            
            if (maxC == rf) {
                h = (gf - bf) / delta + (gf < bf ? 6.0f : 0.0f);
            } else if (maxC == gf) {
                h = (bf - rf) / delta + 2.0f;
            } else {
                h = (rf - gf) / delta + 4.0f;
            }
            h *= 60.0f;
        }
    }
    
    static void hslToRgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b) {
        float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
        float hp = h / 60.0f;
        float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
        float m = l - c / 2.0f;
        
        float rf, gf, bf;
        if (hp < 1) { rf = c; gf = x; bf = 0; }
        else if (hp < 2) { rf = x; gf = c; bf = 0; }
        else if (hp < 3) { rf = 0; gf = c; bf = x; }
        else if (hp < 4) { rf = 0; gf = x; bf = c; }
        else if (hp < 5) { rf = x; gf = 0; bf = c; }
        else { rf = c; gf = 0; bf = x; }
        
        r = static_cast<uint8_t>((rf + m) * 255.0f);
        g = static_cast<uint8_t>((gf + m) * 255.0f);
        b = static_cast<uint8_t>((bf + m) * 255.0f);
    }
    
protected:
    uint32_t time_ = 0;  // Time in ms, updated by derived classes
    
private:
    bool enabled_ = true;
    float intensity_ = 1.0f;
    std::map<std::string, ShaderParamDef> paramDefs_;
    std::map<std::string, float> paramValues_;
};

} // namespace AnimationSystem
