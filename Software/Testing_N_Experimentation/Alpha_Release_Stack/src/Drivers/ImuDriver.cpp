/**
 * @file ImuDriver.cpp
 * @brief IMU Driver implementation - ICM20948 I2C
 */

#include "Drivers/ImuDriver.hpp"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>

namespace Drivers {
namespace ImuDriver {

//=============================================================================
// Constants
//=============================================================================

static constexpr i2c_port_t I2C_PORT = I2C_NUM_0;

// ICM20948 Register addresses
static constexpr uint8_t REG_WHO_AM_I = 0x00;
static constexpr uint8_t REG_PWR_MGMT_1 = 0x06;
static constexpr uint8_t REG_PWR_MGMT_2 = 0x07;
static constexpr uint8_t REG_ACCEL_XOUT_H = 0x2D;
static constexpr uint8_t REG_GYRO_XOUT_H = 0x33;
static constexpr uint8_t WHO_AM_I_VALUE = 0xEA;

// Scale factors
// Default ±4g range: 8192 LSB/g -> multiply raw by 1000/8192 to get mg
static constexpr float ACCEL_SCALE = 1000.0f / 8192.0f;
// Default ±500 dps range: 65.5 LSB/(deg/s)
static constexpr float GYRO_SCALE = 1.0f / 65.5f;

//=============================================================================
// Internal State
//=============================================================================

static bool initialized = false;

//=============================================================================
// Exported IMU Data
//=============================================================================

int16_t accelX = 0;
int16_t accelY = 0;
int16_t accelZ = 0;
int16_t gyroX = 0;
int16_t gyroY = 0;
int16_t gyroZ = 0;

//=============================================================================
// I2C Helper Functions
//=============================================================================

/**
 * @brief Read a single byte from register
 */
static esp_err_t readRegister(uint8_t reg, uint8_t* data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMU_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMU_ADDR << 1) | I2C_MASTER_READ, true);
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
    i2c_master_write_byte(cmd, (IMU_ADDR << 1) | I2C_MASTER_WRITE, true);
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
    i2c_master_write_byte(cmd, (IMU_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMU_ADDR << 1) | I2C_MASTER_READ, true);
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
    
    // Configure I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = I2C_FREQ
        },
        .clk_flags = 0
    };
    
    esp_err_t err = i2c_param_config(I2C_PORT, &conf);
    if (err != ESP_OK) {
        printf("  IMU: I2C config failed: %d\n", err);
        return false;
    }
    
    err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        printf("  IMU: I2C driver install failed: %d\n", err);
        return false;
    }
    
    // Check WHO_AM_I register
    uint8_t whoAmI = 0;
    err = readRegister(REG_WHO_AM_I, &whoAmI);
    if (err != ESP_OK) {
        printf("  IMU: WHO_AM_I read failed: %d\n", err);
        return false;
    }
    
    if (whoAmI != WHO_AM_I_VALUE) {
        printf("  IMU: Wrong WHO_AM_I: 0x%02X (expected 0x%02X)\n", whoAmI, WHO_AM_I_VALUE);
        return false;
    }
    
    printf("  IMU: ICM20948 detected (WHO_AM_I=0x%02X)\n", whoAmI);
    
    // Reset device
    writeRegister(REG_PWR_MGMT_1, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Wake up device, auto-select clock
    writeRegister(REG_PWR_MGMT_1, 0x01);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Enable accelerometer and gyroscope
    writeRegister(REG_PWR_MGMT_2, 0x00);
    
    initialized = true;
    printf("  IMU: Ready on I2C (SDA:%d, SCL:%d)\n", I2C_SDA_PIN, I2C_SCL_PIN);
    return true;
}

void update() {
    if (!initialized) return;
    
    // Read 12 bytes: 6 for accel, 6 for gyro
    uint8_t buffer[12];
    esp_err_t err = readRegisters(REG_ACCEL_XOUT_H, buffer, 12);
    if (err != ESP_OK) return;
    
    // Parse accelerometer (big-endian)
    int16_t rawAccelX = (buffer[0] << 8) | buffer[1];
    int16_t rawAccelY = (buffer[2] << 8) | buffer[3];
    int16_t rawAccelZ = (buffer[4] << 8) | buffer[5];
    
    // Parse gyroscope (big-endian)
    int16_t rawGyroX = (buffer[6] << 8) | buffer[7];
    int16_t rawGyroY = (buffer[8] << 8) | buffer[9];
    int16_t rawGyroZ = (buffer[10] << 8) | buffer[11];
    
    // Convert to milli-g and deg/s
    accelX = (int16_t)(rawAccelX * ACCEL_SCALE);
    accelY = (int16_t)(rawAccelY * ACCEL_SCALE);
    accelZ = (int16_t)(rawAccelZ * ACCEL_SCALE);
    
    gyroX = (int16_t)(rawGyroX * GYRO_SCALE);
    gyroY = (int16_t)(rawGyroY * GYRO_SCALE);
    gyroZ = (int16_t)(rawGyroZ * GYRO_SCALE);
}

bool isInitialized() {
    return initialized;
}

} // namespace ImuDriver
} // namespace Drivers
