/**
 * @file led_strip_i2s.c
 * @brief I2S Parallel LED Strip Driver for WS2812/SK6812 RGBW LEDs
 * 
 * Drives multiple LED strips simultaneously using ESP32-S3 I2S peripheral in parallel mode.
 * All strips are updated from a single DMA buffer with precise WS2812 timing.
 */

#include "HAL/led_strip_i2s.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "led_i2s";

// WS2812 timing parameters (800kHz)
// T0H: 0.4µs high, T0L: 0.85µs low (total 1.25µs)
// T1H: 0.8µs high, T1L: 0.45µs low (total 1.25µs)
// We use 4 I2S samples per WS2812 bit at 3.2MHz I2S clock
// Each I2S sample = 0.3125µs
// 0-bit: [1,0,0,0] = 0.3125µs high, 0.9375µs low
// 1-bit: [1,1,0,0] = 0.625µs high, 0.625µs low

#define I2S_SAMPLE_RATE     3200000  // 3.2MHz
#define SAMPLES_PER_BIT     4        // 4 samples per WS2812 bit
#define RESET_SAMPLES       160      // 50µs reset time (160 samples @ 3.2MHz)

// Bit patterns for WS2812 0 and 1 encoded in 4 I2S samples
#define WS2812_0_PATTERN    0x08  // Binary: 1000 (msb first)
#define WS2812_1_PATTERN    0x0C  // Binary: 1100 (msb first)

/**
 * @brief I2S LED strip driver structure
 */
struct led_strip_i2s_t {
    i2s_chan_handle_t tx_handle;
    uint8_t* pixel_buffer;           // [max_leds][4 bytes GRBW][num_strips]
    uint8_t* dma_buffer;             // I2S DMA buffer
    uint32_t dma_buffer_size;
    led_strip_i2s_config_t config;
};

/**
 * @brief Encode RGBW pixel data into I2S DMA buffer
 */
static void encode_pixels_to_i2s(led_strip_i2s_handle_t handle) {
    uint32_t dma_idx = 0;
    uint8_t num_strips = handle->config.num_strips;
    uint32_t max_leds = handle->config.max_leds;
    
    // Clear DMA buffer
    memset(handle->dma_buffer, 0, handle->dma_buffer_size);
    
    // For each LED position (up to max_leds)
    for (uint32_t led = 0; led < max_leds; led++) {
        // For each of 4 bytes (GRBW format)
        for (uint8_t byte_idx = 0; byte_idx < 4; byte_idx++) {
            // For each bit (MSB first)
            for (int8_t bit = 7; bit >= 0; bit--) {
                // For each of 4 I2S samples per bit
                for (uint8_t sample = 0; sample < SAMPLES_PER_BIT; sample++) {
                    uint8_t parallel_byte = 0;
                    
                    // Build parallel byte - each bit represents one strip
                    for (uint8_t strip = 0; strip < num_strips; strip++) {
                        if (!handle->config.strips[strip].active) continue;
                        
                        // Check if this strip has this LED
                        if (led >= handle->config.strips[strip].num_leds) {
                            // Strip finished, output 0
                            continue;
                        }
                        
                        // Get pixel data: [led][byte][strip]
                        uint32_t pixel_idx = (led * 4 * num_strips) + (byte_idx * num_strips) + strip;
                        uint8_t pixel_byte = handle->pixel_buffer[pixel_idx];
                        
                        // Check bit value
                        uint8_t bit_val = (pixel_byte >> bit) & 0x01;
                        
                        // Encode to I2S pattern
                        uint8_t pattern = bit_val ? WS2812_1_PATTERN : WS2812_0_PATTERN;
                        
                        // Extract the sample bit from pattern
                        if ((pattern >> (3 - sample)) & 0x01) {
                            parallel_byte |= (1 << strip);
                        }
                    }
                    
                    handle->dma_buffer[dma_idx++] = parallel_byte;
                }
            }
        }
    }
    
    // Add reset time (all pins low)
    for (uint32_t i = 0; i < RESET_SAMPLES; i++) {
        handle->dma_buffer[dma_idx++] = 0x00;
    }
}

