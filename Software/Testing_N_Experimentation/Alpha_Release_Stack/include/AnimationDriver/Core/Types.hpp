/*****************************************************************
 * @file Types.hpp
 * @brief Core types for the Animation Driver system
 * 
 * Defines fundamental types, enums, and structures used throughout
 * the animation system.
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cmath>
#include <functional>

namespace AnimationDriver {

// ============================================================
// Forward Declarations
// ============================================================
class Animation;
class Shader;
class Timeline;

// ============================================================
// Basic Types
// ============================================================

using AnimationId = uint16_t;
using ShaderId = uint8_t;
using LayerId = uint8_t;

constexpr AnimationId INVALID_ANIMATION = 0xFFFF;
constexpr ShaderId INVALID_SHADER = 0xFF;
constexpr LayerId INVALID_LAYER = 0xFF;

// ============================================================
// Display Target
// ============================================================

enum class DisplayTarget : uint8_t {
    HUB75 = 0,      // 128x32 RGB LED matrix
    OLED = 1,       // 128x128 monochrome OLED
    BOTH = 2        // Both displays
};

// ============================================================
// Animation State
// ============================================================

enum class AnimationState : uint8_t {
    IDLE,           // Not running
    PLAYING,        // Currently playing
    PAUSED,         // Paused mid-animation
    FINISHED        // Completed (for non-looping)
};

// ============================================================
// Loop Mode
// ============================================================

enum class LoopMode : uint8_t {
    ONCE,           // Play once and stop
    LOOP,           // Loop forever
    PING_PONG,      // Forward then backward
    LOOP_COUNT      // Loop N times
};

// ============================================================
// Blend Mode
// ============================================================

enum class BlendMode : uint8_t {
    REPLACE,        // Overwrite previous
    ADD,            // Additive blending
    MULTIPLY,       // Multiplicative
    SCREEN,         // Screen blend
    OVERLAY,        // Overlay blend
    ALPHA           // Alpha compositing
};

// ============================================================
// Coordinate System
// ============================================================

enum class CoordSpace : uint8_t {
    PIXEL,          // Absolute pixel coordinates
    NORMALIZED,     // 0.0 - 1.0 range
    CENTERED        // -1.0 to 1.0, centered at display center
};

// ============================================================
// Value Range
// ============================================================

struct ValueRange {
    float min = 0.0f;
    float max = 1.0f;
    
    constexpr ValueRange() = default;
    constexpr ValueRange(float min_, float max_) : min(min_), max(max_) {}
    
    float clamp(float v) const {
        return v < min ? min : (v > max ? max : v);
    }
    
    float normalize(float v) const {
        if (max == min) return 0.0f;
        return (clamp(v) - min) / (max - min);
    }
    
    float denormalize(float t) const {
        return min + t * (max - min);
    }
    
    float map(float v, const ValueRange& to) const {
        return to.denormalize(normalize(v));
    }
};

// ============================================================
// 2D Vector
// ============================================================

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    
    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    
    float length() const { return sqrtf(x*x + y*y); }
    float lengthSq() const { return x*x + y*y; }
    
    Vec2 normalized() const {
        float len = length();
        return len > 0.0001f ? Vec2(x/len, y/len) : Vec2(0, 0);
    }
    
    float dot(const Vec2& o) const { return x*o.x + y*o.y; }
    
    static Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    }
};

// ============================================================
// 3D Vector (for IMU data)
// ============================================================

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    
    float length() const { return sqrtf(x*x + y*y + z*z); }
    
    Vec3 normalized() const {
        float len = length();
        return len > 0.0001f ? Vec3(x/len, y/len, z/len) : Vec3(0, 0, 0);
    }
    
    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    
    Vec3 cross(const Vec3& o) const {
        return {
            y*o.z - z*o.y,
            z*o.x - x*o.z,
            x*o.y - y*o.x
        };
    }
    
    // Rotate by Euler angles (in radians)
    Vec3 rotateX(float angle) const;
    Vec3 rotateY(float angle) const;
    Vec3 rotateZ(float angle) const;
};

// ============================================================
// Parameter Value (union for flexibility)
// ============================================================

struct ParamValue {
    enum class Type : uint8_t {
        FLOAT,
        INT,
        VEC2,
        VEC3,
        COLOR
    };
    
    Type type = Type::FLOAT;
    union {
        float f;
        int32_t i;
        struct { float x, y; } v2;
        struct { float x, y, z; } v3;
        struct { uint8_t r, g, b, a; } color;
    } data = {0.0f};
    
    ParamValue() = default;
    ParamValue(float v) : type(Type::FLOAT) { data.f = v; }
    ParamValue(int32_t v) : type(Type::INT) { data.i = v; }
    ParamValue(Vec2 v) : type(Type::VEC2) { data.v2 = {v.x, v.y}; }
    ParamValue(Vec3 v) : type(Type::VEC3) { data.v3 = {v.x, v.y, v.z}; }
    
    float asFloat() const { return type == Type::FLOAT ? data.f : (float)data.i; }
    int32_t asInt() const { return type == Type::INT ? data.i : (int32_t)data.f; }
    Vec2 asVec2() const { return type == Type::VEC2 ? Vec2(data.v2.x, data.v2.y) : Vec2(); }
    Vec3 asVec3() const { return type == Type::VEC3 ? Vec3(data.v3.x, data.v3.y, data.v3.z) : Vec3(); }
};

// ============================================================
// Callback Types
// ============================================================

using UpdateCallback = std::function<void(float deltaTime)>;
using ValueProvider = std::function<float()>;
using Vec2Provider = std::function<Vec2()>;
using Vec3Provider = std::function<Vec3()>;
using TriggerCallback = std::function<void()>;

} // namespace AnimationDriver
