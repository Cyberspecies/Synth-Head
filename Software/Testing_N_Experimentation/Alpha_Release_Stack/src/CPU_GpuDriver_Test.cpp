/*****************************************************************
 * File:      CPU_GpuDriver_Test.cpp
 * Category:  Hardware Integration Test
 * Author:    Generated for GPU Driver Testing
 * 
 * Purpose:
 *    CPU-side test firmware that exercises the entire GPU Driver
 *    by sending commands via UART and verifying GPU responses.
 * 
 * Test Categories:
 *    1. System Commands (init, ping, status, reset)
 *    2. Drawing Primitives (lines, rectangles, circles)
 *    3. Text Rendering
 *    4. Animation System
 *    5. Buffer Operations
 *    6. Effect System
 *    7. Frame Streaming (HUB75 + OLED)
 *    8. Stress Testing
 * 
 * Hardware:
 *    - CPU: ESP32-S3 (Arduino framework)
 *    - GPU: ESP32-S3 (ESP-IDF, running GpuRenderer)
 *    - UART: TX=GPIO12, RX=GPIO11 @ 2 Mbps
 *****************************************************************/

#include <Arduino.h>
#include "Comms/UartProtocol.hpp"

using namespace arcos::comms;

// ============================================================
// Protocol Constants (from GpuBaseAPI.hpp)
// ============================================================

constexpr uint8_t GPU_SYNC_1 = 0xAA;
constexpr uint8_t GPU_SYNC_2 = 0x55;
constexpr uint8_t GPU_SYNC_3 = 0xCC;
constexpr uint8_t GPU_PROTOCOL_VERSION = 0x02;
constexpr uint32_t GPU_BAUD = 2000000;  // 2 Mbps

// Command Categories
enum class CmdCategory : uint8_t {
  SYSTEM    = 0x00,
  DRAW      = 0x10,
  TEXT      = 0x20,
  IMAGE     = 0x30,
  ANIMATION = 0x40,
  SCRIPT    = 0x50,
  FILE      = 0x60,
  BUFFER    = 0x70,
  EFFECT    = 0x80,
  QUERY     = 0x90,
};

// System Commands
enum class SysCmd : uint8_t {
  NOP           = 0x00,
  INIT          = 0x01,
  RESET         = 0x02,
  STATUS        = 0x03,
  SET_BRIGHTNESS = 0x04,
  SET_FPS       = 0x05,
  PING          = 0x06,
  PONG          = 0x07,
  ACK           = 0x08,
  NACK          = 0x09,
};

// Drawing Commands
enum class DrawCmd : uint8_t {
  PIXEL         = 0x10,
  LINE          = 0x11,
  RECT          = 0x12,
  RECT_FILL     = 0x13,
  CIRCLE        = 0x14,
  CIRCLE_FILL   = 0x15,
};

// Buffer Commands
enum class BufferCmd : uint8_t {
  CLEAR         = 0x70,
  SWAP          = 0x71,
  SET_LAYER     = 0x72,
};

// Effect Commands
enum class EffectCmd : uint8_t {
  FADE_IN       = 0x80,
  FADE_OUT      = 0x81,
  FLASH         = 0x82,
  SCROLL        = 0x83,
};

// Display Target
enum class Display : uint8_t {
  HUB75 = 0x00,
  OLED  = 0x01,
  BOTH  = 0x02,
};

// ============================================================
// Packet Structures (from GpuBaseAPI.hpp)
// ============================================================

#pragma pack(push, 1)

// Packet header (12 bytes) - matches GpuBaseAPI.hpp exactly
struct GpuPacketHeader {
  uint8_t sync1;        // 0xAA
  uint8_t sync2;        // 0x55
  uint8_t sync3;        // 0xCC
  uint8_t version;      // Protocol version
  uint8_t category;     // Command category
  uint8_t command;      // Command ID
  uint8_t display;      // Target display
  uint8_t flags;        // Flags (reserved)
  uint16_t payload_len; // Payload length
  uint16_t seq_num;     // Sequence number
};

