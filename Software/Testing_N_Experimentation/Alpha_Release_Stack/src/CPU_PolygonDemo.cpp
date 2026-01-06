/*****************************************************************
 * CPU_PolygonDemo.cpp - Simple filled polygon with RGB effect
 * 
 * Displays a filled polygon on both HUB75 panels with animated
 * RGB color cycling. OLED stays black.
 *****************************************************************/

#include <cstdio>
#include <cstring>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "POLY_DEMO";

// UART config
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int UART_TX_PIN = 12;
constexpr int UART_RX_PIN = 11;
constexpr int UART_BAUD = 10000000;

// Protocol
constexpr uint8_t SYNC0 = 0xAA;
constexpr uint8_t SYNC1 = 0x55;

enum CmdType : uint8_t {
    CLEAR = 0x47,
    SET_TARGET = 0x50,
    PRESENT = 0x51,
    OLED_CLEAR = 0x60,
    OLED_PRESENT = 0x65,
    SET_VAR = 0x30,
    DRAW_POLY = 0x45,
};

// Polygon vertices (scaled for 128x32 display)
// Original: {6,8},{14,8},{20,11},{26,17},{27,19},{28,22},{23,22},{21,19},{19,17},{17,17},{16,19},{18,22},{7,22},{4,20},{2,17},{2,12}
static const int16_t POLY_X[] = {6, 14, 20, 26, 27, 28, 23, 21, 19, 17, 16, 18, 7, 4, 2, 2};
static const int16_t POLY_Y[] = {8, 8, 11, 17, 19, 22, 22, 19, 17, 17, 19, 22, 22, 20, 17, 12};
static const int NUM_VERTS = 16;

static void sendCmd(CmdType type, const uint8_t* payload, uint16_t len) {
    uint8_t header[5] = {SYNC0, SYNC1, type, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
    uart_write_bytes(UART_PORT, header, 5);
    if (len > 0 && payload) {
        uart_write_bytes(UART_PORT, payload, len);
    }
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(50));
}

static void setTarget(uint8_t t) {
    sendCmd(SET_TARGET, &t, 1);
}

static void clear(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t p[3] = {r, g, b};
    sendCmd(CLEAR, p, 3);
}

static void present() {
    sendCmd(PRESENT, nullptr, 0);
}

static void oledClear() {
    sendCmd(OLED_CLEAR, nullptr, 0);
}

static void oledPresent() {
    sendCmd(OLED_PRESENT, nullptr, 0);
}

// Set a 16-bit variable on GPU
static void setVar(uint8_t id, int16_t val) {
    uint8_t p[3] = {id, (uint8_t)(val & 0xFF), (uint8_t)(val >> 8)};
    sendCmd(SET_VAR, p, 3);
}

// Draw polygon using variables (vertices in var slots)
static void drawPoly(uint8_t nVerts, uint8_t varStart, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t p[5] = {nVerts, varStart, r, g, b};
    sendCmd(DRAW_POLY, p, 5);
}

// HSV to RGB conversion
static void hsvToRgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b) {
    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
    float m = v - c;
    
    float rf, gf, bf;
    if (h < 60) { rf = c; gf = x; bf = 0; }
    else if (h < 120) { rf = x; gf = c; bf = 0; }
    else if (h < 180) { rf = 0; gf = c; bf = x; }
    else if (h < 240) { rf = 0; gf = x; bf = c; }
    else if (h < 300) { rf = x; gf = 0; bf = c; }
    else { rf = c; gf = 0; bf = x; }
    
    *r = (uint8_t)((rf + m) * 255);
    *g = (uint8_t)((gf + m) * 255);
    *b = (uint8_t)((bf + m) * 255);
}

