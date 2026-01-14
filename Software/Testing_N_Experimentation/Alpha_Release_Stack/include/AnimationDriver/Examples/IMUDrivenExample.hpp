/*****************************************************************
 * @file IMUDrivenExample.hpp
 * @brief Example: IMU-driven animation with RGB hue cycling
 * 
 * Demonstrates using the IMU to detect device position (with
 * mounting angle compensation) to drive an animation parameter.
 *****************************************************************/

#pragma once

#include "../AnimationDriver.hpp"

namespace AnimationDriver {
namespace Examples {

/**
 * @brief IMU-Driven Hue Cycling Animation
 * 
 * This example shows how to:
 * 1. Set up IMU binding with mounting angle compensation
 * 2. Create a rainbow shader with speed controlled by device tilt
 * 3. Add smoothing/filtering to prevent jitter
 * 
 * Usage:
 *   IMUDrivenAnimation anim;
 *   anim.setup(syncState);  // Pass your SyncState reference
 *   anim.setMountingAngle(45.0f);  // If mounted at 45° angle
 *   
 *   // In your update loop:
 *   anim.update(deltaTime);
 *   const uint8_t* frameData = anim.getFrameData();
 *   // Send frameData to GPU...
 */
class IMUDrivenAnimation {
public:
    IMUDrivenAnimation() : manager_(), imu_(), rainbow_() {}
    
    /**
     * @brief Set up the animation with SyncState data providers
     * 
     * @tparam SyncStateType Your SyncState struct type
     * @param state Reference to your SyncState
     */
    template<typename SyncStateType>
    void setup(SyncStateType& state) {
        // Connect IMU to SyncState
        imu_.setAccelProviders(
            [&state]() { return state.accelX; },
            [&state]() { return state.accelY; },
            [&state]() { return state.accelZ; }
        );
        
        imu_.setGyroProviders(
            [&state]() { return state.gyroX; },
            [&state]() { return state.gyroY; },
            [&state]() { return state.gyroZ; }
        );
        
        // Configure IMU filtering
        IMUConfig config;
        config.smoothingTime = 0.15f;   // 150ms smoothing
        config.deadzone = 0.03f;        // 3% deadzone
        imu_.setConfig(config);
        
        // Configure rainbow shader
        rainbow_.setSpeed(1.0f);
        rainbow_.setSaturation(1.0f);
        rainbow_.setValue(1.0f);
        
        // Bind rainbow speed to pitch angle
        // Pitch range: -90 to +90 degrees -> normalized to -1 to 1
        // We map this to speed 0 to 3
        speedBinding_.setProvider(imu_.pitchProvider());
        speedBinding_.setInputRange(-1.0f, 1.0f);   // Normalized pitch
        speedBinding_.setOutputRange(0.0f, 3.0f);   // Speed range
        speedBinding_.smooth(0.2f);                  // Extra smoothing
        
        // Set up manager
        manager_.setShader(&rainbow_).start();
    }
    
    /**
     * @brief Set mounting angle compensation
     * 
     * Use this if your IMU is mounted at an angle (e.g., on a tilted face)
     * 
     * @param pitchOffset Forward/backward tilt in degrees
     * @param rollOffset Left/right tilt in degrees
     */
    void setMountingAngle(float pitchOffset, float rollOffset = 0.0f) {
        imu_.setMountingAngle(pitchOffset, rollOffset);
    }
    
    /**
     * @brief Set accelerometer calibration offsets
     * 
     * Use these to compensate for sensor bias
     */
    void setAccelCalibration(float x, float y, float z) {
        imu_.setAccelOffset(x, y, z);
    }
    
    /**
     * @brief Update animation - call every frame
     * 
     * @param deltaTime Time since last frame in seconds
     */
    void update(float deltaTime) {
        // Update IMU processing
        imu_.update(deltaTime);
        
        // Get speed from binding and apply to shader
        float speed = speedBinding_.get();
        rainbow_.setSpeed(speed);
        
        // Update animation manager (renders to buffer)
        manager_.update(deltaTime);
    }
    
    /**
     * @brief Get rendered frame data for GPU
     */
    const uint8_t* getFrameData() const {
        return manager_.getHUB75Data();
    }
    
