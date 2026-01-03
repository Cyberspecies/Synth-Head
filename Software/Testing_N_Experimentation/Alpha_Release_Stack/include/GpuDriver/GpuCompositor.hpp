/*****************************************************************
 * File:      GpuCompositor.hpp
 * Category:  GPU Driver / Compositing Pipeline
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Framebuffer-level compositing for multi-layer rendering.
 *    Handles alpha blending, color space management, layer effects,
 *    and final output conversion for display targets.
 * 
 * Features:
 *    - Multiple render layers with blend modes
 *    - Alpha handling (premultiplied, straight)
 *    - Color space conversion (RGB, sRGB, linear)
 *    - Layer effects (opacity, tint, transform)
 *    - Multi-pass compositing pipeline
 *    - Output dithering for low bit-depth displays
 *****************************************************************/

#ifndef GPU_COMPOSITOR_HPP_
#define GPU_COMPOSITOR_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "GpuISA.hpp"

namespace gpu {
namespace compositor {

using namespace isa;

// ============================================================
// Compositing Constants
// ============================================================

constexpr int MAX_LAYERS = 32;
constexpr int MAX_PASSES = 8;

// ============================================================
// Color Space Management
// ============================================================

enum class ColorSpace : uint8_t {
  LINEAR_RGB = 0,  // Linear RGB (physical light values)
  SRGB       = 1,  // sRGB (standard for displays)
  GAMMA_22   = 2,  // Gamma 2.2
  REC709     = 3,  // Rec. 709 (HDTV)
};

// Alpha mode
enum class AlphaMode : uint8_t {
  STRAIGHT      = 0,  // Color stored separately from alpha
  PREMULTIPLIED = 1,  // Color pre-multiplied by alpha
};

// sRGB <-> Linear conversion
inline float srgbToLinear(float c) {
  if (c <= 0.04045f) {
    return c / 12.92f;
  } else {
    return powf((c + 0.055f) / 1.055f, 2.4f);
  }
}

inline float linearToSrgb(float c) {
  if (c <= 0.0031308f) {
    return c * 12.92f;
  } else {
    return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
  }
}

// Gamma conversion
inline float gammaToLinear(float c, float gamma) {
  return powf(c, gamma);
}

inline float linearToGamma(float c, float gamma) {
  return powf(c, 1.0f / gamma);
}

// Color conversion struct
class ColorConversion {
public:
  static ColorF toLinear(const ColorF& c, ColorSpace from) {
    switch (from) {
      case ColorSpace::SRGB:
        return ColorF(srgbToLinear(c.r), srgbToLinear(c.g), 
                      srgbToLinear(c.b), c.a);
      case ColorSpace::GAMMA_22:
        return ColorF(gammaToLinear(c.r, 2.2f), gammaToLinear(c.g, 2.2f),
                      gammaToLinear(c.b, 2.2f), c.a);
      case ColorSpace::REC709:
        // Simplified - Rec709 has similar curve to sRGB
        return ColorF(srgbToLinear(c.r), srgbToLinear(c.g), 
                      srgbToLinear(c.b), c.a);
      case ColorSpace::LINEAR_RGB:
      default:
        return c;
    }
  }
  
  static ColorF fromLinear(const ColorF& c, ColorSpace to) {
    switch (to) {
      case ColorSpace::SRGB:
        return ColorF(linearToSrgb(c.r), linearToSrgb(c.g),
                      linearToSrgb(c.b), c.a);
      case ColorSpace::GAMMA_22:
        return ColorF(linearToGamma(c.r, 2.2f), linearToGamma(c.g, 2.2f),
                      linearToGamma(c.b, 2.2f), c.a);
      case ColorSpace::REC709:
        return ColorF(linearToSrgb(c.r), linearToSrgb(c.g),
                      linearToSrgb(c.b), c.a);
      case ColorSpace::LINEAR_RGB:
      default:
        return c;
    }
  }
  
  // Convert between color spaces
  static ColorF convert(const ColorF& c, ColorSpace from, ColorSpace to) {
    if (from == to) return c;
    ColorF linear = toLinear(c, from);
    return fromLinear(linear, to);
  }
  
  // Convert alpha mode
  static ColorF toPremultiplied(const ColorF& c) {
    return ColorF(c.r * c.a, c.g * c.a, c.b * c.a, c.a);
  }
  
