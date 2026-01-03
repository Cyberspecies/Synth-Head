/*****************************************************************
 * File:      GpuAnimationSystem.hpp
 * Category:  GPU Driver / Animation Framework
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Comprehensive animation system supporting:
 *    - Keyframe animation with multiple interpolation curves
 *    - Hierarchical animation (parent-child relationships)
 *    - Animation composition (layering, blending, masking)
 *    - Transitions and cross-fading between animations
 *    - Procedural modifiers
 *    - Timeline-based animation control
 * 
 * Animation Model:
 *    - Animations operate on "properties" (position, color, etc.)
 *    - Keyframes define property values at specific times
 *    - Interpolation curves control transitions between keyframes
 *    - Multiple animations can be composed via layers
 *****************************************************************/

#ifndef GPU_ANIMATION_SYSTEM_HPP_
#define GPU_ANIMATION_SYSTEM_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "GpuISA.hpp"

namespace gpu {
namespace animation {

using namespace isa;

// ============================================================
// Animation Constants
// ============================================================

constexpr size_t MAX_ANIMATIONS       = 64;
constexpr size_t MAX_KEYFRAMES        = 256;
constexpr size_t MAX_PROPERTIES       = 32;
constexpr size_t MAX_LAYERS           = 16;
constexpr size_t MAX_TRANSITIONS      = 8;
constexpr size_t MAX_MODIFIERS        = 16;
constexpr size_t MAX_CHILDREN         = 16;
constexpr size_t MAX_TIMELINE_EVENTS  = 64;

// ============================================================
// Property Types
// ============================================================

enum class PropertyType : uint8_t {
  NONE        = 0x00,
  
  // Transform properties
  POSITION_X  = 0x01,
  POSITION_Y  = 0x02,
  POSITION_Z  = 0x03,
  ROTATION    = 0x04,
  ROTATION_X  = 0x05,
  ROTATION_Y  = 0x06,
  ROTATION_Z  = 0x07,
  SCALE_X     = 0x08,
  SCALE_Y     = 0x09,
  SCALE_UNIFORM = 0x0A,
  SKEW_X      = 0x0B,
  SKEW_Y      = 0x0C,
  
  // Appearance
  OPACITY     = 0x10,
  COLOR_R     = 0x11,
  COLOR_G     = 0x12,
  COLOR_B     = 0x13,
  COLOR_A     = 0x14,
  BRIGHTNESS  = 0x15,
  CONTRAST    = 0x16,
  SATURATION  = 0x17,
  HUE         = 0x18,
  
  // Shape properties
  WIDTH       = 0x20,
  HEIGHT      = 0x21,
  RADIUS      = 0x22,
  CORNER_RADIUS = 0x23,
  BORDER_WIDTH = 0x24,
  
  // Text properties
  FONT_SIZE   = 0x30,
  LETTER_SPACING = 0x31,
  LINE_HEIGHT = 0x32,
  
  // Effect parameters
  BLUR_RADIUS = 0x40,
  GLOW_RADIUS = 0x41,
  SHADOW_X    = 0x42,
  SHADOW_Y    = 0x43,
  SHADOW_BLUR = 0x44,
  
  // Custom properties (user-defined)
  CUSTOM_0    = 0xF0,
  CUSTOM_1    = 0xF1,
  CUSTOM_2    = 0xF2,
  CUSTOM_3    = 0xF3,
  CUSTOM_4    = 0xF4,
  CUSTOM_5    = 0xF5,
  CUSTOM_6    = 0xF6,
  CUSTOM_7    = 0xF7,
};

// ============================================================
// Animation State
// ============================================================

enum class AnimationState : uint8_t {
  IDLE        = 0x00,
  PLAYING     = 0x01,
  PAUSED      = 0x02,
  STOPPED     = 0x03,
  FINISHED    = 0x04,
  TRANSITIONING = 0x05,
};

// ============================================================
// Loop Modes
// ============================================================

enum class LoopMode : uint8_t {
  NONE        = 0x00,  // Play once
  LOOP        = 0x01,  // Repeat from start
  PING_PONG   = 0x02,  // Alternate forward/backward
  REVERSE     = 0x03,  // Play once in reverse
  LOOP_REVERSE = 0x04, // Loop in reverse
};

// ============================================================
// Keyframe Structure
// ============================================================

struct Keyframe {
  uint32_t    time_ms;        // Time in milliseconds
  float       value;          // Property value at this keyframe
  EasingType  easing;         // Easing to next keyframe
  float       bezier_cp1_x;   // Bezier control point 1 X (if easing = BEZIER)
  float       bezier_cp1_y;   // Bezier control point 1 Y
  float       bezier_cp2_x;   // Bezier control point 2 X
  float       bezier_cp2_y;   // Bezier control point 2 Y
  
