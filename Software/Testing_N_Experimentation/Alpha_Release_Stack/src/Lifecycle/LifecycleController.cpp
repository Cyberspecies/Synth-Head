/**
 * @file LifecycleController.cpp
 * @brief Simplified lifecycle controller
 */

#include "LifecycleController.hpp"
#include "../Modes/BootMode.hpp"
#include "../Modes/CurrentMode.hpp"
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_system.h"

namespace Lifecycle {

//=============================================================================
// Helper Functions
//=============================================================================

static uint32_t getMillis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void delayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

//=============================================================================
// Global Instance
//=============================================================================

static LifecycleController* s_instance = nullptr;

LifecycleController& getLifecycle() {
    if (!s_instance) {
        s_instance = new LifecycleController();
    }
    return *s_instance;
}

//=============================================================================
// LifecycleController Implementation
//=============================================================================

LifecycleController::LifecycleController() {
    s_instance = this;
}

LifecycleController::~LifecycleController() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool LifecycleController::init() {
    m_bootTime = getMillis();
    
    // Initialize buttons for startup mode detection
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << m_btnA) | (1ULL << m_btnB) |
                           (1ULL << m_btnC) | (1ULL << m_btnD);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    
    return true;
}

bool LifecycleController::isButtonPressed(gpio_num_t pin) {
    return gpio_get_level(pin) == 0;  // Active LOW
}

bool LifecycleController::anyButtonPressed() {
    return isButtonPressed(m_btnA) || isButtonPressed(m_btnB) ||
           isButtonPressed(m_btnC) || isButtonPressed(m_btnD);
}

void LifecycleController::waitForButtonRelease() {
    while (anyButtonPressed()) {
        delayMs(50);
    }
    delayMs(100);  // Debounce
}

void LifecycleController::setBootMode(Modes::IBootMode* bootMode) {
    m_bootMode = bootMode;
}

void LifecycleController::setCurrentMode(Modes::ICurrentMode* currentMode) {
    m_currentMode = currentMode;
}

void LifecycleController::run() {
    printf("\n");
    printf("========================================\n");
    printf("  CPU Lifecycle Controller Starting\n");
    printf("========================================\n\n");
    
    // Check startup buttons
    delayMs(100);  // Let buttons settle
    
    bool btnA = isButtonPressed(m_btnA);
    bool btnD = isButtonPressed(m_btnD);
    
    if (btnA && btnD) {
        printf("  [A+D] System Test Loop Mode\n");
        m_systemTestMode = true;
        m_debugMode = true;
    } else if (btnA) {
        printf("  [A] Debug Menu Mode\n");
        m_debugMode = true;
    } else {
        printf("  Normal Boot\n");
    }
    
    // Wait for button release
    waitForButtonRelease();
    
    // Run boot mode
    if (m_bootMode) {
        printf("\n--- Running Boot Mode ---\n");
        if (m_debugMode) {
            m_bootMode->onDebugBoot();
        }
        if (!m_bootMode->onBoot()) {
            printf("[ERROR] Boot mode failed!\n");
        }
    }
    
    // Run appropriate mode
    if (m_debugMode) {
        runDebugMenu();
    } else if (m_currentMode) {
        printf("\n--- Running Current Mode ---\n");
        m_currentMode->onStart();
        
        uint32_t lastTime = getMillis();
        while (true) {
            uint32_t now = getMillis();
            uint32_t deltaMs = now - lastTime;
            lastTime = now;
            
            m_currentMode->onUpdate(deltaMs);
            delayMs(10);
        }
    }
}

//=============================================================================
// Debug Menu System (Simplified)
//=============================================================================

