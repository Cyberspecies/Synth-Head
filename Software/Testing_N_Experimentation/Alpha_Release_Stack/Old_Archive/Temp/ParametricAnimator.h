/**
 * @file ParametricAnimator.h
 * @brief Local animation generator - reconstructs animations from parameters
 * 
 * Runs on CPU to generate 60 FPS animations from compact parameter updates
 */

#ifndef PARAMETRIC_ANIMATOR_H
#define PARAMETRIC_ANIMATOR_H

#include <Arduino.h>
#include "ParametricLedProtocol.h"

class ParametricAnimator {
public:
    ParametricAnimator() 
        : current_animation(ANIM_OFF)
        , param1(0.0f)
        , param2(0.0f)
        , param3(0.0f)
        , local_frame(0)
        , local_time(0)
    {}

    // Update animation parameters (called when new packet arrives)
    void updateParams(const AnimationParams& params) {
        current_animation = static_cast<AnimationType>(params.animation_type);
        param1 = params.param1;
        param2 = params.param2;
        param3 = params.param3;
    }

    // Generate LED data for current frame (called at 60 FPS)
    // Outputs 196 bytes (49 LEDs × 4 bytes RGBW)
    void generateFrame(uint8_t* led_data, uint32_t num_leds) {
        local_time = millis();
        local_frame++;

        switch (current_animation) {
            case ANIM_OFF:
                generateOff(led_data, num_leds);
                break;
            case ANIM_SOLID:
                generateSolid(led_data, num_leds);
                break;
            case ANIM_RAINBOW:
                generateRainbow(led_data, num_leds);
                break;
            case ANIM_GRADIENT:
                generateGradient(led_data, num_leds);
                break;
            case ANIM_WAVE:
                generateWave(led_data, num_leds);
                break;
            case ANIM_BREATHING:
                generateBreathing(led_data, num_leds);
                break;
            default:
                generateOff(led_data, num_leds);
                break;
        }
    }

private:
    AnimationType current_animation;
    float param1, param2, param3;
    uint32_t local_frame;
    uint32_t local_time;

    // Animation generators
    void generateOff(uint8_t* led_data, uint32_t num_leds);
    void generateSolid(uint8_t* led_data, uint32_t num_leds);
    void generateRainbow(uint8_t* led_data, uint32_t num_leds);
    void generateGradient(uint8_t* led_data, uint32_t num_leds);
    void generateWave(uint8_t* led_data, uint32_t num_leds);
    void generateBreathing(uint8_t* led_data, uint32_t num_leds);

    // Helper: HSV to RGBW conversion
    void hsvToRgbw(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w);
    
    // Helper: Linear interpolation
    float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
};

#endif // PARAMETRIC_ANIMATOR_H