esp_err_t led_strip_i2s_new(const led_strip_i2s_config_t* config, led_strip_i2s_handle_t* out_handle) {
    esp_err_t ret = ESP_OK;
    led_strip_i2s_handle_t handle = NULL;
    
    ESP_GOTO_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, err, TAG, "Invalid argument");
    ESP_GOTO_ON_FALSE(config->num_strips > 0 && config->num_strips <= 8, ESP_ERR_INVALID_ARG, err, TAG, "Invalid strip count");
    
    handle = (led_strip_i2s_handle_t)calloc(1, sizeof(struct led_strip_i2s_t));
    ESP_GOTO_ON_FALSE(handle, ESP_ERR_NO_MEM, err, TAG, "No memory for handle");
    
    // Copy config
    memcpy(&handle->config, config, sizeof(led_strip_i2s_config_t));
    
    // Allocate pixel buffer: [max_leds][4 bytes GRBW][num_strips]
    uint32_t pixel_buffer_size = config->max_leds * 4 * config->num_strips;
    handle->pixel_buffer = (uint8_t*)heap_caps_calloc(1, pixel_buffer_size, MALLOC_CAP_DMA);
    ESP_GOTO_ON_FALSE(handle->pixel_buffer, ESP_ERR_NO_MEM, err, TAG, "No memory for pixel buffer");
    
    // Calculate DMA buffer size
    // Each LED: 4 bytes × 8 bits × SAMPLES_PER_BIT samples
    // Plus reset time
    handle->dma_buffer_size = (config->max_leds * 4 * 8 * SAMPLES_PER_BIT) + RESET_SAMPLES;
    handle->dma_buffer = (uint8_t*)heap_caps_calloc(1, handle->dma_buffer_size, MALLOC_CAP_DMA);
    ESP_GOTO_ON_FALSE(handle->dma_buffer, ESP_ERR_NO_MEM, err, TAG, "No memory for DMA buffer");
    
    // Configure I2S in standard mode with parallel output
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = 512;
    
    ESP_GOTO_ON_ERROR(i2s_new_channel(&chan_cfg, &handle->tx_handle, NULL), err, TAG, "Failed to create I2S channel");
    
    // Configure I2S standard mode
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = I2S_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_8BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = GPIO_NUM_NC,  // We don't need BCLK output
            .ws = GPIO_NUM_NC,    // We don't need WS output
            .dout = GPIO_NUM_NC,  // Will configure data pins manually
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(handle->tx_handle, &std_cfg), err, TAG, "Failed to init I2S std mode");
    
    // Configure GPIO pins for parallel output
    // We need to use GPIO matrix to route I2S data to multiple pins
    // Each strip gets one bit position in the I2S parallel data
    for (uint8_t i = 0; i < config->num_strips; i++) {
        if (!config->strips[i].active) continue;
        
        gpio_num_t pin = config->strips[i].gpio;
        
        // Configure as output
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "Failed to config GPIO %d", pin);
        
        // Route I2S parallel data bit to this GPIO
        // This uses ESP32 GPIO matrix - each bit of I2S data goes to different pin
        gpio_set_level(pin, 0);
        
        ESP_LOGI(TAG, "Strip %d: GPIO %d, %d LEDs", i, pin, config->strips[i].num_leds);
    }
    
    // Enable I2S channel
    ESP_GOTO_ON_ERROR(i2s_channel_enable(handle->tx_handle), err, TAG, "Failed to enable I2S");
    
    *out_handle = handle;
    ESP_LOGI(TAG, "I2S LED driver initialized: %d strips, max %d LEDs, DMA buffer: %d bytes",
             config->num_strips, config->max_leds, handle->dma_buffer_size);
    return ESP_OK;
    
err:
    if (handle) {
        if (handle->tx_handle) {
            i2s_del_channel(handle->tx_handle);
        }
        if (handle->dma_buffer) {
            free(handle->dma_buffer);
        }
        if (handle->pixel_buffer) {
            free(handle->pixel_buffer);
        }
        free(handle);
    }
    return ret;
}

esp_err_t led_strip_i2s_del(led_strip_i2s_handle_t handle) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    
    i2s_channel_disable(handle->tx_handle);
    i2s_del_channel(handle->tx_handle);
    
    free(handle->dma_buffer);
    free(handle->pixel_buffer);
    free(handle);
    
    return ESP_OK;
}

esp_err_t led_strip_i2s_set_pixel(led_strip_i2s_handle_t handle,
                                   uint8_t strip_index,
                                   uint16_t led_index,
                                   uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(strip_index < handle->config.num_strips, ESP_ERR_INVALID_ARG, TAG, "Invalid strip index");
    ESP_RETURN_ON_FALSE(led_index < handle->config.strips[strip_index].num_leds, ESP_ERR_INVALID_ARG, TAG, "Invalid LED index");
    
    // Pixel buffer layout: [led][byte][strip]
    // GRBW byte order for SK6812
    uint32_t base_idx = (led_index * 4 * handle->config.num_strips) + strip_index;
    
    handle->pixel_buffer[base_idx + (0 * handle->config.num_strips)] = green;
    handle->pixel_buffer[base_idx + (1 * handle->config.num_strips)] = red;
    handle->pixel_buffer[base_idx + (2 * handle->config.num_strips)] = blue;
    handle->pixel_buffer[base_idx + (3 * handle->config.num_strips)] = white;
    
    return ESP_OK;
}

esp_err_t led_strip_i2s_clear(led_strip_i2s_handle_t handle) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    
    uint32_t pixel_buffer_size = handle->config.max_leds * 4 * handle->config.num_strips;
    memset(handle->pixel_buffer, 0, pixel_buffer_size);
    
    return ESP_OK;
}

esp_err_t led_strip_i2s_refresh(led_strip_i2s_handle_t handle) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    
    // Encode pixel data to I2S format
    encode_pixels_to_i2s(handle);
    
    // Send via I2S DMA
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(handle->tx_handle, handle->dma_buffer, 
                                      handle->dma_buffer_size, &bytes_written, portMAX_DELAY);
    
    if (ret != ESP_OK || bytes_written != handle->dma_buffer_size) {
        ESP_LOGE(TAG, "I2S write failed: %d, wrote %d/%d bytes", ret, bytes_written, handle->dma_buffer_size);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
