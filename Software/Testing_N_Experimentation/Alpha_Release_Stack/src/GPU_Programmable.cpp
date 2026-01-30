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

// Frame rate limiter - prevent overwhelming the display hardware
static int64_t lastPresentTime = 0;
static const int64_t MIN_PRESENT_INTERVAL_US = 8000;  // 8ms = 120 FPS max
static uint32_t droppedFrames = 0;

// ============================================================
// GPU Alert/Feedback System
// ============================================================
// Alert severity levels
enum class AlertLevel : uint8_t {
  INFO = 0,      // Informational
  WARNING = 1,   // Recoverable issue
  ERROR = 2,     // Serious problem
  CRITICAL = 3,  // System failing
};

// Alert types
enum class AlertType : uint8_t {
  NONE = 0x00,
  BUFFER_WARNING = 0x01,    // RX buffer filling up (>50%)
  BUFFER_CRITICAL = 0x02,   // RX buffer almost full (>75%)
  BUFFER_OVERFLOW = 0x03,   // RX buffer overflowed, data lost
  FRAME_DROP = 0x10,        // Frames being dropped
  FRAME_DROP_SEVERE = 0x11, // Many frames dropped (>10/sec)
  HEAP_LOW = 0x20,          // Heap memory low (<50KB)
  HEAP_CRITICAL = 0x21,     // Heap memory critical (<20KB)
  HUB75_ERROR = 0x30,       // HUB75 display error
  OLED_ERROR = 0x31,        // OLED display error
  UART_ERROR = 0x40,        // UART communication error
  PARSER_ERROR = 0x41,      // Command parser error
  RECOVERED = 0xF0,         // Previously reported issue resolved
};

// Alert tracking state
static uint32_t alertsSent = 0;
static int64_t lastAlertTime = 0;
static constexpr int64_t MIN_ALERT_INTERVAL_US = 100000;  // 100ms between alerts
static AlertType lastAlertType = AlertType::NONE;
static uint32_t bufferWarningCount = 0;
static uint32_t bufferOverflowTotal = 0;
static uint32_t parserErrorCount = 0;
static uint32_t frameDropsThisSecond = 0;
static int64_t lastFrameDropReset = 0;
static bool bufferWarningActive = false;
static bool heapWarningActive = false;

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
constexpr int UART_BAUD = 10000000;  // 10 Mbps

// ============================================================
// GPU Memory Limits
// ============================================================
constexpr int MAX_SHADERS = 8;
constexpr int MAX_SHADER_SIZE = 1024;      // 1KB bytecode per shader
constexpr int MAX_SPRITES = 16;
constexpr int MAX_SPRITE_SIZE = 16384;     // 16KB per sprite (supports up to 128x32 RGB or 73x73 RGB)
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

// OLED orientation mode (0-7) - controlled by CPU
// 0 = No transform, 1 = Rotate 180, 2 = Mirror X, 3 = Mirror Y
// 4 = Mirror X+Y, 5 = Rotate 90 CW, 6 = Rotate 90 CCW, 7 = Rot90 + Mirror X
static int oled_orientation = 0;  // CPU-controlled orientation (default: no additional transform)

// Base orientation for physical mounting compensation (always applied AFTER cpu orientation)
// Set to 1 (180° rotation) because display is physically mounted upside down
static const int BASE_OLED_ORIENTATION = 1;

// ============================================================
// GPU Stats (for REQUEST_STATS response)
// ============================================================
static float current_fps = 0.0f;           // Updated every 2 seconds in main loop
static uint32_t current_free_heap = 0;     // Free heap bytes
static uint32_t current_min_heap = 0;      // Minimum free heap seen
static uint8_t gpu_load_percent = 0;       // Estimated GPU load (0-100)
static uint32_t total_frames = 0;          // Total frames rendered since boot

// ============================================================
// Boot Animation & No Signal State
// ============================================================
enum class BootState {
  FADE_IN,       // Fading in the logo
  HOLD,          // Waiting for CPU connection
  FADE_OUT,      // Fading out the logo
  RUNNING,       // Normal operation
  NO_SIGNAL      // CPU disconnected - show no signal animation
};

static BootState bootState = BootState::FADE_IN;
static int64_t bootStartTime = 0;
static int64_t lastCpuCommandTime = 0;      // Any command (for cpuConnected)
static int64_t lastDisplayCommandTime = 0;   // Only display commands (for NO_SIGNAL timeout)
static int64_t fadeOutStartTime = 0;
static bool cpuConnected = false;
static const int64_t FADE_DURATION_US = 1500000;  // 1.5 seconds
static const int64_t NO_SIGNAL_TIMEOUT_US = 3000000;  // 3 seconds

// Logo vertices (scaled from 445x308 SVG)
// Circle: center (216, 114), radius 39.5
// Main outline path (simplified to key vertices)
static const int16_t LOGO_OUTLINE[] = {
  // Y coordinates flipped (228 - originalY)
  238, 225, 221, 227, 161, 227, 142, 226, 106, 223, 89, 222, 73, 217, 59, 212,
  49, 207, 36, 197, 27, 189, 20, 180, 14, 170, 7, 153, 1, 129, 1, 119,
  1, 112, 2, 106, 5, 102, 9, 99, 22, 95, 38, 90, 59, 83, 75, 77,
  90, 69, 102, 61, 117, 50, 131, 39, 140, 30, 149, 22, 159, 16,
  171, 10, 186, 4, 201, 1, 216, 0, 230, 1, 242, 4, 259, 9,
  279, 19, 292, 29, 302, 39, 312, 52, 319, 64, 323, 74, 327, 89,
  329, 106, 329, 122, 327, 139, 322, 155, 317, 167, 311, 177, 304, 185,
  294, 196, 281, 205, 268, 213, 256, 219, 238, 225
};
static const int LOGO_OUTLINE_COUNT = sizeof(LOGO_OUTLINE) / (2 * sizeof(int16_t));

// Right decorative path (simplified)
static const int16_t LOGO_RIGHT[] = {
  385, 131, 348, 78, 343, 77, 342, 81, 344, 88, 346, 100, 346, 112,
  345, 127, 343, 140, 339, 156, 332, 171, 323, 189, 312, 204, 298, 217,
  286, 225, 284, 230, 285, 236, 289, 240, 302, 242, 320, 245, 339, 251,
  355, 258, 372, 267, 405, 288, 433, 305, 440, 308, 443, 308, 444, 306,
  444, 290, 442, 272, 434, 240, 420, 199, 405, 166, 385, 131
};
static const int LOGO_RIGHT_COUNT = sizeof(LOGO_RIGHT) / (2 * sizeof(int16_t));

// Circle parameters
static const float LOGO_CIRCLE_X = 216.0f;
static const float LOGO_CIRCLE_Y = 114.0f;
static const float LOGO_CIRCLE_R = 39.5f;
static const float LOGO_WIDTH = 445.0f;
static const float LOGO_HEIGHT = 308.0f;

// ============================================================
// Pixel Operations
// ============================================================

// Alpha-blend a pixel (for anti-aliasing)
// alpha: 0-255, where 255 = fully opaque
static inline void blendPixelHUB75(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) {
  if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) return;
  if (alpha == 0) return;
  // Mirror X: flip left-to-right
  int mx = (TOTAL_WIDTH - 1) - x;
  int idx = (y * TOTAL_WIDTH + mx) * 3;
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
  // Write directly to buffer - transforms applied in presentHUB75Buffer
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
  // Read directly from buffer (matches setPixelHUB75)
  int idx = (y * TOTAL_WIDTH + x) * 3;
  r = hub75_buffer[idx + 0];
  g = hub75_buffer[idx + 1];
  b = hub75_buffer[idx + 2];
}

// Transform coordinates based on a single orientation mode
static inline void applyOLEDTransform(int& x, int& y, int mode) {
  int tx, ty;
  switch (mode) {
    case 0:  // No transform
      break;
    case 1:  // Rotate 180° (mirror X and Y)
      x = (OLED_WIDTH - 1) - x;
      y = (OLED_HEIGHT - 1) - y;
      break;
    case 2:  // Mirror X only (horizontal flip)
      x = (OLED_WIDTH - 1) - x;
      break;
    case 3:  // Mirror Y only (vertical flip)
      y = (OLED_HEIGHT - 1) - y;
      break;
    case 4:  // Mirror X + Y (same as 180° rotate)
      x = (OLED_WIDTH - 1) - x;
      y = (OLED_HEIGHT - 1) - y;
      break;
    case 5:  // Rotate 90° CW
      tx = (OLED_HEIGHT - 1) - y;
      ty = x;
      x = tx;
      y = ty;
      break;
    case 6:  // Rotate 90° CCW
      tx = y;
      ty = (OLED_WIDTH - 1) - x;
      x = tx;
      y = ty;
      break;
    case 7:  // Rotate 90° CW + Mirror X
      tx = y;
      ty = x;
      x = tx;
      y = ty;
      break;
    default:
      break;
  }
}

// Transform coordinates: apply CPU orientation first, then base orientation for physical mounting
static inline void transformOLEDCoords(int& x, int& y) {
  // First apply CPU-requested orientation
  applyOLEDTransform(x, y, oled_orientation);
  // Then apply base orientation for physical mounting compensation
  applyOLEDTransform(x, y, BASE_OLED_ORIENTATION);
}

// Transform for internal GPU drawing (No Signal, etc.) - only base orientation
static inline void transformOLEDCoordsInternal(int& x, int& y) {
  applyOLEDTransform(x, y, BASE_OLED_ORIENTATION);
}

// setPixelOLED: For CPU commands - applies CPU orientation + base orientation
static inline void setPixelOLED(int x, int y, bool on) {
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
  
  // Apply CPU orientation + base orientation
  transformOLEDCoords(x, y);
  
  // Bounds check after transform
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
  
  int byte_idx = (y / 8) * OLED_WIDTH + x;
  int bit = y % 8;
  if (on) {
    oled_buffer[byte_idx] |= (1 << bit);
  } else {
    oled_buffer[byte_idx] &= ~(1 << bit);
  }
}

