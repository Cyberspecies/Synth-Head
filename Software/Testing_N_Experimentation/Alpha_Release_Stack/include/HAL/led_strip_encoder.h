/*****************************************************************
 * @file led_strip_encoder.h
 * @brief RMT encoder for WS2812/SK6812 LED strips
 * 
 * Based on ESP-IDF's led_strip example. Uses RMT peripheral for
 * precise timing required by WS2812B addressable LEDs.
 * 
 * @author ESP-IDF / ARCOS
 *****************************************************************/

#pragma once

#include "driver/rmt_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED strip encoder configuration
 */
typedef struct {
    uint32_t resolution;    /*!< Encoder resolution, in Hz */
} led_strip_encoder_config_t;

/**
 * @brief Create RMT encoder for encoding LED strip pixels into RMT symbols
 *
 * @param[in] config Encoder configuration
 * @param[out] ret_encoder Returned encoder handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if config or ret_encoder is NULL
 *      - ESP_ERR_NO_MEM if memory allocation failed
 *      - ESP_FAIL on other errors
 */
esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);

#ifdef __cplusplus
}
#endif
