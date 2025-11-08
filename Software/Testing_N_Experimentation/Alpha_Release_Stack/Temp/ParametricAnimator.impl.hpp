/**
 * @file ParametricAnimator.impl.hpp
 * @brief Implementation of parametric animation generators
 */

#ifndef PARAMETRIC_ANIMATOR_IMPL_HPP
#define PARAMETRIC_ANIMATOR_IMPL_HPP

#include "ParametricAnimator.h"
#include <math.h>

// Helper: HSV to RGBW conversion
void ParametricAnimator::hsvToRgbw(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) {
    // Normalize hue to 0-360
    while (h < 0) h += 360.0f;
    while (h >= 360.0f) h -= 360.0f;
    
    // Clamp s and v to 0-1
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    
    // HSV to RGB conversion
    float c = v * s;
    float x = c * (1.0f - fabs(fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r_f, g_f, b_f;
    
    if (h < 60.0f) {
        r_f = c; g_f = x; b_f = 0;
    } else if (h < 120.0f) {
        r_f = x; g_f = c; b_f = 0;
    } else if (h < 180.0f) {
        r_f = 0; g_f = c; b_f = x;
    } else if (h < 240.0f) {
        r_f = 0; g_f = x; b_f = c;
    } else if (h < 300.0f) {
        r_f = x; g_f = 0; b_f = c;
    } else {
        r_f = c; g_f = 0; b_f = x;
    }
    
    // Convert to 0-255 and add brightness offset
    r = (uint8_t)((r_f + m) * 255.0f);
    g = (uint8_t)((g_f + m) * 255.0f);
    b = (uint8_t)((b_f + m) * 255.0f);
    
    // White channel: extract luminance for RGBW LEDs
    // Use desaturation amount as white contribution
    w = (uint8_t)((1.0f - s) * v * 255.0f);
}

// ANIM_OFF: All LEDs off
void ParametricAnimator::generateOff(uint8_t* led_data, uint32_t num_leds) {
    for (uint32_t i = 0; i < num_leds * 4; i++) {
        led_data[i] = 0;
    }
}

// ANIM_SOLID: Single solid color
// param1: hue (0-360)
// param2: saturation (0-1)
// param3: value/brightness (0-1)
void ParametricAnimator::generateSolid(uint8_t* led_data, uint32_t num_leds) {
    uint8_t r, g, b, w;
    hsvToRgbw(param1, param2, param3, r, g, b, w);
    
    for (uint32_t i = 0; i < num_leds; i++) {
        led_data[i * 4 + 0] = r;
        led_data[i * 4 + 1] = g;
        led_data[i * 4 + 2] = b;
        led_data[i * 4 + 3] = w;
    }
}

// ANIM_RAINBOW: Rainbow cycle
// param1: hue_offset (global rotation, degrees)
// param2: hue_speed (degrees per frame, for local animation)
// param3: brightness (0-1)
void ParametricAnimator::generateRainbow(uint8_t* led_data, uint32_t num_leds) {
    // Add local animation: hue advances by param2 degrees per frame
    float animated_offset = param1 + (local_frame * param2);
    
    for (uint32_t i = 0; i < num_leds; i++) {
        // Each LED gets a different hue based on position
        float hue = animated_offset + (i * 360.0f / num_leds);
        
        uint8_t r, g, b, w;
        hsvToRgbw(hue, 1.0f, param3, r, g, b, w);
        
        led_data[i * 4 + 0] = r;
        led_data[i * 4 + 1] = g;
        led_data[i * 4 + 2] = b;
        led_data[i * 4 + 3] = w;
    }
}

// ANIM_GRADIENT: Two-color gradient
// param1: start_hue (0-360)
// param2: end_hue (0-360)
// param3: brightness (0-1)
void ParametricAnimator::generateGradient(uint8_t* led_data, uint32_t num_leds) {
    for (uint32_t i = 0; i < num_leds; i++) {
        float t = (float)i / (float)(num_leds - 1);
        float hue = lerp(param1, param2, t);
        
        uint8_t r, g, b, w;
        hsvToRgbw(hue, 1.0f, param3, r, g, b, w);
        
        led_data[i * 4 + 0] = r;
        led_data[i * 4 + 1] = g;
        led_data[i * 4 + 2] = b;
        led_data[i * 4 + 3] = w;
    }
}

// ANIM_WAVE: Traveling wave
// param1: wave_position (0-1, wraps around)
// param2: wave_speed (units per frame)
// param3: wave_width (0-1)
void ParametricAnimator::generateWave(uint8_t* led_data, uint32_t num_leds) {
    // Add local animation
    float animated_position = param1 + (local_frame * param2);
    animated_position = fmod(animated_position, 1.0f);
    
    for (uint32_t i = 0; i < num_leds; i++) {
        float led_pos = (float)i / (float)num_leds;
        
        // Distance from wave center
        float distance = fabs(led_pos - animated_position);
        if (distance > 0.5f) distance = 1.0f - distance; // Wrap around
        
        // Brightness falls off with distance
        float brightness = 1.0f - (distance / param3);
        if (brightness < 0.0f) brightness = 0.0f;
        
        // Rainbow hue based on position
        float hue = led_pos * 360.0f;
        
        uint8_t r, g, b, w;
        hsvToRgbw(hue, 1.0f, brightness, r, g, b, w);
        
        led_data[i * 4 + 0] = r;
        led_data[i * 4 + 1] = g;
        led_data[i * 4 + 2] = b;
        led_data[i * 4 + 3] = w;
    }
}

// ANIM_BREATHING: Breathing effect
// param1: hue (0-360)
// param2: breath_rate (cycles per second)
// param3: min_brightness (0-1)
void ParametricAnimator::generateBreathing(uint8_t* led_data, uint32_t num_leds) {
    // Calculate breathing brightness using sine wave
    float breath_phase = (local_time / 1000.0f) * param2 * 2.0f * PI;
    float breath_brightness = (sin(breath_phase) + 1.0f) / 2.0f; // 0 to 1
    breath_brightness = lerp(param3, 1.0f, breath_brightness); // Apply min brightness
    
    uint8_t r, g, b, w;
    hsvToRgbw(param1, 1.0f, breath_brightness, r, g, b, w);
    
    for (uint32_t i = 0; i < num_leds; i++) {
        led_data[i * 4 + 0] = r;
        led_data[i * 4 + 1] = g;
        led_data[i * 4 + 2] = b;
        led_data[i * 4 + 3] = w;
    }
}

#endif // PARAMETRIC_ANIMATOR_IMPL_HPP
