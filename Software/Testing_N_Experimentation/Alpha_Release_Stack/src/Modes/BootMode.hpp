/**
 * @file BootMode.hpp
 * @brief Boot mode interface and default implementation
 * 
 * Boot mode handles one-time initialization tasks using SystemAPI:
 * - Hardware initialization via System::IMU, System::Environment, etc.
 * - Loading calibration data
 * - Setting up lookup tables
 * - Preparing resources for Current mode
 * 
 * Similar to Arduino's setup() function.
 * 
 * Usage:
 *     // All hardware access goes through SystemAPI
 *     System::IMU::init();
 *     System::Environment::init();
 *     System::GPS::init();
 *     System::Microphone::init();
 */

#pragma once

namespace Modes {

/**
 * @brief Interface for boot mode implementations
 */
class IBootMode {
public:
    virtual ~IBootMode() = default;
    virtual bool onBoot() = 0;
    virtual void onDebugBoot() = 0;
};

/**
 * @brief Default boot mode implementation
 * 
 * Uses SystemAPI for all hardware initialization.
 * Extend this class for custom boot behavior.
 */
class BootMode : public IBootMode {
public:
    BootMode() = default;
    virtual ~BootMode() = default;
    
    /**
     * @brief Main boot sequence using SystemAPI
     * 
     * Override to add custom initialization.
     */
    bool onBoot() override;
    
    /**
     * @brief Called when entering debug mode
     */
    void onDebugBoot() override;
};

} // namespace Modes
