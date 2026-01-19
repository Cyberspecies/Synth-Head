/**
 * @file GpsDriver.hpp
 * @brief GPS NMEA Parser - NEO-6M/NEO-8M UART Module
 * 
 * Provides non-blocking NMEA sentence parsing from GPS module.
 * Supports GGA, RMC, and VTG sentences for position, time, speed, and heading.
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace Drivers {

/**
 * @brief GPS Driver for NEO-6M/NEO-8M UART Module
 * 
 * Features:
 * - Non-blocking NMEA sentence parsing
 * - Supports GPGGA, GPRMC, GPVTG (and GNSS variants)
 * - Position, altitude, speed, heading, satellites
 * - Date and time extraction
 */
namespace GpsDriver {
    
    //=========================================================================
    // Configuration
    //=========================================================================
    
    /// GPS UART port number
    constexpr int GPS_UART = 2;  // UART2
    
    /// UART pins
    constexpr int GPS_TX_PIN = 1;   // GPS TX -> CPU RX
    constexpr int GPS_RX_PIN = 45;  // GPS RX <- CPU TX (not used)
    
    /// Baud rate (standard for NEO-6M/8M)
    constexpr int GPS_BAUD = 9600;
    
    //=========================================================================
    // GPS Data (Read-only access)
    //=========================================================================
    
    /// Position data
    extern float latitude;      ///< Latitude in decimal degrees
    extern float longitude;     ///< Longitude in decimal degrees
    extern float altitude;      ///< Altitude in meters (from GGA)
    
    /// Movement data
    extern float speed;         ///< Speed in km/h (from RMC/VTG)
    extern float heading;       ///< Course over ground in degrees (from RMC/VTG)
    
    /// Accuracy data
    extern int satellites;      ///< Number of satellites in use
    extern float hdop;          ///< Horizontal dilution of precision
    extern bool valid;          ///< True if we have a valid fix
    
    /// Time data (UTC)
    extern int hour;
    extern int minute;
    extern int second;
    
    /// Date data
    extern int day;
    extern int month;
    extern int year;
    
    /// Debug counter
    extern unsigned long bytesReceived;  ///< Total bytes received (for debugging)
    
    //=========================================================================
    // API
    //=========================================================================
    
    /**
     * @brief Initialize GPS UART interface
     * @return true if initialization succeeded
     */
    bool init();
    
    /**
     * @brief Non-blocking update - reads available bytes and parses complete sentences
     * 
     * Call this frequently (every loop iteration) to process incoming GPS data.
     * This function is non-blocking and returns immediately if no data is available.
     */
    void update();
    
    /**
     * @brief Check if GPS driver is initialized
     * @return true if initialized
     */
    bool isInitialized();

} // namespace GpsDriver

} // namespace Drivers