  static ColorF toStraight(const ColorF& c) {
    if (c.a < 0.001f) return ColorF(0, 0, 0, 0);
    return ColorF(c.r / c.a, c.g / c.a, c.b / c.a, c.a);
  }
};

// ============================================================
// Porter-Duff Compositing Operations
// ============================================================

enum class CompositeOp : uint8_t {
  CLEAR      = 0x00,   // Clear destination
  SRC        = 0x01,   // Source only
  DST        = 0x02,   // Destination only
  SRC_OVER   = 0x03,   // Source over destination (standard alpha blend)
  DST_OVER   = 0x04,   // Destination over source
  SRC_IN     = 0x05,   // Source where destination alpha
  DST_IN     = 0x06,   // Destination where source alpha
  SRC_OUT    = 0x07,   // Source where not destination alpha
  DST_OUT    = 0x08,   // Destination where not source alpha
  SRC_ATOP   = 0x09,   // Source atop destination
  DST_ATOP   = 0x0A,   // Destination atop source
  XOR        = 0x0B,   // Source XOR destination
  PLUS       = 0x0C,   // Additive (saturate)
  
  // Extended blend modes
  MULTIPLY   = 0x10,
  SCREEN     = 0x11,
  OVERLAY    = 0x12,
  DARKEN     = 0x13,
  LIGHTEN    = 0x14,
  COLOR_DODGE = 0x15,
  COLOR_BURN  = 0x16,
  HARD_LIGHT  = 0x17,
  SOFT_LIGHT  = 0x18,
  DIFFERENCE  = 0x19,
  EXCLUSION   = 0x1A,
};

// Porter-Duff compositing (assumes premultiplied alpha)
class PorterDuff {
public:
  static ColorF composite(const ColorF& dst, const ColorF& src, CompositeOp op) {
    switch (op) {
      case CompositeOp::CLEAR:
        return ColorF(0, 0, 0, 0);
        
      case CompositeOp::SRC:
        return src;
        
      case CompositeOp::DST:
        return dst;
        
      case CompositeOp::SRC_OVER: {
        float oa = src.a + dst.a * (1.0f - src.a);
        if (oa < 0.001f) return ColorF(0, 0, 0, 0);
        return ColorF(
          src.r + dst.r * (1.0f - src.a),
          src.g + dst.g * (1.0f - src.a),
          src.b + dst.b * (1.0f - src.a),
          oa
        );
      }
      
      case CompositeOp::DST_OVER: {
        float oa = dst.a + src.a * (1.0f - dst.a);
        if (oa < 0.001f) return ColorF(0, 0, 0, 0);
        return ColorF(
          dst.r + src.r * (1.0f - dst.a),
          dst.g + src.g * (1.0f - dst.a),
          dst.b + src.b * (1.0f - dst.a),
          oa
        );
      }
      
      case CompositeOp::SRC_IN:
        return ColorF(src.r * dst.a, src.g * dst.a, src.b * dst.a, src.a * dst.a);
        
      case CompositeOp::DST_IN:
        return ColorF(dst.r * src.a, dst.g * src.a, dst.b * src.a, dst.a * src.a);
        
      case CompositeOp::SRC_OUT:
        return ColorF(src.r * (1.0f - dst.a), src.g * (1.0f - dst.a),
                      src.b * (1.0f - dst.a), src.a * (1.0f - dst.a));
        
      case CompositeOp::DST_OUT:
        return ColorF(dst.r * (1.0f - src.a), dst.g * (1.0f - src.a),
                      dst.b * (1.0f - src.a), dst.a * (1.0f - src.a));
        
      case CompositeOp::SRC_ATOP:
        return ColorF(
          src.r * dst.a + dst.r * (1.0f - src.a),
          src.g * dst.a + dst.g * (1.0f - src.a),
          src.b * dst.a + dst.b * (1.0f - src.a),
          dst.a
        );
        
      case CompositeOp::DST_ATOP:
        return ColorF(
          dst.r * src.a + src.r * (1.0f - dst.a),
          dst.g * src.a + src.g * (1.0f - dst.a),
          dst.b * src.a + src.b * (1.0f - dst.a),
          src.a
        );
        
      case CompositeOp::XOR:
        return ColorF(
          src.r * (1.0f - dst.a) + dst.r * (1.0f - src.a),
          src.g * (1.0f - dst.a) + dst.g * (1.0f - src.a),
          src.b * (1.0f - dst.a) + dst.b * (1.0f - src.a),
          src.a + dst.a - 2.0f * src.a * dst.a
        );
        
      case CompositeOp::PLUS:
        return ColorF(
          fminf(1.0f, src.r + dst.r),
          fminf(1.0f, src.g + dst.g),
          fminf(1.0f, src.b + dst.b),
          fminf(1.0f, src.a + dst.a)
        );
        
      // Extended blend modes (use separable blend)
      case CompositeOp::MULTIPLY:
      case CompositeOp::SCREEN:
      case CompositeOp::OVERLAY:
      case CompositeOp::DARKEN:
      case CompositeOp::LIGHTEN:
      case CompositeOp::COLOR_DODGE:
      case CompositeOp::COLOR_BURN:
      case CompositeOp::HARD_LIGHT:
      case CompositeOp::SOFT_LIGHT:
      case CompositeOp::DIFFERENCE:
      case CompositeOp::EXCLUSION:
        return blendExtended(dst, src, op);
        
      default:
        return src;
    }
  }
  
private:
  static float blendChannel(float dst, float src, CompositeOp op) {
    switch (op) {
      case CompositeOp::MULTIPLY:
        return dst * src;
      case CompositeOp::SCREEN:
        return 1.0f - (1.0f - dst) * (1.0f - src);
      case CompositeOp::OVERLAY:
        return dst < 0.5f ? 2.0f * dst * src : 1.0f - 2.0f * (1.0f - dst) * (1.0f - src);
      case CompositeOp::DARKEN:
        return fminf(dst, src);
      case CompositeOp::LIGHTEN:
        return fmaxf(dst, src);
      case CompositeOp::COLOR_DODGE:
        return src >= 1.0f ? 1.0f : fminf(1.0f, dst / (1.0f - src));
      case CompositeOp::COLOR_BURN:
        return src <= 0.0f ? 0.0f : fmaxf(0.0f, 1.0f - (1.0f - dst) / src);
      case CompositeOp::HARD_LIGHT:
        return src < 0.5f ? 2.0f * dst * src : 1.0f - 2.0f * (1.0f - dst) * (1.0f - src);
      case CompositeOp::SOFT_LIGHT:
        return src < 0.5f 
          ? dst - (1.0f - 2.0f * src) * dst * (1.0f - dst)
          : dst + (2.0f * src - 1.0f) * (dst < 0.25f 
              ? ((16.0f * dst - 12.0f) * dst + 4.0f) * dst 
              : sqrtf(dst) - dst);
      case CompositeOp::DIFFERENCE:
        return fabsf(dst - src);
      case CompositeOp::EXCLUSION:
        return dst + src - 2.0f * dst * src;
      default:
        return src;
    }
  }
  
