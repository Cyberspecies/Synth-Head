/*****************************************************************
 * @file TransitionRegistry.hpp
 * @brief Registry for transition types - self-registration pattern
 * 
 * Transitions can register themselves using the REGISTER_TRANSITION macro.
 * The registry provides:
 * - List of available transition types for UI
 * - Factory creation of transition instances
 * - JSON export for web API
 * 
 * Usage:
 *   // At end of transition file:
 *   REGISTER_TRANSITION(FadeTransition);
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "TransitionBase.hpp"
#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace AnimationSystem {

/**
 * @brief Info about a registered transition type
 */
struct TransitionTypeInfo {
    std::string typeId;
    std::string displayName;
    std::string description;
    std::string icon;  // HTML entity or emoji for UI
    std::function<std::unique_ptr<TransitionBase>()> factory;
    std::vector<TransitionParamDef> params;
};

/**
 * @brief Registry for all transition types
 */
class TransitionRegistry {
public:
    static TransitionRegistry& instance() {
        static TransitionRegistry reg;
        return reg;
    }
    
    /**
     * @brief Register a transition type with icon
     */
    template<typename T>
    void registerTransition(const std::string& icon = "&#x2192;") {
        T temp;
        TransitionTypeInfo info;
        info.typeId = temp.getTypeId();
        info.displayName = temp.getDisplayName();
        info.description = temp.getDescription();
        info.icon = icon;
        info.factory = []() { return std::make_unique<T>(); };
        
        for (const auto& pair : temp.getParamDefs()) {
            info.params.push_back(pair.second);
        }
        
        transitions_[info.typeId] = info;
    }
    
    /**
     * @brief Create a transition instance by type ID
     */
    std::unique_ptr<TransitionBase> create(const std::string& typeId) {
        auto it = transitions_.find(typeId);
        if (it != transitions_.end()) {
            return it->second.factory();
        }
        return nullptr;
    }
    
    /**
     * @brief Get list of all registered type IDs
     */
    std::vector<std::string> getTypeIds() const {
        std::vector<std::string> ids;
        for (const auto& pair : transitions_) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
    /**
     * @brief Get info about all registered transitions
     */
    std::vector<TransitionTypeInfo> getAllTypeInfos() const {
        std::vector<TransitionTypeInfo> infos;
        for (const auto& pair : transitions_) {
            infos.push_back(pair.second);
        }
        return infos;
    }
    
    /**
     * @brief Get info for a specific transition type
     */
    const TransitionTypeInfo* getTypeInfo(const std::string& typeId) const {
        auto it = transitions_.find(typeId);
        if (it != transitions_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /**
     * @brief Export transition types as JSON for web API
     */
    std::string exportJson() const {
        std::string json = "{\"transitions\":[";
        bool first = true;
        
        for (const auto& pair : transitions_) {
            if (!first) json += ",";
            first = false;
            
            const auto& info = pair.second;
            json += "{";
            json += "\"id\":\"" + info.typeId + "\",";
            json += "\"name\":\"" + info.displayName + "\",";
            json += "\"icon\":\"" + info.icon + "\",";
            json += "\"description\":\"" + info.description + "\",";
            json += "\"params\":[";
            
            bool firstParam = true;
            for (const auto& p : info.params) {
                if (!firstParam) json += ",";
                firstParam = false;
                
                json += "{";
                json += "\"id\":\"" + p.id + "\",";
                json += "\"name\":\"" + p.name + "\",";
                json += "\"type\":\"" + paramTypeStr(p.type) + "\",";
                json += "\"min\":" + std::to_string(p.minVal) + ",";
                json += "\"max\":" + std::to_string(p.maxVal) + ",";
                json += "\"default\":" + std::to_string(p.defaultVal) + ",";
                json += "\"hint\":\"" + p.description + "\"";
                json += "}";
            }
            json += "]}";
        }
        json += "]}";
        return json;
    }
    
private:
    TransitionRegistry() {
        // Add "none" option
        TransitionTypeInfo none;
        none.typeId = "none";
        none.displayName = "None";
        none.description = "Instant switch, no transition";
        none.icon = "&#x2192;";
        none.factory = []() { return nullptr; };
        transitions_["none"] = none;
    }
    
    static std::string paramTypeStr(ParamType t) {
        switch (t) {
            case ParamType::FLOAT: return "range";
            case ParamType::INT: return "int";
            case ParamType::BOOL: return "bool";
            case ParamType::STRING: return "string";
            default: return "range";
        }
    }
    
    std::map<std::string, TransitionTypeInfo> transitions_;
};

/**
 * @brief Helper for auto-registration
 */
template<typename T>
struct TransitionRegistrar {
    TransitionRegistrar(const std::string& icon = "&#x2192;") {
        TransitionRegistry::instance().registerTransition<T>(icon);
    }
};

/**
 * @brief Macro for auto-registering a transition type with icon
 * Place at end of transition header file
 */
#define REGISTER_TRANSITION_WITH_ICON(TransClass, Icon) \
    namespace { \
        static AnimationSystem::TransitionRegistrar<TransClass> _trans_reg_##TransClass(Icon); \
    }

/**
 * @brief Macro for auto-registering a transition type (default icon)
 */
#define REGISTER_TRANSITION(TransClass) \
    REGISTER_TRANSITION_WITH_ICON(TransClass, "&#x2192;")

} // namespace AnimationSystem
