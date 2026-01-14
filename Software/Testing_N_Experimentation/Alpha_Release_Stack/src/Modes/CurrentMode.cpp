/**
 * @file CurrentMode.cpp
 * @brief Current mode implementation using SystemAPI
 * 
 * SystemAPI includes all layers: HAL, BaseAPI, FrameworkAPI
 * Use the appropriate layer for your needs.
 */

#include "CurrentMode.hpp"
#include "SystemAPI/SystemAPI.hpp"
#include "SystemAPI/Web/CaptivePortal.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "SystemAPI/Utils/FileSystemService.hpp"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

namespace Modes {

//=============================================================================
// GPS Driver - NEO-8M UART NMEA Parser
//=============================================================================
namespace GpsDriver {
    // GPS UART pins - CORRECTED based on diagnostic test
    // ESP RX (GPIO 44) <- GPS TX, ESP TX (GPIO 43) -> GPS RX
    static constexpr int GPS_TX_PIN = 43;  // ESP TX -> GPS RX
    static constexpr int GPS_RX_PIN = 44;  // ESP RX <- GPS TX  
    static constexpr int GPS_BAUD = 9600;
    static constexpr uart_port_t GPS_UART = UART_NUM_1;
    
    static bool initialized = false;
    static char nmeaBuffer[256];
    static int nmeaIndex = 0;
    static uint32_t bytesReceived = 0;  // Debug counter
    
    // Parsed GPS data
    static float latitude = 0.0f;
    static float longitude = 0.0f;
    static float altitude = 0.0f;
    static float speed = 0.0f;        // Speed in km/h
    static float heading = 0.0f;      // Course over ground in degrees
    static float hdop = 99.9f;        // Horizontal dilution of precision
    static uint8_t satellites = 0;
    static bool valid = false;
    
    // Time data (UTC from GPS)
    static uint8_t hour = 0;
    static uint8_t minute = 0;
    static uint8_t second = 0;
    static uint8_t day = 0;
    static uint8_t month = 0;
    static uint16_t year = 0;
    
    bool init() {
        if (initialized) return true;
        
        uart_config_t uart_config = {
            .baud_rate = GPS_BAUD,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_DEFAULT,
            .flags = {.allow_pd = 0, .backup_before_sleep = 0}
        };
        
        esp_err_t err = uart_param_config(GPS_UART, &uart_config);
        if (err != ESP_OK) {
            printf("  GPS: UART config failed: %d\n", err);
            return false;
        }
        
        err = uart_set_pin(GPS_UART, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK) {
            printf("  GPS: Pin config failed: %d\n", err);
            return false;
        }
        
        // Increased buffer to 1024 bytes - GPS at 9600 baud sends ~960 bytes/sec
        // This provides ~1 second of buffering for smoother operation
        err = uart_driver_install(GPS_UART, 1024, 0, 0, nullptr, 0);
        if (err != ESP_OK) {
            printf("  GPS: Driver install failed: %d\n", err);
            return false;
        }
        
        initialized = true;
        printf("  GPS: Initialized on UART%d (TX:%d, RX:%d)\n", GPS_UART, GPS_TX_PIN, GPS_RX_PIN);
        return true;
    }
    
    // Parse NMEA coordinate (DDDMM.MMMMM format) to decimal degrees
    float parseCoordinate(const char* str, char direction) {
        if (!str || strlen(str) < 4) return 0.0f;
        
        float raw = atof(str);
        int degrees = (int)(raw / 100);
        float minutes = raw - (degrees * 100);
        float decimal = degrees + (minutes / 60.0f);
        
        if (direction == 'S' || direction == 'W') {
            decimal = -decimal;
        }
        return decimal;
    }
    
