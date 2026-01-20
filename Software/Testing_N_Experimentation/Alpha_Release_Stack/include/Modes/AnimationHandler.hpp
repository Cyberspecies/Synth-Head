/*****************************************************************
 * @file AnimationHandler.hpp
 * @brief Animation Handler for CurrentMode
 * 
 * This file bridges the AnimationSystem with CurrentMode.
 * It handles:
 * - Animation system initialization
 * - Sensor data forwarding to AnimationContext
 * - GPU command routing from AnimationMode
 * 
 * Used by CurrentMode to delegate all animation logic.
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "AnimationSystem/AnimationSystem.hpp"
#include <functional>

namespace Modes {

// Forward declarations for GPU driver types
namespace GpuDriverInterface {
    using ClearFunc = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;
    using BlitSpriteFunc = std::function<void(int id, float x, float y)>;
    using BlitSpriteRotatedFunc = std::function<void(int id, float x, float y, float angle)>;
    using FillCircleFunc = std::function<void(int cx, int cy, int r, uint8_t red, uint8_t green, uint8_t blue)>;
    using FillRectFunc = std::function<void(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)>;
    using PresentFunc = std::function<void()>;
}

/**
 * @brief Animation Handler - manages animation system for CurrentMode
 */
class AnimationHandler {
public:
    AnimationHandler() = default;
    
    /**
     * @brief Initialize the animation handler
     * @return true if successful
     */
    bool init() {
        if (initialized_) return true;
        
        // Initialize the animation system
        if (!AnimationSystem::init()) {
            printf("  AnimationHandler: Failed to init AnimationSystem\n");
            return false;
        }
        
        // Get references
        context_ = &AnimationSystem::getContext();
        registry_ = &AnimationSystem::getParameterRegistry();
        mode_ = &AnimationSystem::getAnimationMode();
        
        // Connect mode to context and registry
        mode_->setContext(context_);
        mode_->setRegistry(registry_);
        
        initialized_ = true;
        printf("  AnimationHandler: Initialized\n");
        return true;
    }
    
    /**
     * @brief Wire GPU callbacks for rendering
     * Call this after GPU driver is initialized
     */
    void wireGpuCallbacks(
        GpuDriverInterface::ClearFunc clearFunc,
        GpuDriverInterface::BlitSpriteFunc blitFunc,
        GpuDriverInterface::BlitSpriteRotatedFunc blitRotatedFunc,
        GpuDriverInterface::FillCircleFunc fillCircleFunc,
        GpuDriverInterface::FillRectFunc fillRectFunc,
        GpuDriverInterface::PresentFunc presentFunc
    ) {
        auto& adapter = mode_->getOutputAdapter();
        
        adapter.onClear = clearFunc;
        adapter.onBlitSprite = blitFunc;
        adapter.onBlitSpriteRotated = blitRotatedFunc;
        adapter.onFillCircle = fillCircleFunc;
        adapter.onFillRect = fillRectFunc;
        adapter.onPresent = presentFunc;
        
        gpuWired_ = true;
        printf("  AnimationHandler: GPU callbacks wired\n");
    }
    
    /**
     * @brief Update sensor inputs
     * Call this every frame to feed sensor data to animations
     */
    void updateSensorInputs(
        float pitch, float roll, float yaw,
        float accelX, float accelY, float accelZ,
        float gyroX, float gyroY, float gyroZ
    ) {
        if (!context_) return;
        
        context_->setInput("imu.pitch", pitch);
        context_->setInput("imu.roll", roll);
        context_->setInput("imu.yaw", yaw);
        context_->setInput("imu.accel_x", accelX);
        context_->setInput("imu.accel_y", accelY);
        context_->setInput("imu.accel_z", accelZ);
        context_->setInput("imu.gyro_x", gyroX);
        context_->setInput("imu.gyro_y", gyroY);
        context_->setInput("imu.gyro_z", gyroZ);
    }
    