void LifecycleController::runDebugMenu() {
    printf("\n");
    printf("========================================\n");
    printf("  Debug Menu\n");
    printf("  A=Up  B=Select  C=Down  D=Exit\n");
    printf("========================================\n\n");
    
    const char* menuItems[] = {
        "1. System Info",
        "2. Button Test",
        "3. Reboot",
        "4. Exit to Normal Mode"
    };
    const int menuCount = 4;
    int selection = 0;
    
    bool lastA = false, lastB = false, lastC = false, lastD = false;
    
    while (true) {
        // Display menu
        printf("\033[2J\033[H");  // Clear screen
        printf("+------------------------------------+\n");
        printf("|         DEBUG MENU                 |\n");
        printf("+------------------------------------+\n");
        for (int i = 0; i < menuCount; i++) {
            if (i == selection) {
                printf("| > %-32s |\n", menuItems[i]);
            } else {
                printf("|   %-32s |\n", menuItems[i]);
            }
        }
        printf("+------------------------------------+\n");
        
        // Wait for input
        while (true) {
            bool currA = isButtonPressed(m_btnA);
            bool currB = isButtonPressed(m_btnB);
            bool currC = isButtonPressed(m_btnC);
            bool currD = isButtonPressed(m_btnD);
            
            // A = Up
            if (currA && !lastA) {
                selection = (selection > 0) ? selection - 1 : menuCount - 1;
                lastA = currA; lastB = currB; lastC = currC; lastD = currD;
                break;
            }
            
            // C = Down
            if (currC && !lastC) {
                selection = (selection < menuCount - 1) ? selection + 1 : 0;
                lastA = currA; lastB = currB; lastC = currC; lastD = currD;
                break;
            }
            
            // B = Select
            if (currB && !lastB) {
                lastA = currA; lastB = currB; lastC = currC; lastD = currD;
                waitForButtonRelease();
                
                switch (selection) {
                    case 0: showSystemInfo(); break;
                    case 1: showButtons(); break;
                    case 2: doReboot(); break;
                    case 3: 
                        printf("  Exiting to normal mode...\n");
                        m_debugMode = false;
                        if (m_currentMode) {
                            m_currentMode->onStart();
                            uint32_t lastTime = getMillis();
                            while (true) {
                                uint32_t now = getMillis();
                                m_currentMode->onUpdate(now - lastTime);
                                lastTime = now;
                                delayMs(10);
                            }
                        }
                        return;
                }
                
                printf("\n  Press any button to continue...\n");
                while (!anyButtonPressed()) delayMs(50);
                waitForButtonRelease();
                break;
            }
            
            // D = Exit
            if (currD && !lastD) {
                printf("  Exiting debug menu...\n");
                return;
            }
            
            lastA = currA; lastB = currB; lastC = currC; lastD = currD;
            delayMs(50);
        }
    }
}

//=============================================================================
// Debug Actions (Simplified)
//=============================================================================

void LifecycleController::showSystemInfo() {
    printf("\033[2J\033[H");
    printf("  System Information\n\n");
    
    printf("  SystemAPI Version: 2.0.0\n");
    printf("  Boot time: %lu ms ago\n", (unsigned long)(getMillis() - m_bootTime));
    printf("  Free heap: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("\n  Button GPIOs:\n");
    printf("    A=GPIO%d B=GPIO%d C=GPIO%d D=GPIO%d\n",
           m_btnA, m_btnB, m_btnC, m_btnD);
}

void LifecycleController::showButtons() {
    printf("\033[2J\033[H");
    printf("  Buttons (hold any 1s to stop)...\n\n");
    waitForButtonRelease();
    
    int holdCount = 0;
    while (holdCount < 20) {
        bool a = isButtonPressed(m_btnA);
        bool b = isButtonPressed(m_btnB);
        bool c = isButtonPressed(m_btnC);
        bool d = isButtonPressed(m_btnD);
        
        printf("  A: %s  B: %s  C: %s  D: %s\r",
               a ? "PRESSED" : "-------",
               b ? "PRESSED" : "-------",
               c ? "PRESSED" : "-------",
               d ? "PRESSED" : "-------");
        fflush(stdout);
        
        if (a || b || c || d) holdCount++;
        else holdCount = 0;
        
        delayMs(50);
    }
    printf("\n");
}

void LifecycleController::doReboot() {
    printf("  Rebooting in 3 seconds...\n");
    delayMs(3000);
    esp_restart();
}

} // namespace Lifecycle
