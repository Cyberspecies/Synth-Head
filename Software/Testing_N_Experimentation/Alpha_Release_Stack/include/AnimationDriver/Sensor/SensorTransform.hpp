/*****************************************************************
 * @file SensorTransform.hpp
 * @brief Transform sensor values for mounting and coordinate systems
 * 
 * Handles conversion from raw sensor-local coordinates to
 * device-local or world coordinates based on mounting configuration.
 *****************************************************************/

#pragma once

#include "SensorTypes.hpp"
#include <cmath>

namespace AnimationDriver {

// ============================================================
// 3x3 Rotation Matrix
// ============================================================

struct Matrix3x3 {
    float m[3][3];
    
    Matrix3x3() {
        identity();
    }
    
    void identity() {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }
    
    // Create rotation matrix from Euler angles (degrees)
    static Matrix3x3 fromEuler(float pitch, float roll, float yaw) {
        Matrix3x3 result;
        
        float cp = cosf(pitch * 3.14159f / 180.0f);
        float sp = sinf(pitch * 3.14159f / 180.0f);
        float cr = cosf(roll * 3.14159f / 180.0f);
        float sr = sinf(roll * 3.14159f / 180.0f);
        float cy = cosf(yaw * 3.14159f / 180.0f);
        float sy = sinf(yaw * 3.14159f / 180.0f);
        
        // Combined rotation matrix (ZYX order)
        result.m[0][0] = cy * cr;
        result.m[0][1] = cy * sr * sp - sy * cp;
        result.m[0][2] = cy * sr * cp + sy * sp;
        result.m[1][0] = sy * cr;
        result.m[1][1] = sy * sr * sp + cy * cp;
        result.m[1][2] = sy * sr * cp - cy * sp;
        result.m[2][0] = -sr;
        result.m[2][1] = cr * sp;
        result.m[2][2] = cr * cp;
        
        return result;
    }
    
    // Transform a 3D vector
    Vec3 transform(const Vec3& v) const {
        return Vec3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }
    
    // Multiply matrices
    Matrix3x3 operator*(const Matrix3x3& other) const {
        Matrix3x3 result;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                result.m[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }
    
    // Transpose (for inverse of rotation matrix)
    Matrix3x3 transpose() const {
        Matrix3x3 result;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                result.m[i][j] = m[j][i];
            }
        }
        return result;
    }
};

// ============================================================
// Transform Configuration
// ============================================================

struct TransformConfig {
    // Mounting rotation (degrees)
    float mountPitch = 0.0f;    // Forward/backward tilt
    float mountRoll = 0.0f;     // Left/right tilt
    float mountYaw = 0.0f;      // Rotation around up axis
    
    // Calibration offsets (subtracted from raw values)
    Vec3 offset = Vec3(0, 0, 0);
    
    // Scale factors (multiplied after offset)
    Vec3 scale = Vec3(1, 1, 1);
    
    // Axis mapping/flipping
    bool flipX = false;
    bool flipY = false;
    bool flipZ = false;
    
    // Axis swapping (0=X, 1=Y, 2=Z)
    int mapX = 0;
    int mapY = 1;
    int mapZ = 2;
    
    // Presets for common mountings
    static TransformConfig Default() {
        return TransformConfig();
    }
    
    static TransformConfig TiltedForward(float degrees) {
        TransformConfig cfg;
        cfg.mountPitch = degrees;
        return cfg;
    }
    
    static TransformConfig Vertical() {
        TransformConfig cfg;
        cfg.mountPitch = 90.0f;
        return cfg;
    }
    
    static TransformConfig UpsideDown() {
        TransformConfig cfg;
        cfg.mountRoll = 180.0f;
        return cfg;
    }
};

// ============================================================
// Sensor Transform
// ============================================================

class SensorTransform {
public:
    SensorTransform() {
        updateMatrix();
    }
    
    // Set transform configuration
    SensorTransform& setConfig(const TransformConfig& config) {
        config_ = config;
        updateMatrix();
        return *this;
    }
    
