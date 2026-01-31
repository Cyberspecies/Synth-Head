/*****************************************************************
 * @file ShaderRegistry.hpp
 * @brief Registry for shader types - self-registration pattern
 * 
 * Shaders can register themselves using the REGISTER_SHADER macro.
 * The registry provides:
 * - List of available shader types for UI
 * - Factory creation of shader instances
 * - JSON export for web API
 * 
 * Usage:
 *   // At end of shader file:
 *   REGISTER_SHADER(HueCycleShader);
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "ShaderBase.hpp"
#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace AnimationSystem {

/**
 * @brief Info about a registered shader type
 */
struct ShaderTypeInfo {
    std::string typeId;
    std::string displayName;
    std::string description;
    std::function<std::unique_ptr<ShaderBase>()> factory;
    std::vector<ShaderParamDef> params;
};

/**
 * @brief Registry for all shader types
 */
class ShaderRegistry {
public:
    static ShaderRegistry& instance() {
        static ShaderRegistry reg;
        return reg;
    }
    
    /**
     * @brief Register a shader type
     */
    template<typename T>
    void registerShader() {
        T temp;  // Create temp instance to get info
        ShaderTypeInfo info;
        info.typeId = temp.getTypeId();
        info.displayName = temp.getDisplayName();
        info.description = temp.getDescription();
        info.factory = []() { return std::make_unique<T>(); };
        
        // Copy parameter definitions
        for (const auto& pair : temp.getParamDefs()) {
            info.params.push_back(pair.second);
        }
        
        shaders_[info.typeId] = info;
    }
    
    /**
     * @brief Create a shader instance by type ID
     */
    std::unique_ptr<ShaderBase> create(const std::string& typeId) {
        auto it = shaders_.find(typeId);
        if (it != shaders_.end()) {
            return it->second.factory();
        }
        return nullptr;
    }
    
    /**
     * @brief Get list of all registered type IDs
     */
    std::vector<std::string> getTypeIds() const {
        std::vector<std::string> ids;
        for (const auto& pair : shaders_) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
    /**
     * @brief Get info about all registered shaders
     */
    std::vector<ShaderTypeInfo> getAllTypeInfos() const {
        std::vector<ShaderTypeInfo> infos;
        for (const auto& pair : shaders_) {
            infos.push_back(pair.second);
        }
        return infos;
    }
    
    /**
     * @brief Get info for a specific shader type
     */
    const ShaderTypeInfo* getTypeInfo(const std::string& typeId) const {
        auto it = shaders_.find(typeId);
        if (it != shaders_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /**
     * @brief Export shader types as JSON for web API
     */
    std::string exportJson() const {
        std::string json = "{\"shaders\":[";
        bool first = true;
        
        for (const auto& pair : shaders_) {
            if (!first) json += ",";
            first = false;
            
            const auto& info = pair.second;
            json += "{";
            json += "\"id\":\"" + info.typeId + "\",";
            json += "\"name\":\"" + info.displayName + "\",";
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
    ShaderRegistry() {
        // Add "none" option
        ShaderTypeInfo none;
        none.typeId = "none";
        none.displayName = "None";
        none.description = "No shader effect";
        none.factory = []() { return nullptr; };
        shaders_["none"] = none;
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
    
    std::map<std::string, ShaderTypeInfo> shaders_;
};

/**
 * @brief Helper for auto-registration
 */
template<typename T>
struct ShaderRegistrar {
    ShaderRegistrar() {
        ShaderRegistry::instance().registerShader<T>();
    }
};

/**
 * @brief Macro for auto-registering a shader type
 * Place at end of shader header file
 */
#define REGISTER_SHADER(ShaderClass) \
    namespace { \
        static AnimationSystem::ShaderRegistrar<ShaderClass> _shader_reg_##ShaderClass; \
    }

} // namespace AnimationSystem
