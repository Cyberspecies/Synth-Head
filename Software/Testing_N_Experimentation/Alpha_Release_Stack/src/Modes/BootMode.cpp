/**
 * @file BootMode.cpp
 * @brief Boot mode implementation using SystemAPI
 * 
 * SystemAPI includes all layers: HAL, BaseAPI, FrameworkAPI
 * Use the appropriate layer for your needs.
 */

#include "BootMode.hpp"
#include "SystemAPI/SystemAPI.hpp"
#include <cstdio>

namespace Modes {

//=============================================================================
// BootMode Implementation
//=============================================================================

bool BootMode::onBoot() {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║          BOOT SEQUENCE             ║\n");
    printf("  ╚════════════════════════════════════╝\n\n");
    
    printf("  SystemAPI Version: %s\n\n", SystemAPI::VERSION);
    
    // TODO: Initialize sensors via HAL layer
    // Example: HAL::I2C, HAL::IMU, HAL::Environmental, etc.
    
    printf("  Boot complete!\n\n");
    
    // Boot succeeds even if some sensors missing
    return true;
}

void BootMode::onDebugBoot() {
    printf("  Debug Boot - Minimal initialization\n");
}

} // namespace Modes
