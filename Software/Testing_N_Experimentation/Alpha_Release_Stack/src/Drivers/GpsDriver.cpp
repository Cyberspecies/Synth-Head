/**
 * @file GpsDriver.cpp
 * @brief GPS NMEA Parser implementation
 */

#include "Drivers/GpsDriver.hpp"
#include "driver/uart.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace Drivers {
namespace GpsDriver {

//=============================================================================
// Internal State
//=============================================================================

static bool initialized = false;
static char nmeaBuffer[256];
static int nmeaIndex = 0;

//=============================================================================
// Exported GPS Data
//=============================================================================

float latitude = 0.0f;
float longitude = 0.0f;
float altitude = 0.0f;
float speed = 0.0f;
float heading = 0.0f;
int satellites = 0;
float hdop = 99.9f;
bool valid = false;
int hour = 0;
int minute = 0;
int second = 0;
int day = 0;
int month = 0;
int year = 0;
unsigned long bytesReceived = 0;

//=============================================================================
// NMEA Parsing Helpers
//=============================================================================

/**
 * @brief Parse NMEA coordinate (DDDMM.MMMMM format) to decimal degrees
 */
static float parseCoordinate(const char* str, char direction) {
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

/**
 * @brief Parse $GPGGA sentence for position and satellites
 * Format: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47
 */
static void parseGGA(char* sentence) {
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

/**
 * @brief Parse $GPRMC/$GNRMC sentence for time, date, speed, and heading
 * Format: $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
 */
static void parseRMC(char* sentence) {
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

/**
 * @brief Parse $GPVTG/$GNVTG sentence for speed and heading (alternative source)
 * Format: $GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48
 */
static void parseVTG(char* sentence) {
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

/**
 * @brief Parse a complete NMEA sentence
 */
static void parseSentence(char* sentence) {
    if (strncmp(sentence, "$GPGGA", 6) == 0 || strncmp(sentence, "$GNGGA", 6) == 0) {
        parseGGA(sentence);
    } else if (strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0) {
        parseRMC(sentence);
    } else if (strncmp(sentence, "$GPVTG", 6) == 0 || strncmp(sentence, "$GNVTG", 6) == 0) {
        parseVTG(sentence);
    }
}

//=============================================================================
// Public API
//=============================================================================

bool init() {
    if (initialized) return true;
    
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT
    };
    
    esp_err_t err = uart_param_config((uart_port_t)GPS_UART, &uart_config);
    if (err != ESP_OK) {
        printf("  GPS: UART config failed: %d\n", err);
        return false;
    }
    
    err = uart_set_pin((uart_port_t)GPS_UART, GPS_RX_PIN, GPS_TX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        printf("  GPS: UART set pins failed: %d\n", err);
        return false;
    }
    
    err = uart_driver_install((uart_port_t)GPS_UART, 1024, 0, 0, nullptr, 0);
    if (err != ESP_OK) {
        printf("  GPS: UART driver install failed: %d\n", err);
        return false;
    }
    
    initialized = true;
    printf("  GPS: Initialized on UART%d (TX:%d, RX:%d @ %d baud)\n", 
           GPS_UART, GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);
    return true;
}

void update() {
    if (!initialized) return;
    
    size_t available = 0;
    uart_get_buffered_data_len((uart_port_t)GPS_UART, &available);
    
    while (available > 0) {
        char c;
        int len = uart_read_bytes((uart_port_t)GPS_UART, &c, 1, 0);
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

bool isInitialized() {
    return initialized;
}

} // namespace GpsDriver
} // namespace Drivers
