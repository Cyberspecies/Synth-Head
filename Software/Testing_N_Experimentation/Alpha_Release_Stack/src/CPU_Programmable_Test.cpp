/*****************************************************************
 * CPU_Programmable_Test.cpp - Test Program for Programmable GPU
 * 
 * Uploads shader bytecode to the GPU and executes it to display:
 * - Animated shapes (hexagons, triangles, rectangles)
 * - Color cycling effects
 * - Bouncing patterns
 * 
 * Protocol:
 *   [SYNC0:0xAA][SYNC1:0x55][CmdType:1][Length:2][Payload:N]
 *****************************************************************/

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

static const char* TAG = "CPU_PROG_TEST";

// ============================================================
// UART Configuration
// ============================================================
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int UART_TX_PIN = 12;  // CPU TX -> GPU RX (GPIO 13)
constexpr int UART_RX_PIN = 11;  // CPU RX <- GPU TX (GPIO 12)
constexpr int UART_BAUD = 10000000;  // 10 Mbps

// ============================================================
// Command Protocol
// ============================================================
constexpr uint8_t SYNC0 = 0xAA;
constexpr uint8_t SYNC1 = 0x55;

enum class CmdType : uint8_t {
  NOP = 0x00,
  UPLOAD_SHADER = 0x10,
  DELETE_SHADER = 0x11,
  EXEC_SHADER = 0x12,
  
  UPLOAD_SPRITE = 0x20,
  DELETE_SPRITE = 0x21,
  
  SET_VAR = 0x30,
  SET_VARS = 0x31,
  
  DRAW_PIXEL = 0x40,
  DRAW_LINE = 0x41,
  DRAW_RECT = 0x42,
  DRAW_FILL = 0x43,
  DRAW_CIRCLE = 0x44,
  DRAW_POLY = 0x45,
  BLIT_SPRITE = 0x46,
  CLEAR = 0x47,
  
  SET_TARGET = 0x50,
  PRESENT = 0x51,
  
  PING = 0xF0,
  RESET = 0xFF,
};

// ============================================================
// Shader Opcodes (must match GPU_Programmable.cpp)
// ============================================================
enum class Op : uint8_t {
  NOP = 0x00,
  HALT = 0x01,
  
  SET = 0x10,
  MOV = 0x11,
  LOAD = 0x12,
  STORE = 0x13,
  
  ADD = 0x20,
  SUB = 0x21,
  MUL = 0x22,
  DIV = 0x23,
  MOD = 0x24,
  NEG = 0x25,
  ABS = 0x26,
  MIN = 0x27,
  MAX = 0x28,
  
  AND = 0x30,
  OR = 0x31,
  XOR = 0x32,
  NOT = 0x33,
  SHL = 0x34,
  SHR = 0x35,
  
  SIN = 0x40,
  COS = 0x41,
  SQRT = 0x42,
  
  SETPX = 0x50,
  GETPX = 0x51,
  FILL = 0x52,
  LINE = 0x53,
  RECT = 0x54,
  CIRCLE = 0x55,
  POLY = 0x56,
  SPRITE = 0x57,
  CLEAR = 0x58,
  
  LOOP = 0x60,
  ENDL = 0x61,
  JMP = 0x62,
  JZ = 0x63,
  JNZ = 0x64,
  JGT = 0x65,
  JLT = 0x66,
  
  GETX = 0x70,
  GETY = 0x71,
  GETW = 0x72,
  GETH = 0x73,
  TIME = 0x74,
  RAND = 0x75,
};

// ============================================================
// Bytecode Builder Helper
// ============================================================
class BytecodeBuilder {
public:
  uint8_t buffer[1024];
  int pos = 0;
  
  void reset() { pos = 0; }
  
  void op(Op opcode) { buffer[pos++] = (uint8_t)opcode; }
  void u8(uint8_t v) { buffer[pos++] = v; }
  void i16(int16_t v) { buffer[pos++] = v & 0xFF; buffer[pos++] = (v >> 8) & 0xFF; }
  
  // SET Rd, imm16
  void SET(uint8_t rd, int16_t val) {
    op(Op::SET); u8(rd); i16(val);
  }
  
  // MOV Rd, Rs
  void MOV(uint8_t rd, uint8_t rs) {
    op(Op::MOV); u8(rd); u8(rs);
  }
  
  // LOAD Rd, var_id
  void LOAD(uint8_t rd, uint8_t var) {
    op(Op::LOAD); u8(rd); u8(var);
  }
  
