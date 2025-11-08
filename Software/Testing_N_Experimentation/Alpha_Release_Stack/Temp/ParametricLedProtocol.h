/**
 * @file ParametricLedProtocol.h
 * @brief Parametric LED Protocol - Send animation parameters instead of raw pixel data
 * 
 * Instead of sending 196 bytes per frame, send small parameter updates
 * CPU locally generates animation from parameters at 60 FPS
 * 
 * Bandwidth reduction: 196 bytes @ 60 FPS → ~12 bytes @ 1-10 FPS (updates only)
 */

#ifndef PARAMETRIC_LED_PROTOCOL_H
#define PARAMETRIC_LED_PROTOCOL_H

#include <stdint.h>

// Animation types
enum AnimationType : uint8_t {
    ANIM_OFF = 0,           // All LEDs off
    ANIM_SOLID = 1,         // Single solid color
    ANIM_RAINBOW = 2,       // Rainbow cycle
    ANIM_GRADIENT = 3,      // Two-color gradient
    ANIM_WAVE = 4,          // Traveling wave
    ANIM_BREATHING = 5,     // Breathing effect
    ANIM_SPARKLE = 6,       // Random sparkles
    ANIM_FIRE = 7,          // Fire effect
    ANIM_STROBE = 8         // Strobe effect
};

// Compact parameter packet (16 bytes total)
struct __attribute__((packed)) AnimationParams {
    uint16_t magic;         // 0xAA55 - sync marker
    uint8_t animation_type; // AnimationType enum
    uint8_t frame_counter;  // Increments each update (for skip detection)
    
    // Universal parameters (meaning depends on animation type)
    float param1;           // e.g., hue offset, primary color H, wave position
    float param2;           // e.g., speed, saturation, wave speed
    float param3;           // e.g., brightness, value, wave width
    
    uint8_t crc8;           // CRC validation
};

// Button data packet (7 bytes - unchanged)
struct __attribute__((packed)) ButtonDataPacket {
    uint16_t magic;         // 0x5AA5
    uint8_t button_a;
    uint8_t button_b;
    uint8_t button_c;
    uint8_t button_d;
    uint8_t crc8;
};

// Parameter meanings by animation type:
//
// ANIM_SOLID:
//   param1: hue (0-360)
//   param2: saturation (0-1)
//   param3: value/brightness (0-1)
//
// ANIM_RAINBOW:
//   param1: hue_offset (global rotation, degrees)
//   param2: hue_speed (degrees per frame)
//   param3: brightness (0-1)
//
// ANIM_GRADIENT:
//   param1: start_hue (0-360)
//   param2: end_hue (0-360)
//   param3: brightness (0-1)
//
// ANIM_WAVE:
//   param1: wave_position (0-1, wraps around)
//   param2: wave_speed (units per frame)
//   param3: wave_width (0-1)
//
// ANIM_BREATHING:
//   param1: hue (0-360)
//   param2: breath_rate (cycles per second)
//   param3: min_brightness (0-1)
//
// ANIM_SPARKLE:
//   param1: hue (0-360, or -1 for random)
//   param2: sparkle_density (0-1)
//   param3: brightness (0-1)

// CRC8 calculation (polynomial 0x07)
inline uint8_t calculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// UDP ports
constexpr uint16_t PARAM_LED_PORT = 8888;
constexpr uint16_t PARAM_BUTTON_PORT = 8889;

#endif // PARAMETRIC_LED_PROTOCOL_H
