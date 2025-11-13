/*****************************************************************
 * File:      CaptivePortalManager.impl.hpp
 * Category:  Manager/WiFi
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Implementation of captive portal manager.
 *****************************************************************/

#ifndef CAPTIVE_PORTAL_MANAGER_IMPL_HPP
#define CAPTIVE_PORTAL_MANAGER_IMPL_HPP

#include <esp_random.h>

namespace arcos::manager {

constexpr const char* DEVICE_BASE_NAME = "SynthHead";
constexpr uint16_t DNS_PORT = 53;
constexpr uint32_t AP_CHANNEL = 1;
constexpr uint32_t MAX_CONNECTIONS = 4;

// Include the full implementation from CaptivePortalManager.cpp here
// This will be copied over in the next step

} // namespace arcos::manager

#endif // CAPTIVE_PORTAL_MANAGER_IMPL_HPP
