/**
 * @file BmeDriver.hpp
 * @brief BME280 Environmental Sensor Driver - I2C
 * 
 * Provides temperature, humidity, and pressure readings from BME280.
 * Shares I2C bus with ImuDriver (GPIO 9/10).
 */

#pragma once

#include <cstdint>

namespace Drivers {

/**
 * @brief BME280 Environmental Sensor Driver
 * 
 * Features:
 * - I2C communication at 400kHz (shared with IMU)
 * - Temperature in Celsius (x100 for precision)
 * - Humidity in % (x100 for precision)
 * - Pressure in Pa
 * - Altitude calculation from sea level pressure
 * - Non-blocking updates
 */
namespace BmeDriver {
    
    //=========================================================================
    // Configuration
    //=========================================================================
    
    /// I2C pins (shared with IMU)
    constexpr int I2C_SDA_PIN = 9;
    constexpr int I2C_SCL_PIN = 10;
    
    /// BME280 I2C address (SDO to GND = 0x76, SDO to VCC = 0x77)
    constexpr uint8_t BME_ADDR = 0x76;
    
    /// Standard sea level pressure in Pa
    constexpr float SEA_LEVEL_PRESSURE = 101325.0f;
    
    //=========================================================================
    // Environmental Data (Read-only access)
    //=========================================================================
    
    /// Temperature in Celsius x100 (e.g., 2350 = 23.50°C)
    extern int32_t temperatureX100;
    
    /// Humidity in % x100 (e.g., 6543 = 65.43%)
    extern int32_t humidityX100;
    
    /// Pressure in Pa (e.g., 101325 = 1013.25 hPa)
    extern uint32_t pressurePa;
    
    /// Altitude in meters x10 (e.g., 1234 = 123.4m)
    extern int32_t altitudeX10;
    
    /// True if BME280 was detected
    extern bool connected;
    
    //=========================================================================
    // API
    //=========================================================================
    
    /**
     * @brief Initialize BME280 sensor
     * 
     * Note: I2C bus must be initialized by ImuDriver first!
     * Call ImuDriver::init() before calling this function.
     * 
     * @return true if BME280 initialization succeeded
     */
    bool init();
    
    /**
     * @brief Non-blocking update - reads temperature, humidity, pressure
     * 
     * Also calculates altitude from sea level pressure.
     * Call this frequently to get up-to-date readings.
     */
    void update();
    
    /**
     * @brief Check if BME280 is initialized
     * @return true if initialized
     */
    bool isInitialized();
    
    /**
     * @brief Get temperature as float in Celsius
     * @return Temperature in °C
     */
    inline float getTemperature() { return temperatureX100 / 100.0f; }
    
    /**
     * @brief Get humidity as float in %
     * @return Humidity in %
     */
    inline float getHumidity() { return humidityX100 / 100.0f; }
    
    /**
     * @brief Get pressure as float in hPa
     * @return Pressure in hPa
     */
    inline float getPressure() { return pressurePa / 100.0f; }
    
    /**
     * @brief Get altitude as float in meters
     * @return Altitude in meters
     */
    inline float getAltitude() { return altitudeX10 / 10.0f; }

} // namespace BmeDriver

} // namespace Drivers
