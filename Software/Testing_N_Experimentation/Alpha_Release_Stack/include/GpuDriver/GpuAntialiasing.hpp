/*****************************************************************
 * File:      GpuAntialiasing.hpp
 * Category:  GPU Driver / Antialiasing System
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Per-pixel coverage antialiasing system for smooth edge rendering.
 *    Computes exact sub-pixel coverage for primitives to scale pixel
 *    intensity, enabling smooth edges without post-process filtering.
 * 
 * Features:
 *    - Sub-pixel sampling patterns (2x2, 4x4, rotated grid, etc.)
 *    - Analytical coverage for common primitives
 *    - Coverage mask computation
 *    - Blending equations for proper compositing
 *    - SDF-based antialiasing integration
 *****************************************************************/

#ifndef GPU_ANTIALIASING_HPP_
#define GPU_ANTIALIASING_HPP_

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "GpuISA.hpp"

namespace gpu {
namespace aa {

using namespace isa;

// ============================================================
// Antialiasing Constants
// ============================================================

constexpr int MAX_SAMPLES = 16;

// ============================================================
// Sample Patterns
// ============================================================

enum class SamplePattern : uint8_t {
  NONE      = 0x00,  // No AA (single sample at center)
  GRID_2X2  = 0x01,  // 4 samples in 2x2 grid
  GRID_4X4  = 0x02,  // 16 samples in 4x4 grid
  ROTATED_GRID = 0x03,  // 4 samples in rotated grid (RGSS)
  QUINCUNX  = 0x04,  // 5 samples (center + corners)
  MSAA_4X   = 0x05,  // 4-sample MSAA pattern
  MSAA_8X   = 0x06,  // 8-sample MSAA pattern
  CUSTOM    = 0xFF,
};

// Sample point (sub-pixel offset from pixel center)
struct SamplePoint {
  float x;   // Offset from center (-0.5 to 0.5)
  float y;
  float weight;  // Sample weight (normalized)
};

// Get sample pattern
inline void getSamplePattern(SamplePattern pattern, SamplePoint* samples, int& count) {
  switch (pattern) {
    case SamplePattern::NONE:
      count = 1;
      samples[0] = {0.0f, 0.0f, 1.0f};
      break;
      
    case SamplePattern::GRID_2X2:
      count = 4;
      samples[0] = {-0.25f, -0.25f, 0.25f};
      samples[1] = { 0.25f, -0.25f, 0.25f};
      samples[2] = {-0.25f,  0.25f, 0.25f};
      samples[3] = { 0.25f,  0.25f, 0.25f};
      break;
      
    case SamplePattern::GRID_4X4:
      count = 16;
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
          int i = y * 4 + x;
          samples[i].x = -0.375f + x * 0.25f;
          samples[i].y = -0.375f + y * 0.25f;
          samples[i].weight = 1.0f / 16.0f;
        }
      }
      break;
      
    case SamplePattern::ROTATED_GRID:
      // RGSS - Rotated Grid Super-Sampling
      count = 4;
      samples[0] = {-0.125f, -0.375f, 0.25f};
      samples[1] = { 0.375f, -0.125f, 0.25f};
      samples[2] = {-0.375f,  0.125f, 0.25f};
      samples[3] = { 0.125f,  0.375f, 0.25f};
      break;
      
    case SamplePattern::QUINCUNX:
      count = 5;
      samples[0] = { 0.0f,   0.0f,  0.5f};   // Center (half weight)
      samples[1] = {-0.5f,  -0.5f,  0.125f}; // Corners (1/8 each)
      samples[2] = { 0.5f,  -0.5f,  0.125f};
      samples[3] = {-0.5f,   0.5f,  0.125f};
      samples[4] = { 0.5f,   0.5f,  0.125f};
      break;
      
    case SamplePattern::MSAA_4X:
      // Standard 4x MSAA positions
      count = 4;
      samples[0] = {-0.375f, -0.125f, 0.25f};
      samples[1] = { 0.125f, -0.375f, 0.25f};
      samples[2] = {-0.125f,  0.375f, 0.25f};
      samples[3] = { 0.375f,  0.125f, 0.25f};
      break;
      
