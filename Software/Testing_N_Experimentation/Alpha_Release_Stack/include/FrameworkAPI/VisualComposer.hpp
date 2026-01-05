/*****************************************************************
 * File:      VisualComposer.hpp
 * Category:  include/FrameworkAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    High-level API for creating animations, effects, and visual
 *    compositions. Abstracts the GPU protocol into easy-to-use
 *    building blocks.
 * 
 * Usage:
 *    VisualComposer composer;
 *    composer.init();
 *    
 *    // Create a text animation
 *    AnimationId id = composer.createTextScroll("Hello World!", {
 *      .speed = 30.0f,
 *      .color = Color::fromHex(0xFF00FF),
 *      .loop = true
 *    });
 *    
 *    // Create a complex sequence
 *    composer.sequence()
 *      .fadeIn(500)
 *      .hold(2000)
 *      .effect(BuiltinEffect::RAINBOW)
 *      .fadeOut(500)
 *      .play();
 *    
 *    // Use easing functions
 *    float t = composer.ease(Easing::EASE_OUT_ELASTIC, progress);
 *****************************************************************/

#ifndef ARCOS_INCLUDE_FRAMEWORKAPI_VISUAL_COMPOSER_HPP_
#define ARCOS_INCLUDE_FRAMEWORKAPI_VISUAL_COMPOSER_HPP_

#include "FrameworkTypes.hpp"
#include <cstring>
#include <cmath>
#include <functional>

namespace arcos::framework {

// Forward declarations
class VisualComposer;

/**
 * Animation identifier
 */
using AnimationId = uint16_t;
constexpr AnimationId INVALID_ANIMATION = 0xFFFF;

/**
 * Text scroll options
 */
struct TextScrollOptions {
  float speed = 30.0f;        // pixels per second
  Color color = Color{255, 255, 255, 255};
  bool loop = true;
  int16_t y_offset = 0;
  Display target = Display::HUB75;
};

/**
 * Fade options
 */
struct FadeOptions {
  uint32_t duration_ms = 500;
  Easing easing = Easing::LINEAR;
  Display target = Display::ALL;
};

/**
 * Effect options
 */
struct EffectOptions {
  BuiltinEffect effect = BuiltinEffect::NONE;
  float intensity = 1.0f;
  float speed = 1.0f;
  Color primary = Color{255, 0, 0, 255};
  Color secondary = Color{0, 0, 255, 255};
  Display target = Display::ALL;
};

/**
 * Layer for composition
 */
struct Layer {
  AnimationId animation = INVALID_ANIMATION;
  BlendMode blend = BlendMode::NORMAL;
  uint8_t opacity = 255;
  bool visible = true;
  int16_t x = 0;
  int16_t y = 0;
};

/**
 * Animation state
 */
enum class AnimState : uint8_t {
  STOPPED,
  PLAYING,
  PAUSED,
  FINISHED
};

/**
 * Internal animation data
 */
struct Animation {
  AnimationId id = INVALID_ANIMATION;
  AnimState state = AnimState::STOPPED;
  Display target = Display::ALL;
  uint32_t start_time = 0;
  uint32_t duration = 0;
  uint32_t elapsed = 0;
  bool loop = false;
  bool active = false;
  
  // Animation-specific data
  enum class Type : uint8_t {
    NONE,
    TEXT_SCROLL,
    FADE_IN,
    FADE_OUT,
    EFFECT,
    CUSTOM
  } type = Type::NONE;
  
  // Text scroll data
  char text[64] = "";
  float scroll_speed = 30.0f;
  float scroll_x = 0;
  Color color;
  int16_t y_offset = 0;
  
  // Fade data
  Easing easing = Easing::LINEAR;
  float fade_start = 0;
  float fade_end = 1;
  float current_value = 0;
  
  // Effect data
  BuiltinEffect effect_type = BuiltinEffect::NONE;
  float intensity = 1.0f;
  float effect_speed = 1.0f;
  Color primary;
  Color secondary;
};

/**
 * Sequence builder for chaining animations
 */
class SequenceBuilder {
public:
  SequenceBuilder(VisualComposer* composer) : composer_(composer) {
    step_count_ = 0;
    memset(steps_, 0, sizeof(steps_));
  }
  
