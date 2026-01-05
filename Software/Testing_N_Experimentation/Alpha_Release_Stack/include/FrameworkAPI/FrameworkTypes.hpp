/*****************************************************************
 * File:      FrameworkTypes.hpp
 * Category:  include/FrameworkAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Common types and enums for the Framework API layer.
 *    Self-contained with optional integration with BaseAPI types.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_FRAMEWORKAPI_FRAMEWORK_TYPES_HPP_
#define ARCOS_INCLUDE_FRAMEWORKAPI_FRAMEWORK_TYPES_HPP_

#include <cstdint>
#include <cmath>
#include <functional>

// Optional: Include BaseTypes if available
#if __has_include("BaseAPI/BaseTypes.hpp")
  #include "BaseAPI/BaseTypes.hpp"
  #define FRAMEWORK_HAS_BASE_TYPES 1
#else
  #define FRAMEWORK_HAS_BASE_TYPES 0
#endif

namespace arcos::framework {

// ============================================================
// Core Types (self-contained or re-export from BaseTypes)
// ============================================================

#if FRAMEWORK_HAS_BASE_TYPES
  // Re-export base types
  using arcos::base::Result;
  using arcos::base::Vec3;
  using arcos::base::Quaternion;
  using arcos::base::Timestamp;
#else
  // Define our own types when BaseTypes not available
  
  /**
   * Result codes for API operations
   */
  enum class Result : uint8_t {
    OK = 0,
    ERROR,
    NOT_INITIALIZED,
    INVALID_PARAMETER,
    BUFFER_FULL,
    BUFFER_EMPTY,
    TIMEOUT,
    NOT_SUPPORTED,
    BUSY,
    NOT_FOUND
  };
  
  /**
   * 3D Vector
   */
  struct Vec3 {
    float x, y, z;
    
    constexpr Vec3() : x(0), y(0), z(0) {}
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float magnitude() const { return sqrtf(x*x + y*y + z*z); }
  };
  
  /**
   * Quaternion for rotations
   */
  struct Quaternion {
    float w, x, y, z;
    
    constexpr Quaternion() : w(1), x(0), y(0), z(0) {}
    constexpr Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}
  };
  
  /**
   * High resolution timestamp
   */
  struct Timestamp {
    uint64_t micros;
    
    Timestamp() : micros(0) {}
    Timestamp(uint64_t us) : micros(us) {}
  };
#endif

// ============================================================
// Display Types
// ============================================================

enum class Display : uint8_t {
  HUB75 = 0,    // LED matrix display (128x32)
  OLED = 1,     // OLED display (128x128)
  ALL = 255     // Target all displays
};

// ============================================================
// Color Types
// ============================================================

struct Color {
  uint8_t r, g, b, a;
  
  constexpr Color() : r(0), g(0), b(0), a(255) {}
  constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) 
    : r(r), g(g), b(b), a(a) {}
  
  // Common colors
  static constexpr Color Black()   { return Color(0, 0, 0); }
  static constexpr Color White()   { return Color(255, 255, 255); }
  static constexpr Color Red()     { return Color(255, 0, 0); }
  static constexpr Color Green()   { return Color(0, 255, 0); }
  static constexpr Color Blue()    { return Color(0, 0, 255); }
  static constexpr Color Yellow()  { return Color(255, 255, 0); }
  static constexpr Color Cyan()    { return Color(0, 255, 255); }
  static constexpr Color Magenta() { return Color(255, 0, 255); }
  static constexpr Color Orange()  { return Color(255, 128, 0); }
  static constexpr Color Purple()  { return Color(128, 0, 255); }
  
  // HSV to RGB conversion
  static Color fromHSV(float h, float s, float v) {
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    s = s < 0 ? 0 : (s > 1 ? 1 : s);
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    
    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
    float m = v - c;
    
    float r, g, b;
    if (h < 60)      { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }
    
    return Color((r + m) * 255, (g + m) * 255, (b + m) * 255);
  }
  
  // Interpolate between two colors
  static Color lerp(const Color& a, const Color& b, float t) {
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return Color(
      a.r + (b.r - a.r) * t,
      a.g + (b.g - a.g) * t,
      a.b + (b.b - a.b) * t,
      a.a + (b.a - a.a) * t
    );
  }
};

// ============================================================
// Input Types
// ============================================================

enum class ButtonId : uint8_t {
  BUTTON_A = 0,
  BUTTON_B = 1,
  BUTTON_C = 2,
  BUTTON_D = 3,
  ENCODER_SW = 4,
  MAX_BUTTONS
};

enum class InputEvent : uint8_t {
  NONE = 0,
  PRESS,        // Button just pressed
  RELEASE,      // Button just released
  CLICK,        // Short press and release
  DOUBLE_CLICK, // Two quick clicks
  LONG_PRESS,   // Held for long press threshold
  HOLD,         // Still being held after long press
  ENCODER_CW,   // Encoder clockwise
  ENCODER_CCW   // Encoder counter-clockwise
};

struct InputState {
  ButtonId button;
  InputEvent event;
  uint32_t timestamp;
  uint32_t duration_ms;  // How long button was held
  int16_t encoder_delta; // Encoder movement
};

using InputCallback = std::function<void(const InputState&)>;

// ============================================================
// Animation Types
// ============================================================

enum class Easing : uint8_t {
  LINEAR = 0,
  EASE_IN_QUAD,
  EASE_OUT_QUAD,
  EASE_IN_OUT_QUAD,
  EASE_IN_CUBIC,
  EASE_OUT_CUBIC,
  EASE_IN_OUT_CUBIC,
  EASE_IN_ELASTIC,
  EASE_OUT_ELASTIC,
  EASE_IN_BOUNCE,
  EASE_OUT_BOUNCE
};

enum class BuiltinEffect : uint8_t {
  NONE = 0,
  SOLID,
  RAINBOW,
  RAINBOW_CYCLE,
  PULSE,
  BREATHE,
  SPARKLE,
  FIRE,
  WAVE,
  SCANNER,
  FADE,
  GRADIENT,
  MATRIX_RAIN,
  PLASMA,
  NOISE
};

enum class BlendMode : uint8_t {
  REPLACE = 0,
  ADD,
  MULTIPLY,
  SCREEN,
  OVERLAY,
  ALPHA
};

// ============================================================
// Network Types
// ============================================================

enum class WiFiState : uint8_t {
  DISCONNECTED = 0,
  CONNECTING,
  CONNECTED,
  AP_MODE,
  CAPTIVE_PORTAL
};

enum class NetworkEvent : uint8_t {
  NONE = 0,
  CONNECTED,
  DISCONNECTED,
  IP_ACQUIRED,
  IP_LOST,
  CONNECT_FAILED,
  CLIENT_CONNECTED,
  CLIENT_DISCONNECTED,
  CONFIG_RECEIVED
};

struct NetworkConfig {
  char ssid[33];
  char password[65];
  char hostname[33];
  bool use_dhcp;
  uint32_t static_ip;
  uint32_t gateway;
  uint32_t subnet;
};

using NetworkCallback = std::function<void(WiFiState)>;

// ============================================================
// Metrics Types
// ============================================================

using MetricCallback = std::function<void(const void* data, size_t size)>;

struct MetricInfo {
  const char* topic;
  const char* type_name;
  size_t data_size;
  uint32_t publish_rate_hz;
  uint32_t subscriber_count;
};

} // namespace arcos::framework

#endif // ARCOS_INCLUDE_FRAMEWORKAPI_FRAMEWORK_TYPES_HPP_