// Simple scanline fill for polygon with horizontal gradient
static void fillPolygonGradient(int16_t* px, int16_t* py, int n, float baseHue) {
    // Find bounding box
    int16_t minX = px[0], maxX = px[0];
    int16_t minY = py[0], maxY = py[0];
    for (int i = 1; i < n; i++) {
        if (px[i] < minX) minX = px[i];
        if (px[i] > maxX) maxX = px[i];
        if (py[i] < minY) minY = py[i];
        if (py[i] > maxY) maxY = py[i];
    }
    
    float width = maxX - minX;
    if (width < 1) width = 1;
    
    // Scanline fill - for each Y, find intersections
    for (int16_t y = minY; y <= maxY; y++) {
        int16_t nodeX[32];
        int nodes = 0;
        
        int j = n - 1;
        for (int i = 0; i < n; i++) {
            if ((py[i] < y && py[j] >= y) || (py[j] < y && py[i] >= y)) {
                nodeX[nodes++] = px[i] + (y - py[i]) * (px[j] - px[i]) / (py[j] - py[i]);
            }
            j = i;
        }
        
        // Sort nodes
        for (int i = 0; i < nodes - 1; i++) {
            for (int k = i + 1; k < nodes; k++) {
                if (nodeX[i] > nodeX[k]) {
                    int16_t tmp = nodeX[i];
                    nodeX[i] = nodeX[k];
                    nodeX[k] = tmp;
                }
            }
        }
        
        // Fill between pairs with gradient
        for (int i = 0; i < nodes; i += 2) {
            if (i + 1 < nodes) {
                // Draw each pixel with gradient hue shift
                for (int16_t x = nodeX[i]; x <= nodeX[i+1]; x++) {
                    // Calculate hue based on X position (red->orange->yellow->green->blue)
                    // Red=0°, Orange=30°, Yellow=60°, Green=120°, Blue=240°
                    float t = (float)(x - minX) / width;
                    float hue = baseHue + t * 240.0f;  // 240 degree span covers red to blue
                    while (hue >= 360.0f) hue -= 360.0f;
                    
                    uint8_t r, g, b;
                    hsvToRgb(hue, 1.0f, 1.0f, &r, &g, &b);
                    
                    // Draw pixel
                    uint8_t p[7];
                    p[0] = x & 0xFF; p[1] = x >> 8;
                    p[2] = y & 0xFF; p[3] = y >> 8;
                    p[4] = r; p[5] = g; p[6] = b;
                    uint8_t hdr[5] = {SYNC0, SYNC1, 0x40, 7, 0};  // DRAW_PIXEL
                    uart_write_bytes(UART_PORT, hdr, 5);
                    uart_write_bytes(UART_PORT, p, 7);
                }
            }
        }
    }
}

static bool initUart() {
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PORT, &cfg);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, -1, -1);
    uart_driver_install(UART_PORT, 1024, 1024, 0, nullptr, 0);
    return true;
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "=== Polygon Demo ===");
    
    initUart();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Clear OLED once
    oledClear();
    oledPresent();
    
    // Prepare polygon vertices - FLIPPED horizontally
    // Left eye: flip within 0-63 range (center = 32)
    // Right eye: flip within 64-127 range (center = 96)
    int16_t leftX[NUM_VERTS], leftY[NUM_VERTS];
    int16_t rightX[NUM_VERTS], rightY[NUM_VERTS];
    
    for (int i = 0; i < NUM_VERTS; i++) {
        // Flip left eye: mirror around x=32
        leftX[i] = 64 - POLY_X[i];
        leftY[i] = POLY_Y[i];
        
        // Flip right eye: mirror around x=96 (64 + 32)
        rightX[i] = 128 - POLY_X[i];
        rightY[i] = POLY_Y[i];
    }
    
    // Static gradient: Red->Orange->Yellow->Green->Blue
    while (true) {
        // Clear and draw
        setTarget(0);
        clear(0, 0, 0);
        
        // Fill both polygons with static gradient (red to blue)
        fillPolygonGradient(leftX, leftY, NUM_VERTS, 0.0f);    // Start at red (0°)
        fillPolygonGradient(rightX, rightY, NUM_VERTS, 0.0f);  // Also start at red
        
        uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
        present();
        
        vTaskDelay(pdMS_TO_TICKS(100));  // Slower refresh since it's static
    }
}
