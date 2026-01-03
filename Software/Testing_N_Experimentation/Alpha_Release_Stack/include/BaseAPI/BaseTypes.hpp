/*****************************************************************
 * File:      BaseTypes.hpp
 * Category:  include/BaseAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Core type definitions for the Base System API layer.
 *    These types are hardware-agnostic and used throughout
 *    the middleware layer.
 * 
 * Layer:
 *    HAL Layer -> [Base System API] -> Application Layer
 *****************************************************************/

#ifndef ARCOS_INCLUDE_BASEAPI_BASE_TYPES_HPP_
#define ARCOS_INCLUDE_BASEAPI_BASE_TYPES_HPP_

#include <stdint.h>
#include <stddef.h>
#include <cmath>

namespace arcos::base{

// ============================================================
// Result Types
// ============================================================

/** Base API operation result codes */
enum class Result : uint8_t{
  OK = 0,              // Operation successful
  ERROR,               // Generic error
  TIMEOUT,             // Operation timed out
  BUSY,                // Resource is busy
  INVALID_PARAM,       // Invalid parameter
  NOT_INITIALIZED,     // Module not initialized
  NOT_CONNECTED,       // Communication not established
  BUFFER_FULL,         // Buffer is full
  BUFFER_EMPTY,        // Buffer is empty
  NO_DATA,             // No data available
  CHECKSUM_ERROR,      // Data integrity failure
  PROTOCOL_ERROR,      // Protocol violation
  SYNC_LOST            // Synchronization lost
};

// ============================================================
// Vector Math Types
// ============================================================

/** 3D float vector with math operations */
struct Vec3{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  
  Vec3() = default;
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_){}
  
  // Basic operations
  Vec3 operator+(const Vec3& v) const{ return Vec3(x + v.x, y + v.y, z + v.z); }
  Vec3 operator-(const Vec3& v) const{ return Vec3(x - v.x, y - v.y, z - v.z); }
  Vec3 operator*(float s) const{ return Vec3(x * s, y * s, z * s); }
  Vec3 operator/(float s) const{ return Vec3(x / s, y / s, z / s); }
  Vec3& operator+=(const Vec3& v){ x += v.x; y += v.y; z += v.z; return *this; }
  Vec3& operator-=(const Vec3& v){ x -= v.x; y -= v.y; z -= v.z; return *this; }
  Vec3& operator*=(float s){ x *= s; y *= s; z *= s; return *this; }
  Vec3& operator/=(float s){ x /= s; y /= s; z /= s; return *this; }
  
  /** Dot product */
  float dot(const Vec3& v) const{ return x * v.x + y * v.y + z * v.z; }
  
  /** Cross product */
  Vec3 cross(const Vec3& v) const{
    return Vec3(
      y * v.z - z * v.y,
      z * v.x - x * v.z,
      x * v.y - y * v.x
    );
  }
  
  /** Magnitude */
  float magnitude() const{ return sqrtf(x*x + y*y + z*z); }
  
  /** Magnitude squared (faster, no sqrt) */
  float magnitudeSquared() const{ return x*x + y*y + z*z; }
  
  /** Normalize (return unit vector) */
  Vec3 normalized() const{
    float m = magnitude();
    if(m > 0.0001f) return *this / m;
    return Vec3(0, 0, 0);
  }
  
  /** Normalize in place */
  void normalize(){
    float m = magnitude();
    if(m > 0.0001f){ x /= m; y /= m; z /= m; }
  }
};

/** Quaternion for rotation representation */
struct Quaternion{
  float w = 1.0f;  // Scalar component
  float x = 0.0f;  // Vector components
  float y = 0.0f;
  float z = 0.0f;
  
  Quaternion() = default;
  Quaternion(float w_, float x_, float y_, float z_)
    : w(w_), x(x_), y(y_), z(z_){}
  
  /** Create from axis-angle representation */
  static Quaternion fromAxisAngle(const Vec3& axis, float angle){
    float half = angle * 0.5f;
    float s = sinf(half);
    Vec3 n = axis.normalized();
    return Quaternion(cosf(half), n.x * s, n.y * s, n.z * s);
  }
  
  /** Create from Euler angles (roll, pitch, yaw in radians) */
  static Quaternion fromEuler(float roll, float pitch, float yaw){
    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);
    float cy = cosf(yaw * 0.5f);
    float sy = sinf(yaw * 0.5f);
    
