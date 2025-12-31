/*****************************************************************
 * File:      AnimationParameterSystem.hpp
 * Category:  Manager
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Granular parameter control system for display effects and shaders.
 *    Allows static values or dynamic sensor-driven modifiers for each
 *    parameter (position, rotation, color, speed, etc.)
 *    
 * Example Use Cases:
 *    - Left sprite Y offset controlled by gyro_y (normal)
 *    - Right sprite Y offset controlled by gyro_y (inverted)
 *    - Both sprites X offset controlled by gyro_z
 *    - Shader hue cycle speed controlled by temperature
 *    - Shader brightness controlled by accelerometer magnitude
 *****************************************************************/

#ifndef ANIMATION_PARAMETER_SYSTEM_HPP
#define ANIMATION_PARAMETER_SYSTEM_HPP

#include <cstdint>
#include <cmath>
#include "Drivers/UART Comms/UartBidirectionalProtocol.h"

namespace arcos::manager {

/**
 * @brief Sensor data sources that can control parameters
 */
enum class SensorSource : uint8_t {
  NONE = 0,           // Static value, no sensor input
  
  // IMU - Accelerometer (g)
  ACCEL_X,
  ACCEL_Y,
  ACCEL_Z,
  ACCEL_MAGNITUDE,    // sqrt(x^2 + y^2 + z^2)
  
  // IMU - Gyroscope (deg/s)
  GYRO_X,
  GYRO_Y,
  GYRO_Z,
  GYRO_MAGNITUDE,
  
  // IMU - Magnetometer (μT)
  MAG_X,
  MAG_Y,
  MAG_Z,
  MAG_MAGNITUDE,
  
  // Environmental
  TEMPERATURE,        // Celsius
  PRESSURE,           // hPa
  HUMIDITY,           // Percent
  
  // Time-based
  TIME_MS,            // Milliseconds since boot
  TIME_SECONDS,       // Seconds since boot
  TIME_SINE_SLOW,     // sin(time / 2000) for slow oscillation
  TIME_SINE_FAST,     // sin(time / 500) for fast oscillation
  
  COUNT
};

/**
 * @brief How to modify/scale the sensor input
 */
enum class ModifierType : uint8_t {
  DIRECT = 0,         // Use sensor value directly
  INVERTED,           // Negate the value (multiply by -1)
  ABSOLUTE,           // Take absolute value
  SCALED,             // Multiply by scale factor
  CLAMPED,            // Clamp to min/max range
  NORMALIZED,         // Map from sensor range to 0-1, then to output range
  THRESHOLD,          // 0 below threshold, 1 above
  COUNT
};

/**
 * @brief Configuration for a single parameter
 */
struct __attribute__((packed)) ParameterConfig {
  SensorSource source;        // Which sensor controls this parameter
  ModifierType modifier;      // How to process the sensor value
  float base_value;           // Static base value (always applied)
  float scale;                // Scale factor for SCALED modifier
  float min_value;            // Minimum value (for CLAMPED/NORMALIZED)
  float max_value;            // Maximum value (for CLAMPED/NORMALIZED)
  float threshold;            // Threshold value (for THRESHOLD modifier)
  uint8_t _reserved[4];       // Padding for alignment
  
  // Total: 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4 = 26 bytes
  
  ParameterConfig() 
    : source(SensorSource::NONE)
    , modifier(ModifierType::DIRECT)
    , base_value(0.0f)
    , scale(1.0f)
    , min_value(0.0f)
    , max_value(1.0f)
    , threshold(0.0f)
  {
    _reserved[0] = _reserved[1] = _reserved[2] = _reserved[3] = 0;
  }
};

/**
 * @brief Sprite animation parameters (for HUB75 display effects)
 */
struct __attribute__((packed)) SpriteAnimationParams {
  // Base positioning (always applied)
  ParameterConfig offset_x;        // Horizontal position offset
  ParameterConfig offset_y;        // Vertical position offset
  ParameterConfig rotation;        // Rotation in degrees
  
