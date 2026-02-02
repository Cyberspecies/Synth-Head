/*****************************************************************
 * @file AnimationSystem.hpp
 * @brief Master Header for Animation System
 * 
 * This is the main entry point for the animation system.
 * Include this file to get access to all animation functionality.
 * 
 * Architecture:
 * - AnimationContext: Unified access to inputs, outputs, sprites
 * - AnimationMode: Lifecycle management for animations
 * - ParameterRegistry: Auto-generation of settings UI
 * - AnimationSet: Collection of animations with parameters
 * 
 * Usage:
 *    #include "AnimationSystem/AnimationSystem.hpp"
 *    
 *    // Get context for inputs/outputs
 *    auto& ctx = AnimationSystem::getContext();
 *    float pitch = ctx.getInput("imu.pitch");
 *    
 *    // Get parameter registry for settings generation
 *    auto& params = AnimationSystem::getParameterRegistry();
 *    auto defs = params.getParameterDefinitions("gyro_eye");
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

// Core components
#include "AnimationContext.hpp"
#include "ParameterDef.hpp"
#include "ParameterRegistry.hpp"
#include "AnimationSet.hpp"
#include "AnimationMode.hpp"

namespace AnimationSystem {

// ============================================================
// Global Accessors
// ============================================================

/**
 * @brief Get the global animation context
 * Contains all inputs, outputs, sprites, and equations
 */
inline AnimationContext& getContext() {
    static AnimationContext instance;
    return instance;
}

/**
 * @brief Get the global parameter registry
 * Used for auto-generating settings UI
 * Auto-initializes on first access (lazy initialization)
 */
inline ParameterRegistry& getParameterRegistry() {
    static ParameterRegistry instance;
    if (!instance.isInitialized()) {
        instance.init();
    }
    return instance;
}

/**
 * @brief Get the animation mode handler
 */
inline AnimationMode& getAnimationMode() {
    static AnimationMode instance;
    return instance;
}

// ============================================================
// System Initialization
// ============================================================

/**
 * @brief Initialize the animation system
 * Call this once during boot
 * @return true if successful
 */
inline bool init() {
    // Initialize context with default values
    auto& ctx = getContext();
    ctx.init();
    
    // Initialize parameter registry with built-in animation sets
    auto& params = getParameterRegistry();
    params.init();
    
    // Initialize animation mode
    auto& mode = getAnimationMode();
    mode.init();
    
    return true;
}

/**
 * @brief Update the animation system
 * Call this every frame from CurrentMode
 * @param deltaTimeMs Time since last update in milliseconds
 */
inline void update(uint32_t deltaTimeMs) {
    auto& ctx = getContext();
    auto& mode = getAnimationMode();
    
    // Update context (sensor polling, equation evaluation)
    ctx.update(deltaTimeMs);
    
    // Update active animation mode
    mode.update(deltaTimeMs);
}

} // namespace AnimationSystem
