/*****************************************************************
 * @file ParameterRegistry.hpp
 * @brief Registry for Animation Sets and Parameters
 * 
 * Maintains a list of all available animation sets and provides
 * methods to query their parameters for auto-generating UI.
 * 
 * The web server queries this registry to:
 * - Get list of available animation sets
 * - Get parameter definitions for each set
 * - Update parameter values
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "AnimationSet.hpp"
#include <vector>
#include <memory>
#include <string>
#include <map>

namespace AnimationSystem {

// ============================================================
// Animation Set Info (for web API)
// ============================================================

struct AnimationSetInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
};

// ============================================================
// Parameter Registry
// ============================================================

class ParameterRegistry {
public:
    static constexpr int MAX_ANIMATION_SETS = 32;
    
    ParameterRegistry() = default;
    
    /**
     * @brief Initialize with built-in animation sets
     */
    void init() {
        // Register built-in animation sets
        registerBuiltIn<StaticSpriteAnimationSet>();
        registerBuiltIn<StaticMirroredAnimationSet>();
        
        initialized_ = true;
    }
    
    // ========================================================
    // Animation Set Registration
    // ========================================================
    
    /**
     * @brief Register a built-in animation set type
     */
    template<typename T>
    void registerBuiltIn() {
        auto set = std::make_unique<T>();
        const char* id = set->getId();
        animationSets_[id] = std::move(set);
    }
    
    /**
     * @brief Register a custom animation set
     */
    void registerSet(std::unique_ptr<AnimationSet> set) {
        if (set) {
            const char* id = set->getId();
            animationSets_[id] = std::move(set);
        }
    }
    
    /**
     * @brief Get animation set by ID
     */
    AnimationSet* getAnimationSet(const std::string& id) {
        auto it = animationSets_.find(id);
        if (it != animationSets_.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    
    /**
     * @brief Get list of all animation set IDs
     */
    std::vector<std::string> getAnimationSetIds() const {
        std::vector<std::string> ids;
        for (const auto& pair : animationSets_) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
    /**
     * @brief Get info about all animation sets
     */
    std::vector<AnimationSetInfo> getAnimationSetInfos() const {
        std::vector<AnimationSetInfo> infos;
        for (const auto& pair : animationSets_) {
            AnimationSetInfo info;
            info.id = pair.second->getId();
            info.name = pair.second->getName();
            info.description = pair.second->getDescription();
            info.category = pair.second->getCategory();
            infos.push_back(info);
        }
        return infos;
    }
    
    // ========================================================
    // Parameter Access (for web API)
    // ========================================================
    
    /**
     * @brief Get parameter definitions for an animation set
     * @param setId Animation set ID
     * @return Vector of parameter definitions
     */
    std::vector<ParameterDef>* getParameterDefinitions(const std::string& setId) {
        auto* set = getAnimationSet(setId);
        if (set) {
            return &set->getParameters();
        }
        return nullptr;
    }
    
    /**
     * @brief Set parameter value
     * @return true if successful
     */
    bool setParameterValue(const std::string& setId, const std::string& paramId, float value) {
        auto* set = getAnimationSet(setId);
        if (set) {
            set->setParameterValue(paramId.c_str(), value);
            return true;
        }
        return false;
    }
    
    bool setParameterValue(const std::string& setId, const std::string& paramId, int value) {
        auto* set = getAnimationSet(setId);
        if (set) {
            set->setParameterValue(paramId.c_str(), value);
            return true;
        }
        return false;
    }
    
    bool setParameterValue(const std::string& setId, const std::string& paramId, bool value) {
        auto* set = getAnimationSet(setId);
        if (set) {
            set->setParameterValue(paramId.c_str(), value);
            return true;
        }
        return false;
    }
    
    bool setParameterValue(const std::string& setId, const std::string& paramId, const std::string& value) {
        auto* set = getAnimationSet(setId);
        if (set) {
            set->setParameterValue(paramId.c_str(), value);
            return true;
        }
        return false;
    }
    
    // ========================================================
    // JSON Export (for web API)
    // ========================================================
    
    /**
     * @brief Export animation sets list as JSON
     * Format: {"sets": [{ "id": "...", "name": "...", "description": "...", "category": "..." }, ...]}
     */
    std::string exportAnimationSetsJson() const {
        std::string json = "{\"sets\":[";
        bool first = true;
        for (const auto& pair : animationSets_) {
            if (!first) json += ",";
            first = false;
            
            json += "{";
            json += "\"id\":\"" + std::string(pair.second->getId()) + "\",";
            json += "\"name\":\"" + std::string(pair.second->getName()) + "\",";
            json += "\"description\":\"" + std::string(pair.second->getDescription()) + "\",";
            json += "\"category\":\"" + std::string(pair.second->getCategory()) + "\"";
            json += "}";
        }
        json += "]}";
        return json;
    }
    
    /**
     * @brief Export parameters for an animation set as JSON
     * Format: {"params": [{ "id": "...", "label": "...", "type": "...", ... }, ...]}
     */
    std::string exportParametersJson(const std::string& setId) const {
        auto it = animationSets_.find(setId);
        if (it == animationSets_.end()) return "{\"params\":[]}";
        
        const auto& params = it->second->getParameters();
        std::string json = "{\"params\":";
        json += exportParameterListJson(params);
        json += "}";
        return json;
    }
    
    /**
     * @brief Reset all animation sets to default values
     */
    void resetAllToDefaults() {
        for (auto& pair : animationSets_) {
            pair.second->resetToDefaults();
        }
    }
    
    /**
     * @brief Export parameter list as JSON
     */
    static std::string exportParameterListJson(const std::vector<ParameterDef>& params) {
        std::string json = "[";
        bool first = true;
        
        for (const auto& p : params) {
            if (!first) json += ",";
            first = false;
            
            json += "{";
            json += "\"id\":\"" + p.id + "\",";
            json += "\"name\":\"" + p.name + "\",";
            json += "\"type\":" + std::to_string(static_cast<int>(p.type)) + ",";
            json += "\"category\":" + std::to_string(static_cast<int>(p.category)) + ",";
            
            // Type-specific values
            switch (p.type) {
                case ParameterType::SLIDER:
                    json += "\"min\":" + floatToString(p.minValue) + ",";
                    json += "\"max\":" + floatToString(p.maxValue) + ",";
                    json += "\"step\":" + floatToString(p.step) + ",";
                    json += "\"value\":" + floatToString(p.floatValue) + ",";
                    json += "\"default\":" + floatToString(p.defaultValue);
                    break;
                    
                case ParameterType::SLIDER_INT:
                    json += "\"min\":" + std::to_string(static_cast<int>(p.minValue)) + ",";
                    json += "\"max\":" + std::to_string(static_cast<int>(p.maxValue)) + ",";
                    json += "\"step\":1,";
                    json += "\"value\":" + std::to_string(p.intValue) + ",";
                    json += "\"default\":" + std::to_string(static_cast<int>(p.defaultValue));
                    break;
                    
                case ParameterType::TOGGLE:
                    json += "\"value\":" + std::string(p.boolValue ? "true" : "false");
                    break;
                    
                case ParameterType::COLOR:
                    json += "\"r\":" + std::to_string(p.colorR) + ",";
                    json += "\"g\":" + std::to_string(p.colorG) + ",";
                    json += "\"b\":" + std::to_string(p.colorB);
                    break;
                    
                case ParameterType::DROPDOWN:
                    json += "\"value\":" + std::to_string(p.intValue) + ",";
                    json += "\"options\":[";
                    for (size_t i = 0; i < p.options.size(); i++) {
                        if (i > 0) json += ",";
                        json += "{\"label\":\"" + p.options[i].label + "\",";
                        json += "\"value\":" + std::to_string(p.options[i].value) + "}";
                    }
                    json += "]";
                    break;
                    
                case ParameterType::INPUT_SELECT:
                case ParameterType::TEXT:
                    json += "\"value\":\"" + escapeJson(p.stringValue) + "\"";
                    break;
                    
                case ParameterType::SPRITE_SELECT:
                case ParameterType::EQUATION_SELECT:
                    json += "\"value\":" + std::to_string(p.intValue);
                    break;
                    
                default:
                    json += "\"value\":null";
                    break;
            }
            
            // Add unit if present
            if (!p.unit.empty()) {
                json += ",\"unit\":\"" + p.unit + "\"";
            }
            
            // Add description if present
            if (!p.description.empty()) {
                json += ",\"description\":\"" + escapeJson(p.description) + "\"";
            }
            
            // Add visibility/enabled state
            if (!p.visible) json += ",\"visible\":false";
            if (!p.enabled) json += ",\"enabled\":false";
            
            json += "}";
        }
        
        json += "]";
        return json;
    }
    
    bool isInitialized() const { return initialized_; }
    
private:
    std::map<std::string, std::unique_ptr<AnimationSet>> animationSets_;
    bool initialized_ = false;
    
    static std::string floatToString(float value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4f", value);
        return buf;
    }
    
    static std::string escapeJson(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

} // namespace AnimationSystem
