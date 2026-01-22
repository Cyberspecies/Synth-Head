/*****************************************************************
 * @file OledBaseDriver.hpp
 * @brief OLED Base Driver - Low-level OLED Drawing Primitives
 * 
 * This driver provides the base layer for OLED display communication.
 * It sends commands to the GPU over UART, and the GPU handles the
 * actual I2C communication with the SH1107 OLED display.
 * 
 * Display Specifications:
 *   - Resolution: 128x128 pixels
 *   - Color: Monochrome (1-bit, on/off)
 *   - Controller: SH1107
 *   - Interface: I2C (via GPU)
 *   - Address: 0x3C
 * 
 * Architecture:
 *   CPU → UART → GPU → I2C → OLED
 * 
 * Usage:
 *   OledBaseDriver oled;
 *   oled.init();
 *   oled.clear();
 *   oled.drawLine(0, 0, 127, 127, true);
 *   oled.drawRect(10, 10, 50, 50, true);
 *   oled.present();
 * 
 * @author ARCOS Framework
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include "GpuDriver/GpuCommands.hpp"

namespace Drivers {

/**
 * @brief OLED Base Driver for low-level display operations
 * 
 * This class provides basic drawing primitives for the 128x128 monochrome
 * OLED display. It uses GpuCommands to send drawing commands to the GPU.
 */
class OledBaseDriver {
public:
    //=========================================================================
    // Constants
    //=========================================================================
    
    static constexpr int16_t WIDTH = 128;
    static constexpr int16_t HEIGHT = 128;
    
    //=========================================================================
    // Initialization
    //=========================================================================
    
    /**
     * @brief Default constructor
     */
    OledBaseDriver() : gpu_(nullptr), initialized_(false) {}
    
    /**
     * @brief Initialize the OLED driver with a GpuCommands instance
     * @param gpu Pointer to initialized GpuCommands instance
     * @return true if initialization succeeded
     */
    bool init(GpuCommands* gpu) {
        if (!gpu || !gpu->isInitialized()) {
            return false;
        }
        gpu_ = gpu;
        initialized_ = true;
        return true;
    }
    
    /**
     * @brief Check if driver is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized_; }
    
    //=========================================================================
    // Basic Drawing Primitives
    //=========================================================================
    
    /**
     * @brief Clear the entire OLED display buffer
     */
    void clear() {
        if (!initialized_) return;
        gpu_->oledClear();
    }
    
    /**
     * @brief Push the framebuffer to the display
     */
    void present() {
        if (!initialized_) return;
        gpu_->oledPresent();
    }
    
    /**
     * @brief Draw a single pixel
     * @param x X coordinate (0-127)
     * @param y Y coordinate (0-127)
     * @param on Pixel state (true = on, false = off)
     */
    void drawPixel(int16_t x, int16_t y, bool on = true) {
        if (!initialized_) return;
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
        gpu_->oledPixel(x, y, on);
    }
    
    /**
     * @brief Draw a line between two points
     * @param x1 Start X
     * @param y1 Start Y
     * @param x2 End X
     * @param y2 End Y
     * @param on Pixel state
     */
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool on = true) {
        if (!initialized_) return;
        gpu_->oledLine(x1, y1, x2, y2, on);
    }
    
    /**
     * @brief Draw a horizontal line (optimized)
     * @param x Start X
     * @param y Y coordinate
     * @param length Line length
     * @param on Pixel state
     */
    void drawHLine(int16_t x, int16_t y, int16_t length, bool on = true) {
        if (!initialized_) return;
        gpu_->oledHLine(x, y, length, on);
    }
    
    /**
     * @brief Draw a vertical line (optimized)
     * @param x X coordinate
     * @param y Start Y
     * @param length Line length
     * @param on Pixel state
     */
    void drawVLine(int16_t x, int16_t y, int16_t length, bool on = true) {
        if (!initialized_) return;
        gpu_->oledVLine(x, y, length, on);
    }
    
    /**
     * @brief Draw a rectangle outline
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param on Pixel state
     */
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true) {
        if (!initialized_) return;
        gpu_->oledRect(x, y, w, h, on);
    }
    
    /**
     * @brief Draw a filled rectangle
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param on Pixel state
     */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true) {
        if (!initialized_) return;
        gpu_->oledFill(x, y, w, h, on);
    }
    
    /**
     * @brief Draw a circle outline
     * @param cx Center X
     * @param cy Center Y
     * @param radius Radius
     * @param on Pixel state
     */
    void drawCircle(int16_t cx, int16_t cy, int16_t radius, bool on = true) {
        if (!initialized_) return;
        gpu_->oledCircle(cx, cy, radius, on);
    }
    
    /**
     * @brief Draw a filled circle
     * @param cx Center X
     * @param cy Center Y
     * @param radius Radius
     * @param on Pixel state
     */
    void fillCircle(int16_t cx, int16_t cy, int16_t radius, bool on = true) {
        if (!initialized_) return;
        gpu_->oledFillCircle(cx, cy, radius, on);
    }
    
    /**
     * @brief Fill the entire screen with a state
     * @param on Fill state (true = all pixels on, false = all off)
     */
    void fill(bool on = true) {
        if (!initialized_) return;
        gpu_->oledFill(0, 0, WIDTH, HEIGHT, on);
    }
    
    //=========================================================================
    // Accessors
    //=========================================================================
    
    /**
     * @brief Get display width
     * @return Width in pixels (128)
     */
    int16_t getWidth() const { return WIDTH; }
    
    /**
     * @brief Get display height
     * @return Height in pixels (128)
     */
    int16_t getHeight() const { return HEIGHT; }
    
    /**
     * @brief Get the underlying GpuCommands instance
     * @return Pointer to GpuCommands
     */
    GpuCommands* getGpu() { return gpu_; }
    
private:
    GpuCommands* gpu_;
    bool initialized_;
};

} // namespace Drivers
