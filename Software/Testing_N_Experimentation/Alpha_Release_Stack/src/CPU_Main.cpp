/**
 * @file CPU_Main.cpp
 * @brief Main entry point for CPU program
 * 
 * This file should remain minimal - just lifecycle startup.
 * All application logic goes in Boot mode and Current mode files.
 * 
 * STARTUP MODES:
 * - Hold A+D during power-on = System Test Loop Mode
 * - Hold A only during power-on = Debug Menu Mode
 * - No buttons = Normal Boot -> Current Mode
 * 
 * DEBUG MENU CONTROLS:
 * - Button A = Previous (navigate up)
 * - Button B = Select/Enter
 * - Button C = Next (navigate down)
 * - Button D = Cancel/Back
 */

#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Lifecycle
#include "Lifecycle/LifecycleController.hpp"

// Application modes
#include "Modes/BootMode.hpp"
#include "Modes/CurrentMode.hpp"

static const char* TAG = "CPU_MAIN";

//=============================================================================
// Application Modes
//=============================================================================

// Create your custom modes here by extending BootMode/CurrentMode
// or implementing IBootMode/ICurrentMode directly

static Modes::BootMode g_bootMode;
static Modes::CurrentMode g_currentMode;

//=============================================================================
// Main Entry Point
//=============================================================================

extern "C" void app_main(void) {
    printf("\n\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║               SYNTH-HEAD CPU STARTING                    ║\n");
    printf("║                                                          ║\n");
    printf("║  Hold A+D at boot = System Test Loop                     ║\n");
    printf("║  Hold A at boot   = Debug Menu                           ║\n");
    printf("║  No buttons       = Normal Operation                     ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    // Get lifecycle controller
    auto& lifecycle = Lifecycle::getLifecycle();
    
    // Initialize lifecycle
    if (!lifecycle.init()) {
        ESP_LOGE(TAG, "Failed to initialize lifecycle controller!");
        return;
    }
    
    // Register application modes
    lifecycle.setBootMode(&g_bootMode);
    lifecycle.setCurrentMode(&g_currentMode);
    
    // Run lifecycle (this blocks forever)
    lifecycle.run();
}
