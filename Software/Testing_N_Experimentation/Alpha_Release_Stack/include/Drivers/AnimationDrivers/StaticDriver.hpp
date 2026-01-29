/*****************************************************************
 * @file StaticDriver.hpp
 * @brief Static animation driver - displays same image on both eyes
 * 
 * Animation type: "static"
 * 
 * Displays the same sprite/vector on both left and right displays.
 * Uses GPU antialiasing for smooth rendering.
 *****************************************************************/

#pragma once

#include "AnimationDriver.hpp"

namespace Drivers {
namespace Animation {

class StaticDriver : public AnimationDriver {
public:
    StaticDriver() = default;
    ~StaticDriver() override = default;
    
    const char* getTypeName() const override {
        return "static";
    }
    
    bool init(const SceneParams& params) override {
        params_ = params;
        active_ = true;
        
        // Initialize frame with static positions
        frame_.spriteId = params.spriteId;
        frame_.renderMode = params.antialiasing ? RenderMode::VECTOR_AA : RenderMode::VECTOR_RAW;
        
        // Center positions (will be converted to pixel coords by renderer)
        frame_.leftX = params.posX;
        frame_.leftY = params.posY;
        frame_.leftScale = params.scale;
        frame_.leftRotation = params.rotation;
        frame_.leftMirror = false;
        
        // Same position for right eye (static = same on both)
        frame_.rightX = params.posX;
        frame_.rightY = params.posY;
        frame_.rightScale = params.scale;
        frame_.rightRotation = params.rotation;
        frame_.rightMirror = false;
        
        // Background
        frame_.bgR = params.bgR;
        frame_.bgG = params.bgG;
        frame_.bgB = params.bgB;
        
        frame_.enabled = true;
        
        return true;
    }
    
    void update(float deltaTime) override {
        // Static driver doesn't animate, nothing to update
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
