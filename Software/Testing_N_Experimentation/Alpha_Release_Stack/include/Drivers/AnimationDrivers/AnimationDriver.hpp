/*****************************************************************
 * @file AnimationDriver.hpp
 * @brief Base interface for modular animation drivers
 * 
 * Animation drivers handle different display modes:
 * - static: Display same image on both eyes
 * - static_mirrored: Display mirrored image on each eye
 * - gyro_eyes: Interactive IMU-based eye tracking
 * - etc.
 * 
 * Each driver is responsible for:
 * 1. Processing scene YAML configuration
 * 2. Generating GPU commands for the display
 * 3. Managing sprite/vector rendering with AA
 *****************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <map>

namespace Drivers {
namespace Animation {

/**
 * @brief Sprite/Vector render mode with antialiasing
 */
enum class RenderMode {
    SPRITE_AA,      // Sprite with antialiasing
    VECTOR_AA,      // Vector with antialiasing  
    SPRITE_RAW,     // Sprite without AA (legacy)
    VECTOR_RAW      // Vector without AA (legacy)
};

/**
 * @brief Display target for animation
 */
enum class DisplayTarget {
    BOTH,           // Both left and right displays
    LEFT_ONLY,      // Left display only
    RIGHT_ONLY      // Right display only
};

/**
 * @brief Animation frame data to send to GPU
 */
struct AnimationFrame {
    // Sprite/Vector ID (0 = default)
    int spriteId = 0;
    
    // Position for left eye
    float leftX = 0.0f;
    float leftY = 0.0f;
    float leftScale = 1.0f;
    float leftRotation = 0.0f;
    bool leftMirror = false;
    
    // Position for right eye  
    float rightX = 0.0f;
    float rightY = 0.0f;
    float rightScale = 1.0f;
    float rightRotation = 0.0f;
    bool rightMirror = false;
    
    // Render settings
    RenderMode renderMode = RenderMode::VECTOR_AA;
    bool enabled = true;
    
    // Background color
    uint8_t bgR = 0;
    uint8_t bgG = 0;
    uint8_t bgB = 0;
};

/**
 * @brief Scene parameters from YAML
 */
struct SceneParams {
    std::string animationType;
    int spriteId = 0;
    bool useDefaultSprite = true;
    bool antialiasing = true;
    bool mirror = false;
    float posX = 0.5f;  // Normalized 0-1
    float posY = 0.5f;
    float scale = 1.0f;
    float rotation = 0.0f;
    uint8_t bgR = 0;
    uint8_t bgG = 0;
    uint8_t bgB = 0;
    
    // Additional params map for driver-specific settings
    std::map<std::string, float> params;
};

/**
 * @brief Base class for all animation drivers
 */
class AnimationDriver {
public:
    virtual ~AnimationDriver() = default;
    
    /**
     * @brief Get the animation type name this driver handles
     */
    virtual const char* getTypeName() const = 0;
    
    /**
     * @brief Initialize the driver with scene parameters
     */
    virtual bool init(const SceneParams& params) = 0;
    
    /**
     * @brief Update the animation state (called each frame)
     * @param deltaTime Time since last update in seconds
     */
    virtual void update(float deltaTime) = 0;
    
    /**
     * @brief Get the current frame data to render
     */
    virtual AnimationFrame getFrame() const = 0;
    
    /**
     * @brief Check if driver is active
     */
    virtual bool isActive() const { return active_; }
    
    /**
     * @brief Set driver active state
     */
    virtual void setActive(bool active) { active_ = active; }

protected:
    bool active_ = false;
    SceneParams params_;
};

} // namespace Animation
} // namespace Drivers
