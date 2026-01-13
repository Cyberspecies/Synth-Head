/*****************************************************************
 * @file WebTypes.hpp
 * @brief Common types and constants for Web module
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>

namespace SystemAPI {
namespace Web {

// ============================================================
// Constants
// ============================================================

/** Portal IP address */
constexpr const char* PORTAL_IP = "192.168.4.1";
constexpr uint8_t PORTAL_IP_BYTES[4] = {192, 168, 4, 1};

/** Server ports */
constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t DNS_PORT = 53;

/** WiFi AP settings */
constexpr uint8_t MAX_WIFI_CLIENTS = 4;
constexpr uint8_t WIFI_CHANNEL = 1;

/** Buffer sizes */
constexpr size_t DNS_BUFFER_SIZE = 512;
constexpr size_t HTTP_BUFFER_SIZE = 1024;
constexpr size_t MAX_SSID_LENGTH = 32;
constexpr size_t MAX_PASSWORD_LENGTH = 64;
constexpr size_t MAX_HOST_HEADER_LENGTH = 128;

// ============================================================
// Enums
// ============================================================

/**
 * @brief HTTP request method
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS,
    UNKNOWN
};

/**
 * @brief HTTP status codes
 */
enum class HttpStatus {
    OK = 200,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    BAD_REQUEST = 400,
    NOT_FOUND = 404,
    INTERNAL_ERROR = 500
};

/**
 * @brief Command types for API
 */
enum class CommandType {
    SET_BRIGHTNESS,
    SET_WIFI_CREDENTIALS,
    RESET_WIFI_TO_AUTO,
    RESTART,
    KICK_CLIENTS,
    SET_EXT_WIFI,
    EXT_WIFI_CONNECT,
    SET_AUTH,
    UNKNOWN
};

/**
 * @brief Convert string to CommandType
 */
inline CommandType stringToCommand(const char* cmd) {
    if (!cmd) return CommandType::UNKNOWN;
    
    if (strcmp(cmd, "setBrightness") == 0) return CommandType::SET_BRIGHTNESS;
    if (strcmp(cmd, "setWifiCredentials") == 0) return CommandType::SET_WIFI_CREDENTIALS;
    if (strcmp(cmd, "resetWifiToAuto") == 0) return CommandType::RESET_WIFI_TO_AUTO;
    if (strcmp(cmd, "restart") == 0) return CommandType::RESTART;
    if (strcmp(cmd, "kickOtherClients") == 0) return CommandType::KICK_CLIENTS;
    if (strcmp(cmd, "setExtWifi") == 0) return CommandType::SET_EXT_WIFI;
    if (strcmp(cmd, "extWifiConnect") == 0) return CommandType::EXT_WIFI_CONNECT;
    if (strcmp(cmd, "setAuth") == 0) return CommandType::SET_AUTH;
    
    return CommandType::UNKNOWN;
}

// ============================================================
// Structures
// ============================================================

/**
 * @brief WiFi credentials
 */
struct WifiCredentials {
    char ssid[MAX_SSID_LENGTH + 1] = {0};
    char password[MAX_PASSWORD_LENGTH + 1] = {0};
    bool isCustom = false;
};

/**
 * @brief Portal configuration
 */
struct PortalConfig {
    char ssid[MAX_SSID_LENGTH + 1] = "Lucidius-AP";
    char password[MAX_PASSWORD_LENGTH + 1] = "";
    bool enableDns = true;
    bool enableCaptivePortal = true;
};

/**
 * @brief Device state for API responses
 */
struct DeviceState {
    char ssid[MAX_SSID_LENGTH + 1] = {0};
    char ip[16] = {0};
    uint8_t clients = 0;
    uint32_t uptime = 0;
    uint32_t freeHeap = 0;
    uint8_t brightness = 100;
    bool wifiCustom = false;
};

} // namespace Web
} // namespace SystemAPI
