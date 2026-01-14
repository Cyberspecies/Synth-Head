/*****************************************************************
 * @file DisplayManager.hpp
 * @brief Central manager for all displays in the system
 *****************************************************************/

#pragma once

#include "DisplayTypes.hpp"
#include "DisplayBuffer.hpp"
#include "VirtualDisplay.hpp"

namespace AnimationDriver {

// ============================================================
// Display Output Interface
// ============================================================

// Interface for sending pixel data to actual hardware
class IDisplayOutput {
public:
    virtual ~IDisplayOutput() = default;
    
    // Send buffer data to hardware
    virtual void flush(DisplayId id, const Color* pixels, int width, int height) = 0;
    
    // Check if display is ready
    virtual bool isReady(DisplayId id) const = 0;
    
    // Get display info
    virtual bool getInfo(DisplayId id, DisplayConfig& config) const = 0;
};

// ============================================================
// Display Manager
// ============================================================

class DisplayManager {
public:
    static constexpr int MAX_DISPLAYS = 4;
    
    // Virtual display dimensions
    static constexpr int HUB75_COMBINED_WIDTH = 128;
    static constexpr int HUB75_COMBINED_HEIGHT = 32;
    static constexpr int OLED_WIDTH = 128;
    static constexpr int OLED_HEIGHT = 128;
    
    DisplayManager() : _output(nullptr), _hub75Initialized(false), _oledInitialized(false) {
    }
    
    // Set output interface (hardware driver)
    void setOutput(IDisplayOutput* output) {
        _output = output;
    }
    
    // --------------------------------------------------------
    // HUB75 Combined Display (both panels as one)
    // --------------------------------------------------------
    
    // Initialize HUB75 as combined display
    void initHub75Combined() {
        // Add left panel
        _hub75.addPhysical(
            DisplayConfig::Hub75Left(),
            0, 0  // Virtual position: left side
        );
        
        // Add right panel
        _hub75.addPhysical(
            DisplayConfig::Hub75Right(),
            64, 0  // Virtual position: right side
        );
        
        _hub75Initialized = true;
    }
    
    // Get HUB75 combined display for drawing
    CombinedHub75Display& hub75() { return _hub75; }
    const CombinedHub75Display& hub75() const { return _hub75; }
    
    // Draw to HUB75 using virtual coordinates (0-127, 0-31)
    void hub75SetPixel(int x, int y, const Color& color) {
        _hub75.setPixel(x, y, color);
    }
    
    // Get pixel from HUB75
    Color hub75GetPixel(int x, int y) const {
        return _hub75.getPixel(x, y);
    }
    
    // Clear HUB75 display
    void hub75Clear() {
        _hub75.clear();
    }
    
    void hub75Clear(const Color& color) {
        _hub75.clear(color);
    }
    
    // HUB75 drawing primitives
    void hub75FillRect(int x, int y, int w, int h, const Color& color) {
        _hub75.fillRect(x, y, w, h, color);
    }
    
    void hub75DrawRect(int x, int y, int w, int h, const Color& color) {
        _hub75.drawRect(x, y, w, h, color);
    }
    
    void hub75DrawCircle(int cx, int cy, int r, const Color& color) {
        _hub75.drawCircle(cx, cy, r, color);
    }
    
    void hub75FillCircle(int cx, int cy, int r, const Color& color) {
        _hub75.fillCircle(cx, cy, r, color);
    }
    
    void hub75DrawLine(int x0, int y0, int x1, int y1, const Color& color) {
        _hub75.drawLine(x0, y0, x1, y1, color);
    }
    
    // Get local coordinates for a specific panel
    // panel: 0 = left, 1 = right
    bool hub75ToLocal(int vx, int vy, int panel, int& localX, int& localY) const {
        return _hub75.getLocalCoordinates(vx, vy, panel, localX, localY);
    }
    
    // Check if coordinate is on left or right panel
    int hub75GetPanel(int vx) const {
        return (vx < 64) ? 0 : 1;
    }
    
    // --------------------------------------------------------
    // OLED Display (separate system)
    // --------------------------------------------------------
    
    // Initialize OLED display
    void initOled() {
        _oled.addPhysical(
            DisplayConfig::Oled128x128(),
            0, 0
        );
        _oledInitialized = true;
    }
    
    // Get OLED display for drawing
    OledDisplay& oled() { return _oled; }
    const OledDisplay& oled() const { return _oled; }
    
    // Draw to OLED
    void oledSetPixel(int x, int y, const Color& color) {
        _oled.setPixel(x, y, color);
    }
    
    Color oledGetPixel(int x, int y) const {
        return _oled.getPixel(x, y);
    }
    
    void oledClear() {
        _oled.clear();
    }
    
    void oledClear(const Color& color) {
        _oled.clear(color);
    }
    
    // OLED drawing primitives
    void oledFillRect(int x, int y, int w, int h, const Color& color) {
        _oled.fillRect(x, y, w, h, color);
    }
    
    void oledDrawRect(int x, int y, int w, int h, const Color& color) {
        _oled.drawRect(x, y, w, h, color);
    }
    
    void oledDrawCircle(int cx, int cy, int r, const Color& color) {
        _oled.drawCircle(cx, cy, r, color);
    }
    
    void oledFillCircle(int cx, int cy, int r, const Color& color) {
        _oled.fillCircle(cx, cy, r, color);
    }
    
    void oledDrawLine(int x0, int y0, int x1, int y1, const Color& color) {
        _oled.drawLine(x0, y0, x1, y1, color);
    }
    
    // --------------------------------------------------------
    // Flush to hardware
    // --------------------------------------------------------
    
    // Flush HUB75 to hardware
    void flushHub75() {
        if (!_output || !_hub75Initialized) return;
        
        if (_hub75.isDirty()) {
            // Extract each panel's region and send
            _hub75.extractRegion(0, _hub75LeftBuffer);
            _hub75.extractRegion(1, _hub75RightBuffer);
            
            _output->flush(DisplayId::HUB75_LEFT, 
                          _hub75LeftBuffer.data(), 64, 32);
            _output->flush(DisplayId::HUB75_RIGHT, 
                          _hub75RightBuffer.data(), 64, 32);
            
            _hub75.clearDirty();
        }
    }
    
    // Flush OLED to hardware
    void flushOled() {
        if (!_output || !_oledInitialized) return;
        
        if (_oled.isDirty()) {
            _output->flush(DisplayId::OLED_PRIMARY,
                          _oled.buffer().data(), 128, 128);
            _oled.clearDirty();
        }
    }
    
    // Flush all displays
    void flushAll() {
        flushHub75();
        flushOled();
    }
    
    // --------------------------------------------------------
    // Utility
    // --------------------------------------------------------
    
    bool isHub75Ready() const { 
        return _hub75Initialized && _output && _output->isReady(DisplayId::HUB75_COMBINED);
    }
    
    bool isOledReady() const {
        return _oledInitialized && _output && _output->isReady(DisplayId::OLED_PRIMARY);
    }

private:
    IDisplayOutput* _output;
    
    // HUB75 combined display
    CombinedHub75Display _hub75;
    Hub75Buffer _hub75LeftBuffer;
    Hub75Buffer _hub75RightBuffer;
    bool _hub75Initialized;
    
    // OLED display
    OledDisplay _oled;
    bool _oledInitialized;
};

} // namespace AnimationDriver