  static ColorF blendExtended(const ColorF& dst, const ColorF& src, CompositeOp op) {
    // Convert from premultiplied to straight for blending
    ColorF dst_s = dst.a > 0.001f ? ColorConversion::toStraight(dst) : ColorF(0,0,0,0);
    ColorF src_s = src.a > 0.001f ? ColorConversion::toStraight(src) : ColorF(0,0,0,0);
    
    // Apply blend mode
    ColorF blended(
      blendChannel(dst_s.r, src_s.r, op),
      blendChannel(dst_s.g, src_s.g, op),
      blendChannel(dst_s.b, src_s.b, op),
      1.0f
    );
    
    // Composite with alpha: result = (1 - src.a) * dst + src.a * blend
    float out_a = src.a + dst.a * (1.0f - src.a);
    if (out_a < 0.001f) return ColorF(0, 0, 0, 0);
    
    return ColorF(
      ((1.0f - src.a) * dst.r + src.a * blended.r * dst.a) / out_a,
      ((1.0f - src.a) * dst.g + src.a * blended.g * dst.a) / out_a,
      ((1.0f - src.a) * dst.b + src.a * blended.b * dst.a) / out_a,
      out_a
    );
  }
};

// ============================================================
// Layer Structure
// ============================================================

struct Layer {
  // Buffer info
  uint8_t* buffer;        // RGBA buffer (4 bytes per pixel)
  int width;
  int height;
  int stride;
  
  // Transform
  float offset_x;
  float offset_y;
  float scale_x;
  float scale_y;
  float rotation;         // Degrees
  
  // Effects
  float opacity;          // 0-1
  ColorF tint;            // Color multiply
  CompositeOp blend_op;
  bool visible;
  bool premultiplied;     // Buffer alpha mode
  ColorSpace color_space;
  
