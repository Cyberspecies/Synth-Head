/**
 * @file parametric_led_cpu_receiver.cpp
 * @brief CPU Parametric LED Receiver - Generates animations from parameters
 * 
 * Bandwidth optimization: Instead of 196 bytes @ 60 FPS, receive ~16 bytes only when parameters change
 * CPU generates 60 FPS animation locally from parameters
 * 
 * Flow:
 * 1. CPU receives animation parameter updates via UART (only when changed)
 * 2. CPU generates LED data at 60 FPS locally
 * 3. CPU sends button state via UART
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include "LedController_new.h"
#include "LedController_new.impl.hpp"
#include "ParametricLedProtocol.h"
#include "ParametricAnimator.h"
#include "ParametricAnimator.impl.hpp"

// UART Configuration
#define UART_BAUD_RATE 921600
#define UART_RX_PIN 11
#define UART_TX_PIN 12

HardwareSerial UartSerial(1);

// LED controller and animator
LedController led_controller;
ParametricAnimator animator;

// Button pins (from PIN_MAPPING_CPU.md)
constexpr uint8_t BUTTON_A_PIN = 5;
constexpr uint8_t BUTTON_B_PIN = 6;
constexpr uint8_t BUTTON_C_PIN = 7;
constexpr uint8_t BUTTON_D_PIN = 15;

// Statistics
uint32_t params_received = 0;
uint32_t params_corrupted = 0;
uint32_t params_skipped = 0;
uint32_t last_param_counter = 0;
uint32_t frames_generated = 0;
uint32_t last_stat_time = 0;

// LED update timing (60 FPS = 16.67ms per frame)
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 17; // ~60 FPS
uint32_t last_led_update_time = 0;

// Temporary buffer for generated LED data
uint8_t generated_led_data[196]; // 49 LEDs × 4 bytes RGBW

// Process received animation parameters
void processAnimationParams(AnimationParams* params) {
    // Validate magic number
    if (params->magic != 0xAA55) {
        params_corrupted++;
        return;
    }

    // Validate CRC
    uint8_t received_crc = params->crc8;
    params->crc8 = 0;
    uint8_t calculated_crc = calculateCRC8((uint8_t*)params, sizeof(AnimationParams) - 1);
    
    if (received_crc != calculated_crc) {
        params_corrupted++;
        return;
    }

    // Detect skipped parameter updates
    if (params_received > 0) {
        uint8_t expected = (last_param_counter % 255) + 1;
        if (params->frame_counter != expected) {
            int16_t skipped = params->frame_counter - expected;
            if (skipped < 0) skipped += 256;
            params_skipped += skipped;
        }
    }

    last_param_counter = params->frame_counter;
    params_received++;

    // Update animator with new parameters
    animator.updateParams(*params);

    // Print parameter update
    Serial.printf("PARAMS: Type=%d | P1=%.2f P2=%.2f P3=%.2f | Counter=%d\n",
                  params->animation_type, params->param1, params->param2, 
                  params->param3, params->frame_counter);
}

// Read button states
void readButtons(ButtonDataPacket& packet) {
    packet.button_a = !digitalRead(BUTTON_A_PIN); // Active LOW
    packet.button_b = !digitalRead(BUTTON_B_PIN);
    packet.button_c = !digitalRead(BUTTON_C_PIN);
    packet.button_d = !digitalRead(BUTTON_D_PIN);
}

// Send button state to GPU
void sendButtonState() {
    ButtonDataPacket packet;
    packet.magic = 0x5AA5;
    readButtons(packet);
    
    // Calculate CRC
    packet.crc8 = 0;
    packet.crc8 = calculateCRC8((uint8_t*)&packet, sizeof(ButtonDataPacket) - 1);

    // Send via UART
    UartSerial.write((uint8_t*)&packet, sizeof(ButtonDataPacket));
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== Parametric LED CPU Receiver (UART) ===");

    // Initialize UART
    UartSerial.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial.printf("UART initialized: RX=%d, TX=%d, Baud=%d\n", UART_RX_PIN, UART_TX_PIN, UART_BAUD_RATE);

    // Initialize button pins
    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);
    pinMode(BUTTON_C_PIN, INPUT_PULLUP);
    pinMode(BUTTON_D_PIN, INPUT_PULLUP);

    // Initialize LED controller
    Serial.println("Initializing LED strips...");
    led_controller.initialize();
    
    // Test pattern
    Serial.println("Running test pattern...");
    led_controller.testPattern();
    delay(1000);

    Serial.println("✓ Ready! Waiting for animation parameters...");

    last_stat_time = millis();
    last_led_update_time = millis();
}

void loop() {
    uint32_t current_time = millis();

    // ===== Receive Animation Parameters via UART =====
    if (UartSerial.available() >= sizeof(AnimationParams)) {
        AnimationParams params;
        UartSerial.readBytes((uint8_t*)&params, sizeof(AnimationParams));
        processAnimationParams(&params);
    }

    // ===== Generate and Update LEDs at 60 FPS =====
    if (current_time - last_led_update_time >= LED_UPDATE_INTERVAL_MS) {
        last_led_update_time = current_time;

        // Generate LED data from current animation parameters
        animator.generateFrame(generated_led_data, 49);
        frames_generated++;

        // Update LED strips with generated data
        // Split data into 4 strips: Left Fin (13), Right Fin (13), Tongue (9), Scale (14)
        const uint8_t* left_fin = generated_led_data;            // 0-12
        const uint8_t* right_fin = generated_led_data + (13*4);  // 13-25
        const uint8_t* tongue = generated_led_data + (26*4);     // 26-34
        const uint8_t* scale = generated_led_data + (35*4);      // 35-48

        led_controller.updateFromUartData(left_fin, right_fin, tongue, scale);
    }

    // ===== Send Button State (10 Hz) =====
    static uint32_t last_button_send = 0;
    if (current_time - last_button_send >= 100) { // 10 Hz
        last_button_send = current_time;
        sendButtonState();
    }

    // ===== Print Statistics (1 Hz) =====
    if (current_time - last_stat_time >= 1000) {
        uint32_t elapsed = current_time - last_stat_time;
        float params_fps = (params_received * 1000.0f) / elapsed;
        float gen_fps = (frames_generated * 1000.0f) / elapsed;
        float skip_rate = params_received > 0 ? (params_skipped * 100.0f) / params_received : 0.0f;

        Serial.println("=====================================");
        Serial.printf("PARAM UPDATE FPS: %.1f | Generated FPS: %.1f\n", params_fps, gen_fps);
        Serial.printf("Received: %u | Skipped: %u (%.1f%%) | Corrupted: %u\n",
                      params_received, params_skipped, skip_rate, params_corrupted);
        Serial.println("=====================================");

        // Reset counters
        params_received = 0;
        params_skipped = 0;
        frames_generated = 0;
        last_stat_time = current_time;
    }
}