  Keyframe() 
    : time_ms(0), value(0), easing(EasingType::LINEAR),
      bezier_cp1_x(0.25f), bezier_cp1_y(0.1f),
      bezier_cp2_x(0.25f), bezier_cp2_y(1.0f) {}
  
  Keyframe(uint32_t time, float val, EasingType ease = EasingType::LINEAR)
    : time_ms(time), value(val), easing(ease),
      bezier_cp1_x(0.25f), bezier_cp1_y(0.1f),
      bezier_cp2_x(0.25f), bezier_cp2_y(1.0f) {}
};

// ============================================================
// Property Track
// ============================================================

struct PropertyTrack {
  PropertyType  property;
  uint8_t       keyframe_count;
  Keyframe      keyframes[16];  // Max 16 keyframes per track
  
  PropertyTrack() : property(PropertyType::NONE), keyframe_count(0) {}
  
  // Add keyframe (sorted by time)
  bool addKeyframe(const Keyframe& kf) {
    if (keyframe_count >= 16) return false;
    
    // Find insertion point
    uint8_t insert_pos = 0;
    while (insert_pos < keyframe_count && keyframes[insert_pos].time_ms < kf.time_ms) {
      insert_pos++;
    }
    
    // Shift existing keyframes
    for (uint8_t i = keyframe_count; i > insert_pos; i--) {
      keyframes[i] = keyframes[i - 1];
    }
    
    keyframes[insert_pos] = kf;
    keyframe_count++;
    return true;
  }
  
  // Get value at time
  float getValue(uint32_t time_ms) const {
    if (keyframe_count == 0) return 0.0f;
    if (keyframe_count == 1) return keyframes[0].value;
    
    // Find surrounding keyframes
    uint8_t prev_idx = 0;
    uint8_t next_idx = 1;
    
    for (uint8_t i = 0; i < keyframe_count - 1; i++) {
      if (time_ms >= keyframes[i].time_ms && time_ms < keyframes[i + 1].time_ms) {
        prev_idx = i;
        next_idx = i + 1;
        break;
      }
      if (i == keyframe_count - 2 && time_ms >= keyframes[i + 1].time_ms) {
        return keyframes[keyframe_count - 1].value;
      }
    }
    
    if (time_ms < keyframes[0].time_ms) {
      return keyframes[0].value;
    }
    
    const Keyframe& kf_prev = keyframes[prev_idx];
    const Keyframe& kf_next = keyframes[next_idx];
    
    // Calculate normalized time [0, 1]
    float duration = (float)(kf_next.time_ms - kf_prev.time_ms);
    if (duration <= 0) return kf_prev.value;
    
    float t = (float)(time_ms - kf_prev.time_ms) / duration;
    t = fmaxf(0.0f, fminf(1.0f, t));
    
    // Apply easing
    float eased_t;
    if (kf_prev.easing == EasingType::BEZIER) {
      eased_t = evaluateCubicBezier(t, kf_prev.bezier_cp1_x, kf_prev.bezier_cp1_y,
                                    kf_prev.bezier_cp2_x, kf_prev.bezier_cp2_y);
    } else {
      eased_t = evaluateEasing(kf_prev.easing, t);
    }
    
    // Interpolate
    return kf_prev.value + (kf_next.value - kf_prev.value) * eased_t;
  }
  
private:
  // Evaluate cubic bezier for custom easing
  static float evaluateCubicBezier(float t, float p1x, float p1y, float p2x, float p2y) {
    // Newton-Raphson to find x parameter
    float x = t;
    for (int i = 0; i < 8; i++) {
      float x1 = 3.0f * p1x * (1.0f - x) * (1.0f - x) * x;
      float x2 = 3.0f * p2x * (1.0f - x) * x * x;
      float x3 = x * x * x;
      float fx = x1 + x2 + x3 - t;
      
      float dx1 = 3.0f * p1x * (1.0f - 3.0f * x + 2.0f * x * x);
      float dx2 = 3.0f * p2x * (2.0f * x - 3.0f * x * x);
      float dx3 = 3.0f * x * x;
      float dfx = dx1 + dx2 + dx3;
      
      if (fabsf(dfx) < 0.0001f) break;
      x = x - fx / dfx;
    }
    
    // Evaluate y at x
    float y1 = 3.0f * p1y * (1.0f - x) * (1.0f - x) * x;
    float y2 = 3.0f * p2y * (1.0f - x) * x * x;
    float y3 = x * x * x;
    return y1 + y2 + y3;
  }
};

// ============================================================
// Animation Definition
// ============================================================

struct AnimationDef {
  uint8_t       id;
  char          name[32];
  uint32_t      duration_ms;
  LoopMode      loop_mode;
  uint8_t       loop_count;       // 0 = infinite
  float         speed;            // Playback speed multiplier
  uint8_t       track_count;
  PropertyTrack tracks[8];        // Max 8 property tracks
  
