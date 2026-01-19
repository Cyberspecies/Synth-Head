/*****************************************************************
 * CPU_SimpleSpriteTest.cpp - Minimal Sprite Display Test
 *
 * This is a stripped-down test to verify sprite display works.
 * Uses the same pattern as WifiSpriteUploadTest but:
 * - No WiFi/HTTP - just hardcoded test sprite
 * - No Application layer conflicts
 * - Direct GpuDriver usage in main loop
 *
 * Tests:
 * 1. GPU init with GpuDriver
 * 2. Sprite upload
 * 3. Continuous render loop with rotation
 *****************************************************************/

#include "SystemAPI/GPU/GpuDriver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cmath>

static const char* TAG = "SIMPLE_SPRITE_TEST";

using namespace SystemAPI;

// ============== Global State ==============
static GpuDriver g_gpu;
static bool g_spriteReady = false;
static float g_spriteAngle = 0.0f;
static const uint8_t SPRITE_ID = 0;

// Test sprite: 16x16 "X" pattern in red/green
static const int SPRITE_WIDTH = 16;
static const int SPRITE_HEIGHT = 16;

static void createTestSprite(uint8_t* pixels) {
    // Create a simple X pattern
    memset(pixels, 0, SPRITE_WIDTH * SPRITE_HEIGHT * 3);
    
    for (int y = 0; y < SPRITE_HEIGHT; y++) {
        for (int x = 0; x < SPRITE_WIDTH; x++) {
            int idx = (y * SPRITE_WIDTH + x) * 3;
            
            // Draw X pattern
            bool onDiagonal1 = (x == y);
            bool onDiagonal2 = (x == SPRITE_HEIGHT - 1 - y);
            bool onBorder = (x == 0 || x == SPRITE_WIDTH-1 || y == 0 || y == SPRITE_HEIGHT-1);
            
            if (onDiagonal1 || onDiagonal2) {
                // Red X
                pixels[idx + 0] = 255;  // R
                pixels[idx + 1] = 50;   // G
                pixels[idx + 2] = 50;   // B
            } else if (onBorder) {
                // Green border
                pixels[idx + 0] = 50;   // R
                pixels[idx + 1] = 255;  // G
                pixels[idx + 2] = 50;   // B
            } else {
                // Dark blue background
                pixels[idx + 0] = 20;   // R
                pixels[idx + 1] = 20;   // G
                pixels[idx + 2] = 60;   // B
            }
        }
    }
}

// ============== Main Application ==============
extern "C" void app_main() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   Simple Sprite Test (No WiFi)             ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    // ========== Initialize GPU ==========
    ESP_LOGI(TAG, "Initializing GPU Driver...");

    GpuConfig gpuConfig;
    gpuConfig.uartPort = UART_NUM_1;
    gpuConfig.txPin = GPIO_NUM_12;
    gpuConfig.rxPin = GPIO_NUM_11;
    gpuConfig.baudRate = 10000000;
    gpuConfig.gpuBootDelayMs = 500;
    gpuConfig.weightedPixels = true;

    if (!g_gpu.init(gpuConfig)) {
        ESP_LOGE(TAG, "Failed to initialize GPU!");
        return;
    }

    g_gpu.startKeepAlive(1000);
    g_gpu.reset();
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(TAG, "GPU initialized!");

    // ========== Create and Upload Test Sprite ==========
    ESP_LOGI(TAG, "Creating test sprite %dx%d...", SPRITE_WIDTH, SPRITE_HEIGHT);
    
    uint8_t spritePixels[SPRITE_WIDTH * SPRITE_HEIGHT * 3];
    createTestSprite(spritePixels);
    
    // ASCII art debug
    ESP_LOGI(TAG, "=== TEST SPRITE ===");
    for (int y = 0; y < SPRITE_HEIGHT; y++) {
        char rowBuf[64] = {0};
        for (int x = 0; x < SPRITE_WIDTH; x++) {
            int idx = (y * SPRITE_WIDTH + x) * 3;
            int brightness = (spritePixels[idx] + spritePixels[idx+1] + spritePixels[idx+2]) / 3;
            rowBuf[x] = (brightness > 100) ? 'O' : '_';
        }
        rowBuf[SPRITE_WIDTH] = '\0';
        ESP_LOGI(TAG, "Row %02d: %s", y, rowBuf);
    }
    ESP_LOGI(TAG, "===================");

    // Upload sprite
    ESP_LOGI(TAG, "Uploading sprite to GPU...");
    bool uploadResult = g_gpu.uploadSprite(SPRITE_ID, SPRITE_WIDTH, SPRITE_HEIGHT,
                                            spritePixels, SpriteFormat::RGB888);
    
    if (uploadResult) {
        ESP_LOGI(TAG, "Sprite upload SUCCESS!");
        g_spriteReady = true;
    } else {
        ESP_LOGE(TAG, "Sprite upload FAILED!");
    }

    // ========== Main Render Loop ==========
    ESP_LOGI(TAG, "Starting render loop...");
    
    uint32_t frameCount = 0;
    int64_t lastLogTime = esp_timer_get_time();
    
    while (true) {
        // Set target to HUB75 display
        g_gpu.setTarget(GpuTarget::HUB75);
        
        if (g_spriteReady) {
            // Clear with dark background
            g_gpu.clear(5, 5, 15);
            
            // Draw sprite at center (64, 16) with rotation
            g_gpu.blitSpriteRotated(SPRITE_ID, 64.0f, 16.0f, g_spriteAngle);
            
            // Update rotation angle
            g_spriteAngle += 1.0f;
            if (g_spriteAngle >= 360.0f) g_spriteAngle -= 360.0f;
        } else {
            // Not ready - show error indicator
            g_gpu.clear(30, 5, 5);  // Red-ish background
            g_gpu.drawRect(10, 5, 108, 22, 255, 0, 0);  // Red border
        }
        
        // Present frame
        g_gpu.present();
        
        // Log status every 3 seconds
        frameCount++;
        int64_t now = esp_timer_get_time();
        if (now - lastLogTime >= 3000000) {  // 3 seconds
            float fps = frameCount / ((now - lastLogTime) / 1000000.0f);
            ESP_LOGI(TAG, "Frame %lu | FPS: %.1f | Angle: %.1f° | Ready: %s",
                     frameCount, fps, g_spriteAngle, g_spriteReady ? "YES" : "NO");
            lastLogTime = now;
            frameCount = 0;
        }
        
        // ~30 FPS - match GPU processing rate
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}
