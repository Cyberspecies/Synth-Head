/*****************************************************************
 * @file AnimationManager.hpp
 * @brief Central manager for the animation system
 * 
 * High-level interface for creating and managing animations.
 * Designed for ease of use from simple to complex scenarios.
 *****************************************************************/

#pragma once

#include "Core/Types.hpp"
#include "Core/Color.hpp"
#include "Shaders/Shaders.hpp"
#include "Animation/Animation.hpp"
#include "Binding/Bindings.hpp"
#include "Render/Render.hpp"
#include <functional>

namespace AnimationDriver {

// ============================================================
// Callback Types for GPU Commands
// ============================================================

using SendHUB75DataCallback = std::function<void(const uint8_t* data, size_t size)>;
using SendOLEDDataCallback = std::function<void(const uint8_t* data, size_t size)>;

// ============================================================
// Animation Manager Configuration
// ============================================================

struct AnimationManagerConfig {
    // Display settings
    int hub75Width = 128;
    int hub75Height = 32;
    int oledWidth = 128;
    int oledHeight = 128;
    
    // Update rate
    float targetFPS = 60.0f;
    
    // Auto-send to GPU
    bool autoSendHub75 = true;
    bool autoSendOled = false;  // OLED usually handled separately
};

// ============================================================
// Animation Manager
// ============================================================

class AnimationManager {
public:
    static constexpr int MAX_SCENES = 8;
    static constexpr int MAX_SHADERS = 16;
    
    AnimationManager() : config_(), activeScene_(nullptr), time_(0.0f), 
                         running_(false), frameCount_(0) {
        // Initialize default render targets
        hub75Target_ = new RenderTarget(DisplayTarget::HUB75);
        oledTarget_ = new RenderTarget(DisplayTarget::OLED);
    }
    
    ~AnimationManager() {
        delete hub75Target_;
        delete oledTarget_;
        
        // Clean up owned scenes
        for (int i = 0; i < sceneCount_; i++) {
            if (ownedScenes_[i]) {
                delete scenes_[i];
            }
        }
    }
    
    // ========================================================
    // Configuration
    // ========================================================
    
    AnimationManager& configure(const AnimationManagerConfig& config) {
        config_ = config;
        return *this;
    }
    
    AnimationManager& setTargetFPS(float fps) {
        config_.targetFPS = fps;
        return *this;
    }
    
    // Set callbacks for sending data to GPU
    AnimationManager& onSendHUB75(SendHUB75DataCallback callback) {
        sendHub75Callback_ = callback;
        return *this;
    }
    
    AnimationManager& onSendOLED(SendOLEDDataCallback callback) {
        sendOledCallback_ = callback;
        return *this;
    }
    
    // ========================================================
    // Quick Setup - Simple API for common use cases
    // ========================================================
    
    // Set a solid color on HUB75
    AnimationManager& setSolidColor(const RGB& color) {
        quickSolid_.setColor(color);
        activeScene_ = nullptr;  // Use quick shader directly
        quickShader_ = &quickSolid_;
        return *this;
    }
    
    // Set rainbow animation
    AnimationManager& setRainbow(float speed = 1.0f) {
        quickRainbow_.setSpeed(speed);
        activeScene_ = nullptr;
        quickShader_ = &quickRainbow_;
        return *this;
    }
    
    // Set plasma effect
    AnimationManager& setPlasma(float speed = 1.0f, float scale = 1.0f) {
        quickPlasma_.setSpeed(speed).setScale(scale);
        activeScene_ = nullptr;
        quickShader_ = &quickPlasma_;
        return *this;
    }
    
    // Set palette-based animation
    AnimationManager& setPalette(const ColorPalette& palette, float speed = 0.5f) {
        quickPalette_.setPalette(palette).setSpeed(speed);
        activeScene_ = nullptr;
        quickShader_ = &quickPalette_;
        return *this;
    }
    
    // ========================================================
    // Scene Management
    // ========================================================
    
    // Create a new scene (owned by manager)
    AnimationScene& createScene(const char* name) {
        if (sceneCount_ < MAX_SCENES) {
            AnimationScene* scene = new AnimationScene(name);
            scenes_[sceneCount_] = scene;
            ownedScenes_[sceneCount_] = true;
            sceneCount_++;
            return *scene;
        }
        // Return last scene as fallback (not ideal, but safe)
        return *scenes_[sceneCount_ - 1];
    }
    
