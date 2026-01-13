/*****************************************************************
 * @file ICommandHandler.hpp
 * @brief Interface for handling API commands
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "SystemAPI/Web/WebTypes.hpp"
#include "cJSON.h"

namespace SystemAPI {
namespace Web {

/**
 * @brief Interface for command handlers
 * 
 * Implement this interface to handle different commands from the web API.
 */
class ICommandHandler {
public:
    virtual ~ICommandHandler() = default;
    
    /**
     * @brief Handle a command
     * @param type The command type
     * @param params JSON parameters (may be null)
     * @return true if command was handled successfully
     */
    virtual bool handleCommand(CommandType type, cJSON* params) = 0;
    
    /**
     * @brief Check if this handler can process a command type
     * @param type The command type
     * @return true if this handler supports the command
     */
    virtual bool canHandle(CommandType type) const = 0;
};

/**
 * @brief Interface for state provider
 * 
 * Provides device state for API responses.
 */
class IStateProvider {
public:
    virtual ~IStateProvider() = default;
    
    /**
     * @brief Get current device state
     * @param state Output state structure
     */
    virtual void getState(DeviceState& state) = 0;
};

/**
 * @brief Interface for WiFi operations
 */
class IWifiManager {
public:
    virtual ~IWifiManager() = default;
    
    /**
     * @brief Set custom WiFi credentials
     * @param ssid Network name
     * @param password Network password
     * @return true on success
     */
    virtual bool setCredentials(const char* ssid, const char* password) = 0;
    
    /**
     * @brief Reset to auto-generated credentials
     * @return true on success
     */
    virtual bool resetToAuto() = 0;
    
    /**
     * @brief Kick all connected clients
     * @return Number of clients kicked
     */
    virtual int kickAllClients() = 0;
    
    /**
     * @brief Get current client count
     */
    virtual uint8_t getClientCount() const = 0;
};

} // namespace Web
} // namespace SystemAPI