  AnimationDef() 
    : id(0), duration_ms(1000), loop_mode(LoopMode::NONE),
      loop_count(0), speed(1.0f), track_count(0) {
    name[0] = '\0';
  }
  
  // Add a property track
  PropertyTrack* addTrack(PropertyType prop) {
    if (track_count >= 8) return nullptr;
    tracks[track_count].property = prop;
    return &tracks[track_count++];
  }
  
  // Get track for property
  PropertyTrack* getTrack(PropertyType prop) {
    for (uint8_t i = 0; i < track_count; i++) {
      if (tracks[i].property == prop) return &tracks[i];
    }
    return nullptr;
  }
  
  const PropertyTrack* getTrack(PropertyType prop) const {
    for (uint8_t i = 0; i < track_count; i++) {
      if (tracks[i].property == prop) return &tracks[i];
    }
    return nullptr;
  }
};

// ============================================================
// Animation Instance (runtime state)
// ============================================================

struct AnimationInstance {
  const AnimationDef* definition;
  AnimationState      state;
  uint32_t           current_time_ms;
  uint32_t           start_time_ms;  // System time when started
  float              current_speed;
  bool               reverse;
  uint8_t            current_loop;
  
  // Cached property values
  float              property_values[32];  // Indexed by PropertyType
  bool               property_dirty[32];
  
  // Hierarchical animation
  uint8_t            parent_id;      // 0xFF = no parent
  uint8_t            child_ids[MAX_CHILDREN];
  uint8_t            child_count;
  
  // Transform accumulation from parent
  float              inherited_x;
  float              inherited_y;
  float              inherited_rotation;
  float              inherited_scale;
  float              inherited_opacity;
  
  AnimationInstance() 
    : definition(nullptr), state(AnimationState::IDLE),
      current_time_ms(0), start_time_ms(0), current_speed(1.0f),
      reverse(false), current_loop(0), parent_id(0xFF), child_count(0),
      inherited_x(0), inherited_y(0), inherited_rotation(0),
      inherited_scale(1.0f), inherited_opacity(1.0f) {
    memset(property_values, 0, sizeof(property_values));
    memset(property_dirty, 0, sizeof(property_dirty));
    memset(child_ids, 0xFF, sizeof(child_ids));
  }
  
  void reset() {
    state = AnimationState::IDLE;
    current_time_ms = 0;
    start_time_ms = 0;
    reverse = false;
    current_loop = 0;
    memset(property_values, 0, sizeof(property_values));
    memset(property_dirty, 0, sizeof(property_dirty));
  }
  
  bool isPlaying() const {
    return state == AnimationState::PLAYING;
  }
  
  float getProperty(PropertyType prop) const {
    return property_values[static_cast<uint8_t>(prop)];
  }
  
  void setProperty(PropertyType prop, float value) {
    property_values[static_cast<uint8_t>(prop)] = value;
    property_dirty[static_cast<uint8_t>(prop)] = true;
  }
};

// ============================================================
// Animation Transition
// ============================================================

struct AnimationTransition {
  uint8_t     from_animation;
  uint8_t     to_animation;
  uint32_t    duration_ms;
  EasingType  easing;
  float       blend_factor;     // Current blend (0 = from, 1 = to)
  bool        active;
  
  AnimationTransition()
    : from_animation(0xFF), to_animation(0xFF), duration_ms(300),
      easing(EasingType::EASE_IN_OUT), blend_factor(0), active(false) {}
};

// ============================================================
// Animation Layer
// ============================================================

enum class LayerBlendMode : uint8_t {
  REPLACE     = 0x00,   // Replace lower layers
  ADDITIVE    = 0x01,   // Add to lower layers
  MULTIPLY    = 0x02,   // Multiply with lower layers
  OVERRIDE    = 0x03,   // Override specific properties only
  AVERAGE     = 0x04,   // Average with lower layers
};

struct AnimationLayer {
  uint8_t         id;
  char            name[32];
  uint8_t         animation_ids[8];   // Animations in this layer
  uint8_t         animation_count;
  float           weight;             // Layer weight (0-1)
  LayerBlendMode  blend_mode;
  bool            enabled;
  
  // Layer mask - which properties this layer affects
  bool            property_mask[32];  // true = affects this property
  
  AnimationLayer()
    : id(0), animation_count(0), weight(1.0f),
      blend_mode(LayerBlendMode::REPLACE), enabled(true) {
    name[0] = '\0';
    memset(animation_ids, 0xFF, sizeof(animation_ids));
    memset(property_mask, true, sizeof(property_mask));
  }
  
  void setMask(PropertyType prop, bool enabled_val) {
    property_mask[static_cast<uint8_t>(prop)] = enabled_val;
  }
  