// setPixelOLEDInternal: For GPU internal drawing (No Signal, boot animation)
// Only applies base orientation for physical mounting, ignores CPU orientation
static inline void setPixelOLEDInternal(int x, int y, bool on) {
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
  
  // Only apply base orientation for physical mounting
  transformOLEDCoordsInternal(x, y);
  
  // Bounds check after transform
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
  
  // Apply orientation transform
  transformOLEDCoords(x, y);
  
  // Bounds check after transform
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
// Boot Animation Drawing Functions
// ============================================================

// Draw a simple Bresenham line (for boot animation - no AA needed)
// Uses setPixelOLEDInternal for OLED to apply only base orientation
static void bootDrawLine(int x0, int y0, int x1, int y1, uint8_t intensity, bool isOled) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  
  while (true) {
    if (isOled) {
      if (intensity > 127) setPixelOLEDInternal(x0, y0, true);
    } else {
      setPixelHUB75(x0, y0, intensity, intensity, intensity);
    }
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// Draw a circle outline (for boot animation)
// Uses setPixelOLEDInternal for OLED to apply only base orientation
static void bootDrawCircle(int cx, int cy, int r, uint8_t intensity, bool isOled) {
  int x = r, y = 0;
  int err = 1 - r;
  
  while (x >= y) {
    if (isOled) {
      if (intensity > 127) {
        setPixelOLEDInternal(cx + x, cy + y, true); setPixelOLEDInternal(cx - x, cy + y, true);
        setPixelOLEDInternal(cx + x, cy - y, true); setPixelOLEDInternal(cx - x, cy - y, true);
        setPixelOLEDInternal(cx + y, cy + x, true); setPixelOLEDInternal(cx - y, cy + x, true);
        setPixelOLEDInternal(cx + y, cy - x, true); setPixelOLEDInternal(cx - y, cy - x, true);
      }
    } else {
      setPixelHUB75(cx + x, cy + y, intensity, intensity, intensity);
      setPixelHUB75(cx - x, cy + y, intensity, intensity, intensity);
      setPixelHUB75(cx + x, cy - y, intensity, intensity, intensity);
      setPixelHUB75(cx - x, cy - y, intensity, intensity, intensity);
      setPixelHUB75(cx + y, cy + x, intensity, intensity, intensity);
      setPixelHUB75(cx - y, cy + x, intensity, intensity, intensity);
      setPixelHUB75(cx + y, cy - x, intensity, intensity, intensity);
      setPixelHUB75(cx - y, cy - x, intensity, intensity, intensity);
    }
    y++;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      x--;
      err += 2 * (y - x + 1);
    }
  }
}

// Draw the logo on a display with given scale and offset
static void drawLogoScaled(float scale, float offsetX, float offsetY, uint8_t intensity, bool isOled) {
  // Draw main outline
  for (int i = 0; i < LOGO_OUTLINE_COUNT - 1; i++) {
    int x0 = (int)(LOGO_OUTLINE[i * 2] * scale + offsetX);
    int y0 = (int)(LOGO_OUTLINE[i * 2 + 1] * scale + offsetY);
    int x1 = (int)(LOGO_OUTLINE[(i + 1) * 2] * scale + offsetX);
    int y1 = (int)(LOGO_OUTLINE[(i + 1) * 2 + 1] * scale + offsetY);
    bootDrawLine(x0, y0, x1, y1, intensity, isOled);
  }
  // Close the outline
  {
    int i = LOGO_OUTLINE_COUNT - 1;
    int x0 = (int)(LOGO_OUTLINE[i * 2] * scale + offsetX);
    int y0 = (int)(LOGO_OUTLINE[i * 2 + 1] * scale + offsetY);
    int x1 = (int)(LOGO_OUTLINE[0] * scale + offsetX);
    int y1 = (int)(LOGO_OUTLINE[1] * scale + offsetY);
    bootDrawLine(x0, y0, x1, y1, intensity, isOled);
  }
  
  // Draw right decorative path
  for (int i = 0; i < LOGO_RIGHT_COUNT - 1; i++) {
    int x0 = (int)(LOGO_RIGHT[i * 2] * scale + offsetX);
    int y0 = (int)(LOGO_RIGHT[i * 2 + 1] * scale + offsetY);
    int x1 = (int)(LOGO_RIGHT[(i + 1) * 2] * scale + offsetX);
    int y1 = (int)(LOGO_RIGHT[(i + 1) * 2 + 1] * scale + offsetY);
    bootDrawLine(x0, y0, x1, y1, intensity, isOled);
  }
  
  // Draw inner circle
  int cx = (int)(LOGO_CIRCLE_X * scale + offsetX);
  int cy = (int)(LOGO_CIRCLE_Y * scale + offsetY);
  int cr = (int)(LOGO_CIRCLE_R * scale);
  if (cr > 0) bootDrawCircle(cx, cy, cr, intensity, isOled);
}

// Draw "NO SIGNAL" text (simplified pixel font)
// mirrorX: if true, draw text mirrored horizontally (for right panel pre-flip)
static void drawNoSignalText(int x, int y, uint8_t intensity, bool isOled, bool mirrorX = false) {
  // Simplified 5x7 font for "NO SIGNAL"
  // Each character is represented as 5 columns of 7 bits
  static const uint8_t FONT_N[] = {0x7F, 0x04, 0x08, 0x10, 0x7F};  // N
  static const uint8_t FONT_O[] = {0x3E, 0x41, 0x41, 0x41, 0x3E};  // O
  static const uint8_t FONT_S[] = {0x26, 0x49, 0x49, 0x49, 0x32};  // S (fixed: was horizontally mirrored)
  static const uint8_t FONT_I[] = {0x00, 0x41, 0x7F, 0x41, 0x00};  // I
  static const uint8_t FONT_G[] = {0x3E, 0x41, 0x49, 0x49, 0x3A};  // G
  static const uint8_t FONT_A[] = {0x7E, 0x09, 0x09, 0x09, 0x7E};  // A
  static const uint8_t FONT_L[] = {0x7F, 0x40, 0x40, 0x40, 0x40};  // L
  
  const uint8_t* letters[] = {FONT_N, FONT_O, nullptr, FONT_S, FONT_I, FONT_G, FONT_N, FONT_A, FONT_L};
  const int textWidth = 54;  // 9 chars * 6px spacing - 1
  
  int cx = x;
  for (int li = 0; li < 9; li++) {
    if (letters[li] == nullptr) {
      cx += 4;  // Space
      continue;
    }
    for (int col = 0; col < 5; col++) {
      uint8_t colData = letters[li][col];
      for (int row = 0; row < 7; row++) {
        if (colData & (1 << row)) {
          int drawX = cx + col;
          // If mirroring, flip X position relative to text start
          if (mirrorX) {
            drawX = x + (textWidth - 1) - (cx - x + col);
          }
          if (isOled) {
            if (intensity > 127) setPixelOLEDInternal(drawX, y + row, true);
          } else {
            setPixelHUB75(drawX, y + row, intensity, intensity, intensity);
          }
        }
      }
    }
    cx += 6;  // Character width + spacing
  }
}

// ============================================================
// PANEL DIAGNOSTIC TEST - CONFIGURABLE SETTINGS
// ============================================================
// Adjust these to find the correct panel configuration

// Panel 0 settings (left 64 pixels in buffer)
static bool PANEL0_MIRROR_X = false;  // Flip horizontally within panel
static bool PANEL0_MIRROR_Y = false;  // Flip vertically within panel
static bool PANEL0_SWAP = false;      // If true, panel 0 data goes to right side

// Panel 1 settings (right 64 pixels in buffer)
static bool PANEL1_MIRROR_X = true;   // Flip horizontally within panel
static bool PANEL1_MIRROR_Y = false;  // Flip vertically within panel

// Global settings
static bool GLOBAL_MIRROR_X = true;   // Mirror entire X axis before panel split
static bool GLOBAL_SWAP_PANELS = true; // Swap left/right panel positions

// RGB Channel order: 0=RGB, 1=RBG, 2=GRB, 3=GBR, 4=BRG, 5=BGR
static int RGB_ORDER = 1;  // Try RBG (swap G and B)

// Enable diagnostic test mode (set true to run test on boot)
static bool RUN_PANEL_TEST = false;  // VERIFIED: cyan square moves correctly with these settings

// Helper to set pixel (no RGB correction here - applied in presentHUB75Buffer)
static inline void setDiagPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) return;
  int idx = (y * TOTAL_WIDTH + x) * 3;
  
  // Write raw RGB - correction applied in presentHUB75Buffer
  hub75_buffer[idx + 0] = r;
  hub75_buffer[idx + 1] = g;
  hub75_buffer[idx + 2] = b;
}

// Present HUB75 buffer to display with configurable panel transforms
static void presentHUB75Buffer() {
  if (!hub75_ok || !hub75) return;
  
  for (int y = 0; y < TOTAL_HEIGHT; y++) {
    for (int x = 0; x < TOTAL_WIDTH; x++) {
      // Start with buffer coordinates
      int bufX = x;
      int bufY = y;
      
      // Apply global X mirror if enabled
      if (GLOBAL_MIRROR_X) {
        bufX = (TOTAL_WIDTH - 1) - bufX;
      }
      
      // Determine which panel this pixel belongs to (in buffer space)
      bool isRightPanel = (bufX >= PANEL_WIDTH);
      int panelX = isRightPanel ? (bufX - PANEL_WIDTH) : bufX;
      
      // Apply per-panel transforms
      int transformedPanelX = panelX;
      int transformedY = bufY;
      
      if (isRightPanel) {
        // Panel 1 transforms
        if (PANEL1_MIRROR_X) transformedPanelX = (PANEL_WIDTH - 1) - panelX;
        if (PANEL1_MIRROR_Y) transformedY = (TOTAL_HEIGHT - 1) - bufY;
      } else {
        // Panel 0 transforms
        if (PANEL0_MIRROR_X) transformedPanelX = (PANEL_WIDTH - 1) - panelX;
        if (PANEL0_MIRROR_Y) transformedY = (TOTAL_HEIGHT - 1) - bufY;
      }
      
      // Calculate display X position
      int displayX;
      if (GLOBAL_SWAP_PANELS) {
        // Swap panel positions
        if (isRightPanel) {
          displayX = transformedPanelX;  // Right panel goes to left side
        } else {
          displayX = PANEL_WIDTH + transformedPanelX;  // Left panel goes to right side
        }
      } else {
        // Normal panel positions
        if (isRightPanel) {
          displayX = PANEL_WIDTH + transformedPanelX;
        } else {
          displayX = transformedPanelX;
        }
      }
      
      // Handle PANEL0_SWAP (swap which panel's data goes where)
      int idx;
      if (PANEL0_SWAP) {
        // This swaps source data, not output position
        // Read from opposite panel
        int readX = isRightPanel ? panelX : (PANEL_WIDTH + panelX);
        idx = (bufY * TOTAL_WIDTH + readX) * 3;
      } else {
        idx = (bufY * TOTAL_WIDTH + x) * 3;
      }
      
      // Apply RGB channel order correction
      uint8_t r = hub75_buffer[idx];
      uint8_t g = hub75_buffer[idx + 1];
      uint8_t b = hub75_buffer[idx + 2];
      uint8_t ch0, ch1, ch2;
      switch (RGB_ORDER) {
        case 0: ch0 = r; ch1 = g; ch2 = b; break;  // RGB
        case 1: ch0 = r; ch1 = b; ch2 = g; break;  // RBG
        case 2: ch0 = g; ch1 = r; ch2 = b; break;  // GRB
        case 3: ch0 = g; ch1 = b; ch2 = r; break;  // GBR
        case 4: ch0 = b; ch1 = r; ch2 = g; break;  // BRG
        case 5: ch0 = b; ch1 = g; ch2 = r; break;  // BGR
        default: ch0 = r; ch1 = g; ch2 = b; break;
      }
      hub75->setPixel(displayX, transformedY, RGB(ch0, ch1, ch2));
    }
  }
  hub75->show();
}

// Draw panel diagnostic test pattern
// Red square: top-left (should appear at x=0-7, y=0-7)
// Blue square: bottom-right (should appear at x=120-127, y=24-31)
// Yellow square: center (should appear at x=60-67, y=12-19)
// Helper to draw a line using Bresenham's algorithm
static void drawDiagLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    setDiagPixel(x0, y0, r, g, b);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// Helper to draw a filled circle using midpoint algorithm
static void drawDiagCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) {
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      if (x*x + y*y <= radius*radius) {
        setDiagPixel(cx + x, cy + y, r, g, b);
      }
    }
  }
}

// Helper to draw a filled rectangle
static void drawDiagRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  for (int py = y; py < y + h; py++) {
    for (int px = x; px < x + w; px++) {
      setDiagPixel(px, py, r, g, b);
    }
  }
}