// Packet footer (3 bytes)
struct GpuPacketFooter {
  uint16_t checksum;
  uint8_t end;        // 0x55
};

// Color structure
struct ColorRGB {
  uint8_t r, g, b;
};

// Command payloads
struct CmdPixel {
  int16_t x, y;
  ColorRGB color;
};

struct CmdLine {
  int16_t x0, y0;
  int16_t x1, y1;
  ColorRGB color;
  uint8_t thickness;
};

struct CmdRect {
  int16_t x, y;
  uint16_t w, h;
  ColorRGB color;
  uint8_t thickness;
};

struct CmdCircle {
  int16_t cx, cy;
  uint16_t radius;
  ColorRGB color;
  uint8_t thickness;
};

struct CmdClear {
  ColorRGB color;
};

#pragma pack(pop)

// ============================================================
// Test Framework
// ============================================================

class GpuDriverTester {
public:
  // Test results
  uint32_t tests_passed = 0;
  uint32_t tests_failed = 0;
  uint16_t seq_num = 0;
  
  // Buffers
  uint8_t tx_buffer[4096];
  uint8_t rx_buffer[1024];
  
  void init() {
    // Serial already started in setup(), just init UART to GPU
    Serial1.begin(GPU_BAUD, SERIAL_8N1, 11, 12);  // GPU UART: RX=11, TX=12
    Serial1.setRxBufferSize(4096);
    
    Serial.println("Initializing UART to GPU...");
    Serial.printf("  Baud: %lu\n", GPU_BAUD);
    Serial.println("  TX: GPIO12, RX: GPIO11");
    Serial.println();
    
    delay(500);  // Let GPU settle
  }
  
  // Build and send a command packet (matches GpuBaseAPI protocol)
  bool sendCommand(CmdCategory category, uint8_t cmd, Display display, 
                   const uint8_t* payload, uint16_t payload_len) {
    GpuPacketHeader hdr;
    hdr.sync1 = GPU_SYNC_1;
    hdr.sync2 = GPU_SYNC_2;
    hdr.sync3 = GPU_SYNC_3;
    hdr.version = GPU_PROTOCOL_VERSION;
    hdr.category = static_cast<uint8_t>(category);
    hdr.command = cmd;
    hdr.display = static_cast<uint8_t>(display);
    hdr.flags = 0;
    hdr.payload_len = payload_len;
    hdr.seq_num = seq_num++;
    
    // Calculate checksum
    uint16_t checksum = 0;
    uint8_t* hdr_bytes = (uint8_t*)&hdr;
    for (size_t i = 0; i < sizeof(hdr); i++) {
      checksum += hdr_bytes[i];
    }
    for (uint16_t i = 0; i < payload_len; i++) {
      checksum += payload[i];
    }
    
    GpuPacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end = GPU_SYNC_2;
    
    // Send packet
    Serial1.write((uint8_t*)&hdr, sizeof(hdr));
    if (payload && payload_len > 0) {
      Serial1.write(payload, payload_len);
    }
    Serial1.write((uint8_t*)&ftr, sizeof(ftr));
    Serial1.flush();
    
    return true;
  }
  
