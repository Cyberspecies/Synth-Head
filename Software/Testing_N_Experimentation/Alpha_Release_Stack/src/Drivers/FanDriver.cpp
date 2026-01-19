/**
 * @file FanDriver.cpp
 * @brief Fan Driver implementation - GPIO On/Off Control
 */

#include "Drivers/FanDriver.hpp"
#include "driver/gpio.h"
#include <cstdio>

namespace Drivers {
namespace FanDriver {

//=============================================================================
// Internal State
//=============================================================================

static bool initialized = false;
static bool currentState = false;  // Track current fan state

//=============================================================================
// Public API
//=============================================================================

bool init() {
    if (initialized) return true;
    
    // Configure fan pins as outputs
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << FAN_1_PIN) | (1ULL << FAN_2_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        printf("  FAN: GPIO config failed: %d\n", err);
        return false;
    }
    
    // Start with fans off
    gpio_set_level((gpio_num_t)FAN_1_PIN, 0);
    gpio_set_level((gpio_num_t)FAN_2_PIN, 0);
    currentState = false;
    
    initialized = true;
    printf("  FAN: Initialized (GPIO %d, %d)\n", FAN_1_PIN, FAN_2_PIN);
    return true;
}

void update(bool enabled) {
    if (!initialized) return;
    
    // Only change GPIO if state changed
    if (enabled != currentState) {
        currentState = enabled;
        gpio_set_level((gpio_num_t)FAN_1_PIN, enabled ? 1 : 0);
        gpio_set_level((gpio_num_t)FAN_2_PIN, enabled ? 1 : 0);
        printf("  FAN: %s\n", enabled ? "ON" : "OFF");
    }
}

bool isOn() {
    return currentState;
}

bool isInitialized() {
    return initialized;
}

} // namespace FanDriver
} // namespace Drivers
