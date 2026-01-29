/*****************************************************************
 * @file StaticMirroredDriver.hpp
 * @brief Static Mirrored animation driver - mirrors image on each eye
 * 
 * Animation type: "static_mirrored"
 * 
 * Displays the sprite/vector on both displays, but the right eye
 * is horizontally mirrored. This creates a symmetrical look ideal
 * for eyes and face displays.
 * Uses GPU antialiasing for smooth rendering.
 *****************************************************************/

#pragma once

#include "AnimationDriver.hpp"

namespace Drivers {
namespace Animation {

class StaticMirroredDriver : public AnimationDriver {
public:
    StaticMirroredDriver() = default;
    ~StaticMirroredDriver() override = default;
    
    const char* getTypeName() const override {
        return "static_mirrored";
    }
    
    bool init(const SceneParams& params) override {
        params_ = params;
        active_ = true;
        
        // Initialize frame with static positions
        frame_.spriteId = params.spriteId;
        frame_.renderMode = params.antialiasing ? RenderMode::VECTOR_AA : RenderMode::VECTOR_RAW;
        
        // Left eye - normal orientation
        frame_.leftX = params.posX;
        frame_.leftY = params.posY;
        frame_.leftScale = params.scale;
        frame_.leftRotation = params.rotation;
        frame_.leftMirror = false;
        
        // Right eye - mirrored horizontally
        frame_.rightX = params.posX;
        frame_.rightY = params.posY;
        frame_.rightScale = params.scale;
        frame_.rightRotation = params.rotation;
        frame_.rightMirror = true;  // Mirror the right eye
        
        // Background
        frame_.bgR = params.bgR;
        frame_.bgG = params.bgG;
        frame_.bgB = params.bgB;
        
        frame_.enabled = true;
        
        return true;
    }
    
    void update(float deltaTime) override {
        // Static mirrored driver doesn't animate, nothing to update
        (void)deltaTime;
    }
    
    AnimationFrame getFrame() const override {
        return frame_;
    }

private:
    AnimationFrame frame_;
};

} // namespace Animation
} // namespace Drivers
