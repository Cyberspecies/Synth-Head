/*****************************************************************
 * @file OledHandler.hpp
 * @brief OLED Handler - High-level OLED Drawing API
 * 
 * This handler provides a convenient high-level API for drawing
 * on the 128x128 monochrome OLED display. It builds on top of
 * OledBaseDriver and GpuCommands to provide easy-to-use functions.
 * 
 * Features:
 *   - Text rendering with multiple scales
 *   - Shape drawing (lines, rectangles, circles, triangles)
 *   - UI widgets (progress bars, buttons, checkboxes)
 *   - Animation helpers
 *   - Screen layout utilities
 * 
 * Usage:
 *   GpuCommands gpu;
 *   gpu.init();
 *   
 *   OledHandler oled;
 *   oled.init(&gpu);
 *   
 *   oled.clear();
 *   oled.drawText(10, 10, "Hello World!");
 *   oled.drawRoundedRect(20, 30, 80, 40, 5);
 *   oled.present();
 * 
 * @author ARCOS Framework
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "Drivers/OledBaseDriver.hpp"
#include "GpuDriver/GpuCommands.hpp"

namespace Drivers {

/**
 * @brief OLED Handler for high-level display operations
 * 
 * Provides a convenient API for common OLED drawing tasks including
 * text rendering, shapes, and UI widgets.
 */
class OledHandler {
public:
    //=========================================================================
    // Constants
    //=========================================================================
    
    static constexpr int16_t WIDTH = OledBaseDriver::WIDTH;
    static constexpr int16_t HEIGHT = OledBaseDriver::HEIGHT;
    
    // Text alignment options
    enum class TextAlign {
        LEFT,
        CENTER,
        RIGHT
    };
    
    //=========================================================================
    // Initialization
    //=========================================================================
    
    OledHandler() : gpu_(nullptr), initialized_(false) {}
    
    /**
     * @brief Initialize with GpuCommands instance
     * @param gpu Pointer to initialized GpuCommands
     * @return true if successful
     */
    bool init(GpuCommands* gpu) {
        if (!gpu || !gpu->isInitialized()) {
            return false;
        }
        gpu_ = gpu;
        if (!baseDriver_.init(gpu)) {
            return false;
        }
        initialized_ = true;
        return true;
    }
    
    bool isInitialized() const { return initialized_; }
    
    //=========================================================================
    // Basic Operations
    //=========================================================================
    
    /** Clear the display buffer */
    void clear() {
        if (!initialized_) return;
        baseDriver_.clear();
    }
    
    /** Push buffer to display */
    void present() {
        if (!initialized_) return;
        baseDriver_.present();
    }
    
    /** Fill entire screen */
    void fill(bool on = true) {
        if (!initialized_) return;
        baseDriver_.fill(on);
    }
    
    //=========================================================================
    // Pixel Operations
    //=========================================================================
    
    void drawPixel(int16_t x, int16_t y, bool on = true) {
        baseDriver_.drawPixel(x, y, on);
    }
    
