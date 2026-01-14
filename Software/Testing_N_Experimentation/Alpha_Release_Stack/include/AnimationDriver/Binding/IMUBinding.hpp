/*****************************************************************
 * @file IMUBinding.hpp
 * @brief Specialized bindings for IMU data with orientation support
 * 
 * Provides easy binding to accelerometer and gyroscope data with
 * automatic orientation computation and mounting angle compensation.
 *****************************************************************/

#pragma once

#include "ValueBinding.hpp"
#include "../Core/Types.hpp"
#include <cmath>

namespace AnimationDriver {

// ============================================================
// IMU Mounting Orientation
// ============================================================

enum class IMUMounting : uint8_t {
    FLAT_FACE_UP,       // Standard mounting, face up
    FLAT_FACE_DOWN,     // Upside down
    VERTICAL_USB_UP,    // Standing, USB port up
    VERTICAL_USB_DOWN,  // Standing, USB port down
    TILTED_FORWARD,     // Tilted forward (like on a face/mask)
    TILTED_BACKWARD,    // Tilted backward
    CUSTOM              // User-defined rotation
};

// ============================================================
// IMU Configuration
// ============================================================

struct IMUConfig {
    // Mounting angles (degrees)
    float mountPitch = 0.0f;    // Forward/backward tilt
    float mountRoll = 0.0f;     // Left/right tilt
    float mountYaw = 0.0f;      // Rotation around vertical axis
    
    // Calibration offsets (raw sensor values)
    float accelOffsetX = 0.0f;
    float accelOffsetY = 0.0f;
    float accelOffsetZ = 0.0f;
    float gyroOffsetX = 0.0f;
    float gyroOffsetY = 0.0f;
    float gyroOffsetZ = 0.0f;
    
    // Scale factors
    float accelScale = 1.0f;
    float gyroScale = 1.0f;
    
    // Filter settings
    float smoothingTime = 0.1f;     // Seconds for low-pass filter
    float deadzone = 0.02f;         // Ignore small movements
    
    // Presets for common mountings
    static IMUConfig FlatFaceUp() {
        IMUConfig cfg;
        return cfg;
    }
    
    static IMUConfig TiltedForward(float degrees) {
        IMUConfig cfg;
        cfg.mountPitch = degrees;
        return cfg;
    }
    
    static IMUConfig Vertical() {
        IMUConfig cfg;
        cfg.mountPitch = 90.0f;
        return cfg;
    }
};

// ============================================================
// IMU Orientation Computer
// ============================================================

class IMUOrientation {
public:
    IMUOrientation() : pitch_(0.0f), roll_(0.0f), yaw_(0.0f) {}
    
    // Update with raw accelerometer data (returns computed pitch/roll)
    void updateFromAccel(float ax, float ay, float az) {
        // Apply calibration offsets
        ax -= config_.accelOffsetX;
        ay -= config_.accelOffsetY;
        az -= config_.accelOffsetZ;
        
        // Compute pitch and roll from accelerometer
        // Pitch: rotation around X axis
        rawPitch_ = atan2f(ay, sqrtf(ax * ax + az * az)) * 180.0f / 3.14159f;
        // Roll: rotation around Y axis
        rawRoll_ = atan2f(-ax, az) * 180.0f / 3.14159f;
        
        // Apply mounting compensation
        pitch_ = rawPitch_ - config_.mountPitch;
        roll_ = rawRoll_ - config_.mountRoll;
        
        // Apply smoothing
        if (initialized_) {
            pitch_ = pitch_ + smoothAlpha_ * (pitch_ - smoothedPitch_);
            roll_ = roll_ + smoothAlpha_ * (roll_ - smoothedRoll_);
        }
        smoothedPitch_ = pitch_;
        smoothedRoll_ = roll_;
        initialized_ = true;
    }
    
    // Update with gyroscope for yaw (requires deltaTime)
    void updateFromGyro(float gx, float gy, float gz, float deltaTime) {
        // Apply calibration
        gz -= config_.gyroOffsetZ;
        
        // Integrate gyro for yaw
        yaw_ += gz * deltaTime * config_.gyroScale;
        
        // Wrap yaw to -180 to 180
        while (yaw_ > 180.0f) yaw_ -= 360.0f;
        while (yaw_ < -180.0f) yaw_ += 360.0f;
    }
    
    // Set configuration
    void setConfig(const IMUConfig& config) {
        config_ = config;
        smoothAlpha_ = config.smoothingTime > 0.0f ? 0.1f / config.smoothingTime : 1.0f;
    }
    
    // Get computed orientation (compensated for mounting)
    float getPitch() const { return pitch_; }
    float getRoll() const { return roll_; }
    float getYaw() const { return yaw_; }
    
    // Get raw orientation (not compensated)
    float getRawPitch() const { return rawPitch_; }
    float getRawRoll() const { return rawRoll_; }
    
    // Get normalized values (-1 to 1, mapped from -90 to 90 degrees)
    float getNormalizedPitch() const { return pitch_ / 90.0f; }
    float getNormalizedRoll() const { return roll_ / 90.0f; }
    