    case SamplePattern::MSAA_8X:
      // Standard 8x MSAA positions
      count = 8;
      samples[0] = {-0.4375f, -0.3125f, 0.125f};
      samples[1] = {-0.1875f, -0.4375f, 0.125f};
      samples[2] = { 0.0625f, -0.1875f, 0.125f};
      samples[3] = { 0.3125f, -0.0625f, 0.125f};
      samples[4] = {-0.3125f,  0.0625f, 0.125f};
      samples[5] = {-0.0625f,  0.1875f, 0.125f};
      samples[6] = { 0.1875f,  0.3125f, 0.125f};
      samples[7] = { 0.4375f,  0.4375f, 0.125f};
      break;
      
    default:
      count = 1;
      samples[0] = {0.0f, 0.0f, 1.0f};
      break;
  }
}

// ============================================================
// Coverage Mask (16-bit for 4x4 grid)
// ============================================================

using CoverageMask = uint16_t;

constexpr CoverageMask COVERAGE_NONE = 0x0000;
constexpr CoverageMask COVERAGE_FULL = 0xFFFF;

// Count set bits in coverage mask
inline int coverageCount(CoverageMask mask) {
  int count = 0;
  while (mask) {
    count += mask & 1;
    mask >>= 1;
  }
  return count;
}

// Convert coverage mask to alpha (0-1)
inline float coverageToAlpha(CoverageMask mask, int total_samples = 16) {
  return (float)coverageCount(mask) / (float)total_samples;
}

// ============================================================
// Analytical Coverage Functions
// ============================================================

class AnalyticalCoverage {
public:
  // Line coverage (line from (x0,y0) to (x1,y1), pixel at (px,py))
  // Returns approximate coverage based on distance to line
  static float line(float px, float py, float x0, float y0, float x1, float y1, 
                    float line_width) {
    // Vector from start to end
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len_sq = dx * dx + dy * dy;
    
    if (len_sq < 0.0001f) {
      // Degenerate line - treat as point
      float dist = sqrtf((px - x0) * (px - x0) + (py - y0) * (py - y0));
      return smoothstep(line_width, 0.0f, dist);
    }
    
    // Project pixel onto line
    float t = ((px - x0) * dx + (py - y0) * dy) / len_sq;
    t = fmaxf(0.0f, fminf(1.0f, t));
    
    // Distance to nearest point on line segment
    float nearest_x = x0 + t * dx;
    float nearest_y = y0 + t * dy;
    float dist = sqrtf((px - nearest_x) * (px - nearest_x) + 
                       (py - nearest_y) * (py - nearest_y));
    
    // Coverage based on distance (with smooth falloff)
    float half_width = line_width * 0.5f;
    return smoothstep(half_width + 0.5f, half_width - 0.5f, dist);
  }
  
  // Circle coverage
  static float circle(float px, float py, float cx, float cy, float radius, bool filled) {
    float dist = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
    
    if (filled) {
      // Filled circle - coverage is 1 inside, smooth falloff at edge
      return smoothstep(radius + 0.5f, radius - 0.5f, dist);
    } else {
      // Circle outline (1 pixel thick)
      float inner = radius - 0.5f;
      float outer = radius + 0.5f;
      float outer_cov = smoothstep(outer + 0.5f, outer - 0.5f, dist);
      float inner_cov = smoothstep(inner + 0.5f, inner - 0.5f, dist);
      return outer_cov - inner_cov;
    }
  }
  
