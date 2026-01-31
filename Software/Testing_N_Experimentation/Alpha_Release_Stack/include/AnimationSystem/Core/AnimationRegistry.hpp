/*****************************************************************
 * @file AnimationRegistry.hpp
 * @brief Animation Registry with auto-discovery
 * 
 * Singleton registry that:
 * - Auto-discovers registered animations at startup
 * - Provides list of available animation types
 * - Creates animation instances by type ID
 * - Exposes parameter metadata for UI/binding
 * 
 * REGISTRATION:
 * Use REGISTER_ANIMATION macro in each animation file:
 *   REGISTER_ANIMATION(StaticAnim)
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "AnimationBase.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace AnimationSystem {

/**
 * @brief Animation type info for UI/discovery
 */
struct AnimationTypeInfo {
    std::string typeId;             // Unique identifier
    std::string displayName;        // Human-readable name
    std::string description;        // Description
    AnimationFactoryFunc factory;   // Factory function
    std::vector<ParamDef> params;   // Parameter definitions
};

/**
 * @brief Singleton animation registry
 */
class AnimationRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static AnimationRegistry& instance() {
        static AnimationRegistry registry;
        return registry;
    }
    
    /**
     * @brief Register an animation type
     * @param typeId Unique type identifier
     * @param factory Factory function to create instances
     */
    void registerAnimation(const std::string& typeId, AnimationFactoryFunc factory) {
        // Create a temporary instance to get metadata
        auto temp = factory();
        if (!temp) return;
        
        AnimationTypeInfo info;
        info.typeId = typeId;
        info.displayName = temp->getDisplayName();
        info.description = temp->getDescription();
        info.factory = factory;
        info.params = temp->getParamDefsOrdered();
        
        animations_[typeId] = info;
        typeIds_.push_back(typeId);
    }
    
    /**
     * @brief Get all registered animation type IDs
     */
    const std::vector<std::string>& getTypeIds() const {
        return typeIds_;
    }
    
    /**
     * @brief Get animation type info
     */
    const AnimationTypeInfo* getTypeInfo(const std::string& typeId) const {
        auto it = animations_.find(typeId);
        if (it != animations_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /**
     * @brief Get all animation type infos
     */
    std::vector<const AnimationTypeInfo*> getAllTypeInfos() const {
        std::vector<const AnimationTypeInfo*> result;
        for (const auto& typeId : typeIds_) {
            auto it = animations_.find(typeId);
            if (it != animations_.end()) {
                result.push_back(&it->second);
            }
        }
        return result;
    }
    
    /**
     * @brief Create animation instance by type ID
     */
    std::unique_ptr<AnimationBase> create(const std::string& typeId) const {
        auto it = animations_.find(typeId);
        if (it != animations_.end()) {
            return it->second.factory();
        }
        return nullptr;
    }
    
    /**
     * @brief Check if animation type exists
     */
    bool hasType(const std::string& typeId) const {
        return animations_.find(typeId) != animations_.end();
    }
    
    /**
     * @brief Get number of registered animations
     */
    size_t count() const {
        return animations_.size();
    }
    
private:
    AnimationRegistry() = default;
    AnimationRegistry(const AnimationRegistry&) = delete;
    AnimationRegistry& operator=(const AnimationRegistry&) = delete;
    
    std::unordered_map<std::string, AnimationTypeInfo> animations_;
    std::vector<std::string> typeIds_;  // Maintains registration order
};

/**
 * @brief Helper class for static registration
 */
template<typename T>
class AnimationRegistrar {
public:
    explicit AnimationRegistrar(const char* typeId) {
        AnimationRegistry::instance().registerAnimation(
            typeId,
            []() -> std::unique_ptr<AnimationBase> {
                return std::make_unique<T>();
            }
        );
    }
};

/**
 * @brief Macro to register an animation type
 * 
 * Usage: REGISTER_ANIMATION(MyAnimation, "my_animation")
 * 
 * Place at file scope (outside any function) in the animation's .hpp file
 */
#define REGISTER_ANIMATION(ClassName, TypeId) \
    static ::AnimationSystem::AnimationRegistrar<ClassName> \
        _registrar_##ClassName(TypeId)

} // namespace AnimationSystem