  /**
   * Add fade in step
   */
  SequenceBuilder& fadeIn(uint32_t duration_ms, Easing easing = Easing::EASE_OUT_QUAD) {
    if (step_count_ < MAX_STEPS) {
      Step& s = steps_[step_count_++];
      s.type = StepType::FADE_IN;
      s.duration = duration_ms;
      s.easing = easing;
    }
    return *this;
  }
  
  /**
   * Add fade out step
   */
  SequenceBuilder& fadeOut(uint32_t duration_ms, Easing easing = Easing::EASE_IN_QUAD) {
    if (step_count_ < MAX_STEPS) {
      Step& s = steps_[step_count_++];
      s.type = StepType::FADE_OUT;
      s.duration = duration_ms;
      s.easing = easing;
    }
    return *this;
  }
  
  /**
   * Add hold/delay step
   */
  SequenceBuilder& hold(uint32_t duration_ms) {
    if (step_count_ < MAX_STEPS) {
      Step& s = steps_[step_count_++];
      s.type = StepType::HOLD;
      s.duration = duration_ms;
    }
    return *this;
  }
  
  /**
   * Add effect step
   */
  SequenceBuilder& effect(BuiltinEffect effect, uint32_t duration_ms = 0) {
    if (step_count_ < MAX_STEPS) {
      Step& s = steps_[step_count_++];
      s.type = StepType::EFFECT;
      s.duration = duration_ms;
      s.effect = effect;
    }
    return *this;
  }
  
  /**
   * Add text display step
   */
  SequenceBuilder& text(const char* str, uint32_t duration_ms, Color color = {255,255,255,255}) {
    if (step_count_ < MAX_STEPS) {
      Step& s = steps_[step_count_++];
      s.type = StepType::TEXT;
      s.duration = duration_ms;
      s.color = color;
      strncpy(s.text, str, sizeof(s.text) - 1);
    }
    return *this;
  }
  
  /**
   * Add custom callback step
   */
  SequenceBuilder& callback(std::function<void()> cb) {
    if (step_count_ < MAX_STEPS) {
      Step& s = steps_[step_count_++];
      s.type = StepType::CALLBACK;
      s.duration = 0;
      s.callback = cb;
    }
    return *this;
  }
  
  /**
   * Set loop mode
   */
  SequenceBuilder& loop(bool enable = true) {
    loop_ = enable;
    return *this;
  }
  
  /**
   * Set target display
   */
  SequenceBuilder& target(Display d) {
    target_ = d;
    return *this;
  }
  
  /**
   * Play the sequence
   */
  AnimationId play();
  
  /**
   * Build without playing (returns sequence animation ID)
   */
  AnimationId build();
  
private:
  friend class VisualComposer;
  
  static constexpr size_t MAX_STEPS = 16;
  
  enum class StepType : uint8_t {
    NONE,
    FADE_IN,
    FADE_OUT,
    HOLD,
    EFFECT,
    TEXT,
    CALLBACK
  };
  
  struct Step {
    StepType type = StepType::NONE;
    uint32_t duration = 0;
    Easing easing = Easing::LINEAR;
    BuiltinEffect effect = BuiltinEffect::NONE;
    Color color;
    char text[32] = "";
    std::function<void()> callback;
  };
  
  VisualComposer* composer_;
  Step steps_[MAX_STEPS];
  size_t step_count_ = 0;
  bool loop_ = false;
  Display target_ = Display::ALL;
};

/**
 * VisualComposer - High-level animation and effects API
 * 
 * Provides:
 * - Text scrolling and display
 * - Fade in/out animations
 * - Built-in effects (rainbow, pulse, etc.)
 * - Animation sequencing
 * - Easing functions
 * - Layer composition
 */
class VisualComposer {
public:
  static constexpr size_t MAX_ANIMATIONS = 16;
  static constexpr size_t MAX_LAYERS = 8;
  
  VisualComposer() = default;
  
  /**
   * Initialize composer
   */
  Result init() {
    next_id_ = 0;
    animation_count_ = 0;
    layer_count_ = 0;
    brightness_ = 255;
    
    memset(animations_, 0, sizeof(animations_));
    memset(layers_, 0, sizeof(layers_));
    
    initialized_ = true;
    return Result::OK;
  }
  