  // ADD Rd, Ra, Rb
  void ADD(uint8_t rd, uint8_t ra, uint8_t rb) {
    op(Op::ADD); u8(rd); u8(ra); u8(rb);
  }
  
  // SUB Rd, Ra, Rb
  void SUB(uint8_t rd, uint8_t ra, uint8_t rb) {
    op(Op::SUB); u8(rd); u8(ra); u8(rb);
  }
  
  // MUL Rd, Ra, Rb
  void MUL(uint8_t rd, uint8_t ra, uint8_t rb) {
    op(Op::MUL); u8(rd); u8(ra); u8(rb);
  }
  
  // SIN Rd, Rs
  void SIN(uint8_t rd, uint8_t rs) {
    op(Op::SIN); u8(rd); u8(rs);
  }
  
  // COS Rd, Rs
  void COS(uint8_t rd, uint8_t rs) {
    op(Op::COS); u8(rd); u8(rs);
  }
  
  // TIME Rd
  void TIME(uint8_t rd) {
    op(Op::TIME); u8(rd);
  }
  
  // CLEAR r, g, b
  void CLEAR(uint8_t r, uint8_t g, uint8_t b) {
    op(Op::CLEAR); u8(r); u8(g); u8(b);
  }
  
  // FILL x, y, w, h, r, g, b (using register indices)
  void FILL_REG(uint8_t xr, uint8_t yr, uint8_t wr, uint8_t hr, uint8_t rr, uint8_t gr, uint8_t br) {
    op(Op::FILL); u8(0x80 | xr); u8(yr); u8(wr); u8(hr); u8(rr); u8(gr); u8(br);
  }
  
  // FILL x, y, w, h, r, g, b (immediate values)
  void FILL_IMM(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
    op(Op::FILL); 
    i16(x); i16(y); i16(w); i16(h); 
    u8(r); u8(g); u8(b);
  }
  
  // LINE x1, y1, x2, y2, r, g, b
  void LINE_IMM(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b) {
    op(Op::LINE);
    i16(x1); i16(y1); i16(x2); i16(y2);
    u8(r); u8(g); u8(b);
  }
  
  // RECT x, y, w, h, r, g, b
  void RECT_IMM(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
    op(Op::RECT);
    i16(x); i16(y); i16(w); i16(h);
    u8(r); u8(g); u8(b);
  }
  
  // CIRCLE cx, cy, radius, r, g, b
  void CIRCLE_IMM(int16_t cx, int16_t cy, int16_t radius, uint8_t r, uint8_t g, uint8_t b) {
    op(Op::CIRCLE);
    i16(cx); i16(cy); i16(radius);
    u8(r); u8(g); u8(b);
  }
  
  // SETPX x, y, r, g, b
  void SETPX_IMM(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
    op(Op::SETPX);
    i16(x); i16(y);
    u8(r); u8(g); u8(b);
  }
  
  void HALT() { op(Op::HALT); }
};

// ============================================================
// Send Command to GPU
// ============================================================
static void sendCommand(CmdType type, const uint8_t* payload, uint16_t len) {
  uint8_t header[5] = {
    SYNC0, SYNC1,
    (uint8_t)type,
    (uint8_t)(len & 0xFF),
    (uint8_t)((len >> 8) & 0xFF)
  };
  
  uart_write_bytes(UART_PORT, header, 5);
  if (len > 0 && payload) {
    uart_write_bytes(UART_PORT, payload, len);
  }
  uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
}

// Upload shader bytecode to a slot
static void uploadShader(uint8_t slot, const uint8_t* bytecode, uint16_t len) {
  uint8_t payload[3 + 1024];
  payload[0] = slot;
  payload[1] = len & 0xFF;
  payload[2] = (len >> 8) & 0xFF;
  memcpy(payload + 3, bytecode, len);
  
  sendCommand(CmdType::UPLOAD_SHADER, payload, 3 + len);
  ESP_LOGI(TAG, "Uploaded shader %d: %d bytes", slot, len);
}

// Execute a shader
static void execShader(uint8_t slot) {
  sendCommand(CmdType::EXEC_SHADER, &slot, 1);
}

// Set a variable
static void setVar(uint8_t id, int16_t value) {
  uint8_t payload[3] = {
    id,
    (uint8_t)(value & 0xFF),
    (uint8_t)((value >> 8) & 0xFF)
  };
  sendCommand(CmdType::SET_VAR, payload, 3);
}