    /**
     * @brief Update GPS inputs
     */
    void updateGpsInputs(
        float latitude, float longitude, float altitude,
        float speed, int satellites, bool valid
    ) {
        if (!context_) return;
        
        context_->setInput("gps.latitude", latitude);
        context_->setInput("gps.longitude", longitude);
        context_->setInput("gps.altitude", altitude);
        context_->setInput("gps.speed", speed);
        context_->setInput("gps.satellites", static_cast<float>(satellites));
        context_->setInput("gps.valid", valid ? 1.0f : 0.0f);
    }
    
    /**
     * @brief Update audio inputs
     */
    void updateAudioInputs(float level, float peak, float bass, float mid, float treble) {
        if (!context_) return;
        
        context_->setInput("audio.level", level);
        context_->setInput("audio.peak", peak);
        context_->setInput("audio.bass", bass);
        context_->setInput("audio.mid", mid);
        context_->setInput("audio.treble", treble);
    }
    
    /**
     * @brief Update environment inputs
     */
    void updateEnvironmentInputs(float temperature, float humidity, float pressure) {
        if (!context_) return;
        
        context_->setInput("env.temperature", temperature);
        context_->setInput("env.humidity", humidity);
        context_->setInput("env.pressure", pressure);
    }
    
    /**
     * @brief Update button inputs
     */
    void updateButtonInputs(bool a, bool b, bool c, bool d) {
        if (!context_) return;
        
        context_->setInput("button.a", a ? 1.0f : 0.0f);
        context_->setInput("button.b", b ? 1.0f : 0.0f);
        context_->setInput("button.c", c ? 1.0f : 0.0f);
        context_->setInput("button.d", d ? 1.0f : 0.0f);
    }
    
    /**
     * @brief Register a sprite with the context
     */
    void registerSprite(int id, const char* name, int width, int height,
                        bool inGpu, bool onSd, const char* sdPath = "") {
        if (context_) {
            context_->registerSprite(id, name, width, height, inGpu, onSd, sdPath);
        }
    }
    
    /**
     * @brief Clear all sprites from context
     */
    void clearSprites() {
        if (context_) {
            context_->clearSprites();
        }
    }
    
    /**
     * @brief Update animation system
     * Call every frame from CurrentMode::onUpdate
     */
    void update(uint32_t deltaTimeMs) {
        if (!initialized_) return;
        
        // Update animation system (context + mode)
        AnimationSystem::update(deltaTimeMs);
    }
    
    /**
     * @brief Render animation to GPU
     * Call after update if animation is enabled
     */
    void render() {
        if (!initialized_ || !gpuWired_) return;
        
        mode_->render();
    }
    
    // ========================================================
    // Animation Control
    // ========================================================
    
    /**
     * @brief Set active animation set
     */
    bool setActiveAnimation(const std::string& id) {
        if (!mode_) return false;
        return mode_->setActiveAnimationSet(id);
    }
    
    /**
     * @brief Get active animation set ID
     */
    std::string getActiveAnimationId() const {
        if (!mode_) return "";
        return mode_->getActiveAnimationSetId();
    }
    
    /**
     * @brief Enable animation rendering
     */
    void enableAnimation(bool enable) {
        if (mode_) {
            mode_->setEnabled(enable);
            enabled_ = enable;
        }
    }
    
    /**
     * @brief Check if animation is enabled
     */
    bool isAnimationEnabled() const {
        return enabled_;
    }
    
    // ========================================================
    // Access to System Components
    // ========================================================
    
    AnimationSystem::AnimationContext* getContext() { return context_; }
    AnimationSystem::ParameterRegistry* getRegistry() { return registry_; }
    AnimationSystem::AnimationMode* getMode() { return mode_; }
    
    bool isInitialized() const { return initialized_; }
    bool isGpuWired() const { return gpuWired_; }
    
private:
    AnimationSystem::AnimationContext* context_ = nullptr;
    AnimationSystem::ParameterRegistry* registry_ = nullptr;
    AnimationSystem::AnimationMode* mode_ = nullptr;
    
    bool initialized_ = false;
    bool gpuWired_ = false;
    bool enabled_ = false;
};

/**
 * @brief Global animation handler instance
 */
inline AnimationHandler& getAnimationHandler() {
    static AnimationHandler instance;
    return instance;
}

} // namespace Modes