  // ==================== Animation Creation ====================
  
  /**
   * Create scrolling text animation
   */
  AnimationId createTextScroll(const char* text, const TextScrollOptions& opts = {}) {
    if (!initialized_) return INVALID_ANIMATION;
    
    Animation* anim = allocAnimation();
    if (!anim) return INVALID_ANIMATION;
    
    anim->type = Animation::Type::TEXT_SCROLL;
    anim->target = opts.target;
    anim->loop = opts.loop;
    anim->scroll_speed = opts.speed;
    anim->color = opts.color;
    anim->y_offset = opts.y_offset;
    anim->scroll_x = 128;  // Start off-screen right
    strncpy(anim->text, text, sizeof(anim->text) - 1);
    
    // Calculate duration based on text length
    float text_width = strlen(text) * 6;  // Approximate width
    anim->duration = static_cast<uint32_t>((128 + text_width) / opts.speed * 1000);
    
    return anim->id;
  }
  
  /**
   * Create fade animation
   */
  AnimationId createFade(bool fade_in, const FadeOptions& opts = {}) {
    if (!initialized_) return INVALID_ANIMATION;
    
    Animation* anim = allocAnimation();
    if (!anim) return INVALID_ANIMATION;
    
    anim->type = fade_in ? Animation::Type::FADE_IN : Animation::Type::FADE_OUT;
    anim->target = opts.target;
    anim->duration = opts.duration_ms;
    anim->easing = opts.easing;
    anim->fade_start = fade_in ? 0.0f : 1.0f;
    anim->fade_end = fade_in ? 1.0f : 0.0f;
    anim->current_value = anim->fade_start;
    anim->loop = false;
    
    return anim->id;
  }
  
  /**
   * Create effect animation
   */
  AnimationId createEffect(const EffectOptions& opts) {
    if (!initialized_) return INVALID_ANIMATION;
    
    Animation* anim = allocAnimation();
    if (!anim) return INVALID_ANIMATION;
    
    anim->type = Animation::Type::EFFECT;
    anim->target = opts.target;
    anim->effect_type = opts.effect;
    anim->intensity = opts.intensity;
    anim->effect_speed = opts.speed;
    anim->primary = opts.primary;
    anim->secondary = opts.secondary;
    anim->loop = true;  // Effects typically loop
    anim->duration = 0; // Infinite
    
    return anim->id;
  }
  
  // ==================== Animation Control ====================
  
  /**
   * Play animation
   */
  Result play(AnimationId id) {
    Animation* anim = findAnimation(id);
    if (!anim) return Result::INVALID_PARAMETER;
    
    anim->state = AnimState::PLAYING;
    anim->start_time = current_time_;
    anim->elapsed = 0;
    
    return Result::OK;
  }
  
  /**
   * Pause animation
   */
  Result pause(AnimationId id) {
    Animation* anim = findAnimation(id);
    if (!anim) return Result::INVALID_PARAMETER;
    
    if (anim->state == AnimState::PLAYING) {
      anim->state = AnimState::PAUSED;
    }
    return Result::OK;
  }
  
  /**
   * Resume paused animation
   */
  Result resume(AnimationId id) {
    Animation* anim = findAnimation(id);
    if (!anim) return Result::INVALID_PARAMETER;
    
    if (anim->state == AnimState::PAUSED) {
      anim->state = AnimState::PLAYING;
      anim->start_time = current_time_ - anim->elapsed;
    }
    return Result::OK;
  }
  
  /**
   * Stop animation
   */
  Result stop(AnimationId id) {
    Animation* anim = findAnimation(id);
    if (!anim) return Result::INVALID_PARAMETER;
    
    anim->state = AnimState::STOPPED;
    anim->elapsed = 0;
    return Result::OK;
  }
  
  /**
   * Remove animation
   */
  Result remove(AnimationId id) {
    Animation* anim = findAnimation(id);
    if (!anim) return Result::INVALID_PARAMETER;
    
    anim->active = false;
    anim->id = INVALID_ANIMATION;
    return Result::OK;
  }
  
  /**
   * Stop all animations
   */
  void stopAll() {
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      if (animations_[i].active) {
        animations_[i].state = AnimState::STOPPED;
      }
    }
  }
  