  // Mask (optional)
  uint8_t* mask_buffer;   // Single channel mask
  int mask_width;
  int mask_height;
  
  Layer()
    : buffer(nullptr), width(0), height(0), stride(0),
      offset_x(0), offset_y(0), scale_x(1.0f), scale_y(1.0f), rotation(0),
      opacity(1.0f), tint(1, 1, 1, 1), blend_op(CompositeOp::SRC_OVER),
      visible(true), premultiplied(true), color_space(ColorSpace::SRGB),
      mask_buffer(nullptr), mask_width(0), mask_height(0) {}
  
  // Sample pixel from layer with transform
  ColorF samplePixel(float x, float y) const {
    if (!buffer || !visible) return ColorF(0, 0, 0, 0);
    
    // Apply inverse transform
    float px = x - offset_x;
    float py = y - offset_y;
    
    // Scale
    px /= scale_x;
    py /= scale_y;
    
    // Rotation (around layer center)
    if (rotation != 0.0f) {
      float cx = width * 0.5f;
      float cy = height * 0.5f;
      float rad = -rotation * 3.14159265f / 180.0f;
      float cos_r = cosf(rad);
      float sin_r = sinf(rad);
      float rx = px - cx;
      float ry = py - cy;
      px = rx * cos_r - ry * sin_r + cx;
      py = rx * sin_r + ry * cos_r + cy;
    }
    
    // Bounds check
    if (px < 0 || px >= width || py < 0 || py >= height) {
      return ColorF(0, 0, 0, 0);
    }
    
    // Bilinear sample
    int ix = (int)px;
    int iy = (int)py;
    float fx = px - ix;
    float fy = py - iy;
    
    auto sample = [this](int x, int y) -> ColorF {
      if (x < 0 || x >= width || y < 0 || y >= height) {
        return ColorF(0, 0, 0, 0);
      }
      uint8_t* p = buffer + y * stride + x * 4;
      return ColorF::fromRGBA(p[0], p[1], p[2], p[3]);
    };
    
    ColorF c00 = sample(ix, iy);
    ColorF c10 = sample(ix + 1, iy);
    ColorF c01 = sample(ix, iy + 1);
    ColorF c11 = sample(ix + 1, iy + 1);
    
    ColorF c0 = c00.lerp(c10, fx);
    ColorF c1 = c01.lerp(c11, fx);
    ColorF result = c0.lerp(c1, fy);
    
    // Apply effects
    result.r *= tint.r;
    result.g *= tint.g;
    result.b *= tint.b;
    result.a *= opacity;
    
    // Apply mask
    if (mask_buffer && mask_width > 0 && mask_height > 0) {
      float mx = px * mask_width / width;
      float my = py * mask_height / height;
      int mix = (int)fminf(fmaxf(mx, 0), mask_width - 1);
      int miy = (int)fminf(fmaxf(my, 0), mask_height - 1);
      float mask_val = mask_buffer[miy * mask_width + mix] / 255.0f;
      result.a *= mask_val;
    }
    
    return result;
  }
};

// ============================================================
// Framebuffer
// ============================================================

class Framebuffer {
public:
  Framebuffer() : buffer_(nullptr), width_(0), height_(0), stride_(0),
                  owns_buffer_(false), color_space_(ColorSpace::SRGB),
                  alpha_mode_(AlphaMode::PREMULTIPLIED) {}
  
  ~Framebuffer() {
    if (owns_buffer_ && buffer_) {
      delete[] buffer_;
    }
  }
  
  // Allocate internal buffer
  bool allocate(int width, int height) {
    if (owns_buffer_ && buffer_) {
      delete[] buffer_;
    }
    
    width_ = width;
    height_ = height;
    stride_ = width * 4;
    buffer_ = new uint8_t[stride_ * height];
    owns_buffer_ = true;
    
    clear();
    return buffer_ != nullptr;
  }
  
  // Use external buffer
  void setBuffer(uint8_t* buffer, int width, int height, int stride = 0) {
    if (owns_buffer_ && buffer_) {
      delete[] buffer_;
    }
    
    buffer_ = buffer;
    width_ = width;
    height_ = height;
    stride_ = stride > 0 ? stride : width * 4;
    owns_buffer_ = false;
  }
  