  bool isMasked(PropertyType prop) const {
    return property_mask[static_cast<uint8_t>(prop)];
  }
};

// ============================================================
// Procedural Modifier
// ============================================================

enum class ModifierType : uint8_t {
  NONE        = 0x00,
  NOISE       = 0x01,   // Perlin/simplex noise
  SINE_WAVE   = 0x02,   // Sinusoidal oscillation
  SAW_WAVE    = 0x03,   // Sawtooth wave
  SQUARE_WAVE = 0x04,   // Square wave
  RANDOM      = 0x05,   // Random jitter
  SPRING      = 0x06,   // Spring physics
  BOUNCE      = 0x07,   // Bounce physics
  FOLLOW      = 0x08,   // Follow target
  CONSTRAINT  = 0x09,   // Constrain to range
  EXPRESSION  = 0x0A,   // Mathematical expression
};

struct ProceduralModifier {
  ModifierType  type;
  PropertyType  target_property;
  float         amplitude;
  float         frequency;
  float         phase;
  float         offset;
  float         min_value;
  float         max_value;
  bool          enabled;
  
  // For spring/bounce physics
  float         damping;
  float         stiffness;
  float         velocity;
  float         target;
  
  ProceduralModifier()
    : type(ModifierType::NONE), target_property(PropertyType::NONE),
      amplitude(1.0f), frequency(1.0f), phase(0.0f), offset(0.0f),
      min_value(-FLT_MAX), max_value(FLT_MAX), enabled(true),
      damping(0.5f), stiffness(0.5f), velocity(0.0f), target(0.0f) {}
  
  float apply(float input_value, float time_s, float dt) {
    if (!enabled) return input_value;
    
    float modifier = 0.0f;
    
    switch (type) {
      case ModifierType::NOISE:
        // Simple pseudo-noise based on time
        modifier = amplitude * sinf(time_s * frequency * 17.37f + phase) *
                   cosf(time_s * frequency * 31.41f);
        break;
        
      case ModifierType::SINE_WAVE:
        modifier = amplitude * sinf(time_s * frequency * 6.28318f + phase);
        break;
        
      case ModifierType::SAW_WAVE: {
        float t = fmodf(time_s * frequency + phase / 6.28318f, 1.0f);
        modifier = amplitude * (2.0f * t - 1.0f);
        break;
      }
        
      case ModifierType::SQUARE_WAVE: {
        float t = fmodf(time_s * frequency + phase / 6.28318f, 1.0f);
        modifier = amplitude * (t < 0.5f ? 1.0f : -1.0f);
        break;
      }
        
      case ModifierType::SPRING: {
        // Simple spring physics
        float diff = target - input_value;
        velocity += diff * stiffness;
        velocity *= (1.0f - damping);
        modifier = velocity * dt * 60.0f;  // Normalize to ~60fps
        break;
      }
        
      case ModifierType::CONSTRAINT:
        // Clamp to range
        return fmaxf(min_value, fminf(max_value, input_value + offset));
        
      default:
        break;
    }
    
    float result = input_value + modifier + offset;
    return fmaxf(min_value, fminf(max_value, result));
  }
};

// ============================================================
// Timeline Event
// ============================================================

enum class TimelineEventType : uint8_t {
  NONE            = 0x00,
  PLAY_ANIMATION  = 0x01,
  STOP_ANIMATION  = 0x02,
  SET_PROPERTY    = 0x03,
  TRIGGER_EVENT   = 0x04,
  START_TRANSITION= 0x05,
  CALL_FUNCTION   = 0x06,
  SET_LAYER_WEIGHT= 0x07,
};

struct TimelineEvent {
  TimelineEventType type;
  uint32_t          time_ms;
  uint8_t           target_id;    // Animation, layer, or custom ID
  float             value;        // Property value or weight
  PropertyType      property;     // For SET_PROPERTY
  bool              executed;
  
  TimelineEvent()
    : type(TimelineEventType::NONE), time_ms(0), target_id(0),
      value(0), property(PropertyType::NONE), executed(false) {}
};

// ============================================================
// Animation Timeline
// ============================================================

struct AnimationTimeline {
  uint32_t        duration_ms;
  bool            loop;
  uint32_t        current_time_ms;
  bool            playing;
  
  TimelineEvent   events[MAX_TIMELINE_EVENTS];
  uint8_t         event_count;
  
  AnimationTimeline()
    : duration_ms(0), loop(false), current_time_ms(0), playing(false), event_count(0) {}
  
  bool addEvent(const TimelineEvent& event) {
    if (event_count >= MAX_TIMELINE_EVENTS) return false;
    
    // Insert sorted by time
    uint8_t insert_pos = 0;
    while (insert_pos < event_count && events[insert_pos].time_ms < event.time_ms) {
      insert_pos++;
    }
    
    for (uint8_t i = event_count; i > insert_pos; i--) {
      events[i] = events[i - 1];
    }
    
    events[insert_pos] = event;
    event_count++;
    
    if (event.time_ms > duration_ms) {
      duration_ms = event.time_ms;
    }
    
    return true;
  }
  
