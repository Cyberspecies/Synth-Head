/**
 * @file parametric_led_gpu_sender.cpp
 * @brief GPU Parametric LED Sender - Sends animation parameters instead of raw pixels
 * 
 * Bandwidth optimization: Send 16 bytes only when parameters change, not 196 bytes @ 60 FPS
 * CPU reconstructs animation locally
 * 
 * Flow:
 * 1. GPU calculates animation parameters
 * 2. GPU sends parameters only when they change (or every 1 second as keepalive)
 * 3. GPU receives button state via UART
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// ===== UART Configuration =====
#define UART_PORT_NUM UART_NUM_1
#define UART_BAUD_RATE 921600
#define UART_TX_PIN GPIO_NUM_12
#define UART_RX_PIN GPIO_NUM_13
#define UART_BUF_SIZE 1024

// ===== Animation Parameters =====
enum AnimationType {
    ANIM_OFF = 0,
    ANIM_SOLID = 1,
    ANIM_RAINBOW = 2,
    ANIM_GRADIENT = 3,
    ANIM_WAVE = 4,
    ANIM_BREATHING = 5
};

struct __attribute__((packed)) AnimationParams {
    uint16_t magic;         // 0xAA55
    uint8_t animation_type;
    uint8_t frame_counter;
    float param1;
    float param2;
    float param3;
    uint8_t crc8;
};

struct __attribute__((packed)) ButtonDataPacket {
    uint16_t magic;         // 0x5AA5
    uint8_t button_a;
    uint8_t button_b;
    uint8_t button_c;
    uint8_t button_d;
    uint8_t crc8;
};

// ===== Global Variables =====
static const char *TAG = "GPU_PARAM_SENDER";

// Animation state
static AnimationParams current_params = {0};
static AnimationParams last_sent_params = {0};
static uint8_t param_counter = 0;
static uint32_t params_sent = 0;
static uint64_t last_param_send_time = 0;

// Button state
static bool button_a = false;
static bool button_b = false;
static bool button_c = false;
static bool button_d = false;

// ===== CRC8 Calculation =====
static uint8_t calculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ===== UART Initialization =====
static void initUart() {
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "UART initialized: TX=%d, RX=%d, Baud=%d", UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);
}

// ===== Check if parameters changed significantly =====
static bool paramsChanged(const AnimationParams* a, const AnimationParams* b) {
    if (a->animation_type != b->animation_type) return true;
    if (fabs(a->param1 - b->param1) > 0.1f) return true;
    if (fabs(a->param2 - b->param2) > 0.01f) return true;
    if (fabs(a->param3 - b->param3) > 0.01f) return true;
    return false;
}

// ===== Send Animation Parameters =====
static void sendAnimationParams() {
    param_counter++;
    current_params.frame_counter = param_counter;
    current_params.magic = 0xAA55;
    
    // Calculate CRC
    current_params.crc8 = 0;
    current_params.crc8 = calculateCRC8((uint8_t*)&current_params, sizeof(AnimationParams) - 1);

    // Send via UART
    int sent = uart_write_bytes(UART_PORT_NUM, &current_params, sizeof(AnimationParams));
    
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send parameters");
    } else {
        params_sent++;
        last_param_send_time = esp_timer_get_time();
        memcpy(&last_sent_params, &current_params, sizeof(AnimationParams));
    }
}

// ===== Receive Button State =====
static void receiveButtonState() {
    ButtonDataPacket packet;
    int len = uart_read_bytes(UART_PORT_NUM, &packet, sizeof(ButtonDataPacket), 0);
    
    if (len == sizeof(ButtonDataPacket)) {
        // Validate magic and CRC
        if (packet.magic == 0x5AA5) {
            uint8_t received_crc = packet.crc8;
            packet.crc8 = 0;
            uint8_t calculated_crc = calculateCRC8((uint8_t*)&packet, sizeof(ButtonDataPacket) - 1);
            
            if (received_crc == calculated_crc) {
                button_a = packet.button_a;
                button_b = packet.button_b;
                button_c = packet.button_c;
                button_d = packet.button_d;
            }
        }
    }
}

// ===== Animation Task =====
static void animation_task(void* pvParameters) {
    ESP_LOGI(TAG, "Animation task started");
    
    uint64_t start_time = esp_timer_get_time();
    uint32_t frame_count = 0;
    uint32_t last_stat_time = 0;
    uint32_t stats_sent_count = 0;

    while (1) {
        uint64_t current_time = esp_timer_get_time();
        uint32_t current_millis = current_time / 1000;
        
        // ===== Update Animation Parameters (Every Frame) =====
        // RAINBOW animation with slowly changing hue offset
        current_params.animation_type = ANIM_RAINBOW;
        current_params.param1 = fmod((current_millis / 100.0f), 360.0f); // Global hue rotation
        current_params.param2 = 0.6f;  // Hue speed (degrees per frame on CPU)
        current_params.param3 = 1.0f;  // Full brightness

        // ===== Send Parameters (Only if Changed or Timeout) =====
        bool should_send = false;
        
        // Send if parameters changed significantly
        if (paramsChanged(&current_params, &last_sent_params)) {
            should_send = true;
        }
        
        // Send keepalive every 1 second
        if ((current_time - last_param_send_time) > 1000000) {
            should_send = true;
        }

        if (should_send) {
            sendAnimationParams();
        }

        // ===== Receive Button State =====
        receiveButtonState();

        // ===== Print Statistics (Every 5 seconds) =====
        if (current_millis - last_stat_time >= 5000) {
            uint32_t params_per_5sec = params_sent - stats_sent_count;
            float params_per_sec = params_per_5sec / 5.0f;
            
            ESP_LOGI(TAG, "===== STATS =====");
            ESP_LOGI(TAG, "Params sent: %u total (%.1f/sec)", params_sent, params_per_sec);
            ESP_LOGI(TAG, "Animation: Type=%d P1=%.1f P2=%.2f P3=%.2f",
                     current_params.animation_type,
                     current_params.param1,
                     current_params.param2,
                     current_params.param3);
            ESP_LOGI(TAG, "Buttons: A=%d B=%d C=%d D=%d", button_a, button_b, button_c, button_d);
            ESP_LOGI(TAG, "================");
            
            last_stat_time = current_millis;
            stats_sent_count = params_sent;
        }

        frame_count++;
        
        // Run at ~60 FPS (parameter calculation rate)
        vTaskDelay(pdMS_TO_TICKS(17));
    }
}

// ===== Main Application =====
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "===== Parametric LED GPU Sender (UART) =====");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize UART
    initUart();

    // Create animation task
    xTaskCreate(animation_task, "animation_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "GPU sender initialized successfully");
}
