# Weighted Pixel Rendering (Anti-Aliasing)

**Version:** 1.0  
**Last Updated:** January 2026  
**Author:** XCR1793 (Feather Forge)

---

## Overview

Weighted pixel rendering is an anti-aliasing technique that creates smoother graphics by calculating how much each pixel covers the underlying geometry. Instead of pixels being simply "on" or "off", they are given opacity values based on their coverage of lines, edges, and shapes.

This feature is **enabled by default** in the GpuDriver to reduce:
- **Aliasing** - The "jagged edge" or "staircase" effect on diagonal lines
- **Stuttery movement** - Jerky motion when sprites/objects move less than 1 pixel per frame
- **Temporal artifacts** - Flickering caused by objects crossing pixel boundaries

---

## How It Works

### The Problem: Integer Coordinates

With traditional integer-based rendering:
```
A line from (0,0) to (10,3) renders as:
██░░░░░░░░░
░░██░░░░░░░
░░░░██░░░░░
░░░░░░████░

Visible staircase pattern (aliasing)
```

When a sprite moves from x=5 to x=6, it "jumps" a full pixel, causing stuttering.

### The Solution: Sub-Pixel Precision

With weighted pixels (8.8 fixed-point coordinates):
```
Same line with anti-aliasing:
█▓░░░░░░░░░
░▓█▓░░░░░░░
░░░▓█▓░░░░░
░░░░░▓███▓░

Smooth edges using partial pixel coverage
▓ = pixel at ~50% opacity
```

When a sprite moves from x=5.0 to x=5.5 to x=6.0, it gradually "fades" between pixel positions.

### 8.8 Fixed-Point Format

Coordinates are encoded as 16-bit values split into:
- **High byte**: Signed integer part (-128 to +127)
- **Low byte**: Fractional part (0-255 = 0.0 to ~0.996)

Example: `45.75` becomes:
- Integer: `45` (0x2D)
- Fraction: `192` (0xC0) because 0.75 × 256 = 192
- Wire format: `[0xC0, 0x2D]` (fraction first, then integer)

---

## GPU Implementation

The GPU calculates pixel weights by determining the coverage of each pixel:

```
┌─────────┬─────────┬─────────┐
│ Pixel A │ Pixel B │ Pixel C │
│ 0% line │ 75% line│ 25% line│
│ coverage│ coverage│ coverage│
└─────────┴─────────┴─────────┘
         Line passes here ─────▶
```

For each pixel the line passes through:
1. Calculate what percentage of the pixel is covered
2. Multiply the color by that percentage
3. Blend with existing pixel (additive or alpha)

This is computed in real-time on the GPU for every frame.

---

## Using Weighted Pixels

### Default Behavior (Recommended)

Weighted pixels are **enabled by default**. Just use the GpuDriver normally:

```cpp
#include "SystemAPI/GPU/GpuDriver.h"
using namespace SystemAPI;

GpuDriver gpu;
gpu.init();  // Weighted pixels ON by default

// Integer methods automatically use weighted rendering
gpu.drawLine(10, 5, 100, 28, Color::Red());  // Anti-aliased!
gpu.drawCircle(64, 16, 12, Color::Green());  // Smooth edges!
```

### Explicit Float Methods (More Control)

For animations and precise positioning, use the float methods (with `F` suffix) directly:

```cpp
// Smooth sprite movement (sub-pixel positioning)
float spriteX = 45.0f;
for (int frame = 0; frame < 100; frame++) {
    gpu.clear(Color::Black());
    
    // Sprite moves 0.3 pixels per frame - NO stuttering!
    gpu.blitSpriteF(0, spriteX, 12.5f);
    spriteX += 0.3f;
    
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(16));
}

// Smooth line animation
float angle = 0.0f;
while (true) {
    gpu.clear(Color::Black());
    
    // Rotating line with smooth edges
    float endX = 64.0f + cosf(angle) * 30.0f;
    float endY = 16.0f + sinf(angle) * 14.0f;
    gpu.drawLineF(64.0f, 16.0f, endX, endY, Color::Cyan());
    
    angle += 0.02f;
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(16));
}
```

