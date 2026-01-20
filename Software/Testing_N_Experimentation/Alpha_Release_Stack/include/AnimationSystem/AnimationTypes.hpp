#pragma once

#include <functional>
#include <cmath>
#include <cstdint>

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

// Animation timing
constexpr uint32_t ANIMATION_DURATION_MS = 5000;  // 5 seconds per animation

} // namespace AnimationSystem
