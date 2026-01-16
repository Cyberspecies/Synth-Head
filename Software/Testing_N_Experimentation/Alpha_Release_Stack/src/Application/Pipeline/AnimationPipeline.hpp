/*****************************************************************
 * File:      AnimationPipeline.hpp
 * Category:  Application/Pipeline
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    High-level animation pipeline that composes shaders, handles
 *    transitions, and manages animation state. Runs on Core 1
 *    alongside GpuPipeline.
 * 
 *    Features:
 *    - Shader composition (layering multiple effects)
 *    - Smooth transitions between expressions
 *    - Keyframe animation support
 *    - Integration with GpuPipeline for rendering
 *****************************************************************/

#ifndef ARCOS_APPLICATION_ANIMATION_PIPELINE_HPP_
#define ARCOS_APPLICATION_ANIMATION_PIPELINE_HPP_

#include <stdint.h>
#include <cmath>
#include "../Core/SyncBuffer.hpp"
#include "GpuPipeline.hpp"

namespace Application {

// ============================================================
// Easing Functions
// ============================================================

namespace Easing {
  inline float linear(float t) { return t; }
  
  inline float easeInQuad(float t) { return t * t; }
  inline float easeOutQuad(float t) { return t * (2.0f - t); }
  inline float easeInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
  }
  
  inline float easeInCubic(float t) { return t * t * t; }
  inline float easeOutCubic(float t) {
    float t1 = t - 1.0f;
    return t1 * t1 * t1 + 1.0f;
  }
  inline float easeInOutCubic(float t) {
    return t < 0.5f 
      ? 4.0f * t * t * t 
      : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
  }
  
  inline float easeInSine(float t) {
    return 1.0f - cosf(t * 3.14159f * 0.5f);
  }
  inline float easeOutSine(float t) {
    return sinf(t * 3.14159f * 0.5f);
  }
  inline float easeInOutSine(float t) {
    return 0.5f * (1.0f - cosf(3.14159f * t));
  }
  
  inline float easeOutBounce(float t) {
    if (t < 1.0f / 2.75f) {
      return 7.5625f * t * t;
    } else if (t < 2.0f / 2.75f) {
      t -= 1.5f / 2.75f;
      return 7.5625f * t * t + 0.75f;
    } else if (t < 2.5f / 2.75f) {
      t -= 2.25f / 2.75f;
      return 7.5625f * t * t + 0.9375f;
    } else {
      t -= 2.625f / 2.75f;
      return 7.5625f * t * t + 0.984375f;
    }
  }
  
  inline float easeOutElastic(float t) {
    if (t == 0.0f || t == 1.0f) return t;
    return powf(2.0f, -10.0f * t) * sinf((t - 0.075f) * (2.0f * 3.14159f) / 0.3f) + 1.0f;
  }
}

// ============================================================
// Animation Keyframe
// ============================================================

struct Keyframe {
  float time;       // Time in seconds
  float value;      // Target value
  uint8_t easing;   // Easing function ID
  
  Keyframe() : time(0.0f), value(0.0f), easing(0) {}
  Keyframe(float t, float v, uint8_t e = 0) : time(t), value(v), easing(e) {}
};

// ============================================================
// Blink Controller
// ============================================================

class BlinkController {
public:
  BlinkController()
    : enabled_(true)
    , blinking_(false)
    , progress_(0.0f)
    , timer_(0.0f)
    , interval_(3.0f)
    , closeDuration_(0.08f)
    , openDuration_(0.12f)
  {}
  
  void setEnabled(bool enabled) { enabled_ = enabled; }
  void setInterval(float seconds) { interval_ = seconds; }
  void setDurations(float close, float open) {
    closeDuration_ = close;
    openDuration_ = open;
  }
  
  /** Trigger a manual blink */
  void triggerBlink() {
    if (!blinking_) {
      blinking_ = true;
      progress_ = 0.0f;
    }
  }
  
  /** Update blink state
   * @param deltaTime Time since last update in seconds
   * @return Current blink progress (0 = open, 1 = closed)
   */
  float update(float deltaTime) {
    if (enabled_ && !blinking_) {
      timer_ += deltaTime;
      if (timer_ >= interval_) {
        triggerBlink();
        timer_ = 0.0f;
        // Add some randomness to next interval
        interval_ = 2.5f + (rand() % 200) / 100.0f;
      }
    }
    
    if (blinking_) {
      float totalDuration = closeDuration_ + openDuration_;
      progress_ += deltaTime;
      
      if (progress_ >= totalDuration) {
        blinking_ = false;
        progress_ = 0.0f;
        return 0.0f;  // Fully open
      }
      
      // Calculate eye openness based on blink phase
      if (progress_ < closeDuration_) {
        // Closing phase
        float t = progress_ / closeDuration_;
        return Easing::easeInQuad(t);  // Fast close
      } else {
        // Opening phase
        float t = (progress_ - closeDuration_) / openDuration_;
        return 1.0f - Easing::easeOutCubic(t);  // Slower open
      }
    }
    
    return 0.0f;
  }
  
  bool isBlinking() const { return blinking_; }
  
private:
  bool enabled_;
  bool blinking_;
  float progress_;
  float timer_;
  float interval_;
  float closeDuration_;
  float openDuration_;
};

// ============================================================
// Look Controller (Eye Position)
// ============================================================