### Disabling Weighted Pixels

If you need hard pixel edges or want to save GPU processing:

```cpp
// Option 1: Disable via config
GpuConfig config;
config.weightedPixels = false;
gpu.init(config);

// Option 2: Toggle at runtime
gpu.setWeightedPixels(false);  // Hard edges
gpu.drawLine(10, 5, 100, 28, Color::Red());  // Aliased line

gpu.setWeightedPixels(true);   // Smooth edges
gpu.drawLine(10, 5, 100, 28, Color::Red());  // Anti-aliased line
```

---

## API Reference

### Configuration

```cpp
struct GpuConfig {
    // ... other fields ...
    bool weightedPixels = true;  // Anti-aliasing enabled by default
};
```

### Runtime Control

```cpp
// Enable/disable anti-aliasing
void setWeightedPixels(bool enabled);
bool getWeightedPixels() const;
```

### Float Drawing Methods

All float methods (`*F` suffix) use weighted pixel rendering regardless of the `weightedPixels` setting:

```cpp
// Lines (anti-aliased)
void drawLineF(float x1, float y1, float x2, float y2, uint8_t r, uint8_t g, uint8_t b);
void drawLineF(float x1, float y1, float x2, float y2, const Color& c);

// Rectangles (anti-aliased outline)
void drawRectF(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b);
void drawRectF(float x, float y, float w, float h, const Color& c);

// Circles (anti-aliased)
void drawCircleF(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b);
void drawCircleF(float cx, float cy, float radius, const Color& c);

// Filled rectangles (anti-aliased edges)
void drawFilledRectF(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b);
void drawFilledRectF(float x, float y, float w, float h, const Color& c);

// Sprites (sub-pixel positioning for smooth movement)
void blitSpriteF(uint8_t spriteId, float x, float y);

// Rotated sprites (transformation matrix + bilinear interpolation)
void blitSpriteRotated(uint8_t spriteId, float x, float y, float angleDegrees);
```

### GPU-Side Anti-Aliasing Control

The GPU maintains its own `aa_enabled` flag. You can toggle it:

```cpp
// Via setWeightedPixels (recommended - syncs both CPU and GPU)
gpu.setWeightedPixels(true);   // Enable AA on GPU
gpu.setWeightedPixels(false);  // Disable AA on GPU

// Direct GPU control (low-level)
gpu.setAntiAliasing(true);
gpu.setAntiAliasing(false);
```

When AA is enabled on the GPU:
- **Lines** use Wu's anti-aliasing algorithm
- **Circles** use distance-based edge blending
- **Filled rectangles** have smooth edges
- **Sprite rotation** uses bilinear interpolation (smooth)

When AA is disabled:
- All drawing uses nearest-neighbor (hard pixels)
- Sprite rotation is jagged but faster

### GPU Commands (Wire Protocol)

| Command | Opcode | Description |
|---------|--------|-------------|
| `DRAW_LINE_F` | `0x48` | Anti-aliased line (8.8 coordinates) |
| `DRAW_CIRCLE_F` | `0x49` | Anti-aliased circle |
| `DRAW_RECT_F` | `0x4A` | Anti-aliased rectangle outline |
| `DRAW_FILL_F` | `0x4B` | Filled rectangle with AA edges |
| `BLIT_SPRITE_F` | `0x4C` | Sprite with sub-pixel position |
| `BLIT_SPRITE_ROT` | `0x4D` | Sprite with rotation angle |
| `SET_AA` | `0x4E` | Toggle GPU anti-aliasing (0=off, 1=on) |

---

## Sprite Rotation

### Overview

The `blitSpriteRotated` function renders a sprite rotated by any angle (not just 90° increments). This uses GPU-side transformation matrix math with optional bilinear interpolation for smooth edges.

### How It Works

