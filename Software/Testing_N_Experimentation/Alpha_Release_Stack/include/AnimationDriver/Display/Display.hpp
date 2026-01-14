/*****************************************************************
 * @file Display.hpp
 * @brief Master include for Display system
 *****************************************************************/

#pragma once

// Display types and configuration
#include "DisplayTypes.hpp"

// Display buffer for pixel storage
#include "DisplayBuffer.hpp"

// Virtual display for combining multiple physical displays
#include "VirtualDisplay.hpp"

// Central display manager
#include "DisplayManager.hpp"

namespace AnimationDriver {

/*****************************************************************
 * Display System Architecture
 * =============================
 * 
 * The display system provides unified coordinate handling for
 * multiple physical displays:
 * 
 * HUB75 Combined Display (128x32)
 * --------------------------------
 * Two 64x32 HUB75 panels treated as one display:
 * 
 *   +------------------+------------------+
 *   |   LEFT PANEL     |   RIGHT PANEL    |
 *   |   (0-63, 0-31)   |   (64-127, 0-31) |
 *   +------------------+------------------+
 *   
 * Virtual coordinates: x: 0-127, y: 0-31
 * Local left panel:    x: 0-63,  y: 0-31
 * Local right panel:   x: 0-63,  y: 0-31 (offset +64 in virtual)
 * 
 * OLED Display (128x128) - Separate System
 * ----------------------------------------
 * Independent coordinate system:
 * 
 *   +------------------+
 *   |                  |
 *   |   OLED DISPLAY   |
 *   |   (0-127, 0-127) |
 *   |                  |
 *   +------------------+
 * 
 * Usage Example
 * -------------
 * 
 *   DisplayManager display;
 *   
 *   // Initialize displays
 *   display.initHub75Combined();
 *   display.initOled();
 *   
 *   // Set hardware output
 *   display.setOutput(&myHardwareDriver);
 *   
 *   // Draw on HUB75 (full 128x32 coordinate space)
 *   display.hub75Clear();
 *   display.hub75FillCircle(64, 16, 10, Color::red());  // Center of combined
 *   display.hub75DrawLine(0, 0, 127, 31, Color::blue()); // Across both panels
 *   
 *   // Draw on OLED (independent 128x128 space)
 *   display.oledClear(Color::black());
 *   display.oledFillCircle(64, 64, 30, Color::white());
 *   
 *   // Flush to hardware
 *   display.flushAll();
 * 
 * Panel-Specific Operations
 * -------------------------
 * When you need to know which physical panel:
 * 
 *   int panel = display.hub75GetPanel(virtualX);  // 0=left, 1=right
 *   
 *   int localX, localY;
 *   if (display.hub75ToLocal(virtualX, virtualY, panel, localX, localY)) {
 *       // localX, localY are in panel-local coordinates (0-63, 0-31)
 *   }
 * 
 *****************************************************************/

} // namespace AnimationDriver