  /**
   * Clear all animations
   */
  void clear() {
    memset(animations_, 0, sizeof(animations_));
    animation_count_ = 0;
  }
  
  // ==================== Sequence Builder ====================
  
  /**
   * Create a sequence builder
   */
  SequenceBuilder sequence() {
    return SequenceBuilder(this);
  }
  
  // ==================== Layer Management ====================
  
  /**
   * Add layer
   */
  Result addLayer(AnimationId animation, BlendMode blend = BlendMode::NORMAL, 
                  uint8_t opacity = 255) {
    if (layer_count_ >= MAX_LAYERS) return Result::BUFFER_FULL;
    
    Layer& l = layers_[layer_count_++];
    l.animation = animation;
    l.blend = blend;
    l.opacity = opacity;
    l.visible = true;
    
    return Result::OK;
  }
  
  /**
   * Set layer visibility
   */
  Result setLayerVisible(size_t index, bool visible) {
    if (index >= layer_count_) return Result::INVALID_PARAMETER;
    layers_[index].visible = visible;
    return Result::OK;
  }
  
  /**
   * Set layer opacity
   */
  Result setLayerOpacity(size_t index, uint8_t opacity) {
    if (index >= layer_count_) return Result::INVALID_PARAMETER;
    layers_[index].opacity = opacity;
    return Result::OK;
  }
  
  // ==================== Global Settings ====================
  
  /**
   * Set global brightness
   */
  void setBrightness(uint8_t brightness) {
    brightness_ = brightness;
  }
  
  /**
   * Get global brightness
   */
  uint8_t getBrightness() const { return brightness_; }
  
  // ==================== Easing Functions ====================
  
  /**
   * Apply easing function
   * @param type Easing type
   * @param t Progress (0.0 to 1.0)
   * @return Eased value (0.0 to 1.0)
   */
  static float ease(Easing type, float t) {
    // Clamp input
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    
    switch (type) {
      case Easing::LINEAR:
        return t;
        
      case Easing::EASE_IN_QUAD:
        return t * t;
        
      case Easing::EASE_OUT_QUAD:
        return t * (2 - t);
        
      case Easing::EASE_IN_OUT_QUAD:
        return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
        
      case Easing::EASE_IN_CUBIC:
        return t * t * t;
        
      case Easing::EASE_OUT_CUBIC:
        return (--t) * t * t + 1;
        
      case Easing::EASE_IN_OUT_CUBIC:
        return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
        
      case Easing::EASE_IN_ELASTIC: {
        if (t == 0 || t == 1) return t;
        float p = 0.3f;
        return -powf(2, 10 * (t - 1)) * sinf((t - 1 - p/4) * (2 * 3.14159f) / p);
      }
        
      case Easing::EASE_OUT_ELASTIC: {
        if (t == 0 || t == 1) return t;
        float p = 0.3f;
        return powf(2, -10 * t) * sinf((t - p/4) * (2 * 3.14159f) / p) + 1;
      }
        
      case Easing::EASE_IN_BOUNCE:
        return 1 - ease(Easing::EASE_OUT_BOUNCE, 1 - t);
        
      case Easing::EASE_OUT_BOUNCE:
        if (t < 1/2.75f) {
          return 7.5625f * t * t;
        } else if (t < 2/2.75f) {
          t -= 1.5f/2.75f;
          return 7.5625f * t * t + 0.75f;
        } else if (t < 2.5f/2.75f) {
          t -= 2.25f/2.75f;
          return 7.5625f * t * t + 0.9375f;
        } else {
          t -= 2.625f/2.75f;
          return 7.5625f * t * t + 0.984375f;
        }
        
      default:
        return t;
    }
  }
  
  // ==================== Update ====================
  