  // Clear to color
  void clear(const ColorF& color = ColorF(0, 0, 0, 0)) {
    if (!buffer_) return;
    
    uint8_t r = color.r8();
    uint8_t g = color.g8();
    uint8_t b = color.b8();
    uint8_t a = color.a8();
    
    for (int y = 0; y < height_; y++) {
      uint8_t* row = buffer_ + y * stride_;
      for (int x = 0; x < width_; x++) {
        row[x * 4 + 0] = r;
        row[x * 4 + 1] = g;
        row[x * 4 + 2] = b;
        row[x * 4 + 3] = a;
      }
    }
  }
  
  // Get/set pixel
  ColorF getPixel(int x, int y) const {
    if (!buffer_ || x < 0 || x >= width_ || y < 0 || y >= height_) {
      return ColorF(0, 0, 0, 0);
    }
    uint8_t* p = buffer_ + y * stride_ + x * 4;
    return ColorF::fromRGBA(p[0], p[1], p[2], p[3]);
  }
  
  void setPixel(int x, int y, const ColorF& color) {
    if (!buffer_ || x < 0 || x >= width_ || y < 0 || y >= height_) return;
    uint8_t* p = buffer_ + y * stride_ + x * 4;
    p[0] = color.r8();
    p[1] = color.g8();
    p[2] = color.b8();
    p[3] = color.a8();
  }
  
  // Composite a layer onto this framebuffer
  void compositeLayer(const Layer& layer) {
    if (!buffer_ || !layer.buffer || !layer.visible) return;
    
    // Compute affected region
    int min_x = (int)fmaxf(0, layer.offset_x);
    int min_y = (int)fmaxf(0, layer.offset_y);
    int max_x = (int)fminf(width_, layer.offset_x + layer.width * layer.scale_x);
    int max_y = (int)fminf(height_, layer.offset_y + layer.height * layer.scale_y);
    
    for (int y = min_y; y < max_y; y++) {
      for (int x = min_x; x < max_x; x++) {
        ColorF dst = getPixel(x, y);
        ColorF src = layer.samplePixel((float)x, (float)y);
        
        // Convert to same color space if needed
        if (layer.color_space != color_space_) {
          src = ColorConversion::convert(src, layer.color_space, color_space_);
        }
        
        // Convert alpha mode
        if (!layer.premultiplied && alpha_mode_ == AlphaMode::PREMULTIPLIED) {
          src = ColorConversion::toPremultiplied(src);
        }
        
        // Composite
        ColorF result = PorterDuff::composite(dst, src, layer.blend_op);
        setPixel(x, y, result);
      }
    }
  }
  
  // Accessors
  uint8_t* getBuffer() const { return buffer_; }
  int getWidth() const { return width_; }
  int getHeight() const { return height_; }
  int getStride() const { return stride_; }
  ColorSpace getColorSpace() const { return color_space_; }
  void setColorSpace(ColorSpace cs) { color_space_ = cs; }
  AlphaMode getAlphaMode() const { return alpha_mode_; }
  void setAlphaMode(AlphaMode m) { alpha_mode_ = m; }

private:
  uint8_t* buffer_;
  int width_;
  int height_;
  int stride_;
  bool owns_buffer_;
  ColorSpace color_space_;
  AlphaMode alpha_mode_;
};

// ============================================================
// Output Dithering
// ============================================================

enum class DitherPattern : uint8_t {
  NONE        = 0,
  ORDERED_2X2 = 1,
  ORDERED_4X4 = 2,
  ORDERED_8X8 = 3,
  BAYER_2X2   = 4,
  BAYER_4X4   = 5,
  FLOYD_STEINBERG = 6,
};

class Dithering {
public:
  // Apply ordered dithering for RGB output
  static void ditherOrdered(const Framebuffer& src, uint8_t* dst, 
                            int bits_r, int bits_g, int bits_b,
                            DitherPattern pattern) {
    const int width = src.getWidth();
    const int height = src.getHeight();
    
    // Bayer matrix 4x4
    static const int bayer4x4[16] = {
       0,  8,  2, 10,
      12,  4, 14,  6,
       3, 11,  1,  9,
      15,  7, 13,  5
    };
    
    // Quantization levels
    int levels_r = (1 << bits_r) - 1;
    int levels_g = (1 << bits_g) - 1;
    int levels_b = (1 << bits_b) - 1;
    
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        ColorF pixel = src.getPixel(x, y);
        
        // Get dither threshold (0-15 for 4x4)
        int dither_idx = (y % 4) * 4 + (x % 4);
        float threshold = (bayer4x4[dither_idx] + 0.5f) / 16.0f - 0.5f;
        
        // Apply dithering
        float dr = pixel.r + threshold / levels_r;
        float dg = pixel.g + threshold / levels_g;
        float db = pixel.b + threshold / levels_b;
        
        // Quantize
        int qr = (int)(fminf(fmaxf(dr, 0.0f), 1.0f) * levels_r + 0.5f);
        int qg = (int)(fminf(fmaxf(dg, 0.0f), 1.0f) * levels_g + 0.5f);
        int qb = (int)(fminf(fmaxf(db, 0.0f), 1.0f) * levels_b + 0.5f);
        
        // Scale back to 8-bit
        dst[(y * width + x) * 3 + 0] = (qr * 255) / levels_r;
        dst[(y * width + x) * 3 + 1] = (qg * 255) / levels_g;
        dst[(y * width + x) * 3 + 2] = (qb * 255) / levels_b;
      }
    }
  }
  