// Set multiple variables
static void setVars(uint8_t startId, const int16_t* values, uint8_t count) {
  uint8_t payload[2 + 256 * 2];
  payload[0] = startId;
  payload[1] = count;
  for (int i = 0; i < count; i++) {
    payload[2 + i*2] = values[i] & 0xFF;
    payload[3 + i*2] = (values[i] >> 8) & 0xFF;
  }
  sendCommand(CmdType::SET_VARS, payload, 2 + count * 2);
}

// Set target (0=HUB75, 1=OLED)
static void setTarget(uint8_t target) {
  sendCommand(CmdType::SET_TARGET, &target, 1);
}

// Present framebuffer
static void present() {
  sendCommand(CmdType::PRESENT, nullptr, 0);
}

// Clear display
static void clearDisplay(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t payload[3] = {r, g, b};
  sendCommand(CmdType::CLEAR, payload, 3);
}

// Draw filled rectangle directly
static void drawFill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t payload[11];
  payload[0] = x & 0xFF; payload[1] = (x >> 8) & 0xFF;
  payload[2] = y & 0xFF; payload[3] = (y >> 8) & 0xFF;
  payload[4] = w & 0xFF; payload[5] = (w >> 8) & 0xFF;
  payload[6] = h & 0xFF; payload[7] = (h >> 8) & 0xFF;
  payload[8] = r; payload[9] = g; payload[10] = b;
  sendCommand(CmdType::DRAW_FILL, payload, 11);
}

// Draw rectangle outline directly
static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t payload[11];
  payload[0] = x & 0xFF; payload[1] = (x >> 8) & 0xFF;
  payload[2] = y & 0xFF; payload[3] = (y >> 8) & 0xFF;
  payload[4] = w & 0xFF; payload[5] = (w >> 8) & 0xFF;
  payload[6] = h & 0xFF; payload[7] = (h >> 8) & 0xFF;
  payload[8] = r; payload[9] = g; payload[10] = b;
  sendCommand(CmdType::DRAW_RECT, payload, 11);
}

// Draw circle directly
static void drawCircle(int16_t cx, int16_t cy, int16_t radius, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t payload[9];
  payload[0] = cx & 0xFF; payload[1] = (cx >> 8) & 0xFF;
  payload[2] = cy & 0xFF; payload[3] = (cy >> 8) & 0xFF;
  payload[4] = radius & 0xFF; payload[5] = (radius >> 8) & 0xFF;
  payload[6] = r; payload[7] = g; payload[8] = b;
  sendCommand(CmdType::DRAW_CIRCLE, payload, 9);
}

// Draw line directly
static void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t payload[11];
  payload[0] = x1 & 0xFF; payload[1] = (x1 >> 8) & 0xFF;
  payload[2] = y1 & 0xFF; payload[3] = (y1 >> 8) & 0xFF;
  payload[4] = x2 & 0xFF; payload[5] = (x2 >> 8) & 0xFF;
  payload[6] = y2 & 0xFF; payload[7] = (y2 >> 8) & 0xFF;
  payload[8] = r; payload[9] = g; payload[10] = b;
  sendCommand(CmdType::DRAW_LINE, payload, 11);
}

// Draw polygon (vertices stored in variables)
static void drawPoly(uint8_t nVerts, uint8_t varStart, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t payload[5] = {nVerts, varStart, r, g, b};
  sendCommand(CmdType::DRAW_POLY, payload, 5);
}

// ============================================================
// Shape Drawing Using Direct Commands
// ============================================================
static void drawShapesDirectCommands() {
  ESP_LOGI(TAG, "Drawing shapes using direct commands...");
  
  // HUB75: 128x32
  setTarget(0);
  clearDisplay(0, 0, 0);
  
  // Left panel (0-63): Hexagon made of lines
  int cx1 = 32, cy1 = 16;
  int r1 = 12;
  for (int i = 0; i < 6; i++) {
    float a1 = (i / 6.0f) * 2 * M_PI;
    float a2 = ((i + 1) / 6.0f) * 2 * M_PI;
    int x1 = cx1 + r1 * cosf(a1);
    int y1 = cy1 + r1 * sinf(a1);
    int x2 = cx1 + r1 * cosf(a2);
    int y2 = cy1 + r1 * sinf(a2);
    drawLine(x1, y1, x2, y2, 255, 0, 128);  // Magenta hexagon
  }
  
  // Right panel (64-127): Concentric shapes
  drawCircle(96, 16, 10, 0, 255, 255);   // Cyan circle
  drawRect(86, 6, 20, 20, 255, 255, 0);  // Yellow square
  drawFill(92, 12, 8, 8, 0, 255, 0);     // Green center
  
  present();
  ESP_LOGI(TAG, "Direct commands sent!");
}

