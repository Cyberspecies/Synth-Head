/**
 * @file MicDriver.cpp
 * @brief Microphone Driver implementation - I2S INMP441
 */

#include "Drivers/MicDriver.hpp"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include <cstdio>
#include <cmath>

namespace Drivers {
namespace MicDriver {

//=============================================================================
// Internal State
//=============================================================================

static i2s_chan_handle_t rxHandle = nullptr;

/// Rolling window for dB averaging
static float dbWindow[WINDOW_SIZE] = {0};
static int windowIndex = 0;

/// Sample buffer
static int32_t sampleBuffer[64];

//=============================================================================
// Exported Audio Data
//=============================================================================

bool initialized = false;
float avgDb = -60.0f;
float currentDb = -60.0f;
uint8_t level = 0;

//=============================================================================
// Public API
//=============================================================================

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

bool isInitialized() {
    return initialized;
}

} // namespace MicDriver
} // namespace Drivers
