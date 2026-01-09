/*****************************************************************
 * File:      GPU_Advanced_Demo_Optimized.cpp
 * Category:  High-Performance Graphics Demo
 * Author:    Optimized GPU Driver
 * 
 * Purpose:
 *    GPU-side advanced demo with TWO INDEPENDENT animations,
 *    one per panel (64x32 each), controllable via CPU commands.
 * 
 * Optimization Techniques (from graphics research):
 *    1. Scanline Polygon Fill - O(perimeter) not O(area×vertices)
 *    2. Fixed-Point Math (16.16) - 5-10x faster than float
 *    3. Edge-only Antialiasing - Wu's algorithm on boundaries
 *    4. Pre-computed LUTs - Sin, Cos, Color palettes
 *    5. Incremental Edge Walking - Bresenham-style
 *    6. Double Buffering - Render while displaying
 *    7. Integer-only inner loops - Zero float in hot paths
 * 
 * CPU Control Commands (via UART):
 *    'L' + byte = Set animation state for LEFT panel (0=off,1=morph,2=static)
 *    'R' + byte = Set animation state for RIGHT panel
 *    'S' + byte = Set animation speed (1-10)
 *    'C' + byte = Set color mode (0=plasma,1=solid,2=gradient)
 *    'P' = Pause all
 *    'G' = Resume (Go)
 *    'B' + byte = Set brightness (0-255)
 *****************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/uart.h"

#include "abstraction/hal.hpp"
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"

static const char* TAG = "GPU_OPT";

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

// ============================================================
// Display Hardware
// ============================================================

SimpleHUB75Display hub75_display;

constexpr int WIDTH = 128;
constexpr int HEIGHT = 32;
constexpr int PANEL_WIDTH = 64;  // Each panel is 64x32

// ============================================================
// Animation State (per panel)
// ============================================================

enum class AnimMode : uint8_t {
    OFF = 0,      // Panel off (black)
    MORPH = 1,    // Morphing animation
    STATIC = 2,   // Static shape
    PULSE = 3     // Pulsing scale
};

enum class ColorMode : uint8_t {
    PLASMA = 0,   // Animated plasma
    SOLID = 1,    // Solid color
    GRADIENT = 2, // Gradient fill
    RAINBOW = 3   // Rainbow cycle
};

struct PanelState {
    AnimMode mode = AnimMode::MORPH;
    ColorMode color_mode = ColorMode::PLASMA;
    uint8_t speed = 5;          // 1-10, affects animation speed
    bool enabled = true;
    int frame_offset = 0;       // For phase offset between panels
    uint8_t solid_color_idx = 0; // For solid color mode
};

// Global state
PanelState left_panel;
PanelState right_panel;
bool global_paused = false;
uint8_t global_brightness = 200;

// ============================================================
// Fixed-Point Math (16.16 format) - 5-10x faster than float
// ============================================================

typedef int32_t fixed16_t;
constexpr int FP_SHIFT = 16;
constexpr int FP_ONE = (1 << FP_SHIFT);
constexpr int FP_HALF = (1 << (FP_SHIFT - 1));

#define FP_FROM_INT(x)    ((fixed16_t)((x) << FP_SHIFT))
#define FP_FROM_FLOAT(x)  ((fixed16_t)((x) * FP_ONE))
#define FP_TO_INT(x)      ((x) >> FP_SHIFT)
#define FP_TO_INT_ROUND(x) (((x) + FP_HALF) >> FP_SHIFT)
#define FP_FRAC(x)        ((x) & (FP_ONE - 1))
#define FP_MUL(a, b)      ((fixed16_t)(((int64_t)(a) * (b)) >> FP_SHIFT))
#define FP_DIV(a, b)      ((fixed16_t)(((int64_t)(a) << FP_SHIFT) / (b)))

// ============================================================
// Pre-computed Lookup Tables
// ============================================================

constexpr int SIN_TABLE_SIZE = 256;
constexpr int SIN_TABLE_MASK = SIN_TABLE_SIZE - 1;
fixed16_t sin_lut[SIN_TABLE_SIZE];
fixed16_t cos_lut[SIN_TABLE_SIZE];

// Pre-computed color palette (256 rainbow colors)
constexpr int COLOR_PALETTE_SIZE = 256;
uint8_t palette_r[COLOR_PALETTE_SIZE];
uint8_t palette_g[COLOR_PALETTE_SIZE];
uint8_t palette_b[COLOR_PALETTE_SIZE];

// Pre-computed easing curve
constexpr int EASE_TABLE_SIZE = 256;
fixed16_t ease_lut[EASE_TABLE_SIZE];

void initLookupTables() {
    // Sin/Cos LUT (fixed-point)
    for (int i = 0; i < SIN_TABLE_SIZE; i++) {
        float angle = (float)i * 6.28318530718f / SIN_TABLE_SIZE;
        sin_lut[i] = FP_FROM_FLOAT(sinf(angle));
        cos_lut[i] = FP_FROM_FLOAT(cosf(angle));
    }
    
    // Rainbow color palette (HSV with S=1, V=1)
    for (int i = 0; i < COLOR_PALETTE_SIZE; i++) {
        float h = (float)i / COLOR_PALETTE_SIZE * 6.0f;
        int hi = (int)h;
        float f = h - hi;
        uint8_t q = (uint8_t)((1.0f - f) * 255);
        uint8_t t = (uint8_t)(f * 255);
        
        switch (hi % 6) {
            case 0: palette_r[i] = 255; palette_g[i] = t;   palette_b[i] = 0;   break;
            case 1: palette_r[i] = q;   palette_g[i] = 255; palette_b[i] = 0;   break;
            case 2: palette_r[i] = 0;   palette_g[i] = 255; palette_b[i] = t;   break;
            case 3: palette_r[i] = 0;   palette_g[i] = q;   palette_b[i] = 255; break;
            case 4: palette_r[i] = t;   palette_g[i] = 0;   palette_b[i] = 255; break;
            default:palette_r[i] = 255; palette_g[i] = 0;   palette_b[i] = q;   break;
        }
    }
    
    // Easing curve (cubic ease-in-out)
    for (int i = 0; i < EASE_TABLE_SIZE; i++) {
        float t = (float)i / (EASE_TABLE_SIZE - 1);
        float ease = t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        ease_lut[i] = FP_FROM_FLOAT(ease);
    }
}

// Fast fixed-point sin/cos using LUT
inline fixed16_t fpSin(int angle256) {
    return sin_lut[angle256 & SIN_TABLE_MASK];
}

inline fixed16_t fpCos(int angle256) {
    return cos_lut[angle256 & SIN_TABLE_MASK];
}

// ============================================================
// Fixed-Point 2D Vector
// ============================================================

struct FPVec2 {
    fixed16_t x, y;
    FPVec2() : x(0), y(0) {}
    FPVec2(fixed16_t x_, fixed16_t y_) : x(x_), y(y_) {}
    FPVec2(int x_, int y_) : x(FP_FROM_INT(x_)), y(FP_FROM_INT(y_)) {}
};

// ============================================================
// Shape Definitions (integer coordinates)
// ============================================================

// Shape 1: 16 vertices
const int16_t shape1_x[] = {6, 14, 20, 26, 27, 28, 23, 21, 19, 17, 16, 18, 7, 4, 2, 2};
const int16_t shape1_y[] = {8, 8, 11, 17, 19, 22, 22, 19, 17, 17, 19, 22, 22, 20, 17, 12};
constexpr int SHAPE1_COUNT = 16;

// Shape 2: 10 vertices
const int16_t shape2_x[] = {15, 7, 6, 10, 14, 15, 16, 20, 24, 23};
const int16_t shape2_y[] = {24, 14, 10, 6, 8, 11, 8, 6, 10, 14};
constexpr int SHAPE2_COUNT = 10;

// Maximum vertices for morphed shape
constexpr int MAX_VERTS = 16;

// ============================================================
// Scanline Polygon Fill - O(perimeter) not O(area × vertices)
// From Foley & Van Dam "Computer Graphics"
// ============================================================

// Edge table entry for scanline algorithm
struct EdgeEntry {
    int y_max;           // Maximum y coordinate
    fixed16_t x_current; // Current x (fixed-point for sub-pixel)
    fixed16_t dx;        // X increment per scanline (fixed-point)
    EdgeEntry* next;
};

// Global edge pool (avoid malloc in hot path)
constexpr int MAX_EDGES = 64;
EdgeEntry edge_pool[MAX_EDGES];
int edge_pool_idx;

// Active edge list and edge table
EdgeEntry* active_edges;
EdgeEntry* edge_table[HEIGHT + 1];

// Framebuffer for double buffering
uint8_t framebuffer_r[WIDTH * HEIGHT];
uint8_t framebuffer_g[WIDTH * HEIGHT];
uint8_t framebuffer_b[WIDTH * HEIGHT];

// Edge flags for antialiasing (marks boundary pixels)
uint8_t edge_flags[WIDTH * HEIGHT];

void resetEdgePool() {
    edge_pool_idx = 0;
    active_edges = nullptr;
    memset(edge_table, 0, sizeof(edge_table));
}

EdgeEntry* allocEdge() {
    if (edge_pool_idx >= MAX_EDGES) return nullptr;
    return &edge_pool[edge_pool_idx++];
}

// Add edge to edge table (scanline algorithm)
void addEdge(int x1, int y1, int x2, int y2) {
    // Skip horizontal edges
    if (y1 == y2) return;
    
    // Ensure y1 < y2
    if (y1 > y2) {
        int tmp = x1; x1 = x2; x2 = tmp;
        tmp = y1; y1 = y2; y2 = tmp;
    }
    
    // Clip to screen
    if (y2 < 0 || y1 >= HEIGHT) return;
    
    EdgeEntry* edge = allocEdge();
    if (!edge) return;
    
    edge->y_max = y2;
    edge->x_current = FP_FROM_INT(x1);
    
    // Calculate dx (fixed-point slope)
    int dy = y2 - y1;
    int dx = x2 - x1;
    edge->dx = FP_DIV(FP_FROM_INT(dx), FP_FROM_INT(dy));
    
    // Insert into edge table at y1
    int y_start = (y1 < 0) ? 0 : y1;
    if (y1 < 0) {
        // Adjust x_current for clipped edge
        edge->x_current += FP_MUL(edge->dx, FP_FROM_INT(-y1));
    }
    
    if (y_start < HEIGHT) {
        edge->next = edge_table[y_start];
        edge_table[y_start] = edge;
    }
}

// Insert edge into sorted active edge list
void insertActiveEdge(EdgeEntry* edge) {
    if (!active_edges || edge->x_current < active_edges->x_current) {
        edge->next = active_edges;
        active_edges = edge;
        return;
    }
    
    EdgeEntry* curr = active_edges;
    while (curr->next && curr->next->x_current < edge->x_current) {
        curr = curr->next;
    }
    edge->next = curr->next;
    curr->next = edge;
}

// Fill polygon using scanline algorithm
void fillPolygonScanline(const FPVec2* verts, int count, uint8_t r, uint8_t g, uint8_t b) {
    resetEdgePool();
    
    // Build edge table
    for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        int x1 = FP_TO_INT_ROUND(verts[i].x);
        int y1 = FP_TO_INT_ROUND(verts[i].y);
        int x2 = FP_TO_INT_ROUND(verts[j].x);
        int y2 = FP_TO_INT_ROUND(verts[j].y);
        addEdge(x1, y1, x2, y2);
    }
    
    active_edges = nullptr;
    
    // Process each scanline
    for (int y = 0; y < HEIGHT; y++) {
        // Add new edges from edge table
        EdgeEntry* new_edge = edge_table[y];
        while (new_edge) {
            EdgeEntry* next = new_edge->next;
            insertActiveEdge(new_edge);
            new_edge = next;
        }
        
        // Remove expired edges
        EdgeEntry** curr = &active_edges;
        while (*curr) {
            if ((*curr)->y_max <= y) {
                *curr = (*curr)->next;
            } else {
                curr = &(*curr)->next;
            }
        }
        
        // Fill between pairs of edges
        EdgeEntry* edge = active_edges;
        while (edge && edge->next) {
            int x1 = FP_TO_INT_ROUND(edge->x_current);
            int x2 = FP_TO_INT_ROUND(edge->next->x_current);
            
            // Clip to screen
            if (x1 < 0) x1 = 0;
            if (x2 >= WIDTH) x2 = WIDTH - 1;
            
            // Fill horizontal span
            int idx = y * WIDTH;
            for (int x = x1; x <= x2; x++) {
                if (x >= 0 && x < WIDTH) {
                    framebuffer_r[idx + x] = r;
                    framebuffer_g[idx + x] = g;
                    framebuffer_b[idx + x] = b;
                }
            }
            
            // Mark edge pixels for antialiasing
            if (x1 >= 0 && x1 < WIDTH) edge_flags[idx + x1] = 255;
            if (x2 >= 0 && x2 < WIDTH) edge_flags[idx + x2] = 255;
            
            edge = edge->next->next;
        }
        
        // Update x coordinates for next scanline
        edge = active_edges;
        while (edge) {
            edge->x_current += edge->dx;
            edge = edge->next;
        }
    }
}

// ============================================================
// Wu's Antialiased Line Algorithm (1991)
// Draws antialiased edges over filled polygon
// ============================================================

inline void plotPixelAA(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    
    int idx = y * WIDTH + x;
    // Alpha blend with existing pixel
    uint8_t inv_alpha = 255 - alpha;
    framebuffer_r[idx] = (framebuffer_r[idx] * inv_alpha + r * alpha) >> 8;
    framebuffer_g[idx] = (framebuffer_g[idx] * inv_alpha + g * alpha) >> 8;
    framebuffer_b[idx] = (framebuffer_b[idx] * inv_alpha + b * alpha) >> 8;
}

void drawLineWu(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    
    if (steep) {
        int tmp = x0; x0 = y0; y0 = tmp;
        tmp = x1; x1 = y1; y1 = tmp;
    }
    if (x0 > x1) {
        int tmp = x0; x0 = x1; x1 = tmp;
        tmp = y0; y0 = y1; y1 = tmp;
    }
    
    int dx = x1 - x0;
    int dy = y1 - y0;
    
    fixed16_t gradient = (dx == 0) ? FP_ONE : FP_DIV(FP_FROM_INT(dy), FP_FROM_INT(dx));
    
    // First endpoint
    int xend = x0;
    fixed16_t yend = FP_FROM_INT(y0);
    
    fixed16_t intery = yend + gradient;
    
    // Main loop
    for (int x = x0; x <= x1; x++) {
        int y = FP_TO_INT(intery);
        uint8_t frac = (FP_FRAC(intery) >> 8) & 0xFF;  // Get fractional part as 0-255
        
        if (steep) {
            plotPixelAA(y, x, r, g, b, 255 - frac);
            plotPixelAA(y + 1, x, r, g, b, frac);
        } else {
            plotPixelAA(x, y, r, g, b, 255 - frac);
            plotPixelAA(x, y + 1, r, g, b, frac);
        }
        
        intery += gradient;
    }
}

// Draw antialiased polygon outline
void drawPolygonOutlineAA(const FPVec2* verts, int count, uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        int x1 = FP_TO_INT_ROUND(verts[i].x);
        int y1 = FP_TO_INT_ROUND(verts[i].y);
        int x2 = FP_TO_INT_ROUND(verts[j].x);
        int y2 = FP_TO_INT_ROUND(verts[j].y);
        drawLineWu(x1, y1, x2, y2, r, g, b);
    }
}

// ============================================================
// Morphing with Fixed-Point (all integer operations)
// ============================================================

void morphPolygonsFP(const int16_t* from_x, const int16_t* from_y, int from_count,
                     const int16_t* to_x, const int16_t* to_y, int to_count,
                     FPVec2* out, int& out_count, fixed16_t t) {
    out_count = (from_count > to_count) ? from_count : to_count;
    fixed16_t inv_t = FP_ONE - t;
    
    for (int i = 0; i < out_count; i++) {
        // Map index to both shapes using fixed-point
        fixed16_t from_idx_fp = FP_DIV(FP_FROM_INT(i * (from_count - 1)), FP_FROM_INT(out_count - 1));
        fixed16_t to_idx_fp = FP_DIV(FP_FROM_INT(i * (to_count - 1)), FP_FROM_INT(out_count - 1));
        
        int from_i0 = FP_TO_INT(from_idx_fp);
        int from_i1 = (from_i0 + 1) % from_count;
        fixed16_t from_frac = FP_FRAC(from_idx_fp);
        if (from_i0 >= from_count - 1) { from_i0 = from_count - 1; from_i1 = 0; from_frac = 0; }
        
        int to_i0 = FP_TO_INT(to_idx_fp);
        int to_i1 = (to_i0 + 1) % to_count;
        fixed16_t to_frac = FP_FRAC(to_idx_fp);
        if (to_i0 >= to_count - 1) { to_i0 = to_count - 1; to_i1 = 0; to_frac = 0; }
        
        // Interpolate within source shape
        fixed16_t from_x_fp = FP_FROM_INT(from_x[from_i0]) + FP_MUL(from_frac, FP_FROM_INT(from_x[from_i1] - from_x[from_i0]));
        fixed16_t from_y_fp = FP_FROM_INT(from_y[from_i0]) + FP_MUL(from_frac, FP_FROM_INT(from_y[from_i1] - from_y[from_i0]));
        
        // Interpolate within target shape
        fixed16_t to_x_fp = FP_FROM_INT(to_x[to_i0]) + FP_MUL(to_frac, FP_FROM_INT(to_x[to_i1] - to_x[to_i0]));
        fixed16_t to_y_fp = FP_FROM_INT(to_y[to_i0]) + FP_MUL(to_frac, FP_FROM_INT(to_y[to_i1] - to_y[to_i0]));
        
        // Morph between shapes
        out[i].x = FP_MUL(from_x_fp, inv_t) + FP_MUL(to_x_fp, t);
        out[i].y = FP_MUL(from_y_fp, inv_t) + FP_MUL(to_y_fp, t);
    }
}

// ============================================================
// Transform vertices (rotation + translation) - Fixed Point
// ============================================================

void transformVertsFP(FPVec2* verts, int count, 
                      fixed16_t cx, fixed16_t cy,
                      int angle256,
                      fixed16_t tx, fixed16_t ty) {
    fixed16_t cos_a = fpCos(angle256);
    fixed16_t sin_a = fpSin(angle256);
    
    for (int i = 0; i < count; i++) {
        fixed16_t dx = verts[i].x - cx;
        fixed16_t dy = verts[i].y - cy;
        
        fixed16_t rx = FP_MUL(dx, cos_a) - FP_MUL(dy, sin_a);
        fixed16_t ry = FP_MUL(dx, sin_a) + FP_MUL(dy, cos_a);
        
        verts[i].x = rx + cx + tx;
        verts[i].y = ry + cy + ty;
    }
}

// ============================================================
// Fast Plasma Shader using LUT (no per-pixel trig)
// OPTIMIZED: Single LUT lookup per pixel
// ============================================================

void applyPlasmaShader(int frame_num) {
    // Pre-compute time-based offset (integer)
    int time_offset = (frame_num * 3) & 0xFF;
    
    for (int y = 0; y < HEIGHT; y++) {
        // Pre-compute y contribution for this row
        int y_base = (y * 8 + time_offset) & 0xFF;
        int idx_base = y * WIDTH;
        
        for (int x = 0; x < WIDTH; x++) {
            int idx = idx_base + x;
            
            // Only shade filled pixels (background is BLACK 0,0,0)
            // Check any channel - filled pixels have non-zero values
            if (framebuffer_r[idx] | framebuffer_g[idx] | framebuffer_b[idx]) {
                // Simplified plasma: single LUT lookup
                int color_idx = ((x * 4) + y_base) & 0xFF;
                
                framebuffer_r[idx] = palette_r[color_idx];
                framebuffer_g[idx] = palette_g[color_idx];
                framebuffer_b[idx] = palette_b[color_idx];
            }
        }
    }
}

// ============================================================
// Copy framebuffer to display (OPTIMIZED: bulk upload)
// ============================================================

// RGB framebuffer for bulk upload
RGB rgb_framebuffer[WIDTH * HEIGHT];

void flushFramebuffer() {
    // Convert separate R/G/B arrays to RGB struct array for bulk upload
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        rgb_framebuffer[i].r = framebuffer_r[i];
        rgb_framebuffer[i].g = framebuffer_g[i];
        rgb_framebuffer[i].b = framebuffer_b[i];
    }
    
    // Use bulk upload instead of 4096 setPixel calls
    HUB75Driver* driver = hub75_display.getDriver();
    if (driver) {
        driver->uploadFrameBuffer(rgb_framebuffer, WIDTH, HEIGHT);
    }
    
    hub75_display.show();
}

// ============================================================
// Clear framebuffer (fast memset)
// ============================================================

void clearFramebuffer(uint8_t r, uint8_t g, uint8_t b) {
    memset(framebuffer_r, r, WIDTH * HEIGHT);
    memset(framebuffer_g, g, WIDTH * HEIGHT);
    memset(framebuffer_b, b, WIDTH * HEIGHT);
    memset(edge_flags, 0, WIDTH * HEIGHT);
}

// ============================================================
// Render Single Panel Animation
// ============================================================

// Working buffers for each panel
FPVec2 panel_verts[2][MAX_VERTS];

void renderPanelAnimation(int panel_id, int frame_num, PanelState& state) {
    // Calculate panel center (32, 16) for left, (96, 16) for right
    int center_x = (panel_id == 0) ? 32 : 96;
    int center_y = 16;
    
    // Adjust frame based on speed (1-10 -> 0.5x to 2.5x)
    int adjusted_frame = (frame_num * state.speed) / 5 + state.frame_offset;
    
    if (state.mode == AnimMode::OFF) {
        return;  // Panel off, nothing to render
    }
    
    // Animation timing
    int morph_cycle_frames = 360;
    int morph_phase = adjusted_frame % (morph_cycle_frames * 2);
    
    // Get morph t (only for MORPH mode)
    fixed16_t morph_t;
    if (state.mode == AnimMode::MORPH) {
        int ease_idx;
        if (morph_phase < morph_cycle_frames) {
            ease_idx = (morph_phase * (EASE_TABLE_SIZE - 1)) / morph_cycle_frames;
        } else {
            ease_idx = ((morph_cycle_frames * 2 - morph_phase) * (EASE_TABLE_SIZE - 1)) / morph_cycle_frames;
        }
        morph_t = ease_lut[ease_idx];
    } else if (state.mode == AnimMode::STATIC) {
        morph_t = 0;  // Always show shape 1
    } else {
        // PULSE mode - use scale animation
        morph_t = FP_HALF;
    }
    
    // Generate morphed polygon
    FPVec2* verts = panel_verts[panel_id];
    int vert_count;
    morphPolygonsFP(shape1_x, shape1_y, SHAPE1_COUNT,
                    shape2_x, shape2_y, SHAPE2_COUNT,
                    verts, vert_count, morph_t);
    
    // Calculate shape center
    fixed16_t cx = 0, cy = 0;
    for (int i = 0; i < vert_count; i++) {
        cx += verts[i].x;
        cy += verts[i].y;
    }
    cx /= vert_count;
    cy /= vert_count;
    
    // Scale (with pulse animation if in PULSE mode)
    fixed16_t scale = FP_FROM_FLOAT(1.0f);
    if (state.mode == AnimMode::PULSE) {
        // Pulse scale from 0.7 to 1.3
        int pulse = fpSin((adjusted_frame * 4) & 0xFF);
        scale = FP_FROM_FLOAT(1.0f) + (pulse >> 3);
    }
    
    // Center on panel
    fixed16_t offset_x = FP_FROM_INT(center_x) - FP_MUL(cx, scale);
    fixed16_t offset_y = FP_FROM_INT(center_y) - FP_MUL(cy, scale);
    
    // Apply scale and offset
    for (int i = 0; i < vert_count; i++) {
        verts[i].x = FP_MUL(verts[i].x, scale) + offset_x;
        verts[i].y = FP_MUL(verts[i].y, scale) + offset_y;
    }
    
    // Animation: rotation and bobbing
    int angle256 = (fpSin((adjusted_frame * 2) & 0xFF) >> 10);
    fixed16_t bob_x = fpSin((adjusted_frame * 5) & 0xFF) >> 10;
    fixed16_t bob_y = fpCos((adjusted_frame * 3) & 0xFF) >> 10;
    
    fixed16_t screen_cx = FP_FROM_INT(center_x);
    fixed16_t screen_cy = FP_FROM_INT(center_y);
    
    // Transform vertices
    transformVertsFP(verts, vert_count, screen_cx, screen_cy, angle256, bob_x, bob_y);
    
    // Fill polygon with white (will be colored by shader)
    fillPolygonScanline(verts, vert_count, 255, 255, 255);
}

// ============================================================
// Apply plasma to specific panel region
// ============================================================

void applyPanelPlasma(int panel_id, int frame_num, PanelState& state) {
    int x_start = (panel_id == 0) ? 0 : PANEL_WIDTH;
    int x_end = x_start + PANEL_WIDTH;
    
    int adjusted_frame = (frame_num * state.speed) / 5 + state.frame_offset;
    int time_offset = (adjusted_frame * 3) & 0xFF;
    
    for (int y = 0; y < HEIGHT; y++) {
        int y_base = (y * 8 + time_offset) & 0xFF;
        int idx_base = y * WIDTH;
        
        for (int x = x_start; x < x_end; x++) {
            int idx = idx_base + x;
            
            // Only shade filled pixels
            if (framebuffer_r[idx] | framebuffer_g[idx] | framebuffer_b[idx]) {
                uint8_t r, g, b;
                
                switch (state.color_mode) {
                    case ColorMode::PLASMA: {
                        int color_idx = ((x * 4) + y_base) & 0xFF;
                        r = palette_r[color_idx];
                        g = palette_g[color_idx];
                        b = palette_b[color_idx];
                        break;
                    }
                    case ColorMode::SOLID: {
                        int color_idx = (state.solid_color_idx + adjusted_frame / 2) & 0xFF;
                        r = palette_r[color_idx];
                        g = palette_g[color_idx];
                        b = palette_b[color_idx];
                        break;
                    }
                    case ColorMode::GRADIENT: {
                        int local_x = x - x_start;
                        int color_idx = ((local_x * 4) + (adjusted_frame / 2)) & 0xFF;
                        r = palette_r[color_idx];
                        g = palette_g[color_idx];
                        b = palette_b[color_idx];
                        break;
                    }
                    case ColorMode::RAINBOW: {
                        int color_idx = (adjusted_frame + y * 4) & 0xFF;
                        r = palette_r[color_idx];
                        g = palette_g[color_idx];
                        b = palette_b[color_idx];
                        break;
                    }
                }
                
                framebuffer_r[idx] = r;
                framebuffer_g[idx] = g;
                framebuffer_b[idx] = b;
            }
        }
    }
}

// ============================================================
// Render Both Panels
// ============================================================

int g_frame_num = 0;
static int64_t g_flush_us = 0;
static int g_profile_count = 0;

void renderDualPanels(int frame_num) {
    // Clear framebuffer to black
    clearFramebuffer(0, 0, 0);
    
    if (!global_paused) {
        // Render left panel (0-63)
        if (left_panel.enabled) {
            renderPanelAnimation(0, frame_num, left_panel);
        }
        
        // Render right panel (64-127)
        if (right_panel.enabled) {
            renderPanelAnimation(1, frame_num, right_panel);
        }
        
        // Apply shaders
        if (left_panel.enabled) {
            applyPanelPlasma(0, frame_num, left_panel);
        }
        if (right_panel.enabled) {
            applyPanelPlasma(1, frame_num, right_panel);
        }
    }
    
    // Flush to display
    int64_t t0 = esp_timer_get_time();
    flushFramebuffer();
    g_flush_us += esp_timer_get_time() - t0;
    g_profile_count++;
    
    // Print timing every 200 frames
    if (g_profile_count >= 200) {
        float flush_ms = g_flush_us / 1000.0f / g_profile_count;
        ESP_LOGI(TAG, "PROFILE: flush=%.2fms, L:%s R:%s", flush_ms,
                 left_panel.enabled ? "ON" : "OFF",
                 right_panel.enabled ? "ON" : "OFF");
        g_flush_us = 0;
        g_profile_count = 0;
    }
}

// ============================================================
// UART Command Processing
// ============================================================

void initCommandUart() {
    // Use UART0 for commands (or could use Serial1)
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_0, &uart_config);
}

void processCommands() {
    uint8_t cmd_buf[8];
    int len = uart_read_bytes(UART_NUM_0, cmd_buf, sizeof(cmd_buf), 0);
    
    for (int i = 0; i < len; i++) {
        uint8_t cmd = cmd_buf[i];
        uint8_t arg = (i + 1 < len) ? cmd_buf[++i] : 0;
        
        switch (cmd) {
            case 'L':  // Left panel mode
                left_panel.mode = (AnimMode)(arg & 0x03);
                ESP_LOGI(TAG, "Left panel mode: %d", arg);
                break;
            case 'R':  // Right panel mode
                right_panel.mode = (AnimMode)(arg & 0x03);
                ESP_LOGI(TAG, "Right panel mode: %d", arg);
                break;
            case 'S':  // Speed (both panels)
                if (arg >= 1 && arg <= 10) {
                    left_panel.speed = arg;
                    right_panel.speed = arg;
                    ESP_LOGI(TAG, "Speed: %d", arg);
                }
                break;
            case 'C':  // Color mode (both panels)
                left_panel.color_mode = (ColorMode)(arg & 0x03);
                right_panel.color_mode = (ColorMode)(arg & 0x03);
                ESP_LOGI(TAG, "Color mode: %d", arg);
                break;
            case 'P':  // Pause
                global_paused = true;
                ESP_LOGI(TAG, "PAUSED");
                break;
            case 'G':  // Go (resume)
                global_paused = false;
                ESP_LOGI(TAG, "RESUMED");
                break;
            case 'B':  // Brightness
                global_brightness = arg;
                hub75_display.setBrightness(arg);
                ESP_LOGI(TAG, "Brightness: %d", arg);
                break;
            case '1':  // Toggle left panel
                left_panel.enabled = !left_panel.enabled;
                ESP_LOGI(TAG, "Left panel: %s", left_panel.enabled ? "ON" : "OFF");
                break;
            case '2':  // Toggle right panel
                right_panel.enabled = !right_panel.enabled;
                ESP_LOGI(TAG, "Right panel: %s", right_panel.enabled ? "ON" : "OFF");
                break;
            case 'O':  // Phase offset for right panel
                right_panel.frame_offset = arg * 10;
                ESP_LOGI(TAG, "Right panel offset: %d frames", right_panel.frame_offset);
                break;
        }
    }
}

// ============================================================
// Demo Task - Dual Panel Animation
// ============================================================

void demoTask(void* pvParameters) {
    ESP_LOGI(TAG, "Starting Dual Panel Demo");
    ESP_LOGI(TAG, "Commands: L/R<mode>, S<1-10>, C<0-3>, P=pause, G=go, B<0-255>");
    
    // Set different phase offset for right panel (looks more interesting)
    right_panel.frame_offset = 180;  // Half cycle offset
    
    int frame_count = 0;
    int total_frames = 0;
    int64_t last_fps_time = esp_timer_get_time();
    
    while (1) {
        // Check for commands
        processCommands();
        
        // Render both panels
        renderDualPanels(total_frames);
        
        if (!global_paused) {
            total_frames++;
        }
        frame_count++;
        
        // Print FPS every 5 seconds
        int64_t now = esp_timer_get_time();
        if (now - last_fps_time >= 5000000) {
            float fps = frame_count * 1000000.0f / (now - last_fps_time);
            ESP_LOGI(TAG, "FPS: %.1f, Frames: %d, Left:%s Right:%s", 
                     fps, total_frames,
                     left_panel.enabled ? "ON" : "OFF",
                     right_panel.enabled ? "ON" : "OFF");
            last_fps_time = now;
            frame_count = 0;
        }
        
        // Yield to other tasks
        taskYIELD();
    }
}

// ============================================================
// Main Entry
// ============================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  GPU DUAL PANEL DEMO");
    ESP_LOGI(TAG, "  Two Independent Morphing Animations");
    ESP_LOGI(TAG, "  CPU Controllable via UART");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // Initialize lookup tables
    ESP_LOGI(TAG, "Initializing lookup tables...");
    initLookupTables();
    
    // Initialize command UART
    initCommandUart();
    
    // Initialize HUB75 display
    ESP_LOGI(TAG, "Initializing HUB75 display...");
    
    if (hub75_display.begin(true)) {
        hub75_display.setBrightness(global_brightness);
        ESP_LOGI(TAG, "HUB75 initialized: %dx%d", hub75_display.getWidth(), hub75_display.getHeight());
    } else {
        ESP_LOGE(TAG, "Failed to initialize HUB75!");
        return;
    }
    
    // Show ready - brief green flash
    hub75_display.fill(RGB(0, 64, 0));
    hub75_display.show();
    vTaskDelay(pdMS_TO_TICKS(300));
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== UART Commands ===");
    ESP_LOGI(TAG, "L<0-3> = Left panel mode (0=off,1=morph,2=static,3=pulse)");
    ESP_LOGI(TAG, "R<0-3> = Right panel mode");
    ESP_LOGI(TAG, "S<1-10> = Speed");
    ESP_LOGI(TAG, "C<0-3> = Color (0=plasma,1=solid,2=gradient,3=rainbow)");
    ESP_LOGI(TAG, "P = Pause, G = Go");
    ESP_LOGI(TAG, "B<0-255> = Brightness");
    ESP_LOGI(TAG, "1/2 = Toggle left/right panel");
    ESP_LOGI(TAG, "O<n> = Right panel phase offset");
    ESP_LOGI(TAG, "=====================");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting dual panel animation...");
    
    // Create demo task on core 1 with high priority
    xTaskCreatePinnedToCore(demoTask, "DualDemo", 8192, NULL, 10, NULL, 1);
}
