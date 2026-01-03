/*****************************************************************
 * File:      GpuBaseAPI.hpp
 * Category:  GPU Driver / Base API
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Base API definitions for CPU-GPU communication.
 *    Defines the command protocol for sending graphics commands,
 *    vectors, files, and scripts to the GPU for rendering.
 * 
 * Architecture:
 *    CPU sends high-level commands → GPU renders to displays
 *    - Reduces bandwidth (commands vs raw pixels)
 *    - GPU handles rendering, animations, effects
 *    - CPU focuses on logic, sensors, communication
 * 
 * Displays:
 *    - HUB75: 128x32 RGB LED matrix
 *    - OLED:  128x128 monochrome
 *****************************************************************/

#ifndef GPU_DRIVER_BASE_API_HPP_
#define GPU_DRIVER_BASE_API_HPP_

#include <stdint.h>
#include <string.h>

namespace gpu {

// ============================================================
// Protocol Constants
// ============================================================

constexpr uint8_t SYNC_BYTE_1 = 0xAA;
constexpr uint8_t SYNC_BYTE_2 = 0x55;
constexpr uint8_t SYNC_BYTE_3 = 0xCC;
constexpr uint8_t PROTOCOL_VERSION = 0x02;

// Communication settings
constexpr uint32_t GPU_BAUD_RATE = 2000000;  // 2 Mbps - reliable for commands
constexpr uint16_t MAX_PACKET_SIZE = 4096;   // Max packet payload
constexpr uint16_t MAX_SCRIPT_SIZE = 2048;   // Max script size
constexpr uint32_t ACK_TIMEOUT_MS = 100;     // ACK timeout
constexpr uint8_t MAX_RETRIES = 3;

// Display dimensions
constexpr uint16_t HUB75_WIDTH = 128;
constexpr uint16_t HUB75_HEIGHT = 32;
constexpr uint16_t OLED_WIDTH = 128;
constexpr uint16_t OLED_HEIGHT = 128;

// Resource limits
constexpr uint8_t MAX_SPRITES = 32;
constexpr uint8_t MAX_FONTS = 8;
constexpr uint8_t MAX_ANIMATIONS = 16;
constexpr uint8_t MAX_LAYERS = 4;

// ============================================================
// Target Display
// ============================================================

enum class Display : uint8_t {
  HUB75     = 0x00,  // RGB LED matrix
  OLED      = 0x01,  // Monochrome OLED
  BOTH      = 0x02,  // Apply to both displays
};

// ============================================================
// Command Categories
// ============================================================

enum class CmdCategory : uint8_t {
  SYSTEM    = 0x00,  // System commands (init, status, reset)
  DRAW      = 0x10,  // Drawing primitives (line, rect, circle)
  TEXT      = 0x20,  // Text rendering
  IMAGE     = 0x30,  // Image/sprite operations
  ANIMATION = 0x40,  // Animation control
  SCRIPT    = 0x50,  // Script execution
  FILE      = 0x60,  // File transfer
  BUFFER    = 0x70,  // Buffer operations (clear, swap, blend)
  EFFECT    = 0x80,  // Visual effects
  QUERY     = 0x90,  // Query GPU state
};

// ============================================================
// System Commands (0x00-0x0F)
// ============================================================

enum class SysCmd : uint8_t {
  NOP           = 0x00,  // No operation
  INIT          = 0x01,  // Initialize GPU
  RESET         = 0x02,  // Reset GPU state
  STATUS        = 0x03,  // Get GPU status
  SET_BRIGHTNESS = 0x04, // Set display brightness
  SET_FPS       = 0x05,  // Set target FPS
  PING          = 0x06,  // Ping for latency
  PONG          = 0x07,  // Pong response
  ACK           = 0x08,  // Acknowledge
  NACK          = 0x09,  // Negative acknowledge
  VERSION       = 0x0A,  // Get protocol version
  CAPABILITIES  = 0x0B,  // Get GPU capabilities
  POWER_MODE    = 0x0C,  // Set power mode
  DEBUG         = 0x0F,  // Debug command
};

// ============================================================
// Drawing Commands (0x10-0x1F)
// ============================================================

enum class DrawCmd : uint8_t {
  PIXEL         = 0x10,  // Draw single pixel
  LINE          = 0x11,  // Draw line
  RECT          = 0x12,  // Draw rectangle (outline)
  RECT_FILL     = 0x13,  // Draw filled rectangle
  CIRCLE        = 0x14,  // Draw circle (outline)
  CIRCLE_FILL   = 0x15,  // Draw filled circle
  ELLIPSE       = 0x16,  // Draw ellipse
  TRIANGLE      = 0x17,  // Draw triangle
  POLYGON       = 0x18,  // Draw polygon
  ARC           = 0x19,  // Draw arc
  BEZIER        = 0x1A,  // Draw bezier curve
  POLYLINE      = 0x1B,  // Draw connected lines
  ROUNDED_RECT  = 0x1C,  // Draw rounded rectangle
  GRADIENT_RECT = 0x1D,  // Draw gradient rectangle
};

// ============================================================
// Text Commands (0x20-0x2F)
// ============================================================

enum class TextCmd : uint8_t {
  DRAW_CHAR     = 0x20,  // Draw single character
  DRAW_STRING   = 0x21,  // Draw string
  SET_FONT      = 0x22,  // Set current font
  SET_SIZE      = 0x23,  // Set text size/scale
  SET_COLOR     = 0x24,  // Set text color
  SET_ALIGN     = 0x25,  // Set text alignment
  SET_WRAP      = 0x26,  // Set text wrap mode
  MEASURE       = 0x27,  // Measure text dimensions
  DRAW_FORMATTED = 0x28, // Draw formatted text
  SET_CURSOR    = 0x29,  // Set text cursor position
};

// ============================================================
// Image Commands (0x30-0x3F)
// ============================================================

enum class ImageCmd : uint8_t {
  DRAW_SPRITE   = 0x30,  // Draw sprite at position
  LOAD_SPRITE   = 0x31,  // Load sprite to GPU memory
  UNLOAD_SPRITE = 0x32,  // Unload sprite from GPU
  DRAW_BITMAP   = 0x33,  // Draw raw bitmap data
  DRAW_ICON     = 0x34,  // Draw icon from icon set
  SET_PALETTE   = 0x35,  // Set color palette
  TRANSFORM     = 0x36,  // Transform sprite (rotate, scale)
  TILE          = 0x37,  // Draw tiled pattern
  BLIT          = 0x38,  // Blit region
  COPY_REGION   = 0x39,  // Copy screen region
};

// ============================================================
// Animation Commands (0x40-0x4F)
// ============================================================

enum class AnimCmd : uint8_t {
  CREATE        = 0x40,  // Create animation
  START         = 0x41,  // Start animation
  STOP          = 0x42,  // Stop animation
  PAUSE         = 0x43,  // Pause animation
  RESUME        = 0x44,  // Resume animation
  SET_FRAME     = 0x45,  // Set animation frame
  SET_SPEED     = 0x46,  // Set animation speed
  SET_LOOP      = 0x47,  // Set loop mode
  DESTROY       = 0x48,  // Destroy animation
  LIST          = 0x49,  // List active animations
  TRANSITION    = 0x4A,  // Screen transition effect
};

// ============================================================
// Script Commands (0x50-0x5F)
// ============================================================

enum class ScriptCmd : uint8_t {
  UPLOAD        = 0x50,  // Upload script to GPU
  EXECUTE       = 0x51,  // Execute script by ID
  STOP          = 0x52,  // Stop running script
  DELETE        = 0x53,  // Delete script from GPU
  LIST          = 0x54,  // List stored scripts
  SET_VAR       = 0x55,  // Set script variable
  GET_VAR       = 0x56,  // Get script variable
  CALL_FUNC     = 0x57,  // Call script function
  INLINE        = 0x58,  // Execute inline script
};

// ============================================================
// File Commands (0x60-0x6F)
// ============================================================

enum class FileCmd : uint8_t {
  UPLOAD_START  = 0x60,  // Start file upload
  UPLOAD_DATA   = 0x61,  // Upload file data chunk
  UPLOAD_END    = 0x62,  // End file upload
  DOWNLOAD_REQ  = 0x63,  // Request file download
  DELETE        = 0x64,  // Delete file from GPU
  LIST          = 0x65,  // List files on GPU
  INFO          = 0x66,  // Get file info
  FORMAT        = 0x67,  // Format GPU storage
  FREE_SPACE    = 0x68,  // Get free space
};

// ============================================================
// Buffer Commands (0x70-0x7F)
// ============================================================

enum class BufferCmd : uint8_t {
  CLEAR         = 0x70,  // Clear buffer
  SWAP          = 0x71,  // Swap double buffer
  SET_LAYER     = 0x72,  // Set active layer
  BLEND_LAYERS  = 0x73,  // Blend layers together
  COPY          = 0x74,  // Copy buffer
  FILL          = 0x75,  // Fill buffer with color
  SET_CLIP      = 0x76,  // Set clipping rectangle
  CLEAR_CLIP    = 0x77,  // Clear clipping
  LOCK          = 0x78,  // Lock buffer for multi-command
  UNLOCK        = 0x79,  // Unlock and display
};

// ============================================================
// Effect Commands (0x80-0x8F)
// ============================================================

enum class EffectCmd : uint8_t {
  FADE          = 0x80,  // Fade in/out
  SCROLL        = 0x81,  // Scroll effect
  SHAKE         = 0x82,  // Shake effect
  BLUR          = 0x83,  // Blur effect
  PIXELATE      = 0x84,  // Pixelate effect
  INVERT        = 0x85,  // Invert colors
  RAINBOW       = 0x86,  // Rainbow cycle effect
  PLASMA        = 0x87,  // Plasma effect
  FIRE          = 0x88,  // Fire effect
  MATRIX        = 0x89,  // Matrix rain effect
  PARTICLES     = 0x8A,  // Particle system
  WAVE          = 0x8B,  // Wave distortion
};

// ============================================================
// Query Commands (0x90-0x9F)
// ============================================================

enum class QueryCmd : uint8_t {
  FPS           = 0x90,  // Get current FPS
  MEMORY        = 0x91,  // Get memory usage
  SPRITES       = 0x92,  // Get sprite info
  ANIMATIONS    = 0x93,  // Get animation status
  ERRORS        = 0x94,  // Get error log
  DISPLAY_INFO  = 0x95,  // Get display info
  PERFORMANCE   = 0x96,  // Get performance stats
};

// ============================================================
// Color Formats
// ============================================================

enum class ColorFormat : uint8_t {
  RGB888        = 0x00,  // 24-bit RGB (3 bytes)
  RGB565        = 0x01,  // 16-bit RGB (2 bytes)
  MONO          = 0x02,  // 1-bit monochrome
  GRAYSCALE     = 0x03,  // 8-bit grayscale
  PALETTE       = 0x04,  // Indexed color (palette)
  RGBA8888      = 0x05,  // 32-bit RGBA (4 bytes)
};

// ============================================================
// Text Alignment
// ============================================================

enum class TextAlign : uint8_t {
  LEFT          = 0x00,
  CENTER        = 0x01,
  RIGHT         = 0x02,
  TOP           = 0x00,
  MIDDLE        = 0x10,
  BOTTOM        = 0x20,
};

// ============================================================
// Animation Loop Modes
// ============================================================

enum class LoopMode : uint8_t {
  ONCE          = 0x00,  // Play once
  LOOP          = 0x01,  // Loop forever
  PING_PONG     = 0x02,  // Play forward then backward
  REVERSE       = 0x03,  // Play in reverse
};

// ============================================================
// Blend Modes
// ============================================================

enum class BlendMode : uint8_t {
  NORMAL        = 0x00,  // Normal (replace)
  ADD           = 0x01,  // Additive
  SUBTRACT      = 0x02,  // Subtractive
  MULTIPLY      = 0x03,  // Multiply
  SCREEN        = 0x04,  // Screen
  OVERLAY       = 0x05,  // Overlay
  XOR           = 0x06,  // XOR
  ALPHA         = 0x07,  // Alpha blend
};

// ============================================================
// Data Structures
// ============================================================

#pragma pack(push, 1)

// Packet header (12 bytes)
struct PacketHeader {
  uint8_t sync1;        // SYNC_BYTE_1 (0xAA)
  uint8_t sync2;        // SYNC_BYTE_2 (0x55)
  uint8_t sync3;        // SYNC_BYTE_3 (0xCC)
  uint8_t version;      // Protocol version
  uint8_t category;     // Command category
  uint8_t command;      // Command ID
  uint8_t display;      // Target display
  uint8_t flags;        // Flags (reserved)
  uint16_t payload_len; // Payload length
  uint16_t seq_num;     // Sequence number
};

// Packet footer (3 bytes)
struct PacketFooter {
  uint16_t checksum;    // Payload checksum
  uint8_t end_byte;     // End marker (0x55)
};

// Color structures
struct ColorRGB {
  uint8_t r, g, b;
  