  void reset() {
    current_time_ms = 0;
    for (uint8_t i = 0; i < event_count; i++) {
      events[i].executed = false;
    }
  }
};

// ============================================================
// Animation System (Main Controller)
// ============================================================

class AnimationSystem {
public:
  AnimationSystem() : system_time_ms_(0) {
    reset();
  }
  
  void reset() {
    system_time_ms_ = 0;
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      instances_[i].reset();
      definitions_[i] = AnimationDef();
    }
    for (size_t i = 0; i < MAX_LAYERS; i++) {
      layers_[i] = AnimationLayer();
      layers_[i].id = i;
    }
    for (size_t i = 0; i < MAX_TRANSITIONS; i++) {
      transitions_[i] = AnimationTransition();
    }
    for (size_t i = 0; i < MAX_MODIFIERS; i++) {
      modifiers_[i] = ProceduralModifier();
    }
    timeline_ = AnimationTimeline();
    
    active_count_ = 0;
    layer_count_ = 1;  // Default layer 0
    modifier_count_ = 0;
  }
  
  // ===== Animation Definition =====
  
  AnimationDef* createAnimation(uint8_t id, const char* name, uint32_t duration_ms) {
    if (id >= MAX_ANIMATIONS) return nullptr;
    
    AnimationDef& def = definitions_[id];
    def.id = id;
    strncpy(def.name, name, sizeof(def.name) - 1);
    def.duration_ms = duration_ms;
    def.loop_mode = LoopMode::NONE;
    def.speed = 1.0f;
    def.track_count = 0;
    
    instances_[id].definition = &def;
    instances_[id].reset();
    
    return &def;
  }
  
  AnimationDef* getAnimation(uint8_t id) {
    if (id >= MAX_ANIMATIONS) return nullptr;
    return &definitions_[id];
  }
  
  // ===== Playback Control =====
  
  bool play(uint8_t id, float speed = 1.0f) {
    if (id >= MAX_ANIMATIONS) return false;
    if (!instances_[id].definition) return false;
    
    AnimationInstance& inst = instances_[id];
    inst.state = AnimationState::PLAYING;
    inst.start_time_ms = system_time_ms_;
    inst.current_speed = speed * inst.definition->speed;
    inst.reverse = (speed < 0);
    inst.current_loop = 0;
    
    if (inst.state != AnimationState::PAUSED) {
      inst.current_time_ms = inst.reverse ? inst.definition->duration_ms : 0;
    }
    
    active_count_++;
    return true;
  }
  
  bool stop(uint8_t id) {
    if (id >= MAX_ANIMATIONS) return false;
    
    AnimationInstance& inst = instances_[id];
    if (inst.state != AnimationState::IDLE) {
      inst.state = AnimationState::STOPPED;
      active_count_--;
    }
    return true;
  }
  
  bool pause(uint8_t id) {
    if (id >= MAX_ANIMATIONS) return false;
    
    AnimationInstance& inst = instances_[id];
    if (inst.state == AnimationState::PLAYING) {
      inst.state = AnimationState::PAUSED;
    }
    return true;
  }
  
  bool resume(uint8_t id) {
    if (id >= MAX_ANIMATIONS) return false;
    
    AnimationInstance& inst = instances_[id];
    if (inst.state == AnimationState::PAUSED) {
      inst.state = AnimationState::PLAYING;
    }
    return true;
  }
  
  bool seek(uint8_t id, uint32_t time_ms) {
    if (id >= MAX_ANIMATIONS) return false;
    
    instances_[id].current_time_ms = time_ms;
    updateInstance(instances_[id]);
    return true;
  }
  
  // ===== Transitions =====
  
  bool startTransition(uint8_t from_id, uint8_t to_id, uint32_t duration_ms,
                       EasingType easing = EasingType::EASE_IN_OUT) {
    // Find free transition slot
    AnimationTransition* trans = nullptr;
    for (size_t i = 0; i < MAX_TRANSITIONS; i++) {
      if (!transitions_[i].active) {
        trans = &transitions_[i];
        break;
      }
    }
    if (!trans) return false;
    
    trans->from_animation = from_id;
    trans->to_animation = to_id;
    trans->duration_ms = duration_ms;
    trans->easing = easing;
    trans->blend_factor = 0.0f;
    trans->active = true;
    
    // Start the target animation
    play(to_id);
    
    return true;
  }
  
  // ===== Layers =====
  