    // Parse $GPGGA sentence for position and satellites
    void parseGGA(char* sentence) {
        // $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47
        char* tokens[15];
        int tokenCount = 0;
        
        char* token = strtok(sentence, ",");
        while (token && tokenCount < 15) {
            tokens[tokenCount++] = token;
            token = strtok(nullptr, ",");
        }
        
        if (tokenCount >= 10) {
            // Fix quality (token 6): 0 = invalid, 1+ = valid
            int fixQuality = (tokenCount > 6) ? atoi(tokens[6]) : 0;
            valid = (fixQuality > 0);
            
            if (valid) {
                // Latitude (token 2) and N/S (token 3)
                if (tokenCount > 3 && strlen(tokens[2]) > 0) {
                    latitude = parseCoordinate(tokens[2], tokens[3][0]);
                }
                
                // Longitude (token 4) and E/W (token 5)
                if (tokenCount > 5 && strlen(tokens[4]) > 0) {
                    longitude = parseCoordinate(tokens[4], tokens[5][0]);
                }
                
                // Altitude (token 9)
                if (tokenCount > 9 && strlen(tokens[9]) > 0) {
                    altitude = atof(tokens[9]);
                }
            }
            
            // Satellites (token 7) - always parse even without fix
            if (tokenCount > 7 && strlen(tokens[7]) > 0) {
                satellites = atoi(tokens[7]);
            }
            
            // HDOP (token 8)
            if (tokenCount > 8 && strlen(tokens[8]) > 0) {
                hdop = atof(tokens[8]);
            }
        }
    }
    
    // Parse $GPRMC/$GNRMC sentence for time, date, speed, and heading
    void parseRMC(char* sentence) {
        // $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
        // Fields: time, status, lat, N/S, lon, E/W, speed(knots), course, date, mag_var, E/W
        char* tokens[15];
        int tokenCount = 0;
        
        char* token = strtok(sentence, ",");
        while (token && tokenCount < 15) {
            tokens[tokenCount++] = token;
            token = strtok(nullptr, ",");
        }
        
        if (tokenCount >= 10) {
            // Time (token 1) - HHMMSS.sss
            if (strlen(tokens[1]) >= 6) {
                hour = (tokens[1][0] - '0') * 10 + (tokens[1][1] - '0');
                minute = (tokens[1][2] - '0') * 10 + (tokens[1][3] - '0');
                second = (tokens[1][4] - '0') * 10 + (tokens[1][5] - '0');
            }
            
            // Date (token 9) - DDMMYY
            if (tokenCount > 9 && strlen(tokens[9]) >= 6) {
                day = (tokens[9][0] - '0') * 10 + (tokens[9][1] - '0');
                month = (tokens[9][2] - '0') * 10 + (tokens[9][3] - '0');
                year = 2000 + (tokens[9][4] - '0') * 10 + (tokens[9][5] - '0');
            }
            
            // Speed in knots (token 7) -> convert to km/h
            if (tokenCount > 7 && strlen(tokens[7]) > 0) {
                float speedKnots = atof(tokens[7]);
                speed = speedKnots * 1.852f;  // knots to km/h
            }
            
            // Course/heading (token 8)
            if (tokenCount > 8 && strlen(tokens[8]) > 0) {
                heading = atof(tokens[8]);
            }
        }
    }
    
    // Parse $GPVTG/$GNVTG sentence for speed and heading (alternative source)
    void parseVTG(char* sentence) {
        // $GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48
        // Fields: course_true, T, course_mag, M, speed_knots, N, speed_kmh, K
        char* tokens[10];
        int tokenCount = 0;
        
        char* token = strtok(sentence, ",");
        while (token && tokenCount < 10) {
            tokens[tokenCount++] = token;
            token = strtok(nullptr, ",");
        }
        
        if (tokenCount >= 8) {
            // True heading (token 1)
            if (strlen(tokens[1]) > 0) {
                heading = atof(tokens[1]);
            }
            
            // Speed in km/h (token 7)
            if (strlen(tokens[7]) > 0) {
                speed = atof(tokens[7]);
            }
        }
    }
    