class LookController {
public:
  LookController()
    : targetX_(0.0f), targetY_(0.0f)
    , currentX_(0.0f), currentY_(0.0f)
    , smoothing_(0.15f)
  {}
  
  void setTarget(float x, float y) {
    targetX_ = clamp(x, -1.0f, 1.0f);
    targetY_ = clamp(y, -1.0f, 1.0f);
  }
  
  void setSmoothing(float s) { smoothing_ = clamp(s, 0.01f, 1.0f); }
  
  /** Update position with smoothing */
  void update(float deltaTime) {
    // Exponential smoothing
    float factor = 1.0f - expf(-smoothing_ * 60.0f * deltaTime);
    currentX_ += (targetX_ - currentX_) * factor;
    currentY_ += (targetY_ - currentY_) * factor;
  }
  
  float getX() const { return currentX_; }
  float getY() const { return currentY_; }
  
private:
  float targetX_, targetY_;
  float currentX_, currentY_;
  float smoothing_;
  
  static float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }
};

// ============================================================
// Expression Controller
// ============================================================

class ExpressionController {
public:
  ExpressionController()
    : currentId_(0), targetId_(0)
    , blendProgress_(1.0f)
    , transitionDuration_(0.3f)
    , transitioning_(false)
  {}
  
  void setExpression(uint8_t id) {
    if (id != targetId_) {
      targetId_ = id;
      blendProgress_ = 0.0f;
      transitioning_ = true;
    }
  }
  
  void setTransitionDuration(float duration) {
    transitionDuration_ = duration;
  }
  
  void update(float deltaTime) {
    if (transitioning_) {
      blendProgress_ += deltaTime / transitionDuration_;
      if (blendProgress_ >= 1.0f) {
        blendProgress_ = 1.0f;
        currentId_ = targetId_;
        transitioning_ = false;
      }
    }
  }
  
  uint8_t getCurrentId() const { return currentId_; }
  uint8_t getTargetId() const { return targetId_; }
  float getBlend() const { 
    return Easing::easeInOutCubic(blendProgress_); 
  }
  bool isTransitioning() const { return transitioning_; }
  
private:
  uint8_t currentId_;
  uint8_t targetId_;
  float blendProgress_;
  float transitionDuration_;
  bool transitioning_;
};

// ============================================================
// Animation Pipeline
// ============================================================

class AnimationPipeline {
public:
  static constexpr const char* TAG = "AnimPipe";
  
  AnimationPipeline()
    : initialized_(false)
    , time_(0.0f)
    , deltaTime_(0.0f)
    , frameCount_(0)
    , gpuPipeline_(nullptr)
    , animBuffer_(nullptr)
  {}
  
  /** Initialize the animation pipeline
   * @param gpu GpuPipeline instance for rendering
   * @param animBuffer Triple buffer for receiving animation params
   */
  bool init(GpuPipeline* gpu, AnimationBuffer* animBuffer) {
    if (initialized_) return true;
    
    gpuPipeline_ = gpu;
    animBuffer_ = animBuffer;
    
    initialized_ = true;
    return true;
  }
  
  /** Update animation state and render one frame
   * @param deltaTime Time since last frame in seconds
   */
  void update(float deltaTime) {
    if (!initialized_ || !gpuPipeline_) return;
    
    time_ += deltaTime;
    deltaTime_ = deltaTime;
    frameCount_++;
    
    // Get latest animation params from Core 0
    AnimationParams params;
    bool hasNew = false;
    if (animBuffer_) {
      hasNew = animBuffer_->swapAndRead(params);
    }
    
    // Skip rendering if paused (manual scene mode)
    if (params.paused) {
      return;  // Don't render anything, let manual scene stay on display
    }
    
    // Update internal controllers
    blinkController_.setEnabled(true);  // Use auto-blink
    
    // If manual blink received from params
    if (params.blinkProgress > 0.5f && !blinkController_.isBlinking()) {
      blinkController_.triggerBlink();
    }
    
    // Update blink
    float blinkProgress = blinkController_.update(deltaTime);
    
    // Update look position with smoothing
    lookController_.setTarget(params.lookX, params.lookY);
    lookController_.update(deltaTime);
    
    // Update expression
    expressionController_.setExpression(params.expressionId);
    expressionController_.update(deltaTime);
    
    // Prepare colors
    RGB primary(params.primaryR, params.primaryG, params.primaryB);
    RGB secondary(params.secondaryR, params.secondaryG, params.secondaryB);
    
    // Render frame
    gpuPipeline_->processFrame(
      lookController_.getX(),
      lookController_.getY(),
      blinkProgress,
      params.shaderType,
      params.shaderSpeed,
      params.brightness,
      primary,
      secondary,
      deltaTime
    );
  }
  
  /** Get time since init */
  float getTime() const { return time_; }
  
  /** Get frame count */
  uint32_t getFrameCount() const { return frameCount_; }
  
  /** Get controllers for direct access */
  BlinkController& getBlinkController() { return blinkController_; }
  LookController& getLookController() { return lookController_; }
  ExpressionController& getExpressionController() { return expressionController_; }
  
private:
  bool initialized_;
  float time_;
  float deltaTime_;
  uint32_t frameCount_;
  
  GpuPipeline* gpuPipeline_;
  AnimationBuffer* animBuffer_;
  
  BlinkController blinkController_;
  LookController lookController_;
  ExpressionController expressionController_;
};

} // namespace Application

#endif // ARCOS_APPLICATION_ANIMATION_PIPELINE_HPP_