  constexpr ColorRGB() : r(0), g(0), b(0) {}
  constexpr ColorRGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
  
  static constexpr ColorRGB fromHex(uint32_t hex) {
    return ColorRGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
  }
  
  constexpr uint16_t toRGB565() const {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
};

struct ColorRGBA {
  uint8_t r, g, b, a;
  
  ColorRGBA() : r(0), g(0), b(0), a(255) {}
  ColorRGBA(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) 
    : r(r_), g(g_), b(b_), a(a_) {}
};

// Point structure
struct Point {
  int16_t x, y;
  
  Point() : x(0), y(0) {}
  Point(int16_t x_, int16_t y_) : x(x_), y(y_) {}
};

// Rectangle structure
struct Rect {
  int16_t x, y;
  uint16_t w, h;
  
  Rect() : x(0), y(0), w(0), h(0) {}
  Rect(int16_t x_, int16_t y_, uint16_t w_, uint16_t h_) 
    : x(x_), y(y_), w(w_), h(h_) {}
};

// ============================================================
// Command Payloads
// ============================================================

// Draw pixel
struct CmdPixel {
  int16_t x, y;
  ColorRGB color;
};

// Draw line
struct CmdLine {
  int16_t x0, y0;
  int16_t x1, y1;
  ColorRGB color;
  uint8_t thickness;
};

// Draw rectangle
struct CmdRect {
  int16_t x, y;
  uint16_t w, h;
  ColorRGB color;
  uint8_t thickness;  // 0 = filled
};

// Draw circle
struct CmdCircle {
  int16_t cx, cy;
  uint16_t radius;
  ColorRGB color;
  uint8_t thickness;  // 0 = filled
};

// Draw text
struct CmdText {
  int16_t x, y;
  uint8_t font_id;
  uint8_t scale;
  ColorRGB color;
  uint8_t align;
  uint8_t str_len;
  // char str[str_len] follows
};

// Draw sprite
struct CmdSprite {
  uint8_t sprite_id;
  int16_t x, y;
  uint8_t frame;
  uint8_t flags;  // flip, rotate, etc.
};

// Load sprite header
struct CmdLoadSprite {
  uint8_t sprite_id;
  uint16_t width, height;
  uint8_t frames;
  uint8_t format;  // ColorFormat
  uint32_t data_size;
  // pixel data follows
};

// Animation create
struct CmdAnimCreate {
  uint8_t anim_id;
  uint8_t sprite_id;
  uint8_t start_frame;
  uint8_t end_frame;
  uint16_t frame_delay_ms;
  uint8_t loop_mode;
};

// Script upload
struct CmdScriptUpload {
  uint8_t script_id;
  uint16_t script_len;
  // script data follows
};

// File upload start
struct CmdFileStart {
  uint8_t file_type;  // 0=sprite, 1=font, 2=script, 3=data
  uint32_t file_size;
  uint16_t name_len;
  // filename follows
};

// File upload data chunk
struct CmdFileData {
  uint32_t offset;
  uint16_t chunk_len;
  // data follows
};

// Buffer clear
struct CmdBufferClear {
  ColorRGB color;
  uint8_t layer;
};

// Effect parameters
struct CmdEffect {
  uint8_t effect_type;
  uint16_t duration_ms;
  uint8_t intensity;
  uint8_t param1;
  uint8_t param2;
};

// GPU status response
struct GpuStatus {
  uint32_t uptime_ms;
  uint8_t hub75_fps;
  uint8_t oled_fps;
  uint8_t cpu_usage;
  uint8_t memory_usage;
  uint16_t frames_rendered;
  uint16_t errors;
  uint8_t sprites_loaded;
  uint8_t animations_active;
};

// GPU capabilities
struct GpuCapabilities {
  uint8_t protocol_version;
  uint16_t hub75_width;
  uint16_t hub75_height;
  uint16_t oled_width;
  uint16_t oled_height;
  uint8_t max_sprites;
  uint8_t max_animations;
  uint8_t max_layers;
  uint32_t free_memory;
  uint32_t storage_size;
};

#pragma pack(pop)

// ============================================================
// Utility Functions
// ============================================================

inline uint16_t calculateChecksum(const uint8_t* data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
  }
  return sum;
}

inline bool validatePacketHeader(const PacketHeader& hdr) {
  return hdr.sync1 == SYNC_BYTE_1 && 
         hdr.sync2 == SYNC_BYTE_2 && 
         hdr.sync3 == SYNC_BYTE_3 &&
         hdr.version == PROTOCOL_VERSION;
}

// ============================================================
// Predefined Colors (C++11 compatible)
// ============================================================

namespace Colors {
  // Use explicit constructor calls for strict C++11 compatibility
  constexpr ColorRGB Black   = ColorRGB(0, 0, 0);
  constexpr ColorRGB White   = ColorRGB(255, 255, 255);
  constexpr ColorRGB Red     = ColorRGB(255, 0, 0);
  constexpr ColorRGB Green   = ColorRGB(0, 255, 0);
  constexpr ColorRGB Blue    = ColorRGB(0, 0, 255);
  constexpr ColorRGB Yellow  = ColorRGB(255, 255, 0);
  constexpr ColorRGB Cyan    = ColorRGB(0, 255, 255);
  constexpr ColorRGB Magenta = ColorRGB(255, 0, 255);
  constexpr ColorRGB Orange  = ColorRGB(255, 165, 0);
  constexpr ColorRGB Purple  = ColorRGB(128, 0, 128);
  constexpr ColorRGB Pink    = ColorRGB(255, 192, 203);
  constexpr ColorRGB Gray    = ColorRGB(128, 128, 128);
}

} // namespace gpu

#endif // GPU_DRIVER_BASE_API_HPP_