// ============================================================
// Build Shaders for Animation
// ============================================================
static BytecodeBuilder builder;

// Shader 0: Clear screen to color based on time
static void buildClearShader() {
  builder.reset();
  
  // R0 = time, R1 = color phase
  builder.TIME(0);                    // R0 = millis
  builder.SET(1, 4);                  // R1 = 4 (shift amount)
  builder.op(Op::SHR); builder.u8(0); builder.u8(0); builder.u8(1);  // R0 = R0 >> 4
  
  // Use low bits for color
  builder.SET(2, 255);                // R2 = 255
  builder.op(Op::AND); builder.u8(1); builder.u8(0); builder.u8(2); // R1 = R0 & 255
  
  // Clear with shifting color
  builder.CLEAR(0, 0, 32);            // Dark blue background
  builder.HALT();
  
  uploadShader(0, builder.buffer, builder.pos);
}

// Shader 1: Draw animated hexagon on left panel
static void buildHexagonShader() {
  builder.reset();
  
  // Variables:
  // V0 = center X (32)
  // V1 = center Y (16)
  // V2 = radius (12)
  // V3 = rotation offset (updated by CPU)
  // V4-V15 = vertex coordinates (6 vertices * 2 coords)
  
  // This shader draws lines between vertices stored in variables
  // The CPU will compute vertex positions and update variables each frame
  
  // Draw 6 lines forming hexagon
  // Line 1: V4,V5 -> V6,V7
  builder.LOAD(0, 4);   // R0 = V4 (x1)
  builder.LOAD(1, 5);   // R1 = V5 (y1)
  builder.LOAD(2, 6);   // R2 = V6 (x2)
  builder.LOAD(3, 7);   // R3 = V7 (y2)
  builder.LINE_IMM(0, 0, 0, 0, 255, 0, 128);  // Placeholder - we'll use direct commands
  
  builder.HALT();
  
  uploadShader(1, builder.buffer, builder.pos);
}

// Shader 2: Draw animated shapes on right panel  
static void buildShapesShader() {
  builder.reset();
  
  // Variables:
  // V20 = circle center X (animated)
  // V21 = circle center Y
  // V22 = circle radius
  // V23 = rect X
  // V24 = rect Y
  
  // Get animated positions from variables and draw
  builder.LOAD(0, 20);  // R0 = circle X
  builder.LOAD(1, 21);  // R1 = circle Y
  builder.LOAD(2, 22);  // R2 = radius
  
  // Draw circle - using immediate for now
  builder.CIRCLE_IMM(96, 16, 10, 0, 255, 255);
  
  builder.HALT();
  
  uploadShader(2, builder.buffer, builder.pos);
}

