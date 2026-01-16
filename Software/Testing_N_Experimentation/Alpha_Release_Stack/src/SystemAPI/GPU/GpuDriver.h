/*****************************************************************
 * GpuDriver.h - CPU-side GPU Command Driver
 * 
 * High-level API for communicating with the GPU via UART.
 * Based on working code from CPU_SpriteDemo and CPU_PolygonDemo.
 * 
 * Features:
 * - Simple drawing primitives (pixels, lines, rectangles, circles)
 * - Weighted pixel rendering (anti-aliasing) for smooth animations
 * - Sub-pixel precision using 8.8 fixed-point coordinates
 * - Sprite upload and blitting with optional smoothing
 * - Automatic keep-alive polling
 * - Thread-safe command sending
 * 
 * Weighted Pixels (Anti-Aliasing):
 *   When enabled (default), drawing operations use sub-pixel precision.
 *   The GPU calculates pixel coverage to control opacity, reducing aliasing
 *   and creating smoother motion for moving sprites and vectors.
 * 
 * Usage:
 *   GpuDriver gpu;
 *   gpu.init();  // Weighted pixels ON by default
 *   gpu.clear(0, 0, 0);
 *   gpu.drawLine(10, 5, 100, 28, Color::Red());  // Auto anti-aliased!
 *   gpu.drawLineF(10.5f, 5.25f, 100.75f, 28.3f, Color::Green()); // Explicit float
 *   gpu.blitSpriteF(0, 45.5f, 12.25f);  // Sub-pixel positioned sprite
 *   gpu.present();
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace SystemAPI {

// ============== Configuration ==============
struct GpuConfig {
    uart_port_t uartPort = UART_NUM_1;
    gpio_num_t txPin = GPIO_NUM_12;
    gpio_num_t rxPin = GPIO_NUM_11;
    int baudRate = 10000000;  // 10 Mbps
    int rxBufferSize = 1024;
    int txBufferSize = 1024;
    int gpuBootDelayMs = 500;  // Wait for GPU to boot
    bool weightedPixels = true;  // Enable anti-aliased rendering by default
};

// ============== Command Types ==============
enum class GpuCommand : uint8_t {
    // System
    NOP             = 0x00,
    PING            = 0xF0,
    PONG            = 0xF1,
    RESET           = 0xFF,
    
    // Shaders
    UPLOAD_SHADER   = 0x10,
    DELETE_SHADER   = 0x11,
    EXEC_SHADER     = 0x12,
    
    // Sprites
    UPLOAD_SPRITE   = 0x20,
    DELETE_SPRITE   = 0x21,
    
    // Variables
    SET_VAR         = 0x30,
    SET_VARS        = 0x31,
    
    // Drawing (Integer)
    DRAW_PIXEL      = 0x40,
    DRAW_LINE       = 0x41,
    DRAW_RECT       = 0x42,
    DRAW_FILL       = 0x43,  // Filled rectangle
    DRAW_CIRCLE     = 0x44,
    DRAW_POLY       = 0x45,
    BLIT_SPRITE     = 0x46,
    CLEAR           = 0x47,
    
    // Drawing (Float 8.8 fixed point) - Anti-aliased/weighted pixel rendering
    DRAW_LINE_F     = 0x48,  // Line with sub-pixel precision (anti-aliased)
    DRAW_CIRCLE_F   = 0x49,  // Circle with sub-pixel precision
    DRAW_RECT_F     = 0x4A,  // Rectangle (outline) with sub-pixel precision
    DRAW_FILL_F     = 0x4B,  // Filled rectangle with AA edges
    BLIT_SPRITE_F   = 0x4C,  // Sprite blit with sub-pixel position
    BLIT_SPRITE_ROT = 0x4D,  // Sprite blit with rotation angle
    SET_AA          = 0x4E,  // Toggle anti-aliasing (0=off, 1=on)
    
    // Target Control
    SET_TARGET      = 0x50,
    PRESENT         = 0x51,
    
    // OLED Specific
    OLED_CLEAR      = 0x60,
    OLED_LINE       = 0x61,
    OLED_RECT       = 0x62,
    OLED_FILL       = 0x63,
    OLED_CIRCLE     = 0x64,
    OLED_PRESENT    = 0x65,
};

// ============== Render Targets ==============
enum class GpuTarget : uint8_t {
    HUB75 = 0,  // 128x32 RGB LED matrix
    OLED  = 1,  // 128x128 monochrome
};

// ============== Sprite Format ==============
enum class SpriteFormat : uint8_t {
    RGB888 = 0,  // 3 bytes per pixel (R, G, B)
    MONO1BPP = 1, // 1 bit per pixel (packed)
};

// ============== Color Helper ==============
struct Color {
    uint8_t r, g, b;
    
    Color() : r(0), g(0), b(0) {}
    Color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    
    // Common colors
    static Color Black()   { return Color(0, 0, 0); }
    static Color White()   { return Color(255, 255, 255); }
    static Color Red()     { return Color(255, 0, 0); }
    static Color Green()   { return Color(0, 255, 0); }
    static Color Blue()    { return Color(0, 0, 255); }
    static Color Yellow()  { return Color(255, 255, 0); }
    static Color Cyan()    { return Color(0, 255, 255); }
    static Color Magenta() { return Color(255, 0, 255); }
    static Color Orange()  { return Color(255, 128, 0); }
};

// ============== GPU Driver Class ==============
class GpuDriver {
public:
    GpuDriver() = default;
    ~GpuDriver();
    
    // ===== Initialization =====
    bool init(const GpuConfig& config = GpuConfig());
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    
    // ===== Target Control =====
    void setTarget(GpuTarget target);
    void present();
    
    // ===== Screen Operations =====
    void clear(uint8_t r, uint8_t g, uint8_t b);
    void clear(const Color& c) { clear(c.r, c.g, c.b); }
    
    // ===== Weighted Pixel Mode (Anti-Aliasing) =====
    // When enabled, integer drawing operations internally use float precision for smooth edges
    // This also enables GPU-side bilinear filtering for sprite rotation
    void setWeightedPixels(bool enabled) { m_weightedPixels = enabled; syncAntiAliasingState(); }
    bool getWeightedPixels() const { return m_weightedPixels; }
    
    // Direct control of GPU-side anti-aliasing (called automatically by setWeightedPixels)
    void setAntiAliasing(bool enabled);
    
    // ===== Drawing Primitives (Integer coordinates) =====
    // When weightedPixels is enabled (default), these automatically use anti-aliased rendering
    void drawPixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    void drawPixel(int16_t x, int16_t y, const Color& c) { drawPixel(x, y, c.r, c.g, c.b); }
    
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b);
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, const Color& c) { drawLine(x1, y1, x2, y2, c.r, c.g, c.b); }
    
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, const Color& c) { drawRect(x, y, w, h, c.r, c.g, c.b); }
    
    void drawFilledRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
    void drawFilledRect(int16_t x, int16_t y, int16_t w, int16_t h, const Color& c) { drawFilledRect(x, y, w, h, c.r, c.g, c.b); }
    
    void drawCircle(int16_t cx, int16_t cy, int16_t radius, uint8_t r, uint8_t g, uint8_t b);
    void drawCircle(int16_t cx, int16_t cy, int16_t radius, const Color& c) { drawCircle(cx, cy, radius, c.r, c.g, c.b); }
    
    // ===== Drawing Primitives - Float/Anti-Aliased (Sub-pixel precision) =====
    // These use 8.8 fixed-point coordinates for sub-pixel precision
    // GPU calculates pixel coverage for smooth edges and motion
    // Use these explicitly when you need precise float positioning
    void drawLineF(float x1, float y1, float x2, float y2, uint8_t r, uint8_t g, uint8_t b);
    void drawLineF(float x1, float y1, float x2, float y2, const Color& c) { drawLineF(x1, y1, x2, y2, c.r, c.g, c.b); }
    
    void drawRectF(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b);
    void drawRectF(float x, float y, float w, float h, const Color& c) { drawRectF(x, y, w, h, c.r, c.g, c.b); }
    
    void drawFilledRectF(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b);
    void drawFilledRectF(float x, float y, float w, float h, const Color& c) { drawFilledRectF(x, y, w, h, c.r, c.g, c.b); }
    
    void drawCircleF(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b);
    void drawCircleF(float cx, float cy, float radius, const Color& c) { drawCircleF(cx, cy, radius, c.r, c.g, c.b); }
    
    // ===== Polygon Drawing =====
    // Vertices are passed as arrays of x and y coordinates
    void drawFilledPolygon(const int16_t* xPoints, const int16_t* yPoints, uint8_t numVertices,
                           uint8_t r, uint8_t g, uint8_t b);
    void drawFilledPolygon(const int16_t* xPoints, const int16_t* yPoints, uint8_t numVertices, const Color& c) {
        drawFilledPolygon(xPoints, yPoints, numVertices, c.r, c.g, c.b);
    }
    
    // ===== Sprite Operations =====
    // Upload sprite to GPU memory (cached until deleted or reset)
    bool uploadSprite(uint8_t spriteId, uint8_t width, uint8_t height, 
                      const uint8_t* pixelData, SpriteFormat format = SpriteFormat::RGB888);
    
    // Delete sprite from GPU memory
    void deleteSprite(uint8_t spriteId);
    
    // Draw sprite at position (sprite must be uploaded first)
    void blitSprite(uint8_t spriteId, int16_t x, int16_t y);
    
    // Float version - sub-pixel positioning for smooth sprite movement
    void blitSpriteF(uint8_t spriteId, float x, float y);
    
    // Rotated sprite - rotation around center with optional anti-aliased bilinear interpolation
    // angleDegrees: rotation in degrees (clockwise)
    // Uses GPU-side transformation matrix and bilinear sampling when AA is enabled
    void blitSpriteRotated(uint8_t spriteId, float x, float y, float angleDegrees);
    void blitSpriteRotated(uint8_t spriteId, int16_t x, int16_t y, float angleDegrees) {
        blitSpriteRotated(spriteId, (float)x, (float)y, angleDegrees);
    }
    
    // ===== OLED Specific (always targets OLED) =====
    void oledClear();
    void oledPresent();
    void oledDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool on = true);
    void oledDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true);
    void oledDrawFilledRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true);
    void oledDrawCircle(int16_t cx, int16_t cy, int16_t radius, bool on = true);
    
    // ===== Variables (for shaders/polygons) =====
    void setVar(uint8_t index, int16_t value);
    void setVars(uint8_t startIndex, const int16_t* values, uint8_t count);
    
    // ===== System Commands =====
    bool ping(uint32_t timeoutMs = 100);  // Returns true if GPU responds
    void reset();
    void nop();  // No operation (keep-alive)
    
    // ===== Keep-Alive =====
    // Start background task that sends periodic commands to prevent GPU timeout
    void startKeepAlive(uint32_t intervalMs = 1000);
    void stopKeepAlive();
    bool isKeepAliveRunning() const { return m_keepAliveRunning; }
    
    // ===== Low-Level Access =====
    // Send raw command (for advanced use)
    void sendCommand(GpuCommand cmd, const uint8_t* payload = nullptr, uint16_t length = 0);
    
    // Read response from GPU (for PONG, CONFIG_RESPONSE, etc.)
    int readResponse(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 100);

private:
    static constexpr uint8_t SYNC_BYTE_0 = 0xAA;
    static constexpr uint8_t SYNC_BYTE_1 = 0x55;
    static constexpr char TAG[] = "GpuDriver";
    
    bool m_initialized = false;
    GpuConfig m_config;
    SemaphoreHandle_t m_mutex = nullptr;
    
    // Keep-alive task
    TaskHandle_t m_keepAliveTask = nullptr;
    volatile bool m_keepAliveRunning = false;
    uint32_t m_keepAliveInterval = 1000;
    
    // Weighted pixel mode (anti-aliasing)
    bool m_weightedPixels = true;  // Enabled by default
    
    // Float to 8.8 fixed-point conversion helper
    static void floatToFixed88(float val, uint8_t* outFrac, uint8_t* outInt);
    
    // Sync AA state with GPU
    void syncAntiAliasingState();
    
    static void keepAliveTaskFunc(void* param);
    
    // Helper to acquire mutex
    class MutexLock {
    public:
        MutexLock(SemaphoreHandle_t mutex) : m_mutex(mutex) {
            if (m_mutex) xSemaphoreTake(m_mutex, portMAX_DELAY);
        }
        ~MutexLock() {
            if (m_mutex) xSemaphoreGive(m_mutex);
        }
    private:
        SemaphoreHandle_t m_mutex;
    };
};

} // namespace SystemAPI