    size_t getFrameDataSize() const {
        return manager_.getHUB75DataSize();
    }
    
    // Direct access for debugging
    float getCurrentPitch() const { return imu_.getPitch(); }
    float getCurrentRoll() const { return imu_.getRoll(); }
    float getCurrentSpeed() const { return speedBinding_.get(); }
    
private:
    AnimationManager manager_;
    IMUBinding imu_;
    RainbowHShader rainbow_;
    ValueBinding speedBinding_;
};

/**
 * @brief More complex IMU example with multiple effects
 * 
 * - Pitch controls hue speed
 * - Roll controls brightness
 * - Shake detection triggers sparkle effect
 */
class AdvancedIMUAnimation {
public:
    AdvancedIMUAnimation() = default;
    
    template<typename SyncStateType>
    void setup(SyncStateType& state) {
        // Set up IMU binding
        imu_.setAccelProviders(
            [&state]() { return state.accelX; },
            [&state]() { return state.accelY; },
            [&state]() { return state.accelZ; }
        );
        
        IMUConfig config;
        config.smoothingTime = 0.1f;
        config.deadzone = 0.02f;
        imu_.setConfig(config);
        
        // Set up shaders
        rainbow_.setSpeed(1.0f).setSaturation(1.0f).setValue(1.0f);
        sparkle_.setDensity(0.05f).setSpeed(3.0f);
        sparkle_.setBaseColor(RGB::Black());
        sparkle_.setSparkleColor(RGB::White());
        
        // Set up value bindings
        speedBinding_.setProvider(imu_.pitchProvider());
        speedBinding_.setOutputRange(0.2f, 4.0f);
        speedBinding_.spring(SpringConfig::Smooth());
        
        brightnessBinding_.setProvider(imu_.rollProvider());
        brightnessBinding_.setOutputRange(0.3f, 1.0f);
        brightnessBinding_.smooth(0.2f);
        
        // Create scene with layers
        scene_.setName("IMU Scene");
        scene_.addLayer("rainbow", &rainbow_);
        scene_.addLayer("sparkle", &sparkle_, BlendMode::ADD);
        
        // Initially disable sparkle
        scene_.setLayerEnabled(1, false);
        sparkleOpacity_ = 0.0f;
        
        manager_.setActiveScene(&scene_).start();
    }
    
    void setMountingAngle(float pitch, float roll = 0.0f) {
        imu_.setMountingAngle(pitch, roll);
    }
    
    void update(float deltaTime) {
        // Update IMU
        imu_.update(deltaTime);
        
        // Apply bindings to shaders
        float speed = speedBinding_.get();
        rainbow_.setSpeed(speed);
        
        float brightness = brightnessBinding_.get();
        rainbow_.setValue(brightness);
        
        // Detect shake (high acceleration magnitude)
        Vec3 accel = imu_.getRawAccel();
        float accelMag = sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
        
        // If shake detected, trigger sparkle
        if (accelMag > 1.5f) {  // Threshold for shake
            sparkleOpacity_ = 1.0f;
            scene_.setLayerEnabled(1, true);
        }
        
        // Fade sparkle
        if (sparkleOpacity_ > 0.0f) {
            sparkleOpacity_ -= deltaTime * 2.0f;  // Fade over 0.5 seconds
            if (sparkleOpacity_ <= 0.0f) {
                sparkleOpacity_ = 0.0f;
                scene_.setLayerEnabled(1, false);
            } else {
                scene_.setLayerOpacity(1, sparkleOpacity_);
            }
        }
        
        // Update manager
        manager_.update(deltaTime);
    }
    
    const uint8_t* getFrameData() const { return manager_.getHUB75Data(); }
    size_t getFrameDataSize() const { return manager_.getHUB75DataSize(); }
    
private:
    AnimationManager manager_;
    AnimationScene scene_;
    IMUBinding imu_;
    
    RainbowHShader rainbow_;
    SparkleShader sparkle_;
    
    ValueBinding speedBinding_;
    ValueBinding brightnessBinding_;
    
    float sparkleOpacity_ = 0.0f;
};

} // namespace Examples
} // namespace AnimationDriver