    // Parse a complete NMEA sentence
    void parseSentence(char* sentence) {
        if (strncmp(sentence, "$GPGGA", 6) == 0 || strncmp(sentence, "$GNGGA", 6) == 0) {
            parseGGA(sentence);
        } else if (strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0) {
            parseRMC(sentence);
        } else if (strncmp(sentence, "$GPVTG", 6) == 0 || strncmp(sentence, "$GNVTG", 6) == 0) {
            parseVTG(sentence);
        }
    }
    
    // Non-blocking update - reads available bytes and parses complete sentences
    void update() {
        if (!initialized) return;
        
        size_t available = 0;
        uart_get_buffered_data_len(GPS_UART, &available);
        
        while (available > 0) {
            char c;
            int len = uart_read_bytes(GPS_UART, &c, 1, 0);
            if (len <= 0) break;
            
            bytesReceived++;  // Count all bytes for debug
            
            if (c == '$') {
                // Start of new sentence
                nmeaIndex = 0;
            }
            
            if (nmeaIndex < (int)(sizeof(nmeaBuffer) - 1)) {
                nmeaBuffer[nmeaIndex++] = c;
            }
            
            if (c == '\n' || c == '\r') {
                // End of sentence
                nmeaBuffer[nmeaIndex] = '\0';
                if (nmeaIndex > 6) {
                    parseSentence(nmeaBuffer);
                }
                nmeaIndex = 0;
            }
            
            available--;
        }
    }
}

//=============================================================================
// Microphone Driver - I2S INMP441 with Rolling Average
//=============================================================================
namespace MicDriver {
    // Microphone I2S pins (from PIN_MAPPING_CPU.md)
    static constexpr int MIC_WS_PIN = 42;    // Word Select (LRCLK)
    static constexpr int MIC_BCK_PIN = 40;   // Bit Clock
    static constexpr int MIC_DATA_PIN = 2;   // Data out
    static constexpr int MIC_LR_PIN = 41;    // L/R Select (tie low for left)
    
    static i2s_chan_handle_t rxHandle = nullptr;
    static bool initialized = false;
    
    // Rolling window for dB averaging (non-blocking)
    static constexpr int WINDOW_SIZE = 16;
    static float dbWindow[WINDOW_SIZE] = {0};
    static int windowIndex = 0;
    static float avgDb = -60.0f;
    static float currentDb = -60.0f;
    static uint8_t level = 0;
    
    // Sample buffer
    static int32_t sampleBuffer[64];
    
    bool init() {
        if (initialized) return true;
        
        // Configure L/R pin to LOW (select left channel)
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << MIC_LR_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
        gpio_set_level((gpio_num_t)MIC_LR_PIN, 0);
        
        // I2S channel configuration
        i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM_0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = 4,
            .dma_frame_num = 64,
            .auto_clear_after_cb = false,
            .auto_clear_before_cb = false,
            .intr_priority = 0
        };
        
        esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &rxHandle);
        if (err != ESP_OK) {
            printf("  MIC: Channel create failed: %d\n", err);
            return false;
        }
        
