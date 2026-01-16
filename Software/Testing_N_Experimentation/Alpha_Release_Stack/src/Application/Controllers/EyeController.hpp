/*****************************************************************
 * File:      EyeController.hpp
 * Category:  Application/Controllers
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    High-level eye animation controller that runs on Core 0.
 *    Provides easy-to-use API for controlling eye animations.
 *    Communicates with Core 1 via the animation buffer.
 * 
 *    Features:
 *    - Look position control (manual + sensor-driven)
 *    - Blink control (manual + automatic)
 *    - Expression/emotion switching
 *    - Shader and color control
 *    - Idle behaviors (random look, auto-blink)
 *****************************************************************/

#ifndef ARCOS_APPLICATION_EYE_CONTROLLER_HPP_
#define ARCOS_APPLICATION_EYE_CONTROLLER_HPP_

#include <stdint.h>
#include <cmath>
#include <cstdlib>
#include "../Core/SyncBuffer.hpp"
#include "esp_timer.h"

namespace Application {

// ============================================================
// Eye Controller Configuration
// ============================================================

struct EyeControllerConfig {
  // Auto-blink settings
  bool autoBlinkEnabled = true;
  float autoBlinkIntervalMin = 2.5f;
  float autoBlinkIntervalMax = 5.0f;
  
  // Idle look settings
  bool idleLookEnabled = true;
  float idleLookInterval = 3.0f;
  float idleLookRange = 0.3f;
  
  // IMU look control
  bool imuLookEnabled = false;
  float imuSensitivity = 0.05f;
  float imuDeadzone = 5.0f;  // degrees
  
  // Audio reactivity
  bool audioReactiveEnabled = false;
  float audioLookScale = 0.2f;
  float audioPulseScale = 0.3f;
  
  // Default visuals
  uint8_t defaultShader = 1;       // Rainbow
  float defaultShaderSpeed = 1.0f;
  uint8_t defaultBrightness = 80;
  uint8_t primaryR = 255;
  uint8_t primaryG = 255;
  uint8_t primaryB = 255;
  uint8_t secondaryR = 0;
  uint8_t secondaryG = 0;
  uint8_t secondaryB = 255;
  bool mirrorMode = true;
};

// ============================================================
// Idle Behavior State
// ============================================================

struct IdleState {
  float lookTimer = 0.0f;
  float lookTargetX = 0.0f;
  float lookTargetY = 0.0f;
  float nextLookTime = 3.0f;
  
  float blinkTimer = 0.0f;
  float nextBlinkTime = 3.0f;
};

// ============================================================
// Eye Controller
// ============================================================

class EyeController {
public:
  EyeController()
    : animBuffer_(nullptr)
    , time_(0.0f)
    , frameId_(0)
    , manualOverride_(false)
    , manualOverrideTimer_(0.0f)
  {
    // Initialize with defaults
    params_.shaderType = config_.defaultShader;
    params_.shaderSpeed = config_.defaultShaderSpeed;
    params_.brightness = config_.defaultBrightness;
    params_.primaryR = config_.primaryR;
    params_.primaryG = config_.primaryG;
    params_.primaryB = config_.primaryB;
    params_.secondaryR = config_.secondaryR;
    params_.secondaryG = config_.secondaryG;
    params_.secondaryB = config_.secondaryB;
    params_.mirrorMode = config_.mirrorMode;
  }
  
  /** Initialize with animation buffer for Core 1 communication */
  void init(AnimationBuffer* buffer) {
    animBuffer_ = buffer;
  }
  
  /** Configure the controller */
  void configure(const EyeControllerConfig& config) {
    config_ = config;
    params_.shaderType = config_.defaultShader;
    params_.shaderSpeed = config_.defaultShaderSpeed;
    params_.brightness = config_.defaultBrightness;
    params_.primaryR = config_.primaryR;
    params_.primaryG = config_.primaryG;
    params_.primaryB = config_.primaryB;
    params_.secondaryR = config_.secondaryR;
    params_.secondaryG = config_.secondaryG;
    params_.secondaryB = config_.secondaryB;
    params_.mirrorMode = config_.mirrorMode;
  }
  
  // ========================================================
  // Manual Control API (Core 0)
  // ========================================================
  
  /** Set eye look position
   * @param x Horizontal position (-1 = left, 0 = center, 1 = right)
   * @param y Vertical position (-1 = down, 0 = center, 1 = up)
   */
  void setLook(float x, float y) {
    params_.lookX = clamp(x, -1.0f, 1.0f);
    params_.lookY = clamp(y, -1.0f, 1.0f);
    setManualOverride();
  }
  
  /** Trigger a blink */
  void triggerBlink() {
    params_.blinkProgress = 1.0f;  // Signal to Core 1
    setManualOverride();
  }
  
  /** Set expression/eye set
   * @param id Expression ID (0-15)
   * @param blend Optional blend factor for transition
   */
  void setExpression(uint8_t id, float blend = 1.0f) {
    params_.expressionId = id;
    params_.expressionBlend = clamp(blend, 0.0f, 1.0f);
  }
  
  /** Set shader type
   * @param type 0=solid, 1=rainbow, 2=gradient, 3=pulse, 4=plasma
   * @param speed Animation speed multiplier
   */
  void setShader(uint8_t type, float speed = 1.0f) {
    params_.shaderType = type;
    params_.shaderSpeed = speed;
  }
  
  /** Set primary color */
  void setPrimaryColor(uint8_t r, uint8_t g, uint8_t b) {
    params_.primaryR = r;
    params_.primaryG = g;
    params_.primaryB = b;
  }
  
