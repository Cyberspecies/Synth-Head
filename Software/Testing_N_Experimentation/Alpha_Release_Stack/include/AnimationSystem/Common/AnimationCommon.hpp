/*****************************************************************
 * @file AnimationCommon.hpp
 * @brief Common types and constants for the Animation System
 * 
 * Shared definitions used across animations, transitions, and shaders.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <functional>

namespace AnimationSystem {

// ================================================================
// GPU CALLBACK TYPES
// ================================================================
using ClearFunc = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;
using BlitSpriteFunc = std::function<void(int id, float x, float y)>;
using BlitSpriteRotatedFunc = std::function<void(int id, float x, float y, float angle)>;
using FillCircleFunc = std::function<void(int cx, int cy, int r, uint8_t red, uint8_t green, uint8_t blue)>;
using DrawCircleFFunc = std::function<void(float x, float y, float radius, uint8_t r, uint8_t g, uint8_t b)>;
using FillRectFunc = std::function<void(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)>;
using DrawLineFunc = std::function<void(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b)>;
using DrawPixelFunc = std::function<void(int x, int y, uint8_t r, uint8_t g, uint8_t b)>;
using PresentFunc = std::function<void()>;

// ================================================================
// DISPLAY CONSTANTS (128x32 HUB75, split into 2 x 64x32 eyes)
// ================================================================
constexpr int DISPLAY_W = 128;
constexpr int DISPLAY_H = 32;
constexpr int EYE_W = 64;
constexpr int EYE_H = 32;
constexpr int LEFT_EYE_X = 0;
constexpr int RIGHT_EYE_X = 64;

// ================================================================
// BASE ANIMATION INTERFACE
// ================================================================
struct IAnimation {
    virtual ~IAnimation() = default;
    virtual void update(uint32_t deltaMs) = 0;
    virtual void render(FillRectFunc fillRect, DrawPixelFunc drawPixel, 
                       ClearFunc clear, PresentFunc present) = 0;
    virtual void reset() {}
};

// ================================================================
// BASE TRANSITION INTERFACE
// ================================================================
struct ITransition {
    virtual ~ITransition() = default;
    virtual void init() = 0;
    virtual void update(uint32_t deltaMs) = 0;
    virtual void render(FillRectFunc fillRect, DrawPixelFunc drawPixel,
                       ClearFunc clear, PresentFunc present) = 0;
    virtual bool isComplete() const = 0;
    virtual void reset() = 0;
    
    // Capture pixels from an animation for transition effects
    virtual void capturePixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {}
};

// ================================================================
// BASE SHADER/EFFECT INTERFACE
// ================================================================
struct IShader {
    virtual ~IShader() = default;
    virtual void update(uint32_t deltaMs) = 0;
    virtual void apply(FillRectFunc fillRect) = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
    virtual void setIntensity(float intensity) = 0;
    virtual float getIntensity() const = 0;
    virtual void reset() = 0;
};

} // namespace AnimationSystem
