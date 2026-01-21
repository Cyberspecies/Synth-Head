#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2S Parallel LED Strip Driver
 * 
 * Drives up to 8 LED strips simultaneously using I2S peripheral in parallel mode.
 * All strips are updated at the same time from a single DMA buffer.
 */

typedef struct led_strip_i2s_t* led_strip_i2s_handle_t;

/**
 * @brief LED strip configuration
 */
typedef struct {
    gpio_num_t gpio;        // GPIO pin for this strip
    uint16_t num_leds;      // Number of LEDs in this strip
    bool active;            // Whether this strip is active
} led_strip_i2s_strip_config_t;

/**
 * @brief I2S LED driver configuration
 */
typedef struct {
    led_strip_i2s_strip_config_t strips[8];  // Up to 8 strips
    uint8_t num_strips;                      // Number of active strips
    uint32_t max_leds;                       // Maximum LEDs across all strips
} led_strip_i2s_config_t;

/**
 * @brief Initialize I2S LED strip driver
 * 
 * @param config Configuration for LED strips
 * @param out_handle Output handle for the driver
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_strip_i2s_new(const led_strip_i2s_config_t* config, led_strip_i2s_handle_t* out_handle);

/**
 * @brief Delete I2S LED strip driver
 * 
 * @param handle Driver handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_strip_i2s_del(led_strip_i2s_handle_t handle);

/**
 * @brief Set pixel color for a specific strip (RGBW format)
 * 
 * @param handle Driver handle
 * @param strip_index Strip index (0-7)
 * @param led_index LED index within the strip
 * @param red Red value (0-255)
 * @param green Green value (0-255)
 * @param blue Blue value (0-255)
 * @param white White value (0-255)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_strip_i2s_set_pixel(led_strip_i2s_handle_t handle, 
                                   uint8_t strip_index,
                                   uint16_t led_index,
                                   uint8_t red, uint8_t green, uint8_t blue, uint8_t white);

/**
 * @brief Clear all pixels on all strips
 * 
 * @param handle Driver handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_strip_i2s_clear(led_strip_i2s_handle_t handle);

/**
 * @brief Refresh all LED strips (send data via I2S DMA)
 * 
 * @param handle Driver handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_strip_i2s_refresh(led_strip_i2s_handle_t handle);

#ifdef __cplusplus
}
#endif
