/**
 * @file CurrentMode.cpp
 * @brief Current mode implementation using SystemAPI
 * 
 * SystemAPI includes all layers: HAL, BaseAPI, FrameworkAPI
 * Use the appropriate layer for your needs.
 */

#include "CurrentMode.hpp"
#include "SystemAPI/SystemAPI.hpp"
#include <cstdio>

namespace Modes {

//=============================================================================
// CurrentMode Implementation
//=============================================================================

void CurrentMode::onStart() {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║        CURRENT MODE STARTED        ║\n");
    printf("  ╚════════════════════════════════════╝\n\n");
    
    m_updateCount = 0;
    m_totalTime = 0;
}

void CurrentMode::onUpdate(uint32_t deltaMs) {
    m_updateCount++;
    m_totalTime += deltaMs;
    
    // TODO: Read sensors via HAL layer
    // Example: HAL::IMU::read(), HAL::Environmental::update(), etc.
    
    // Example: Print status every second
    if (m_totalTime >= 1000) {
        printf("  Update #%lu | deltaMs=%lu\n",
               (unsigned long)m_updateCount,
               (unsigned long)deltaMs);
        m_totalTime = 0;
    }
}

void CurrentMode::onStop() {
    printf("  Current mode stopped after %lu updates\n", (unsigned long)m_updateCount);
}

} // namespace Modes