        // I2S standard configuration for INMP441
        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = 16000,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256
            },
            .slot_cfg = {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_MONO,
                .slot_mask = I2S_STD_SLOT_LEFT,
                .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
                .ws_pol = false,
                .bit_shift = true,
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false
            },
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = (gpio_num_t)MIC_BCK_PIN,
                .ws = (gpio_num_t)MIC_WS_PIN,
                .dout = I2S_GPIO_UNUSED,
                .din = (gpio_num_t)MIC_DATA_PIN,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false
                }
            }
        };
        
        err = i2s_channel_init_std_mode(rxHandle, &std_cfg);
        if (err != ESP_OK) {
            printf("  MIC: Init std mode failed: %d\n", err);
            i2s_del_channel(rxHandle);
            rxHandle = nullptr;
            return false;
        }
        
        err = i2s_channel_enable(rxHandle);
        if (err != ESP_OK) {
            printf("  MIC: Enable failed: %d\n", err);
            i2s_del_channel(rxHandle);
            rxHandle = nullptr;
            return false;
        }
        
        // Initialize window with quiet values
        for (int i = 0; i < WINDOW_SIZE; i++) {
            dbWindow[i] = -60.0f;
        }
        
        initialized = true;
        printf("  MIC: Initialized on I2S0 (WS:%d, BCK:%d, DATA:%d)\n", MIC_WS_PIN, MIC_BCK_PIN, MIC_DATA_PIN);
        return true;
    }
    
    // Non-blocking update - reads samples and updates rolling average
    void update() {
        if (!initialized || !rxHandle) return;
        
        size_t bytesRead = 0;
        esp_err_t err = i2s_channel_read(rxHandle, sampleBuffer, sizeof(sampleBuffer), &bytesRead, 0);
        
        if (err == ESP_OK && bytesRead > 0) {
            int numSamples = bytesRead / sizeof(int32_t);
            
            // Calculate RMS of this batch
            int64_t sumSquares = 0;
            int32_t peak = 0;
            
            for (int i = 0; i < numSamples; i++) {
                // INMP441 outputs 24-bit data in upper bits of 32-bit word
                int32_t sample = sampleBuffer[i] >> 8;  // Shift to get 24-bit value
                if (sample < 0) sample = -sample;
                if (sample > peak) peak = sample;
                sumSquares += (int64_t)sample * sample;
            }
            
            float rms = sqrtf((float)sumSquares / numSamples);
            
            // Convert to dB (reference: max 24-bit value = 8388607)
            // Add small offset to avoid log(0)
            float db = 20.0f * log10f((rms + 1.0f) / 8388607.0f);
            
            // Clamp to reasonable range
            if (db < -60.0f) db = -60.0f;
            if (db > 0.0f) db = 0.0f;
            
            // Add to rolling window
            dbWindow[windowIndex] = db;
            windowIndex = (windowIndex + 1) % WINDOW_SIZE;
            
            // Calculate rolling average
            float sum = 0.0f;
            for (int i = 0; i < WINDOW_SIZE; i++) {
                sum += dbWindow[i];
            }
            avgDb = sum / WINDOW_SIZE;
            currentDb = db;
            
            // Calculate level (0-100)
            level = (uint8_t)((avgDb + 60.0f) * 100.0f / 60.0f);
            if (level > 100) level = 100;
        }
    }
}

//=============================================================================
// IMU Driver - ICM20948 I2C
//=============================================================================
namespace ImuDriver {
    // I2C pins (from PIN_MAPPING_CPU.md)
    static constexpr int I2C_SDA_PIN = 9;
    static constexpr int I2C_SCL_PIN = 10;
    static constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
    static constexpr uint32_t I2C_FREQ = 400000;  // 400kHz
    
    // ICM20948 I2C address (AD0 low = 0x68, AD0 high = 0x69)
    static constexpr uint8_t IMU_ADDR = 0x68;
    
    // ICM20948 Register addresses
    static constexpr uint8_t REG_WHO_AM_I = 0x00;
    static constexpr uint8_t REG_PWR_MGMT_1 = 0x06;
    static constexpr uint8_t REG_PWR_MGMT_2 = 0x07;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x2D;
    static constexpr uint8_t REG_GYRO_XOUT_H = 0x33;
    static constexpr uint8_t WHO_AM_I_VALUE = 0xEA;
    
    static bool initialized = false;
    
    // Parsed IMU data (in milli-g and degrees/sec)
    static int16_t accelX = 0, accelY = 0, accelZ = 0;
    static int16_t gyroX = 0, gyroY = 0, gyroZ = 0;
    
    // Scale factors
    // Default ±4g range: 8192 LSB/g -> multiply raw by 1000/8192 to get mg
    static constexpr float ACCEL_SCALE = 1000.0f / 8192.0f;
    // Default ±500 dps range: 65.5 LSB/(deg/s)
    static constexpr float GYRO_SCALE = 1.0f / 65.5f;
    
