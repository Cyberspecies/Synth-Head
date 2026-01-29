/*****************************************************************
 * @file AnimationDriverRegistry.hpp
 * @brief Registry for modular animation drivers
 * 
 * Manages all available animation drivers and provides factory
 * methods to create drivers by type name.
 *****************************************************************/

#pragma once

#include "AnimationDriver.hpp"
#include "StaticDriver.hpp"
#include "StaticMirroredDriver.hpp"

#include <memory>
#include <string>
#include <map>
#include <functional>

namespace Drivers {
namespace Animation {

/**
 * @brief Factory and registry for animation drivers
 */
class DriverRegistry {
public:
    using DriverFactory = std::function<std::unique_ptr<AnimationDriver>()>;
    
    /**
     * @brief Get singleton instance
     */
    static DriverRegistry& instance() {
        static DriverRegistry registry;
        return registry;
    }
    
    /**
     * @brief Register a driver factory
     */
    void registerDriver(const std::string& typeName, DriverFactory factory) {
        factories_[typeName] = factory;
    }
    
    /**
     * @brief Create a driver by type name
     */
    std::unique_ptr<AnimationDriver> createDriver(const std::string& typeName) {
        auto it = factories_.find(typeName);
        if (it != factories_.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    /**
     * @brief Check if a driver type is registered
     */
    bool hasDriver(const std::string& typeName) const {
        return factories_.find(typeName) != factories_.end();
    }
    
    /**
     * @brief Get list of all registered driver types
     */
    std::vector<std::string> getRegisteredTypes() const {
        std::vector<std::string> types;
        for (const auto& kv : factories_) {
            types.push_back(kv.first);
        }
        return types;
    }
    
    /**
     * @brief Initialize registry with built-in drivers
     */
    void initBuiltinDrivers() {
        // Static driver - same image on both displays
        registerDriver("static", []() {
            return std::make_unique<StaticDriver>();
        });
        
        // Static mirrored - mirrored image on right display
        registerDriver("static_mirrored", []() {
            return std::make_unique<StaticMirroredDriver>();
        });
        
        // TODO: Add more drivers as they are implemented
        // registerDriver("gyro_eyes", []() { return std::make_unique<GyroEyesDriver>(); });
        // registerDriver("blink", []() { return std::make_unique<BlinkDriver>(); });
    }

private:
    DriverRegistry() {
        initBuiltinDrivers();
    }
    
    std::map<std::string, DriverFactory> factories_;
};

/**
 * @brief Helper to create and initialize a driver from scene params
 */
inline std::unique_ptr<AnimationDriver> createAnimationDriver(const SceneParams& params) {
    auto driver = DriverRegistry::instance().createDriver(params.animationType);
    if (driver) {
        driver->init(params);
    }
    return driver;
}

} // namespace Animation
} // namespace Drivers