    //=========================================================================
    // Line Drawing
    //=========================================================================
    
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool on = true) {
        baseDriver_.drawLine(x1, y1, x2, y2, on);
    }
    
    void drawHLine(int16_t x, int16_t y, int16_t length, bool on = true) {
        baseDriver_.drawHLine(x, y, length, on);
    }
    
    void drawVLine(int16_t x, int16_t y, int16_t length, bool on = true) {
        baseDriver_.drawVLine(x, y, length, on);
    }
    
    /**
     * @brief Draw a dashed line
     */
    void drawDashedLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                        int dashLen = 4, int gapLen = 2, bool on = true) {
        if (!initialized_) return;
        
        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx * dx + dy * dy);
        if (length < 1) return;
        
        float nx = dx / length;
        float ny = dy / length;
        float total = dashLen + gapLen;
        float pos = 0;
        
        while (pos < length) {
            float dashEnd = pos + dashLen;
            if (dashEnd > length) dashEnd = length;
            
            int16_t sx = (int16_t)(x1 + nx * pos);
            int16_t sy = (int16_t)(y1 + ny * pos);
            int16_t ex = (int16_t)(x1 + nx * dashEnd);
            int16_t ey = (int16_t)(y1 + ny * dashEnd);
            
            baseDriver_.drawLine(sx, sy, ex, ey, on);
            pos += total;
        }
    }
    
    //=========================================================================
    // Rectangle Drawing
    //=========================================================================
    
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true) {
        baseDriver_.drawRect(x, y, w, h, on);
    }
    
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true) {
        baseDriver_.fillRect(x, y, w, h, on);
    }
    
    /**
     * @brief Draw a rounded rectangle outline
     */
    void drawRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h, 
                         int16_t r, bool on = true) {
        if (!initialized_) return;
        if (r > w / 2) r = w / 2;
        if (r > h / 2) r = h / 2;
        
        // Top and bottom edges
        baseDriver_.drawHLine(x + r, y, w - 2 * r, on);
        baseDriver_.drawHLine(x + r, y + h - 1, w - 2 * r, on);
        
        // Left and right edges
        baseDriver_.drawVLine(x, y + r, h - 2 * r, on);
        baseDriver_.drawVLine(x + w - 1, y + r, h - 2 * r, on);
        
        // Corners using circle quadrants
        drawCorner(x + r, y + r, r, 1, on);           // Top-left
        drawCorner(x + w - r - 1, y + r, r, 2, on);   // Top-right
        drawCorner(x + r, y + h - r - 1, r, 4, on);   // Bottom-left
        drawCorner(x + w - r - 1, y + h - r - 1, r, 8, on); // Bottom-right
    }
    
    /**
     * @brief Draw a filled rounded rectangle
     */
    void fillRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h, 
                         int16_t r, bool on = true) {
        if (!initialized_) return;
        if (r > w / 2) r = w / 2;
        if (r > h / 2) r = h / 2;
        
        // Center rectangle
        baseDriver_.fillRect(x + r, y, w - 2 * r, h, on);
        
        // Side rectangles
        baseDriver_.fillRect(x, y + r, r, h - 2 * r, on);
        baseDriver_.fillRect(x + w - r, y + r, r, h - 2 * r, on);
        
        // Filled corners
        fillCorner(x + r, y + r, r, 1, on);
        fillCorner(x + w - r - 1, y + r, r, 2, on);
        fillCorner(x + r, y + h - r - 1, r, 4, on);
        fillCorner(x + w - r - 1, y + h - r - 1, r, 8, on);
    }
    
    //=========================================================================
    // Circle Drawing
    //=========================================================================
    
    void drawCircle(int16_t cx, int16_t cy, int16_t r, bool on = true) {
        baseDriver_.drawCircle(cx, cy, r, on);
    }
    
    void fillCircle(int16_t cx, int16_t cy, int16_t r, bool on = true) {
        baseDriver_.fillCircle(cx, cy, r, on);
    }
    
    /**
     * @brief Draw an arc (portion of circle outline)
     */
    void drawArc(int16_t cx, int16_t cy, int16_t r, 
                 float startAngle, float endAngle, bool on = true) {
        if (!initialized_) return;
        
        // Convert to radians
        float start = startAngle * 3.14159f / 180.0f;
        float end = endAngle * 3.14159f / 180.0f;
        
        // Draw arc using line segments
        int segments = (int)(r * fabsf(end - start) / 4);
        if (segments < 8) segments = 8;
        
        float step = (end - start) / segments;
        float angle = start;
        
        int16_t prevX = cx + (int16_t)(r * cosf(angle));
        int16_t prevY = cy + (int16_t)(r * sinf(angle));
        
        for (int i = 1; i <= segments; i++) {
            angle += step;
            int16_t x = cx + (int16_t)(r * cosf(angle));
            int16_t y = cy + (int16_t)(r * sinf(angle));
            baseDriver_.drawLine(prevX, prevY, x, y, on);
            prevX = x;
            prevY = y;
        }
    }
    
    //=========================================================================
    // Triangle Drawing
    //=========================================================================
    
    /**
     * @brief Draw a triangle outline
     */
    void drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                      int16_t x3, int16_t y3, bool on = true) {
        if (!initialized_) return;
        baseDriver_.drawLine(x1, y1, x2, y2, on);
        baseDriver_.drawLine(x2, y2, x3, y3, on);
        baseDriver_.drawLine(x3, y3, x1, y1, on);
    }
    
    /**
     * @brief Draw a filled triangle using scanline algorithm
     */
    void fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                      int16_t x3, int16_t y3, bool on = true) {
        if (!initialized_) return;
        
        // Sort vertices by Y coordinate
        if (y1 > y2) { swap(x1, x2); swap(y1, y2); }
        if (y1 > y3) { swap(x1, x3); swap(y1, y3); }
        if (y2 > y3) { swap(x2, x3); swap(y2, y3); }
        
        // Fill using horizontal lines
        for (int16_t y = y1; y <= y3; y++) {
            int16_t xa, xb;
            
            if (y < y2) {
                xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1 != 0 ? y2 - y1 : 1);
                xb = x1 + (x3 - x1) * (y - y1) / (y3 - y1 != 0 ? y3 - y1 : 1);
            } else {
                xa = x2 + (x3 - x2) * (y - y2) / (y3 - y2 != 0 ? y3 - y2 : 1);
                xb = x1 + (x3 - x1) * (y - y1) / (y3 - y1 != 0 ? y3 - y1 : 1);
            }
            
            if (xa > xb) swap(xa, xb);
            baseDriver_.drawHLine(xa, y, xb - xa + 1, on);
        }
    }
    
    //=========================================================================
    // Text Rendering
    //=========================================================================
    
    /**
     * @brief Draw text at position
     * @param x X position
     * @param y Y position
     * @param text Text string
     * @param scale Font scale (1 = 5x7, 2 = 10x14, etc.)
     * @param on Pixel state
     */
    void drawText(int16_t x, int16_t y, const char* text, int scale = 1, bool on = true) {
        if (!initialized_ || !text) return;
        gpu_->oledText(x, y, text, scale, on);
    }
    
    /**
     * @brief Draw text centered horizontally
     */
    void drawTextCentered(int16_t y, const char* text, int scale = 1, bool on = true) {
        if (!initialized_ || !text) return;
        gpu_->oledTextCentered(y, text, scale, on);
    }
    
    /**
     * @brief Draw text with alignment
     */
    void drawTextAligned(int16_t x, int16_t y, const char* text, 
                         TextAlign align, int scale = 1, bool on = true) {
        if (!initialized_ || !text) return;
        
        int w = textWidth(text, scale);
        int16_t drawX = x;
        
        switch (align) {
            case TextAlign::CENTER:
                drawX = x - w / 2;
                break;
            case TextAlign::RIGHT:
                drawX = x - w;
                break;
            default:
                break;
        }
        
        gpu_->oledText(drawX, y, text, scale, on);
    }
    
    /**
     * @brief Draw integer value
     */
    void drawInt(int16_t x, int16_t y, int value, int scale = 1, bool on = true) {
        if (!initialized_) return;
        gpu_->oledInt(x, y, value, scale, on);
    }
    
    /**
     * @brief Draw float value with specified precision
     */
    void drawFloat(int16_t x, int16_t y, float value, int decimals = 2, 
                   int scale = 1, bool on = true) {
        if (!initialized_) return;
        char buf[16];
        snprintf(buf, sizeof(buf), "%.*f", decimals, value);
        gpu_->oledText(x, y, buf, scale, on);
    }
    
    /**
     * @brief Get text width in pixels
     */
    int textWidth(const char* text, int scale = 1) {
        if (!text) return 0;
        return gpu_->textWidth(text, scale);
    }
    
    /**
     * @brief Get text height in pixels (based on scale)
     */
    int textHeight(int scale = 1) {
        return 7 * scale;  // 5x7 font
    }
    
    //=========================================================================
    // UI Widgets
    //=========================================================================
    
    /**
     * @brief Draw a progress bar
     * @param x X position
     * @param y Y position
     * @param w Width
     * @param h Height
     * @param value Progress (0-100)
     */
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, int value) {
        if (!initialized_) return;
        gpu_->oledProgressBar(x, y, w, h, value);
    }
    
    /**
     * @brief Draw a button with text
     */
    void drawButton(int16_t x, int16_t y, const char* text, bool selected = false) {
        if (!initialized_) return;
        gpu_->oledButton(x, y, text, selected);
    }
    
    /**
     * @brief Draw a checkbox
     */
    void drawCheckbox(int16_t x, int16_t y, bool checked, const char* label = nullptr) {
        if (!initialized_) return;
        gpu_->oledCheckbox(x, y, checked, label);
    }
    
    /**
     * @brief Draw a horizontal slider
     */
    void drawSlider(int16_t x, int16_t y, int16_t w, int value) {
        if (!initialized_) return;
        
        // Track
        baseDriver_.fillRect(x, y + 2, w, 5, true);
        
        // Handle position
        int handleX = x + (w - 8) * value / 100;
        baseDriver_.fillRect(handleX, y, 8, 9, false);
        baseDriver_.drawRect(handleX, y, 8, 9, true);
    }
    
    /**
     * @brief Draw a titled frame/box
     */
    void drawFrame(int16_t x, int16_t y, int16_t w, int16_t h, const char* title = nullptr) {
        if (!initialized_) return;
        
        baseDriver_.drawRect(x, y, w, h, true);
        
        if (title && strlen(title) > 0) {
            int tw = textWidth(title, 1) + 4;
            int tx = x + 4;
            
            // Clear area for title
            baseDriver_.fillRect(tx - 1, y, tw, 1, false);
            
            // Draw title
            gpu_->oledText(tx, y - 3, title, 1, true);
        }
    }
    
    //=========================================================================
    // Pattern Drawing
    //=========================================================================
    
    /**
     * @brief Fill area with checkerboard pattern
     */
    void drawCheckerboard(int16_t x, int16_t y, int16_t w, int16_t h, int size = 4) {
        if (!initialized_) return;
        
        for (int16_t py = y; py < y + h; py += size) {
            for (int16_t px = x; px < x + w; px += size) {
                bool on = ((px / size + py / size) % 2) == 0;
                int16_t rw = (px + size > x + w) ? (x + w - px) : size;
                int16_t rh = (py + size > y + h) ? (y + h - py) : size;
                baseDriver_.fillRect(px, py, rw, rh, on);
            }
        }
    }
    
    /**
     * @brief Draw crosshatch pattern
     */
    void drawCrosshatch(int16_t x, int16_t y, int16_t w, int16_t h, int spacing = 8) {
        if (!initialized_) return;
        
        // Diagonal lines (forward slash direction)
        for (int i = -h; i < w; i += spacing) {
            int16_t x1 = x + i;
            int16_t y1 = y + h;
            int16_t x2 = x + i + h;
            int16_t y2 = y;
            baseDriver_.drawLine(x1, y1, x2, y2, true);
        }
        
        // Diagonal lines (backslash direction)
        for (int i = 0; i < w + h; i += spacing) {
            int16_t x1 = x + i;
            int16_t y1 = y;
            int16_t x2 = x + i - h;
            int16_t y2 = y + h;
            baseDriver_.drawLine(x1, y1, x2, y2, true);
        }
    }
    
    //=========================================================================
    // Accessors
    //=========================================================================
    
    OledBaseDriver* getBaseDriver() { return &baseDriver_; }
    GpuCommands* getGpu() { return gpu_; }
    int16_t getWidth() const { return WIDTH; }
    int16_t getHeight() const { return HEIGHT; }
    