  // Rectangle coverage
  static float rectangle(float px, float py, float x, float y, float w, float h, bool filled) {
    // Distance to nearest edge
    float dx = fmaxf(fmaxf(x - px, px - (x + w)), 0.0f);
    float dy = fmaxf(fmaxf(y - py, py - (y + h)), 0.0f);
    
    if (filled) {
      // Inside rectangle?
      if (px >= x && px <= x + w && py >= y && py <= y + h) {
        // Distance to nearest edge (for AA at edges)
        float edge_dist = fminf(
          fminf(px - x, (x + w) - px),
          fminf(py - y, (y + h) - py)
        );
        return smoothstep(0.0f, 1.0f, edge_dist + 0.5f);
      } else {
        // Outside - use corner distance for smooth corners
        float dist = sqrtf(dx * dx + dy * dy);
        return smoothstep(0.5f, -0.5f, dist);
      }
    } else {
      // Rectangle outline
      // Check if near any edge
      float near_left   = smoothstep(1.0f, 0.0f, fabsf(px - x));
      float near_right  = smoothstep(1.0f, 0.0f, fabsf(px - (x + w)));
      float near_top    = smoothstep(1.0f, 0.0f, fabsf(py - y));
      float near_bottom = smoothstep(1.0f, 0.0f, fabsf(py - (y + h)));
      
      float in_h = (px >= x - 0.5f && px <= x + w + 0.5f) ? 1.0f : 0.0f;
      float in_v = (py >= y - 0.5f && py <= y + h + 0.5f) ? 1.0f : 0.0f;
      
      return fminf(1.0f, 
        (near_left + near_right) * in_v + 
        (near_top + near_bottom) * in_h);
    }
  }
  
