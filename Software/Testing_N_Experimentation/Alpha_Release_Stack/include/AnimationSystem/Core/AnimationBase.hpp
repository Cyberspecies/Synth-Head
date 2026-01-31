/*****************************************************************
 * @file AnimationBase.hpp
 * @brief Base class for all animations with parameter system
 * 
 * Provides:
 * - Parameter definition and metadata for equation binding
 * - Self-registration macro for auto-discovery
 * - Standard interface for update/render
 * 
 * CREATING A NEW ANIMATION:
 * 1. Create a new .hpp file in Animations/ folder
 * 2. Inherit from AnimationBase
 * 3. Define parameters in constructor using defineParam()
 * 4. Implement update() and render()
 * 5. Use REGISTER_ANIMATION macro at file scope
 * 6. Include in AllAnimations.hpp
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "../Common/AnimationCommon.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace AnimationSystem {

// ================================================================
// PARAMETER TYPES
// ================================================================

enum class ParamType {
    FLOAT,      // Floating point value
    INT,        // Integer value
    BOOL,       // Boolean toggle
    COLOR       // RGB color (3 floats: r, g, b normalized 0-1)
};

/**
 * @brief Parameter definition with metadata
 */
struct ParamDef {
    std::string name;           // Unique parameter name (e.g., "left.offset_x")
    std::string displayName;    // Human-readable name
    std::string description;    // Tooltip/help text
    ParamType type;             // Parameter type
    float minValue = 0.0f;      // Minimum value (for FLOAT/INT)
    float maxValue = 1.0f;      // Maximum value (for FLOAT/INT)
    float defaultValue = 0.0f;  // Default value
    std::string group;          // Parameter group (e.g., "left", "right", "background")
};

/**
 * @brief Runtime parameter value with equation binding
 */
struct ParamValue {
    float value = 0.0f;                         // Current value
    std::string equationBinding;                // Equation name to bind (empty = static)
    std::function<float()> equationGetter;      // Resolved equation callback
    
    // For color type (3 components)
    float r = 0.0f, g = 0.0f, b = 0.0f;
    
    // Get value (from equation if bound, else static)
    float get() const {
        if (equationGetter) return equationGetter();
        return value;
    }
    
    // Get color components
    void getColor(uint8_t& outR, uint8_t& outG, uint8_t& outB) const {
        outR = static_cast<uint8_t>(r * 255.0f);
        outG = static_cast<uint8_t>(g * 255.0f);
        outB = static_cast<uint8_t>(b * 255.0f);
    }
};

// ================================================================
// BASE ANIMATION CLASS
// ================================================================

/**
 * @brief Base class for all animations
 * 
 * Animations inherit from this and define their parameters.
 * The registry system auto-discovers registered animations.
 */
class AnimationBase {
public:
    virtual ~AnimationBase() = default;
    
    // ============================================================
    // REQUIRED OVERRIDES
    // ============================================================
    
    /**
     * @brief Get unique animation type ID
     * @return String identifier (e.g., "static", "sway", "gyro_eyes")
     */
    virtual const char* getTypeId() const = 0;
    
    /**
     * @brief Get display name for UI
     */
    virtual const char* getDisplayName() const = 0;
    
    /**
     * @brief Get animation description
     */
    virtual const char* getDescription() const = 0;
    
    /**
     * @brief Update animation state
     * @param deltaMs Milliseconds since last update
     */
    virtual void update(uint32_t deltaMs) = 0;
    
    /**
     * @brief Render animation frame
     */
    virtual void render() = 0;
    
    // ============================================================
    // OPTIONAL OVERRIDES
    // ============================================================
    
    /**
     * @brief Reset animation to initial state
     */
    virtual void reset() {
        // Reset all parameters to defaults
        for (auto& [name, value] : paramValues_) {
            auto it = paramDefs_.find(name);
            if (it != paramDefs_.end()) {
                value.value = it->second.defaultValue;
            }
        }
    }
    
    /**
     * @brief Called when animation is activated
     */
    virtual void onActivate() {}
    
    /**
     * @brief Called when animation is deactivated
     */
    virtual void onDeactivate() {}
    
    // ============================================================
    // PARAMETER SYSTEM
    // ============================================================
    
    /**
     * @brief Get all parameter definitions
     */
    const std::unordered_map<std::string, ParamDef>& getParamDefs() const {
        return paramDefs_;
    }
    
    /**
     * @brief Get parameter definitions as ordered vector (for UI)
     */
    std::vector<ParamDef> getParamDefsOrdered() const {
        std::vector<ParamDef> result;
        result.reserve(paramDefs_.size());
        for (const auto& [name, def] : paramDefs_) {
            result.push_back(def);
        }
        return result;
    }
    
    /**
     * @brief Set parameter value directly
     */
    void setParam(const std::string& name, float value) {
        auto it = paramValues_.find(name);
        if (it != paramValues_.end()) {
            it->second.value = value;
        }
    }
    
