/**
 * @file GPU_Programmable.cpp
 * @brief Fully Programmable GPU - No hardcoded effects
 * 
 * Architecture:
 * - Shader slots: CPU uploads bytecode programs
 * - Sprite bank: CPU uploads bitmap sprites  
 * - Variables: CPU sets values, shaders read them
 * - Registers: Runtime computation in shaders
 * - Framebuffers: HUB75 (128x32 RGB) + OLED (128x128 mono)
 * 
 * The GPU starts EMPTY - all effects come from CPU-uploaded shaders
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

// ARCOS HUB75 driver
#include "abstraction/hal.hpp"
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"

// OLED driver
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

// Import types from ARCOS namespaces
using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

static const char* TAG = "GPU_PROG";

// Debug counters (atomics for cross-core visibility)
static std::atomic<uint32_t> dbg_hub75_presents{0};
static std::atomic<uint32_t> dbg_oled_presents{0};
static std::atomic<uint32_t> dbg_oled_updates{0};
static std::atomic<uint32_t> dbg_cmd_count{0};
static std::atomic<uint32_t> dbg_oled_cmd_count{0};
static std::atomic<int64_t> dbg_last_hub75_present{0};
static std::atomic<int64_t> dbg_last_oled_present{0};

// ============================================================
// Hardware Configuration
// ============================================================
constexpr int PANEL_WIDTH = 64;
constexpr int PANEL_HEIGHT = 32;
constexpr int NUM_PANELS = 2;
constexpr int TOTAL_WIDTH = PANEL_WIDTH * NUM_PANELS;  // 128
constexpr int TOTAL_HEIGHT = PANEL_HEIGHT;              // 32

constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 128;

// UART
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int UART_RX_PIN = 13;
constexpr int UART_TX_PIN = 12;
constexpr int UART_BAUD = 10000000;

// ============================================================
// GPU Memory Limits
// ============================================================
constexpr int MAX_SHADERS = 8;
constexpr int MAX_SHADER_SIZE = 1024;      // 1KB bytecode per shader
constexpr int MAX_SPRITES = 16;
constexpr int MAX_SPRITE_SIZE = 512;       // 512 bytes per sprite
constexpr int MAX_VARIABLES = 256;
constexpr int MAX_REGISTERS = 16;
constexpr int MAX_STACK = 16;              // Loop stack depth

// ============================================================
// Lookup Tables (256 entries, 0-255 output)
// ============================================================
static uint8_t SIN_LUT[256];
static uint8_t COS_LUT[256];
static uint8_t SQRT_LUT[256];

static void initLUTs() {
  for (int i = 0; i < 256; i++) {
    float angle = (i / 256.0f) * 2.0f * M_PI;
    SIN_LUT[i] = (uint8_t)(127.5f + 127.5f * sinf(angle));
    COS_LUT[i] = (uint8_t)(127.5f + 127.5f * cosf(angle));
    SQRT_LUT[i] = (uint8_t)(sqrtf(i / 255.0f) * 255.0f);
  }
}

// ============================================================
// Shader Bytecode Opcodes
// ============================================================
enum class Op : uint8_t {
  NOP = 0x00,
  HALT = 0x01,
  
  // Register operations
  SET = 0x10,      // SET Rd, imm16
  MOV = 0x11,      // MOV Rd, Rs
  LOAD = 0x12,     // LOAD Rd, var_id
  STORE = 0x13,    // STORE var_id, Rs
  
  // Arithmetic (all operate on 16-bit signed)
  ADD = 0x20,      // ADD Rd, Ra, Rb
  SUB = 0x21,      // SUB Rd, Ra, Rb
  MUL = 0x22,      // MUL Rd, Ra, Rb (result >> 8 for fixed-point)
  DIV = 0x23,      // DIV Rd, Ra, Rb
  MOD = 0x24,      // MOD Rd, Ra, Rb
  NEG = 0x25,      // NEG Rd, Rs
  ABS = 0x26,      // ABS Rd, Rs
  MIN = 0x27,      // MIN Rd, Ra, Rb
  MAX = 0x28,      // MAX Rd, Ra, Rb
  
  // Bitwise
  AND = 0x30,      // AND Rd, Ra, Rb
  OR = 0x31,       // OR Rd, Ra, Rb
  XOR = 0x32,      // XOR Rd, Ra, Rb
  NOT = 0x33,      // NOT Rd, Rs
  SHL = 0x34,      // SHL Rd, Rs, imm
  SHR = 0x35,      // SHR Rd, Rs, imm
  
  // LUT functions
  SIN = 0x40,      // SIN Rd, Rs (index into SIN_LUT)
  COS = 0x41,      // COS Rd, Rs
  SQRT = 0x42,     // SQRT Rd, Rs
  
  // Drawing (immediate values or registers)
  SETPX = 0x50,    // SETPX x, y, r, g, b
  GETPX = 0x51,    // GETPX Rd, x, y
  FILL = 0x52,     // FILL x, y, w, h, r, g, b
  LINE = 0x53,     // LINE x1, y1, x2, y2, r, g, b
  RECT = 0x54,     // RECT x, y, w, h, r, g, b (outline)
  CIRCLE = 0x55,   // CIRCLE cx, cy, r, r, g, b
  POLY = 0x56,     // POLY n_verts, vert_var_start, r, g, b
  SPRITE = 0x57,   // SPRITE id, x, y
  CLEAR = 0x58,    // CLEAR r, g, b
  
  // Control flow
  LOOP = 0x60,     // LOOP count_reg - begin loop
  ENDL = 0x61,     // ENDL - end loop
  JMP = 0x62,      // JMP offset (signed 16-bit)
  JZ = 0x63,       // JZ Rs, offset
  JNZ = 0x64,      // JNZ Rs, offset
  JGT = 0x65,      // JGT Rs, offset (if Rs > 0)
  JLT = 0x66,      // JLT Rs, offset (if Rs < 0)
  
  // Special
  GETX = 0x70,     // GETX Rd - get current pixel X (in pixel shader)
  GETY = 0x71,     // GETY Rd - get current pixel Y
  GETW = 0x72,     // GETW Rd - get framebuffer width
  GETH = 0x73,     // GETH Rd - get framebuffer height
  TIME = 0x74,     // TIME Rd - get milliseconds
  RAND = 0x75,     // RAND Rd - pseudo-random value
};

// ============================================================
// Shader Structure
// ============================================================
struct Shader {
  uint8_t bytecode[MAX_SHADER_SIZE];
  uint16_t length;
  bool valid;
};

// ============================================================
// Sprite Structure
// ============================================================
struct Sprite {
  uint8_t* data;      // RGB888 or 1-bit depending on target
  uint8_t width;
  uint8_t height;
  uint8_t format;     // 0=RGB888, 1=mono
  bool valid;
};

// ============================================================
// GPU State
// ============================================================
struct GPUState {
  // Shader slots
  Shader shaders[MAX_SHADERS];
  
  // Sprite bank
  Sprite sprites[MAX_SPRITES];
  
  // Variables (CPU-writable, shader-readable)
  int16_t variables[MAX_VARIABLES];
  
  // Runtime registers (shader-local)
  int16_t regs[MAX_REGISTERS];
  
  // Loop stack
  struct LoopFrame {
    uint16_t pc;      // Return address
    int16_t counter;  // Remaining iterations
  };
  LoopFrame loopStack[MAX_STACK];
  int loopSP;
  
  // Current pixel position (for pixel shaders)
  int16_t px, py;
  
  // Target framebuffer (0=HUB75, 1=OLED)
  uint8_t target;
  
  // Time
  uint32_t startTime;
  uint32_t frameCount;
  
  // Random seed
  uint32_t randSeed;
};

static GPUState gpu;

// ============================================================
// Framebuffers
// ============================================================
static uint8_t* hub75_buffer = nullptr;  // TOTAL_WIDTH * TOTAL_HEIGHT * 3
static uint8_t* oled_buffer = nullptr;   // OLED_WIDTH * OLED_HEIGHT / 8

constexpr int HUB75_BUFFER_SIZE = TOTAL_WIDTH * TOTAL_HEIGHT * 3;
constexpr int OLED_BUFFER_SIZE = OLED_WIDTH * OLED_HEIGHT / 8;

// ============================================================
// Hardware Instances
// ============================================================
using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

static SimpleHUB75Display* hub75 = nullptr;
static bool hub75_ok = false;

static DRIVER_OLED_SH1107* oled = nullptr;
static bool oled_ok = false;

// OLED update runs on Core 0 to avoid interfering with HUB75 DMA on Core 1
static volatile bool oled_update_pending = false;
static uint8_t* oled_update_buffer = nullptr;  // Double buffer for safe cross-core transfer

// Anti-aliasing enabled by default
static bool aa_enabled = true;

// ============================================================
// Pixel Operations
// ============================================================

// Alpha-blend a pixel (for anti-aliasing)
// alpha: 0-255, where 255 = fully opaque
static inline void blendPixelHUB75(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) {
  if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) return;
  if (alpha == 0) return;
  int idx = (y * TOTAL_WIDTH + x) * 3;
  if (alpha == 255) {
    hub75_buffer[idx + 0] = r;
    hub75_buffer[idx + 1] = g;
    hub75_buffer[idx + 2] = b;
  } else {
    // Linear blend: out = bg * (1 - a) + fg * a
    uint8_t inv_alpha = 255 - alpha;
    hub75_buffer[idx + 0] = (hub75_buffer[idx + 0] * inv_alpha + r * alpha) >> 8;
    hub75_buffer[idx + 1] = (hub75_buffer[idx + 1] * inv_alpha + g * alpha) >> 8;
    hub75_buffer[idx + 2] = (hub75_buffer[idx + 2] * inv_alpha + b * alpha) >> 8;
  }
}

static inline void setPixelHUB75(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) return;
  int idx = (y * TOTAL_WIDTH + x) * 3;
  hub75_buffer[idx + 0] = r;
  hub75_buffer[idx + 1] = g;
  hub75_buffer[idx + 2] = b;
}

static inline void getPixelHUB75(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) {
    r = g = b = 0;
    return;
  }
  int idx = (y * TOTAL_WIDTH + x) * 3;
  r = hub75_buffer[idx + 0];
  g = hub75_buffer[idx + 1];
  b = hub75_buffer[idx + 2];
}

static inline void setPixelOLED(int x, int y, bool on) {
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
  int byte_idx = (y / 8) * OLED_WIDTH + x;
  int bit = y % 8;
  if (on) {
    oled_buffer[byte_idx] |= (1 << bit);
  } else {
    oled_buffer[byte_idx] &= ~(1 << bit);
  }
}

static inline bool getPixelOLED(int x, int y) {
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return false;
  int byte_idx = (y / 8) * OLED_WIDTH + x;
  int bit = y % 8;
  return (oled_buffer[byte_idx] >> bit) & 1;
}

// Unified pixel set based on target
static inline void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (gpu.target == 0) {
    setPixelHUB75(x, y, r, g, b);
  } else {
    // Convert to mono: simple threshold
    bool on = (r + g + b) > 384;
    setPixelOLED(x, y, on);
  }
}

static inline uint32_t getPixel(int x, int y) {
  if (gpu.target == 0) {
    uint8_t r, g, b;
    getPixelHUB75(x, y, r, g, b);
    return (r << 16) | (g << 8) | b;
  } else {
    return getPixelOLED(x, y) ? 0xFFFFFF : 0x000000;
  }
}

// ============================================================
// Drawing Primitives (with Fast Anti-Aliasing)
// ============================================================

// Helper: Clamp float to 0-1 range
static inline float clamp01(float x) {
  return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

// Helper: Fractional part
static inline float fract(float x) {
  return x - floorf(x);
}

// Fast anti-aliased line using Xiaolin Wu algorithm with float coordinates
// This walks along the line and only touches pixels near the line (O(length) not O(area))
static void drawLineAA(float x0, float y0, float x1, float y1, uint8_t r, uint8_t g, uint8_t b) {
  bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
  
  if (steep) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (x0 > x1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }
  
  float dx = x1 - x0;
  float dy = y1 - y0;
  float gradient = (dx < 0.0001f) ? 1.0f : dy / dx;
  
  // First endpoint
  float xend = roundf(x0);
  float yend = y0 + gradient * (xend - x0);
  float xgap = 1.0f - fract(x0 + 0.5f);
  int xpxl1 = (int)xend;
  int ypxl1 = (int)floorf(yend);
  float frac1 = fract(yend);
  
  if (steep) {
    blendPixelHUB75(ypxl1, xpxl1, r, g, b, (uint8_t)((1.0f - frac1) * xgap * 255.0f));
    blendPixelHUB75(ypxl1 + 1, xpxl1, r, g, b, (uint8_t)(frac1 * xgap * 255.0f));
  } else {
    blendPixelHUB75(xpxl1, ypxl1, r, g, b, (uint8_t)((1.0f - frac1) * xgap * 255.0f));
    blendPixelHUB75(xpxl1, ypxl1 + 1, r, g, b, (uint8_t)(frac1 * xgap * 255.0f));
  }
  
  float intery = yend + gradient;
  
  // Second endpoint
  xend = roundf(x1);
  yend = y1 + gradient * (xend - x1);
  xgap = fract(x1 + 0.5f);
  int xpxl2 = (int)xend;
  int ypxl2 = (int)floorf(yend);
  float frac2 = fract(yend);
  
  if (steep) {
    blendPixelHUB75(ypxl2, xpxl2, r, g, b, (uint8_t)((1.0f - frac2) * xgap * 255.0f));
    blendPixelHUB75(ypxl2 + 1, xpxl2, r, g, b, (uint8_t)(frac2 * xgap * 255.0f));
  } else {
    blendPixelHUB75(xpxl2, ypxl2, r, g, b, (uint8_t)((1.0f - frac2) * xgap * 255.0f));
    blendPixelHUB75(xpxl2, ypxl2 + 1, r, g, b, (uint8_t)(frac2 * xgap * 255.0f));
  }
  
  // Main line body
  if (steep) {
    for (int x = xpxl1 + 1; x < xpxl2; x++) {
      int y = (int)floorf(intery);
      float f = fract(intery);
      blendPixelHUB75(y, x, r, g, b, (uint8_t)((1.0f - f) * 255.0f));
      blendPixelHUB75(y + 1, x, r, g, b, (uint8_t)(f * 255.0f));
      intery += gradient;
    }
  } else {
    for (int x = xpxl1 + 1; x < xpxl2; x++) {
      int y = (int)floorf(intery);
      float f = fract(intery);
      blendPixelHUB75(x, y, r, g, b, (uint8_t)((1.0f - f) * 255.0f));
      blendPixelHUB75(x, y + 1, r, g, b, (uint8_t)(f * 255.0f));
      intery += gradient;
    }
  }
}

// Non-AA Bresenham line (used for OLED or when AA disabled)
static void drawLineBasic(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  
  while (true) {
    setPixel(x0, y0, r, g, b);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// Main line function with integer coords - uses AA for HUB75 when enabled
static void drawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
  if (aa_enabled && gpu.target == 0) {
    drawLineAA((float)x0, (float)y0, (float)x1, (float)y1, r, g, b);
  } else {
    drawLineBasic(x0, y0, x1, y1, r, g, b);
  }
}

// Float coordinate line - enables sub-pixel smooth movement
static void drawLineF(float x0, float y0, float x1, float y1, uint8_t r, uint8_t g, uint8_t b) {
  if (aa_enabled && gpu.target == 0) {
    drawLineAA(x0, y0, x1, y1, r, g, b);
  } else {
    drawLineBasic((int)roundf(x0), (int)roundf(y0), (int)roundf(x1), (int)roundf(y1), r, g, b);
  }
}

static void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  drawLine(x, y, x + w - 1, y, r, g, b);
  drawLine(x + w - 1, y, x + w - 1, y + h - 1, r, g, b);
  drawLine(x + w - 1, y + h - 1, x, y + h - 1, r, g, b);
  drawLine(x, y + h - 1, x, y, r, g, b);
}

// Float coordinate rect
static void drawRectF(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b) {
  drawLineF(x, y, x + w, y, r, g, b);
  drawLineF(x + w, y, x + w, y + h, r, g, b);
  drawLineF(x + w, y + h, x, y + h, r, g, b);
  drawLineF(x, y + h, x, y, r, g, b);
}

static void fillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  for (int py = y; py < y + h; py++) {
    for (int px = x; px < x + w; px++) {
      setPixel(px, py, r, g, b);
    }
  }
}

// Fast AA circle using midpoint algorithm with distance-based alpha
static void drawCircleAA(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b) {
  // Scan only pixels near the circle edge
  int ir = (int)ceilf(radius);
  int icx = (int)roundf(cx);
  int icy = (int)roundf(cy);
  
  for (int py = icy - ir - 1; py <= icy + ir + 1; py++) {
    if (py < 0 || py >= TOTAL_HEIGHT) continue;
    for (int px = icx - ir - 1; px <= icx + ir + 1; px++) {
      if (px < 0 || px >= TOTAL_WIDTH) continue;
      
      float dx = px + 0.5f - cx;
      float dy = py + 0.5f - cy;
      float dist = sqrtf(dx * dx + dy * dy);
      float diff = fabsf(dist - radius);
      
      // Only draw pixels within 1 pixel of the circle edge
      if (diff < 1.0f) {
        uint8_t alpha = (uint8_t)((1.0f - diff) * 255.0f);
        blendPixelHUB75(px, py, r, g, b, alpha);
      }
    }
  }
}

// Basic Bresenham circle (for OLED or when AA disabled)
static void drawCircleBasic(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) {
  int x = radius;
  int y = 0;
  int err = 0;
  
  while (x >= y) {
    setPixel(cx + x, cy + y, r, g, b);
    setPixel(cx + y, cy + x, r, g, b);
    setPixel(cx - y, cy + x, r, g, b);
    setPixel(cx - x, cy + y, r, g, b);
    setPixel(cx - x, cy - y, r, g, b);
    setPixel(cx - y, cy - x, r, g, b);
    setPixel(cx + y, cy - x, r, g, b);
    setPixel(cx + x, cy - y, r, g, b);
    
    y++;
    err += 1 + 2 * y;
    if (2 * (err - x) + 1 > 0) {
      x--;
      err += 1 - 2 * x;
    }
  }
}

// Main circle function (integer coords)
static void drawCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) {
  if (aa_enabled && gpu.target == 0) {
    drawCircleAA((float)cx, (float)cy, (float)radius, r, g, b);
  } else {
    drawCircleBasic(cx, cy, radius, r, g, b);
  }
}

// Float coordinate circle - enables sub-pixel smooth movement
static void drawCircleF(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b) {
  if (aa_enabled && gpu.target == 0) {
    drawCircleAA(cx, cy, radius, r, g, b);
  } else {
    drawCircleBasic((int)roundf(cx), (int)roundf(cy), (int)roundf(radius), r, g, b);
  }
}

// Filled circle with AA edge
static void fillCircle(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b) {
  int minX = (int)floorf(cx - radius - 1);
  int maxX = (int)ceilf(cx + radius + 1);
  int minY = (int)floorf(cy - radius - 1);
  int maxY = (int)ceilf(cy + radius + 1);
  
  if (minX < 0) minX = 0;
  if (minY < 0) minY = 0;
  if (maxX >= TOTAL_WIDTH) maxX = TOTAL_WIDTH - 1;
  if (maxY >= TOTAL_HEIGHT) maxY = TOTAL_HEIGHT - 1;
  
  for (int py = minY; py <= maxY; py++) {
    for (int px = minX; px <= maxX; px++) {
      float dx = px + 0.5f - cx;
      float dy = py + 0.5f - cy;
      float dist = sqrtf(dx * dx + dy * dy);
      
      if (aa_enabled && gpu.target == 0) {
        // AA edge
        if (dist <= radius - 0.5f) {
          setPixelHUB75(px, py, r, g, b);
        } else if (dist < radius + 0.5f) {
          uint8_t alpha = (uint8_t)((radius + 0.5f - dist) * 255.0f);
          blendPixelHUB75(px, py, r, g, b, alpha);
        }
      } else {
        if (dist <= radius) {
          setPixel(px, py, r, g, b);
        }
      }
    }
  }
}

static void fillPolygon(int n, int16_t* vx, int16_t* vy, uint8_t r, uint8_t g, uint8_t b) {
  // Find bounding box
  int minY = vy[0], maxY = vy[0];
  for (int i = 1; i < n; i++) {
    if (vy[i] < minY) minY = vy[i];
    if (vy[i] > maxY) maxY = vy[i];
  }
  
  // Scanline fill
  for (int y = minY; y <= maxY; y++) {
    int nodes[32];
    int nodeCount = 0;
    
    int j = n - 1;
    for (int i = 0; i < n; i++) {
      if ((vy[i] < y && vy[j] >= y) || (vy[j] < y && vy[i] >= y)) {
        nodes[nodeCount++] = vx[i] + (y - vy[i]) * (vx[j] - vx[i]) / (vy[j] - vy[i]);
      }
      j = i;
    }
    
    // Sort nodes
    for (int i = 0; i < nodeCount - 1; i++) {
      for (int j = i + 1; j < nodeCount; j++) {
        if (nodes[i] > nodes[j]) {
          int tmp = nodes[i];
          nodes[i] = nodes[j];
          nodes[j] = tmp;
        }
      }
    }
    
    // Fill between pairs
    for (int i = 0; i < nodeCount; i += 2) {
      if (i + 1 < nodeCount) {
        for (int x = nodes[i]; x <= nodes[i + 1]; x++) {
          setPixel(x, y, r, g, b);
        }
      }
    }
  }
}

// Bilinear sample from RGB sprite (for supersampling)
static void sampleSpriteRGB(Sprite& s, float fx, float fy, uint8_t& r, uint8_t& g, uint8_t& b) {
  int x0 = (int)floorf(fx);
  int y0 = (int)floorf(fy);
  int x1 = x0 + 1;
  int y1 = y0 + 1;
  float dx = fx - x0;
  float dy = fy - y0;
  
  // Clamp to sprite bounds
  x0 = (x0 < 0) ? 0 : (x0 >= s.width) ? s.width - 1 : x0;
  y0 = (y0 < 0) ? 0 : (y0 >= s.height) ? s.height - 1 : y0;
  x1 = (x1 < 0) ? 0 : (x1 >= s.width) ? s.width - 1 : x1;
  y1 = (y1 < 0) ? 0 : (y1 >= s.height) ? s.height - 1 : y1;
  
  // Get four corner pixels
  int idx00 = (y0 * s.width + x0) * 3;
  int idx10 = (y0 * s.width + x1) * 3;
  int idx01 = (y1 * s.width + x0) * 3;
  int idx11 = (y1 * s.width + x1) * 3;
  
  // Bilinear interpolation
  float w00 = (1 - dx) * (1 - dy);
  float w10 = dx * (1 - dy);
  float w01 = (1 - dx) * dy;
  float w11 = dx * dy;
  
  r = (uint8_t)(s.data[idx00 + 0] * w00 + s.data[idx10 + 0] * w10 + 
                s.data[idx01 + 0] * w01 + s.data[idx11 + 0] * w11);
  g = (uint8_t)(s.data[idx00 + 1] * w00 + s.data[idx10 + 1] * w10 + 
                s.data[idx01 + 1] * w01 + s.data[idx11 + 1] * w11);
  b = (uint8_t)(s.data[idx00 + 2] * w00 + s.data[idx10 + 2] * w10 + 
                s.data[idx01 + 2] * w01 + s.data[idx11 + 2] * w11);
}

static void blitSprite(int id, int dx, int dy) {
  if (id < 0 || id >= MAX_SPRITES || !gpu.sprites[id].valid) return;
  
  Sprite& s = gpu.sprites[id];
  
  if (s.format == 0 && gpu.target == 0) {
    // RGB sprite to HUB75
    if (aa_enabled) {
      // Supersampled blit: 2x2 samples per output pixel
      for (int y = 0; y < s.height; y++) {
        for (int x = 0; x < s.width; x++) {
          uint16_t tr = 0, tg = 0, tb = 0;
          // Sample at 4 sub-pixel positions
          for (int sy = 0; sy < 2; sy++) {
            for (int sx = 0; sx < 2; sx++) {
              uint8_t sr, sg, sb;
              float fx = x + sx * 0.5f;
              float fy = y + sy * 0.5f;
              sampleSpriteRGB(s, fx, fy, sr, sg, sb);
              tr += sr; tg += sg; tb += sb;
            }
          }
          // Average the 4 samples
          setPixelHUB75(dx + x, dy + y, tr >> 2, tg >> 2, tb >> 2);
        }
      }
    } else {
      // Direct blit (no AA)
      for (int y = 0; y < s.height; y++) {
        for (int x = 0; x < s.width; x++) {
          int idx = (y * s.width + x) * 3;
          setPixelHUB75(dx + x, dy + y, s.data[idx], s.data[idx + 1], s.data[idx + 2]);
        }
      }
    }
  } else if (s.format == 1 && gpu.target == 1) {
    // Mono sprite to OLED
    for (int y = 0; y < s.height; y++) {
      for (int x = 0; x < s.width; x++) {
        int byte_idx = (y * ((s.width + 7) / 8)) + (x / 8);
        int bit = 7 - (x % 8);
        bool on = (s.data[byte_idx] >> bit) & 1;
        setPixelOLED(dx + x, dy + y, on);
      }
    }
  }
}

// ============================================================
// Simple PRNG
// ============================================================
static uint16_t gpuRand() {
  gpu.randSeed = gpu.randSeed * 1103515245 + 12345;
  return (gpu.randSeed >> 16) & 0xFFFF;
}

// ============================================================
// Shader Bytecode Interpreter
// ============================================================
static void executeShader(int slot) {
  if (slot < 0 || slot >= MAX_SHADERS || !gpu.shaders[slot].valid) return;
  
  Shader& shader = gpu.shaders[slot];
  uint8_t* code = shader.bytecode;
  uint16_t pc = 0;
  int maxInstructions = 100000;  // Prevent infinite loops
  
  // Reset registers and loop stack
  memset(gpu.regs, 0, sizeof(gpu.regs));
  gpu.loopSP = 0;
  
  while (pc < shader.length && maxInstructions-- > 0) {
    Op op = (Op)code[pc++];
    
    switch (op) {
      case Op::NOP:
        break;
        
      case Op::HALT:
        return;
        
      case Op::SET: {
        uint8_t rd = code[pc++];
        int16_t imm = (int16_t)(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        if (rd < MAX_REGISTERS) gpu.regs[rd] = imm;
        break;
      }
      
      case Op::MOV: {
        uint8_t rd = code[pc++];
        uint8_t rs = code[pc++];
        if (rd < MAX_REGISTERS && rs < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[rs];
        break;
      }
      
      case Op::LOAD: {
        uint8_t rd = code[pc++];
        uint8_t var = code[pc++];
        if (rd < MAX_REGISTERS && var < MAX_VARIABLES) gpu.regs[rd] = gpu.variables[var];
        break;
      }
      
      case Op::STORE: {
        uint8_t var = code[pc++];
        uint8_t rs = code[pc++];
        if (var < MAX_VARIABLES && rs < MAX_REGISTERS) gpu.variables[var] = gpu.regs[rs];
        break;
      }
      
      case Op::ADD: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[ra] + gpu.regs[rb];
        break;
      }
      
      case Op::SUB: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[ra] - gpu.regs[rb];
        break;
      }
      
      case Op::MUL: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = (gpu.regs[ra] * gpu.regs[rb]) >> 8;
        break;
      }
      
      case Op::DIV: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS && gpu.regs[rb] != 0) gpu.regs[rd] = gpu.regs[ra] / gpu.regs[rb];
        break;
      }
      
      case Op::MOD: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS && gpu.regs[rb] != 0) gpu.regs[rd] = gpu.regs[ra] % gpu.regs[rb];
        break;
      }
      
      case Op::NEG: {
        uint8_t rd = code[pc++], rs = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = -gpu.regs[rs];
        break;
      }
      
      case Op::ABS: {
        uint8_t rd = code[pc++], rs = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[rs] < 0 ? -gpu.regs[rs] : gpu.regs[rs];
        break;
      }
      
      case Op::MIN: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[ra] < gpu.regs[rb] ? gpu.regs[ra] : gpu.regs[rb];
        break;
      }
      
      case Op::MAX: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[ra] > gpu.regs[rb] ? gpu.regs[ra] : gpu.regs[rb];
        break;
      }
      
      case Op::AND: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[ra] & gpu.regs[rb];
        break;
      }
      
      case Op::OR: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[ra] | gpu.regs[rb];
        break;
      }
      
      case Op::XOR: {
        uint8_t rd = code[pc++], ra = code[pc++], rb = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[ra] ^ gpu.regs[rb];
        break;
      }
      
      case Op::NOT: {
        uint8_t rd = code[pc++], rs = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = ~gpu.regs[rs];
        break;
      }
      
      case Op::SHL: {
        uint8_t rd = code[pc++], rs = code[pc++], imm = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[rs] << imm;
        break;
      }
      
      case Op::SHR: {
        uint8_t rd = code[pc++], rs = code[pc++], imm = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.regs[rs] >> imm;
        break;
      }
      
      case Op::SIN: {
        uint8_t rd = code[pc++], rs = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = SIN_LUT[gpu.regs[rs] & 255];
        break;
      }
      
      case Op::COS: {
        uint8_t rd = code[pc++], rs = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = COS_LUT[gpu.regs[rs] & 255];
        break;
      }
      
      case Op::SQRT: {
        uint8_t rd = code[pc++], rs = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = SQRT_LUT[gpu.regs[rs] & 255];
        break;
      }
      
      case Op::SETPX: {
        uint8_t xr = code[pc++], yr = code[pc++];
        uint8_t rr = code[pc++], gr = code[pc++], br = code[pc++];
        setPixel(gpu.regs[xr], gpu.regs[yr], gpu.regs[rr], gpu.regs[gr], gpu.regs[br]);
        break;
      }
      
      case Op::GETPX: {
        uint8_t rd = code[pc++], xr = code[pc++], yr = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = getPixel(gpu.regs[xr], gpu.regs[yr]) & 0xFFFF;
        break;
      }
      
      case Op::FILL: {
        uint8_t xr = code[pc++], yr = code[pc++], wr = code[pc++], hr = code[pc++];
        uint8_t rr = code[pc++], gr = code[pc++], br = code[pc++];
        fillRect(gpu.regs[xr], gpu.regs[yr], gpu.regs[wr], gpu.regs[hr],
                 gpu.regs[rr], gpu.regs[gr], gpu.regs[br]);
        break;
      }
      
      case Op::LINE: {
        uint8_t x1r = code[pc++], y1r = code[pc++], x2r = code[pc++], y2r = code[pc++];
        uint8_t rr = code[pc++], gr = code[pc++], br = code[pc++];
        drawLine(gpu.regs[x1r], gpu.regs[y1r], gpu.regs[x2r], gpu.regs[y2r],
                 gpu.regs[rr], gpu.regs[gr], gpu.regs[br]);
        break;
      }
      
      case Op::RECT: {
        uint8_t xr = code[pc++], yr = code[pc++], wr = code[pc++], hr = code[pc++];
        uint8_t rr = code[pc++], gr = code[pc++], br = code[pc++];
        drawRect(gpu.regs[xr], gpu.regs[yr], gpu.regs[wr], gpu.regs[hr],
                 gpu.regs[rr], gpu.regs[gr], gpu.regs[br]);
        break;
      }
      
      case Op::CIRCLE: {
        uint8_t cxr = code[pc++], cyr = code[pc++], radr = code[pc++];
        uint8_t rr = code[pc++], gr = code[pc++], br = code[pc++];
        drawCircle(gpu.regs[cxr], gpu.regs[cyr], gpu.regs[radr],
                   gpu.regs[rr], gpu.regs[gr], gpu.regs[br]);
        break;
      }
      
      case Op::POLY: {
        uint8_t nr = code[pc++], var_start = code[pc++];
        uint8_t rr = code[pc++], gr = code[pc++], br = code[pc++];
        int n = gpu.regs[nr];
        if (n > 0 && n <= 16) {
          int16_t vx[16], vy[16];
          for (int i = 0; i < n; i++) {
            vx[i] = gpu.variables[var_start + i * 2];
            vy[i] = gpu.variables[var_start + i * 2 + 1];
          }
          fillPolygon(n, vx, vy, gpu.regs[rr], gpu.regs[gr], gpu.regs[br]);
        }
        break;
      }
      
      case Op::SPRITE: {
        uint8_t idr = code[pc++], xr = code[pc++], yr = code[pc++];
        blitSprite(gpu.regs[idr], gpu.regs[xr], gpu.regs[yr]);
        break;
      }
      
      case Op::CLEAR: {
        uint8_t rr = code[pc++], gr = code[pc++], br = code[pc++];
        if (gpu.target == 0) {
          uint8_t r = gpu.regs[rr], g = gpu.regs[gr], b = gpu.regs[br];
          for (int i = 0; i < TOTAL_WIDTH * TOTAL_HEIGHT; i++) {
            hub75_buffer[i * 3 + 0] = r;
            hub75_buffer[i * 3 + 1] = g;
            hub75_buffer[i * 3 + 2] = b;
          }
        } else {
          uint8_t val = (gpu.regs[rr] + gpu.regs[gr] + gpu.regs[br]) > 384 ? 0xFF : 0x00;
          memset(oled_buffer, val, OLED_BUFFER_SIZE);
        }
        break;
      }
      
      case Op::LOOP: {
        uint8_t count_reg = code[pc++];
        if (gpu.loopSP < MAX_STACK) {
          gpu.loopStack[gpu.loopSP].pc = pc;
          gpu.loopStack[gpu.loopSP].counter = gpu.regs[count_reg];
          gpu.loopSP++;
        }
        break;
      }
      
      case Op::ENDL: {
        if (gpu.loopSP > 0) {
          gpu.loopStack[gpu.loopSP - 1].counter--;
          if (gpu.loopStack[gpu.loopSP - 1].counter > 0) {
            pc = gpu.loopStack[gpu.loopSP - 1].pc;
          } else {
            gpu.loopSP--;
          }
        }
        break;
      }
      
      case Op::JMP: {
        int16_t offset = (int16_t)(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        pc += offset;
        break;
      }
      
      case Op::JZ: {
        uint8_t rs = code[pc++];
        int16_t offset = (int16_t)(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        if (gpu.regs[rs] == 0) pc += offset;
        break;
      }
      
      case Op::JNZ: {
        uint8_t rs = code[pc++];
        int16_t offset = (int16_t)(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        if (gpu.regs[rs] != 0) pc += offset;
        break;
      }
      
      case Op::JGT: {
        uint8_t rs = code[pc++];
        int16_t offset = (int16_t)(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        if (gpu.regs[rs] > 0) pc += offset;
        break;
      }
      
      case Op::JLT: {
        uint8_t rs = code[pc++];
        int16_t offset = (int16_t)(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        if (gpu.regs[rs] < 0) pc += offset;
        break;
      }
      
      case Op::GETX: {
        uint8_t rd = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.px;
        break;
      }
      
      case Op::GETY: {
        uint8_t rd = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpu.py;
        break;
      }
      
      case Op::GETW: {
        uint8_t rd = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = (gpu.target == 0) ? TOTAL_WIDTH : OLED_WIDTH;
        break;
      }
      
      case Op::GETH: {
        uint8_t rd = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = (gpu.target == 0) ? TOTAL_HEIGHT : OLED_HEIGHT;
        break;
      }
      
      case Op::TIME: {
        uint8_t rd = code[pc++];
        if (rd < MAX_REGISTERS) {
          uint32_t ms = (esp_timer_get_time() - gpu.startTime) / 1000;
          gpu.regs[rd] = ms & 0xFFFF;
        }
        break;
      }
      
      case Op::RAND: {
        uint8_t rd = code[pc++];
        if (rd < MAX_REGISTERS) gpu.regs[rd] = gpuRand();
        break;
      }
      
      default:
        // Unknown opcode - skip
        break;
    }
  }
}

// ============================================================
// Command Protocol from CPU
// ============================================================
#pragma pack(push, 1)

enum class CmdType : uint8_t {
  NOP = 0x00,
  UPLOAD_SHADER = 0x10,
  DELETE_SHADER = 0x11,
  EXEC_SHADER = 0x12,
  
  UPLOAD_SPRITE = 0x20,
  DELETE_SPRITE = 0x21,
  
  SET_VAR = 0x30,
  SET_VARS = 0x31,       // Set multiple variables
  
  DRAW_PIXEL = 0x40,
  DRAW_LINE = 0x41,
  DRAW_RECT = 0x42,
  DRAW_FILL = 0x43,
  DRAW_CIRCLE = 0x44,
  DRAW_POLY = 0x45,
  BLIT_SPRITE = 0x46,
  CLEAR = 0x47,
  
  // Float coordinate versions (sub-pixel precision for smooth animation)
  DRAW_LINE_F = 0x48,    // Float coords: x0,y0,x1,y1 as 16.16 fixed point
  DRAW_CIRCLE_F = 0x49,  // Float coords: cx,cy,r as 16.16 fixed point
  DRAW_RECT_F = 0x4A,    // Float coords: x,y,w,h as 16.16 fixed point
  
  SET_TARGET = 0x50,     // 0=HUB75, 1=OLED
  PRESENT = 0x51,        // Push framebuffer to display
  
  // OLED-specific commands (always target OLED buffer)
  OLED_CLEAR = 0x60,
  OLED_LINE = 0x61,
  OLED_RECT = 0x62,
  OLED_FILL = 0x63,
  OLED_CIRCLE = 0x64,
  OLED_PRESENT = 0x65,
  
  PING = 0xF0,
  RESET = 0xFF,
};

struct CmdHeader {
  uint8_t sync[2];       // 0xAA, 0x55
  CmdType type;
  uint16_t length;       // Payload length
};

#pragma pack(pop)

constexpr uint8_t SYNC0 = 0xAA;
constexpr uint8_t SYNC1 = 0x55;

// ============================================================
// Command Processing
// ============================================================
static uint8_t cmd_buffer[2048];

static void processCommand(const CmdHeader* hdr, const uint8_t* payload) {
  switch (hdr->type) {
    case CmdType::UPLOAD_SHADER: {
      if (hdr->length < 3) break;
      uint8_t slot = payload[0];
      uint16_t len = payload[1] | (payload[2] << 8);
      if (slot < MAX_SHADERS && len <= MAX_SHADER_SIZE && hdr->length >= 3 + len) {
        memcpy(gpu.shaders[slot].bytecode, payload + 3, len);
        gpu.shaders[slot].length = len;
        gpu.shaders[slot].valid = true;
        ESP_LOGI(TAG, "Shader %d uploaded: %d bytes", slot, len);
      }
      break;
    }
    
    case CmdType::DELETE_SHADER: {
      if (hdr->length >= 1) {
        uint8_t slot = payload[0];
        if (slot < MAX_SHADERS) {
          gpu.shaders[slot].valid = false;
          ESP_LOGI(TAG, "Shader %d deleted", slot);
        }
      }
      break;
    }
    
    case CmdType::EXEC_SHADER: {
      if (hdr->length >= 1) {
        uint8_t slot = payload[0];
        executeShader(slot);
      }
      break;
    }
    
    case CmdType::UPLOAD_SPRITE: {
      if (hdr->length < 4) break;
      uint8_t id = payload[0];
      uint8_t w = payload[1];
      uint8_t h = payload[2];
      uint8_t fmt = payload[3];
      int dataSize = (fmt == 0) ? (w * h * 3) : ((w + 7) / 8 * h);
      
      if (id < MAX_SPRITES && hdr->length >= 4 + dataSize && dataSize <= MAX_SPRITE_SIZE) {
        if (gpu.sprites[id].data == nullptr) {
          gpu.sprites[id].data = (uint8_t*)heap_caps_malloc(MAX_SPRITE_SIZE, MALLOC_CAP_DEFAULT);
        }
        if (gpu.sprites[id].data) {
          memcpy(gpu.sprites[id].data, payload + 4, dataSize);
          gpu.sprites[id].width = w;
          gpu.sprites[id].height = h;
          gpu.sprites[id].format = fmt;
          gpu.sprites[id].valid = true;
          ESP_LOGI(TAG, "Sprite %d uploaded: %dx%d fmt=%d", id, w, h, fmt);
        }
      }
      break;
    }
    
    case CmdType::DELETE_SPRITE: {
      if (hdr->length >= 1) {
        uint8_t id = payload[0];
        if (id < MAX_SPRITES) {
          gpu.sprites[id].valid = false;
          ESP_LOGI(TAG, "Sprite %d deleted", id);
        }
      }
      break;
    }
    
    case CmdType::SET_VAR: {
      if (hdr->length >= 3) {
        uint8_t var = payload[0];
        int16_t val = (int16_t)(payload[1] | (payload[2] << 8));
        if (var < MAX_VARIABLES) {
          gpu.variables[var] = val;
        }
      }
      break;
    }
    
    case CmdType::SET_VARS: {
      // Format: start_var, count, values[]
      if (hdr->length >= 2) {
        uint8_t start = payload[0];
        uint8_t count = payload[1];
        for (int i = 0; i < count && start + i < MAX_VARIABLES && 2 + i * 2 + 1 < hdr->length; i++) {
          gpu.variables[start + i] = (int16_t)(payload[2 + i * 2] | (payload[2 + i * 2 + 1] << 8));
        }
      }
      break;
    }
    
    case CmdType::DRAW_PIXEL: {
      if (hdr->length >= 5) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        setPixel(x, y, payload[4], payload[5], payload[6]);
      }
      break;
    }
    
    case CmdType::DRAW_LINE: {
      if (hdr->length >= 11) {
        int16_t x1 = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y1 = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t x2 = (int16_t)(payload[4] | (payload[5] << 8));
        int16_t y2 = (int16_t)(payload[6] | (payload[7] << 8));
        drawLine(x1, y1, x2, y2, payload[8], payload[9], payload[10]);
      }
      break;
    }
    
    case CmdType::DRAW_RECT: {
      if (hdr->length >= 11) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t w = (int16_t)(payload[4] | (payload[5] << 8));
        int16_t h = (int16_t)(payload[6] | (payload[7] << 8));
        drawRect(x, y, w, h, payload[8], payload[9], payload[10]);
      }
      break;
    }
    
    case CmdType::DRAW_FILL: {
      if (hdr->length >= 11) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t w = (int16_t)(payload[4] | (payload[5] << 8));
        int16_t h = (int16_t)(payload[6] | (payload[7] << 8));
        fillRect(x, y, w, h, payload[8], payload[9], payload[10]);
      }
      break;
    }
    
    case CmdType::DRAW_CIRCLE: {
      if (hdr->length >= 9) {
        int16_t cx = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t cy = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t r = (int16_t)(payload[4] | (payload[5] << 8));
        drawCircle(cx, cy, r, payload[6], payload[7], payload[8]);
      }
      break;
    }
    
    // Float coordinate commands - uses 8.8 fixed point for sub-pixel precision
    case CmdType::DRAW_LINE_F: {
      if (hdr->length >= 11) {
        // 8.8 fixed point: high byte = integer, low byte = fraction (0-255 = 0.0-0.996)
        float x1 = (int8_t)payload[1] + (payload[0] / 256.0f);
        float y1 = (int8_t)payload[3] + (payload[2] / 256.0f);
        float x2 = (int8_t)payload[5] + (payload[4] / 256.0f);
        float y2 = (int8_t)payload[7] + (payload[6] / 256.0f);
        drawLineF(x1, y1, x2, y2, payload[8], payload[9], payload[10]);
      }
      break;
    }
    
    case CmdType::DRAW_CIRCLE_F: {
      if (hdr->length >= 9) {
        float cx = (int8_t)payload[1] + (payload[0] / 256.0f);
        float cy = (int8_t)payload[3] + (payload[2] / 256.0f);
        float r = (int8_t)payload[5] + (payload[4] / 256.0f);
        drawCircleF(cx, cy, r, payload[6], payload[7], payload[8]);
      }
      break;
    }
    
    case CmdType::DRAW_RECT_F: {
      if (hdr->length >= 11) {
        float x = (int8_t)payload[1] + (payload[0] / 256.0f);
        float y = (int8_t)payload[3] + (payload[2] / 256.0f);
        float w = (int8_t)payload[5] + (payload[4] / 256.0f);
        float h = (int8_t)payload[7] + (payload[6] / 256.0f);
        drawRectF(x, y, w, h, payload[8], payload[9], payload[10]);
      }
      break;
    }
    
    case CmdType::DRAW_POLY: {
      if (hdr->length >= 4) {
        uint8_t n = payload[0];
        uint8_t r = payload[1], g = payload[2], b = payload[3];
        if (n <= 16 && hdr->length >= 4 + n * 4) {
          int16_t vx[16], vy[16];
          for (int i = 0; i < n; i++) {
            vx[i] = (int16_t)(payload[4 + i * 4] | (payload[5 + i * 4] << 8));
            vy[i] = (int16_t)(payload[6 + i * 4] | (payload[7 + i * 4] << 8));
          }
          fillPolygon(n, vx, vy, r, g, b);
        }
      }
      break;
    }
    
    case CmdType::BLIT_SPRITE: {
      if (hdr->length >= 5) {
        uint8_t id = payload[0];
        int16_t x = (int16_t)(payload[1] | (payload[2] << 8));
        int16_t y = (int16_t)(payload[3] | (payload[4] << 8));
        blitSprite(id, x, y);
      }
      break;
    }
    
    case CmdType::CLEAR: {
      if (hdr->length >= 3) {
        if (gpu.target == 0) {
          for (int i = 0; i < TOTAL_WIDTH * TOTAL_HEIGHT; i++) {
            hub75_buffer[i * 3 + 0] = payload[0];
            hub75_buffer[i * 3 + 1] = payload[1];
            hub75_buffer[i * 3 + 2] = payload[2];
          }
        } else {
          uint8_t val = (payload[0] + payload[1] + payload[2]) > 384 ? 0xFF : 0x00;
          memset(oled_buffer, val, OLED_BUFFER_SIZE);
        }
      }
      break;
    }
    
    case CmdType::SET_TARGET: {
      if (hdr->length >= 1) {
        gpu.target = payload[0] & 1;
      }
      break;
    }
    
    case CmdType::PRESENT: {
      if (gpu.target == 0 && hub75_ok) {
        // Copy internal buffer to HUB75 display
        for (int y = 0; y < TOTAL_HEIGHT; y++) {
          for (int x = 0; x < TOTAL_WIDTH; x++) {
            int idx = (y * TOTAL_WIDTH + x) * 3;
            hub75->setPixel(x, y, RGB(hub75_buffer[idx], hub75_buffer[idx+1], hub75_buffer[idx+2]));
          }
        }
        hub75->show();
        dbg_hub75_presents.fetch_add(1, std::memory_order_relaxed);
        // Use release semantics so Core 0 sees this update
        dbg_last_hub75_present.store(esp_timer_get_time(), std::memory_order_release);
      } else if (gpu.target == 1 && oled_ok) {
        // Copy to update buffer and signal Core 0 task (non-blocking)
        memcpy(oled_update_buffer, oled_buffer, OLED_BUFFER_SIZE);
        oled_update_pending = true;
      }
      gpu.frameCount++;
      break;
    }
    
    // ========== OLED-Specific Commands (always target OLED) ==========
    case CmdType::OLED_CLEAR: {
      memset(oled_buffer, 0, OLED_BUFFER_SIZE);
      break;
    }
    
    case CmdType::OLED_LINE: {
      if (hdr->length >= 9) {
        int16_t x1 = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y1 = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t x2 = (int16_t)(payload[4] | (payload[5] << 8));
        int16_t y2 = (int16_t)(payload[6] | (payload[7] << 8));
        bool on = payload[8] > 0;
        // Draw directly to OLED buffer using Bresenham
        int dx = abs(x2 - x1);
        int dy = -abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx + dy;
        while (true) {
          if (x1 >= 0 && x1 < OLED_WIDTH && y1 >= 0 && y1 < OLED_HEIGHT) {
            int byte_idx = (y1 / 8) * OLED_WIDTH + x1;
            int bit = y1 % 8;
            if (on) oled_buffer[byte_idx] |= (1 << bit);
            else oled_buffer[byte_idx] &= ~(1 << bit);
          }
          if (x1 == x2 && y1 == y2) break;
          int e2 = 2 * err;
          if (e2 >= dy) { err += dy; x1 += sx; }
          if (e2 <= dx) { err += dx; y1 += sy; }
        }
      }
      break;
    }
    
    case CmdType::OLED_RECT: {
      if (hdr->length >= 9) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t w = (int16_t)(payload[4] | (payload[5] << 8));
        int16_t h = (int16_t)(payload[6] | (payload[7] << 8));
        bool on = payload[8] > 0;
        // Draw rectangle outline to OLED
        for (int px = x; px < x + w && px < OLED_WIDTH; px++) {
          if (px >= 0) {
            if (y >= 0 && y < OLED_HEIGHT) {
              int idx = (y / 8) * OLED_WIDTH + px;
              if (on) oled_buffer[idx] |= (1 << (y % 8));
              else oled_buffer[idx] &= ~(1 << (y % 8));
            }
            int y2 = y + h - 1;
            if (y2 >= 0 && y2 < OLED_HEIGHT) {
              int idx = (y2 / 8) * OLED_WIDTH + px;
              if (on) oled_buffer[idx] |= (1 << (y2 % 8));
              else oled_buffer[idx] &= ~(1 << (y2 % 8));
            }
          }
        }
        for (int py = y; py < y + h && py < OLED_HEIGHT; py++) {
          if (py >= 0) {
            if (x >= 0 && x < OLED_WIDTH) {
              int idx = (py / 8) * OLED_WIDTH + x;
              if (on) oled_buffer[idx] |= (1 << (py % 8));
              else oled_buffer[idx] &= ~(1 << (py % 8));
            }
            int x2 = x + w - 1;
            if (x2 >= 0 && x2 < OLED_WIDTH) {
              int idx = (py / 8) * OLED_WIDTH + x2;
              if (on) oled_buffer[idx] |= (1 << (py % 8));
              else oled_buffer[idx] &= ~(1 << (py % 8));
            }
          }
        }
      }
      break;
    }
    
    case CmdType::OLED_FILL: {
      if (hdr->length >= 9) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t w = (int16_t)(payload[4] | (payload[5] << 8));
        int16_t h = (int16_t)(payload[6] | (payload[7] << 8));
        bool on = payload[8] > 0;
        for (int py = y; py < y + h && py < OLED_HEIGHT; py++) {
          if (py < 0) continue;
          for (int px = x; px < x + w && px < OLED_WIDTH; px++) {
            if (px < 0) continue;
            int idx = (py / 8) * OLED_WIDTH + px;
            if (on) oled_buffer[idx] |= (1 << (py % 8));
            else oled_buffer[idx] &= ~(1 << (py % 8));
          }
        }
      }
      break;
    }
    
    case CmdType::OLED_CIRCLE: {
      if (hdr->length >= 7) {
        int16_t cx = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t cy = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t r = (int16_t)(payload[4] | (payload[5] << 8));
        bool on = payload[6] > 0;
        // Bresenham circle
        int x = r, y = 0, err = 0;
        while (x >= y) {
          int pts[8][2] = {{cx+x,cy+y},{cx-x,cy+y},{cx+x,cy-y},{cx-x,cy-y},
                           {cx+y,cy+x},{cx-y,cy+x},{cx+y,cy-x},{cx-y,cy-x}};
          for (int i = 0; i < 8; i++) {
            int px = pts[i][0], py = pts[i][1];
            if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
              int idx = (py / 8) * OLED_WIDTH + px;
              if (on) oled_buffer[idx] |= (1 << (py % 8));
              else oled_buffer[idx] &= ~(1 << (py % 8));
            }
          }
          y++; err += 1 + 2*y;
          if (2*(err-x) + 1 > 0) { x--; err += 1 - 2*x; }
        }
      }
      break;
    }
    
    case CmdType::OLED_PRESENT: {
      if (oled_ok) {
        memcpy(oled_update_buffer, oled_buffer, OLED_BUFFER_SIZE);
        oled_update_pending = true;
        dbg_oled_presents++;
        dbg_last_oled_present = esp_timer_get_time();
        // Clear buffer after present to prevent stale data
        memset(oled_buffer, 0, OLED_BUFFER_SIZE);
      }
      break;
    }
    
    case CmdType::PING: {
      ESP_LOGI(TAG, "PING received");
      // Could send response back
      break;
    }
    
    case CmdType::RESET: {
      ESP_LOGI(TAG, "RESET received");
      // Clear all state
      for (int i = 0; i < MAX_SHADERS; i++) gpu.shaders[i].valid = false;
      for (int i = 0; i < MAX_SPRITES; i++) gpu.sprites[i].valid = false;
      memset(gpu.variables, 0, sizeof(gpu.variables));
      memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
      memset(oled_buffer, 0, OLED_BUFFER_SIZE);
      gpu.target = 0;
      gpu.frameCount = 0;
      break;
    }
    
    default:
      break;
  }
}

// ============================================================
// UART Receive Task
// ============================================================
static void uartTask(void* arg) {
  uint8_t rx_buffer[256];  // Bulk read buffer
  int state = 0;  // 0=sync0, 1=sync1, 2=type, 3=len_lo, 4=len_hi, 5=payload
  CmdHeader hdr;
  int payload_pos = 0;
  int64_t lastByteTime = esp_timer_get_time();
  
  ESP_LOGI(TAG, "UART RX task started");
  
  while (true) {
    // Read as many bytes as available (up to buffer size)
    int len = uart_read_bytes(UART_PORT, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(1));
    
    if (len <= 0) {
      // Timeout - if we were mid-packet for too long, reset
      if (state > 0) {
        int64_t now = esp_timer_get_time();
        if (now - lastByteTime > 50000) {  // 50ms timeout
          state = 0;
        }
      }
      continue;
    }
    
    lastByteTime = esp_timer_get_time();
    
    // Process all received bytes
    for (int i = 0; i < len; i++) {
      uint8_t byte = rx_buffer[i];
      
      switch (state) {
        case 0:  // Waiting for SYNC0
          if (byte == SYNC0) state = 1;
          break;
          
        case 1:  // Waiting for SYNC1
          if (byte == SYNC1) {
            state = 2;
          } else if (byte == SYNC0) {
            state = 1;  // Stay looking for SYNC1
          } else {
            state = 0;  // Reset
          }
          break;
          
        case 2:  // Read command type
          hdr.type = (CmdType)byte;
          // Validate command type - reject obviously invalid ones
          if ((uint8_t)hdr.type > 0xFF || 
              ((uint8_t)hdr.type > 0x6F && (uint8_t)hdr.type < 0xF0)) {
            // Invalid command type, likely desync (0x00-0x6F and 0xF0-0xFF are valid)
            state = (byte == SYNC0) ? 1 : 0;
          } else {
            state = 3;
          }
          break;
          
        case 3:  // Read length low byte
          hdr.length = byte;
          state = 4;
          break;
          
        case 4:  // Read length high byte
          hdr.length |= (byte << 8);
          if (hdr.length == 0) {
            processCommand(&hdr, nullptr);
            state = 0;
          } else if (hdr.length > 512) {  // More conservative limit
            // Likely corrupt - flush UART and resync
            uint8_t flush[64];
            while (uart_read_bytes(UART_PORT, flush, sizeof(flush), 0) > 0) {}
            state = 0;
          } else {
            state = 5;
            payload_pos = 0;
          }
          break;
          
        case 5:  // Read payload
          cmd_buffer[payload_pos++] = byte;
          if (payload_pos >= hdr.length) {
            processCommand(&hdr, cmd_buffer);
            state = 0;
          }
          break;
      }
    }
  }
}

// ============================================================
// OLED Update Task (runs on Core 0 to avoid HUB75 DMA conflicts)
// ============================================================
static void oledTask(void* arg) {
  ESP_LOGI(TAG, "OLED task started on Core 0");
  int64_t lastUpdateTime = 0;
  const int MIN_MS_AFTER_HUB75 = 8;  // Wait at least 8ms after HUB75 present
  static uint32_t oled_update_num = 0;  // For sparse logging
  
  while (true) {
    if (oled_update_pending && oled_ok) {
      oled_update_pending = false;
      
      // Copy buffer first (fast operation)
      memcpy(oled->getBuffer(), oled_update_buffer, OLED_BUFFER_SIZE);
      
      // Wait until at least MIN_MS_AFTER_HUB75 since last HUB75 present
      // This gives DMA time to settle before we start I2C traffic
      int retries = 0;
      int64_t sinceHUB75;
      while (retries < 50) {  // Max ~100ms of waiting
        int64_t now = esp_timer_get_time();
        // Use acquire semantics to ensure we see the latest value from Core 1
        int64_t lastHUB75 = dbg_last_hub75_present.load(std::memory_order_acquire);
        sinceHUB75 = (now - lastHUB75) / 1000;  // ms
        
        // Check if we're past the minimum time
        if (sinceHUB75 >= MIN_MS_AFTER_HUB75) {
          break;  // Safe to proceed
        }
        
        // Short yield then check again
        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms tick
        retries++;
      }
      
      // Do the I2C update
      oled->updateDisplay();
      dbg_oled_updates++;
      oled_update_num++;
      lastUpdateTime = esp_timer_get_time();
      
      // Log only every 10th update to reduce overhead
      if ((oled_update_num % 10) == 0) {
        int64_t now = esp_timer_get_time();
        int64_t lastHUB75 = dbg_last_hub75_present.load(std::memory_order_acquire);
        sinceHUB75 = (now - lastHUB75) / 1000;
        ESP_LOGI(TAG, "OLED #%lu: since_hub75=%lldms, retries=%d", 
                 (unsigned long)oled_update_num, sinceHUB75, retries);
      }
      
      // Give HUB75 DMA time to recover after I2C burst
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // Check every 50ms (~20Hz max)
  }
}

// ============================================================
// Hardware Initialization
// ============================================================
static bool initHUB75() {
  ESP_LOGI(TAG, "--- HUB75 Init ---");
  
  HUB75Config config = HUB75Config::getDefault();
  config.colour_depth = 5;
  config.colour_buffer_count = 5;
  config.enable_double_buffering = true;
  config.enable_gamma_correction = true;
  config.gamma_value = 2.2f;
  
  hub75 = new SimpleHUB75Display();
  if (hub75->begin(true, config)) {
    hub75->setBrightness(200);
    hub75->clear();
    hub75->show();
    ESP_LOGI(TAG, "HUB75 OK: %dx%d", TOTAL_WIDTH, TOTAL_HEIGHT);
    return true;
  }
  ESP_LOGE(TAG, "HUB75 FAILED");
  return false;
}

static bool initI2C() {
  ESP_LOGI(TAG, "Initializing I2C for OLED...");
  HalResult result = ESP32S3_I2C_HAL::Initialize(0, 2, 1, 400000, 1000);
  if (result != HalResult::Success) {
    ESP_LOGE(TAG, "I2C init failed!");
    return false;
  }
  ESP_LOGI(TAG, "I2C OK (SDA=2, SCL=1, 400kHz)");
  return true;
}

static bool initOLED() {
  ESP_LOGI(TAG, "--- OLED Init ---");
  
  if (!initI2C()) {
    return false;
  }
  
  OLEDConfig oled_cfg;
  oled_cfg.contrast = 0xFF;
  
  oled = new DRIVER_OLED_SH1107(0x3C, 0);
  if (!oled->initialize(oled_cfg)) {
    ESP_LOGE(TAG, "OLED init failed");
    return false;
  }
  
  oled->clearBuffer();
  oled->updateDisplay();
  
  ESP_LOGI(TAG, "OLED OK: %dx%d", OLED_WIDTH, OLED_HEIGHT);
  return true;
}

static bool initUART() {
  ESP_LOGI(TAG, "--- UART Init ---");
  
  uart_config_t uart_cfg = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_DEFAULT,
  };
  
  uart_param_config(UART_PORT, &uart_cfg);
  uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, -1, -1);
  uart_driver_install(UART_PORT, 8192, 1024, 0, nullptr, 0);  // Larger RX buffer
  
  ESP_LOGI(TAG, "UART OK: %d baud, RX=%d, TX=%d, RX_BUF=8KB", UART_BAUD, UART_RX_PIN, UART_TX_PIN);
  return true;
}

// ============================================================
// Main Entry Point
// ============================================================
extern "C" void app_main() {
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, " GPU Programmable - No Hardcoded Effects");
  ESP_LOGI(TAG, "========================================");
  
  // Initialize LUTs
  initLUTs();
  
  // Initialize GPU state
  memset(&gpu, 0, sizeof(gpu));
  gpu.startTime = esp_timer_get_time();
  gpu.randSeed = esp_timer_get_time();
  
  // Allocate framebuffers
  hub75_buffer = (uint8_t*)heap_caps_malloc(HUB75_BUFFER_SIZE, MALLOC_CAP_DMA);
  oled_buffer = (uint8_t*)heap_caps_malloc(OLED_BUFFER_SIZE, MALLOC_CAP_DEFAULT);
  oled_update_buffer = (uint8_t*)heap_caps_malloc(OLED_BUFFER_SIZE, MALLOC_CAP_DEFAULT);
  
  if (!hub75_buffer || !oled_buffer || !oled_update_buffer) {
    ESP_LOGE(TAG, "Failed to allocate framebuffers!");
    return;
  }
  
  memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
  memset(oled_buffer, 0, OLED_BUFFER_SIZE);
  memset(oled_update_buffer, 0, OLED_BUFFER_SIZE);
  
  ESP_LOGI(TAG, "Framebuffers: HUB75=%d bytes, OLED=%d bytes", 
           HUB75_BUFFER_SIZE, OLED_BUFFER_SIZE);
  
  // Initialize hardware
  hub75_ok = initHUB75();
  oled_ok = initOLED();
  initUART();
  
  // Start UART receive task on Core 1 (needs larger stack for command processing)
  xTaskCreatePinnedToCore(uartTask, "uart_rx", 8192, nullptr, 5, nullptr, 1);
  
  // Start OLED update task on Core 0 (needs larger stack for I2C + logging)
  xTaskCreatePinnedToCore(oledTask, "oled_update", 4096, nullptr, 3, nullptr, 0);
  
  // Print ready message
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== GPU READY ===");
  ESP_LOGI(TAG, "  Shaders: %d slots x %d bytes", MAX_SHADERS, MAX_SHADER_SIZE);
  ESP_LOGI(TAG, "  Sprites: %d slots x %d bytes", MAX_SPRITES, MAX_SPRITE_SIZE);
  ESP_LOGI(TAG, "  Variables: %d x 16-bit", MAX_VARIABLES);
  ESP_LOGI(TAG, "  HUB75: %s (%dx%d)", hub75_ok ? "OK" : "FAIL", TOTAL_WIDTH, TOTAL_HEIGHT);
  ESP_LOGI(TAG, "  OLED: %s (%dx%d)", oled_ok ? "OK" : "FAIL", OLED_WIDTH, OLED_HEIGHT);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Waiting for CPU commands...");
  ESP_LOGI(TAG, "");
  
  // Main loop - just status updates
  uint32_t lastStatus = 0;
  uint32_t lastFrameCount = 0;
  uint32_t lastOledUpdates = 0;
  uint32_t lastHub75Presents = 0;
  uint32_t lastOledPresents = 0;
  
  while (true) {
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (now - lastStatus >= 2000) {
      uint32_t frames = gpu.frameCount - lastFrameCount;
      float fps = frames * 1000.0f / (now - lastStatus);
      
      uint32_t hub75_count = dbg_hub75_presents.load(std::memory_order_relaxed);
      uint32_t oled_present_count = dbg_oled_presents.load(std::memory_order_relaxed);
      uint32_t oled_update_count = dbg_oled_updates.load(std::memory_order_relaxed);
      
      uint32_t hub75_rate = hub75_count - lastHub75Presents;
      uint32_t oled_present_rate = oled_present_count - lastOledPresents;
      uint32_t oled_update_rate = oled_update_count - lastOledUpdates;
      
      // Get heap info to detect memory leaks
      uint32_t freeHeap = esp_get_free_heap_size();
      uint32_t minFreeHeap = esp_get_minimum_free_heap_size();
      
      ESP_LOGI(TAG, "=== STATUS ===");
      ESP_LOGI(TAG, "  FPS: %.1f | HUB75: %lu/2s | OLED_cmd: %lu/2s | OLED_i2c: %lu/2s",
               fps, (unsigned long)hub75_rate, (unsigned long)oled_present_rate, (unsigned long)oled_update_rate);
      ESP_LOGI(TAG, "  Heap: %lu free, %lu min | Total: HUB75=%lu, OLED=%lu",
               (unsigned long)freeHeap, (unsigned long)minFreeHeap,
               (unsigned long)hub75_count, (unsigned long)oled_present_count);
      
      lastStatus = now;
      lastFrameCount = gpu.frameCount;
      lastHub75Presents = hub75_count;
      lastOledPresents = oled_present_count;
      lastOledUpdates = oled_update_count;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