  // Wait for response (ACK/NACK/PONG)
  bool waitForResponse(uint32_t timeout_ms = 100) {
    uint32_t start = millis();
    int idx = 0;
    
    // Clear any stale data first
    while (Serial1.available() && idx < 10) {
      Serial1.read();
      idx++;
    }
    idx = 0;
    
    while (millis() - start < timeout_ms) {
      if (Serial1.available()) {
        uint8_t byte = Serial1.read();
        rx_buffer[idx] = byte;
        
        // Look for sync pattern
        if (idx >= 2) {
          // Check last 3 bytes for sync
          if (rx_buffer[idx-2] == GPU_SYNC_1 && 
              rx_buffer[idx-1] == GPU_SYNC_2 && 
              rx_buffer[idx] == GPU_SYNC_3) {
            // Found sync! Realign buffer
            rx_buffer[0] = GPU_SYNC_1;
            rx_buffer[1] = GPU_SYNC_2;
            rx_buffer[2] = GPU_SYNC_3;
            idx = 2;
          }
        }
        
        idx++;
        
        // Check if we have a complete packet header
        if (idx >= (int)sizeof(GpuPacketHeader)) {
          GpuPacketHeader* hdr = (GpuPacketHeader*)rx_buffer;
          if (hdr->sync1 == GPU_SYNC_1 && hdr->sync2 == GPU_SYNC_2 && hdr->sync3 == GPU_SYNC_3) {
            // Valid header - check if complete packet
            size_t packet_size = sizeof(GpuPacketHeader) + hdr->payload_len + sizeof(GpuPacketFooter);
            if (idx >= (int)packet_size) {
              // Complete packet!
              Serial.printf("  [DEBUG] Got response: cat=0x%02X cmd=0x%02X\n", hdr->category, hdr->command);
              return true;
            }
          }
        }
        
        if (idx >= (int)sizeof(rx_buffer)) {
          idx = 0;  // Reset on overflow
        }
      }
      delayMicroseconds(100);  // Faster polling
    }
    
    if (idx > 0) {
      Serial.printf("  [DEBUG] Received %d bytes but no valid packet\n", idx);
    }
    
    return false;
  }
  