  // Triangle coverage using edge functions
  static float triangle(float px, float py,
                        float x0, float y0, float x1, float y1, float x2, float y2) {
    // Edge functions
    auto edge = [](float px, float py, float x0, float y0, float x1, float y1) {
      return (px - x0) * (y1 - y0) - (py - y0) * (x1 - x0);
    };
    
    float e0 = edge(px, py, x0, y0, x1, y1);
    float e1 = edge(px, py, x1, y1, x2, y2);
    float e2 = edge(px, py, x2, y2, x0, y0);
    
    // Determine winding
    float area = edge(x2, y2, x0, y0, x1, y1);
    if (area < 0) {
      e0 = -e0;
      e1 = -e1;
      e2 = -e2;
    }
    
    // Inside test with antialiasing
    float min_e = fminf(fminf(e0, e1), e2);
    
    // Compute edge gradients for proper AA
    float len0 = sqrtf((y1 - y0) * (y1 - y0) + (x1 - x0) * (x1 - x0));
    float len1 = sqrtf((y2 - y1) * (y2 - y1) + (x2 - x1) * (x2 - x1));
    float len2 = sqrtf((y0 - y2) * (y0 - y2) + (x0 - x2) * (x0 - x2));
    
    float norm_e0 = e0 / (len0 + 0.0001f);
    float norm_e1 = e1 / (len1 + 0.0001f);
    float norm_e2 = e2 / (len2 + 0.0001f);
    
    float min_norm = fminf(fminf(norm_e0, norm_e1), norm_e2);
    
    return smoothstep(-0.5f, 0.5f, min_norm);
  }

private:
  static float smoothstep(float edge0, float edge1, float x) {
    float t = fmaxf(0.0f, fminf(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
  }
};

// ============================================================
// Multi-Sample Coverage
// ============================================================

template<typename SDFEvaluator>
class MultiSampleCoverage {
public:
  MultiSampleCoverage(SamplePattern pattern = SamplePattern::GRID_4X4) {
    getSamplePattern(pattern, samples_, sample_count_);
  }
  
  // Evaluate coverage using SDF at multiple sample points
  float evaluate(float px, float py, const SDFEvaluator& sdf) const {
    float total_coverage = 0.0f;
    
    for (int i = 0; i < sample_count_; i++) {
      float sx = px + samples_[i].x;
      float sy = py + samples_[i].y;
      
      float d = sdf(sx, sy);
      
      // Sample is covered if inside (d <= 0)
      if (d <= 0.0f) {
        total_coverage += samples_[i].weight;
      }
    }
    
    return total_coverage;
  }
  
  // Evaluate with per-sample output (for coverage mask)
  CoverageMask evaluateMask(float px, float py, const SDFEvaluator& sdf) const {
    CoverageMask mask = 0;
    
    for (int i = 0; i < sample_count_; i++) {
      float sx = px + samples_[i].x;
      float sy = py + samples_[i].y;
      
      float d = sdf(sx, sy);
      
      if (d <= 0.0f) {
        mask |= (1 << i);
      }
    }
    
    return mask;
  }
  
  // Evaluate with weighted color (for color bleeding / anti-aliasing)
  ColorF evaluateColor(float px, float py, 
                       const SDFEvaluator& sdf,
                       const ColorF& fg_color,
                       const ColorF& bg_color) const {
    ColorF result(0, 0, 0, 0);
    
    for (int i = 0; i < sample_count_; i++) {
      float sx = px + samples_[i].x;
      float sy = py + samples_[i].y;
      
      float d = sdf(sx, sy);
      
      // Soft transition based on SDF distance
      float t = smoothstep(0.5f, -0.5f, d);
      
      ColorF sample_color = bg_color.lerp(fg_color, t);
      result = result + sample_color * samples_[i].weight;
    }
    
    return result;
  }

private:
  SamplePoint samples_[MAX_SAMPLES];
  int sample_count_;
  
  static float smoothstep(float edge0, float edge1, float x) {
    float t = fmaxf(0.0f, fminf(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
  }
};

// ============================================================
// SDF-Based Antialiasing
// ============================================================

class SDFAntialiasing {
public:
  // Single-sample AA using SDF gradient
  static float coverage(float sdf_distance, float aa_width = 1.0f) {
    // Smooth coverage based on distance
    // aa_width controls the transition zone
    return smoothstep(aa_width * 0.5f, -aa_width * 0.5f, sdf_distance);
  }
  
  // Coverage with screen-space derivative compensation
  static float coverageScreenSpace(float sdf_distance, float dFdx, float dFdy) {
    // Compute gradient length for proper screen-space AA
    float grad_len = sqrtf(dFdx * dFdx + dFdy * dFdy);
    if (grad_len < 0.0001f) grad_len = 0.0001f;
    
    // Convert distance to screen pixels
    float screen_dist = sdf_distance / grad_len;
    
    return smoothstep(0.5f, -0.5f, screen_dist);
  }
  
  // Stroke antialiasing
  static float strokeCoverage(float sdf_distance, float stroke_width, float aa_width = 1.0f) {
    float half_stroke = stroke_width * 0.5f;
    
    // Distance to stroke edge
    float stroke_dist = fabsf(sdf_distance) - half_stroke;
    
    return smoothstep(aa_width * 0.5f, -aa_width * 0.5f, stroke_dist);
  }
  
  // Fill with stroke coverage
  static void fillAndStrokeCoverage(float sdf_distance, 
                                     float stroke_width,
                                     float aa_width,
                                     float& fill_cov,
                                     float& stroke_cov) {
    fill_cov = coverage(sdf_distance, aa_width);
    
    if (stroke_width > 0) {
      float half_stroke = stroke_width * 0.5f;
      float inner_dist = sdf_distance + half_stroke;
      float outer_dist = sdf_distance - half_stroke;
      
      float outer_cov = coverage(-outer_dist, aa_width);
      float inner_cov = coverage(-inner_dist, aa_width);
      
      stroke_cov = outer_cov * (1.0f - inner_cov);
    } else {
      stroke_cov = 0.0f;
    }
  }

private:
  static float smoothstep(float edge0, float edge1, float x) {
    float t = fmaxf(0.0f, fminf(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
  }
};

// ============================================================
// Pixel Blending with Coverage
// ============================================================

class CoverageBlending {
public:
  // Standard alpha blend with coverage
  static ColorF blend(const ColorF& dst, const ColorF& src, float coverage) {
    ColorF src_premul = src;
    src_premul.a *= coverage;
    return dst.blend(src_premul);
  }
  
  // Blend fill and stroke
  static ColorF blendFillStroke(const ColorF& dst,
                                 const ColorF& fill_color,
                                 const ColorF& stroke_color,
                                 float fill_coverage,
                                 float stroke_coverage) {
    // Stroke on top of fill
    ColorF result = dst;
    
    // Fill first
    if (fill_coverage > 0.0f) {
      ColorF fill = fill_color;
      fill.a *= fill_coverage;
      result = result.blend(fill);
    }
    
    // Stroke on top
    if (stroke_coverage > 0.0f) {
      ColorF stroke = stroke_color;
      stroke.a *= stroke_coverage;
      result = result.blend(stroke);
    }
    
    return result;
  }
  
  // Additive blend with coverage
  static ColorF blendAdditive(const ColorF& dst, const ColorF& src, float coverage) {
    return ColorF(
      fminf(1.0f, dst.r + src.r * src.a * coverage),
      fminf(1.0f, dst.g + src.g * src.a * coverage),
      fminf(1.0f, dst.b + src.b * src.a * coverage),
      fminf(1.0f, dst.a + src.a * coverage)
    );
  }
  
  // Multiply blend with coverage
  static ColorF blendMultiply(const ColorF& dst, const ColorF& src, float coverage) {
    return ColorF(
      dst.r * (1.0f - coverage) + dst.r * src.r * coverage,
      dst.g * (1.0f - coverage) + dst.g * src.g * coverage,
      dst.b * (1.0f - coverage) + dst.b * src.b * coverage,
      dst.a
    );
  }
  
  // Coverage-aware compositing with mask
  static ColorF compositeMasked(const ColorF& dst, 
                                 const ColorF& src,
                                 float coverage,
                                 CoverageMask dst_mask,
                                 CoverageMask src_mask) {
    // Only blend where masks overlap
    CoverageMask overlap = dst_mask & src_mask;
    float overlap_cov = coverageToAlpha(overlap);
    
    // Blend with adjusted coverage
    float adjusted_cov = coverage * overlap_cov;
    
    ColorF src_adjusted = src;
    src_adjusted.a *= adjusted_cov;
    
    return dst.blend(src_adjusted);
  }
};

// ============================================================
// Antialiased Primitive Renderer
// ============================================================

class AAPrimitiveRenderer {
public:
  struct Config {
    float aa_width;
    float stroke_width;
    ColorF fill_color;
    ColorF stroke_color;
    bool enable_fill;
    bool enable_stroke;
    
    Config()
      : aa_width(1.5f), stroke_width(1.0f),
        fill_color(1, 1, 1, 1), stroke_color(0, 0, 0, 1),
        enable_fill(true), enable_stroke(false) {}
  };
  
  void setConfig(const Config& config) { config_ = config; }
  const Config& getConfig() const { return config_; }
  
  // Render a line with AA
  void renderLine(uint8_t* buffer, int width, int height, int stride,
                  float x0, float y0, float x1, float y1) {
    if (stride == 0) stride = width * 3;
    
    // Bounding box with margin
    int min_x = (int)fmaxf(0, fminf(x0, x1) - config_.stroke_width - 1);
    int max_x = (int)fminf(width - 1, fmaxf(x0, x1) + config_.stroke_width + 1);
    int min_y = (int)fmaxf(0, fminf(y0, y1) - config_.stroke_width - 1);
    int max_y = (int)fminf(height - 1, fmaxf(y0, y1) + config_.stroke_width + 1);
    
    for (int y = min_y; y <= max_y; y++) {
      for (int x = min_x; x <= max_x; x++) {
        float px = x + 0.5f;
        float py = y + 0.5f;
        
        float coverage = AnalyticalCoverage::line(px, py, x0, y0, x1, y1, config_.stroke_width);
        
        if (coverage > 0.001f) {
          uint8_t* p = buffer + y * stride + x * 3;
          ColorF dst = ColorF::fromRGB(p[0], p[1], p[2]);
          ColorF result = CoverageBlending::blend(dst, config_.stroke_color, coverage);
          p[0] = result.r8();
          p[1] = result.g8();
          p[2] = result.b8();
        }
      }
    }
  }
  
  // Render a filled circle with AA
  void renderFilledCircle(uint8_t* buffer, int width, int height, int stride,
                          float cx, float cy, float radius) {
    if (stride == 0) stride = width * 3;
    
    int min_x = (int)fmaxf(0, cx - radius - 1);
    int max_x = (int)fminf(width - 1, cx + radius + 1);
    int min_y = (int)fmaxf(0, cy - radius - 1);
    int max_y = (int)fminf(height - 1, cy + radius + 1);
    
    for (int y = min_y; y <= max_y; y++) {
      for (int x = min_x; x <= max_x; x++) {
        float px = x + 0.5f;
        float py = y + 0.5f;
        
        float coverage = AnalyticalCoverage::circle(px, py, cx, cy, radius, true);
        
        if (coverage > 0.001f) {
          uint8_t* p = buffer + y * stride + x * 3;
          ColorF dst = ColorF::fromRGB(p[0], p[1], p[2]);
          
          ColorF result;
          if (config_.enable_stroke && config_.stroke_width > 0) {
            float fill_cov = AnalyticalCoverage::circle(px, py, cx, cy, 
                                                         radius - config_.stroke_width, true);
            float stroke_cov = coverage - fill_cov;
            result = CoverageBlending::blendFillStroke(dst, config_.fill_color, 
                                                        config_.stroke_color,
                                                        fill_cov, stroke_cov);
          } else {
            result = CoverageBlending::blend(dst, config_.fill_color, coverage);
          }
          
          p[0] = result.r8();
          p[1] = result.g8();
          p[2] = result.b8();
        }
      }
    }
  }
  
  // Render a filled rectangle with AA
  void renderFilledRect(uint8_t* buffer, int width, int height, int stride,
                        float rx, float ry, float rw, float rh) {
    if (stride == 0) stride = width * 3;
    
    int min_x = (int)fmaxf(0, rx - 1);
    int max_x = (int)fminf(width - 1, rx + rw + 1);
    int min_y = (int)fmaxf(0, ry - 1);
    int max_y = (int)fminf(height - 1, ry + rh + 1);
    
    for (int y = min_y; y <= max_y; y++) {
      for (int x = min_x; x <= max_x; x++) {
        float px = x + 0.5f;
        float py = y + 0.5f;
        
        float coverage = AnalyticalCoverage::rectangle(px, py, rx, ry, rw, rh, true);
        
        if (coverage > 0.001f) {
          uint8_t* p = buffer + y * stride + x * 3;
          ColorF dst = ColorF::fromRGB(p[0], p[1], p[2]);
          ColorF result = CoverageBlending::blend(dst, config_.fill_color, coverage);
          p[0] = result.r8();
          p[1] = result.g8();
          p[2] = result.b8();
        }
      }
    }
  }
  
  // Render a filled triangle with AA
  void renderFilledTriangle(uint8_t* buffer, int width, int height, int stride,
                            float x0, float y0, float x1, float y1, float x2, float y2) {
    if (stride == 0) stride = width * 3;
    
    int min_x = (int)fmaxf(0, fminf(fminf(x0, x1), x2) - 1);
    int max_x = (int)fminf(width - 1, fmaxf(fmaxf(x0, x1), x2) + 1);
    int min_y = (int)fmaxf(0, fminf(fminf(y0, y1), y2) - 1);
    int max_y = (int)fminf(height - 1, fmaxf(fmaxf(y0, y1), y2) + 1);
    
    for (int y = min_y; y <= max_y; y++) {
      for (int x = min_x; x <= max_x; x++) {
        float px = x + 0.5f;
        float py = y + 0.5f;
        
        float coverage = AnalyticalCoverage::triangle(px, py, x0, y0, x1, y1, x2, y2);
        
        if (coverage > 0.001f) {
          uint8_t* p = buffer + y * stride + x * 3;
          ColorF dst = ColorF::fromRGB(p[0], p[1], p[2]);
          ColorF result = CoverageBlending::blend(dst, config_.fill_color, coverage);
          p[0] = result.r8();
          p[1] = result.g8();
          p[2] = result.b8();
        }
      }
    }
  }

private:
  Config config_;
};

} // namespace aa
} // namespace gpu

#endif // GPU_ANTIALIASING_HPP_