  /** Set secondary color (for gradient, etc.) */
  void setSecondaryColor(uint8_t r, uint8_t g, uint8_t b) {
    params_.secondaryR = r;
    params_.secondaryG = g;
    params_.secondaryB = b;
  }
  
  /** Set brightness (0-100) */
  void setBrightness(uint8_t brightness) {
    params_.brightness = brightness > 100 ? 100 : brightness;
  }
  
  /** Set mirror mode */
  void setMirrorMode(bool enabled) {
    params_.mirrorMode = enabled;
  }
  
  // ========================================================
  // Sensor Input API (Core 0)
  // ========================================================
  
  /** Update from IMU data
   * @param pitch Pitch angle in degrees
   * @param roll Roll angle in degrees
   */
  void updateFromIMU(float pitch, float roll) {
    if (!config_.imuLookEnabled) return;
    
    // Apply deadzone
    if (fabsf(pitch) < config_.imuDeadzone) pitch = 0;
    if (fabsf(roll) < config_.imuDeadzone) roll = 0;
    
    // Map to eye position
    float x = clamp(roll * config_.imuSensitivity, -1.0f, 1.0f);
    float y = clamp(-pitch * config_.imuSensitivity, -1.0f, 1.0f);
    
    params_.lookX = x;
    params_.lookY = y;
  }
  
  /** Update from audio level
   * @param levelDb Audio level in dB (-60 to 0)
   */
  void updateFromAudio(float levelDb) {
    if (!config_.audioReactiveEnabled) return;
    
    // Normalize to 0-1
    float level = (levelDb + 60.0f) / 60.0f;
    level = clamp(level, 0.0f, 1.0f);
    
    // Subtle vertical look on sound peaks
    if (level > 0.7f) {
      params_.lookY = (level - 0.7f) * config_.audioLookScale * 3.0f;
    }
    
    // Could also modulate brightness or shader speed
  }
  
  // ========================================================
  // Update Loop (Call from Core 0)
  // ========================================================
  
  /** Update controller and publish to Core 1
   * @param deltaTime Time since last update in seconds
   */
  void update(float deltaTime) {
    time_ += deltaTime;
    frameId_++;
    
    // Handle manual override timeout
    if (manualOverride_) {
      manualOverrideTimer_ -= deltaTime;
      if (manualOverrideTimer_ <= 0.0f) {
        manualOverride_ = false;
      }
    }
    
    // Run idle behaviors if no manual override
    if (!manualOverride_) {
      updateIdleBehaviors(deltaTime);
    }
    
    // Clear blink signal after sending (it's just a trigger)
    params_.blinkProgress = 0.0f;
    
    // Update frame ID
    params_.frameId = frameId_;
    
    // Publish to Core 1
    if (animBuffer_) {
      AnimationParams& writeBuffer = animBuffer_->getWriteBuffer();
      writeBuffer = params_;
      animBuffer_->publishWrite();
    }
  }
  
  // ========================================================
  // State Queries
  // ========================================================
  
  float getLookX() const { return params_.lookX; }
  float getLookY() const { return params_.lookY; }
  uint8_t getExpression() const { return params_.expressionId; }
  uint8_t getShaderType() const { return params_.shaderType; }
  uint8_t getBrightness() const { return params_.brightness; }
  uint32_t getFrameId() const { return frameId_; }
  float getTime() const { return time_; }
  bool isManualOverride() const { return manualOverride_; }
  
  /** Get current params (for debugging) */
  const AnimationParams& getParams() const { return params_; }
  
  /** Get config */
  EyeControllerConfig& getConfig() { return config_; }
  
private:
  AnimationBuffer* animBuffer_;
  AnimationParams params_;
  EyeControllerConfig config_;
  IdleState idle_;
  
  float time_;
  uint32_t frameId_;
  
  bool manualOverride_;
  float manualOverrideTimer_;
  
  static constexpr float MANUAL_OVERRIDE_DURATION = 2.0f;
  
  void setManualOverride() {
    manualOverride_ = true;
    manualOverrideTimer_ = MANUAL_OVERRIDE_DURATION;
  }
  
  static float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }
  
  static float randomFloat() {
    return (float)(rand() % 1000) / 1000.0f;
  }
  
  void updateIdleBehaviors(float deltaTime) {
    // Auto-blink
    if (config_.autoBlinkEnabled) {
      idle_.blinkTimer += deltaTime;
      if (idle_.blinkTimer >= idle_.nextBlinkTime) {
        triggerBlink();
        idle_.blinkTimer = 0.0f;
        // Random next blink time
        idle_.nextBlinkTime = config_.autoBlinkIntervalMin + 
          randomFloat() * (config_.autoBlinkIntervalMax - config_.autoBlinkIntervalMin);
      }
    }
    
    // Idle look
    if (config_.idleLookEnabled) {
      idle_.lookTimer += deltaTime;
      if (idle_.lookTimer >= idle_.nextLookTime) {
        // Pick new random target
        idle_.lookTargetX = (randomFloat() - 0.5f) * 2.0f * config_.idleLookRange;
        idle_.lookTargetY = (randomFloat() - 0.5f) * 2.0f * config_.idleLookRange;
        idle_.lookTimer = 0.0f;
        // Random next look time
        idle_.nextLookTime = 2.0f + randomFloat() * 4.0f;
      }
      
      // Smooth towards target
      float smoothing = 0.05f;
      params_.lookX += (idle_.lookTargetX - params_.lookX) * smoothing;
      params_.lookY += (idle_.lookTargetY - params_.lookY) * smoothing;
    }
  }
};

// ============================================================
// Global Instance
// ============================================================

inline EyeController& getEyeController() {
  static EyeController instance;
  return instance;
}

} // namespace Application

#endif // ARCOS_APPLICATION_EYE_CONTROLLER_HPP_