  AnimationLayer* createLayer(uint8_t id, const char* name) {
    if (id >= MAX_LAYERS) return nullptr;
    
    AnimationLayer& layer = layers_[id];
    layer.id = id;
    strncpy(layer.name, name, sizeof(layer.name) - 1);
    layer.enabled = true;
    layer.weight = 1.0f;
    
    if (id >= layer_count_) layer_count_ = id + 1;
    
    return &layer;
  }
  
  bool addToLayer(uint8_t layer_id, uint8_t anim_id) {
    if (layer_id >= MAX_LAYERS) return false;
    
    AnimationLayer& layer = layers_[layer_id];
    if (layer.animation_count >= 8) return false;
    
    layer.animation_ids[layer.animation_count++] = anim_id;
    return true;
  }
  
  void setLayerWeight(uint8_t layer_id, float weight) {
    if (layer_id < MAX_LAYERS) {
      layers_[layer_id].weight = fmaxf(0.0f, fminf(1.0f, weight));
    }
  }
  
  // ===== Hierarchical Animation =====
  
  bool setParent(uint8_t child_id, uint8_t parent_id) {
    if (child_id >= MAX_ANIMATIONS || parent_id >= MAX_ANIMATIONS) return false;
    if (child_id == parent_id) return false;
    
    // Check for cycles
    uint8_t current = parent_id;
    while (current != 0xFF) {
      if (current == child_id) return false;  // Cycle detected
      current = instances_[current].parent_id;
    }
    
    // Remove from old parent
    uint8_t old_parent = instances_[child_id].parent_id;
    if (old_parent != 0xFF) {
      AnimationInstance& old_parent_inst = instances_[old_parent];
      for (uint8_t i = 0; i < old_parent_inst.child_count; i++) {
        if (old_parent_inst.child_ids[i] == child_id) {
          // Shift remaining children
          for (uint8_t j = i; j < old_parent_inst.child_count - 1; j++) {
            old_parent_inst.child_ids[j] = old_parent_inst.child_ids[j + 1];
          }
          old_parent_inst.child_count--;
          break;
        }
      }
    }
    
    // Add to new parent
    instances_[child_id].parent_id = parent_id;
    if (parent_id != 0xFF) {
      AnimationInstance& parent_inst = instances_[parent_id];
      if (parent_inst.child_count < MAX_CHILDREN) {
        parent_inst.child_ids[parent_inst.child_count++] = child_id;
      }
    }
    
    return true;
  }
  
  // ===== Modifiers =====
  
  ProceduralModifier* addModifier(uint8_t anim_id, PropertyType prop, ModifierType type) {
    if (modifier_count_ >= MAX_MODIFIERS) return nullptr;
    
    ProceduralModifier& mod = modifiers_[modifier_count_++];
    mod.type = type;
    mod.target_property = prop;
    mod.enabled = true;
    
    return &mod;
  }
  
  // ===== Update =====
  
  void update(uint32_t delta_ms) {
    system_time_ms_ += delta_ms;
    float delta_s = delta_ms / 1000.0f;
    
    // Update timeline
    if (timeline_.playing) {
      updateTimeline(delta_ms);
    }
    
    // Update transitions
    updateTransitions(delta_ms);
    
    // Update all playing animations
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      AnimationInstance& inst = instances_[i];
      if (inst.state == AnimationState::PLAYING) {
        updateInstance(inst, delta_ms);
      }
    }
    
    // Update hierarchical transforms (top-down)
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      if (instances_[i].parent_id == 0xFF) {
        updateHierarchy(i);
      }
    }
    
    // Apply modifiers
    for (size_t i = 0; i < modifier_count_; i++) {
      applyModifier(modifiers_[i], delta_s);
    }
    
    // Compose layers
    composeLayers();
  }
  
  // ===== Query =====
  
  float getPropertyValue(uint8_t anim_id, PropertyType prop) const {
    if (anim_id >= MAX_ANIMATIONS) return 0.0f;
    return instances_[anim_id].getProperty(prop);
  }
  
  AnimationState getState(uint8_t anim_id) const {
    if (anim_id >= MAX_ANIMATIONS) return AnimationState::IDLE;
    return instances_[anim_id].state;
  }
  
  uint32_t getCurrentTime(uint8_t anim_id) const {
    if (anim_id >= MAX_ANIMATIONS) return 0;
    return instances_[anim_id].current_time_ms;
  }
  
  const AnimationInstance* getInstance(uint8_t id) const {
    if (id >= MAX_ANIMATIONS) return nullptr;
    return &instances_[id];
  }
  
  // Get composed property value from all layers
  float getComposedProperty(PropertyType prop) const {
    return composed_values_[static_cast<uint8_t>(prop)];
  }

