/**
 * @brief OLED Orientation Test
 * 
 * Automatically cycles through orientation modes every 3 seconds.
 * Test order:
 * 1. Base (mode 0) - normal orientation (with physical 180° compensation)
 * 2. 90° CW (mode 5)
 * 3. 180° (mode 1)
 * 4. 90° CCW (mode 6)
 * 5. Mirror X (mode 2) from base
 * 6. Mirror Y (mode 3) from base
 * 7. Mirror X+Y combo (mode 4)
 * 8. Rotate 90° + Mirror X (mode 7)
 * 
 * The display is physically mounted 180° upside down, so the GPU applies
 * a base 180° rotation to compensate. CPU orientation modes add additional
 * transforms on top of that base.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"

#include "GpuDriver/GpuCommands.hpp"

static const char* TAG = "OLED_ORIENT";

static GpuCommands gpu;

// Test sequence - tests in logical order
struct OrientationTest {
    int mode;
    const char* name;
    const char* description;
};

static const OrientationTest testSequence[] = {
    {0, "BASE",      "Normal (0 deg)"},
    {5, "ROT 90 CW", "Rotate 90 CW"},
    {1, "ROT 180",   "Rotate 180"},
    {6, "ROT 90CCW", "Rotate 90 CCW"},
    {2, "MIRROR X",  "Mirror Horiz"},
    {3, "MIRROR Y",  "Mirror Vert"},
    {4, "MIR X+Y",   "Mirror X+Y"},
    {7, "90+MIR X",  "Rot90 + MirX"},
};
static const int NUM_TESTS = sizeof(testSequence) / sizeof(testSequence[0]);

void showOrientationTest(int testIndex) {
    const OrientationTest& test = testSequence[testIndex];
    
    gpu.oledClear();
    
    // Draw test number and mode info at top
    char line1[32];
    snprintf(line1, sizeof(line1), "%d/%d: %s", testIndex + 1, NUM_TESTS, test.name);
    gpu.oledText(5, 2, line1);
    gpu.oledText(5, 14, test.description);
    
    // Draw an arrow pointing UP (should point UP when orientation is correct)
    int cx = 64;  // Center X
    int cy = 72;  // Center Y of arrow
    
    // Arrow body (vertical line)
    gpu.oledLine(cx, cy - 20, cx, cy + 20, true);
    
    // Arrow head (pointing up) - thick
    gpu.oledLine(cx, cy - 20, cx - 12, cy - 5, true);
    gpu.oledLine(cx, cy - 20, cx + 12, cy - 5, true);
    gpu.oledLine(cx - 1, cy - 20, cx - 12, cy - 6, true);
    gpu.oledLine(cx + 1, cy - 20, cx + 12, cy - 6, true);
    
    // Draw "UP" text above arrow
    gpu.oledText(cx - 8, cy - 35, "UP");
    
    // Draw corner markers (these help identify orientation)
    gpu.oledText(2, 2, "TL");       // Top-left
    gpu.oledText(105, 2, "TR");     // Top-right
    gpu.oledText(2, 118, "BL");     // Bottom-left
    gpu.oledText(105, 118, "BR");   // Bottom-right
    
    // Draw border
    gpu.oledRect(0, 0, 127, 127, true);
    
    // Draw a small reference square in top-right corner
    gpu.oledFill(110, 25, 15, 15, true);
    
    gpu.oledPresent();
}

extern "C" void app_main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              OLED ORIENTATION TEST                           ║\n");
    printf("║  Auto-cycles through modes every 3 seconds                   ║\n");
    printf("║  Arrow should point UP, corners should match labels          ║\n");
    printf("║  Press ENTER to skip to next mode                            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    // Initialize UART for user input
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    
    // Initialize GPU
    ESP_LOGI(TAG, "Initializing GPU connection...");
    gpu.init();
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for GPU to be ready
    
    int currentTestIndex = 0;
    
    // Display initial test
    ESP_LOGI(TAG, "Test %d/%d: Mode %d (%s) - %s", 
             currentTestIndex + 1, NUM_TESTS,
             testSequence[currentTestIndex].mode,
             testSequence[currentTestIndex].name,
             testSequence[currentTestIndex].description);
    
    gpu.oledSetOrientation(testSequence[currentTestIndex].mode);
    vTaskDelay(pdMS_TO_TICKS(100));
    showOrientationTest(currentTestIndex);
    
    uint8_t rxBuf[16];
    int64_t lastChangeTime = esp_timer_get_time();
    const int64_t CHANGE_INTERVAL_US = 3000000;  // 3 seconds
    
    while (true) {
        // Check for user input to skip
        int len = uart_read_bytes(UART_NUM_0, rxBuf, sizeof(rxBuf), pdMS_TO_TICKS(100));
        
        int64_t now = esp_timer_get_time();
        bool shouldAdvance = (len > 0) || ((now - lastChangeTime) >= CHANGE_INTERVAL_US);
        
        if (shouldAdvance) {
            currentTestIndex = (currentTestIndex + 1) % NUM_TESTS;
            lastChangeTime = now;
            
            ESP_LOGI(TAG, "Test %d/%d: Mode %d (%s) - %s", 
                     currentTestIndex + 1, NUM_TESTS,
                     testSequence[currentTestIndex].mode,
                     testSequence[currentTestIndex].name,
                     testSequence[currentTestIndex].description);
            
            gpu.oledSetOrientation(testSequence[currentTestIndex].mode);
            vTaskDelay(pdMS_TO_TICKS(50));
            showOrientationTest(currentTestIndex);
        }
    }
}
