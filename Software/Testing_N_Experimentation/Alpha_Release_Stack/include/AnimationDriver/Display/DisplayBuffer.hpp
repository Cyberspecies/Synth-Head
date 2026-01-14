/*****************************************************************
 * @file DisplayBuffer.hpp
 * @brief Display buffer for pixel data with color support
 *****************************************************************/

#pragma once

#include "DisplayTypes.hpp"
#include "../Core/Color.hpp"
#include <cstring>
#include <algorithm>

namespace AnimationDriver {

// ============================================================
// Display Buffer - Stores pixel data
// ============================================================

template<int WIDTH, int HEIGHT>
class DisplayBuffer {
public:
    static constexpr int BUFFER_WIDTH = WIDTH;
    static constexpr int BUFFER_HEIGHT = HEIGHT;
    static constexpr int PIXEL_COUNT = WIDTH * HEIGHT;
    
    DisplayBuffer() : _dirty(true) {
        clear();
    }
    
    // Clear to black
    void clear() {
        memset(_pixels, 0, sizeof(_pixels));
        _dirty = true;
    }
    
    // Clear to specific color
    void clear(const Color& color) {
        for (int i = 0; i < PIXEL_COUNT; i++) {
            _pixels[i] = color;
        }
        _dirty = true;
    }
    
    // Set pixel (bounds checked)
    void setPixel(int x, int y, const Color& color) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            _pixels[y * WIDTH + x] = color;
            _dirty = true;
        }
    }
    
    // Get pixel (bounds checked)
    Color getPixel(int x, int y) const {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            return _pixels[y * WIDTH + x];
        }
        return Color::black();
    }
    
    // Set pixel (no bounds check - faster)
    void setPixelFast(int x, int y, const Color& color) {
        _pixels[y * WIDTH + x] = color;
        _dirty = true;
    }
    
    // Get pixel (no bounds check - faster)
    Color getPixelFast(int x, int y) const {
        return _pixels[y * WIDTH + x];
    }
    
    // Set pixel by index
    void setPixelIndex(int idx, const Color& color) {
        if (idx >= 0 && idx < PIXEL_COUNT) {
            _pixels[idx] = color;
            _dirty = true;
        }
    }
    
    // Draw horizontal line
    void drawHLine(int x, int y, int w, const Color& color) {
        int x1 = (x < 0) ? 0 : x;
        int x2 = (x + w > WIDTH) ? WIDTH : (x + w);
        if (y >= 0 && y < HEIGHT) {
            for (int px = x1; px < x2; px++) {
                _pixels[y * WIDTH + px] = color;
            }
            _dirty = true;
        }
    }
    
    // Draw vertical line
    void drawVLine(int x, int y, int h, const Color& color) {
        int y1 = (y < 0) ? 0 : y;
        int y2 = (y + h > HEIGHT) ? HEIGHT : (y + h);
        if (x >= 0 && x < WIDTH) {
            for (int py = y1; py < y2; py++) {
                _pixels[py * WIDTH + x] = color;
            }
            _dirty = true;
        }
    }
    
    // Draw filled rectangle
    void fillRect(int x, int y, int w, int h, const Color& color) {
        int x1 = (x < 0) ? 0 : x;
        int y1 = (y < 0) ? 0 : y;
        int x2 = (x + w > WIDTH) ? WIDTH : (x + w);
        int y2 = (y + h > HEIGHT) ? HEIGHT : (y + h);
        
        for (int py = y1; py < y2; py++) {
            for (int px = x1; px < x2; px++) {
                _pixels[py * WIDTH + px] = color;
            }
        }
        _dirty = true;
    }
    
    // Draw rectangle outline
    void drawRect(int x, int y, int w, int h, const Color& color) {
        drawHLine(x, y, w, color);
        drawHLine(x, y + h - 1, w, color);
        drawVLine(x, y, h, color);
        drawVLine(x + w - 1, y, h, color);
    }
    
    // Draw circle (midpoint algorithm)
    void drawCircle(int cx, int cy, int r, const Color& color) {
        int x = r;
        int y = 0;
        int err = 0;
        
        while (x >= y) {
            setPixel(cx + x, cy + y, color);
            setPixel(cx + y, cy + x, color);
            setPixel(cx - y, cy + x, color);
            setPixel(cx - x, cy + y, color);
            setPixel(cx - x, cy - y, color);
            setPixel(cx - y, cy - x, color);
            setPixel(cx + y, cy - x, color);
            setPixel(cx + x, cy - y, color);
            
            y++;
            err += 1 + 2 * y;
            if (2 * (err - x) + 1 > 0) {
                x--;
                err += 1 - 2 * x;
            }
        }
    }
    
    // Fill circle
    void fillCircle(int cx, int cy, int r, const Color& color) {
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) {
                    setPixel(cx + x, cy + y, color);
                }
            }
        }
    }
    
    // Draw line (Bresenham)
    void drawLine(int x0, int y0, int x1, int y1, const Color& color) {
        int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
        int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;
        
        while (true) {
            setPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
    
    // Copy from another buffer (with offset)
    template<int SW, int SH>
    void copyFrom(const DisplayBuffer<SW, SH>& src, int destX, int destY) {
        for (int sy = 0; sy < SH; sy++) {
            int dy = destY + sy;
            if (dy < 0 || dy >= HEIGHT) continue;
            
            for (int sx = 0; sx < SW; sx++) {
                int dx = destX + sx;
                if (dx < 0 || dx >= WIDTH) continue;
                
                setPixelFast(dx, dy, src.getPixelFast(sx, sy));
            }
        }
    }
    
    // Blend from another buffer (alpha composite)
    template<int SW, int SH>
    void blendFrom(const DisplayBuffer<SW, SH>& src, int destX, int destY, float alpha = 1.0f) {
        for (int sy = 0; sy < SH; sy++) {
            int dy = destY + sy;
            if (dy < 0 || dy >= HEIGHT) continue;
            
            for (int sx = 0; sx < SW; sx++) {
                int dx = destX + sx;
                if (dx < 0 || dx >= WIDTH) continue;
                
                Color srcColor = src.getPixelFast(sx, sy);
                Color destColor = getPixelFast(dx, dy);
                setPixelFast(dx, dy, destColor.blend(srcColor, alpha));
            }
        }
    }
    
    // Access raw data
    Color* data() { return _pixels; }
    const Color* data() const { return _pixels; }
    
    // Dirty flag management
    bool isDirty() const { return _dirty; }
    void markDirty() { _dirty = true; }
    void clearDirty() { _dirty = false; }
    
    // Get dimensions
    int width() const { return WIDTH; }
    int height() const { return HEIGHT; }
    int pixelCount() const { return PIXEL_COUNT; }

private:
    Color _pixels[PIXEL_COUNT];
    bool _dirty;
};

// ============================================================
// Common buffer sizes
// ============================================================

using Hub75Buffer = DisplayBuffer<64, 32>;           // Single HUB75 panel
using Hub75CombinedBuffer = DisplayBuffer<128, 32>;  // Combined HUB75 panels
using OledBuffer = DisplayBuffer<128, 128>;          // 128x128 OLED
using Oled64Buffer = DisplayBuffer<128, 64>;         // 128x64 OLED

} // namespace AnimationDriver
