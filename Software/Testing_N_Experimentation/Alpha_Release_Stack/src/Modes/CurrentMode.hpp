/**
 * @file CurrentMode.hpp
 * @brief Current mode interface and default implementation
 * 
 * Current mode is the main application loop using SystemAPI:
 * - Runs continuously after boot
 * - Reads sensors via System::IMU::read(), etc.
 * - Processes application logic
 * - Responds to user input via System::Buttons
 * 
 * Similar to Arduino's loop() function.
 * 
 * Usage:
 *     // All sensor reading goes through SystemAPI
 *     auto imu = System::IMU::read();
 *     float temp = System::Environment::getTemperature();
 *     System::GPS::update();
 *     auto btns = System::Buttons::read();
 */

#pragma once

#include <cstdint>

namespace Modes {

/**
 * @brief Interface for current mode implementations
 */
class ICurrentMode {
public:
    virtual ~ICurrentMode() = default;
    virtual void onStart() = 0;
    virtual void onUpdate(uint32_t deltaMs) = 0;
    virtual void onStop() = 0;
};

/**
 * @brief Default current mode implementation
 * 
 * Uses SystemAPI for all sensor access.
 * Extend this class for custom behavior.
 */
class CurrentMode : public ICurrentMode {
public:
    CurrentMode() = default;
    virtual ~CurrentMode() = default;
    
    /**
     * @brief Called once after boot, before loop starts
     */
    void onStart() override;
    
    /**
     * @brief Main update loop - called repeatedly
     * 
     * Override this for your main application logic.
     * Uses SystemAPI for all hardware access.
     * 
     * @param deltaMs Milliseconds since last update
     */
    void onUpdate(uint32_t deltaMs) override;
    
    /**
     * @brief Called when application is stopping
     */
    void onStop() override;
    
private:
    uint32_t m_updateCount = 0;
    uint32_t m_totalTime = 0;
    uint32_t m_credentialPrintTime = 0;
};

} // namespace Modes