    // Add external scene (not owned)
    AnimationManager& addScene(AnimationScene* scene) {
        if (sceneCount_ < MAX_SCENES && scene) {
            scenes_[sceneCount_] = scene;
            ownedScenes_[sceneCount_] = false;
            sceneCount_++;
        }
        return *this;
    }
    
    // Set active scene by name
    AnimationManager& setActiveScene(const char* name) {
        for (int i = 0; i < sceneCount_; i++) {
            if (strcmp(scenes_[i]->getName(), name) == 0) {
                activeScene_ = scenes_[i];
                quickShader_ = nullptr;
                return *this;
            }
        }
        return *this;
    }
    
    // Set active scene by pointer
    AnimationManager& setActiveScene(AnimationScene* scene) {
        activeScene_ = scene;
        quickShader_ = nullptr;
        return *this;
    }
    
    // Get scene by name
    AnimationScene* getScene(const char* name) {
        for (int i = 0; i < sceneCount_; i++) {
            if (strcmp(scenes_[i]->getName(), name) == 0) {
                return scenes_[i];
            }
        }
        return nullptr;
    }
    
    // ========================================================
    // Custom Shader Support
    // ========================================================
    
    // Set a custom shader directly
    AnimationManager& setShader(ShaderBase* shader) {
        quickShader_ = shader;
        activeScene_ = nullptr;
        return *this;
    }
    
    // ========================================================
    // Update and Render
    // ========================================================
    
    // Main update function - call every frame
    void update(float deltaTime) {
        if (!running_) return;
        
        time_ += deltaTime;
        frameCount_++;
        
        // Update render targets
        hub75Target_->update(deltaTime);
        
        // Update active scene or quick shader
        if (activeScene_) {
            activeScene_->update(deltaTime);
            activeScene_->render(hub75Target_);
        } else if (quickShader_) {
            hub75Target_->renderShader(quickShader_);
        }
        
        // Auto-send to GPU if configured
        if (config_.autoSendHub75 && sendHub75Callback_) {
            sendHub75Callback_(hub75Target_->getData(), hub75Target_->getDataSize());
        }
    }
    
    // Start animation playback
    AnimationManager& start() {
        running_ = true;
        return *this;
    }
    
    // Stop animation playback
    AnimationManager& stop() {
        running_ = false;
        return *this;
    }
    
    // Pause/resume
    AnimationManager& pause() {
        running_ = false;
        return *this;
    }
    
    AnimationManager& resume() {
        running_ = true;
        return *this;
    }
    
    // Reset time
    AnimationManager& resetTime() {
        time_ = 0.0f;
        hub75Target_->resetTime();
        oledTarget_->resetTime();
        return *this;
    }
    
    // ========================================================
    // Direct Access
    // ========================================================
    
    // Get render targets for custom rendering
    RenderTarget* getHUB75Target() { return hub75Target_; }
    RenderTarget* getOLEDTarget() { return oledTarget_; }
    
    // Get frame buffer data
    const uint8_t* getHUB75Data() const { return hub75Target_->getData(); }
    size_t getHUB75DataSize() const { return hub75Target_->getDataSize(); }
    
    // ========================================================
    // State Queries
    // ========================================================
    
    bool isRunning() const { return running_; }
    float getTime() const { return time_; }
    uint32_t getFrameCount() const { return frameCount_; }
    float getFPS() const { return config_.targetFPS; }
    
private:
    AnimationManagerConfig config_;
    
    // Render targets
    RenderTarget* hub75Target_;
    RenderTarget* oledTarget_;
    
    // Scene management
    AnimationScene* scenes_[MAX_SCENES];
    bool ownedScenes_[MAX_SCENES] = {false};
    int sceneCount_ = 0;
    AnimationScene* activeScene_;
    
    // Quick shaders for simple use cases
    SolidShader quickSolid_;
    RainbowHShader quickRainbow_;
    PlasmaShader quickPlasma_;
    PaletteShader quickPalette_;
    ShaderBase* quickShader_ = nullptr;
    
    // Callbacks
    SendHUB75DataCallback sendHub75Callback_;
    SendOLEDDataCallback sendOledCallback_;
    
    // State
    float time_;
    bool running_;
    uint32_t frameCount_;
};

} // namespace AnimationDriver
