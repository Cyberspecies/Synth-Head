/**
 * @file MicDriver.hpp
 * @brief Microphone Driver - I2S INMP441 with Rolling Average
 * 
 * Provides non-blocking audio level measurement with smoothed dB values.
 */

#pragma once

#include <cstdint>

namespace Drivers {

/**
 * @brief Microphone Driver for INMP441 I2S MEMS Microphone
 * 
 * Features:
 * - Non-blocking I2S audio capture
 * - Rolling average dB calculation for stable readings
 * - Level percentage (0-100) output
 */
namespace MicDriver {
    
    //=========================================================================
    // Configuration
    //=========================================================================
    
    /// Microphone I2S pins
    constexpr int MIC_WS_PIN = 42;    ///< Word Select (LRCLK)
    constexpr int MIC_BCK_PIN = 40;   ///< Bit Clock
    constexpr int MIC_DATA_PIN = 2;   ///< Data out
    constexpr int MIC_LR_PIN = 41;    ///< L/R Select (tie low for left)
    
    /// Rolling window size for averaging
    constexpr int WINDOW_SIZE = 16;
    
    //=========================================================================
    // Audio Data (Read-only access)
    //=========================================================================
    
    /// Average dB level (smoothed, use for display)
    extern float avgDb;
    
    /// Current instantaneous dB level (noisy, use for reactive effects)
    extern float currentDb;
    
    /// Level as percentage (0-100, derived from avgDb)
    extern uint8_t level;
    
    /// Whether the microphone is initialized
    extern bool initialized;
    
    //=========================================================================
    // API
    //=========================================================================
    
    /**
     * @brief Initialize I2S microphone interface
     * @return true if initialization succeeded
     */
    bool init();
    
    /**
     * @brief Non-blocking update - reads samples and updates rolling average
     * 
     * Call this frequently to get up-to-date audio levels.
     * This function is non-blocking and returns immediately if no data is available.
     */
    void update();
    
    /**
     * @brief Check if microphone is initialized
     * @return true if initialized
     */
    bool isInitialized();

} // namespace MicDriver

} // namespace Drivers