private:
    GpuCommands* gpu_;
    OledBaseDriver baseDriver_;
    bool initialized_;
    
    // Helper to swap values
    template<typename T>
    void swap(T& a, T& b) {
        T temp = a;
        a = b;
        b = temp;
    }
    
    // Draw corner arc for rounded rectangles
    void drawCorner(int16_t cx, int16_t cy, int16_t r, uint8_t corner, bool on) {
        int16_t f = 1 - r;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * r;
        int16_t x = 0;
        int16_t y = r;
        
        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;
            
            if (corner & 1) { // Top-left
                baseDriver_.drawPixel(cx - y, cy - x, on);
                baseDriver_.drawPixel(cx - x, cy - y, on);
            }
            if (corner & 2) { // Top-right
                baseDriver_.drawPixel(cx + x, cy - y, on);
                baseDriver_.drawPixel(cx + y, cy - x, on);
            }
            if (corner & 4) { // Bottom-left
                baseDriver_.drawPixel(cx - y, cy + x, on);
                baseDriver_.drawPixel(cx - x, cy + y, on);
            }
            if (corner & 8) { // Bottom-right
                baseDriver_.drawPixel(cx + x, cy + y, on);
                baseDriver_.drawPixel(cx + y, cy + x, on);
            }
        }
    }
    
    // Fill corner for rounded rectangles
    void fillCorner(int16_t cx, int16_t cy, int16_t r, uint8_t corner, bool on) {
        int16_t f = 1 - r;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * r;
        int16_t x = 0;
        int16_t y = r;
        
        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;
            
            if (corner & 1) { // Top-left
                baseDriver_.drawVLine(cx - x, cy - y, y, on);
                baseDriver_.drawHLine(cx - y, cy - x, y, on);
            }
            if (corner & 2) { // Top-right
                baseDriver_.drawVLine(cx + x, cy - y, y, on);
                baseDriver_.drawHLine(cx + 1, cy - x, y, on);
            }
            if (corner & 4) { // Bottom-left
                baseDriver_.drawVLine(cx - x, cy + 1, y, on);
                baseDriver_.drawHLine(cx - y, cy + x, y, on);
            }
            if (corner & 8) { // Bottom-right
                baseDriver_.drawVLine(cx + x, cy + 1, y, on);
                baseDriver_.drawHLine(cx + 1, cy + x, y, on);
            }
        }
    }
};

} // namespace Drivers
