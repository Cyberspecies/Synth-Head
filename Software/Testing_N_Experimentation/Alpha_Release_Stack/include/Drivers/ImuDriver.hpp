/**
 * @file ImuDriver.hpp
 * @brief IMU Driver - ICM20948 I2C
 * 
 * Provides 6-axis IMU data (accelerometer and gyroscope) from ICM20948.
 */

#pragma once

#include <cstdint>

namespace Drivers {

/**
 * @brief IMU Driver for ICM20948 9-axis IMU
 * 
 * Features:
 * - I2C communication at 400kHz
 * - Accelerometer data in milli-g
 * - Gyroscope data in degrees/second
 * - Non-blocking updates
 */
namespace ImuDriver {
    
    //=========================================================================
    // Configuration
    //=========================================================================
    
    /// I2C pins
    constexpr int I2C_SDA_PIN = 9;
    constexpr int I2C_SCL_PIN = 10;
    
    /// I2C frequency
    constexpr uint32_t I2C_FREQ = 400000;  // 400kHz
    
    /// ICM20948 I2C address (AD0 low = 0x68, AD0 high = 0x69)
    constexpr uint8_t IMU_ADDR = 0x68;
    
    //=========================================================================
    // IMU Data (Read-only access)
    //=========================================================================
    
    /// Accelerometer data in milli-g
    extern int16_t accelX;
    extern int16_t accelY;
    extern int16_t accelZ;
    
    /// Gyroscope data in degrees/second
    extern int16_t gyroX;
    extern int16_t gyroY;
    extern int16_t gyroZ;
    
    //=========================================================================
    // API
    //=========================================================================
    
    /**
     * @brief Initialize I2C interface and configure ICM20948
     * @return true if initialization succeeded
     */
    bool init();
    
    /**
     * @brief Non-blocking update - reads accelerometer and gyroscope data
     * 
     * Call this frequently to get up-to-date IMU readings.
     */
    void update();
    
    /**
     * @brief Check if IMU is initialized
     * @return true if initialized
     */
    bool isInitialized();

} // namespace ImuDriver

} // namespace Drivers
