/*****************************************************************
 * CPU_GpuDriverTest.cpp - Comprehensive GPU Driver Test Suite
 * 
 * Extensive tests covering:
 * 1. Basic commands (clear, pixel, present)
 * 2. Vector primitives (lines, rectangles, circles, polygons)
 * 3. Raster operations (sprites upload, blit)
 * 4. Anti-aliasing on/off comparison
 * 5. Animation tests (movement, sub-pixel precision)
 * 6. Complex final demo (10+ sprites, rotating, shaded vectors)
 *****************************************************************/

#include "SystemAPI/GPU/GpuDriver.h"
#include "esp_log.h"
#include <cstring>
#include <cmath>

static const char* TAG = "GPU_TEST";

using namespace SystemAPI;

// Helper to create gradient-shaded sprite
static void createGradientSprite(uint8_t* data, int size, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 3;
            // Radial gradient from center
            float cx = size / 2.0f;
            float cy = size / 2.0f;
            float dist = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
            float maxDist = sqrtf(cx * cx + cy * cy);
            float intensity = 1.0f - (dist / maxDist) * 0.7f;
            data[idx + 0] = (uint8_t)(r * intensity);
            data[idx + 1] = (uint8_t)(g * intensity);
            data[idx + 2] = (uint8_t)(b * intensity);
        }
    }
}

// Helper to create triangle sprite with shading
static void createTriangleSprite(uint8_t* data, int size, uint8_t r, uint8_t g, uint8_t b) {
    memset(data, 0, size * size * 3);
    float cx = size / 2.0f;
    float top = 1.0f;
    float botL = size - 2.0f;
    float botR = size - 2.0f;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float px = x + 0.5f;
            float py = y + 0.5f;
            
            // Triangle vertices: top center, bottom left, bottom right
            float e01 = (size - 2 - cx) * (py - top) - (botL - top) * (px - cx);
            float e12 = (1 - (size - 2)) * (py - botL) - 0 * (px - (size - 2));
            float e20 = (cx - 1) * (py - botL) - (top - botL) * (px - 1);
            
            bool inside = (e01 >= 0 && e12 >= 0 && e20 >= 0) || (e01 <= 0 && e12 <= 0 && e20 <= 0);
            
            if (inside) {
                int idx = (y * size + x) * 3;
                // Vertical gradient shading
                float shade = 0.4f + 0.6f * (1.0f - (float)y / size);
                data[idx + 0] = (uint8_t)(r * shade);
                data[idx + 1] = (uint8_t)(g * shade);
                data[idx + 2] = (uint8_t)(b * shade);
            }
        }
    }
}

// Helper to create diamond sprite
static void createDiamondSprite(uint8_t* data, int size, uint8_t r, uint8_t g, uint8_t b) {
    memset(data, 0, size * size * 3);
    float cx = size / 2.0f;
    float cy = size / 2.0f;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = fabsf(x - cx);
            float dy = fabsf(y - cy);
            if (dx + dy < cx - 1) {
                int idx = (y * size + x) * 3;
                float dist = (dx + dy) / cx;
                float shade = 1.0f - dist * 0.5f;
                data[idx + 0] = (uint8_t)(r * shade);
                data[idx + 1] = (uint8_t)(g * shade);
                data[idx + 2] = (uint8_t)(b * shade);
            }
        }
    }
}

