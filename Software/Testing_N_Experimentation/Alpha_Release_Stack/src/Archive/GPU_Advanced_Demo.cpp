/*****************************************************************
 * File:      GPU_Advanced_Demo.cpp
 * Category:  Hardware Integration Demo
 * Author:    Generated for GPU Driver Advanced Demo
 * 
 * Purpose:
 *    GPU-side advanced demo showing SDF morphing, antialiasing,
 *    RGB pixel shaders, and smooth animation with easing.
 * 
 * Features:
 *    1. SDF polygon morphing between two shapes
 *    2. Per-pixel antialiasing using SDF distance
 *    3. Animated RGB pixel shader (rainbow/plasma)
 *    4. Smooth easing functions (ease-in-out)
 *    5. Rotation and translation animation
 *    6. Filled polygon with antialiased edges
 *****************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

// Include the ARCOS display drivers
#include "abstraction/hal.hpp"
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"

static const char* TAG = "GPU_DEMO";

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

// ============================================================
// Display Hardware
// ============================================================

SimpleHUB75Display hub75_display;

// Display dimensions
constexpr int WIDTH = 128;
constexpr int HEIGHT = 32;
constexpr float PI = 3.14159265358979f;
constexpr float TWO_PI = 6.28318530717958f;

// ============================================================
// Math Utilities
// ============================================================

inline float clampf(float x, float lo, float hi) { return fmaxf(lo, fminf(hi, x)); }
inline float mixf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float edge0, float edge1, float x) {
  float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// Easing functions
inline float easeInOutCubic(float t) {
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

inline float easeInOutSine(float t) {
  return -(cosf(PI * t) - 1.0f) / 2.0f;
}

// ============================================================
// Polygon SDF - Signed Distance to Arbitrary Polygon
// ============================================================

struct Vec2 {
  float x, y;
  Vec2() : x(0), y(0) {}
  Vec2(float x_, float y_) : x(x_), y(y_) {}
};

// Compute signed distance to a polygon
float polygonSDF(float px, float py, const Vec2* verts, int n) {
  float d = (px - verts[0].x) * (px - verts[0].x) + (py - verts[0].y) * (py - verts[0].y);
  float s = 1.0f;
  
  for (int i = 0, j = n - 1; i < n; j = i++) {
    // Edge vector
    float ex = verts[j].x - verts[i].x;
    float ey = verts[j].y - verts[i].y;
    
    // Vector from vertex to point
    float wx = px - verts[i].x;
    float wy = py - verts[i].y;
    
    // Project point onto edge
    float t = clampf((wx * ex + wy * ey) / (ex * ex + ey * ey), 0.0f, 1.0f);
    
    // Closest point on edge
    float bx = wx - ex * t;
    float by = wy - ey * t;
    
    // Distance to edge
    float d2 = bx * bx + by * by;
    d = fminf(d, d2);
    
    // Winding number for sign
    bool c1 = py >= verts[i].y;
    bool c2 = py < verts[j].y;
    bool c3 = ex * wy > ey * wx;
    
    if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) {
      s *= -1.0f;
    }
  }
  
  return s * sqrtf(d);
}

// ============================================================
// Shape Definitions (normalized to ~0-30 range)
// ============================================================

// Shape 1: User's first shape (looks like a speech bubble or complex shape)
// {6, 8}, {14, 8}, {20, 11}, {26, 17}, {27, 19}, {28, 22}, {23, 22}, {21, 19}, {19, 17}, {17, 17}, {16, 19}, {18, 22}, {7, 22}, {4, 20}, {2, 17}, {2, 12}
const Vec2 shape1_verts[] = {
  {6, 8}, {14, 8}, {20, 11}, {26, 17}, {27, 19}, {28, 22}, 
  {23, 22}, {21, 19}, {19, 17}, {17, 17}, {16, 19}, {18, 22}, 
  {7, 22}, {4, 20}, {2, 17}, {2, 12}
};
constexpr int shape1_count = sizeof(shape1_verts) / sizeof(shape1_verts[0]);

// Shape 2: User's second shape (looks like a star/explosion)
// {15, 24}, {7, 14}, {6, 10}, {10, 6}, {14, 8}, {15, 11}, {16, 8}, {20, 6}, {24, 10}, {23, 14}
const Vec2 shape2_verts[] = {
  {15, 24}, {7, 14}, {6, 10}, {10, 6}, {14, 8}, 
  {15, 11}, {16, 8}, {20, 6}, {24, 10}, {23, 14}
};
constexpr int shape2_count = sizeof(shape2_verts) / sizeof(shape2_verts[0]);

// Morphed vertices (max of both counts)
constexpr int max_verts = shape1_count > shape2_count ? shape1_count : shape2_count;
Vec2 morphed_verts[max_verts];

// ============================================================
// Morph between two polygons (with vertex interpolation)
// ============================================================

void morphPolygons(const Vec2* from, int from_count, 
                   const Vec2* to, int to_count,
                   Vec2* out, int& out_count, float t) {
  // Use the larger count as output
  out_count = from_count > to_count ? from_count : to_count;
  
  for (int i = 0; i < out_count; i++) {
    // Map index to both shapes
    float from_idx = (float)i * (from_count - 1) / (out_count - 1);
    float to_idx = (float)i * (to_count - 1) / (out_count - 1);
    
    // Interpolate within source shape
    int from_i0 = (int)from_idx;
    int from_i1 = (from_i0 + 1) % from_count;
    float from_frac = from_idx - from_i0;
    if (from_i0 >= from_count - 1) { from_i0 = from_count - 1; from_i1 = 0; from_frac = 0; }
    
    float from_x = mixf(from[from_i0].x, from[from_i1].x, from_frac);
    float from_y = mixf(from[from_i0].y, from[from_i1].y, from_frac);
    
    // Interpolate within target shape
    int to_i0 = (int)to_idx;
    int to_i1 = (to_i0 + 1) % to_count;
    float to_frac = to_idx - to_i0;
    if (to_i0 >= to_count - 1) { to_i0 = to_count - 1; to_i1 = 0; to_frac = 0; }
    
    float to_x = mixf(to[to_i0].x, to[to_i1].x, to_frac);
    float to_y = mixf(to[to_i0].y, to[to_i1].y, to_frac);
    
    // Morph between shapes
    out[i].x = mixf(from_x, to_x, t);
    out[i].y = mixf(from_y, to_y, t);
  }
}

// ============================================================
// RGB Pixel Shader - Rainbow/Plasma effect
// ============================================================

void rgbShader(float x, float y, float time, float sdf, 
               uint8_t& r, uint8_t& g, uint8_t& b) {
  // Plasma effect based on position and time
  float v1 = sinf(x * 0.1f + time);
  float v2 = sinf((y * 0.1f + time * 0.7f));
  float v3 = sinf((x * 0.1f + y * 0.1f + time * 0.5f));
  float v4 = sinf(sqrtf((x - 64) * (x - 64) + (y - 16) * (y - 16)) * 0.15f - time * 2.0f);
  
  float v = (v1 + v2 + v3 + v4) * 0.25f;
  
  // Convert to RGB
  float hue = (v + 1.0f) * 0.5f + time * 0.1f;  // 0-1 range, shifting with time
  hue = fmodf(hue, 1.0f);
  
  // HSV to RGB (simplified, S=1, V=1)
  float h = hue * 6.0f;
  int i = (int)h;
  float f = h - i;
  
  float q = 1.0f - f;
  float t_val = f;
  
  switch (i % 6) {
    case 0: r = 255; g = (uint8_t)(t_val * 255); b = 0; break;
    case 1: r = (uint8_t)(q * 255); g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = (uint8_t)(t_val * 255); break;
    case 3: r = 0; g = (uint8_t)(q * 255); b = 255; break;
    case 4: r = (uint8_t)(t_val * 255); g = 0; b = 255; break;
    default: r = 255; g = 0; b = (uint8_t)(q * 255); break;
  }
  
  // Add SDF-based edge glow
  float edge_glow = smoothstep(2.0f, 0.0f, fabsf(sdf));
  r = (uint8_t)clampf(r + edge_glow * 50, 0, 255);
  g = (uint8_t)clampf(g + edge_glow * 50, 0, 255);
  b = (uint8_t)clampf(b + edge_glow * 50, 0, 255);
}

// ============================================================
// Transform point (rotation + translation)
// ============================================================

void transformPoint(float& px, float& py, float cx, float cy, float angle, float tx, float ty) {
  // Translate to origin
  float dx = px - cx;
  float dy = py - cy;
  
  // Rotate
  float c = cosf(angle);
  float s = sinf(angle);
  float rx = dx * c - dy * s;
  float ry = dx * s + dy * c;
  
  // Translate back and apply additional translation
  px = rx + cx + tx;
  py = ry + cy + ty;
}

// ============================================================
// Fast Sin/Cos using lookup table
// ============================================================

constexpr int SIN_TABLE_SIZE = 256;
float sin_table[SIN_TABLE_SIZE];
bool tables_initialized = false;

void initTables() {
  if (tables_initialized) return;
  for (int i = 0; i < SIN_TABLE_SIZE; i++) {
    sin_table[i] = sinf((float)i * TWO_PI / SIN_TABLE_SIZE);
  }
  tables_initialized = true;
}

inline float fastSin(float x) {
  int idx = (int)(x * SIN_TABLE_SIZE / TWO_PI) & (SIN_TABLE_SIZE - 1);
  return sin_table[idx];
}

inline float fastCos(float x) {
  int idx = ((int)(x * SIN_TABLE_SIZE / TWO_PI) + SIN_TABLE_SIZE / 4) & (SIN_TABLE_SIZE - 1);
  return sin_table[idx];
}

// ============================================================
// Render Frame with SDF, Antialiasing, and Shader
// ============================================================

// Pre-compute transformed vertices (global to avoid stack allocation)
Vec2 g_transformed[max_verts];
int g_vert_count;

void renderFrame(float time) {
  initTables();
  
  // Animation parameters
  float morph_cycle = 6.0f;  // Seconds for full morph cycle
  float morph_phase = fmodf(time, morph_cycle * 2.0f);
  float morph_t;
  
  if (morph_phase < morph_cycle) {
    morph_t = easeInOutCubic(morph_phase / morph_cycle);
  } else {
    morph_t = 1.0f - easeInOutCubic((morph_phase - morph_cycle) / morph_cycle);
  }
  
  // Generate morphed polygon
  morphPolygons(shape1_verts, shape1_count, shape2_verts, shape2_count,
                morphed_verts, g_vert_count, morph_t);
  
  // Calculate center of morphed shape
  float cx = 0, cy = 0;
  for (int i = 0; i < g_vert_count; i++) {
    cx += morphed_verts[i].x;
    cy += morphed_verts[i].y;
  }
  cx /= g_vert_count;
  cy /= g_vert_count;
  
  // Animation: rotation and bobbing motion (use fast trig)
  float rotation = fastSin(time * 0.5f) * 0.3f;
  float bob_x = fastSin(time * 1.2f) * 5.0f;
  float bob_y = fastCos(time * 0.8f) * 3.0f;
  
  // Pre-compute rotation sin/cos
  float rot_c = fastCos(rotation);
  float rot_s = fastSin(rotation);
  
  // Scale and offsets
  float scale = 1.0f;
  float offset_x = (WIDTH / 2.0f) - cx * scale;
  float offset_y = (HEIGHT / 2.0f) - cy * scale;
  float screen_cx = WIDTH / 2.0f;
  float screen_cy = HEIGHT / 2.0f;
  
  // Apply transforms to vertices
  for (int i = 0; i < g_vert_count; i++) {
    float vx = morphed_verts[i].x * scale + offset_x;
    float vy = morphed_verts[i].y * scale + offset_y;
    
    // Rotation around center + bob
    float dx = vx - screen_cx;
    float dy = vy - screen_cy;
    g_transformed[i].x = dx * rot_c - dy * rot_s + screen_cx + bob_x;
    g_transformed[i].y = dx * rot_s + dy * rot_c + screen_cy + bob_y;
  }
  
  // Calculate bounding box for early rejection
  float min_x = g_transformed[0].x, max_x = g_transformed[0].x;
  float min_y = g_transformed[0].y, max_y = g_transformed[0].y;
  for (int i = 1; i < g_vert_count; i++) {
    if (g_transformed[i].x < min_x) min_x = g_transformed[i].x;
    if (g_transformed[i].x > max_x) max_x = g_transformed[i].x;
    if (g_transformed[i].y < min_y) min_y = g_transformed[i].y;
    if (g_transformed[i].y > max_y) max_y = g_transformed[i].y;
  }
  
  // Expand bbox for antialiasing
  int bbox_x0 = (int)fmaxf(0, min_x - 3);
  int bbox_x1 = (int)fminf(WIDTH - 1, max_x + 3);
  int bbox_y0 = (int)fmaxf(0, min_y - 3);
  int bbox_y1 = (int)fminf(HEIGHT - 1, max_y + 3);
  
  // Pre-compute time-dependent shader values
  float time_offset = fmodf(time * 0.1f, 1.0f);
  
  // Render each pixel
  for (int py = 0; py < HEIGHT; py++) {
    for (int px = 0; px < WIDTH; px++) {
      // Quick bounds check
      bool in_bbox = (px >= bbox_x0 && px <= bbox_x1 && py >= bbox_y0 && py <= bbox_y1);
      
      if (in_bbox) {
        // Compute SDF distance
        float sdf = polygonSDF((float)px + 0.5f, (float)py + 0.5f, g_transformed, g_vert_count);
        
        // Antialiasing: smooth alpha based on SDF
        float alpha = smoothstep(1.0f, -1.0f, sdf);
        
        if (alpha > 0.01f) {
          // Fast plasma shader using lookup tables
          float v1 = fastSin(px * 0.1f + time);
          float v2 = fastSin(py * 0.1f + time * 0.7f);
          float v3 = fastSin((px + py) * 0.07f + time * 0.5f);
          float v = (v1 + v2 + v3) * 0.333f;
          
          // Fast HSV to RGB
          float hue = (v + 1.0f) * 0.5f + time_offset;
          hue = hue - (int)hue;  // Fast fmod for 0-1
          float h = hue * 6.0f;
          int hi = (int)h;
          float f = h - hi;
          
          uint8_t r, g, b;
          switch (hi % 6) {
            case 0: r = 255; g = (uint8_t)(f * 255); b = 0; break;
            case 1: r = (uint8_t)((1-f) * 255); g = 255; b = 0; break;
            case 2: r = 0; g = 255; b = (uint8_t)(f * 255); break;
            case 3: r = 0; g = (uint8_t)((1-f) * 255); b = 255; break;
            case 4: r = (uint8_t)(f * 255); g = 0; b = 255; break;
            default: r = 255; g = 0; b = (uint8_t)((1-f) * 255); break;
          }
          
          // Apply alpha
          r = (uint8_t)(r * alpha);
          g = (uint8_t)(g * alpha);
          b = (uint8_t)(b * alpha);
          
          hub75_display.setPixel(px, py, RGB(r, g, b));
        } else {
          // Background
          hub75_display.setPixel(px, py, RGB(4, 2, 4));
        }
      } else {
        // Outside bounding box - just background
        hub75_display.setPixel(px, py, RGB(4, 2, 4));
      }
    }
  }
  
  hub75_display.show();
}

// ============================================================
// Main Demo Task
// ============================================================

void demoTask(void* pvParameters) {
  ESP_LOGI(TAG, "Starting SDF Morphing Demo");
  
  int frame_count = 0;
  int64_t start_time = esp_timer_get_time();
  int64_t last_fps_time = start_time;
  
  while (1) {
    // Calculate animation time
    int64_t now = esp_timer_get_time();
    float time = (now - start_time) / 1000000.0f;
    
    // Render frame
    renderFrame(time);
    frame_count++;
    
    // Print FPS every 5 seconds
    if (now - last_fps_time >= 5000000) {
      float fps = frame_count * 1000000.0f / (now - last_fps_time);
      ESP_LOGI(TAG, "FPS: %.1f, Frame: %d, Time: %.1fs", fps, frame_count, time);
      last_fps_time = now;
      frame_count = 0;
    }
    
    // Target ~30 FPS (but may be slower due to rendering)
    vTaskDelay(1);
  }
}

// ============================================================
// Main Entry
// ============================================================

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "  GPU ADVANCED DEMO");
  ESP_LOGI(TAG, "  SDF Morphing + Antialiasing + Shader");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "");
  
  // Initialize HUB75
  ESP_LOGI(TAG, "Initializing HUB75 display...");
  if (hub75_display.begin()) {
    hub75_display.setBrightness(200);
    ESP_LOGI(TAG, "HUB75 initialized: %dx%d", hub75_display.getWidth(), hub75_display.getHeight());
  } else {
    ESP_LOGE(TAG, "Failed to initialize HUB75!");
    return;
  }
  
  // Clear to show we're ready
  hub75_display.fill(RGB(0, 32, 0));
  hub75_display.show();
  vTaskDelay(pdMS_TO_TICKS(500));
  
  ESP_LOGI(TAG, "Shape 1: %d vertices", shape1_count);
  ESP_LOGI(TAG, "Shape 2: %d vertices", shape2_count);
  ESP_LOGI(TAG, "Starting demo...");
  
  // Create demo task
  xTaskCreatePinnedToCore(demoTask, "DemoTask", 8192, NULL, 5, NULL, 1);
}