  /**
   * Update all animations
   * @param dt_ms Time since last update
   * @param render_callback Called to render each animation frame
   */
  void update(uint32_t dt_ms, 
              std::function<void(const Animation&, float)> render_callback = nullptr) {
    if (!initialized_) return;
    
    current_time_ += dt_ms;
    
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      Animation& anim = animations_[i];
      if (!anim.active || anim.state != AnimState::PLAYING) continue;
      
      // Update elapsed time
      anim.elapsed = current_time_ - anim.start_time;
      
      // Calculate progress (0.0 to 1.0)
      float progress = (anim.duration > 0) 
        ? static_cast<float>(anim.elapsed) / anim.duration 
        : 0.0f;
      
      // Handle animation completion
      if (anim.duration > 0 && anim.elapsed >= anim.duration) {
        if (anim.loop) {
          anim.start_time = current_time_;
          anim.elapsed = 0;
          progress = 0;
          
          // Reset animation-specific state
          if (anim.type == Animation::Type::TEXT_SCROLL) {
            anim.scroll_x = 128;
          }
        } else {
          anim.state = AnimState::FINISHED;
          progress = 1.0f;
        }
      }
      
      // Update animation-specific state
      updateAnimation(anim, dt_ms, progress);
      
      // Call render callback
      if (render_callback) {
        render_callback(anim, progress);
      }
    }
  }
  
  /**
   * Get animation state
   */
  AnimState getState(AnimationId id) const {
    const Animation* anim = findAnimationConst(id);
    return anim ? anim->state : AnimState::STOPPED;
  }
  
  /**
   * Check if animation is playing
   */
  bool isPlaying(AnimationId id) const {
    return getState(id) == AnimState::PLAYING;
  }
  
private:
  friend class SequenceBuilder;
  
  Animation* allocAnimation() {
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      if (!animations_[i].active) {
        Animation& anim = animations_[i];
        memset(&anim, 0, sizeof(Animation));
        anim.id = next_id_++;
        anim.active = true;
        anim.state = AnimState::STOPPED;
        animation_count_++;
        return &anim;
      }
    }
    return nullptr;
  }
  
  Animation* findAnimation(AnimationId id) {
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      if (animations_[i].active && animations_[i].id == id) {
        return &animations_[i];
      }
    }
    return nullptr;
  }
  
  const Animation* findAnimationConst(AnimationId id) const {
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      if (animations_[i].active && animations_[i].id == id) {
        return &animations_[i];
      }
    }
    return nullptr;
  }
  
  void updateAnimation(Animation& anim, uint32_t dt_ms, float progress) {
    switch (anim.type) {
      case Animation::Type::TEXT_SCROLL: {
        // Update scroll position
        float dt_sec = dt_ms / 1000.0f;
        anim.scroll_x -= anim.scroll_speed * dt_sec;
        
        // Reset when text scrolls off screen
        float text_width = strlen(anim.text) * 6;
        if (anim.scroll_x < -text_width && anim.loop) {
          anim.scroll_x = 128;
        }
        break;
      }
      
      case Animation::Type::FADE_IN:
      case Animation::Type::FADE_OUT: {
        float eased = ease(anim.easing, progress);
        anim.current_value = anim.fade_start + (anim.fade_end - anim.fade_start) * eased;
        break;
      }
      
      case Animation::Type::EFFECT:
        // Effects are typically stateless per-frame
        break;
        
      default:
        break;
    }
  }
  
  bool initialized_ = false;
  uint32_t current_time_ = 0;
  AnimationId next_id_ = 0;
  
  Animation animations_[MAX_ANIMATIONS];
  size_t animation_count_ = 0;
  
  Layer layers_[MAX_LAYERS];
  size_t layer_count_ = 0;
  
  uint8_t brightness_ = 255;
};

// SequenceBuilder implementations
inline AnimationId SequenceBuilder::play() {
  AnimationId id = build();
  if (id != INVALID_ANIMATION && composer_) {
    composer_->play(id);
  }
  return id;
}

inline AnimationId SequenceBuilder::build() {
  if (!composer_ || step_count_ == 0) return INVALID_ANIMATION;
  
  // For now, create a simple duration-based animation
  // Full implementation would chain animations
  Animation* anim = composer_->allocAnimation();
  if (!anim) return INVALID_ANIMATION;
  
  anim->type = Animation::Type::CUSTOM;
  anim->target = target_;
  anim->loop = loop_;
  
  // Calculate total duration
  uint32_t total = 0;
  for (size_t i = 0; i < step_count_; i++) {
    total += steps_[i].duration;
  }
  anim->duration = total;
  
  return anim->id;
}

} // namespace arcos::framework

#endif // ARCOS_INCLUDE_FRAMEWORKAPI_VISUAL_COMPOSER_HPP_