  // Convert to RGB565 with dithering
  static void ditherToRGB565(const Framebuffer& src, uint16_t* dst,
                             DitherPattern pattern = DitherPattern::BAYER_4X4) {
    const int width = src.getWidth();
    const int height = src.getHeight();
    
    // Bayer matrix 4x4
    static const int bayer4x4[16] = {
       0,  8,  2, 10,
      12,  4, 14,  6,
       3, 11,  1,  9,
      15,  7, 13,  5
    };
    
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        ColorF pixel = src.getPixel(x, y);
        
        // Get dither threshold
        int dither_idx = (y % 4) * 4 + (x % 4);
        float threshold = (bayer4x4[dither_idx] + 0.5f) / 16.0f - 0.5f;
        
        // Apply dithering and quantize to 5/6/5 bits
        float dr = pixel.r + threshold / 31.0f;
        float dg = pixel.g + threshold / 63.0f;
        float db = pixel.b + threshold / 31.0f;
        
        int r5 = (int)(fminf(fmaxf(dr, 0.0f), 1.0f) * 31.0f + 0.5f);
        int g6 = (int)(fminf(fmaxf(dg, 0.0f), 1.0f) * 63.0f + 0.5f);
        int b5 = (int)(fminf(fmaxf(db, 0.0f), 1.0f) * 31.0f + 0.5f);
        
        dst[y * width + x] = (r5 << 11) | (g6 << 5) | b5;
      }
    }
  }
};

// ============================================================
// Compositing Pipeline
// ============================================================

struct CompositePass {
  enum class Type : uint8_t {
    COMPOSITE_LAYERS,  // Composite layers onto target
    EFFECT,            // Apply effect
    COPY,              // Copy source to target
    CONVERT,           // Color space conversion
  };
  
  Type type;
  int source_fb;       // Source framebuffer index (-1 for layers)
  int target_fb;       // Target framebuffer index
  int layer_start;     // First layer index
  int layer_count;     // Number of layers
  ColorSpace convert_to;  // For CONVERT type
  
  CompositePass()
    : type(Type::COMPOSITE_LAYERS), source_fb(-1), target_fb(0),
      layer_start(0), layer_count(0), convert_to(ColorSpace::SRGB) {}
};

class Compositor {
public:
  Compositor() : layer_count_(0), pass_count_(0), active_fb_(0) {
    memset(layers_, 0, sizeof(layers_));
    memset(passes_, 0, sizeof(passes_));
  }
  
  // Layer management
  int addLayer() {
    if (layer_count_ >= MAX_LAYERS) return -1;
    layers_[layer_count_] = Layer();
    return layer_count_++;
  }
  
  Layer* getLayer(int index) {
    if (index < 0 || index >= layer_count_) return nullptr;
    return &layers_[index];
  }
  
  int getLayerCount() const { return layer_count_; }
  
  // Pass management
  int addPass(const CompositePass& pass) {
    if (pass_count_ >= MAX_PASSES) return -1;
    passes_[pass_count_] = pass;
    return pass_count_++;
  }
  
  void clearPasses() { pass_count_ = 0; }
  
  // Framebuffer management
  Framebuffer* getFramebuffer(int index) {
    if (index < 0 || index >= MAX_PASSES) return nullptr;
    return &framebuffers_[index];
  }
  
  void setActiveFramebuffer(int index) {
    if (index >= 0 && index < MAX_PASSES) {
      active_fb_ = index;
    }
  }
  