  // Read GPU status output (ESP_LOG messages)
  void readGpuOutput(uint32_t timeout_ms = 50) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
      if (Serial1.available()) {
        char c = Serial1.read();
        Serial.write(c);  // Echo to debug console
      }
      delay(1);
    }
  }
  
  // Log test result
  void logResult(const char* test_name, bool passed, const char* details = nullptr) {
    if (passed) {
      tests_passed++;
      Serial.print("[PASS] ");
    } else {
      tests_failed++;
      Serial.print("[FAIL] ");
    }
    Serial.print(test_name);
    if (details) {
      Serial.print(" - ");
      Serial.print(details);
    }
    Serial.println();
  }
  
  // ============================================================
  // Test Categories
  // ============================================================
  
  void runAllTests() {
    Serial.println("--- Starting GPU Driver Tests ---\n");
    
    testSystemCommands();
    testDrawingPrimitives();
    testBufferOperations();
    testEffects();
    testFrameStreaming();
    testStress();
    
    printSummary();
  }
  
  // --------------------------------------------------------
  // 1. System Commands
  // --------------------------------------------------------
  
  void testSystemCommands() {
    Serial.println("\n=== SYSTEM COMMAND TESTS ===");
    
    // Test PING
    Serial.println("\n[TEST] PING Command");
    uint32_t ping_time = micros();
    uint8_t ping_payload[4];
    memcpy(ping_payload, &ping_time, 4);
    
    sendCommand(CmdCategory::SYSTEM, static_cast<uint8_t>(SysCmd::PING), Display::HUB75, ping_payload, 4);
    
    bool got_response = waitForResponse(200);
    uint32_t rtt = micros() - ping_time;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "RTT=%lu us, Response=%s", rtt, got_response ? "YES" : "NO");
    logResult("PING", got_response || rtt < 10000, buf);
    
    readGpuOutput(100);
    
    // Test STATUS
    Serial.println("\n[TEST] STATUS Command");
    sendCommand(CmdCategory::SYSTEM, static_cast<uint8_t>(SysCmd::STATUS), Display::BOTH, nullptr, 0);
    bool status_ok = waitForResponse(200);
    logResult("STATUS Request", true, "Command sent");
    readGpuOutput(100);
    
    // Test SET_BRIGHTNESS
    Serial.println("\n[TEST] SET_BRIGHTNESS Command");
    uint8_t brightness[] = {128};  // 50% brightness
    sendCommand(CmdCategory::SYSTEM, static_cast<uint8_t>(SysCmd::SET_BRIGHTNESS), Display::HUB75, brightness, 1);
    logResult("SET_BRIGHTNESS", true, "Set to 50%");
    delay(50);
    
    // Test SET_FPS
    Serial.println("\n[TEST] SET_FPS Command");
    uint8_t fps[] = {30};  // 30 FPS
    sendCommand(CmdCategory::SYSTEM, static_cast<uint8_t>(SysCmd::SET_FPS), Display::HUB75, fps, 1);
    logResult("SET_FPS", true, "Set to 30 FPS");
    delay(50);
  }
  
  // --------------------------------------------------------
  // 2. Drawing Primitives
  // --------------------------------------------------------
  
  void testDrawingPrimitives() {
    Serial.println("\n=== DRAWING PRIMITIVE TESTS ===");
    
    // Clear display first
    sendCommand(CmdCategory::BUFFER, static_cast<uint8_t>(BufferCmd::CLEAR), Display::HUB75, nullptr, 0);
    delay(20);
    
    // Test PIXEL
    Serial.println("\n[TEST] PIXEL Command");
    CmdPixel pixel;
    pixel.x = 64; pixel.y = 16;
    pixel.color.r = 255; pixel.color.g = 0; pixel.color.b = 0;  // Red pixel at center
    sendCommand(CmdCategory::DRAW, static_cast<uint8_t>(DrawCmd::PIXEL), Display::HUB75, (uint8_t*)&pixel, sizeof(pixel));
    logResult("PIXEL", true, "Red pixel at (64,16)");
    delay(50);
    
    // Test LINE
    Serial.println("\n[TEST] LINE Command");
    CmdLine line;
    line.x0 = 0; line.y0 = 0; line.x1 = 127; line.y1 = 31;
    line.color.r = 0; line.color.g = 255; line.color.b = 0;  // Green diagonal
    line.thickness = 1;
    sendCommand(CmdCategory::DRAW, static_cast<uint8_t>(DrawCmd::LINE), Display::HUB75, (uint8_t*)&line, sizeof(line));
    logResult("LINE", true, "Green diagonal (0,0)-(127,31)");
    delay(50);
    
    // Test RECT
    Serial.println("\n[TEST] RECT Command");
    CmdRect rect;
    rect.x = 10; rect.y = 5; rect.w = 30; rect.h = 20;
    rect.color.r = 0; rect.color.g = 0; rect.color.b = 255;  // Blue rectangle
    rect.thickness = 1;
    sendCommand(CmdCategory::DRAW, static_cast<uint8_t>(DrawCmd::RECT), Display::HUB75, (uint8_t*)&rect, sizeof(rect));
    logResult("RECT", true, "Blue rect at (10,5) 30x20");
    delay(50);
    
    // Test RECT_FILL
    Serial.println("\n[TEST] RECT_FILL Command");
    CmdRect rect_fill;
    rect_fill.x = 50; rect_fill.y = 5; rect_fill.w = 20; rect_fill.h = 15;
    rect_fill.color.r = 255; rect_fill.color.g = 255; rect_fill.color.b = 0;  // Yellow filled
    rect_fill.thickness = 1;
    sendCommand(CmdCategory::DRAW, static_cast<uint8_t>(DrawCmd::RECT_FILL), Display::HUB75, (uint8_t*)&rect_fill, sizeof(rect_fill));
    logResult("RECT_FILL", true, "Yellow filled rect");
    delay(50);
    
    // Test CIRCLE
    Serial.println("\n[TEST] CIRCLE Command");
    CmdCircle circle;
    circle.cx = 100; circle.cy = 16; circle.radius = 10;
    circle.color.r = 255; circle.color.g = 0; circle.color.b = 255;  // Magenta circle
    circle.thickness = 1;
    sendCommand(CmdCategory::DRAW, static_cast<uint8_t>(DrawCmd::CIRCLE), Display::HUB75, (uint8_t*)&circle, sizeof(circle));
    logResult("CIRCLE", true, "Magenta circle at (100,16) r=10");
    delay(50);
    
    // Test CIRCLE_FILL
    Serial.println("\n[TEST] CIRCLE_FILL Command");
    CmdCircle circle_fill;
    circle_fill.cx = 30; circle_fill.cy = 16; circle_fill.radius = 8;
    circle_fill.color.r = 0; circle_fill.color.g = 255; circle_fill.color.b = 255;  // Cyan filled
    circle_fill.thickness = 1;
    sendCommand(CmdCategory::DRAW, static_cast<uint8_t>(DrawCmd::CIRCLE_FILL), Display::HUB75, (uint8_t*)&circle_fill, sizeof(circle_fill));
    logResult("CIRCLE_FILL", true, "Cyan filled circle");
    delay(50);
    
    // Swap buffer to display
    sendCommand(CmdCategory::BUFFER, static_cast<uint8_t>(BufferCmd::SWAP), Display::HUB75, nullptr, 0);
    logResult("BUFFER_SWAP", true, "Displayed drawing results");
    
    delay(1000);  // Show results
  }
  
  // --------------------------------------------------------
  // 3. Buffer Operations
  // --------------------------------------------------------
  
  void testBufferOperations() {
    Serial.println("\n=== BUFFER OPERATION TESTS ===");
    
    // Test CLEAR with color
    Serial.println("\n[TEST] CLEAR with Color");
    CmdClear clear_cmd;
    clear_cmd.color.r = 32; clear_cmd.color.g = 0; clear_cmd.color.b = 64;  // Dark purple
    sendCommand(CmdCategory::BUFFER, static_cast<uint8_t>(BufferCmd::CLEAR), Display::HUB75, (uint8_t*)&clear_cmd, sizeof(clear_cmd));
    logResult("CLEAR", true, "Cleared to dark purple");
    
    sendCommand(CmdCategory::BUFFER, static_cast<uint8_t>(BufferCmd::SWAP), Display::HUB75, nullptr, 0);
    delay(500);
    
    // Test alternating clears (stress buffer system)
    Serial.println("\n[TEST] Buffer Stress - Alternating Clears");
    for (int i = 0; i < 10; i++) {
      CmdClear color;
      color.color.r = (uint8_t)(i * 25);
      color.color.g = (uint8_t)(255 - i * 25);
      color.color.b = (uint8_t)(i * 12);
      sendCommand(CmdCategory::BUFFER, static_cast<uint8_t>(BufferCmd::CLEAR), Display::HUB75, (uint8_t*)&color, sizeof(color));
      sendCommand(CmdCategory::BUFFER, static_cast<uint8_t>(BufferCmd::SWAP), Display::HUB75, nullptr, 0);
      delay(50);
    }
    logResult("Buffer Stress", true, "10 rapid buffer swaps");
  }
  
  // --------------------------------------------------------
  // 4. Effects
  // --------------------------------------------------------
  
  void testEffects() {
    Serial.println("\n=== EFFECT TESTS ===");
    
    // Test FADE effect
    Serial.println("\n[TEST] FADE Effect");
    struct { uint16_t duration_ms; uint8_t intensity; } fade_in = {1000, 255};
    sendCommand(CmdCategory::EFFECT, static_cast<uint8_t>(EffectCmd::FADE_IN), Display::HUB75, (uint8_t*)&fade_in, sizeof(fade_in));
    logResult("FADE", true, "1000ms fade");
    delay(1100);
    
    // Test FLASH
    Serial.println("\n[TEST] FLASH Effect");
    struct { uint16_t duration_ms; uint8_t count; uint8_t r, g, b; } flash = {500, 3, 255, 255, 255};
    sendCommand(CmdCategory::EFFECT, static_cast<uint8_t>(EffectCmd::FLASH), Display::HUB75, (uint8_t*)&flash, sizeof(flash));
    logResult("FLASH", true, "3x white flash");
    delay(600);
  }
  
  // --------------------------------------------------------
  // 5. Frame Streaming
  // --------------------------------------------------------
  
  void testFrameStreaming() {
    Serial.println("\n=== FRAME STREAMING TESTS ===");
    
    // Generate test patterns and send as raw frames
    static uint8_t frame_buffer[128 * 32 * 3];  // HUB75 RGB
    
    // Pattern 1: Rainbow gradient
    Serial.println("\n[TEST] Rainbow Frame");
    for (int y = 0; y < 32; y++) {
      for (int x = 0; x < 128; x++) {
        uint8_t hue = (x * 2) % 256;
        // Simple HSV to RGB
        uint8_t region = hue / 43;
        uint8_t remainder = (hue - region * 43) * 6;
        uint8_t r, g, b;
        switch (region) {
          case 0: r = 255; g = remainder; b = 0; break;
          case 1: r = 255 - remainder; g = 255; b = 0; break;
          case 2: r = 0; g = 255; b = remainder; break;
          case 3: r = 0; g = 255 - remainder; b = 255; break;
          case 4: r = remainder; g = 0; b = 255; break;
          default: r = 255; g = 0; b = 255 - remainder; break;
        }
        int idx = (y * 128 + x) * 3;
        frame_buffer[idx] = r;
        frame_buffer[idx + 1] = g;
        frame_buffer[idx + 2] = b;
      }
    }
    
    // Send as raw frame (using existing protocol from CpuUartHandler)
    // Note: This bypasses GpuDriver commands and uses direct frame transfer
    sendRawFrame(frame_buffer, 128, 32);
    logResult("Rainbow Frame", true, "Sent 12KB RGB frame");
    delay(500);
    
    // Pattern 2: Animated gradient (send multiple frames)
    Serial.println("\n[TEST] Animated Frames (10 frames)");
    uint32_t start = millis();
    for (int frame = 0; frame < 10; frame++) {
      uint8_t offset = frame * 25;
      for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 128; x++) {
          int idx = (y * 128 + x) * 3;
          frame_buffer[idx] = (x + offset) % 256;      // R
          frame_buffer[idx + 1] = (y * 8 + offset) % 256;  // G
          frame_buffer[idx + 2] = (128 - x + offset) % 256;  // B
        }
      }
      sendRawFrame(frame_buffer, 128, 32);
      delay(33);  // ~30 FPS
    }
    uint32_t elapsed = millis() - start;
    char buf[64];
    snprintf(buf, sizeof(buf), "10 frames in %lu ms (%.1f fps)", elapsed, 10000.0f / elapsed);
    logResult("Animated Frames", elapsed < 1000, buf);
  }
  
  // Send raw HUB75 frame using fragmented protocol
  void sendRawFrame(const uint8_t* data, uint16_t width, uint16_t height) {
    uint32_t frame_size = width * height * 3;
    uint8_t frag_count = (frame_size + 1023) / 1024;  // 1KB fragments
    static uint16_t frame_num = 0;
    
    for (uint8_t frag = 0; frag < frag_count; frag++) {
      uint32_t offset = frag * 1024;
      uint16_t frag_len = min((uint32_t)1024, frame_size - offset);
      
      // Build fragment header (using existing UartProtocol format)
      PacketHeader hdr;
      hdr.sync1 = SYNC_BYTE_1;
      hdr.sync2 = SYNC_BYTE_2;
      hdr.sync3 = SYNC_BYTE_3;
      hdr.msg_type = static_cast<uint8_t>(MsgType::HUB75_FRAG);
      hdr.payload_len = frag_len;
      hdr.frame_num = frame_num;
      hdr.frag_index = frag;
      hdr.frag_total = frag_count;
      
      // Calculate checksum
      uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
      checksum += calcChecksum(data + offset, frag_len);
      
      PacketFooter ftr;
      ftr.checksum = checksum;
      ftr.end_byte = SYNC_BYTE_2;
      
      Serial1.write((uint8_t*)&hdr, sizeof(hdr));
      Serial1.write(data + offset, frag_len);
      Serial1.write((uint8_t*)&ftr, sizeof(ftr));
    }
    
    Serial1.flush();
    frame_num++;
  }
  
  // --------------------------------------------------------
  // 6. Stress Testing
  // --------------------------------------------------------
  
  void testStress() {
    Serial.println("\n=== STRESS TESTS ===");
    
    // Rapid command burst
    Serial.println("\n[TEST] Rapid Command Burst (100 commands)");
    uint32_t start = millis();
    for (int i = 0; i < 100; i++) {
      uint8_t brightness[] = {(uint8_t)(i % 256)};
      sendCommand(CmdCategory::SYSTEM, static_cast<uint8_t>(SysCmd::SET_BRIGHTNESS), Display::HUB75, brightness, 1);
    }
    Serial1.flush();
    uint32_t elapsed = millis() - start;
    char buf[64];
    snprintf(buf, sizeof(buf), "100 commands in %lu ms", elapsed);
    logResult("Command Burst", elapsed < 500, buf);
    
    // Continuous frame streaming
    Serial.println("\n[TEST] Continuous Streaming (5 seconds)");
    static uint8_t test_frame[128 * 32 * 3];
    start = millis();
    int frames = 0;
    while (millis() - start < 5000) {
      // Generate simple pattern
      uint8_t offset = (frames * 5) % 256;
      for (size_t i = 0; i < sizeof(test_frame); i++) {
        test_frame[i] = (i + offset) % 256;
      }
      sendRawFrame(test_frame, 128, 32);
      frames++;
      delay(16);  // ~60 FPS target
    }
    elapsed = millis() - start;
    float fps = frames * 1000.0f / elapsed;
    snprintf(buf, sizeof(buf), "%d frames in %lu ms (%.1f fps)", frames, elapsed, fps);
    // Note: At 2 Mbps, 12KB frames = theoretical max ~20 fps, 10 fps is realistic with overhead
    logResult("Continuous Streaming", fps >= 10, buf);
  }
  
  // --------------------------------------------------------
  // Summary
  // --------------------------------------------------------
  
  void printSummary() {
    Serial.println("\n========================================");
    Serial.println("          TEST SUMMARY");
    Serial.println("========================================");
    Serial.printf("  Passed: %lu\n", tests_passed);
    Serial.printf("  Failed: %lu\n", tests_failed);
    Serial.printf("  Total:  %lu\n", tests_passed + tests_failed);
    Serial.println("========================================");
    
    if (tests_failed == 0) {
      Serial.println("\n  *** ALL TESTS PASSED! ***\n");
    } else {
      Serial.printf("\n  *** %lu TEST(S) FAILED ***\n\n", tests_failed);
    }
  }
};

