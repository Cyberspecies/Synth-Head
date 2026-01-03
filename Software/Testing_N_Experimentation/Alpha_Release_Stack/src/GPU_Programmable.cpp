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

// ============================================================
// Pixel Operations
// ============================================================
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
// Drawing Primitives
// ============================================================
static void drawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
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

static void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  drawLine(x, y, x + w - 1, y, r, g, b);
  drawLine(x + w - 1, y, x + w - 1, y + h - 1, r, g, b);
  drawLine(x + w - 1, y + h - 1, x, y + h - 1, r, g, b);
  drawLine(x, y + h - 1, x, y, r, g, b);
}

static void fillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  for (int py = y; py < y + h; py++) {
    for (int px = x; px < x + w; px++) {
      setPixel(px, py, r, g, b);
    }
  }
}

static void drawCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) {
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

static void blitSprite(int id, int dx, int dy) {
  if (id < 0 || id >= MAX_SPRITES || !gpu.sprites[id].valid) return;
  
  Sprite& s = gpu.sprites[id];
  
  if (s.format == 0 && gpu.target == 0) {
    // RGB sprite to HUB75
    for (int y = 0; y < s.height; y++) {
      for (int x = 0; x < s.width; x++) {
        int idx = (y * s.width + x) * 3;
        setPixelHUB75(dx + x, dy + y, s.data[idx], s.data[idx + 1], s.data[idx + 2]);
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
  
  SET_TARGET = 0x50,     // 0=HUB75, 1=OLED
  PRESENT = 0x51,        // Push framebuffer to display
  
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
      } else if (gpu.target == 1 && oled_ok) {
        memcpy(oled->getBuffer(), oled_buffer, OLED_BUFFER_SIZE);
        oled->updateDisplay();
      }
      gpu.frameCount++;
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
  uint8_t byte;
  int state = 0;  // 0=sync0, 1=sync1, 2=type, 3=len_lo, 4=len_hi, 5=payload
  CmdHeader hdr;
  int payload_pos = 0;
  
  ESP_LOGI(TAG, "UART RX task started");
  
  while (true) {
    int len = uart_read_bytes(UART_PORT, &byte, 1, pdMS_TO_TICKS(10));
    if (len <= 0) continue;
    
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
        state = 3;
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
        } else if (hdr.length > sizeof(cmd_buffer)) {
          ESP_LOGW(TAG, "Payload too large: %d", hdr.length);
          state = 0;  // Too large, reset
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
  uart_driver_install(UART_PORT, 4096, 1024, 0, nullptr, 0);
  
  ESP_LOGI(TAG, "UART OK: %d baud, RX=%d, TX=%d", UART_BAUD, UART_RX_PIN, UART_TX_PIN);
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
  
  if (!hub75_buffer || !oled_buffer) {
    ESP_LOGE(TAG, "Failed to allocate framebuffers!");
    return;
  }
  
  memset(hub75_buffer, 0, HUB75_BUFFER_SIZE);
  memset(oled_buffer, 0, OLED_BUFFER_SIZE);
  
  ESP_LOGI(TAG, "Framebuffers: HUB75=%d bytes, OLED=%d bytes", 
           HUB75_BUFFER_SIZE, OLED_BUFFER_SIZE);
  
  // Initialize hardware
  hub75_ok = initHUB75();
  oled_ok = initOLED();
  initUART();
  
  // Start UART receive task
  xTaskCreatePinnedToCore(uartTask, "uart_rx", 4096, nullptr, 5, nullptr, 1);
  
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
  
  while (true) {
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (now - lastStatus >= 2000) {
      uint32_t frames = gpu.frameCount - lastFrameCount;
      float fps = frames * 1000.0f / (now - lastStatus);
      
      int validShaders = 0, validSprites = 0;
      for (int i = 0; i < MAX_SHADERS; i++) if (gpu.shaders[i].valid) validShaders++;
      for (int i = 0; i < MAX_SPRITES; i++) if (gpu.sprites[i].valid) validSprites++;
      
      ESP_LOGI(TAG, "FPS: %.1f | Shaders: %d | Sprites: %d | Vars in use: checking...",
               fps, validShaders, validSprites);
      
      lastStatus = now;
      lastFrameCount = gpu.frameCount;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