  Framebuffer* getActiveFramebuffer() {
    return &framebuffers_[active_fb_];
  }
  
  // Execute compositing pipeline
  void execute() {
    for (int i = 0; i < pass_count_; i++) {
      executePass(passes_[i]);
    }
  }
  
  // Simple composite: all layers onto framebuffer 0
  void compositeAll() {
    Framebuffer* target = &framebuffers_[0];
    if (!target->getBuffer()) return;
    
    target->clear();
    
    for (int i = 0; i < layer_count_; i++) {
      target->compositeLayer(layers_[i]);
    }
  }
  
  // Output to RGB888 buffer
  void outputToRGB(uint8_t* dst, int width, int height) {
    Framebuffer* src = getActiveFramebuffer();
    if (!src->getBuffer()) return;
    
    // Simple copy (could add scaling, conversion, etc.)
    int copy_w = (width < src->getWidth()) ? width : src->getWidth();
    int copy_h = (height < src->getHeight()) ? height : src->getHeight();
    
    for (int y = 0; y < copy_h; y++) {
      for (int x = 0; x < copy_w; x++) {
        ColorF pixel = src->getPixel(x, y);
        
        // Convert from linear to sRGB if needed
        if (src->getColorSpace() == ColorSpace::LINEAR_RGB) {
          pixel = ColorConversion::fromLinear(pixel, ColorSpace::SRGB);
        }
        
        dst[(y * width + x) * 3 + 0] = pixel.r8();
        dst[(y * width + x) * 3 + 1] = pixel.g8();
        dst[(y * width + x) * 3 + 2] = pixel.b8();
      }
    }
  }
  
  // Output to RGB565 buffer with dithering
  void outputToRGB565(uint16_t* dst, int width, int height, bool dither = true) {
    Framebuffer* src = getActiveFramebuffer();
    if (!src->getBuffer()) return;
    
    if (dither) {
      Dithering::ditherToRGB565(*src, dst);
    } else {
      int copy_w = (width < src->getWidth()) ? width : src->getWidth();
      int copy_h = (height < src->getHeight()) ? height : src->getHeight();
      
      for (int y = 0; y < copy_h; y++) {
        for (int x = 0; x < copy_w; x++) {
          ColorF pixel = src->getPixel(x, y);
          int r5 = (int)(pixel.r * 31.0f);
          int g6 = (int)(pixel.g * 63.0f);
          int b5 = (int)(pixel.b * 31.0f);
          dst[y * width + x] = (r5 << 11) | (g6 << 5) | b5;
        }
      }
    }
  }

private:
  void executePass(const CompositePass& pass) {
    Framebuffer* target = &framebuffers_[pass.target_fb];
    
    switch (pass.type) {
      case CompositePass::Type::COMPOSITE_LAYERS:
        for (int i = pass.layer_start; i < pass.layer_start + pass.layer_count; i++) {
          if (i >= 0 && i < layer_count_) {
            target->compositeLayer(layers_[i]);
          }
        }
        break;
        
      case CompositePass::Type::COPY:
        if (pass.source_fb >= 0 && pass.source_fb < MAX_PASSES) {
          Framebuffer* src = &framebuffers_[pass.source_fb];
          // Simple pixel copy
          for (int y = 0; y < target->getHeight() && y < src->getHeight(); y++) {
            for (int x = 0; x < target->getWidth() && x < src->getWidth(); x++) {
              target->setPixel(x, y, src->getPixel(x, y));
            }
          }
        }
        break;
        
      case CompositePass::Type::CONVERT:
        // Convert color space in place
        for (int y = 0; y < target->getHeight(); y++) {
          for (int x = 0; x < target->getWidth(); x++) {
            ColorF pixel = target->getPixel(x, y);
            pixel = ColorConversion::convert(pixel, target->getColorSpace(), pass.convert_to);
            target->setPixel(x, y, pixel);
          }
        }
        target->setColorSpace(pass.convert_to);
        break;
        
      case CompositePass::Type::EFFECT:
        // Effects would be applied here
        break;
    }
  }
  
  Layer layers_[MAX_LAYERS];
  int layer_count_;
  
  CompositePass passes_[MAX_PASSES];
  int pass_count_;
  
  Framebuffer framebuffers_[MAX_PASSES];
  int active_fb_;
};

} // namespace compositor
} // namespace gpu

#endif // GPU_COMPOSITOR_HPP_