// ============================================================
// Animation Loop Using Direct Commands
// ============================================================
static void animationTaskDirect(void* param) {
  ESP_LOGI(TAG, "Starting animation with direct commands...");
  
  uint32_t frame = 0;
  uint32_t lastFps = 0;
  uint32_t fpsCounter = 0;
  int64_t fpsTime = esp_timer_get_time();
  
  while (true) {
    int64_t frameStart = esp_timer_get_time();
    
    // Clear
    setTarget(0);
    clearDisplay(0, 0, 20);  // Dark blue
    
    // Compute hexagon vertices for left panel
    float angle = frame * 0.05f;
    int cx1 = 32, cy1 = 16, r1 = 12;
    
    // Draw hexagon
    for (int i = 0; i < 6; i++) {
      float a1 = angle + (i / 6.0f) * 2 * M_PI;
      float a2 = angle + ((i + 1) / 6.0f) * 2 * M_PI;
      int x1 = cx1 + r1 * cosf(a1);
      int y1 = cy1 + r1 * sinf(a1);
      int x2 = cx1 + r1 * cosf(a2);
      int y2 = cy1 + r1 * sinf(a2);
      
      // Color based on vertex
      uint8_t red = 128 + 127 * sinf(angle + i);
      uint8_t green = 0;
      uint8_t blue = 128 + 127 * cosf(angle + i);
      
      drawLine(x1, y1, x2, y2, red, green, blue);
    }
    
    // Draw triangle on left panel (smaller, inside hexagon)
    float triAngle = -angle * 1.5f;
    int tr = 6;
    for (int i = 0; i < 3; i++) {
      float a1 = triAngle + (i / 3.0f) * 2 * M_PI;
      float a2 = triAngle + ((i + 1) / 3.0f) * 2 * M_PI;
      int x1 = cx1 + tr * cosf(a1);
      int y1 = cy1 + tr * sinf(a1);
      int x2 = cx1 + tr * cosf(a2);
      int y2 = cy1 + tr * sinf(a2);
      drawLine(x1, y1, x2, y2, 255, 255, 0);  // Yellow triangle
    }
    
    // Right panel: Bouncing shapes
    int cx2 = 96, cy2 = 16;
    
    // Bouncing circle
    int circleY = 16 + 8 * sinf(frame * 0.1f);
    drawCircle(cx2, circleY, 8, 0, 255, 255);  // Cyan
    
    // Pulsing rectangle
    int rectSize = 12 + 4 * sinf(frame * 0.15f);
    drawRect(cx2 - rectSize/2, cy2 - rectSize/2, rectSize, rectSize, 255, 128, 0);  // Orange
    
    // Small spinning square at center
    float sqAngle = frame * 0.2f;
    int sqr = 4;
    for (int i = 0; i < 4; i++) {
      float a1 = sqAngle + (i / 4.0f) * 2 * M_PI;
      float a2 = sqAngle + ((i + 1) / 4.0f) * 2 * M_PI;
      int x1 = cx2 + sqr * cosf(a1);
      int y1 = cy2 + sqr * sinf(a1);
      int x2 = cx2 + sqr * cosf(a2);
      int y2 = cy2 + sqr * sinf(a2);
      drawLine(x1, y1, x2, y2, 0, 255, 0);  // Green
    }
    
    // Present to display
    present();
    
    frame++;
    fpsCounter++;
    
    // FPS calculation
    int64_t now = esp_timer_get_time();
    if (now - fpsTime >= 1000000) {
      lastFps = fpsCounter;
      fpsCounter = 0;
      fpsTime = now;
      ESP_LOGI(TAG, "FPS: %lu | Frame: %lu", lastFps, frame);
    }
    
    // Frame timing
    int64_t elapsed = esp_timer_get_time() - frameStart;
    int64_t target = 33333;  // ~30 FPS
    if (elapsed < target) {
      vTaskDelay(pdMS_TO_TICKS((target - elapsed) / 1000));
    }
  }
}

// ============================================================
// UART Initialization
// ============================================================
static bool initUART() {
  ESP_LOGI(TAG, "Initializing UART...");
  
  uart_config_t uart_config = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_DEFAULT,
  };
  
  esp_err_t err = uart_driver_install(UART_PORT, 4096, 4096, 0, nullptr, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART driver install failed: %d", err);
    return false;
  }
  
  err = uart_param_config(UART_PORT, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART config failed: %d", err);
    return false;
  }
  
  err = uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, -1, -1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART set pin failed: %d", err);
    return false;
  }
  
  ESP_LOGI(TAG, "UART OK: TX=%d, RX=%d, Baud=%d", UART_TX_PIN, UART_RX_PIN, UART_BAUD);
  return true;
}

// ============================================================
// Main Entry Point
// ============================================================
extern "C" void app_main(void) {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║   CPU Programmable GPU Test                  ║");
  ESP_LOGI(TAG, "║   Sending shapes to GPU via UART             ║");
  ESP_LOGI(TAG, "╚══════════════════════════════════════════════╝");
  ESP_LOGI(TAG, "");
  
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  if (!initUART()) {
    ESP_LOGE(TAG, "UART init failed!");
    return;
  }
  
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Send ping to GPU
  ESP_LOGI(TAG, "Sending PING to GPU...");
  sendCommand(CmdType::PING, nullptr, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Send reset
  ESP_LOGI(TAG, "Sending RESET to GPU...");
  sendCommand(CmdType::RESET, nullptr, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Start animation task
  xTaskCreatePinnedToCore(
    animationTaskDirect,
    "anim",
    8192,
    nullptr,
    5,
    nullptr,
    1
  );
  
  ESP_LOGI(TAG, "Animation task started!");
  
  // Main loop - just monitor
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