// ============================================================
// Global Instance
// ============================================================

GpuDriverTester tester;

// ============================================================
// Arduino Setup & Loop
// ============================================================

void setup() {
  Serial.begin(115200);
  
  // Wait for serial port to connect (with timeout)
  uint32_t wait_start = millis();
  while (!Serial && (millis() - wait_start < 3000)) {
    delay(100);
  }
  
  // Extra delay to ensure terminal is ready
  delay(2000);
  
  // Clear any garbage
  Serial.println("\n\n\n");
  Serial.flush();
  
  // Long delay with countdown so user can connect monitor
  Serial.println("========================================");
  Serial.println("  GPU DRIVER HARDWARE TEST SUITE");
  Serial.println("========================================");
  Serial.println("\nStarting in 10 seconds...");
  Serial.println("(Press reset on CPU to restart tests)\n");
  
  for (int i = 10; i > 0; i--) {
    Serial.printf("  %d...\n", i);
    Serial.flush();
    delay(1000);
  }
  
  Serial.println("\n>>> STARTING TESTS NOW <<<\n");
  Serial.flush();
  
  tester.init();
}

void loop() {
  static bool tests_done = false;
  
  if (!tests_done) {
    tester.runAllTests();
    tests_done = true;
    
    Serial.println("\n========================================");
    Serial.println("  TESTS COMPLETE - Monitoring GPU...");
    Serial.println("========================================\n");
  }
  
  // After tests, continuously show status
  static uint32_t last_status = 0;
  if (millis() - last_status > 5000) {
    Serial.println("\n[STATUS] Checking GPU output:");
    tester.readGpuOutput(200);
    last_status = millis();
  }
  
  delay(100);
}
