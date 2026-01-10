/**
 * @file LifecycleController.hpp
 * @brief Main lifecycle controller for CPU program
 * 
 * Manages the overall program lifecycle including:
 * - Boot sequence
 * - Main loop execution
 * - Debug menu and console interface
 * 
 * NOTE: All hardware initialization is done through SystemAPI,
 * not directly by the LifecycleController.
 */

#pragma once

#include "driver/gpio.h"
#include <cstdint>

// Forward declarations
namespace Modes {
    class IBootMode;
    class ICurrentMode;
}

namespace Lifecycle {

//=============================================================================
// Lifecycle Controller
//=============================================================================

class LifecycleController {
public:
    LifecycleController();
    ~LifecycleController();
    
    /**
     * @brief Initialize the lifecycle controller
     * @return true if successful
     */
    bool init();
    
    /**
     * @brief Register boot mode handler
     */
    void setBootMode(Modes::IBootMode* bootMode);
    
    /**
     * @brief Register current mode handler
     */
    void setCurrentMode(Modes::ICurrentMode* currentMode);
    
    /**
     * @brief Run the lifecycle (blocks forever)
     * 
     * This handles:
     * 1. Check for debug mode entry (buttons at boot)
     * 2. Run boot mode
     * 3. Run current mode loop OR debug menu
     */
    void run();
    
    /**
     * @brief Check if in debug mode
     */
    bool isDebugMode() const { return m_debugMode; }
    
private:
    // Mode handlers
    Modes::IBootMode* m_bootMode = nullptr;
    Modes::ICurrentMode* m_currentMode = nullptr;
    
    // State
    bool m_debugMode = false;
    bool m_systemTestMode = false;
    uint32_t m_bootTime = 0;
    
    // Button GPIOs (from SystemAPI config)
    gpio_num_t m_btnA = GPIO_NUM_5;
    gpio_num_t m_btnB = GPIO_NUM_6;
    gpio_num_t m_btnC = GPIO_NUM_7;
    gpio_num_t m_btnD = GPIO_NUM_15;
    
    // Button helpers
    bool isButtonPressed(gpio_num_t pin);
    bool anyButtonPressed();
    void waitForButtonRelease();
    
    // Debug menu system
    void runDebugMenu();
    
    // Debug actions
    void showSystemInfo();
    void showButtons();
    void doReboot();
};

/**
 * @brief Get the global lifecycle controller instance
 */
LifecycleController& getLifecycle();

} // namespace Lifecycle