    // Set mounting angles (degrees)
    SensorTransform& setMounting(float pitch, float roll = 0.0f, float yaw = 0.0f) {
        config_.mountPitch = pitch;
        config_.mountRoll = roll;
        config_.mountYaw = yaw;
        updateMatrix();
        return *this;
    }
    
    // Set calibration offset
    SensorTransform& setOffset(float x, float y, float z) {
        config_.offset = Vec3(x, y, z);
        return *this;
    }
    
    SensorTransform& setOffset(const Vec3& offset) {
        config_.offset = offset;
        return *this;
    }
    
    // Set scale factors
    SensorTransform& setScale(float x, float y, float z) {
        config_.scale = Vec3(x, y, z);
        return *this;
    }
    
    SensorTransform& setScale(float uniform) {
        config_.scale = Vec3(uniform, uniform, uniform);
        return *this;
    }
    
    // Set axis flipping
    SensorTransform& setFlip(bool x, bool y, bool z) {
        config_.flipX = x;
        config_.flipY = y;
        config_.flipZ = z;
        return *this;
    }
    
    // Set axis remapping
    SensorTransform& setAxisMap(int xFrom, int yFrom, int zFrom) {
        config_.mapX = xFrom;
        config_.mapY = yFrom;
        config_.mapZ = zFrom;
        return *this;
    }
    
    // Transform a value
    SensorValue transform(const SensorValue& input) const {
        switch (input.type) {
            case SensorValueType::SCALAR:
                return transformScalar(input);
            case SensorValueType::VEC2:
                return transformVec2(input);
            case SensorValueType::VEC3:
                return transformVec3(input);
            case SensorValueType::VEC4:
                return transformVec4(input);
            default:
                return input;
        }
    }
    
    // Transform a Vec3 directly
    Vec3 transformVec3Direct(const Vec3& v) const {
        // Apply offset
        Vec3 result(
            v.x - config_.offset.x,
            v.y - config_.offset.y,
            v.z - config_.offset.z
        );
        
        // Apply axis remapping
        float components[3] = {result.x, result.y, result.z};
        result.x = components[config_.mapX];
        result.y = components[config_.mapY];
        result.z = components[config_.mapZ];
        
        // Apply flipping
        if (config_.flipX) result.x = -result.x;
        if (config_.flipY) result.y = -result.y;
        if (config_.flipZ) result.z = -result.z;
        
        // Apply scale
        result.x *= config_.scale.x;
        result.y *= config_.scale.y;
        result.z *= config_.scale.z;
        
        // Apply rotation matrix
        return rotationMatrix_.transform(result);
    }
    
    // Get the rotation matrix
    const Matrix3x3& getRotationMatrix() const { return rotationMatrix_; }
    
    // Get config
    const TransformConfig& getConfig() const { return config_; }
    
private:
    void updateMatrix() {
        rotationMatrix_ = Matrix3x3::fromEuler(
            config_.mountPitch,
            config_.mountRoll,
            config_.mountYaw
        );
    }
    
    SensorValue transformScalar(const SensorValue& input) const {
        // Scalars just get offset and scale applied
        return SensorValue((input.scalar - config_.offset.x) * config_.scale.x);
    }
    
    SensorValue transformVec2(const SensorValue& input) const {
        float x = (input.vec2.x - config_.offset.x) * config_.scale.x;
        float y = (input.vec2.y - config_.offset.y) * config_.scale.y;
        if (config_.flipX) x = -x;
        if (config_.flipY) y = -y;
        return SensorValue(x, y);
    }
    
    SensorValue transformVec3(const SensorValue& input) const {
        Vec3 v = transformVec3Direct(Vec3(input.vec3.x, input.vec3.y, input.vec3.z));
        return SensorValue(v.x, v.y, v.z);
    }
    
    SensorValue transformVec4(const SensorValue& input) const {
        // For quaternions, we'd need quaternion multiplication
        // For now, just pass through
        return input;
    }
    
    TransformConfig config_;
    Matrix3x3 rotationMatrix_;
};

} // namespace AnimationDriver
