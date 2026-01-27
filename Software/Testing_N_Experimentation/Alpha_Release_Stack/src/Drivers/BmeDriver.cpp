/**
 * @file BmeDriver.cpp
 * @brief BME280 Environmental Sensor Driver implementation - I2C
 */

#include "Drivers/BmeDriver.hpp"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cmath>

namespace Drivers {
namespace BmeDriver {

//=============================================================================
// Constants
//=============================================================================

static constexpr i2c_port_t I2C_PORT = I2C_NUM_0;

// BME280 Register addresses
static constexpr uint8_t REG_CHIP_ID = 0xD0;
static constexpr uint8_t REG_RESET = 0xE0;
static constexpr uint8_t REG_CTRL_HUM = 0xF2;
static constexpr uint8_t REG_STATUS = 0xF3;
static constexpr uint8_t REG_CTRL_MEAS = 0xF4;
static constexpr uint8_t REG_CONFIG = 0xF5;
static constexpr uint8_t REG_PRESS_MSB = 0xF7;
static constexpr uint8_t REG_CALIB00 = 0x88;
static constexpr uint8_t REG_CALIB26 = 0xE1;

static constexpr uint8_t CHIP_ID_BME280 = 0x60;
static constexpr uint8_t CHIP_ID_BMP280 = 0x58;

//=============================================================================
// Internal State
//=============================================================================

static bool initialized = false;
static bool isBme280 = true;  // false = BMP280 (no humidity)

// Calibration data
static uint16_t dig_T1;
static int16_t dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t dig_H1, dig_H3;
static int16_t dig_H2, dig_H4, dig_H5;
static int8_t dig_H6;

static int32_t t_fine;

//=============================================================================
// Exported Environmental Data
//=============================================================================

int32_t temperatureX100 = 0;
int32_t humidityX100 = 0;
uint32_t pressurePa = 0;
int32_t altitudeX10 = 0;
bool connected = false;

//=============================================================================
// I2C Helper Functions (reusing I2C port from ImuDriver)
//=============================================================================

/**
 * @brief Read a single byte from register
 */
static esp_err_t readRegister(uint8_t reg, uint8_t* data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Write a single byte to register
 */
static esp_err_t writeRegister(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Read multiple bytes from register
 */
static esp_err_t readRegisters(uint8_t reg, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

//=============================================================================
// Public API
//=============================================================================

bool init() {
    if (initialized) return true;
    
    // Note: I2C bus should already be initialized by ImuDriver
    // We just use the same I2C port
    
    // Check chip ID
    uint8_t chipId = 0;
    esp_err_t err = readRegister(REG_CHIP_ID, &chipId);
    if (err != ESP_OK) {
        printf("  BME: Chip ID read failed: %d\n", err);
        connected = false;
        return false;
    }
    
    if (chipId == CHIP_ID_BME280) {
        isBme280 = true;
        printf("  BME: BME280 detected (ID=0x%02X)\n", chipId);
    } else if (chipId == CHIP_ID_BMP280) {
        isBme280 = false;
        printf("  BME: BMP280 detected (ID=0x%02X, no humidity)\n", chipId);
    } else {
        printf("  BME: Unknown chip ID: 0x%02X\n", chipId);
        connected = false;
        return false;
    }
    
    // Soft reset
    writeRegister(REG_RESET, 0xB6);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Read temperature and pressure calibration data (26 bytes from 0x88)
    uint8_t calib[26];
    err = readRegisters(REG_CALIB00, calib, 26);
    if (err != ESP_OK) {
        printf("  BME: Calibration read failed: %d\n", err);
        connected = false;
        return false;
    }
    
    dig_T1 = (calib[1] << 8) | calib[0];
    dig_T2 = (calib[3] << 8) | calib[2];
    dig_T3 = (calib[5] << 8) | calib[4];
    dig_P1 = (calib[7] << 8) | calib[6];
    dig_P2 = (calib[9] << 8) | calib[8];
    dig_P3 = (calib[11] << 8) | calib[10];
    dig_P4 = (calib[13] << 8) | calib[12];
    dig_P5 = (calib[15] << 8) | calib[14];
    dig_P6 = (calib[17] << 8) | calib[16];
    dig_P7 = (calib[19] << 8) | calib[18];
    dig_P8 = (calib[21] << 8) | calib[20];
    dig_P9 = (calib[23] << 8) | calib[22];
    dig_H1 = calib[25];
    
    // Read humidity calibration data (7 bytes from 0xE1) - BME280 only
    if (isBme280) {
        uint8_t calibH[7];
        err = readRegisters(REG_CALIB26, calibH, 7);
        if (err != ESP_OK) {
            printf("  BME: Humidity calibration read failed: %d\n", err);
            connected = false;
            return false;
        }
        dig_H2 = (calibH[1] << 8) | calibH[0];
        dig_H3 = calibH[2];
        dig_H4 = (calibH[3] << 4) | (calibH[4] & 0x0F);
        dig_H5 = (calibH[5] << 4) | ((calibH[4] >> 4) & 0x0F);
        dig_H6 = calibH[6];
    }
    
    // Configure sensor
    // Humidity oversampling x1 (BME280 only)
    if (isBme280) {
        writeRegister(REG_CTRL_HUM, 0x01);
    }
    
    // Config: standby 1000ms, filter off
    writeRegister(REG_CONFIG, 0x05 << 5);
    
    // Ctrl_meas: temp x1, pressure x1, normal mode
    uint8_t ctrl_meas = (0x01 << 5) | (0x01 << 2) | 0x03;
    writeRegister(REG_CTRL_MEAS, ctrl_meas);
    
    initialized = true;
    connected = true;
    printf("  BME: Ready on I2C addr 0x%02X (shared bus)\n", BME_ADDR);
    return true;
}

void update() {
    if (!initialized) {
        connected = false;
        return;
    }
    
    // Read raw data (8 bytes: press[3], temp[3], hum[2])
    uint8_t buffer[8];
    esp_err_t err = readRegisters(REG_PRESS_MSB, buffer, 8);
    if (err != ESP_OK) {
        connected = false;
        return;
    }
    
    int32_t adc_P = ((uint32_t)buffer[0] << 12) | ((uint32_t)buffer[1] << 4) | (buffer[2] >> 4);
    int32_t adc_T = ((uint32_t)buffer[3] << 12) | ((uint32_t)buffer[4] << 4) | (buffer[5] >> 4);
    int32_t adc_H = ((uint32_t)buffer[6] << 8) | buffer[7];
    
    //=========================================================================
    // Compensate temperature (from BME280 datasheet)
    //=========================================================================
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    int32_t T = (t_fine * 5 + 128) >> 8;
    temperatureX100 = T;  // Temperature in 0.01°C units
    
    //=========================================================================
    // Compensate pressure (from BME280 datasheet)
    //=========================================================================
    int64_t var1_64 = ((int64_t)t_fine) - 128000;
    int64_t var2_64 = var1_64 * var1_64 * (int64_t)dig_P6;
    var2_64 = var2_64 + ((var1_64 * (int64_t)dig_P5) << 17);
    var2_64 = var2_64 + (((int64_t)dig_P4) << 35);
    var1_64 = ((var1_64 * var1_64 * (int64_t)dig_P3) >> 8) + ((var1_64 * (int64_t)dig_P2) << 12);
    var1_64 = (((((int64_t)1) << 47) + var1_64)) * ((int64_t)dig_P1) >> 33;
    
    if (var1_64 == 0) {
        pressurePa = 0;
    } else {
        int64_t p = 1048576 - adc_P;
        p = (((p << 31) - var2_64) * 3125) / var1_64;
        var1_64 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        var2_64 = (((int64_t)dig_P8) * p) >> 19;
        p = ((p + var1_64 + var2_64) >> 8) + (((int64_t)dig_P7) << 4);
        pressurePa = (uint32_t)(p >> 8);  // Pressure in Pa
    }
    
    //=========================================================================
    // Compensate humidity (BME280 only, from datasheet)
    //=========================================================================
    if (isBme280) {
        int32_t v_x1_u32r = (t_fine - ((int32_t)76800));
        v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) +
                ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) *
                (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
                ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
        v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
        v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
        v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
        humidityX100 = (int32_t)((v_x1_u32r >> 12) * 100 / 1024);  // Humidity in 0.01% units
    } else {
        humidityX100 = 0;
    }
    
    //=========================================================================
    // Calculate altitude from pressure
    //=========================================================================
    float pressureHPa = pressurePa / 100.0f;
    float altitude = 44330.0f * (1.0f - powf(pressureHPa / (SEA_LEVEL_PRESSURE / 100.0f), 0.1903f));
    altitudeX10 = (int32_t)(altitude * 10.0f);
    
    connected = true;
}

bool isInitialized() {
    return initialized;
}

} // namespace BmeDriver
} // namespace Drivers