    /**
     * @brief Set parameter color value
     */
    void setParamColor(const std::string& name, float r, float g, float b) {
        auto it = paramValues_.find(name);
        if (it != paramValues_.end()) {
            it->second.r = r;
            it->second.g = g;
            it->second.b = b;
        }
    }
    
    /**
     * @brief Get parameter value
     */
    float getParam(const std::string& name) const {
        auto it = paramValues_.find(name);
        if (it != paramValues_.end()) {
            return it->second.get();
        }
        return 0.0f;
    }
    
    /**
     * @brief Get parameter as bool
     */
    bool getParamBool(const std::string& name) const {
        return getParam(name) > 0.5f;
    }
    
    /**
     * @brief Get parameter as int
     */
    int getParamInt(const std::string& name) const {
        return static_cast<int>(getParam(name));
    }
    
    /**
     * @brief Get parameter color
     */
    void getParamColor(const std::string& name, uint8_t& r, uint8_t& g, uint8_t& b) const {
        auto it = paramValues_.find(name);
        if (it != paramValues_.end()) {
            it->second.getColor(r, g, b);
        } else {
            r = g = b = 0;
        }
    }
    
    /**
     * @brief Bind parameter to equation
     * @param paramName Parameter to bind
     * @param equationName Equation name (from equation system)
     * @param getter Callback that returns equation value
     */
    void bindEquation(const std::string& paramName, 
                      const std::string& equationName,
                      std::function<float()> getter) {
        auto it = paramValues_.find(paramName);
        if (it != paramValues_.end()) {
            it->second.equationBinding = equationName;
            it->second.equationGetter = getter;
        }
    }
    
    /**
     * @brief Unbind equation from parameter
     */
    void unbindEquation(const std::string& paramName) {
        auto it = paramValues_.find(paramName);
        if (it != paramValues_.end()) {
            it->second.equationBinding.clear();
            it->second.equationGetter = nullptr;
        }
    }
    
    /**
     * @brief Get equation binding for parameter
     */
    std::string getEquationBinding(const std::string& paramName) const {
        auto it = paramValues_.find(paramName);
        if (it != paramValues_.end()) {
            return it->second.equationBinding;
        }
        return "";
    }
    
    // ============================================================
    // GPU CALLBACKS (set by system)
    // ============================================================
    
    ClearFunc clear = nullptr;
    FillRectFunc fillRect = nullptr;
    DrawPixelFunc drawPixel = nullptr;
    DrawLineFunc drawLine = nullptr;
    FillCircleFunc fillCircle = nullptr;
    BlitSpriteFunc blitSprite = nullptr;
    BlitSpriteRotatedFunc blitSpriteRotated = nullptr;
    PresentFunc present = nullptr;
    
    // Extended sprite blit with clipping
    using BlitSpriteClippedFunc = std::function<void(
        int id, float x, float y, float angle, bool mirrorX,
        int clipX, int clipY, int clipW, int clipH, bool applyClip
    )>;
    BlitSpriteClippedFunc blitSpriteClipped = nullptr;
    
protected:
    // ============================================================
    // PARAMETER DEFINITION (call in constructor)
    // ============================================================
    
    /**
     * @brief Define a parameter for this animation
     */
    void defineParam(const std::string& name, 
                     const std::string& displayName,
                     const std::string& description,
                     ParamType type,
                     float defaultValue,
                     float minValue = 0.0f,
                     float maxValue = 1.0f,
                     const std::string& group = "") {
        ParamDef def;
        def.name = name;
        def.displayName = displayName;
        def.description = description;
        def.type = type;
        def.defaultValue = defaultValue;
        def.minValue = minValue;
        def.maxValue = maxValue;
        def.group = group;
        
        paramDefs_[name] = def;
        
        ParamValue val;
        val.value = defaultValue;
        paramValues_[name] = val;
    }
    
    /**
     * @brief Define a color parameter
     */
    void defineColorParam(const std::string& name,
                          const std::string& displayName,
                          const std::string& description,
                          float defaultR, float defaultG, float defaultB,
                          const std::string& group = "") {
        ParamDef def;
        def.name = name;
        def.displayName = displayName;
        def.description = description;
        def.type = ParamType::COLOR;
        def.group = group;
        
        paramDefs_[name] = def;
        
        ParamValue val;
        val.r = defaultR;
        val.g = defaultG;
        val.b = defaultB;
        paramValues_[name] = val;
    }
    
private:
    std::unordered_map<std::string, ParamDef> paramDefs_;
    std::unordered_map<std::string, ParamValue> paramValues_;
};

// ================================================================
// ANIMATION FACTORY FUNCTION TYPE
// ================================================================

using AnimationFactoryFunc = std::function<std::unique_ptr<AnimationBase>()>;

} // namespace AnimationSystem