1. **Inverse Mapping**: For each screen pixel, calculate where it maps back to in the sprite's coordinate space
2. **Transformation Matrix**: Apply 2D rotation matrix around sprite center
3. **Sampling**: 
   - AA ON: Bilinear interpolation (smooth)
   - AA OFF: Nearest neighbor (fast, jagged)
4. **Edge AA**: Optional alpha blending at sprite boundaries

### Example: Rotating Triangle

```cpp
// Create a 16x16 triangle sprite on CPU
uint8_t triangleData[16 * 16 * 3];
// ... fill with triangle pixels ...

// Upload to GPU
gpu.uploadSprite(0, 16, 16, triangleData, SpriteFormat::RGB888);

// Rotate smoothly in animation loop
float angle = 0.0f;
while (true) {
    gpu.clear(Color::Black());
    
    // Draw sprite rotated at center of screen
    gpu.blitSpriteRotated(0, 64.0f, 16.0f, angle);
    
    angle += 2.0f;  // 2 degrees per frame
    if (angle >= 360.0f) angle -= 360.0f;
    
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(16));
}
```

### Performance Notes

- Rotation is computed per-pixel on the GPU
- Bounding box optimization limits computation to rotated sprite area
- Bilinear interpolation (AA on) adds ~2x computation but looks much smoother
- For performance-critical code, consider pre-rotating sprites at compile time

---

## Performance Considerations

### GPU Processing

Weighted pixel rendering requires more GPU computation:
- **Lines**: ~2-3x more pixel calculations (coverage math per pixel)
- **Circles**: ~2x more calculations
- **Sprites**: Bilinear interpolation for sub-pixel positions

The ESP32-S3 GPU handles this comfortably at 60 FPS for typical scenes.

### When to Disable

Consider disabling weighted pixels for:
- **Retro/pixel art** styles where hard edges are desired
- **Text rendering** where sharpness matters more than smoothness
- **Static UI elements** that don't animate
- **Battery-constrained** scenarios

### When to Use (Default)

Keep weighted pixels enabled for:
- **Moving sprites** (games, animations)
- **Rotating/scaling** graphics
- **Slow movement** (< 1 pixel per frame)
- **Diagonal lines** and curves
- **Smooth visual quality**

---

## Technical Details

### Fixed-Point Conversion

```cpp
// Convert float to 8.8 fixed-point
void floatToFixed88(float val, uint8_t* outFrac, uint8_t* outInt) {
    int8_t intPart = (int8_t)val;
    float fracPart = val - (float)intPart;
    if (fracPart < 0) fracPart += 1.0f;  // Handle negatives
    
    *outFrac = (uint8_t)(fracPart * 256.0f);
    *outInt = (uint8_t)intPart;
}
```

### Wire Format Examples

**Line from (10.5, 5.25) to (100.75, 28.0):**
```
Command: 0x48 (DRAW_LINE_F)
Payload: [
    0x80, 0x0A,  // X1: 10.5 (frac=128, int=10)
    0x40, 0x05,  // Y1: 5.25 (frac=64, int=5)
    0xC0, 0x64,  // X2: 100.75 (frac=192, int=100)
    0x00, 0x1C,  // Y2: 28.0 (frac=0, int=28)
    0xFF, 0x00, 0x00  // Color: Red
]
```

---

## Comparison

### Without Weighted Pixels
![Without AA](docs/images/no_aa.png)
- Hard pixel edges
- Visible staircase patterns
- Stuttery sprite movement

### With Weighted Pixels
![With AA](docs/images/with_aa.png)
- Smooth edges
- Gradual pixel transitions
- Fluid sprite movement

---

## See Also

- [GPU_COMMAND_REFERENCE.md](GPU_COMMAND_REFERENCE.md) - Full command protocol
- [API_INDEX.md](API_INDEX.md) - API documentation index
- [GpuDriver.h](src/SystemAPI/GPU/GpuDriver.h) - Driver header

---

*Document generated for ARCOS GPU Protocol v1.0*