  // Dynamic modifiers (can use sensors)
  ParameterConfig dynamic_offset_x; // Additional X movement
  ParameterConfig dynamic_offset_y; // Additional Y movement
  ParameterConfig dynamic_rotation; // Additional rotation
  
  // Scale and alpha
  ParameterConfig scale;           // Sprite scale (1.0 = normal)
  ParameterConfig alpha;           // Transparency (0-255)
  
  uint8_t enabled;                 // 1 = enabled, 0 = disabled
  uint8_t _reserved[7];            // Padding
  
  // Total: 8 * 26 + 1 + 7 = 216 bytes
  
  SpriteAnimationParams() : enabled(1) {
    for(int i = 0; i < 7; i++) _reserved[i] = 0;
  }
};

/**
 * @brief Dual sprite configuration (left and right sprites)
 */
struct __attribute__((packed)) DualSpriteConfig {
  SpriteAnimationParams left_sprite;
  SpriteAnimationParams right_sprite;
  
  // Total: 216 * 2 = 432 bytes
};

/**
 * @brief Shader-specific parameters
 */
struct __attribute__((packed)) ShaderParams {
  // Common parameters (used by multiple shaders)
  ParameterConfig hue_offset;      // Hue shift amount (0-360)
  ParameterConfig hue_speed;       // Hue cycle speed
  ParameterConfig color1_r;        // Primary color red
  ParameterConfig color1_g;        // Primary color green
  ParameterConfig color1_b;        // Primary color blue
  ParameterConfig color2_r;        // Secondary color red
  ParameterConfig color2_g;        // Secondary color green
  ParameterConfig color2_b;        // Secondary color blue
  ParameterConfig brightness;      // Overall brightness
  ParameterConfig breathe_speed;   // Breathing animation speed
  
  // Effect-specific parameters
  ParameterConfig intensity;       // Effect intensity/strength
  ParameterConfig scale_x;         // Horizontal scale
  ParameterConfig scale_y;         // Vertical scale
  ParameterConfig offset_x;        // Horizontal offset
  ParameterConfig offset_y;        // Vertical offset
  ParameterConfig rotation;        // Rotation angle
  
  uint8_t _reserved[8];            // Padding
  
  // Total: 16 * 26 + 8 = 424 bytes
  
  ShaderParams() {
    for(int i = 0; i < 8; i++) _reserved[i] = 0;
  }
};

/**
 * @brief Complete animation configuration
 */
struct __attribute__((packed)) AnimationConfiguration {
  DualSpriteConfig sprite_config;
  ShaderParams shader_params;
  
  uint32_t last_update_time;       // Timestamp of last update
  uint8_t config_version;          // Configuration version
  uint8_t _reserved[3];            // Padding
  
  // Total: 432 + 424 + 4 + 1 + 3 = 864 bytes
  
