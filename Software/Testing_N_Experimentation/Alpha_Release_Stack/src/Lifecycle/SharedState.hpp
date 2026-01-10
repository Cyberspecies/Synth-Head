/**
 * @file SharedState.hpp
 * @brief Shared state between Boot and Current modes
 * 
 * This structure allows communication between Boot and Current modes.
 * Boot mode initializes resources, Current mode uses them.
 */

#pragma once

#include <cstdint>
#include "driver/i2c.h"
#include "driver/i2s_std.h"

namespace Lifecycle {

//=============================================================================
// Sensor Data Structures
//=============================================================================

struct IMUData {
    int16_t accelX = 0, accelY = 0, accelZ = 0;
    int16_t gyroX = 0, gyroY = 0, gyroZ = 0;
    float temperature = 0;
    bool valid = false;
};

struct EnvironmentData {
    int32_t tempRaw = 0;
    int32_t pressRaw = 0;
    int32_t humidRaw = 0;
    float temperature = 0;    // Celsius
    float pressure = 0;       // hPa
    float humidity = 0;       // %
    bool valid = false;
};

struct GPSData {
    char time[12] = "";
    char lat[16] = "";
    char latDir = ' ';
    char lon[16] = "";
    char lonDir = ' ';
    int fixQuality = 0;
    int numSats = 0;
    char altitude[10] = "";
    char speed[10] = "";
    char course[10] = "";
    char hdop[8] = "";
    bool valid = false;
    // Last known good position
    char lastLat[16] = "";
    char lastLatDir = ' ';
    char lastLon[16] = "";
    char lastLonDir = ' ';
    char lastAlt[10] = "";
    int64_t lastFixTime = 0;
};

struct AudioData {
    int32_t current = 0;
    int32_t average = 0;
    int32_t peak = 0;
    bool valid = false;
};

struct ButtonState {
    bool a = false;
    bool b = false;
    bool c = false;
    bool d = false;
};

//=============================================================================
// BME280 Calibration Data
//=============================================================================

struct BME280Calibration {
    uint16_t dig_T1 = 0;
    int16_t dig_T2 = 0, dig_T3 = 0;
    uint16_t dig_P1 = 0;
    int16_t dig_P2 = 0, dig_P3 = 0, dig_P4 = 0, dig_P5 = 0;
    int16_t dig_P6 = 0, dig_P7 = 0, dig_P8 = 0, dig_P9 = 0;
    uint8_t dig_H1 = 0, dig_H3 = 0;
    int16_t dig_H2 = 0, dig_H4 = 0, dig_H5 = 0;
    int8_t dig_H6 = 0;
    int32_t t_fine = 0;  // Used in compensation calculations
    bool loaded = false;
};

//=============================================================================
// Hardware State
//=============================================================================

struct HardwareState {
    // Initialization flags
    bool i2cInitialized = false;
    bool gpsInitialized = false;
    bool micInitialized = false;
    bool buttonsInitialized = false;
    
    // Sensor presence
    bool icm20948Present = false;
    bool bme280Present = false;
    
    // I2S handle for microphone
    i2s_chan_handle_t i2sRxHandle = nullptr;
    
    // Calibration data
    BME280Calibration bme280Cal;
};

//=============================================================================
// Shared State
//=============================================================================

/**
 * @brief Main shared state structure passed between Boot and Current modes
 */
struct SharedState {
    // Hardware state (set by Boot, used by Current)
    HardwareState hw;
    
    // Live sensor data (updated by Current)
    IMUData imu;
    EnvironmentData env;
    GPSData gps;
    AudioData audio;
    ButtonState buttons;
    
    // System state
    bool debugMode = false;
    bool systemTestMode = false;
    uint32_t bootTime = 0;
    
    // User data (can be used for application-specific state)
    void* userData = nullptr;
};

} // namespace Lifecycle
