/*****************************************************************
 * File:      GpuSDF.hpp
 * Category:  GPU Driver / Signed Distance Field Rendering
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Signed Distance Field (SDF) rendering system for procedural
 *    geometry, implicit surfaces, and resolution-independent shapes.
 * 
 * Features:
 *    - Primitive SDF shapes (circle, box, line, etc.)
 *    - Boolean operations (union, subtract, intersect)
 *    - Smooth blending between shapes
 *    - Per-pixel antialiasing from SDF
 *    - Gradient computation for normals/lighting
 *    - Animation support for morphing shapes
 *****************************************************************/

#ifndef GPU_SDF_HPP_
#define GPU_SDF_HPP_

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "GpuISA.hpp"

namespace gpu {
namespace sdf {

using namespace isa;

// ============================================================
// SDF Constants
// ============================================================

constexpr float SDF_INFINITY = 1e10f;
constexpr float SDF_EPSILON  = 0.0001f;
constexpr size_t MAX_SDF_NODES = 64;

// ============================================================
// 2D Vector Operations
// ============================================================

inline float dot2(float x, float y) { return x * x + y * y; }
inline float length2(float x, float y) { return sqrtf(dot2(x, y)); }
inline float sign(float x) { return x < 0.0f ? -1.0f : (x > 0.0f ? 1.0f : 0.0f); }
inline float clamp(float x, float lo, float hi) { return fmaxf(lo, fminf(hi, x)); }
inline float mix(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float edge0, float edge1, float x) {
  float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// ============================================================
// SDF Primitive Types
// ============================================================

enum class SDFType : uint8_t {
  NONE          = 0x00,
  
  // Primitives
  CIRCLE        = 0x01,
  BOX           = 0x02,
  ROUNDED_BOX   = 0x03,
  SEGMENT       = 0x04,
  CAPSULE       = 0x05,
  TRIANGLE      = 0x06,
  POLYGON       = 0x07,
  ELLIPSE       = 0x08,
  PIE           = 0x09,      // Pie/wedge shape
  ARC           = 0x0A,      // Arc shape
  RING          = 0x0B,      // Ring/annulus
  CROSS         = 0x0C,      // Cross shape
  STAR          = 0x0D,      // N-pointed star
  HEART         = 0x0E,      // Heart shape
  
  // Boolean operations
  UNION         = 0x20,
  SUBTRACT      = 0x21,
  INTERSECT     = 0x22,
  XOR           = 0x23,
  
  // Smooth boolean
  SMOOTH_UNION  = 0x30,
  SMOOTH_SUBTRACT = 0x31,
  SMOOTH_INTERSECT = 0x32,
  
  // Modifiers
  TRANSLATE     = 0x40,
  ROTATE        = 0x41,
  SCALE         = 0x42,
  ROUND         = 0x43,
  ONION         = 0x44,      // Hollow/shell
  REPEAT        = 0x45,      // Tiled repetition
  
  // Special
  CUSTOM        = 0xF0,
};

// ============================================================
// SDF Node (for building SDF trees)
// ============================================================

struct SDFNode {
  SDFType   type;
  uint8_t   child_a;      // Index of first child (for operations)
  uint8_t   child_b;      // Index of second child (for operations)
  
  // Transform
  float     tx, ty;       // Translation
  float     rotation;     // Rotation angle (radians)
  float     scale;        // Uniform scale
  
  // Shape parameters
  float     params[8];    // Shape-specific parameters
  
  // Visual properties
  ColorF    fill_color;
  ColorF    stroke_color;
  float     stroke_width;
  
  // Blending
  float     smooth_k;     // Smoothing factor for smooth operations
  
  SDFNode() 
    : type(SDFType::NONE), child_a(0xFF), child_b(0xFF),
      tx(0), ty(0), rotation(0), scale(1.0f),
      stroke_width(0), smooth_k(4.0f) {
    memset(params, 0, sizeof(params));
    fill_color = ColorF(1, 1, 1, 1);
    stroke_color = ColorF(0, 0, 0, 1);
  }
};

// ============================================================
// SDF Primitive Functions
// ============================================================

class SDFPrimitives {
public:
  // Circle: params[0] = radius
  static float circle(float px, float py, float cx, float cy, float radius) {
    return length2(px - cx, py - cy) - radius;
  }
  
  // Box: params[0] = half_width, params[1] = half_height
  static float box(float px, float py, float cx, float cy, float hw, float hh) {
    float dx = fabsf(px - cx) - hw;
    float dy = fabsf(py - cy) - hh;
    float outside = length2(fmaxf(dx, 0.0f), fmaxf(dy, 0.0f));
    float inside = fminf(fmaxf(dx, dy), 0.0f);
    return outside + inside;
  }
  
  // Rounded box: params[0] = hw, params[1] = hh, params[2] = corner_radius
  static float roundedBox(float px, float py, float cx, float cy, 
                          float hw, float hh, float r) {
    float dx = fabsf(px - cx) - hw + r;
    float dy = fabsf(py - cy) - hh + r;
    float outside = length2(fmaxf(dx, 0.0f), fmaxf(dy, 0.0f));
    float inside = fminf(fmaxf(dx, dy), 0.0f);
    return outside + inside - r;
  }
  
  // Line segment: params[0-1] = start, params[2-3] = end
  static float segment(float px, float py, float x0, float y0, float x1, float y1) {
    float pax = px - x0;
    float pay = py - y0;
    float bax = x1 - x0;
    float bay = y1 - y0;
    float h = clamp((pax * bax + pay * bay) / (bax * bax + bay * bay), 0.0f, 1.0f);
    return length2(pax - bax * h, pay - bay * h);
  }
  
  // Capsule: segment with radius
  static float capsule(float px, float py, float x0, float y0, float x1, float y1, float r) {
    return segment(px, py, x0, y0, x1, y1) - r;
  }
  
  // Triangle
  static float triangle(float px, float py, 
                        float x0, float y0, float x1, float y1, float x2, float y2) {
    // Edge 0
    float e0x = x1 - x0;
    float e0y = y1 - y0;
    float v0x = px - x0;
    float v0y = py - y0;
    float c0 = e0x * v0y - e0y * v0x;
    
    // Edge 1
    float e1x = x2 - x1;
    float e1y = y2 - y1;
    float v1x = px - x1;
    float v1y = py - y1;
    float c1 = e1x * v1y - e1y * v1x;
    
    // Edge 2
    float e2x = x0 - x2;
    float e2y = y0 - y2;
    float v2x = px - x2;
    float v2y = py - y2;
    float c2 = e2x * v2y - e2y * v2x;
    
    // Inside test
    if (c0 >= 0 && c1 >= 0 && c2 >= 0) {
      // Inside - negative distance
      float d0 = fabsf(c0) / length2(e0x, e0y);
      float d1 = fabsf(c1) / length2(e1x, e1y);
      float d2 = fabsf(c2) / length2(e2x, e2y);
      return -fminf(fminf(d0, d1), d2);
    }
    if (c0 <= 0 && c1 <= 0 && c2 <= 0) {
      // Inside (opposite winding)
      float d0 = fabsf(c0) / length2(e0x, e0y);
      float d1 = fabsf(c1) / length2(e1x, e1y);
      float d2 = fabsf(c2) / length2(e2x, e2y);
      return -fminf(fminf(d0, d1), d2);
    }
    
    // Outside - distance to nearest edge
    float d0 = segment(px, py, x0, y0, x1, y1);
    float d1 = segment(px, py, x1, y1, x2, y2);
    float d2 = segment(px, py, x2, y2, x0, y0);
    return fminf(fminf(d0, d1), d2);
  }
  
  // Ellipse: params[0] = half_width, params[1] = half_height
  static float ellipse(float px, float py, float cx, float cy, float a, float b) {
    // Approximate ellipse SDF
    float dx = (px - cx) / a;
    float dy = (py - cy) / b;
    float d = sqrtf(dx * dx + dy * dy);
    
    if (d < SDF_EPSILON) return -fminf(a, b);
    
    // Normalize
    dx /= d;
    dy /= d;
    
    // Point on ellipse in this direction
    float k = 1.0f / sqrtf((dx * dx) / (a * a) + (dy * dy) / (b * b));
    
    return (d - 1.0f) * fminf(a, b);
  }
  
  // Pie/wedge: params[0] = radius, params[1] = aperture (radians)
  static float pie(float px, float py, float cx, float cy, float r, float aperture) {
    float dx = px - cx;
    float dy = py - cy;
    float l = length2(dx, dy);
    
    // Angle of point
    float angle = atan2f(dy, dx);
    
    // Clamp angle to aperture
    float half_ap = aperture * 0.5f;
    if (fabsf(angle) < half_ap) {
      return l - r;  // Inside aperture
    }
    
    // Outside aperture - distance to edge
    float s = sinf(half_ap);
    float c = cosf(half_ap);
    float edge_x = c * r;
    float edge_y = s * r;
    
    if (angle > 0) {
      return segment(dx, dy, 0, 0, edge_x, edge_y);
    } else {
      return segment(dx, dy, 0, 0, edge_x, -edge_y);
    }
  }
  
  // Ring: params[0] = outer_radius, params[1] = thickness
  static float ring(float px, float py, float cx, float cy, float r, float thickness) {
    float d = length2(px - cx, py - cy);
    return fabsf(d - r) - thickness;
  }
  
  // N-pointed star: params[0] = outer_radius, params[1] = inner_radius, params[2] = n_points
  static float star(float px, float py, float cx, float cy, 
                    float r_out, float r_in, float n) {
    float dx = px - cx;
    float dy = py - cy;
    
    // Angle of point
    float angle = atan2f(dy, dx);
    
    // Sector angle
    float sector = 3.14159265f / n;
    
    // Fold into one sector
    float a = fmodf(angle + sector, 2.0f * sector) - sector;
    
    // Distance
    float d = length2(dx, dy);
    
    // Interpolate between inner and outer radius based on angle
    float t = fabsf(a) / sector;
    float r = mix(r_out, r_in, t);
    
    return d - r;
  }
  
  // Heart shape (approximate)
  static float heart(float px, float py, float cx, float cy, float size) {
    float dx = (px - cx) / size;
    float dy = (py - cy) / size + 0.5f;  // Offset center
    
    // Heart formula
    float x2 = dx * dx;
    float y2 = dy * dy;
    float d = x2 + y2 - 1.0f;
    
    float heart_val = d * d * d - x2 * y2 * dy;
    
    // Convert to approximate SDF
    return heart_val * size * 0.3f;
  }
};

// ============================================================
// SDF Boolean Operations
// ============================================================

class SDFOperations {
public:
  // Union (min)
  static float opUnion(float d1, float d2) {
    return fminf(d1, d2);
  }
  
  // Subtraction (max with negation)
  static float opSubtract(float d1, float d2) {
    return fmaxf(d1, -d2);
  }
  
  // Intersection (max)
  static float opIntersect(float d1, float d2) {
    return fmaxf(d1, d2);
  }
  
  // XOR
  static float opXor(float d1, float d2) {
    return fmaxf(fminf(d1, d2), -fmaxf(d1, d2));
  }
  
  // Smooth union
  static float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return mix(d2, d1, h) - k * h * (1.0f - h);
  }
  
  // Smooth subtraction
  static float opSmoothSubtract(float d1, float d2, float k) {
    float h = clamp(0.5f - 0.5f * (d2 + d1) / k, 0.0f, 1.0f);
    return mix(d1, -d2, h) + k * h * (1.0f - h);
  }
  
  // Smooth intersection
  static float opSmoothIntersect(float d1, float d2, float k) {
    float h = clamp(0.5f - 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return mix(d2, d1, h) + k * h * (1.0f - h);
  }
  
  // Blend between two SDFs
  static float opBlend(float d1, float d2, float t) {
    return mix(d1, d2, t);
  }
};

// ============================================================
// SDF Modifiers
// ============================================================

class SDFModifiers {
public:
  // Round (expand)
  static float opRound(float d, float r) {
    return d - r;
  }
  
  // Onion (shell/hollow)
  static float opOnion(float d, float thickness) {
    return fabsf(d) - thickness;
  }
  
  // Annular (ring from any shape)
  static float opAnnular(float d, float r) {
    return fabsf(d) - r;
  }
  
  // Transform point (for evaluation)
  static void transformPoint(float& px, float& py, 
                             float tx, float ty, float rotation, float scale) {
    // Inverse transform for SDF evaluation
    float s = 1.0f / scale;
    float c = cosf(-rotation);
    float sn = sinf(-rotation);
    
    float dx = (px - tx) * s;
    float dy = (py - ty) * s;
    
    px = dx * c - dy * sn;
    py = dx * sn + dy * c;
  }
  
  // Repetition (tiling)
  static void opRepeat(float& px, float& py, float spacing_x, float spacing_y) {
    if (spacing_x > 0) {
      px = fmodf(px + spacing_x * 0.5f, spacing_x) - spacing_x * 0.5f;
    }
    if (spacing_y > 0) {
      py = fmodf(py + spacing_y * 0.5f, spacing_y) - spacing_y * 0.5f;
    }
  }
  
  // Limited repetition
  static void opRepeatLimited(float& px, float& py, 
                              float spacing_x, float spacing_y,
                              float limit_x, float limit_y) {
    if (spacing_x > 0) {
      px = px - spacing_x * clamp(roundf(px / spacing_x), -limit_x, limit_x);
    }
    if (spacing_y > 0) {
      py = py - spacing_y * clamp(roundf(py / spacing_y), -limit_y, limit_y);
    }
  }
  
  // Symmetry (mirror)
  static void opSymmetryX(float& px) { px = fabsf(px); }
  static void opSymmetryY(float& py) { py = fabsf(py); }
};

// ============================================================
// SDF Scene (collection of nodes)
// ============================================================

class SDFScene {
public:
  SDFScene() : node_count_(0), root_node_(0xFF) {}
  
  // Add a primitive
  uint8_t addCircle(float cx, float cy, float radius, ColorF color) {
    uint8_t id = allocNode();
    if (id == 0xFF) return 0xFF;
    
    SDFNode& node = nodes_[id];
    node.type = SDFType::CIRCLE;
    node.tx = cx;
    node.ty = cy;
    node.params[0] = radius;
    node.fill_color = color;
    
    if (root_node_ == 0xFF) root_node_ = id;
    return id;
  }
  
  uint8_t addBox(float cx, float cy, float hw, float hh, ColorF color) {
    uint8_t id = allocNode();
    if (id == 0xFF) return 0xFF;
    
    SDFNode& node = nodes_[id];
    node.type = SDFType::BOX;
    node.tx = cx;
    node.ty = cy;
    node.params[0] = hw;
    node.params[1] = hh;
    node.fill_color = color;
    
    if (root_node_ == 0xFF) root_node_ = id;
    return id;
  }
  
  uint8_t addRoundedBox(float cx, float cy, float hw, float hh, float r, ColorF color) {
    uint8_t id = allocNode();
    if (id == 0xFF) return 0xFF;
    
    SDFNode& node = nodes_[id];
    node.type = SDFType::ROUNDED_BOX;
    node.tx = cx;
    node.ty = cy;
    node.params[0] = hw;
    node.params[1] = hh;
    node.params[2] = r;
    node.fill_color = color;
    
    if (root_node_ == 0xFF) root_node_ = id;
    return id;
  }
  
  uint8_t addSegment(float x0, float y0, float x1, float y1, float width, ColorF color) {
    uint8_t id = allocNode();
    if (id == 0xFF) return 0xFF;
    
    SDFNode& node = nodes_[id];
    node.type = SDFType::CAPSULE;
    node.params[0] = x0;
    node.params[1] = y0;
    node.params[2] = x1;
    node.params[3] = y1;
    node.params[4] = width;
    node.fill_color = color;
    
    if (root_node_ == 0xFF) root_node_ = id;
    return id;
  }
  
  uint8_t addTriangle(float x0, float y0, float x1, float y1, float x2, float y2, ColorF color) {
    uint8_t id = allocNode();
    if (id == 0xFF) return 0xFF;
    
    SDFNode& node = nodes_[id];
    node.type = SDFType::TRIANGLE;
    node.params[0] = x0;
    node.params[1] = y0;
    node.params[2] = x1;
    node.params[3] = y1;
    node.params[4] = x2;
    node.params[5] = y2;
    node.fill_color = color;
    
    if (root_node_ == 0xFF) root_node_ = id;
    return id;
  }
  
  uint8_t addRing(float cx, float cy, float r, float thickness, ColorF color) {
    uint8_t id = allocNode();
    if (id == 0xFF) return 0xFF;
    
    SDFNode& node = nodes_[id];
    node.type = SDFType::RING;
    node.tx = cx;
    node.ty = cy;
    node.params[0] = r;
    node.params[1] = thickness;
    node.fill_color = color;
    
    if (root_node_ == 0xFF) root_node_ = id;
    return id;
  }
  
  uint8_t addStar(float cx, float cy, float r_out, float r_in, int n, ColorF color) {
    uint8_t id = allocNode();
    if (id == 0xFF) return 0xFF;
    
    SDFNode& node = nodes_[id];
    node.type = SDFType::STAR;
    node.tx = cx;
    node.ty = cy;
    node.params[0] = r_out;
    node.params[1] = r_in;
    node.params[2] = (float)n;
    node.fill_color = color;
    
    if (root_node_ == 0xFF) root_node_ = id;
    return id;
  }
  
  // Boolean operations
  uint8_t addUnion(uint8_t a, uint8_t b) {
    return addOperation(SDFType::UNION, a, b);
  }
  
  uint8_t addSubtract(uint8_t a, uint8_t b) {
    return addOperation(SDFType::SUBTRACT, a, b);
  }
  
  uint8_t addIntersect(uint8_t a, uint8_t b) {
    return addOperation(SDFType::INTERSECT, a, b);
  }
  
  uint8_t addSmoothUnion(uint8_t a, uint8_t b, float k = 4.0f) {
    uint8_t id = addOperation(SDFType::SMOOTH_UNION, a, b);
    if (id != 0xFF) nodes_[id].smooth_k = k;
    return id;
  }
  
  uint8_t addSmoothSubtract(uint8_t a, uint8_t b, float k = 4.0f) {
    uint8_t id = addOperation(SDFType::SMOOTH_SUBTRACT, a, b);
    if (id != 0xFF) nodes_[id].smooth_k = k;
    return id;
  }
  
  // Modifiers
  void setTransform(uint8_t id, float tx, float ty, float rotation = 0, float scale = 1.0f) {
    if (id < node_count_) {
      nodes_[id].tx = tx;
      nodes_[id].ty = ty;
      nodes_[id].rotation = rotation;
      nodes_[id].scale = scale;
    }
  }
  
  void setStroke(uint8_t id, ColorF color, float width) {
    if (id < node_count_) {
      nodes_[id].stroke_color = color;
      nodes_[id].stroke_width = width;
    }
  }
  
  void setRoot(uint8_t id) {
    if (id < node_count_) root_node_ = id;
  }
  
  // Evaluate SDF at point
  float evaluate(float px, float py) const {
    if (root_node_ == 0xFF) return SDF_INFINITY;
    return evaluateNode(root_node_, px, py);
  }
  
  // Evaluate with color
  float evaluate(float px, float py, ColorF& out_color) const {
    if (root_node_ == 0xFF) {
      out_color = ColorF(0, 0, 0, 0);
      return SDF_INFINITY;
    }
    return evaluateNodeWithColor(root_node_, px, py, out_color);
  }
  
  // Compute gradient (for normals)
  void gradient(float px, float py, float& gx, float& gy) const {
    const float eps = 0.5f;
    gx = evaluate(px + eps, py) - evaluate(px - eps, py);
    gy = evaluate(px, py + eps) - evaluate(px, py - eps);
    float len = sqrtf(gx * gx + gy * gy);
    if (len > SDF_EPSILON) {
      gx /= len;
      gy /= len;
    }
  }
  
  // Get node for modification
  SDFNode* getNode(uint8_t id) {
    return (id < node_count_) ? &nodes_[id] : nullptr;
  }
  
  const SDFNode* getNode(uint8_t id) const {
    return (id < node_count_) ? &nodes_[id] : nullptr;
  }
  
  // Clear scene
  void clear() {
    node_count_ = 0;
    root_node_ = 0xFF;
  }

private:
  SDFNode  nodes_[MAX_SDF_NODES];
  uint8_t  node_count_;
  uint8_t  root_node_;
  
  uint8_t allocNode() {
    if (node_count_ >= MAX_SDF_NODES) return 0xFF;
    nodes_[node_count_] = SDFNode();
    return node_count_++;
  }
  
  uint8_t addOperation(SDFType type, uint8_t a, uint8_t b) {
    uint8_t id = allocNode();
    if (id == 0xFF) return 0xFF;
    
    SDFNode& node = nodes_[id];
    node.type = type;
    node.child_a = a;
    node.child_b = b;
    
    // Inherit color from first child
    if (a < node_count_) {
      node.fill_color = nodes_[a].fill_color;
    }
    
    root_node_ = id;
    return id;
  }
  
  float evaluateNode(uint8_t id, float px, float py) const {
    if (id >= node_count_) return SDF_INFINITY;
    
    const SDFNode& node = nodes_[id];
    float lpx = px, lpy = py;
    
    // Apply inverse transform
    if (node.scale != 1.0f || node.rotation != 0.0f || node.tx != 0.0f || node.ty != 0.0f) {
      SDFModifiers::transformPoint(lpx, lpy, node.tx, node.ty, node.rotation, node.scale);
    }
    
    float d;
    
    switch (node.type) {
      case SDFType::CIRCLE:
        d = SDFPrimitives::circle(lpx, lpy, 0, 0, node.params[0]);
        break;
        
      case SDFType::BOX:
        d = SDFPrimitives::box(lpx, lpy, 0, 0, node.params[0], node.params[1]);
        break;
        
      case SDFType::ROUNDED_BOX:
        d = SDFPrimitives::roundedBox(lpx, lpy, 0, 0, 
            node.params[0], node.params[1], node.params[2]);
        break;
        
      case SDFType::CAPSULE:
        d = SDFPrimitives::capsule(lpx, lpy,
            node.params[0], node.params[1],
            node.params[2], node.params[3], node.params[4]);
        break;
        
      case SDFType::TRIANGLE:
        d = SDFPrimitives::triangle(lpx, lpy,
            node.params[0], node.params[1],
            node.params[2], node.params[3],
            node.params[4], node.params[5]);
        break;
        
      case SDFType::RING:
        d = SDFPrimitives::ring(lpx, lpy, 0, 0, node.params[0], node.params[1]);
        break;
        
      case SDFType::STAR:
        d = SDFPrimitives::star(lpx, lpy, 0, 0,
            node.params[0], node.params[1], node.params[2]);
        break;
        
      case SDFType::UNION:
        d = SDFOperations::opUnion(
            evaluateNode(node.child_a, px, py),
            evaluateNode(node.child_b, px, py));
        break;
        
      case SDFType::SUBTRACT:
        d = SDFOperations::opSubtract(
            evaluateNode(node.child_a, px, py),
            evaluateNode(node.child_b, px, py));
        break;
        
      case SDFType::INTERSECT:
        d = SDFOperations::opIntersect(
            evaluateNode(node.child_a, px, py),
            evaluateNode(node.child_b, px, py));
        break;
        
      case SDFType::SMOOTH_UNION:
        d = SDFOperations::opSmoothUnion(
            evaluateNode(node.child_a, px, py),
            evaluateNode(node.child_b, px, py),
            node.smooth_k);
        break;
        
      case SDFType::SMOOTH_SUBTRACT:
        d = SDFOperations::opSmoothSubtract(
            evaluateNode(node.child_a, px, py),
            evaluateNode(node.child_b, px, py),
            node.smooth_k);
        break;
        
      default:
        d = SDF_INFINITY;
        break;
    }
    
    // Apply scale to distance
    if (node.scale != 1.0f) {
      d *= node.scale;
    }
    
    return d;
  }
  
  float evaluateNodeWithColor(uint8_t id, float px, float py, ColorF& out_color) const {
    if (id >= node_count_) return SDF_INFINITY;
    
    const SDFNode& node = nodes_[id];
    
    // For operations, blend colors based on which child is closer
    if (node.type >= SDFType::UNION && node.type <= SDFType::SMOOTH_INTERSECT) {
      ColorF color_a, color_b;
      float d_a = evaluateNodeWithColor(node.child_a, px, py, color_a);
      float d_b = evaluateNodeWithColor(node.child_b, px, py, color_b);
      
      float d;
      switch (node.type) {
        case SDFType::UNION:
          d = SDFOperations::opUnion(d_a, d_b);
          out_color = (d_a < d_b) ? color_a : color_b;
          break;
          
        case SDFType::SUBTRACT:
          d = SDFOperations::opSubtract(d_a, d_b);
          out_color = color_a;
          break;
          
        case SDFType::INTERSECT:
          d = SDFOperations::opIntersect(d_a, d_b);
          out_color = (d_a > d_b) ? color_a : color_b;
          break;
          
        case SDFType::SMOOTH_UNION: {
          d = SDFOperations::opSmoothUnion(d_a, d_b, node.smooth_k);
          float h = clamp(0.5f + 0.5f * (d_b - d_a) / node.smooth_k, 0.0f, 1.0f);
          out_color = color_b.lerp(color_a, h);
          break;
        }
          
        case SDFType::SMOOTH_SUBTRACT: {
          d = SDFOperations::opSmoothSubtract(d_a, d_b, node.smooth_k);
          out_color = color_a;
          break;
        }
          
        default:
          d = SDF_INFINITY;
          break;
      }
      
      return d;
    }
    
    // Primitive - use node's fill color
    out_color = node.fill_color;
    return evaluateNode(id, px, py);
  }
};

// ============================================================
// SDF Renderer
// ============================================================

class SDFRenderer {
public:
  struct Config {
    float aa_width;           // Antialiasing width (pixels)
    float stroke_aa_width;    // Stroke AA width
    bool  enable_stroke;
    bool  enable_fill;
    bool  enable_shadow;
    float shadow_offset_x;
    float shadow_offset_y;
    float shadow_blur;
    ColorF shadow_color;
    
    Config() 
      : aa_width(1.5f), stroke_aa_width(1.0f),
        enable_stroke(true), enable_fill(true), enable_shadow(false),
        shadow_offset_x(2), shadow_offset_y(2), shadow_blur(4),
        shadow_color(0, 0, 0, 0.5f) {}
  };
  
  SDFRenderer() {}
  
  void setConfig(const Config& config) { config_ = config; }
  const Config& getConfig() const { return config_; }
  
  // Render SDF scene to buffer
  void render(const SDFScene& scene, uint8_t* buffer, 
              int width, int height, int stride = 0) {
    if (stride == 0) stride = width * 3;
    
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        // Sample at pixel center
        float px = x + 0.5f;
        float py = y + 0.5f;
        
        ColorF pixel_color(0, 0, 0, 0);
        
        // Shadow pass
        if (config_.enable_shadow) {
          float shadow_px = px - config_.shadow_offset_x;
          float shadow_py = py - config_.shadow_offset_y;
          float shadow_d = scene.evaluate(shadow_px, shadow_py);
          float shadow_alpha = smoothstep(config_.shadow_blur, 0, shadow_d);
          ColorF shadow = config_.shadow_color;
          shadow.a *= shadow_alpha;
          pixel_color = pixel_color.blend(shadow);
        }
        
        // Main SDF evaluation
        ColorF sdf_color;
        float d = scene.evaluate(px, py, sdf_color);
        
        // Fill with antialiasing
        if (config_.enable_fill) {
          float fill_alpha = smoothstep(config_.aa_width, -config_.aa_width, d);
          ColorF fill = sdf_color;
          fill.a *= fill_alpha;
          pixel_color = pixel_color.blend(fill);
        }
        
        // Stroke
        if (config_.enable_stroke) {
          // For stroked shapes, we'd need stroke width per node
          // Simplified: use stroke on root if present
        }
        
        // Write to buffer (RGB)
        uint8_t* p = buffer + y * stride + x * 3;
        p[0] = pixel_color.r8();
        p[1] = pixel_color.g8();
        p[2] = pixel_color.b8();
      }
    }
  }
  
  // Render single pixel (for use in shader)
  ColorF renderPixel(const SDFScene& scene, float px, float py) const {
    ColorF pixel_color(0, 0, 0, 0);
    
    // Shadow
    if (config_.enable_shadow) {
      float shadow_px = px - config_.shadow_offset_x;
      float shadow_py = py - config_.shadow_offset_y;
      float shadow_d = scene.evaluate(shadow_px, shadow_py);
      float shadow_alpha = smoothstep(config_.shadow_blur, 0, shadow_d);
      ColorF shadow = config_.shadow_color;
      shadow.a *= shadow_alpha;
      pixel_color = pixel_color.blend(shadow);
    }
    
    // Main shape
    ColorF sdf_color;
    float d = scene.evaluate(px, py, sdf_color);
    
    if (config_.enable_fill) {
      float fill_alpha = smoothstep(config_.aa_width, -config_.aa_width, d);
      ColorF fill = sdf_color;
      fill.a *= fill_alpha;
      pixel_color = pixel_color.blend(fill);
    }
    
    return pixel_color;
  }

private:
  Config config_;
};

} // namespace sdf
} // namespace gpu

#endif // GPU_SDF_HPP_