// Diagnostic pattern for each panel: 4 corner squares + connecting lines + center shape
static void drawPanelDiagnostic() {
  // Clear buffer
  memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
  
  const int SQUARE_SIZE = 6;
  const int HALF_SQ = SQUARE_SIZE / 2;
  
  // For each panel (left=0, right=1)
  for (int panel = 0; panel < 2; panel++) {
    int panelX = panel * PANEL_WIDTH;  // 0 for left, 64 for right
    
    // Corner positions (relative to panel)
    // Top-left corner: (0, 0)
    // Top-right corner: (PANEL_WIDTH-1, 0)
    // Bottom-left corner: (0, TOTAL_HEIGHT-1)
    // Bottom-right corner: (PANEL_WIDTH-1, TOTAL_HEIGHT-1)
    
    int tlX = panelX + HALF_SQ;                      // Top-left center
    int tlY = HALF_SQ;
    int trX = panelX + PANEL_WIDTH - 1 - HALF_SQ;   // Top-right center
    int trY = HALF_SQ;
    int blX = panelX + HALF_SQ;                      // Bottom-left center
    int blY = TOTAL_HEIGHT - 1 - HALF_SQ;
    int brX = panelX + PANEL_WIDTH - 1 - HALF_SQ;   // Bottom-right center
    int brY = TOTAL_HEIGHT - 1 - HALF_SQ;
    
    // Draw 4 corner squares:
    // Top-right = RED
    drawDiagRect(trX - HALF_SQ, trY - HALF_SQ, SQUARE_SIZE, SQUARE_SIZE, 255, 0, 0);
    // Top-left = BLUE
    drawDiagRect(tlX - HALF_SQ, tlY - HALF_SQ, SQUARE_SIZE, SQUARE_SIZE, 0, 0, 255);
    // Bottom-left = GREEN
    drawDiagRect(blX - HALF_SQ, blY - HALF_SQ, SQUARE_SIZE, SQUARE_SIZE, 0, 255, 0);
    // Bottom-right = WHITE
    drawDiagRect(brX - HALF_SQ, brY - HALF_SQ, SQUARE_SIZE, SQUARE_SIZE, 255, 255, 255);
    
    // Draw connecting lines between square centers:
    // RED line: Red (top-right) to Blue (top-left) - top edge
    drawDiagLine(trX, trY, tlX, tlY, 255, 0, 0);
    // BLUE line: Blue (top-left) to Green (bottom-left) - left edge
    drawDiagLine(tlX, tlY, blX, blY, 0, 0, 255);
    // GREEN line: Green (bottom-left) to White (bottom-right) - bottom edge
    drawDiagLine(blX, blY, brX, brY, 0, 255, 0);
    // WHITE line: White (bottom-right) to Red (top-right) - right edge
    drawDiagLine(brX, brY, trX, trY, 255, 255, 255);
    
    // Center shape
    int centerX = panelX + PANEL_WIDTH / 2;
    int centerY = TOTAL_HEIGHT / 2;
    
    if (panel == 0) {
      // Left panel: draw a circle (outline)
      int radius = 8;
      for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
          int distSq = x*x + y*y;
          // Draw if on the circle edge (between inner and outer radius)
          if (distSq >= (radius-1)*(radius-1) && distSq <= radius*radius) {
            setDiagPixel(centerX + x, centerY + y, 255, 255, 0);  // YELLOW circle
          }
        }
      }
    } else {
      // Right panel: draw a square (outline)
      int halfSize = 8;
      // Top and bottom edges
      for (int x = -halfSize; x <= halfSize; x++) {
        setDiagPixel(centerX + x, centerY - halfSize, 255, 0, 255);  // MAGENTA
        setDiagPixel(centerX + x, centerY + halfSize, 255, 0, 255);
      }
      // Left and right edges
      for (int y = -halfSize; y <= halfSize; y++) {
        setDiagPixel(centerX - halfSize, centerY + y, 255, 0, 255);
        setDiagPixel(centerX + halfSize, centerY + y, 255, 0, 255);
      }
    }
  }
  
  // Draw panel divider line (between the two panels)
  for (int y = 0; y < TOTAL_HEIGHT; y++) {
    setDiagPixel(63, y, 128, 128, 128);  // Gray divider
    setDiagPixel(64, y, 128, 128, 128);
  }
  
  // Print current config to log
  ESP_LOGI(TAG, "=== PANEL DIAGNOSTIC ===");
  ESP_LOGI(TAG, "RGB_ORDER=%d (0=RGB,1=RBG,2=GRB,3=GBR,4=BRG,5=BGR)", RGB_ORDER);
  ESP_LOGI(TAG, "Panel0: MirrorX=%d MirrorY=%d Swap=%d", PANEL0_MIRROR_X, PANEL0_MIRROR_Y, PANEL0_SWAP);
  ESP_LOGI(TAG, "Panel1: MirrorX=%d MirrorY=%d", PANEL1_MIRROR_X, PANEL1_MIRROR_Y);
  ESP_LOGI(TAG, "Global: MirrorX=%d SwapPanels=%d", GLOBAL_MIRROR_X, GLOBAL_SWAP_PANELS);
  ESP_LOGI(TAG, "Expected: RED=top-left, BLUE=bottom-right, YELLOW=center");
  ESP_LOGI(TAG, "GREEN line = panel boundary (x=63/64)");
  ESP_LOGI(TAG, "WHITE logos centered on each panel");
  
  presentHUB75Buffer();
  
  // === OLED Display ===
  // Top section (y=0-47): Reference logos (how they SHOULD look)
  // Middle section (y=48): Divider line
  // Bottom section (y=49-80): HUB75 buffer replication 1:1 (32 rows)
  // Lower section (y=88-119): Second copy showing what HUB75 buffer looks like after transforms
  memset(oled_buffer, 0, OLED_BUFFER_SIZE);
  
  // Draw reference logos on OLED top area (y=8-40)
  // These are drawn directly without any panel transforms
  float oledLogoScale = 0.10f;
  float oledLogoW = LOGO_WIDTH * oledLogoScale;
  float oledLogoH = LOGO_HEIGHT * oledLogoScale;
  
  // Left reference logo (x=0-63, y=8-40)
  float oled_left_offsetX = (64 - oledLogoW) / 2.0f;
  float oled_left_offsetY = 8 + (32 - oledLogoH) / 2.0f;
  
  for (int i = 0; i < LOGO_OUTLINE_COUNT - 1; i++) {
    int x0 = (int)(LOGO_OUTLINE[i * 2] * oledLogoScale + oled_left_offsetX);
    int y0 = (int)(LOGO_OUTLINE[i * 2 + 1] * oledLogoScale + oled_left_offsetY);
    int x1 = (int)(LOGO_OUTLINE[(i + 1) * 2] * oledLogoScale + oled_left_offsetX);
    int y1 = (int)(LOGO_OUTLINE[(i + 1) * 2 + 1] * oledLogoScale + oled_left_offsetY);
    // Bresenham line for OLED
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      if (x0 >= 0 && x0 < OLED_WIDTH && y0 >= 0 && y0 < OLED_HEIGHT) {
        int byteIdx = (y0 / 8) * OLED_WIDTH + x0;
        int bitIdx = y0 % 8;
        oled_buffer[byteIdx] |= (1 << bitIdx);
      }
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  
  // Right reference logo (x=64-127, y=8-40)
  float oled_right_offsetX = 64 + (64 - oledLogoW) / 2.0f;
  float oled_right_offsetY = 8 + (32 - oledLogoH) / 2.0f;
  
  for (int i = 0; i < LOGO_OUTLINE_COUNT - 1; i++) {
    int x0 = (int)(LOGO_OUTLINE[i * 2] * oledLogoScale + oled_right_offsetX);
    int y0 = (int)(LOGO_OUTLINE[i * 2 + 1] * oledLogoScale + oled_right_offsetY);
    int x1 = (int)(LOGO_OUTLINE[(i + 1) * 2] * oledLogoScale + oled_right_offsetX);
    int y1 = (int)(LOGO_OUTLINE[(i + 1) * 2 + 1] * oledLogoScale + oled_right_offsetY);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      if (x0 >= 0 && x0 < OLED_WIDTH && y0 >= 0 && y0 < OLED_HEIGHT) {
        int byteIdx = (y0 / 8) * OLED_WIDTH + x0;
        int bitIdx = y0 % 8;
        oled_buffer[byteIdx] |= (1 << bitIdx);
      }
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  
  // Draw horizontal line separating reference from replication (y=48)
  for (int x = 0; x < OLED_WIDTH; x++) {
    int byteIdx = (48 / 8) * OLED_WIDTH + x;
    int bitIdx = 48 % 8;
    oled_buffer[byteIdx] |= (1 << bitIdx);
  }
  
  // Draw vertical line at center (x=64) for reference area
  for (int y = 0; y < 48; y++) {
    int byteIdx = (y / 8) * OLED_WIDTH + 64;
    int bitIdx = y % 8;
    oled_buffer[byteIdx] |= (1 << bitIdx);
  }
  
  // HUB75 buffer replication 1:1 (y=49-80, which is 32 rows for 128x32)
  // This shows what's in the HUB75 buffer - use raw buffer (correct!)
  int replicationY = 49;
  for (int y = 0; y < TOTAL_HEIGHT; y++) {
    for (int x = 0; x < TOTAL_WIDTH; x++) {
      int idx = (y * TOTAL_WIDTH + x) * 3;
      uint8_t r = hub75_buffer[idx + 0];
      uint8_t g = hub75_buffer[idx + 1];
      uint8_t b = hub75_buffer[idx + 2];
      
      // Luminance threshold (lowered to catch pure blue which has lum=28)
      uint16_t lum = (77 * r + 150 * g + 29 * b) >> 8;
      if (lum >= 16) {
        int oledY = replicationY + y;
        if (oledY < OLED_HEIGHT) {
          int byteIdx = (oledY / 8) * OLED_WIDTH + x;
          int bitIdx = oledY % 8;
          oled_buffer[byteIdx] |= (1 << bitIdx);
        }
      }
    }
  }
  
  // Update OLED directly (task not running yet)
  if (oled_ok && oled) {
    memcpy(oled->getBuffer(), oled_buffer, OLED_BUFFER_SIZE);
    oled->updateDisplay();
  }
  
  ESP_LOGI(TAG, "OLED: Top=Reference logos, Bottom=Raw HUB75 buffer (1:1)");
}

// Calculate logo scale to fit within constraints:
// - Max 50% of screen width
// - Max 80% of screen height
// Returns scale factor and offsets for centering
static void calculateLogoFit(int screenW, int screenH, float& scale, float& offsetX, float& offsetY) {
  float maxW = screenW * 0.5f;   // 50% of width
  float maxH = screenH * 0.8f;   // 80% of height
  
  // Calculate scale to fit within both constraints
  float scaleW = maxW / LOGO_WIDTH;
  float scaleH = maxH / LOGO_HEIGHT;
  scale = (scaleW < scaleH) ? scaleW : scaleH;  // Use smaller scale
  
  // Calculate centered position
  float scaledW = LOGO_WIDTH * scale;
  float scaledH = LOGO_HEIGHT * scale;
  offsetX = (screenW - scaledW) / 2.0f;
  offsetY = (screenH - scaledH) / 2.0f;
}

// Update boot animation - returns true if still in boot/no-signal state
static bool updateBootAnimation() {
  int64_t now = esp_timer_get_time();
  
  switch (bootState) {
    case BootState::FADE_IN: {
      // If CPU connected during fade-in, skip directly to RUNNING
      if (cpuConnected) {
        bootState = BootState::RUNNING;
        ESP_LOGI(TAG, "Boot: CPU connected during fade-in, skipping to normal operation");
        return false;
      }
      
      int64_t elapsed = now - bootStartTime;
      float progress = (float)elapsed / (float)FADE_DURATION_US;
      if (progress >= 1.0f) progress = 1.0f;
      
      uint8_t intensity = (uint8_t)(progress * 255.0f);
      
      // Clear buffers
      memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
      memset(oled_buffer, 0, OLED_BUFFER_SIZE);
      
      // Draw logo on each HUB75 panel (2 panels, 64x32 each)
      float hub75_scale, hub75_offsetX, hub75_offsetY;
      calculateLogoFit(PANEL_WIDTH, PANEL_HEIGHT, hub75_scale, hub75_offsetX, hub75_offsetY);
      // Left panel (0-63)
      drawLogoScaled(hub75_scale, hub75_offsetX, hub75_offsetY, intensity, false);
      // Right panel (64-127)
      drawLogoScaled(hub75_scale, hub75_offsetX + PANEL_WIDTH, hub75_offsetY, intensity, false);
      
      // Draw logo on OLED (128x128) - single display
      float oled_scale, oled_offsetX, oled_offsetY;
      calculateLogoFit(OLED_WIDTH, OLED_HEIGHT, oled_scale, oled_offsetX, oled_offsetY);
      drawLogoScaled(oled_scale, oled_offsetX, oled_offsetY, intensity, true);
      
      // Present displays
      presentHUB75Buffer();
      if (oled_ok) {
        memcpy(oled_update_buffer, oled_buffer, OLED_BUFFER_SIZE);
        oled_update_pending = true;
      }
      
      if (progress >= 1.0f) {
        bootState = BootState::HOLD;
        ESP_LOGI(TAG, "Boot: Fade-in complete, waiting for CPU...");
      }
      return true;
    }
    
    case BootState::HOLD: {
      // Check if CPU has connected - skip directly to RUNNING to avoid display conflict
      if (cpuConnected) {
        bootState = BootState::RUNNING;
        ESP_LOGI(TAG, "Boot: CPU connected, skipping to normal operation");
        return false;  // Immediately let CPU take control
      }
      
      // Timeout: if CPU doesn't connect within 5 seconds of boot, show NO SIGNAL
      int64_t holdElapsed = now - bootStartTime - FADE_DURATION_US;
      if (!cpuConnected && holdElapsed > 5000000) {  // 5 seconds after fade-in
        bootState = BootState::NO_SIGNAL;
        ESP_LOGW(TAG, "Boot: CPU connection timeout, showing No Signal");
      }
      
      // Keep displaying logo at full brightness
      memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
      memset(oled_buffer, 0, OLED_BUFFER_SIZE);
      
      // Draw logo on each HUB75 panel (2 panels, 64x32 each)
      float hub75_scale, hub75_offsetX, hub75_offsetY;
      calculateLogoFit(PANEL_WIDTH, PANEL_HEIGHT, hub75_scale, hub75_offsetX, hub75_offsetY);
      // Left panel (0-63)
      drawLogoScaled(hub75_scale, hub75_offsetX, hub75_offsetY, 255, false);
      // Right panel (64-127)
      drawLogoScaled(hub75_scale, hub75_offsetX + PANEL_WIDTH, hub75_offsetY, 255, false);
      
      // Draw logo on OLED (128x128) - single display
      float oled_scale, oled_offsetX, oled_offsetY;
      calculateLogoFit(OLED_WIDTH, OLED_HEIGHT, oled_scale, oled_offsetX, oled_offsetY);
      drawLogoScaled(oled_scale, oled_offsetX, oled_offsetY, 255, true);
      
      // Present displays
      presentHUB75Buffer();
      if (oled_ok) {
        memcpy(oled_update_buffer, oled_buffer, OLED_BUFFER_SIZE);
        oled_update_pending = true;
      }
      return true;
    }
    
    case BootState::FADE_OUT: {
      int64_t elapsed = now - fadeOutStartTime;
      float progress = (float)elapsed / (float)FADE_DURATION_US;
      if (progress >= 1.0f) progress = 1.0f;
      
      uint8_t intensity = (uint8_t)((1.0f - progress) * 255.0f);
      
      // Clear buffers
      memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
      memset(oled_buffer, 0, OLED_BUFFER_SIZE);
      
      // Draw fading logo on each HUB75 panel (2 panels, 64x32 each)
      float hub75_scale, hub75_offsetX, hub75_offsetY;
      calculateLogoFit(PANEL_WIDTH, PANEL_HEIGHT, hub75_scale, hub75_offsetX, hub75_offsetY);
      // Left panel (0-63)
      drawLogoScaled(hub75_scale, hub75_offsetX, hub75_offsetY, intensity, false);
      // Right panel (64-127)
      drawLogoScaled(hub75_scale, hub75_offsetX + PANEL_WIDTH, hub75_offsetY, intensity, false);
      
      // Draw fading logo on OLED - single display
      float oled_scale, oled_offsetX, oled_offsetY;
      calculateLogoFit(OLED_WIDTH, OLED_HEIGHT, oled_scale, oled_offsetX, oled_offsetY);
      drawLogoScaled(oled_scale, oled_offsetX, oled_offsetY, intensity, true);
      
      // Present displays
      presentHUB75Buffer();
      if (oled_ok) {
        memcpy(oled_update_buffer, oled_buffer, OLED_BUFFER_SIZE);
        oled_update_pending = true;
      }
      
      if (progress >= 1.0f) {
        bootState = BootState::RUNNING;
        ESP_LOGI(TAG, "Boot: Splash complete, running normally");
      }
      return true;
    }
    
    case BootState::RUNNING: {
      // Check for CPU timeout - only display commands reset this, not PINGs
      // If we've never received a display command (lastDisplayCommandTime == 0), 
      // use bootStartTime as the reference point
      int64_t refTime = (lastDisplayCommandTime > 0) ? lastDisplayCommandTime : bootStartTime;
      if ((now - refTime) > NO_SIGNAL_TIMEOUT_US) {
        bootState = BootState::NO_SIGNAL;
        cpuConnected = false;
        ESP_LOGW(TAG, "CPU disconnected (no display commands for %.1fs) - showing No Signal",
                 (float)(now - refTime) / 1000000.0f);
      }
      return false;  // Normal operation
    }
    
    case BootState::NO_SIGNAL: {
      // Swaying "NO SIGNAL" animation to prevent burn-in
      float t = (float)now / 1000000.0f;  // Time in seconds (proper float division)
      
      // Clear buffers
      memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
      memset(oled_buffer, 0, OLED_BUFFER_SIZE);
      
      // "NO SIGNAL" text dimensions: 9 chars * 6px = 54px wide, 7px tall
      const int textW = 54;
      const int textH = 7;
      
      // HUB75: Sway within bounds of each panel (64x32 each)
      int hub75_maxSwayX = (PANEL_WIDTH - textW) / 2 - 2;  // Leave 2px margin
      int hub75_maxSwayY = (PANEL_HEIGHT - textH) / 2 - 2;
      if (hub75_maxSwayX < 0) hub75_maxSwayX = 0;
      if (hub75_maxSwayY < 0) hub75_maxSwayY = 0;
      float hub75_swayX = sinf(t * 0.5f) * hub75_maxSwayX;
      float hub75_swayY = cosf(t * 0.3f) * hub75_maxSwayY;
      int hub75_textX = (PANEL_WIDTH - textW) / 2 + (int)hub75_swayX;
      int hub75_textY = (PANEL_HEIGHT - textH) / 2 + (int)hub75_swayY;
      // Left panel (0-63) - draw mirrored so it appears correct after X-flip
      drawNoSignalText(hub75_textX, hub75_textY, 180, false, true);
      // Right panel (64-127) - draw normal (X-flip will make it correct)
      drawNoSignalText(hub75_textX + PANEL_WIDTH, hub75_textY, 180, false, false);
      
      // OLED: Single display, larger sway range since screen is bigger
      int oled_maxSwayX = (OLED_WIDTH - textW) / 2 - 4;
      int oled_maxSwayY = (OLED_HEIGHT - textH) / 2 - 4;
      float oled_swayX = sinf(t * 0.5f) * oled_maxSwayX;
      float oled_swayY = cosf(t * 0.3f) * oled_maxSwayY;
      int oled_textX = (OLED_WIDTH - textW) / 2 + (int)oled_swayX;
      int oled_textY = (OLED_HEIGHT - textH) / 2 + (int)oled_swayY;
      drawNoSignalText(oled_textX, oled_textY, 255, true);
      
      // Present displays
      presentHUB75Buffer();
      if (oled_ok) {
        memcpy(oled_update_buffer, oled_buffer, OLED_BUFFER_SIZE);
        oled_update_pending = true;
      }
      
      // Check if CPU reconnected
      if (cpuConnected) {
        bootState = BootState::RUNNING;
        ESP_LOGI(TAG, "CPU reconnected - resuming normal operation");
        return false;
      }
      
      vTaskDelay(pdMS_TO_TICKS(30));  // ~30 FPS for animation
      return true;
    }
  }
  return false;
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

// Helper: Accumulate color to a pixel with weight (for splatting)
// Uses additive blending to accumulate contributions from multiple sprite pixels
static float splat_r[TOTAL_WIDTH * TOTAL_HEIGHT];
static float splat_g[TOTAL_WIDTH * TOTAL_HEIGHT];
static float splat_b[TOTAL_WIDTH * TOTAL_HEIGHT];
static float splat_w[TOTAL_WIDTH * TOTAL_HEIGHT];

static inline void splatPixel(int x, int y, float r, float g, float b, float weight) {
  if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) return;
  int idx = y * TOTAL_WIDTH + x;
  splat_r[idx] += r * weight;
  splat_g[idx] += g * weight;
  splat_b[idx] += b * weight;
  splat_w[idx] += weight;
}

static inline void clearSplatBuffer() {
  memset(splat_r, 0, sizeof(splat_r));
  memset(splat_g, 0, sizeof(splat_g));
  memset(splat_b, 0, sizeof(splat_b));
  memset(splat_w, 0, sizeof(splat_w));
}

static inline void flushSplatBuffer(int minX, int minY, int maxX, int maxY) {
  for (int y = minY; y <= maxY; y++) {
    for (int x = minX; x <= maxX; x++) {
      if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT) continue;
      int idx = y * TOTAL_WIDTH + x;
      if (splat_w[idx] > 0.001f) {
        // Normalize accumulated color by total weight
        uint8_t r = (uint8_t)fminf(255.0f, splat_r[idx] / splat_w[idx]);
        uint8_t g = (uint8_t)fminf(255.0f, splat_g[idx] / splat_w[idx]);
        uint8_t b = (uint8_t)fminf(255.0f, splat_b[idx] / splat_w[idx]);
        
        // Alpha based on coverage (clamped to 1.0)
        float coverage = fminf(1.0f, splat_w[idx]);
        uint8_t alpha = (uint8_t)(coverage * 255.0f);
        
        if (alpha > 250) {
          setPixelHUB75(x, y, r, g, b);
        } else if (alpha > 4) {
          blendPixelHUB75(x, y, r, g, b, alpha);
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

// Filled rectangle with AA edges (sub-pixel precision)
static void fillRectF(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b) {
  if (w <= 0 || h <= 0) return;
  
  // Compute bounds with 1 pixel margin for AA edges
  int minX = (int)floorf(x);
  int maxX = (int)ceilf(x + w);
  int minY = (int)floorf(y);
  int maxY = (int)ceilf(y + h);
  
  // Clamp to screen
  if (minX < 0) minX = 0;
  if (minY < 0) minY = 0;
  if (maxX >= TOTAL_WIDTH) maxX = TOTAL_WIDTH - 1;
  if (maxY >= TOTAL_HEIGHT) maxY = TOTAL_HEIGHT - 1;
  
  float x1 = x, y1 = y, x2 = x + w, y2 = y + h;
  
  for (int py = minY; py <= maxY; py++) {
    for (int px = minX; px <= maxX; px++) {
      // Calculate pixel center coverage for AA
      float pxc = px + 0.5f;
      float pyc = py + 0.5f;
      
      // Distance from each edge (positive = inside)
      float dLeft   = pxc - x1;
      float dRight  = x2 - pxc;
      float dTop    = pyc - y1;
      float dBottom = y2 - pyc;
      
      // Minimum distance to any edge (clamped to [-0.5, 0.5])
      float coverage = fminf(fminf(dLeft, dRight), fminf(dTop, dBottom));
      
      if (aa_enabled && gpu.target == 0) {
        if (coverage >= 0.5f) {
          // Fully inside
          setPixelHUB75(px, py, r, g, b);
        } else if (coverage > -0.5f) {
          // Partial coverage for AA edge
          uint8_t alpha = (uint8_t)((coverage + 0.5f) * 255.0f);
          blendPixelHUB75(px, py, r, g, b, alpha);
        }
      } else {
        // No AA: simple threshold
        if (coverage >= 0) {
          setPixel(px, py, r, g, b);
        }
      }
    }
  }
}

// Sprite blit with sub-pixel positioning using BILINEAR SPLATTING
// Each sprite pixel "splats" its color to the 4 screen pixels it overlaps
// This creates smooth sub-pixel movement with proper AA
// O(spritePixels) complexity - same efficient algorithm as blitSpriteRotated
static void blitSpriteF(int id, float dx, float dy) {
  if (id < 0 || id >= MAX_SPRITES || !gpu.sprites[id].valid) return;
  
  Sprite& s = gpu.sprites[id];
  
  if (s.format == 0 && gpu.target == 0) {
    if (aa_enabled) {
      // Efficient bilinear splatting - O(spritePixels) complexity
      // Same algorithm as blitSpriteRotated but without rotation
      
      // Calculate bounding box for splat buffer clear
      int pMinX = (int)floorf(dx);
      int pMaxX = (int)ceilf(dx + s.width) + 1;
      int pMinY = (int)floorf(dy);
      int pMaxY = (int)ceilf(dy + s.height) + 1;
      if (pMinX < 0) pMinX = 0;
      if (pMinY < 0) pMinY = 0;
      if (pMaxX >= TOTAL_WIDTH) pMaxX = TOTAL_WIDTH - 1;
      if (pMaxY >= TOTAL_HEIGHT) pMaxY = TOTAL_HEIGHT - 1;
      
      // Clear the splat buffer for the affected region
      for (int y = pMinY; y <= pMaxY; y++) {
        for (int x = pMinX; x <= pMaxX; x++) {
          int idx = y * TOTAL_WIDTH + x;
          splat_r[idx] = splat_g[idx] = splat_b[idx] = splat_w[idx] = 0;
        }
      }
      
      // For each sprite pixel, splat to 4 overlapping screen pixels
      for (int sy = 0; sy < s.height; sy++) {
        for (int sx = 0; sx < s.width; sx++) {
          // Get sprite pixel color
          int sidx = (sy * s.width + sx) * 3;
          float pr = s.data[sidx + 0];
          float pg = s.data[sidx + 1];
          float pb = s.data[sidx + 2];
          
          // Screen position of this sprite pixel
          float screenX = dx + sx;
          float screenY = dy + sy;
          
          // Integer screen position and fractional offset
          int ix = (int)floorf(screenX);
          int iy = (int)floorf(screenY);
          float fx = screenX - ix;
          float fy = screenY - iy;
          
          // Bilinear splat weights
          float w00 = (1.0f - fx) * (1.0f - fy);
          float w10 = fx * (1.0f - fy);
          float w01 = (1.0f - fx) * fy;
          float w11 = fx * fy;
          
          // Splat to 4 overlapping screen pixels
          splatPixel(ix,     iy,     pr, pg, pb, w00);
          splatPixel(ix + 1, iy,     pr, pg, pb, w10);
          splatPixel(ix,     iy + 1, pr, pg, pb, w01);
          splatPixel(ix + 1, iy + 1, pr, pg, pb, w11);
        }
      }
      
      // Resolve splat buffer to screen
      for (int y = pMinY; y <= pMaxY; y++) {
        for (int x = pMinX; x <= pMaxX; x++) {
          int idx = y * TOTAL_WIDTH + x;
          if (splat_w[idx] > 0.001f) {
            uint8_t r = (uint8_t)fminf(255.0f, splat_r[idx] / splat_w[idx]);
            uint8_t g = (uint8_t)fminf(255.0f, splat_g[idx] / splat_w[idx]);
            uint8_t b = (uint8_t)fminf(255.0f, splat_b[idx] / splat_w[idx]);
            uint8_t alpha = (uint8_t)fminf(255.0f, splat_w[idx] * 255.0f);
            if (alpha > 250) {
              setPixelHUB75(x, y, r, g, b);
            } else if (alpha > 4) {
              blendPixelHUB75(x, y, r, g, b, alpha);
            }
          }
        }
      }
    } else {
      // No AA: Direct integer blit (nearest neighbor)
      int ix = (int)roundf(dx);
      int iy = (int)roundf(dy);
      for (int y = 0; y < s.height; y++) {
        for (int x = 0; x < s.width; x++) {
          int idx = (y * s.width + x) * 3;
          setPixelHUB75(ix + x, iy + y, s.data[idx], s.data[idx + 1], s.data[idx + 2]);
        }
      }
    }
  } else {
    // For OLED or non-RGB, fall back to integer blit
    blitSprite(id, (int)roundf(dx), (int)roundf(dy));
  }
}

// Sprite blit with rotation using BILINEAR SPLATTING
// Each sprite pixel is rotated and "splats" to overlapping screen pixels
// This creates smooth rotation AND sub-pixel movement with proper AA
static void blitSpriteRotated(int id, float dx, float dy, float angleDeg) {
  if (id < 0 || id >= MAX_SPRITES || !gpu.sprites[id].valid) {
    static int invalid_count = 0;
    if (++invalid_count <= 5) {
      ESP_LOGW(TAG, "blitSpriteRotated: invalid sprite id=%d valid=%d", id, 
               (id >= 0 && id < MAX_SPRITES) ? gpu.sprites[id].valid : -1);
    }
    return;
  }
  
  Sprite& s = gpu.sprites[id];
  
  if (s.format != 0 || gpu.target != 0) {
    // For OLED or mono sprites, fall back to non-rotated blit
    blitSprite(id, (int)roundf(dx), (int)roundf(dy));
    return;
  }
  
  // Convert angle to radians
  float angleRad = angleDeg * (3.14159265f / 180.0f);
  float cosA = cosf(angleRad);
  float sinA = sinf(angleRad);
  
  // Sprite center (pivot point for rotation)
  float cx = s.width / 2.0f;
  float cy = s.height / 2.0f;
  
  // Calculate bounding box of rotated sprite
  float corners[4][2] = {
    {-cx, -cy},
    {s.width - cx, -cy},
    {s.width - cx, s.height - cy},
    {-cx, s.height - cy}
  };
  
  float minX = 9999, maxX = -9999, minY = 9999, maxY = -9999;
  for (int i = 0; i < 4; i++) {
    float rx = corners[i][0] * cosA - corners[i][1] * sinA + dx;
    float ry = corners[i][0] * sinA + corners[i][1] * cosA + dy;
    if (rx < minX) minX = rx;
    if (rx > maxX) maxX = rx;
    if (ry < minY) minY = ry;
    if (ry > maxY) maxY = ry;
  }
  
  // Screen bounds with margin
  int pMinX = (int)floorf(minX) - 1;
  int pMaxX = (int)ceilf(maxX) + 1;
  int pMinY = (int)floorf(minY) - 1;
  int pMaxY = (int)ceilf(maxY) + 1;
  if (pMinX < 0) pMinX = 0;
  if (pMinY < 0) pMinY = 0;
  if (pMaxX >= TOTAL_WIDTH) pMaxX = TOTAL_WIDTH - 1;
  if (pMaxY >= TOTAL_HEIGHT) pMaxY = TOTAL_HEIGHT - 1;
  
  if (aa_enabled) {
    // BILINEAR SPLATTING with rotation
    // Clear the splat buffer for the affected region
    for (int y = pMinY; y <= pMaxY; y++) {
      for (int x = pMinX; x <= pMaxX; x++) {
        int idx = y * TOTAL_WIDTH + x;
        splat_r[idx] = splat_g[idx] = splat_b[idx] = splat_w[idx] = 0;
      }
    }
    
    // For each sprite pixel, rotate and splat to screen
    for (int sy = 0; sy < s.height; sy++) {
      for (int sx = 0; sx < s.width; sx++) {
        // Get sprite pixel color
        int sidx = (sy * s.width + sx) * 3;
        float pr = s.data[sidx + 0];
        float pg = s.data[sidx + 1];
        float pb = s.data[sidx + 2];
        
        // Position relative to sprite center
        float relX = sx - cx + 0.5f;  // +0.5 for pixel center
        float relY = sy - cy + 0.5f;
        
        // Rotate around center and translate to screen position
        float screenX = relX * cosA - relY * sinA + dx;
        float screenY = relX * sinA + relY * cosA + dy;
        
        // Integer screen position and fractional offset
        int ix = (int)floorf(screenX);
        int iy = (int)floorf(screenY);
        float fx = screenX - ix;
        float fy = screenY - iy;
        
        // Bilinear splat weights
        float w00 = (1.0f - fx) * (1.0f - fy);
        float w10 = fx * (1.0f - fy);
        float w01 = (1.0f - fx) * fy;
        float w11 = fx * fy;
        
        // Splat to 4 overlapping screen pixels
        splatPixel(ix,     iy,     pr, pg, pb, w00);
        splatPixel(ix + 1, iy,     pr, pg, pb, w10);
        splatPixel(ix,     iy + 1, pr, pg, pb, w01);
        splatPixel(ix + 1, iy + 1, pr, pg, pb, w11);
      }
    }
    
    // Flush accumulated colors to screen
    flushSplatBuffer(pMinX, pMinY, pMaxX, pMaxY);
    
  } else {
    // No AA: Inverse mapping with nearest neighbor
    for (int py = pMinY; py <= pMaxY; py++) {
      for (int px = pMinX; px <= pMaxX; px++) {
        float screenX = (float)px - dx;
        float screenY = (float)py - dy;
        
        // Inverse rotation
        float spriteX = screenX * cosA + screenY * sinA + cx;
        float spriteY = -screenX * sinA + screenY * cosA + cy;
        
        if (spriteX >= 0 && spriteX < s.width && spriteY >= 0 && spriteY < s.height) {
          int sx = (int)spriteX;
          int sy = (int)spriteY;
          int idx = (sy * s.width + sx) * 3;
          setPixelHUB75(px, py, s.data[idx], s.data[idx + 1], s.data[idx + 2]);
        }
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
  CLEAR_ALL_SPRITES = 0x22,  // Clear all sprites in one command
  
  // Chunked sprite upload protocol
  SPRITE_BEGIN = 0x23,     // Begin chunked upload: id, w, h, fmt, totalSize (2 bytes)
  SPRITE_CHUNK = 0x24,     // Data chunk: id, chunkIdx, data... (up to 256 bytes)
  SPRITE_END = 0x25,       // Finalize upload: id, expectedChunks
  
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
  DRAW_LINE_F = 0x48,    // Float coords: x0,y0,x1,y1 as 8.8 fixed point
  DRAW_CIRCLE_F = 0x49,  // Float coords: cx,cy,r as 8.8 fixed point
  DRAW_RECT_F = 0x4A,    // Float coords: x,y,w,h as 8.8 fixed point
  DRAW_FILL_F = 0x4B,    // Filled rect with AA edges: x,y,w,h as 8.8 fixed point
  BLIT_SPRITE_F = 0x4C,  // Sprite with sub-pixel position: id, x, y as 8.8 fixed point
  BLIT_SPRITE_ROT = 0x4D,// Sprite with rotation: id, x, y (8.8), angle (8.8 fixed = degrees)
  SET_AA = 0x4E,         // Toggle anti-aliasing: 0=off, 1=on (default on)
  
  SET_TARGET = 0x50,     // 0=HUB75, 1=OLED
  PRESENT = 0x51,        // Push framebuffer to display
  
  // OLED-specific commands (always target OLED buffer)
  OLED_CLEAR = 0x60,
  OLED_LINE = 0x61,
  OLED_RECT = 0x62,
  OLED_FILL = 0x63,
  OLED_CIRCLE = 0x64,
  OLED_PRESENT = 0x65,
  OLED_PIXEL = 0x66,
  OLED_VLINE = 0x67,     // Vertical line (fast for text rendering)
  OLED_HLINE = 0x68,     // Horizontal line
  OLED_FILL_CIRCLE = 0x69,
  OLED_SET_ORIENTATION = 0x6A,  // Set OLED orientation mode (0-7)
  OLED_TEXT = 0x6B,      // Native text rendering: x(2), y(2), scale(1), on(1), text(N)
  OLED_MIRROR_HUB75 = 0x6C,    // Mirror HUB75 to OLED: threshold(1), scaleMode(1), yOffset(1)
  
  // System commands
  PING = 0xF0,           // CPU ping request
  PONG = 0xF1,           // GPU pong response with uptime
  REQUEST_CONFIG = 0xF2, // Request GPU configuration
  CONFIG_RESPONSE = 0xF3,// GPU configuration response
  REQUEST_STATS = 0xF4,  // Request GPU performance stats
  STATS_RESPONSE = 0xF5, // GPU stats response (FPS, RAM, load)
  
  // GPU->CPU Alert System (GPU sends these automatically)
  ALERT = 0xF6,          // GPU alert notification
  CLEAR_ALERT = 0xF7,    // Clear specific alert condition
  REQUEST_ALERTS = 0xF8, // Request current alert status
  ALERTS_RESPONSE = 0xF9,// Response with all active alerts
  
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
static uint8_t cmd_buffer[512];  // Max single command payload (chunks are small)

// Chunked sprite upload state
struct ChunkedUpload {
  bool active = false;
  uint8_t spriteId = 0;
  uint8_t width = 0;
  uint8_t height = 0;
  uint8_t format = 0;
  uint16_t totalSize = 0;
  uint16_t receivedSize = 0;
  uint16_t expectedChunks = 0;
  uint16_t receivedChunks = 0;
  uint8_t* buffer = nullptr;
};
static ChunkedUpload chunkedUpload;

// ============================================================
// GPU Alert Sending Functions
// ============================================================

/**
 * Send an alert to the CPU
 * Alert packet format:
 *   Header: [0xAA][0x55][0xF6][len_lo][len_hi]
 *   Payload (16 bytes):
 *     [0]     AlertLevel (0=INFO, 1=WARNING, 2=ERROR, 3=CRITICAL)
 *     [1]     AlertType (specific alert code)
 *     [2-5]   value1 (uint32_t) - context-specific value
 *     [6-9]   value2 (uint32_t) - context-specific value  
 *     [10-13] timestamp_ms (uint32_t) - uptime when alert occurred
 *     [14-15] alert_count (uint16_t) - total alerts sent
 */
static void sendAlert(AlertLevel level, AlertType type, uint32_t value1 = 0, uint32_t value2 = 0) {
  // Rate limit alerts to prevent flooding the CPU
  int64_t now = esp_timer_get_time();
  if (now - lastAlertTime < MIN_ALERT_INTERVAL_US && type == lastAlertType) {
    return;  // Skip duplicate alerts that are too frequent
  }
  lastAlertTime = now;
  lastAlertType = type;
  alertsSent++;
  
  uint32_t uptime_ms = (now - gpu.startTime) / 1000;
  
  // Build alert packet
  uint8_t header[5] = {
    0xAA, 0x55,
    static_cast<uint8_t>(CmdType::ALERT),
    16, 0  // Payload length = 16 bytes
  };
  
  uint8_t payload[16];
  payload[0] = static_cast<uint8_t>(level);
  payload[1] = static_cast<uint8_t>(type);
  // value1 (little-endian)
  payload[2] = (value1 >>  0) & 0xFF;
  payload[3] = (value1 >>  8) & 0xFF;
  payload[4] = (value1 >> 16) & 0xFF;
  payload[5] = (value1 >> 24) & 0xFF;
  // value2 (little-endian)
  payload[6] = (value2 >>  0) & 0xFF;
  payload[7] = (value2 >>  8) & 0xFF;
  payload[8] = (value2 >> 16) & 0xFF;
  payload[9] = (value2 >> 24) & 0xFF;
  // timestamp (little-endian)
  payload[10] = (uptime_ms >>  0) & 0xFF;
  payload[11] = (uptime_ms >>  8) & 0xFF;
  payload[12] = (uptime_ms >> 16) & 0xFF;
  payload[13] = (uptime_ms >> 24) & 0xFF;
  // alert count (little-endian)
  payload[14] = (alertsSent >>  0) & 0xFF;
  payload[15] = (alertsSent >>  8) & 0xFF;
  
  uart_write_bytes(UART_PORT, header, sizeof(header));
  uart_write_bytes(UART_PORT, payload, sizeof(payload));
  
  // Log locally too
  const char* levelStr = (level == AlertLevel::INFO) ? "INFO" :
                         (level == AlertLevel::WARNING) ? "WARN" :
                         (level == AlertLevel::ERROR) ? "ERROR" : "CRIT";
  ESP_LOGW(TAG, "ALERT [%s] type=0x%02X val1=%lu val2=%lu", 
           levelStr, (int)type, (unsigned long)value1, (unsigned long)value2);
}

/**
 * Check various conditions and send alerts as needed
 * Called periodically from the UART task
 */
static void checkAndSendAlerts(size_t bufferUsed, size_t bufferSize) {
  int64_t now = esp_timer_get_time();
  
  // --- Buffer level alerts ---
  float bufferPercent = (bufferUsed * 100.0f) / bufferSize;
  
  if (bufferPercent > 75.0f) {
    if (!bufferWarningActive || bufferPercent > 90.0f) {
      sendAlert(AlertLevel::ERROR, AlertType::BUFFER_CRITICAL, 
                (uint32_t)bufferUsed, (uint32_t)bufferSize);
      bufferWarningActive = true;
    }
  } else if (bufferPercent > 50.0f) {
    if (!bufferWarningActive) {
      sendAlert(AlertLevel::WARNING, AlertType::BUFFER_WARNING,
                (uint32_t)bufferUsed, (uint32_t)bufferSize);
      bufferWarningActive = true;
      bufferWarningCount++;
    }
  } else if (bufferWarningActive && bufferPercent < 25.0f) {
    // Buffer recovered
    sendAlert(AlertLevel::INFO, AlertType::RECOVERED,
              (uint32_t)AlertType::BUFFER_WARNING, (uint32_t)bufferUsed);
    bufferWarningActive = false;
  }
  
  // --- Frame drop rate alerts (per second) ---
  if (now - lastFrameDropReset > 1000000) {  // Every second
    if (frameDropsThisSecond > 10) {
      sendAlert(AlertLevel::WARNING, AlertType::FRAME_DROP_SEVERE,
                frameDropsThisSecond, droppedFrames);
    } else if (frameDropsThisSecond > 0) {
      // Only send occasional frame drop alerts (every 5 drops)
      if ((droppedFrames % 5) == 0) {
        sendAlert(AlertLevel::INFO, AlertType::FRAME_DROP,
                  frameDropsThisSecond, droppedFrames);
      }
    }
    frameDropsThisSecond = 0;
    lastFrameDropReset = now;
  }
  
  // --- Heap memory alerts ---
  uint32_t freeHeap = esp_get_free_heap_size();
  if (freeHeap < 20000) {
    sendAlert(AlertLevel::CRITICAL, AlertType::HEAP_CRITICAL,
              freeHeap, esp_get_minimum_free_heap_size());
    heapWarningActive = true;
  } else if (freeHeap < 50000) {
    if (!heapWarningActive) {
      sendAlert(AlertLevel::WARNING, AlertType::HEAP_LOW,
                freeHeap, esp_get_minimum_free_heap_size());
      heapWarningActive = true;
    }
  } else if (heapWarningActive && freeHeap > 80000) {
    sendAlert(AlertLevel::INFO, AlertType::RECOVERED,
              (uint32_t)AlertType::HEAP_LOW, freeHeap);
    heapWarningActive = false;
  }
}

/**
 * Send buffer overflow alert (called when overflow detected)
 */
static void sendBufferOverflowAlert(size_t bytesLost) {
  bufferOverflowTotal++;
  sendAlert(AlertLevel::CRITICAL, AlertType::BUFFER_OVERFLOW,
            bytesLost, bufferOverflowTotal);
}

/**
 * Send parser error alert
 */
static void sendParserErrorAlert(uint8_t badByte, uint8_t state) {
  parserErrorCount++;
  sendAlert(AlertLevel::WARNING, AlertType::PARSER_ERROR,
            (badByte << 8) | state, parserErrorCount);
}

// Check if command type is a display command (affects display content)
static bool isDisplayCommand(CmdType type) {
  switch (type) {
    // System commands that don't affect display
    case CmdType::PING:
    case CmdType::PONG:
    case CmdType::REQUEST_CONFIG:
    case CmdType::CONFIG_RESPONSE:
    case CmdType::REQUEST_STATS:
    case CmdType::STATS_RESPONSE:
    case CmdType::ALERT:
    case CmdType::CLEAR_ALERT:
    case CmdType::REQUEST_ALERTS:
    case CmdType::ALERTS_RESPONSE:
    case CmdType::NOP:
      return false;
    // All other commands affect display content
    default:
      return true;
  }
}

static void processCommand(const CmdHeader* hdr, const uint8_t* payload) {
  // Track CPU connection for boot animation
  int64_t now = esp_timer_get_time();
  lastCpuCommandTime = now;
  if (!cpuConnected) {
    cpuConnected = true;
    ESP_LOGI(TAG, "CPU connected (received command 0x%02X)", (int)hdr->type);
  }
  
  // Track display commands separately for NO_SIGNAL timeout
  if (isDisplayCommand(hdr->type)) {
    lastDisplayCommandTime = now;
  }
  
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
          ESP_LOGI(TAG, "Sprite %d uploaded: %dx%d fmt=%d dataSize=%d", id, w, h, fmt, dataSize);
        }
      } else {
        ESP_LOGW(TAG, "Sprite upload rejected: id=%d w=%d h=%d fmt=%d len=%d need=%d", 
                 id, w, h, fmt, hdr->length, 4 + dataSize);
      }
      break;
    }
    
    case CmdType::DELETE_SPRITE: {
      if (hdr->length >= 1) {
        uint8_t id = payload[0];
        if (id < MAX_SPRITES) {
          // Clear sprite data to prevent glitchy residual images
          if (gpu.sprites[id].data != nullptr) {
            memset(gpu.sprites[id].data, 0, MAX_SPRITE_SIZE);
          }
          gpu.sprites[id].width = 0;
          gpu.sprites[id].height = 0;
          gpu.sprites[id].format = 0;
          gpu.sprites[id].valid = false;
          ESP_LOGI(TAG, "Sprite %d deleted and cleared", id);
        }
      }
      break;
    }
    
    case CmdType::CLEAR_ALL_SPRITES: {
      // Clear ALL sprite slots in one efficient command
      ESP_LOGI(TAG, "CLEAR_ALL_SPRITES - clearing all %d sprite slots", MAX_SPRITES);
      for (int i = 0; i < MAX_SPRITES; i++) {
        if (gpu.sprites[i].data != nullptr) {
          memset(gpu.sprites[i].data, 0, MAX_SPRITE_SIZE);
        }
        gpu.sprites[i].width = 0;
        gpu.sprites[i].height = 0;
        gpu.sprites[i].format = 0;
        gpu.sprites[i].valid = false;
      }
      ESP_LOGI(TAG, "All sprites cleared");
      break;
    }
    
    // ============ CHUNKED SPRITE UPLOAD PROTOCOL ============
    case CmdType::SPRITE_BEGIN: {
      // Begin chunked upload: id, w, h, fmt, totalSize_lo, totalSize_hi
      if (hdr->length >= 6) {
        uint8_t id = payload[0];
        uint8_t w = payload[1];
        uint8_t h = payload[2];
        uint8_t fmt = payload[3];
        uint16_t totalSize = payload[4] | (payload[5] << 8);
        
        if (id >= MAX_SPRITES || totalSize > MAX_SPRITE_SIZE) {
          ESP_LOGW(TAG, "SPRITE_BEGIN rejected: id=%d size=%d", id, totalSize);
          break;
        }
        
        // Allocate sprite buffer if needed
        if (gpu.sprites[id].data == nullptr) {
          gpu.sprites[id].data = (uint8_t*)heap_caps_malloc(MAX_SPRITE_SIZE, MALLOC_CAP_DEFAULT);
          if (!gpu.sprites[id].data) {
            ESP_LOGE(TAG, "SPRITE_BEGIN: malloc failed for sprite %d", id);
            break;
          }
        }
        
        // Also allocate chunked upload buffer if first time
        if (chunkedUpload.buffer == nullptr) {
          chunkedUpload.buffer = (uint8_t*)heap_caps_malloc(MAX_SPRITE_SIZE, MALLOC_CAP_DEFAULT);
          if (!chunkedUpload.buffer) {
            ESP_LOGE(TAG, "SPRITE_BEGIN: malloc failed for chunk buffer");
            break;
          }
        }
        
        // Initialize chunked upload state
        chunkedUpload.active = true;
        chunkedUpload.spriteId = id;
        chunkedUpload.width = w;
        chunkedUpload.height = h;
        chunkedUpload.format = fmt;
        chunkedUpload.totalSize = totalSize;
        chunkedUpload.receivedSize = 0;
        chunkedUpload.receivedChunks = 0;
        chunkedUpload.expectedChunks = (totalSize + 255) / 256;  // 256 bytes per chunk
        memset(chunkedUpload.buffer, 0, MAX_SPRITE_SIZE);
        
        ESP_LOGI(TAG, "SPRITE_BEGIN: id=%d %dx%d fmt=%d size=%d chunks=%d", 
                 id, w, h, fmt, totalSize, chunkedUpload.expectedChunks);
      }
      break;
    }
    
    case CmdType::SPRITE_CHUNK: {
      // Data chunk: id, chunkIdx_lo, chunkIdx_hi, data...
      if (hdr->length >= 3 && chunkedUpload.active) {
        uint8_t id = payload[0];
        uint16_t chunkIdx = payload[1] | (payload[2] << 8);
        uint16_t dataLen = hdr->length - 3;
        
        if (id != chunkedUpload.spriteId) {
          ESP_LOGW(TAG, "SPRITE_CHUNK: wrong sprite id=%d expected=%d", id, chunkedUpload.spriteId);
          break;
        }
        
        // Calculate offset and copy data
        uint32_t offset = chunkIdx * 256;
        if (offset + dataLen > MAX_SPRITE_SIZE) {
          ESP_LOGW(TAG, "SPRITE_CHUNK: overflow chunk=%d dataLen=%d", chunkIdx, dataLen);
          break;
        }
        
        memcpy(chunkedUpload.buffer + offset, payload + 3, dataLen);
        chunkedUpload.receivedSize += dataLen;
        chunkedUpload.receivedChunks++;
        
        // Log progress every 4 chunks
        if ((chunkedUpload.receivedChunks % 4) == 0 || chunkedUpload.receivedChunks == chunkedUpload.expectedChunks) {
          ESP_LOGI(TAG, "SPRITE_CHUNK: %d/%d chunks, %d/%d bytes", 
                   chunkedUpload.receivedChunks, chunkedUpload.expectedChunks,
                   chunkedUpload.receivedSize, chunkedUpload.totalSize);
        }
      }
      break;
    }
    
    case CmdType::SPRITE_END: {
      // Finalize upload: id, expectedChunks_lo, expectedChunks_hi
      if (hdr->length >= 3 && chunkedUpload.active) {
        uint8_t id = payload[0];
        uint16_t expectedChunks = payload[1] | (payload[2] << 8);
        
        if (id != chunkedUpload.spriteId) {
          ESP_LOGW(TAG, "SPRITE_END: wrong sprite id=%d expected=%d", id, chunkedUpload.spriteId);
          chunkedUpload.active = false;
          break;
        }
        
        if (chunkedUpload.receivedChunks < expectedChunks) {
          ESP_LOGW(TAG, "SPRITE_END: incomplete upload, got %d/%d chunks", 
                   chunkedUpload.receivedChunks, expectedChunks);
          // Still finalize with what we have
        }
        
        // Copy assembled data to sprite
        memcpy(gpu.sprites[id].data, chunkedUpload.buffer, chunkedUpload.totalSize);
        gpu.sprites[id].width = chunkedUpload.width;
        gpu.sprites[id].height = chunkedUpload.height;
        gpu.sprites[id].format = chunkedUpload.format;
        gpu.sprites[id].valid = true;
        
        ESP_LOGI(TAG, "SPRITE_END: sprite %d complete! %dx%d, %d bytes in %d chunks",
                 id, chunkedUpload.width, chunkedUpload.height, 
                 chunkedUpload.receivedSize, chunkedUpload.receivedChunks);
        
        chunkedUpload.active = false;
      }
      break;
    }
    // ============ END CHUNKED SPRITE UPLOAD ============

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
    
    case CmdType::DRAW_FILL_F: {
      if (hdr->length >= 11) {
        float x = (int8_t)payload[1] + (payload[0] / 256.0f);
        float y = (int8_t)payload[3] + (payload[2] / 256.0f);
        float w = (int8_t)payload[5] + (payload[4] / 256.0f);
        float h = (int8_t)payload[7] + (payload[6] / 256.0f);
        fillRectF(x, y, w, h, payload[8], payload[9], payload[10]);
      }
      break;
    }
    
    case CmdType::BLIT_SPRITE_F: {
      if (hdr->length >= 5) {
        uint8_t id = payload[0];
        float x = (int8_t)payload[2] + (payload[1] / 256.0f);
        float y = (int8_t)payload[4] + (payload[3] / 256.0f);
        blitSpriteF(id, x, y);
      }
      break;
    }
    
    case CmdType::BLIT_SPRITE_ROT: {
      if (hdr->length >= 7) {
        uint8_t id = payload[0];
        float x = (int8_t)payload[2] + (payload[1] / 256.0f);
        float y = (int8_t)payload[4] + (payload[3] / 256.0f);
        float angle = (int16_t)(payload[5] | (payload[6] << 8)) / 256.0f;  // 8.8 fixed point degrees
        
        // DEBUG: Log blit command reception (first few times only)
        static int blit_debug_count = 0;
        if (blit_debug_count < 5) {
          blit_debug_count++;
          ESP_LOGI(TAG, "BLIT_SPRITE_ROT: id=%d pos=(%.1f,%.1f) angle=%.1f valid=%d target=%d", 
                   id, x, y, angle, 
                   (id < MAX_SPRITES) ? gpu.sprites[id].valid : -1,
                   gpu.target);
        }
        
        blitSpriteRotated(id, x, y, angle);
      }
      break;
    }
    
    case CmdType::SET_AA: {
      if (hdr->length >= 1) {
        aa_enabled = (payload[0] != 0);
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
      // CRITICAL: Flush any buffered commands BEFORE processing present
      // This ensures we always display the LATEST frame, not stale data
      size_t buffered = 0;
      uart_get_buffered_data_len(UART_PORT, &buffered);
      if (buffered > 256) {  // If there's a significant backlog
        // Log and flush - we're behind, catch up by discarding old frames
        static uint32_t flushCount = 0;
        flushCount++;
        if ((flushCount % 10) == 1) {  // Log every 10th flush
          ESP_LOGW(TAG, "PRESENT: flushing %d bytes backlog (catch-up #%lu)", (int)buffered, flushCount);
        }
        uart_flush_input(UART_PORT);
      }
      
      // Frame rate limiter - drop frames if coming too fast
      int64_t now = esp_timer_get_time();
      int64_t elapsed = now - lastPresentTime;
      if (elapsed < MIN_PRESENT_INTERVAL_US) {
        // Too soon - skip this frame to let GPU catch up
        droppedFrames++;
        frameDropsThisSecond++;  // Track for alert system
        if ((droppedFrames % 100) == 0) {
          ESP_LOGW(TAG, "Frame rate limiting: dropped %lu frames (last interval: %lld us)",
                   droppedFrames, elapsed);
        }
        break;  // Skip this present
      }
      lastPresentTime = now;
      
      // DEBUG: Log PRESENT command (first few times only)
      static int present_debug_count = 0;
      if (present_debug_count < 5) {
        present_debug_count++;
        ESP_LOGI(TAG, "PRESENT: target=%d hub75_ok=%d frame=%lu", 
                 gpu.target, hub75_ok, gpu.frameCount);
      }
      
      if (gpu.target == 0 && hub75_ok) {
        // Use presentHUB75Buffer which applies panel transforms and RGB correction
        presentHUB75Buffer();
        dbg_hub75_presents.fetch_add(1, std::memory_order_relaxed);
        // Use release semantics so Core 0 sees this update
        dbg_last_hub75_present.store(now, std::memory_order_release);
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
        // Draw using Bresenham with orientation-aware setPixelOLED
        int dx = abs(x2 - x1);
        int dy = -abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx + dy;
        while (true) {
          setPixelOLED(x1, y1, on);
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
        // Draw rectangle outline using orientation-aware setPixelOLED
        int y2 = y + h - 1;
        int x2 = x + w - 1;
        // Top and bottom edges
        for (int px = x; px <= x2; px++) {
          setPixelOLED(px, y, on);
          setPixelOLED(px, y2, on);
        }
        // Left and right edges
        for (int py = y; py <= y2; py++) {
          setPixelOLED(x, py, on);
          setPixelOLED(x2, py, on);
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
        // Filled rectangle using orientation-aware setPixelOLED
        for (int py = y; py < y + h; py++) {
          for (int px = x; px < x + w; px++) {
            setPixelOLED(px, py, on);
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
        // Bresenham circle using orientation-aware setPixelOLED
        int x = r, y = 0, err = 0;
        while (x >= y) {
          setPixelOLED(cx + x, cy + y, on);
          setPixelOLED(cx - x, cy + y, on);
          setPixelOLED(cx + x, cy - y, on);
          setPixelOLED(cx - x, cy - y, on);
          setPixelOLED(cx + y, cy + x, on);
          setPixelOLED(cx - y, cy + x, on);
          setPixelOLED(cx + y, cy - x, on);
          setPixelOLED(cx - y, cy - x, on);
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
    
    case CmdType::OLED_PIXEL: {
      if (hdr->length >= 5) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        bool on = payload[4] > 0;
        setPixelOLED(x, y, on);
      }
      break;
    }
    
    case CmdType::OLED_VLINE: {
      // Optimized vertical line: x, y, length, on
      if (hdr->length >= 7) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t len = (int16_t)(payload[4] | (payload[5] << 8));
        bool on = payload[6] > 0;
        // Use orientation-aware setPixelOLED
        for (int py = y; py < y + len; py++) {
          setPixelOLED(x, py, on);
        }
      }
      break;
    }
    
    case CmdType::OLED_HLINE: {
      // Optimized horizontal line: x, y, length, on
      if (hdr->length >= 7) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t len = (int16_t)(payload[4] | (payload[5] << 8));
        bool on = payload[6] > 0;
        // Use orientation-aware setPixelOLED
        for (int px = x; px < x + len; px++) {
          setPixelOLED(px, y, on);
        }
      }
      break;
    }
    
    case CmdType::OLED_FILL_CIRCLE: {
      if (hdr->length >= 7) {
        int16_t cx = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t cy = (int16_t)(payload[2] | (payload[3] << 8));
        int16_t r = (int16_t)(payload[4] | (payload[5] << 8));
        bool on = payload[6] > 0;
        // Filled circle using scanlines with orientation-aware setPixelOLED
        for (int y = -r; y <= r; y++) {
          int py = cy + y;
          int dx = (int)sqrtf(r * r - y * y);
          for (int x = -dx; x <= dx; x++) {
            int px = cx + x;
            setPixelOLED(px, py, on);
          }
        }
      }
      break;
    }
    
    case CmdType::OLED_SET_ORIENTATION: {
      if (hdr->length >= 1) {
        int mode = payload[0];
        if (mode >= 0 && mode <= 7) {
          oled_orientation = mode;
          ESP_LOGI(TAG, "OLED orientation set to mode %d", mode);
        }
      }
      break;
    }
    
    case CmdType::OLED_TEXT: {
      // Native text rendering: x(2), y(2), scale(1), on(1), text(N)
      // 5x7 font (same as CPU side)
      static const uint8_t FONT_5X7[95][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, // (space)
        {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
        {0x00, 0x07, 0x00, 0x07, 0x00}, // "
        {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
        {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
        {0x23, 0x13, 0x08, 0x64, 0x62}, // %
        {0x36, 0x49, 0x55, 0x22, 0x50}, // &
        {0x00, 0x05, 0x03, 0x00, 0x00}, // '
        {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
        {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
        {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // *
        {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
        {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
        {0x08, 0x08, 0x08, 0x08, 0x08}, // -
        {0x00, 0x60, 0x60, 0x00, 0x00}, // .
        {0x20, 0x10, 0x08, 0x04, 0x02}, // /
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
        {0x00, 0x36, 0x36, 0x00, 0x00}, // :
        {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
        {0x00, 0x08, 0x14, 0x22, 0x41}, // <
        {0x14, 0x14, 0x14, 0x14, 0x14}, // =
        {0x41, 0x22, 0x14, 0x08, 0x00}, // >
        {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
        {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
        {0x7F, 0x09, 0x09, 0x01, 0x01}, // F
        {0x3E, 0x41, 0x41, 0x51, 0x32}, // G
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
        {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
        {0x46, 0x49, 0x49, 0x49, 0x31}, // S
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
        {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
        {0x63, 0x14, 0x08, 0x14, 0x63}, // X
        {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
        {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
        {0x00, 0x00, 0x7F, 0x41, 0x41}, // [
        {0x02, 0x04, 0x08, 0x10, 0x20}, // (backslash)
        {0x41, 0x41, 0x7F, 0x00, 0x00}, // ]
        {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
        {0x40, 0x40, 0x40, 0x40, 0x40}, // _
        {0x00, 0x01, 0x02, 0x04, 0x00}, // `
        {0x20, 0x54, 0x54, 0x54, 0x78}, // a
        {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
        {0x38, 0x44, 0x44, 0x44, 0x20}, // c
        {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
        {0x38, 0x54, 0x54, 0x54, 0x18}, // e
        {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
        {0x08, 0x14, 0x54, 0x54, 0x3C}, // g
        {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
        {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
        {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
        {0x00, 0x7F, 0x10, 0x28, 0x44}, // k
        {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
        {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
        {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
        {0x38, 0x44, 0x44, 0x44, 0x38}, // o
        {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
        {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
        {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
        {0x48, 0x54, 0x54, 0x54, 0x20}, // s
        {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
        {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
        {0x44, 0x28, 0x10, 0x28, 0x44}, // x
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
        {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
        {0x00, 0x08, 0x36, 0x41, 0x00}, // {
        {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
        {0x00, 0x41, 0x36, 0x08, 0x00}, // }
        {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
      };
      
      if (hdr->length >= 6) {
        int16_t x = (int16_t)(payload[0] | (payload[1] << 8));
        int16_t y = (int16_t)(payload[2] | (payload[3] << 8));
        int scale = payload[4];
        bool on = payload[5] > 0;
        const char* text = (const char*)&payload[6];
        int textLen = hdr->length - 6;
        
        int cursorX = x;
        for (int i = 0; i < textLen && text[i] != '\0'; i++) {
          char c = text[i];
          if (c < 32 || c > 126) c = '?';
          int idx = c - 32;
          
          // Draw character column by column
          for (int col = 0; col < 5; col++) {
            uint8_t colData = FONT_5X7[idx][col];
            for (int row = 0; row < 7; row++) {
              if (colData & (1 << row)) {
                if (scale == 1) {
                  setPixelOLED(cursorX + col, y + row, on);
                } else {
                  // Scaled pixel
                  for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                      setPixelOLED(cursorX + col * scale + sx, y + row * scale + sy, on);
                    }
                  }
                }
              }
            }
          }
          cursorX += 6 * scale;  // 5 pixel char + 1 pixel spacing
        }
      }
      break;
    }
    
    case CmdType::OLED_MIRROR_HUB75: {
      // HUB75 to OLED mirroring using luminance threshold
      // Payload: threshold(1), scaleMode(1), yOffset(1)
      // threshold: 0-255 luminance cutoff (default 128)
      // scaleMode: 0=1:1 (32 rows centered), 1=4x vertical scale (fills 128 rows)
      // yOffset: Y offset for 1:1 mode (default 48 to center 32 in 128)
      
      uint8_t threshold = 128;
      uint8_t scaleMode = 1;  // Default to 4x scale
      uint8_t yOffset = 48;   // Center for 1:1 mode
      
      if (hdr->length >= 1) threshold = payload[0];
      if (hdr->length >= 2) scaleMode = payload[1];
      if (hdr->length >= 3) yOffset = payload[2];
      
      if (scaleMode == 0) {
        // 1:1 mode - direct copy with Y offset
        // HUB75 is 128x32, OLED is 128x128
        // Clear only the 32-pixel region we're writing to
        for (int y = 0; y < TOTAL_HEIGHT; y++) {
          int oledY = y + yOffset;
          if (oledY < 0 || oledY >= OLED_HEIGHT) continue;
          int byteIdx = (oledY / 8) * OLED_WIDTH;
          int bitIdx = oledY % 8;
          uint8_t clearMask = ~(1 << bitIdx);
          for (int x = 0; x < TOTAL_WIDTH; x++) {
            oled_buffer[byteIdx + x] &= clearMask;
          }
        }
        
        // Direct copy from HUB75 buffer to OLED
        for (int y = 0; y < TOTAL_HEIGHT; y++) {
          int oledY = y + yOffset;
          if (oledY < 0 || oledY >= OLED_HEIGHT) continue;
          
          for (int x = 0; x < TOTAL_WIDTH; x++) {
            // Read buffer directly
            int idx = (y * TOTAL_WIDTH + x) * 3;
            uint8_t r = hub75_buffer[idx + 0];
            uint8_t g = hub75_buffer[idx + 1];
            uint8_t b = hub75_buffer[idx + 2];
            
            // Fast luminance: (77*R + 150*G + 29*B) >> 8
            uint16_t lum = (77 * r + 150 * g + 29 * b) >> 8;
            
            if (lum >= threshold) {
              int byteIdx = (oledY / 8) * OLED_WIDTH + x;
              int bitIdx = oledY % 8;
              oled_buffer[byteIdx] |= (1 << bitIdx);
            }
          }
        }
      } else {
        // 4x vertical scale mode - fills 128x128 from 128x32
        // Each HUB75 row becomes 4 OLED rows
        // Clear entire OLED buffer since we fill it completely
        memset(oled_buffer, 0, OLED_BUFFER_SIZE);
        
        for (int y = 0; y < TOTAL_HEIGHT; y++) {
          int baseY = y * 4;
          
          for (int x = 0; x < TOTAL_WIDTH; x++) {
            // Direct buffer read
            int idx = (y * TOTAL_WIDTH + x) * 3;
            uint8_t r = hub75_buffer[idx + 0];
            uint8_t g = hub75_buffer[idx + 1];
            uint8_t b = hub75_buffer[idx + 2];
            
            // Fast luminance calculation
            uint16_t lum = (77 * r + 150 * g + 29 * b) >> 8;
            
            if (lum >= threshold) {
              // Scale 4x vertically - set 4 consecutive rows
              for (int sy = 0; sy < 4; sy++) {
                int oledY = baseY + sy;
                int byteIdx = (oledY / 8) * OLED_WIDTH + x;
                int bitIdx = oledY % 8;
                oled_buffer[byteIdx] |= (1 << bitIdx);
              }
            }
          }
        }
      }
      break;
    }
    
    case CmdType::PING: {
      ESP_LOGI(TAG, "PING received - sending PONG with uptime");
      // Calculate uptime in milliseconds
      uint32_t uptime_ms = (esp_timer_get_time() - gpu.startTime) / 1000;
      
      // Build PONG response: [0xAA][0x55][PONG=0xF1][len=4][uptime_ms:4]
      uint8_t response[9];
      response[0] = 0xAA;  // Sync byte 1
      response[1] = 0x55;  // Sync byte 2
      response[2] = static_cast<uint8_t>(CmdType::PONG);  // Command type
      response[3] = 4;     // Payload length low byte
      response[4] = 0;     // Payload length high byte
      // Uptime in little-endian
      response[5] = (uptime_ms >>  0) & 0xFF;
      response[6] = (uptime_ms >>  8) & 0xFF;
      response[7] = (uptime_ms >> 16) & 0xFF;
      response[8] = (uptime_ms >> 24) & 0xFF;
      
      uart_write_bytes(UART_PORT, response, sizeof(response));
      uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(50));  // Ensure response is fully sent
      ESP_LOGI(TAG, "PONG sent: uptime=%lu ms", (unsigned long)uptime_ms);
      break;
    }
    
    case CmdType::REQUEST_CONFIG: {
      ESP_LOGI(TAG, "REQUEST_CONFIG received - sending GPU configuration");
      
      // Calculate uptime
      uint32_t uptime_ms = (esp_timer_get_time() - gpu.startTime) / 1000;
      
      // Build CONFIG_RESPONSE
      // Payload structure (32 bytes):
      //   [0]     panel_count (2: HUB75 + OLED)
      //   [1]     panel1_type (0=HUB75_RGB)
      //   [2-3]   panel1_width (128)
      //   [4-5]   panel1_height (32)
      //   [6]     panel1_bit_depth (24 for RGB888)
      //   [7]     panel2_type (1=OLED_MONO)
      //   [8-9]   panel2_width (128)
      //   [10-11] panel2_height (128)
      //   [12]    panel2_bit_depth (1 for monochrome)
      //   [13-16] total_uptime_ms (uint32_t)
      //   [17-20] max_data_rate_bps (uint32_t, 10000000)
      //   [21-22] command_version (uint16_t, e.g. 0x0100 = v1.0)
      //   [23]    hub75_ok flag
      //   [24]    oled_ok flag
      //   [25-31] reserved (zeros)
      
      uint8_t payload[32];
      memset(payload, 0, sizeof(payload));
      
      // Panel count
      payload[0] = 2;
      
      // Panel 1: HUB75 RGB
      payload[1] = 0;  // Type: HUB75_RGB
      payload[2] = TOTAL_WIDTH & 0xFF;   // Width low
      payload[3] = (TOTAL_WIDTH >> 8) & 0xFF;  // Width high
      payload[4] = TOTAL_HEIGHT & 0xFF;  // Height low
      payload[5] = (TOTAL_HEIGHT >> 8) & 0xFF;  // Height high
      payload[6] = 24;  // Bit depth (RGB888)
      
      // Panel 2: OLED Monochrome
      payload[7] = 1;  // Type: OLED_MONO
      payload[8] = OLED_WIDTH & 0xFF;   // Width low
      payload[9] = (OLED_WIDTH >> 8) & 0xFF;  // Width high
      payload[10] = OLED_HEIGHT & 0xFF;  // Height low
      payload[11] = (OLED_HEIGHT >> 8) & 0xFF;  // Height high
      payload[12] = 1;  // Bit depth (monochrome)
      
      // Total uptime (little-endian)
      payload[13] = (uptime_ms >>  0) & 0xFF;
      payload[14] = (uptime_ms >>  8) & 0xFF;
      payload[15] = (uptime_ms >> 16) & 0xFF;
      payload[16] = (uptime_ms >> 24) & 0xFF;
      
      // Max data rate: 10,000,000 bps (little-endian)
      uint32_t max_rate = UART_BAUD;
      payload[17] = (max_rate >>  0) & 0xFF;
      payload[18] = (max_rate >>  8) & 0xFF;
      payload[19] = (max_rate >> 16) & 0xFF;
      payload[20] = (max_rate >> 24) & 0xFF;
      
      // Command version: 1.0 = 0x0100
      payload[21] = 0x00;  // Minor version
      payload[22] = 0x01;  // Major version
      
      // Hardware status flags
      payload[23] = hub75_ok ? 1 : 0;
      payload[24] = oled_ok ? 1 : 0;
      
      // Build response header: [0xAA][0x55][CONFIG_RESPONSE=0xF3][len=32]
      uint8_t header[5];
      header[0] = 0xAA;
      header[1] = 0x55;
      header[2] = static_cast<uint8_t>(CmdType::CONFIG_RESPONSE);
      header[3] = sizeof(payload) & 0xFF;
      header[4] = (sizeof(payload) >> 8) & 0xFF;
      
      uart_write_bytes(UART_PORT, header, sizeof(header));
      uart_write_bytes(UART_PORT, payload, sizeof(payload));
      
      ESP_LOGI(TAG, "CONFIG_RESPONSE sent: panels=%d, uptime=%lu ms, baud=%d",
               payload[0], (unsigned long)uptime_ms, UART_BAUD);
      break;
    }
    
    case CmdType::REQUEST_STATS: {
      ESP_LOGI(TAG, "REQUEST_STATS received - sending GPU performance stats");
      
      // Calculate uptime
      uint32_t uptime_ms = (esp_timer_get_time() - gpu.startTime) / 1000;
      
      // Update heap stats now for freshest data
      current_free_heap = esp_get_free_heap_size();
      current_min_heap = esp_get_minimum_free_heap_size();
      
      // Build STATS_RESPONSE
      // Payload structure (24 bytes):
      //   [0-3]   fps_x100 (uint32_t) - FPS * 100 for 2 decimal precision
      //   [4-7]   free_heap (uint32_t) - Free heap bytes
      //   [8-11]  min_heap (uint32_t) - Minimum free heap since boot
      //   [12]    gpu_load (uint8_t) - Estimated GPU load 0-100%
      //   [13-16] total_frames (uint32_t) - Total frames since boot
      //   [17-20] uptime_ms (uint32_t) - Uptime in milliseconds
      //   [21]    hub75_ok (uint8_t) - HUB75 status
      //   [22]    oled_ok (uint8_t) - OLED status
      //   [23]    reserved
      
      uint8_t payload[24];
      memset(payload, 0, sizeof(payload));
      
      // FPS * 100 (little-endian)
      uint32_t fps_x100 = (uint32_t)(current_fps * 100.0f);
      payload[0] = (fps_x100 >>  0) & 0xFF;
      payload[1] = (fps_x100 >>  8) & 0xFF;
      payload[2] = (fps_x100 >> 16) & 0xFF;
      payload[3] = (fps_x100 >> 24) & 0xFF;
      
      // Free heap (little-endian)
      payload[4] = (current_free_heap >>  0) & 0xFF;
      payload[5] = (current_free_heap >>  8) & 0xFF;
      payload[6] = (current_free_heap >> 16) & 0xFF;
      payload[7] = (current_free_heap >> 24) & 0xFF;
      
      // Min heap (little-endian)
      payload[8]  = (current_min_heap >>  0) & 0xFF;
      payload[9]  = (current_min_heap >>  8) & 0xFF;
      payload[10] = (current_min_heap >> 16) & 0xFF;
      payload[11] = (current_min_heap >> 24) & 0xFF;
      
      // GPU load estimate
      payload[12] = gpu_load_percent;
      
      // Total frames (little-endian)
      payload[13] = (total_frames >>  0) & 0xFF;
      payload[14] = (total_frames >>  8) & 0xFF;
      payload[15] = (total_frames >> 16) & 0xFF;
      payload[16] = (total_frames >> 24) & 0xFF;
      
      // Uptime (little-endian)
      payload[17] = (uptime_ms >>  0) & 0xFF;
      payload[18] = (uptime_ms >>  8) & 0xFF;
      payload[19] = (uptime_ms >> 16) & 0xFF;
      payload[20] = (uptime_ms >> 24) & 0xFF;
      
      // Hardware status
      payload[21] = hub75_ok ? 1 : 0;
      payload[22] = oled_ok ? 1 : 0;
      
      // Build response header
      uint8_t header[5];
      header[0] = 0xAA;
      header[1] = 0x55;
      header[2] = static_cast<uint8_t>(CmdType::STATS_RESPONSE);
      header[3] = sizeof(payload) & 0xFF;
      header[4] = (sizeof(payload) >> 8) & 0xFF;
      
      uart_write_bytes(UART_PORT, header, sizeof(header));
      uart_write_bytes(UART_PORT, payload, sizeof(payload));
      
      ESP_LOGI(TAG, "STATS_RESPONSE sent: fps=%.2f, heap=%lu/%lu, load=%d%%, frames=%lu",
               current_fps, (unsigned long)current_free_heap, (unsigned long)current_min_heap,
               gpu_load_percent, (unsigned long)total_frames);
      break;
    }
    
    case CmdType::REQUEST_ALERTS: {
      ESP_LOGI(TAG, "REQUEST_ALERTS received - sending alert status");
      
      // Build ALERTS_RESPONSE
      // Payload structure (32 bytes):
      //   [0-3]   total_alerts_sent (uint32_t)
      //   [4-7]   dropped_frames (uint32_t)
      //   [8-11]  buffer_warnings (uint32_t)
      //   [12-15] buffer_overflows (uint32_t)
      //   [16-19] parser_errors (uint32_t)
      //   [20]    buffer_warning_active (bool)
      //   [21]    heap_warning_active (bool)
      //   [22]    last_alert_type (AlertType)
      //   [23]    current_buffer_percent (0-100)
      //   [24-27] free_heap (uint32_t)
      //   [28-31] frames_dropped_this_sec (uint32_t)
      
      uint8_t payload[32];
      memset(payload, 0, sizeof(payload));
      
      // Total alerts sent
      payload[0] = (alertsSent >>  0) & 0xFF;
      payload[1] = (alertsSent >>  8) & 0xFF;
      payload[2] = (alertsSent >> 16) & 0xFF;
      payload[3] = (alertsSent >> 24) & 0xFF;
      
      // Dropped frames
      payload[4] = (droppedFrames >>  0) & 0xFF;
      payload[5] = (droppedFrames >>  8) & 0xFF;
      payload[6] = (droppedFrames >> 16) & 0xFF;
      payload[7] = (droppedFrames >> 24) & 0xFF;
      
      // Buffer warnings
      payload[8]  = (bufferWarningCount >>  0) & 0xFF;
      payload[9]  = (bufferWarningCount >>  8) & 0xFF;
      payload[10] = (bufferWarningCount >> 16) & 0xFF;
      payload[11] = (bufferWarningCount >> 24) & 0xFF;
      
      // Buffer overflows
      payload[12] = (bufferOverflowTotal >>  0) & 0xFF;
      payload[13] = (bufferOverflowTotal >>  8) & 0xFF;
      payload[14] = (bufferOverflowTotal >> 16) & 0xFF;
      payload[15] = (bufferOverflowTotal >> 24) & 0xFF;
      
      // Parser errors
      payload[16] = (parserErrorCount >>  0) & 0xFF;
      payload[17] = (parserErrorCount >>  8) & 0xFF;
      payload[18] = (parserErrorCount >> 16) & 0xFF;
      payload[19] = (parserErrorCount >> 24) & 0xFF;
      
      // Status flags
      payload[20] = bufferWarningActive ? 1 : 0;
      payload[21] = heapWarningActive ? 1 : 0;
      payload[22] = static_cast<uint8_t>(lastAlertType);
      
      // Current buffer percent
      size_t buffered = 0;
      uart_get_buffered_data_len(UART_PORT, &buffered);
      payload[23] = (uint8_t)((buffered * 100) / 16384);
      
      // Free heap
      uint32_t freeHeap = esp_get_free_heap_size();
      payload[24] = (freeHeap >>  0) & 0xFF;
      payload[25] = (freeHeap >>  8) & 0xFF;
      payload[26] = (freeHeap >> 16) & 0xFF;
      payload[27] = (freeHeap >> 24) & 0xFF;
      
      // Frames dropped this second
      payload[28] = (frameDropsThisSecond >>  0) & 0xFF;
      payload[29] = (frameDropsThisSecond >>  8) & 0xFF;
      payload[30] = (frameDropsThisSecond >> 16) & 0xFF;
      payload[31] = (frameDropsThisSecond >> 24) & 0xFF;
      
      // Build response header
      uint8_t header[5];
      header[0] = 0xAA;
      header[1] = 0x55;
      header[2] = static_cast<uint8_t>(CmdType::ALERTS_RESPONSE);
      header[3] = sizeof(payload) & 0xFF;
      header[4] = (sizeof(payload) >> 8) & 0xFF;
      
      uart_write_bytes(UART_PORT, header, sizeof(header));
      uart_write_bytes(UART_PORT, payload, sizeof(payload));
      
      ESP_LOGI(TAG, "ALERTS_RESPONSE sent: alerts=%lu, drops=%lu, overflows=%lu",
               alertsSent, droppedFrames, bufferOverflowTotal);
      break;
    }
    
    case CmdType::RESET: {
      ESP_LOGI(TAG, "RESET received - clearing all GPU state");
      
      // Clear all shaders
      for (int i = 0; i < MAX_SHADERS; i++) {
        gpu.shaders[i].valid = false;
        gpu.shaders[i].length = 0;
        memset(gpu.shaders[i].bytecode, 0, MAX_SHADER_SIZE);
      }
      
      // Clear all sprites - zero out data buffers to prevent glitchy images
      for (int i = 0; i < MAX_SPRITES; i++) {
        if (gpu.sprites[i].data != nullptr) {
          memset(gpu.sprites[i].data, 0, MAX_SPRITE_SIZE);
        }
        gpu.sprites[i].width = 0;
        gpu.sprites[i].height = 0;
        gpu.sprites[i].format = 0;
        gpu.sprites[i].valid = false;
      }
      
      // Clear variables
      memset(gpu.variables, 0, sizeof(gpu.variables));
      
      // Clear both framebuffers completely
      memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
      memset(oled_buffer, 0, OLED_BUFFER_SIZE);
      
      // Reset state
      gpu.target = 0;
      gpu.frameCount = 0;
      gpu.randSeed = 0;
      gpu.loopSP = 0;
      
      ESP_LOGI(TAG, "GPU RESET complete - all caches cleared");
      break;
    }
    
    default:
      break;
  }
}

// ============================================================
// UART Receive Task
// ============================================================
static uint32_t totalBytesReceived = 0;
static int64_t lastRxLogTime = 0;

static void uartTask(void* arg) {
  uint8_t rx_buffer[256];  // Bulk read buffer
  int state = 0;  // 0=sync0, 1=sync1, 2=type, 3=len_lo, 4=len_hi, 5=payload
  CmdHeader hdr;
  int payload_pos = 0;
  int64_t lastByteTime = esp_timer_get_time();
  int64_t lastBufferCheck = esp_timer_get_time();
  uint32_t overflowCount = 0;
  
  ESP_LOGI(TAG, "UART RX task started on UART%d (RX=%d, TX=%d, baud=%d)", 
           UART_PORT, UART_RX_PIN, UART_TX_PIN, UART_BAUD);
  
  while (true) {
    // Periodic buffer overflow check (every 500ms)
    int64_t now = esp_timer_get_time();
    if (now - lastBufferCheck > 500000) {
      lastBufferCheck = now;
      size_t buffered = 0;
      uart_get_buffered_data_len(UART_PORT, &buffered);
      
      // Log RX stats every 5 seconds
      if (now - lastRxLogTime > 5000000) {
        lastRxLogTime = now;
        ESP_LOGI(TAG, "UART RX: total=%lu bytes, buffered=%d, alerts=%lu", 
                 totalBytesReceived, (int)buffered, alertsSent);
      }
      
      // Check and send alerts for buffer levels, heap, etc.
      checkAndSendAlerts(buffered, 16384);  // 16KB buffer
      
      // If buffer is getting full (>75%), flush it to recover
      if (buffered > 12288) {  // 75% of 16KB
        overflowCount++;
        ESP_LOGW(TAG, "UART RX buffer overflow detected (%d bytes), flushing... (count: %lu)", 
                 (int)buffered, overflowCount);
        sendBufferOverflowAlert(buffered);  // Alert CPU before flushing
        uart_flush_input(UART_PORT);
        state = 0;  // Reset parser state
        continue;
      }
    }
    
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
    
    totalBytesReceived += len;
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
          } else if (hdr.length > 300) {  // Max chunk size ~256 + header
            // Likely corrupt - flush UART and resync  
            ESP_LOGW(TAG, "Rejecting oversized command: type=0x%02X len=%d", (uint8_t)hdr.type, hdr.length);
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
      
      // Log only every 30th update to reduce overhead
      if ((oled_update_num % 30) == 0) {
        int64_t now = esp_timer_get_time();
        int64_t lastHUB75 = dbg_last_hub75_present.load(std::memory_order_acquire);
        sinceHUB75 = (now - lastHUB75) / 1000;
        ESP_LOGI(TAG, "OLED #%lu: since_hub75=%lldms, retries=%d", 
                 (unsigned long)oled_update_num, sinceHUB75, retries);
      }
      
      // Give HUB75 DMA time to recover after I2C burst (reduced from 20ms)
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms (~30Hz potential)
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
  // OLED is mounted upside down - use default (both true)
  oled_cfg.flip_horizontal = true;
  oled_cfg.flip_vertical = true;
  
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
  uart_driver_install(UART_PORT, 16384, 2048, 0, nullptr, 0);  // 16KB RX buffer for high throughput
  
  ESP_LOGI(TAG, "UART OK: %d baud, RX=%d, TX=%d, RX_BUF=16KB", UART_BAUD, UART_RX_PIN, UART_TX_PIN);
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
  
  // Initialize boot animation timing
  bootStartTime = esp_timer_get_time();
  bootState = BootState::FADE_IN;
  cpuConnected = false;
  lastCpuCommandTime = 0;
  lastDisplayCommandTime = 0;
  
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
  
  // Run panel diagnostic test if enabled
  if (RUN_PANEL_TEST && hub75_ok) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "*** PANEL ANIMATION TEST ENABLED ***");
    ESP_LOGI(TAG, "Square moving from left to right...");
    ESP_LOGI(TAG, "");
    
    const int SQUARE_SIZE = 10;
    const int SPEED = 2;  // pixels per frame
    int squareX = 0;
    int squareY = (TOTAL_HEIGHT - SQUARE_SIZE) / 2;  // Center vertically
    
    while (RUN_PANEL_TEST) {
      // Clear buffer
      memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
      
      // Draw moving square (cyan color)
      for (int y = squareY; y < squareY + SQUARE_SIZE; y++) {
        for (int x = squareX; x < squareX + SQUARE_SIZE; x++) {
          if (x >= 0 && x < TOTAL_WIDTH) {
            setDiagPixel(x, y, 0, 255, 255);  // CYAN
          }
        }
      }
      
      // Draw panel divider (gray vertical line)
      for (int y = 0; y < TOTAL_HEIGHT; y++) {
        setDiagPixel(63, y, 64, 64, 64);
        setDiagPixel(64, y, 64, 64, 64);
      }
      
      // Present to display
      presentHUB75Buffer();
      
      // Move square
      squareX += SPEED;
      
      // Wrap around when off the right edge
      if (squareX >= TOTAL_WIDTH) {
        squareX = -SQUARE_SIZE;  // Start from off-screen left
      }
      
      vTaskDelay(pdMS_TO_TICKS(30));  // ~33 FPS
    }
  }
  
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
  ESP_LOGI(TAG, "Starting boot animation...");
  ESP_LOGI(TAG, "");
  
  // Main loop - boot animation + status updates
  uint32_t lastStatus = 0;
  uint32_t lastFrameCount = 0;
  uint32_t lastOledUpdates = 0;
  uint32_t lastHub75Presents = 0;
  uint32_t lastOledPresents = 0;
  
  while (true) {
    // Run boot animation / no-signal animation if active
    if (updateBootAnimation()) {
      vTaskDelay(pdMS_TO_TICKS(30));  // ~30 FPS for boot animation
      continue;
    }
    
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
      
      // Update global stats for REQUEST_STATS responses
      current_fps = fps;
      current_free_heap = freeHeap;
      current_min_heap = minFreeHeap;
      total_frames = gpu.frameCount;
      // Estimate GPU load based on FPS vs target (60 FPS = fully utilized)
      gpu_load_percent = (fps > 0) ? (uint8_t)((fps / 60.0f) * 100.0f) : 0;
      if (gpu_load_percent > 100) gpu_load_percent = 100;
      
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
