/*****************************************************************
 * @file SensorTypes.hpp
 * @brief Core types for the generalized sensor system
 * 
 * Supports any sensor type: IMU, humidity, GPS, temperature,
 * light, audio, proximity, etc.
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include <cstdint>
#include <functional>

namespace AnimationDriver {

// ============================================================
// Sensor Category
// ============================================================

enum class SensorCategory : uint8_t {
    MOTION,         // Accelerometer, gyroscope, magnetometer
    POSITION,       // GPS, indoor positioning
    ENVIRONMENTAL,  // Temperature, humidity, pressure, air quality
    LIGHT,          // Ambient light, UV, color sensors
    PROXIMITY,      // Distance, presence, touch
    AUDIO,          // Microphone, sound level
    BIOMETRIC,      // Heart rate, etc.
    CUSTOM          // User-defined sensors
};

// ============================================================
// Sensor Value Type
// ============================================================

enum class SensorValueType : uint8_t {
    SCALAR,         // Single float value (temperature, humidity)
    VEC2,           // 2D value (GPS lat/lon, joystick)
    VEC3,           // 3D value (accelerometer, gyroscope)
    VEC4,           // 4D value (quaternion orientation)
    BOOLEAN,        // On/off state (button, presence)
    INTEGER,        // Discrete value
    RAW_BUFFER      // Raw byte array
};

// ============================================================
// Coordinate Space
// ============================================================

enum class CoordinateSpace : uint8_t {
    SENSOR_LOCAL,   // Raw sensor coordinates
    DEVICE_LOCAL,   // Transformed for device mounting
    WORLD,          // Global/world coordinates
    NORMALIZED      // Normalized 0-1 or -1 to 1
};

// ============================================================
// Sensor Value Union
// ============================================================

struct SensorValue {
    SensorValueType type;
    
    union {
        float scalar;
        bool boolean;
        int32_t integer;
        struct { float x, y; } vec2;
        struct { float x, y, z; } vec3;
        struct { float x, y, z, w; } vec4;
    };
    
    // Constructors
    SensorValue() : type(SensorValueType::SCALAR), scalar(0.0f) {}
    
    explicit SensorValue(float v) : type(SensorValueType::SCALAR), scalar(v) {}
    explicit SensorValue(bool v) : type(SensorValueType::BOOLEAN), boolean(v) {}
    explicit SensorValue(int32_t v) : type(SensorValueType::INTEGER), integer(v) {}
    
    SensorValue(float x, float y) : type(SensorValueType::VEC2) {
        vec2.x = x; vec2.y = y;
    }
    
    SensorValue(float x, float y, float z) : type(SensorValueType::VEC3) {
        vec3.x = x; vec3.y = y; vec3.z = z;
    }
    
    SensorValue(float x, float y, float z, float w) : type(SensorValueType::VEC4) {
        vec4.x = x; vec4.y = y; vec4.z = z; vec4.w = w;
    }
    
    // Get as specific types
    float asScalar() const {
        switch (type) {
            case SensorValueType::SCALAR: return scalar;
            case SensorValueType::BOOLEAN: return boolean ? 1.0f : 0.0f;
            case SensorValueType::INTEGER: return static_cast<float>(integer);
            case SensorValueType::VEC2: return vec2.x;
            case SensorValueType::VEC3: return vec3.x;
            default: return 0.0f;
        }
    }
    
    Vec2 asVec2() const {
        switch (type) {
            case SensorValueType::VEC2: return Vec2(vec2.x, vec2.y);
            case SensorValueType::VEC3: return Vec2(vec3.x, vec3.y);
            case SensorValueType::SCALAR: return Vec2(scalar, 0.0f);
            default: return Vec2(0, 0);
        }
    }
    
    Vec3 asVec3() const {
        switch (type) {
            case SensorValueType::VEC3: return Vec3(vec3.x, vec3.y, vec3.z);
            case SensorValueType::VEC2: return Vec3(vec2.x, vec2.y, 0.0f);
            case SensorValueType::SCALAR: return Vec3(scalar, 0.0f, 0.0f);
            default: return Vec3(0, 0, 0);
        }
    }
    
    // Magnitude (for vectors)
    float magnitude() const {
        switch (type) {
            case SensorValueType::VEC2:
                return sqrtf(vec2.x * vec2.x + vec2.y * vec2.y);
            case SensorValueType::VEC3:
                return sqrtf(vec3.x * vec3.x + vec3.y * vec3.y + vec3.z * vec3.z);
            case SensorValueType::VEC4:
                return sqrtf(vec4.x * vec4.x + vec4.y * vec4.y + 
                            vec4.z * vec4.z + vec4.w * vec4.w);
            default:
                return fabsf(asScalar());
        }
    }
};

// ============================================================
// Sensor Metadata
// ============================================================

struct SensorInfo {
    char name[24];              // Sensor name/ID
    SensorCategory category;    // Sensor category
    SensorValueType valueType;  // Output value type
    
    // Value range info
    float minValue;             // Minimum expected value
    float maxValue;             // Maximum expected value
    float resolution;           // Sensor resolution
    
    // Update info
    float updateRateHz;         // Expected update rate
    uint32_t lastUpdateMs;      // Last update timestamp
    
    SensorInfo() : category(SensorCategory::CUSTOM), 
                   valueType(SensorValueType::SCALAR),
                   minValue(0.0f), maxValue(1.0f),
                   resolution(0.01f), updateRateHz(60.0f),
                   lastUpdateMs(0) {
        name[0] = '\0';
    }
};

// ============================================================
// Callback Types
// ============================================================

using SensorUpdateCallback = std::function<void(const SensorValue&)>;
using GestureCallback = std::function<void(const char* gestureName, float intensity)>;

} // namespace AnimationDriver