    // Helper: Read a single byte from register
    esp_err_t readRegister(uint8_t reg, uint8_t* data) {
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
    
    // Helper: Write a single byte to register
    esp_err_t writeRegister(uint8_t reg, uint8_t value) {
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
    
    // Helper: Read multiple bytes from register
    esp_err_t readRegisters(uint8_t reg, uint8_t* data, size_t len) {
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
    
    // Non-blocking update - reads accel and gyro data
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
}

//=============================================================================
// GPU UART Driver - ESP-to-ESP Communication (Proper Protocol)
//=============================================================================
#include "GpuDriver/GpuCommands.hpp"

namespace GpuDriver {
    // GPU UART pins (from PIN_MAPPING_CPU.md)
    static constexpr int GPU_TX_PIN = 12;   // CPU TX -> GPU RX
    static constexpr int GPU_RX_PIN = 11;   // CPU RX <- GPU TX
    
    static bool initialized = false;
    static bool connected = false;
    static uint32_t gpuUptimeMs = 0;    // GPU uptime from PONG response
    static uint32_t lastPingTime = 0;
    static uint32_t lastStatsTime = 0;
    static constexpr uint32_t PING_INTERVAL_MS = 1000;  // Send ping every 1s
    static constexpr uint32_t STATS_INTERVAL_MS = 2000; // Fetch stats every 2s
    
    // GPU stats
    static float gpuFps = 0.0f;
    static uint32_t gpuFreeHeap = 0;
    static uint32_t gpuMinHeap = 0;
    static uint8_t gpuLoad = 0;
    static uint32_t gpuTotalFrames = 0;
    static bool gpuHub75Ok = false;
    static bool gpuOledOk = false;
    
    // GPU command interface
    static GpuCommands gpu;
    
    bool init() {
        if (initialized) return true;
        
        // Initialize using GpuCommands (10Mbps)
        if (!gpu.init(UART_NUM_2, GPU_TX_PIN, GPU_RX_PIN)) {
            printf("  GPU: Init failed\n");
            return false;
        }
        
        initialized = true;
        lastPingTime = 0;
        connected = false;
        printf("  GPU: UART initialized via GpuCommands (TX:%d, RX:%d @ 10Mbps)\n", GPU_TX_PIN, GPU_RX_PIN);
        return true;
    }
    
    // Non-blocking update - periodically ping GPU and fetch stats
    void update(uint32_t currentTimeMs) {
        if (!initialized) return;
        
        // Send periodic ping with response
        if (currentTimeMs - lastPingTime >= PING_INTERVAL_MS) {
            lastPingTime = currentTimeMs;
            
            uint32_t uptime = 0;
            if (gpu.pingWithResponse(uptime, 100)) {  // 100ms timeout
                connected = true;
                gpuUptimeMs = uptime;
            } else {
                connected = false;
                gpuUptimeMs = 0;
            }
        }
        
        // Fetch GPU stats periodically (less frequently than ping)
        if (connected && (currentTimeMs - lastStatsTime >= STATS_INTERVAL_MS)) {
            lastStatsTime = currentTimeMs;
            
            GpuCommands::GpuStatsResponse stats;
            if (gpu.requestStats(stats, 100)) {  // 100ms timeout
                gpuFps = stats.fps;
                gpuFreeHeap = stats.freeHeap;
                gpuMinHeap = stats.minHeap;
                gpuLoad = stats.loadPercent;
                gpuTotalFrames = stats.totalFrames;
                gpuUptimeMs = stats.uptimeMs;
                gpuHub75Ok = stats.hub75Ok;
                gpuOledOk = stats.oledOk;
            }
        }
    }
    
    // Getters
    GpuCommands& getGpu() { return gpu; }
    uint32_t getGpuUptime() { return gpuUptimeMs; }
}

// Simulated sensor values (for sensors not yet implemented)
static float simTemp = 22.5f;
static float simHumidity = 45.0f;
static float simPressure = 1013.25f;

//=============================================================================
// CurrentMode Implementation
//=============================================================================

void CurrentMode::onStart() {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║        CURRENT MODE STARTED        ║\n");
    printf("  ╚════════════════════════════════════╝\n\n");
    
    // Initialize GPS driver
    if (GpsDriver::init()) {
        printf("  GPS: Ready\n");
    } else {
        printf("  GPS: Init failed - will show N/C\n");
    }
    
    // Initialize Microphone driver
    if (MicDriver::init()) {
        printf("  MIC: Ready\n");
    } else {
        printf("  MIC: Init failed - will use simulation\n");
    }
    
    // Initialize IMU driver
    if (ImuDriver::init()) {
        printf("  IMU: Ready\n");
    } else {
        printf("  IMU: Init failed - will use simulation\n");
    }
    
    // Initialize GPU UART driver
    if (GpuDriver::init()) {
        printf("  GPU: UART Ready - waiting for connection\n");
    } else {
        printf("  GPU: UART init failed - will show N/C\n");
    }
    
    // Initialize SD Card
    auto& sdCard = SystemAPI::Utils::FileSystemService::instance();
    SystemAPI::Utils::SdCardPins sdPins = {
        .miso = 14,
        .mosi = 47,
        .clk = 21,
        .cs = 48
    };
    if (sdCard.init(sdPins)) {
        printf("  SD Card: Ready (%llu MB total, %llu MB free)\n", 
               sdCard.getTotalBytes() / (1024 * 1024),
               sdCard.getFreeBytes() / (1024 * 1024));
    } else {
        printf("  SD Card: Not available\n");
    }
    
    // Print initial credentials
    auto& security = arcos::security::SecurityDriver::instance();
    printf("  WiFi SSID: %s\n", security.getSSID());
    printf("  WiFi Pass: %s\n", security.getPassword());
    printf("  Portal IP: 192.168.4.1\n");
    printf("  Easy URL:  Type ANY domain (e.g. go.to, a.a)\n");
    printf("\n");
    
    m_updateCount = 0;
    m_totalTime = 0;
    m_credentialPrintTime = 0;
    
    // Initialize SyncState with simulated data
    auto& state = SystemAPI::SYNC_STATE.state();
    state.mode = SystemAPI::SystemMode::RUNNING;
    snprintf(state.statusText, sizeof(state.statusText), "Running");
}

void CurrentMode::onUpdate(uint32_t deltaMs) {
    m_updateCount++;
    m_totalTime += deltaMs;
    m_credentialPrintTime += deltaMs;
    
    // Update captive portal (handles DNS, HTTP, WebSocket)
    auto& portal = SystemAPI::Web::CaptivePortal::instance();
    portal.update();
    
    // Update hardware drivers (non-blocking)
    uint32_t currentTimeMs = (uint32_t)(esp_timer_get_time() / 1000);
    GpsDriver::update();
    MicDriver::update();
    ImuDriver::update();
    GpuDriver::update(currentTimeMs);
    
    // Get SyncState reference
    auto& state = SystemAPI::SYNC_STATE.state();
    
    // Update system stats
    state.uptime = esp_timer_get_time() / 1000000; // seconds
    state.freeHeap = esp_get_free_heap_size();
    
    // Smooth CPU and FPS values using exponential moving average to prevent jitter
    static float smoothedCpu = 40.0f;
    static float smoothedFps = 60.0f;
    float targetCpu = 35.0f + (rand() % 200) / 10.0f;
    float targetFps = 58.0f + (rand() % 40) / 10.0f;
    smoothedCpu = smoothedCpu * 0.95f + targetCpu * 0.05f;  // Heavy smoothing
    smoothedFps = smoothedFps * 0.95f + targetFps * 0.05f;
    state.cpuUsage = smoothedCpu;
    state.fps = smoothedFps;
    
    // Simulate environmental sensor with slight drift
    simTemp += ((rand() % 20) - 10) / 100.0f;
    if (simTemp < 18.0f) simTemp = 18.0f;
    if (simTemp > 30.0f) simTemp = 30.0f;
    state.temperature = simTemp;
    
    simHumidity += ((rand() % 20) - 10) / 100.0f;
    if (simHumidity < 30.0f) simHumidity = 30.0f;
    if (simHumidity > 70.0f) simHumidity = 70.0f;
    state.humidity = simHumidity;
    
    simPressure += ((rand() % 10) - 5) / 10.0f;
    if (simPressure < 1000.0f) simPressure = 1000.0f;
    if (simPressure > 1030.0f) simPressure = 1030.0f;
    state.pressure = simPressure;
    
    // Update IMU from real driver (values in mg and deg/s)
    state.accelX = ImuDriver::accelX;
    state.accelY = ImuDriver::accelY;
    state.accelZ = ImuDriver::accelZ;
    state.gyroX = ImuDriver::gyroX;
    state.gyroY = ImuDriver::gyroY;
    state.gyroZ = ImuDriver::gyroZ;
    
    // Process IMU calibration (accumulate samples if calibrating)
    SystemAPI::Web::HttpServer::processImuCalibration();
    // Apply IMU calibration to get device-frame values
    SystemAPI::Web::HttpServer::applyImuCalibration();
    
    // Update microphone from real driver (rolling average for stability)
    state.micConnected = MicDriver::initialized;
    state.micLevel = MicDriver::level;
    state.micDb = MicDriver::avgDb;  // Use averaged dB for stable display
    
    // Update GPS from real driver (full data including speed, heading, time)
    state.gpsValid = GpsDriver::valid;
    state.satellites = GpsDriver::satellites;
    state.latitude = GpsDriver::latitude;
    state.longitude = GpsDriver::longitude;
    state.altitude = GpsDriver::altitude;
    state.gpsSpeed = GpsDriver::speed;
    state.gpsHeading = GpsDriver::heading;
    state.gpsHdop = GpsDriver::hdop;
    state.gpsHour = GpsDriver::hour;
    state.gpsMinute = GpsDriver::minute;
    state.gpsSecond = GpsDriver::second;
    state.gpsDay = GpsDriver::day;
    state.gpsMonth = GpsDriver::month;
    state.gpsYear = GpsDriver::year;
    
    // Update GPU connection status
    state.gpuConnected = GpuDriver::connected;
    
    // Update GPU stats
    state.gpuFps = GpuDriver::gpuFps;
    state.gpuFreeHeap = GpuDriver::gpuFreeHeap;
    state.gpuMinHeap = GpuDriver::gpuMinHeap;
    state.gpuLoad = GpuDriver::gpuLoad;
    state.gpuTotalFrames = GpuDriver::gpuTotalFrames;
    state.gpuUptime = GpuDriver::gpuUptimeMs;
    state.gpuHub75Ok = GpuDriver::gpuHub75Ok;
    state.gpuOledOk = GpuDriver::gpuOledOk;
    
    // Print credentials every 10 seconds
    if (m_credentialPrintTime >= 10000) {
        auto& security = arcos::security::SecurityDriver::instance();
        printf("  ----------------------------------------\n");
        printf("  WiFi SSID: %s\n", security.getSSID());
        printf("  WiFi Pass: %s\n", security.getPassword());
        printf("  Portal: 192.168.4.1 or type any URL\n");
        printf("  GPS: %s (Sats: %d, RX: %lu bytes)\n", 
               GpsDriver::valid ? "Fix" : "Searching", 
               GpsDriver::satellites,
               (unsigned long)GpsDriver::bytesReceived);
        printf("  GPU: %s\n", GpuDriver::connected ? "Connected" : "N/C");
        printf("  MIC: %.1f dB (avg)\n", MicDriver::avgDb);
        printf("  ----------------------------------------\n");
        m_credentialPrintTime = 0;
    }
    
    // Example: Print status every 5 seconds
    if (m_totalTime >= 5000) {
        printf("  Update #%lu | Clients: %d\n",
               (unsigned long)m_updateCount,
               portal.getClientCount());
        m_totalTime = 0;
    }
}

void CurrentMode::onStop() {
    printf("  Current mode stopped after %lu updates\n", (unsigned long)m_updateCount);
}

} // namespace Modes