// Helper to create star sprite
static void createStarSprite(uint8_t* data, int size, uint8_t r, uint8_t g, uint8_t b) {
    memset(data, 0, size * size * 3);
    float cx = size / 2.0f;
    float cy = size / 2.0f;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float angle = atan2f(dy, dx);
            float dist = sqrtf(dx * dx + dy * dy);
            // 5-pointed star shape
            float starRadius = (cx - 2) * (0.5f + 0.5f * fabsf(sinf(angle * 2.5f)));
            if (dist < starRadius) {
                int idx = (y * size + x) * 3;
                float shade = 1.0f - (dist / starRadius) * 0.4f;
                data[idx + 0] = (uint8_t)(r * shade);
                data[idx + 1] = (uint8_t)(g * shade);
                data[idx + 2] = (uint8_t)(b * shade);
            }
        }
    }
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   COMPREHENSIVE GPU DRIVER TEST SUITE      ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    GpuDriver gpu;
    
    GpuConfig config;
    config.uartPort = UART_NUM_1;
    config.txPin = GPIO_NUM_12;
    config.rxPin = GPIO_NUM_11;
    config.baudRate = 10000000;
    config.gpuBootDelayMs = 500;
    config.weightedPixels = true;
    
    ESP_LOGI(TAG, "Initializing GPU Driver...");
    if (!gpu.init(config)) {
        ESP_LOGE(TAG, "FAILED to initialize GPU driver!");
        return;
    }
    ESP_LOGI(TAG, "GPU Driver initialized successfully!");
    
    gpu.startKeepAlive(1000);
    
    // Reset GPU state
    ESP_LOGI(TAG, "Resetting GPU...");
    gpu.reset();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    //================================================================
    // TEST 1: BASIC COMMANDS
    //================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "TEST 1: BASIC COMMANDS");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    
    // 1.1 Clear to solid colors
    ESP_LOGI(TAG, "1.1 Clear to RED");
    gpu.setTarget(GpuTarget::HUB75);
    gpu.clear(Color::Red());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(800));
    
    ESP_LOGI(TAG, "1.2 Clear to GREEN");
    gpu.clear(Color::Green());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(800));
    
    ESP_LOGI(TAG, "1.3 Clear to BLUE");
    gpu.clear(Color::Blue());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(800));
    
    // 1.2 Individual pixels
    ESP_LOGI(TAG, "1.4 Individual pixels - gradient pattern");
    gpu.clear(Color::Black());
    for (int x = 0; x < 128; x += 4) {
        for (int y = 0; y < 32; y += 4) {
            uint8_t r = (x * 2) & 0xFF;
            uint8_t g = (y * 8) & 0xFF;
            uint8_t b = ((x + y) * 2) & 0xFF;
            gpu.drawPixel(x, y, r, g, b);
        }
    }
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    //================================================================
    // TEST 2: VECTOR PRIMITIVES
    //================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "TEST 2: VECTOR PRIMITIVES");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    
    // 2.1 Lines
    ESP_LOGI(TAG, "2.1 Lines - various angles");
    gpu.clear(0, 0, 15);
    gpu.drawLine(0, 0, 127, 31, Color::White());      // Diagonal
    gpu.drawLine(0, 31, 127, 0, Color::Yellow());     // Opposite diagonal
    gpu.drawLine(0, 16, 127, 16, Color::Red());       // Horizontal
    gpu.drawLine(64, 0, 64, 31, Color::Green());      // Vertical
    gpu.drawLine(10, 5, 50, 25, Color::Cyan());       // Arbitrary
    gpu.drawLine(80, 28, 120, 3, Color::Magenta());   // Another arbitrary
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 2.2 Rectangles (outlines)
    ESP_LOGI(TAG, "2.2 Rectangle outlines");
    gpu.clear(5, 5, 15);
    gpu.drawRect(5, 3, 30, 20, Color::Red());
    gpu.drawRect(40, 5, 25, 18, Color::Green());
    gpu.drawRect(70, 2, 20, 25, Color::Blue());
    gpu.drawRect(95, 8, 28, 15, Color::Yellow());
    // Nested rectangles
    gpu.drawRect(15, 8, 10, 10, Color::White());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 2.3 Filled rectangles
    ESP_LOGI(TAG, "2.3 Filled rectangles");
    gpu.clear(Color::Black());
    gpu.drawFilledRect(5, 3, 20, 12, Color::Red());
    gpu.drawFilledRect(30, 8, 20, 12, Color::Green());
    gpu.drawFilledRect(55, 5, 20, 15, Color::Blue());
    gpu.drawFilledRect(80, 2, 18, 10, Color::Yellow());
    gpu.drawFilledRect(102, 12, 22, 16, Color::Cyan());
    // Overlapping
    gpu.drawFilledRect(15, 18, 30, 10, Color::Magenta());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 2.4 Circles
    ESP_LOGI(TAG, "2.4 Circles - various sizes");
    gpu.clear(10, 5, 20);
    gpu.drawCircle(20, 16, 12, Color::Red());
    gpu.drawCircle(50, 16, 10, Color::Green());
    gpu.drawCircle(80, 16, 8, Color::Blue());
    gpu.drawCircle(105, 16, 6, Color::Yellow());
    gpu.drawCircle(64, 16, 14, Color::White());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 2.5 Polygons (filled triangles)
    ESP_LOGI(TAG, "2.5 Filled polygons");
    gpu.clear(5, 10, 15);
    int16_t tx1[] = {20, 5, 35};
    int16_t ty1[] = {5, 28, 28};
    gpu.drawFilledPolygon(tx1, ty1, 3, Color::Red());
    
    int16_t tx2[] = {60, 45, 75};
    int16_t ty2[] = {3, 20, 20};
    gpu.drawFilledPolygon(tx2, ty2, 3, Color::Green());
    
    int16_t tx3[] = {100, 85, 115};
    int16_t ty3[] = {28, 8, 8};
    gpu.drawFilledPolygon(tx3, ty3, 3, Color::Blue());
    
    // Quadrilateral
    int16_t qx[] = {50, 40, 50, 60};
    int16_t qy[] = {22, 28, 30, 28};
    gpu.drawFilledPolygon(qx, qy, 4, Color::Yellow());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    //================================================================
    // TEST 3: RASTER OPERATIONS (SPRITES)
    //================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "TEST 3: RASTER OPERATIONS (SPRITES)");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    
    // Create various sprite shapes
    const int SP_SIZE = 12;
    uint8_t spriteData[SP_SIZE * SP_SIZE * 3];
    
    // Sprite 0: Red gradient circle
    ESP_LOGI(TAG, "3.1 Creating sprites...");
    createGradientSprite(spriteData, SP_SIZE, 255, 50, 50);
    gpu.uploadSprite(0, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 1: Green gradient circle
    createGradientSprite(spriteData, SP_SIZE, 50, 255, 50);
    gpu.uploadSprite(1, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 2: Blue gradient circle
    createGradientSprite(spriteData, SP_SIZE, 50, 100, 255);
    gpu.uploadSprite(2, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 3: Cyan triangle
    createTriangleSprite(spriteData, SP_SIZE, 0, 255, 255);
    gpu.uploadSprite(3, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 4: Yellow diamond
    createDiamondSprite(spriteData, SP_SIZE, 255, 255, 0);
    gpu.uploadSprite(4, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 5: Magenta star
    createStarSprite(spriteData, SP_SIZE, 255, 0, 255);
    gpu.uploadSprite(5, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 6: Orange gradient
    createGradientSprite(spriteData, SP_SIZE, 255, 128, 0);
    gpu.uploadSprite(6, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 7: Pink triangle
    createTriangleSprite(spriteData, SP_SIZE, 255, 100, 150);
    gpu.uploadSprite(7, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 8: Lime diamond
    createDiamondSprite(spriteData, SP_SIZE, 180, 255, 0);
    gpu.uploadSprite(8, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    // Sprite 9: White star
    createStarSprite(spriteData, SP_SIZE, 255, 255, 255);
    gpu.uploadSprite(9, SP_SIZE, SP_SIZE, spriteData, SpriteFormat::RGB888);
    
    ESP_LOGI(TAG, "3.2 Uploaded 10 sprites (12x12 each)");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Display all sprites
    ESP_LOGI(TAG, "3.3 Displaying all sprites");
    gpu.clear(15, 15, 25);
    for (int i = 0; i < 10; i++) {
        int x = (i % 5) * 24 + 8;
        int y = (i / 5) * 14 + 4;
        gpu.blitSprite(i, x, y);
    }
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    //================================================================
    // TEST 4: ANTI-ALIASING COMPARISON
    //================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "TEST 4: ANTI-ALIASING COMPARISON");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    
    // 4.1 Lines with AA OFF
    ESP_LOGI(TAG, "4.1 Lines - AA OFF (aliased/jagged)");
    gpu.setWeightedPixels(false);
    gpu.clear(0, 0, 20);
    gpu.drawLineF(5.0f, 5.0f, 60.0f, 28.0f, Color::White());
    gpu.drawLineF(70.0f, 3.0f, 120.0f, 25.0f, Color::Yellow());
    gpu.drawCircleF(32.0f, 16.0f, 10.0f, Color::Red());
    gpu.drawCircleF(96.0f, 16.0f, 8.0f, Color::Cyan());
    // Text indicator
    gpu.drawFilledRect(55, 0, 18, 6, Color::Red());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    // 4.2 Lines with AA ON
    ESP_LOGI(TAG, "4.2 Lines - AA ON (smooth edges)");
    gpu.setWeightedPixels(true);
    gpu.clear(0, 0, 20);
    gpu.drawLineF(5.0f, 5.0f, 60.0f, 28.0f, Color::White());
    gpu.drawLineF(70.0f, 3.0f, 120.0f, 25.0f, Color::Yellow());
    gpu.drawCircleF(32.0f, 16.0f, 10.0f, Color::Red());
    gpu.drawCircleF(96.0f, 16.0f, 8.0f, Color::Cyan());
    // Text indicator
    gpu.drawFilledRect(55, 0, 18, 6, Color::Green());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    // 4.3 Side by side comparison
    ESP_LOGI(TAG, "4.3 Side-by-side: Left=AA OFF, Right=AA ON");
    gpu.clear(5, 5, 15);
    // Left side - AA OFF
    gpu.setWeightedPixels(false);
    gpu.drawLineF(5.0f, 5.0f, 55.0f, 28.0f, Color::White());
    gpu.drawCircleF(30.0f, 16.0f, 8.0f, Color::Yellow());
    // Right side - AA ON
    gpu.setWeightedPixels(true);
    gpu.drawLineF(70.0f, 5.0f, 120.0f, 28.0f, Color::White());
    gpu.drawCircleF(95.0f, 16.0f, 8.0f, Color::Yellow());
    // Divider
    gpu.drawLine(63, 0, 63, 31, Color::Red());
    gpu.present();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    //================================================================
    // TEST 5: ANIMATION TESTS
    //================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "TEST 5: ANIMATION TESTS");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    
    // 5.1 Sub-pixel sprite movement comparison
    ESP_LOGI(TAG, "5.1 Sprite movement - INTEGER vs SUB-PIXEL+ROTATION");
    ESP_LOGI(TAG, "     Left: Integer (choppy), Right: Sub-pixel + rotating (smooth)");
    gpu.setWeightedPixels(true);
    float sx = 10.0f, sy = 10.0f;
    float sx2 = 70.0f, sy2 = 10.0f;
    float svx = 0.15f, svy = 0.1f;  // Very slow for clear sub-pixel demo
    float sprRot = 0.0f;
    for (int frame = 0; frame < 300; frame++) {
        sx += svx; sy += svy;
        sx2 += svx; sy2 += svy;
        sprRot += 1.0f;  // Rotate 1 degree per frame
        if (sprRot >= 360.0f) sprRot -= 360.0f;
        
        if (sx < 6 || sx > 50) svx = -svx;
        if (sy < 6 || sy > 16) svy = -svy;
        
        gpu.clear(8, 8, 20);
        
        // Left: Integer position (jerky snap to pixels)
        gpu.blitSprite(3, (int16_t)sx, (int16_t)sy);
        
        // Right: Sub-pixel + rotation with AA (silky smooth)
        gpu.blitSpriteRotated(4, sx2, sy2, sprRot);
        
        gpu.present();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    
    // 5.2 Moving lines (AA animation)
    ESP_LOGI(TAG, "5.2 Rotating lines with AA");
    gpu.setWeightedPixels(true);
    float angle = 0.0f;
    for (int frame = 0; frame < 180; frame++) {
        angle += 3.0f;
        float rad = angle * 3.14159f / 180.0f;
        float cx = 64.0f, cy = 16.0f;
        float len = 14.0f;
        
        gpu.clear(5, 5, 20);
        // Multiple rotating lines
        for (int i = 0; i < 6; i++) {
            float a = rad + i * 3.14159f / 3.0f;
            float x1 = cx + cosf(a) * len;
            float y1 = cy + sinf(a) * len;
            float x2 = cx - cosf(a) * len;
            float y2 = cy - sinf(a) * len;
            uint8_t r = (i * 40 + 50) & 0xFF;
            uint8_t g = (i * 30 + 100) & 0xFF;
            uint8_t b = (i * 50 + 80) & 0xFF;
            gpu.drawLineF(x1, y1, x2, y2, r, g, b);
        }
        gpu.present();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    
    // 5.3 Bouncing circles
    ESP_LOGI(TAG, "5.3 Bouncing circles with AA");
    float cx[3] = {30.0f, 64.0f, 100.0f};
    float cy[3] = {10.0f, 20.0f, 15.0f};
    float cvx[3] = {0.8f, -0.6f, 1.0f};
    float cvy[3] = {0.5f, 0.7f, -0.4f};
    float cr[3] = {6.0f, 8.0f, 5.0f};
    
    for (int frame = 0; frame < 180; frame++) {
        gpu.clear(5, 10, 20);
        for (int i = 0; i < 3; i++) {
            cx[i] += cvx[i]; cy[i] += cvy[i];
            if (cx[i] < cr[i] || cx[i] > 128 - cr[i]) cvx[i] = -cvx[i];
            if (cy[i] < cr[i] || cy[i] > 32 - cr[i]) cvy[i] = -cvy[i];
            
            uint8_t colors[3][3] = {{255, 100, 100}, {100, 255, 100}, {100, 100, 255}};
            gpu.drawCircleF(cx[i], cy[i], cr[i], colors[i][0], colors[i][1], colors[i][2]);
        }
        gpu.present();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    
    //================================================================
    // TEST 6: COMPLEX FINAL DEMO - SUB-PIXEL + ROTATION
    //================================================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "TEST 6: COMPLEX FINAL DEMO");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "10 sprites with SUB-PIXEL movement + ROTATION");
    ESP_LOGI(TAG, "AA toggles every 5s to show smooth vs aliased");
    
    // Sprite positions, velocities, angles
    float spX[10], spY[10];
    float spVx[10], spVy[10];
    float spAngle[10], spRotSpeed[10];
    
    for (int i = 0; i < 10; i++) {
        // Initial positions spread across screen
        spX[i] = 10.0f + (i % 5) * 22.0f;
        spY[i] = 6.0f + (i / 5) * 12.0f;
        
        // SUB-PIXEL velocities (0.1 - 0.5 pixels/frame for visible smooth motion)
        spVx[i] = 0.15f + (i * 0.04f);
        spVy[i] = 0.12f + ((9 - i) * 0.03f);
        if (i % 2) spVx[i] = -spVx[i];
        if (i % 3 == 0) spVy[i] = -spVy[i];
        
        // Rotation angles and speeds
        spAngle[i] = i * 36.0f;  // Start at different angles
        spRotSpeed[i] = 0.5f + (i * 0.3f);  // Varying rotation speeds
        if (i % 2) spRotSpeed[i] = -spRotSpeed[i];  // Some rotate opposite
    }
    
    // Vector shapes state
    float vAngle = 0.0f;
    float lineX1 = 20.0f, lineY1 = 5.0f;
    float lineVx = 0.25f, lineVy = 0.15f;
    
    int frameCount = 0;
    bool aaState = true;
    gpu.setWeightedPixels(true);  // Start with AA on
    
    while (true) {
        // Toggle AA every 300 frames (~5 seconds)
        if (frameCount > 0 && frameCount % 300 == 0) {
            aaState = !aaState;
            gpu.setWeightedPixels(aaState);
            ESP_LOGI(TAG, "AA: %s | Frame %d | Watch sprite edges!", aaState ? "ON" : "OFF", frameCount);
        }
        
        // Update sprite positions with SUB-PIXEL precision
        for (int i = 0; i < 10; i++) {
            // Update position (sub-pixel movement)
            spX[i] += spVx[i];
            spY[i] += spVy[i];
            
            // Update rotation angle
            spAngle[i] += spRotSpeed[i];
            if (spAngle[i] >= 360.0f) spAngle[i] -= 360.0f;
            if (spAngle[i] < 0.0f) spAngle[i] += 360.0f;
            
            // Bounce off walls (account for sprite size ~12px)
            if (spX[i] < 2 || spX[i] > 114) {
                spVx[i] = -spVx[i];
                spX[i] = (spX[i] < 2) ? 2.0f : 114.0f;
            }
            if (spY[i] < 2 || spY[i] > 18) {
                spVy[i] = -spVy[i];
                spY[i] = (spY[i] < 2) ? 2.0f : 18.0f;
            }
        }
        
        // Update vector shapes
        vAngle += 1.2f;
        if (vAngle >= 360.0f) vAngle -= 360.0f;
        
        lineX1 += lineVx;
        lineY1 += lineVy;
        if (lineX1 < 5 || lineX1 > 60) lineVx = -lineVx;
        if (lineY1 < 3 || lineY1 > 25) lineVy = -lineVy;
        
        // Draw frame
        gpu.setTarget(GpuTarget::HUB75);
        
        // Dark background to show AA edges clearly
        gpu.clear(8, 8, 16);
        
        // Draw rotating vector lines (shaded by angle)
        float rad = vAngle * 3.14159f / 180.0f;
        for (int i = 0; i < 4; i++) {
            float a = rad + i * 3.14159f / 2.0f;
            float ccx = 64.0f, ccy = 16.0f;
            float len = 11.0f;
            float x1 = ccx + cosf(a) * len;
            float y1 = ccy + sinf(a) * len;
            float x2 = ccx - cosf(a) * len * 0.5f;
            float y2 = ccy - sinf(a) * len * 0.5f;
            uint8_t shade = (uint8_t)(128 + 127 * sinf(a));
            gpu.drawLineF(x1, y1, x2, y2, shade, shade / 2, 255 - shade / 2);
        }
        
        // Diagonal line with sub-pixel movement
        gpu.drawLineF(lineX1, lineY1, lineX1 + 35.0f, lineY1 + 12.0f, 200, 200, 100);
        
        // Draw rotating circles in orbit
        for (int i = 0; i < 3; i++) {
            float orbA = rad * (1.0f + i * 0.3f) + i * 2.0f;
            float orbX = 100.0f + cosf(orbA) * 14.0f;
            float orbY = 16.0f + sinf(orbA) * 7.0f;
            float orbR = 2.5f + i;
            uint8_t orbShade = (uint8_t)(150 + 100 * sinf(orbA));
            gpu.drawCircleF(orbX, orbY, orbR, orbShade, 255 - orbShade / 2, orbShade / 2);
        }
        
        // Draw all 10 sprites with SUB-PIXEL position AND ROTATION
        for (int i = 0; i < 10; i++) {
            // Use rotated blit for smooth sub-pixel + rotation
            gpu.blitSpriteRotated(i, spX[i], spY[i], spAngle[i]);
        }
        
        // AA state indicator (top-left corner)
        if (aaState) {
            gpu.drawFilledRect(0, 0, 5, 5, Color::Green());
        } else {
            gpu.drawFilledRect(0, 0, 5, 5, Color::Red());
        }
        
        // Frame counter indicator (pulsing pixel)
        uint8_t pulse = (uint8_t)(128 + 127 * sinf(frameCount * 0.1f));
        gpu.drawPixel(124, 2, pulse, pulse, pulse);
        
        gpu.present();
        
        frameCount++;
        if (frameCount % 600 == 0) {
            ESP_LOGI(TAG, "Frame %d | Sprites rotating & moving sub-pixel", frameCount);
        }
        
        vTaskDelay(pdMS_TO_TICKS(16));  // ~60 FPS
    }
}