private:
  AnimationDef        definitions_[MAX_ANIMATIONS];
  AnimationInstance   instances_[MAX_ANIMATIONS];
  AnimationLayer      layers_[MAX_LAYERS];
  AnimationTransition transitions_[MAX_TRANSITIONS];
  ProceduralModifier  modifiers_[MAX_MODIFIERS];
  AnimationTimeline   timeline_;
  
  uint32_t            system_time_ms_;
  uint8_t             active_count_;
  uint8_t             layer_count_;
  uint8_t             modifier_count_;
  
  // Composed output values
  float               composed_values_[32];
  
  void updateInstance(AnimationInstance& inst, uint32_t delta_ms = 0) {
    if (!inst.definition) return;
    
    const AnimationDef& def = *inst.definition;
    
    // Advance time
    if (delta_ms > 0 && inst.state == AnimationState::PLAYING) {
      float advance = delta_ms * fabsf(inst.current_speed);
      
      if (inst.reverse) {
        inst.current_time_ms -= (uint32_t)advance;
        if ((int32_t)inst.current_time_ms < 0) {
          handleLoopEnd(inst, true);
        }
      } else {
        inst.current_time_ms += (uint32_t)advance;
        if (inst.current_time_ms >= def.duration_ms) {
          handleLoopEnd(inst, false);
        }
      }
    }
    
    // Evaluate all property tracks
    for (uint8_t i = 0; i < def.track_count; i++) {
      const PropertyTrack& track = def.tracks[i];
      float value = track.getValue(inst.current_time_ms);
      inst.setProperty(track.property, value);
    }
  }
  
  void handleLoopEnd(AnimationInstance& inst, bool at_start) {
    const AnimationDef& def = *inst.definition;
    
    switch (def.loop_mode) {
      case LoopMode::NONE:
        inst.state = AnimationState::FINISHED;
        inst.current_time_ms = at_start ? 0 : def.duration_ms;
        active_count_--;
        break;
        
      case LoopMode::LOOP:
        inst.current_loop++;
        if (def.loop_count > 0 && inst.current_loop >= def.loop_count) {
          inst.state = AnimationState::FINISHED;
          active_count_--;
        } else {
          inst.current_time_ms = at_start ? def.duration_ms : 0;
        }
        break;
        
      case LoopMode::PING_PONG:
        inst.reverse = !inst.reverse;
        inst.current_loop++;
        if (def.loop_count > 0 && inst.current_loop >= def.loop_count * 2) {
          inst.state = AnimationState::FINISHED;
          active_count_--;
        }
        break;
        
      case LoopMode::REVERSE:
        inst.state = AnimationState::FINISHED;
        active_count_--;
        break;
        
      default:
        break;
    }
  }
  
  void updateTransitions(uint32_t delta_ms) {
    for (size_t i = 0; i < MAX_TRANSITIONS; i++) {
      AnimationTransition& trans = transitions_[i];
      if (!trans.active) continue;
      
      float progress = (float)delta_ms / trans.duration_ms;
      trans.blend_factor += progress;
      
      if (trans.blend_factor >= 1.0f) {
        // Transition complete
        trans.blend_factor = 1.0f;
        stop(trans.from_animation);
        trans.active = false;
      } else {
        // Blend animations
        float t = evaluateEasing(trans.easing, trans.blend_factor);
        blendAnimations(trans.from_animation, trans.to_animation, t);
      }
    }
  }
  
  void blendAnimations(uint8_t from_id, uint8_t to_id, float t) {
    if (from_id >= MAX_ANIMATIONS || to_id >= MAX_ANIMATIONS) return;
    
    AnimationInstance& from_inst = instances_[from_id];
    AnimationInstance& to_inst = instances_[to_id];
    
    // Blend all properties
    for (uint8_t prop = 0; prop < 32; prop++) {
      float from_val = from_inst.property_values[prop];
      float to_val = to_inst.property_values[prop];
      to_inst.property_values[prop] = from_val + (to_val - from_val) * t;
    }
  }
  
  void updateHierarchy(uint8_t id) {
    AnimationInstance& inst = instances_[id];
    
    // Get parent transforms
    if (inst.parent_id != 0xFF) {
      const AnimationInstance& parent = instances_[inst.parent_id];
      inst.inherited_x = parent.inherited_x + parent.getProperty(PropertyType::POSITION_X);
      inst.inherited_y = parent.inherited_y + parent.getProperty(PropertyType::POSITION_Y);
      inst.inherited_rotation = parent.inherited_rotation + parent.getProperty(PropertyType::ROTATION);
      inst.inherited_scale = parent.inherited_scale * parent.getProperty(PropertyType::SCALE_UNIFORM);
      inst.inherited_opacity = parent.inherited_opacity * parent.getProperty(PropertyType::OPACITY);
    } else {
      inst.inherited_x = 0;
      inst.inherited_y = 0;
      inst.inherited_rotation = 0;
      inst.inherited_scale = 1.0f;
      inst.inherited_opacity = 1.0f;
    }
    
    // Update children
    for (uint8_t i = 0; i < inst.child_count; i++) {
      updateHierarchy(inst.child_ids[i]);
    }
  }
  
  void applyModifier(ProceduralModifier& mod, float delta_s) {
    if (!mod.enabled) return;
    
    float time_s = system_time_ms_ / 1000.0f;
    
    // Find animations using this property and apply modifier
    for (size_t i = 0; i < MAX_ANIMATIONS; i++) {
      AnimationInstance& inst = instances_[i];
      if (inst.state != AnimationState::PLAYING) continue;
      
      uint8_t prop_idx = static_cast<uint8_t>(mod.target_property);
      float value = inst.property_values[prop_idx];
      inst.property_values[prop_idx] = mod.apply(value, time_s, delta_s);
    }
  }
  
  void composeLayers() {
    // Reset composed values
    memset(composed_values_, 0, sizeof(composed_values_));
    
    // Compose from bottom layer to top
    for (uint8_t layer_idx = 0; layer_idx < layer_count_; layer_idx++) {
      const AnimationLayer& layer = layers_[layer_idx];
      if (!layer.enabled || layer.weight <= 0) continue;
      
      // Gather values from all animations in this layer
      float layer_values[32];
      bool layer_has_value[32];
      memset(layer_has_value, 0, sizeof(layer_has_value));
      
      for (uint8_t i = 0; i < layer.animation_count; i++) {
        uint8_t anim_id = layer.animation_ids[i];
        if (anim_id >= MAX_ANIMATIONS) continue;
        
        const AnimationInstance& inst = instances_[anim_id];
        if (inst.state != AnimationState::PLAYING && inst.state != AnimationState::PAUSED) continue;
        
        for (uint8_t prop = 0; prop < 32; prop++) {
          if (inst.property_dirty[prop] && layer.property_mask[prop]) {
            if (!layer_has_value[prop]) {
              layer_values[prop] = inst.property_values[prop];
              layer_has_value[prop] = true;
            } else {
              // Average multiple animations in same layer
              layer_values[prop] = (layer_values[prop] + inst.property_values[prop]) * 0.5f;
            }
          }
        }
      }
      
      // Blend with composed values based on blend mode
      for (uint8_t prop = 0; prop < 32; prop++) {
        if (!layer_has_value[prop]) continue;
        
        float src = composed_values_[prop];
        float dst = layer_values[prop] * layer.weight;
        
        switch (layer.blend_mode) {
          case LayerBlendMode::REPLACE:
            composed_values_[prop] = dst;
            break;
            
          case LayerBlendMode::ADDITIVE:
            composed_values_[prop] = src + dst;
            break;
            
          case LayerBlendMode::MULTIPLY:
            composed_values_[prop] = src * dst;
            break;
            
          case LayerBlendMode::OVERRIDE:
            composed_values_[prop] = dst;
            break;
            
          case LayerBlendMode::AVERAGE:
            composed_values_[prop] = (src + dst) * 0.5f;
            break;
        }
      }
    }
  }
  
  void updateTimeline(uint32_t delta_ms) {
    uint32_t old_time = timeline_.current_time_ms;
    timeline_.current_time_ms += delta_ms;
    
    // Execute events in range
    for (uint8_t i = 0; i < timeline_.event_count; i++) {
      TimelineEvent& event = timeline_.events[i];
      if (event.executed) continue;
      if (event.time_ms > old_time && event.time_ms <= timeline_.current_time_ms) {
        executeTimelineEvent(event);
      }
    }
    
    // Handle loop
    if (timeline_.current_time_ms >= timeline_.duration_ms) {
      if (timeline_.loop) {
        timeline_.current_time_ms = 0;
        timeline_.reset();
      } else {
        timeline_.playing = false;
      }
    }
  }
  
  void executeTimelineEvent(TimelineEvent& event) {
    switch (event.type) {
      case TimelineEventType::PLAY_ANIMATION:
        play(event.target_id);
        break;
        
      case TimelineEventType::STOP_ANIMATION:
        stop(event.target_id);
        break;
        
      case TimelineEventType::SET_PROPERTY:
        if (event.target_id < MAX_ANIMATIONS) {
          instances_[event.target_id].setProperty(event.property, event.value);
        }
        break;
        
      case TimelineEventType::START_TRANSITION:
        // target_id encodes both animations (high nibble = from, low nibble = to)
        startTransition(event.target_id >> 4, event.target_id & 0x0F, 
                        (uint32_t)event.value);
        break;
        
      case TimelineEventType::SET_LAYER_WEIGHT:
        setLayerWeight(event.target_id, event.value);
        break;
        
      default:
        break;
    }
    
    event.executed = true;
  }
};

} // namespace animation
} // namespace gpu

#endif // GPU_ANIMATION_SYSTEM_HPP_
