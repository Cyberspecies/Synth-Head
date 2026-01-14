/*****************************************************************
 * @file VirtualDisplay.hpp
 * @brief Virtual display that combines multiple physical displays
 *****************************************************************/

#pragma once

#include "DisplayTypes.hpp"
#include "DisplayBuffer.hpp"

namespace AnimationDriver {

// ============================================================
// Physical Display Info
// ============================================================

struct PhysicalDisplay {
    DisplayConfig config;
    
    // Local buffer offset in virtual coordinate space
    int virtualX;
    int virtualY;
    
    // Enabled for rendering
    bool active;
    
    PhysicalDisplay() : virtualX(0), virtualY(0), active(true) {}
};

// ============================================================
// Virtual Display - Manages combined coordinate space
// ============================================================

template<int VWIDTH, int VHEIGHT, int MAX_PHYSICAL = 4>
class VirtualDisplay {
public:
    VirtualDisplay() : _physicalCount(0), _dirty(true) {
        _buffer.clear();
    }
    
    // Add a physical display to this virtual display
    bool addPhysical(const DisplayConfig& config, int virtualX, int virtualY) {
        if (_physicalCount >= MAX_PHYSICAL) return false;
        
        PhysicalDisplay& pd = _physicals[_physicalCount++];
        pd.config = config;
        pd.virtualX = virtualX;
        pd.virtualY = virtualY;
        pd.active = config.enabled;
        
        return true;
    }
    
    // Set pixel in virtual coordinate space
    void setPixel(int x, int y, const Color& color) {
        _buffer.setPixel(x, y, color);
    }
    
    // Get pixel from virtual coordinate space
    Color getPixel(int x, int y) const {
        return _buffer.getPixel(x, y);
    }
    
    // Clear entire virtual display
    void clear() {
        _buffer.clear();
    }
    
    void clear(const Color& color) {
        _buffer.clear(color);
    }
    
    // Drawing primitives (delegate to buffer)
    void drawHLine(int x, int y, int w, const Color& color) {
        _buffer.drawHLine(x, y, w, color);
    }
    
    void drawVLine(int x, int y, int h, const Color& color) {
        _buffer.drawVLine(x, y, h, color);
    }
    
    void fillRect(int x, int y, int w, int h, const Color& color) {
        _buffer.fillRect(x, y, w, h, color);
    }
    
    void drawRect(int x, int y, int w, int h, const Color& color) {
        _buffer.drawRect(x, y, w, h, color);
    }
    
    void drawCircle(int cx, int cy, int r, const Color& color) {
        _buffer.drawCircle(cx, cy, r, color);
    }
    
    void fillCircle(int cx, int cy, int r, const Color& color) {
        _buffer.fillCircle(cx, cy, r, color);
    }
    
    void drawLine(int x0, int y0, int x1, int y1, const Color& color) {
        _buffer.drawLine(x0, y0, x1, y1, color);
    }
    
    // Convert virtual coordinates to physical display coordinates
    bool virtualToPhysical(int vx, int vy, int& physIdx, int& px, int& py) const {
        for (int i = 0; i < _physicalCount; i++) {
            const PhysicalDisplay& pd = _physicals[i];
            if (!pd.active) continue;
            
            int localX = vx - pd.virtualX;
            int localY = vy - pd.virtualY;
            
            if (localX >= 0 && localX < pd.config.width &&
                localY >= 0 && localY < pd.config.height) {
                
                physIdx = i;
                
                // Apply rotation
                switch (pd.config.rotation) {
                    case 90:
                        px = localY;
                        py = pd.config.width - 1 - localX;
                        break;
                    case 180:
                        px = pd.config.width - 1 - localX;
                        py = pd.config.height - 1 - localY;
                        break;
                    case 270:
                        px = pd.config.height - 1 - localY;
                        py = localX;
                        break;
                    default: // 0
                        px = localX;
                        py = localY;
                        break;
                }
                
                // Apply flip
                if (pd.config.flipX) {
                    px = pd.config.width - 1 - px;
                }
                if (pd.config.flipY) {
                    py = pd.config.height - 1 - py;
                }
                
                return true;
            }
        }
        return false;
    }
    
    // Get physical display by index
    const PhysicalDisplay* getPhysical(int index) const {
        return (index >= 0 && index < _physicalCount) ? &_physicals[index] : nullptr;
    }
    
    PhysicalDisplay* getPhysical(int index) {
        return (index >= 0 && index < _physicalCount) ? &_physicals[index] : nullptr;
    }
    
    // Enable/disable physical display
    void setPhysicalActive(int index, bool active) {
        if (index >= 0 && index < _physicalCount) {
            _physicals[index].active = active;
        }
    }
    
    // Get local coordinates within a physical display
    bool getLocalCoordinates(int vx, int vy, int physIdx, int& localX, int& localY) const {
        if (physIdx < 0 || physIdx >= _physicalCount) return false;
        
        const PhysicalDisplay& pd = _physicals[physIdx];
        localX = vx - pd.virtualX;
        localY = vy - pd.virtualY;
        
        return (localX >= 0 && localX < pd.config.width &&
                localY >= 0 && localY < pd.config.height);
    }
    
    // Extract region for a specific physical display
    template<int PW, int PH>
    void extractRegion(int physIdx, DisplayBuffer<PW, PH>& dest) const {
        if (physIdx < 0 || physIdx >= _physicalCount) return;
        
        const PhysicalDisplay& pd = _physicals[physIdx];
        
        for (int y = 0; y < PH && y < pd.config.height; y++) {
            for (int x = 0; x < PW && x < pd.config.width; x++) {
                int vx = pd.virtualX + x;
                int vy = pd.virtualY + y;
                
                if (vx >= 0 && vx < VWIDTH && vy >= 0 && vy < VHEIGHT) {
                    Color c = _buffer.getPixelFast(vx, vy);
                    
                    // Apply transformations
                    int dx = x, dy = y;
                    
                    switch (pd.config.rotation) {
                        case 90:
                            dx = y;
                            dy = pd.config.width - 1 - x;
                            break;
                        case 180:
                            dx = pd.config.width - 1 - x;
                            dy = pd.config.height - 1 - y;
                            break;
                        case 270:
                            dx = pd.config.height - 1 - y;
                            dy = x;
                            break;
                        default:
                            break;
                    }
                    
                    if (pd.config.flipX) dx = pd.config.width - 1 - dx;
                    if (pd.config.flipY) dy = pd.config.height - 1 - dy;
                    
                    dest.setPixel(dx, dy, c);
                }
            }
        }
    }
    
    // Access buffer directly
    DisplayBuffer<VWIDTH, VHEIGHT>& buffer() { return _buffer; }
    const DisplayBuffer<VWIDTH, VHEIGHT>& buffer() const { return _buffer; }
    
    // Dirty flag
    bool isDirty() const { return _buffer.isDirty(); }
    void markDirty() { _buffer.markDirty(); }
    void clearDirty() { _buffer.clearDirty(); }
    
    // Dimensions
    int width() const { return VWIDTH; }
    int height() const { return VHEIGHT; }
    int physicalCount() const { return _physicalCount; }

private:
    DisplayBuffer<VWIDTH, VHEIGHT> _buffer;
    PhysicalDisplay _physicals[MAX_PHYSICAL];
    int _physicalCount;
    bool _dirty;
};

// ============================================================
// Common virtual display configurations
// ============================================================

// Combined HUB75 display (128x32 from two 64x32 panels)
using CombinedHub75Display = VirtualDisplay<128, 32, 2>;

// Single OLED display
using OledDisplay = VirtualDisplay<128, 128, 1>;

} // namespace AnimationDriver