    // Reset
    void reset() {
        pitch_ = roll_ = yaw_ = 0.0f;
        rawPitch_ = rawRoll_ = 0.0f;
        initialized_ = false;
    }
    
private:
    IMUConfig config_;
    float pitch_, roll_, yaw_;
    float rawPitch_, rawRoll_;
    float smoothedPitch_ = 0.0f;
    float smoothedRoll_ = 0.0f;
    float smoothAlpha_ = 0.1f;
    bool initialized_ = false;
};

// ============================================================
// IMU Binding - Complete IMU integration
// ============================================================

class IMUBinding {
public:
    IMUBinding() = default;
    
    // Set data providers (connect to SyncState)
    IMUBinding& setAccelProviders(ValueProvider ax, ValueProvider ay, ValueProvider az) {
        accelX_ = ax;
        accelY_ = ay;
        accelZ_ = az;
        return *this;
    }
    
    IMUBinding& setGyroProviders(ValueProvider gx, ValueProvider gy, ValueProvider gz) {
        gyroX_ = gx;
        gyroY_ = gy;
        gyroZ_ = gz;
        return *this;
    }
    
    // Configure
    IMUBinding& setConfig(const IMUConfig& config) {
        config_ = config;
        orientation_.setConfig(config);
        setupFilters();
        return *this;
    }
    
    // Set mounting angle (shortcut for common use case)
    IMUBinding& setMountingAngle(float pitch, float roll = 0.0f, float yaw = 0.0f) {
        config_.mountPitch = pitch;
        config_.mountRoll = roll;
        config_.mountYaw = yaw;
        orientation_.setConfig(config_);
        return *this;
    }
    
    // Set calibration offsets
    IMUBinding& setAccelOffset(float x, float y, float z) {
        config_.accelOffsetX = x;
        config_.accelOffsetY = y;
        config_.accelOffsetZ = z;
        orientation_.setConfig(config_);
        return *this;
    }
    
    // Update (call each frame)
    void update(float deltaTime) {
        if (!accelX_ || !accelY_ || !accelZ_) return;
        
        // Read raw values
        float ax = accelX_();
        float ay = accelY_();
        float az = accelZ_();
        
        // Update orientation
        orientation_.updateFromAccel(ax, ay, az);
        
        // Update gyro if available
        if (gyroX_ && gyroY_ && gyroZ_) {
            orientation_.updateFromGyro(gyroX_(), gyroY_(), gyroZ_(), deltaTime);
        }
        
        // Process through filters
        filteredPitch_ = pitchFilter_.process(orientation_.getPitch());
        filteredRoll_ = rollFilter_.process(orientation_.getRoll());
    }
    
    // Get orientation values
    float getPitch() const { return filteredPitch_; }
    float getRoll() const { return filteredRoll_; }
    float getYaw() const { return orientation_.getYaw(); }
    
    // Get normalized (-1 to 1)
    float getNormalizedPitch() const { return filteredPitch_ / 90.0f; }
    float getNormalizedRoll() const { return filteredRoll_ / 90.0f; }
    
    // Get raw accelerometer (with calibration but no orientation computation)
    Vec3 getRawAccel() const {
        if (!accelX_ || !accelY_ || !accelZ_) return Vec3(0, 0, 0);
        return Vec3(
            accelX_() - config_.accelOffsetX,
            accelY_() - config_.accelOffsetY,
            accelZ_() - config_.accelOffsetZ
        );
    }
    
    // Get raw gyroscope (with calibration)
    Vec3 getRawGyro() const {
        if (!gyroX_ || !gyroY_ || !gyroZ_) return Vec3(0, 0, 0);
        return Vec3(
            gyroX_() - config_.gyroOffsetX,
            gyroY_() - config_.gyroOffsetY,
            gyroZ_() - config_.gyroOffsetZ
        );
    }
    
    // Create value providers for binding to parameters
    ValueProvider pitchProvider() const {
        return [this]() { return this->getNormalizedPitch(); };
    }
    
    ValueProvider rollProvider() const {
        return [this]() { return this->getNormalizedRoll(); };
    }
    
    ValueProvider yawProvider() const {
        return [this]() { return this->orientation_.getYaw() / 180.0f; };
    }
    
    // Reset
    void reset() {
        orientation_.reset();
        pitchFilter_.reset();
        rollFilter_.reset();
        filteredPitch_ = 0.0f;
        filteredRoll_ = 0.0f;
    }
    
private:
    void setupFilters() {
        pitchFilter_.clear();
        rollFilter_.clear();
        
        if (config_.deadzone > 0.0f) {
            pitchFilter_.deadzone(config_.deadzone);
            rollFilter_.deadzone(config_.deadzone);
        }
        
        if (config_.smoothingTime > 0.0f) {
            pitchFilter_.smooth(config_.smoothingTime);
            rollFilter_.smooth(config_.smoothingTime);
        }
    }
    
    IMUConfig config_;
    IMUOrientation orientation_;
    
    ValueProvider accelX_, accelY_, accelZ_;
    ValueProvider gyroX_, gyroY_, gyroZ_;
    
    FilterChain pitchFilter_;
    FilterChain rollFilter_;
    
    float filteredPitch_ = 0.0f;
    float filteredRoll_ = 0.0f;
};

} // namespace AnimationDriver