    return Quaternion(
      cr * cp * cy + sr * sp * sy,
      sr * cp * cy - cr * sp * sy,
      cr * sp * cy + sr * cp * sy,
      cr * cp * sy - sr * sp * cy
    );
  }
  
  /** Quaternion multiplication */
  Quaternion operator*(const Quaternion& q) const{
    return Quaternion(
      w*q.w - x*q.x - y*q.y - z*q.z,
      w*q.x + x*q.w + y*q.z - z*q.y,
      w*q.y - x*q.z + y*q.w + z*q.x,
      w*q.z + x*q.y - y*q.x + z*q.w
    );
  }
  
  /** Magnitude */
  float magnitude() const{ return sqrtf(w*w + x*x + y*y + z*z); }
  
  /** Normalize in place */
  void normalize(){
    float m = magnitude();
    if(m > 0.0001f){ w /= m; x /= m; y /= m; z /= m; }
  }
  
  /** Get normalized quaternion */
  Quaternion normalized() const{
    float m = magnitude();
    if(m > 0.0001f) return Quaternion(w/m, x/m, y/m, z/m);
    return Quaternion();
  }
  
  /** Conjugate (inverse for unit quaternion) */
  Quaternion conjugate() const{ return Quaternion(w, -x, -y, -z); }
  
  /** Rotate a vector by this quaternion */
  Vec3 rotate(const Vec3& v) const{
    Quaternion p(0, v.x, v.y, v.z);
    Quaternion result = (*this) * p * conjugate();
    return Vec3(result.x, result.y, result.z);
  }
  
  /** Convert to Euler angles (roll, pitch, yaw in radians) */
  void toEuler(float& roll, float& pitch, float& yaw) const{
    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    roll = atan2f(sinr_cosp, cosr_cosp);
    
    // Pitch (y-axis rotation)
    float sinp = 2.0f * (w * y - z * x);
    if(fabsf(sinp) >= 1.0f)
      pitch = copysignf(3.14159265f / 2.0f, sinp);
    else
      pitch = asinf(sinp);
    
    // Yaw (z-axis rotation)
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    yaw = atan2f(siny_cosp, cosy_cosp);
  }
};

// ============================================================
// Color Types
// ============================================================

/** RGB color with float operations */
struct Color{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;  // Alpha for blending
  
  Color() = default;
  Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
    : r(r_), g(g_), b(b_), a(a_){}
  
  /** Create from HSV (h: 0-255, s: 0-255, v: 0-255) */
  static Color fromHSV(uint8_t h, uint8_t s, uint8_t v){
    if(s == 0) return Color(v, v, v);
    
    uint8_t region = h / 43;
    uint8_t remainder = (h - region * 43) * 6;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch(region){
      case 0:  return Color(v, t, p);
      case 1:  return Color(q, v, p);
      case 2:  return Color(p, v, t);
      case 3:  return Color(p, q, v);
      case 4:  return Color(t, p, v);
      default: return Color(v, p, q);
    }
  }
  
  /** Pack to 16-bit RGB565 */
  uint16_t toRGB565() const{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  }
  
  /** Unpack from 16-bit RGB565 */
  static Color fromRGB565(uint16_t c){
    return Color(
      ((c >> 11) & 0x1F) << 3,
      ((c >> 5) & 0x3F) << 2,
      (c & 0x1F) << 3
    );
  }
};

/** RGBW color with white channel */
struct ColorW : public Color{
  uint8_t w = 0;  // White channel
  
  ColorW() = default;
  ColorW(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t w_ = 0)
    : Color(r_, g_, b_), w(w_){}
  ColorW(const Color& c, uint8_t w_ = 0)
    : Color(c), w(w_){}
};

// ============================================================
// Time Types
// ============================================================

/** Timestamp in milliseconds */
using Timestamp = uint32_t;

/** Time delta in milliseconds */
using TimeDelta = uint32_t;

// ============================================================
// System Identification
// ============================================================

/** Device role in the system */
enum class DeviceRole : uint8_t{
  CPU = 0,    // Main processor (sensors, logic)
  GPU = 1,    // Graphics processor (displays, LEDs)
  UNKNOWN = 255
};

/** System version info */
struct Version{
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  
  Version() : major(0), minor(1), patch(0){}
  Version(uint8_t maj, uint8_t min, uint8_t pat)
    : major(maj), minor(min), patch(pat){}
};

// ============================================================
// Constants
// ============================================================

/** Math constants - using unique names to avoid conflicts with Arduino.h macros */
namespace math{
  // Undef any conflicting Arduino macros (they use #define which pollutes namespace)
  #ifdef PI
  #undef PI
  #endif
  #ifdef TWO_PI
  #undef TWO_PI
  #endif
  #ifdef HALF_PI
  #undef HALF_PI
  #endif
  #ifdef DEG_TO_RAD
  #undef DEG_TO_RAD
  #endif
  #ifdef RAD_TO_DEG
  #undef RAD_TO_DEG
  #endif
  
  constexpr float PI = 3.14159265358979323846f;
  constexpr float TWO_PI = 2.0f * PI;
  constexpr float HALF_PI = PI / 2.0f;
  constexpr float DEG_TO_RAD = PI / 180.0f;
  constexpr float RAD_TO_DEG = 180.0f / PI;
  constexpr float GRAVITY = 9.80665f;  // m/s²
}

} // namespace arcos::base

#endif // ARCOS_INCLUDE_BASEAPI_BASE_TYPES_HPP_