  AnimationConfiguration() : last_update_time(0), config_version(1) {
    _reserved[0] = _reserved[1] = _reserved[2] = 0;
  }
};

/**
 * @brief Parameter evaluator - reads sensor data and computes final parameter values
 */
class ParameterEvaluator {
public:
  /**
   * @brief Evaluate a parameter configuration with current sensor data
   * @param config Parameter configuration
   * @param sensor_data Current sensor readings
   * @param time_ms Current time in milliseconds
   * @return Final computed value
   */
  static float evaluate(const ParameterConfig& config, 
                       const arcos::communication::SensorDataPayload& sensor_data,
                       uint32_t time_ms) {
    // Start with base value
    float value = config.base_value;
    
    // Get sensor value if source is specified
    if(config.source != SensorSource::NONE) {
      float sensor_value = getSensorValue(config.source, sensor_data, time_ms);
      
      // Apply modifier
      float modified_value = applyModifier(sensor_value, config);
      
      // Add to base value
      value += modified_value;
    }
    
    return value;
  }

private:
  /**
   * @brief Extract sensor value from sensor data
   */
  static float getSensorValue(SensorSource source, 
                              const arcos::communication::SensorDataPayload& sensor_data,
                              uint32_t time_ms) {
    switch(source) {
      // Accelerometer
      case SensorSource::ACCEL_X: return sensor_data.accel_x;
      case SensorSource::ACCEL_Y: return sensor_data.accel_y;
      case SensorSource::ACCEL_Z: return sensor_data.accel_z;
      case SensorSource::ACCEL_MAGNITUDE:
        return sqrtf(sensor_data.accel_x * sensor_data.accel_x + 
                    sensor_data.accel_y * sensor_data.accel_y + 
                    sensor_data.accel_z * sensor_data.accel_z);
      
      // Gyroscope
      case SensorSource::GYRO_X: return sensor_data.gyro_x;
      case SensorSource::GYRO_Y: return sensor_data.gyro_y;
      case SensorSource::GYRO_Z: return sensor_data.gyro_z;
      case SensorSource::GYRO_MAGNITUDE:
        return sqrtf(sensor_data.gyro_x * sensor_data.gyro_x + 
                    sensor_data.gyro_y * sensor_data.gyro_y + 
                    sensor_data.gyro_z * sensor_data.gyro_z);
      
      // Magnetometer
      case SensorSource::MAG_X: return sensor_data.mag_x;
      case SensorSource::MAG_Y: return sensor_data.mag_y;
      case SensorSource::MAG_Z: return sensor_data.mag_z;
      case SensorSource::MAG_MAGNITUDE:
        return sqrtf(sensor_data.mag_x * sensor_data.mag_x + 
                    sensor_data.mag_y * sensor_data.mag_y + 
                    sensor_data.mag_z * sensor_data.mag_z);
      
      // Environmental
      case SensorSource::TEMPERATURE: return sensor_data.temperature;
      case SensorSource::PRESSURE: return sensor_data.pressure;
      case SensorSource::HUMIDITY: return sensor_data.humidity;
      
      // Time-based
      case SensorSource::TIME_MS: return static_cast<float>(time_ms);
      case SensorSource::TIME_SECONDS: return time_ms / 1000.0f;
      case SensorSource::TIME_SINE_SLOW: return sinf(time_ms / 2000.0f);
      case SensorSource::TIME_SINE_FAST: return sinf(time_ms / 500.0f);
      
      default: return 0.0f;
    }
  }
  
  /**
   * @brief Apply modifier to sensor value
   */
  static float applyModifier(float sensor_value, const ParameterConfig& config) {
    switch(config.modifier) {
      case ModifierType::DIRECT:
        return sensor_value;
      
      case ModifierType::INVERTED:
        return -sensor_value;
      
      case ModifierType::ABSOLUTE:
        return fabsf(sensor_value);
      
      case ModifierType::SCALED:
        return sensor_value * config.scale;
      
      case ModifierType::CLAMPED: {
        float scaled = sensor_value * config.scale;
        if(scaled < config.min_value) return config.min_value;
        if(scaled > config.max_value) return config.max_value;
        return scaled;
      }
      
      case ModifierType::NORMALIZED: {
        // Map from assumed sensor range to 0-1, then to output range
        // Assume sensor range is -config.scale to +config.scale
        float normalized = (sensor_value + config.scale) / (2.0f * config.scale);
        normalized = fminf(1.0f, fmaxf(0.0f, normalized)); // Clamp 0-1
        return config.min_value + normalized * (config.max_value - config.min_value);
      }
      
      case ModifierType::THRESHOLD:
        return (sensor_value >= config.threshold) ? config.max_value : config.min_value;
      
      default:
        return sensor_value;
    }
  }
};

} // namespace arcos::manager

#endif // ANIMATION_PARAMETER_SYSTEM_HPP
