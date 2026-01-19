/**
 * @file FanDriver.hpp
 * @brief Fan Driver - Simple GPIO On/Off Control
 * 
 * Controls two cooling fans via GPIO pins.
 */

#pragma once

#include <cstdint>

namespace Drivers {

/**
 * @brief Fan Driver for GPIO-controlled cooling fans
 * 
 * Features:
 * - Simple on/off control for two fans
 * - State change tracking to minimize GPIO writes
 */
namespace FanDriver {
    
    //=========================================================================
    // Configuration
    //=========================================================================
    
    /// Fan GPIO pins
    constexpr int FAN_1_PIN = 17;
    constexpr int FAN_2_PIN = 36;
    
    //=========================================================================
    // API
    //=========================================================================
    
    /**
     * @brief Initialize fan GPIO pins
     * @return true if initialization succeeded
     */
    bool init();
    
    /**
     * @brief Update fan state based on enabled flag
     * 
     * Only changes GPIO if state has changed to minimize I/O overhead.
     * 
     * @param enabled true to turn fans on, false to turn off
     */
    void update(bool enabled);
    
    /**
     * @brief Check if fans are currently on
     * @return true if fans are running
     */
    bool isOn();
    
    /**
     * @brief Check if fan driver is initialized
     * @return true if initialized
     */
    bool isInitialized();

} // namespace FanDriver

} // namespace Drivers
