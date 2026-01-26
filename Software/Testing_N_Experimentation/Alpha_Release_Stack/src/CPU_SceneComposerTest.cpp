/**
 * @file CPU_SceneComposerTest.cpp
 * @brief Test application for the Scene Composition System
 * 
 * Demonstrates:
 * - Creating separate scenes for OLED and HUB75
 * - Layer-based composition with z-ordering
 * - Static and animated layers
 * - Dynamic scene loading/unloading
 * - Custom draw callbacks
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"

#include "GpuDriver/GpuCommands.hpp"
#include "FrameworkAPI/SceneComposer.hpp"

static const char* TAG = "SCENE_TEST";

using namespace SceneAPI;

// Global instances
static GpuCommands gpu;
static SceneComposer composer;

// ============================================================
// Custom Animation Callbacks
// ============================================================

// Bouncing ball animation - updates position based on progress
bool bouncingBallUpdate(Layer* layer, uint32_t deltaMs, void* userData) {
    // Use animProgress to calculate bounce position
    float t = layer->animProgress;
    
    // Parabolic bounce (y = 4h * t * (1 - t)) where h is max height
    float bounceHeight = 40.0f;
    float y = bounceHeight * 4.0f * t * (1.0f - t);
    
    // Store base Y in height field, calculate actual Y
    layer->y = 100 - (int16_t)y;  // OLED bottom area
    
    // Horizontal movement
    layer->x = 10 + (int16_t)(t * 108.0f);  // Move across screen
    
    return true;
}

// Pulsing circle animation - changes radius
bool pulsingCircleUpdate(Layer* layer, uint32_t deltaMs, void* userData) {
    float t = layer->animProgress;
    
    // Sine wave for smooth pulsing
    float pulse = sinf(t * 3.14159f * 2.0f);
    layer->radius = 15 + (int16_t)(pulse * 8.0f);
    
    return true;
}

// Spinning line animation for HUB75
bool spinningLineUpdate(Layer* layer, uint32_t deltaMs, void* userData) {
    float t = layer->animProgress;
    float angle = t * 3.14159f * 2.0f;
    
    // Center of rotation
    int16_t cx = 64;
    int16_t cy = 16;
    int16_t length = 12;
    
    // Calculate line endpoints
    layer->x = cx + (int16_t)(cosf(angle) * length);
    layer->y = cy + (int16_t)(sinf(angle) * length);
    layer->x2 = cx - (int16_t)(cosf(angle) * length);
    layer->y2 = cy - (int16_t)(sinf(angle) * length);
    
    return true;
}

// Color cycling for HUB75 rectangle
bool colorCycleUpdate(Layer* layer, uint32_t deltaMs, void* userData) {
    float t = layer->animProgress;
    
    // HSV to RGB (simplified, hue only)
    float h = t * 6.0f;
    int i = (int)h;
    float f = h - i;
    
    uint8_t q = (uint8_t)(255 * (1 - f));
    uint8_t t_val = (uint8_t)(255 * f);
    
    switch (i % 6) {
        case 0: layer->color = Color(255, t_val, 0); break;
        case 1: layer->color = Color(q, 255, 0); break;
        case 2: layer->color = Color(0, 255, t_val); break;
        case 3: layer->color = Color(0, q, 255); break;
        case 4: layer->color = Color(t_val, 0, 255); break;
        case 5: layer->color = Color(255, 0, q); break;
    }
    
    return true;
}

// ============================================================
// Custom Draw Callback
// ============================================================

// Draws a crosshair pattern
void drawCrosshair(GpuCommands* gpu, Layer* layer, DisplayTarget target, void* userData) {
    int16_t cx = layer->x;
    int16_t cy = layer->y;
    int16_t size = layer->width > 0 ? layer->width : 10;
    
    if (target == DisplayTarget::OLED) {
        gpu->oledLine(cx - size, cy, cx + size, cy, true);
        gpu->oledLine(cx, cy - size, cx, cy + size, true);
        gpu->oledCircle(cx, cy, size / 2, true);
    } else {
        gpu->hub75Line(cx - size, cy, cx + size, cy, 
                      layer->color.r, layer->color.g, layer->color.b);
        gpu->hub75Line(cx, cy - size, cx, cy + size,
                      layer->color.r, layer->color.g, layer->color.b);
        gpu->hub75Circle(cx, cy, size / 2,
                        layer->color.r, layer->color.g, layer->color.b);
    }
}

// ============================================================
// Scene Builders
// ============================================================

// Build OLED demo scene with multiple layers
Scene* buildOledDemoScene() {
    Scene* scene = composer.createScene(DisplayTarget::OLED, "OledDemo");
    if (!scene) {
        ESP_LOGE(TAG, "Failed to create OLED demo scene");
        return nullptr;
    }
    
    ESP_LOGI(TAG, "Building OLED demo scene...");
    
    // Background layer (lowest z-order)
    Layer* border = composer.addRectangle(scene, 0, 0, 127, 127, Color::White(), false, -10);
    if (border) ESP_LOGI(TAG, "  Added border layer");
    
    // Static title text
    Layer* title = composer.addText(scene, 5, 5, "Scene Demo", Color::White(), 10);
    if (title) ESP_LOGI(TAG, "  Added title layer");
    
    // Animated bouncing ball
    Layer* ball = composer.addCircle(scene, 64, 80, 8, Color::White(), true, 5);
    if (ball) {
        composer.setAnimation(ball, AnimationType::LOOP, 0.5f, bouncingBallUpdate);
        ESP_LOGI(TAG, "  Added bouncing ball layer");
    }
    
    // Pulsing circle
    Layer* pulse = composer.addCircle(scene, 100, 30, 15, Color::White(), false, 3);
    if (pulse) {
        composer.setAnimation(pulse, AnimationType::LOOP, 2.0f, pulsingCircleUpdate);
        ESP_LOGI(TAG, "  Added pulsing circle layer");
    }
    
    // Static decorative lines
    composer.addLine(scene, 0, 20, 127, 20, Color::White(), 0);
    composer.addLine(scene, 0, 110, 127, 110, Color::White(), 0);
    
    // Custom crosshair
    Layer* crosshair = composer.addCustom(scene, drawCrosshair, nullptr, 2);
    if (crosshair) {
        crosshair->x = 30;
        crosshair->y = 65;
        crosshair->width = 12;
        ESP_LOGI(TAG, "  Added crosshair layer");
    }
    
    ESP_LOGI(TAG, "OLED scene built with %d layers", scene->layerCount);
    return scene;
}

// Build HUB75 demo scene
Scene* buildHub75DemoScene() {
    Scene* scene = composer.createScene(DisplayTarget::HUB75, "Hub75Demo");
    if (!scene) {
        ESP_LOGE(TAG, "Failed to create HUB75 demo scene");
        return nullptr;
    }
    
    scene->backgroundColor = Color(0, 0, 20);  // Dark blue background
    
    ESP_LOGI(TAG, "Building HUB75 demo scene...");
    
    // Color cycling rectangle
    Layer* colorRect = composer.addRectangle(scene, 5, 5, 20, 22, Color::Red(), true, 5);
    if (colorRect) {
        composer.setAnimation(colorRect, AnimationType::LOOP, 0.3f, colorCycleUpdate);
        ESP_LOGI(TAG, "  Added color cycling rectangle");
    }
    
    // Spinning line
    Layer* spinner = composer.addLine(scene, 64, 16, 64, 16, Color::White(), 10);
    if (spinner) {
        composer.setAnimation(spinner, AnimationType::LOOP, 1.0f, spinningLineUpdate);
        ESP_LOGI(TAG, "  Added spinning line");
    }
    
    // Static elements
    composer.addText(scene, 30, 2, "HUB75", Color::Green(), 0);
    composer.addCircle(scene, 110, 16, 10, Color::Cyan(), false, 0);
    
    // Decorative dots
    for (int i = 0; i < 5; i++) {
        composer.addPixel(scene, 90 + i * 6, 28, Color::Yellow(), 0);
    }
    
    ESP_LOGI(TAG, "HUB75 scene built with %d layers", scene->layerCount);
    return scene;
}

// Build simple static OLED scene
Scene* buildOledStaticScene() {
    Scene* scene = composer.createScene(DisplayTarget::OLED, "OledStatic");
    if (!scene) return nullptr;
    
    ESP_LOGI(TAG, "Building OLED static scene...");
    
    composer.addText(scene, 20, 50, "STATIC", Color::White(), 0);
    composer.addText(scene, 25, 65, "SCENE", Color::White(), 0);
    composer.addRectangle(scene, 10, 40, 107, 50, Color::White(), false, -1);
    
    ESP_LOGI(TAG, "Static scene built");
    return scene;
}

// ============================================================
// Main Application
// ============================================================

extern "C" void app_main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           SCENE COMPOSER TEST                                ║\n");
    printf("║  Demonstrates layer-based scene composition                  ║\n");
    printf("║  Press 1/2/3 to switch OLED scenes                           ║\n");
    printf("║  Press 'p' to pause/resume animations                        ║\n");
    printf("║  Press 'd' to delete current OLED scene                      ║\n");
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
        .flags = {0},
    };
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    
    // Initialize GPU
    ESP_LOGI(TAG, "Initializing GPU connection...");
    if (!gpu.init()) {
        ESP_LOGE(TAG, "FAILED to initialize GPU!");
        return;
    }
    ESP_LOGI(TAG, "GPU initialized successfully");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Direct GPU test - verify connection works before starting SceneComposer
    ESP_LOGI(TAG, "Testing direct GPU commands...");
    
    // Test HUB75
    ESP_LOGI(TAG, "  -> HUB75 clear to RED");
    gpu.hub75Clear(255, 0, 0);
    gpu.hub75Present();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "  -> HUB75 clear to GREEN");
    gpu.hub75Clear(0, 255, 0);
    gpu.hub75Present();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "  -> HUB75 clear to BLUE");
    gpu.hub75Clear(0, 0, 255);
    gpu.hub75Present();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test OLED
    ESP_LOGI(TAG, "  -> OLED clear");
    gpu.oledClear();
    gpu.oledText(10, 50, "GPU OK!");
    gpu.oledPresent();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Direct GPU test complete - if you didn't see color changes, check GPU connection!");
    
    // === SIMPLIFIED ANIMATION TEST ===
    // Before using SceneComposer, test if raw animation works
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SIMPLE RAW ANIMATION TEST (no SceneComposer) ===");
    ESP_LOGI(TAG, "You should see a moving rectangle on HUB75...");
    
    for (int frame = 0; frame < 120; frame++) {  // 2 seconds at 60fps
        int x = frame % 100;  // Move from 0 to 99
        
        // Clear and draw
        gpu.hub75Clear(0, 0, 30);  // Dark blue background
        gpu.hub75Fill(x, 5, 20, 20, 255, 100, 0);  // Orange rectangle
        gpu.hub75Text(5, 26, "MOVING", 255, 255, 255);  // White text
        gpu.hub75Present();
        
        // Also update OLED
        gpu.oledClear();
        gpu.oledRect(x, 40, 20, 20, true);
        gpu.oledText(10, 100, "FRAME TEST");
        gpu.oledPresent();
        
        vTaskDelay(pdMS_TO_TICKS(16));  // ~60fps
    }
    
    ESP_LOGI(TAG, "Raw animation test complete. Did you see movement?");
    ESP_LOGI(TAG, "If YES - SceneComposer has a bug");
    ESP_LOGI(TAG, "If NO  - GPU connection or firmware issue");
    ESP_LOGI(TAG, "");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Initialize Scene Composer
    ESP_LOGI(TAG, "Initializing Scene Composer...");
    composer.init(&gpu);
    
    // Build initial scenes
    Scene* oledDemo = buildOledDemoScene();
    Scene* oledStatic = buildOledStaticScene();
    Scene* hub75Demo = buildHub75DemoScene();
    
    // Activate scenes
    if (oledDemo) {
        composer.setActiveScene(DisplayTarget::OLED, oledDemo);
        ESP_LOGI(TAG, "Activated OLED demo scene");
    }
    if (hub75Demo) {
        composer.setActiveScene(DisplayTarget::HUB75, hub75Demo);
        ESP_LOGI(TAG, "Activated HUB75 demo scene");
    }
    
    ESP_LOGI(TAG, "Starting AUTOMATIC TEST SEQUENCE...");
    
    // ============================================================
    // AUTOMATIC TEST CONFIGURATION
    // ============================================================
    const uint32_t TARGET_FPS = 60;                    // Default 60 FPS
    const uint32_t MIN_FPS = 30;                       // Minimum acceptable FPS
    const uint32_t FRAME_TIME_MS = 1000 / TARGET_FPS;  // ~16ms for 60fps
    const uint32_t TEST_DURATION_MS = 3000;            // Each test phase: 3 seconds
    const uint32_t FPS_REPORT_INTERVAL_MS = 1000;      // Report FPS every second
    
    uint8_t rxBuf[16];
    uint32_t frameCount = 0;
    uint32_t lastFpsTime = 0;
    uint32_t testStartTime = 0;
    uint8_t testPhase = 0;
    bool autoTestMode = true;
    float measuredFps = 0.0f;
    
    // Test phase descriptions
    const char* testNames[] = {
        "OLED Demo + HUB75 Demo (animations running)",
        "OLED Static + HUB75 Demo",
        "OLED Demo + HUB75 Demo (PAUSED)",
        "OLED Demo + HUB75 Demo (RESUMED)",
        "Dynamic Scene Creation",
        "Scene Deletion + Recreation",
        "OLED Orientation Cycle (hardware)",
        "HUB75 Transform: ROTATE_180",
        "HUB75 Transform: MIRROR_X",
        "HUB75 Transform: MIRROR_Y",
        "Reset Transforms to NORMAL",
        "Final: All animations running"
    };
    const uint8_t NUM_TEST_PHASES = 12;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           AUTOMATIC TEST MODE                                ║\n");
    printf("║  Target FPS: %lu (minimum: %lu)                              ║\n", TARGET_FPS, MIN_FPS);
    printf("║  Each test phase: %lu seconds                                ║\n", TEST_DURATION_MS / 1000);
    printf("║  Press 'm' for manual mode                                   ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    ESP_LOGI(TAG, "=== TEST PHASE %d: %s ===", testPhase + 1, testNames[testPhase]);
    testStartTime = (uint32_t)(esp_timer_get_time() / 1000);
    
    while (true) {
        uint32_t currentTime = (uint32_t)(esp_timer_get_time() / 1000);  // ms
        uint32_t frameStartTime = currentTime;
        
        // Check for user input (switch to manual mode)
        int len = uart_read_bytes(UART_NUM_0, rxBuf, sizeof(rxBuf), 0);
        if (len > 0) {
            char cmd = rxBuf[0];
            if (cmd == 'm' || cmd == 'M') {
                autoTestMode = !autoTestMode;
                ESP_LOGI(TAG, "Switched to %s mode", autoTestMode ? "AUTOMATIC" : "MANUAL");
            }
        }
        
        // ============================================================
        // AUTOMATIC TEST SEQUENCE
        // ============================================================
        if (autoTestMode && (currentTime - testStartTime >= TEST_DURATION_MS)) {
            // Move to next test phase
            testPhase++;
            if (testPhase >= NUM_TEST_PHASES) {
                testPhase = 0;  // Loop back
                ESP_LOGI(TAG, "=== TEST CYCLE COMPLETE - RESTARTING ===");
            }
            
            testStartTime = currentTime;
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "=== TEST PHASE %d/%d: %s ===", testPhase + 1, NUM_TEST_PHASES, testNames[testPhase]);
            
            Scene* activeOled = composer.getActiveScene(DisplayTarget::OLED);
            Scene* activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
            
            switch (testPhase) {
                case 0:
                    // Phase 1: OLED Demo + HUB75 Demo (fresh start)
                    if (!oledDemo) oledDemo = buildOledDemoScene();
                    if (!hub75Demo) hub75Demo = buildHub75DemoScene();
                    if (oledDemo) {
                        oledDemo->paused = false;
                        composer.setActiveScene(DisplayTarget::OLED, oledDemo);
                    }
                    if (hub75Demo) {
                        hub75Demo->paused = false;
                        composer.setActiveScene(DisplayTarget::HUB75, hub75Demo);
                    }
                    break;
                    
                case 1:
                    // Phase 2: Switch to OLED Static
                    if (!oledStatic) oledStatic = buildOledStaticScene();
                    if (oledStatic) {
                        composer.setActiveScene(DisplayTarget::OLED, oledStatic);
                        ESP_LOGI(TAG, "  -> Switched OLED to Static scene");
                    }
                    break;
                    
                case 2:
                    // Phase 3: Pause animations
                    if (!oledDemo) oledDemo = buildOledDemoScene();
                    if (oledDemo) {
                        composer.setActiveScene(DisplayTarget::OLED, oledDemo);
                        oledDemo->paused = true;
                        ESP_LOGI(TAG, "  -> OLED animations PAUSED");
                    }
                    activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
                    if (activeHub75) {
                        activeHub75->paused = true;
                        ESP_LOGI(TAG, "  -> HUB75 animations PAUSED");
                    }
                    break;
                    
                case 3:
                    // Phase 4: Resume animations
                    activeOled = composer.getActiveScene(DisplayTarget::OLED);
                    if (activeOled) {
                        activeOled->paused = false;
                        ESP_LOGI(TAG, "  -> OLED animations RESUMED");
                    }
                    activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
                    if (activeHub75) {
                        activeHub75->paused = false;
                        ESP_LOGI(TAG, "  -> HUB75 animations RESUMED");
                    }
                    break;
                    
                case 4:
                    // Phase 5: Create dynamic scene
                    {
                        Scene* newScene = composer.createScene(DisplayTarget::OLED, "AutoTest");
                        if (newScene) {
                            composer.addText(newScene, 10, 30, "AUTO TEST", Color::White(), 0);
                            composer.addText(newScene, 10, 45, "DYNAMIC", Color::White(), 0);
                            Layer* anim = composer.addCircle(newScene, 64, 90, 15, Color::White(), true, 0);
                            if (anim) {
                                composer.setAnimation(anim, AnimationType::PING_PONG, 0.5f, pulsingCircleUpdate);
                            }
                            composer.addRectangle(newScene, 5, 5, 117, 117, Color::White(), false, -1);
                            composer.setActiveScene(DisplayTarget::OLED, newScene);
                            ESP_LOGI(TAG, "  -> Created dynamic scene with %d layers", newScene->layerCount);
                        }
                    }
                    break;
                    
                case 5:
                    // Phase 6: Delete and recreate
                    activeOled = composer.getActiveScene(DisplayTarget::OLED);
                    if (activeOled) {
                        uint8_t oldId = activeOled->id;
                        composer.deleteScene(DisplayTarget::OLED, oldId);
                        ESP_LOGI(TAG, "  -> Deleted scene %d", oldId);
                        
                        // Clear references
                        if (oledDemo && oledDemo->id == oldId) oledDemo = nullptr;
                        if (oledStatic && oledStatic->id == oldId) oledStatic = nullptr;
                    }
                    // Recreate demo scene
                    oledDemo = buildOledDemoScene();
                    if (oledDemo) {
                        composer.setActiveScene(DisplayTarget::OLED, oledDemo);
                        ESP_LOGI(TAG, "  -> Recreated OLED Demo scene");
                    }
                    break;
                    
                case 6:
                    // Phase 7: OLED Orientation Cycle (hardware)
                    // Test multiple OLED orientations using hardware command
                    ESP_LOGI(TAG, "  -> Testing OLED hardware orientation modes");
                    
                    // Cycle through several orientations quickly for demonstration
                    for (int orient = 0; orient <= 3; orient++) {
                        composer.setOledOrientation(static_cast<OledOrientation>(orient));
                        ESP_LOGI(TAG, "     OLED orientation: %d", orient);
                        
                        // Render a frame with this orientation
                        composer.update(currentTime);
                        composer.renderAndPresent();
                        vTaskDelay(pdMS_TO_TICKS(700));  // Show each orientation for 700ms
                    }
                    // Reset to normal
                    composer.setOledOrientation(OledOrientation::NORMAL);
                    ESP_LOGI(TAG, "  -> OLED orientation reset to NORMAL");
                    break;
                    // Phase 7: Final - all running
                    if (!oledDemo) oledDemo = buildOledDemoScene();
                    if (oledDemo) {
                        oledDemo->paused = false;
                        composer.setActiveScene(DisplayTarget::OLED, oledDemo);
                    }
                    activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
                    if (activeHub75) activeHub75->paused = false;
                    ESP_LOGI(TAG, "  -> All animations running");
                    break;
                    
                case 7:
                    // Phase 8: HUB75 Transform ROTATE_180
                    activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
                    if (activeHub75) {
                        composer.setSceneTransform(activeHub75, PanelTransform::ROTATE_180);
                        composer.setSceneDisplaySize(activeHub75, 128, 32);  // HUB75 dimensions
                        ESP_LOGI(TAG, "  -> HUB75 ROTATE_180 transform applied");
                    }
                    break;
                    
                case 8:
                    // Phase 9: HUB75 Transform MIRROR_X
                    activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
                    if (activeHub75) {
                        composer.setSceneTransform(activeHub75, PanelTransform::MIRROR_X);
                        ESP_LOGI(TAG, "  -> HUB75 MIRROR_X transform applied");
                    }
                    break;
                    
                case 9:
                    // Phase 10: HUB75 Transform MIRROR_Y
                    activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
                    if (activeHub75) {
                        composer.setSceneTransform(activeHub75, PanelTransform::MIRROR_Y);
                        ESP_LOGI(TAG, "  -> HUB75 MIRROR_Y transform applied");
                    }
                    break;
                    
                case 10:
                    // Phase 11: Reset transforms to NORMAL
                    activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
                    if (activeHub75) {
                        composer.setSceneTransform(activeHub75, PanelTransform::NONE);
                        ESP_LOGI(TAG, "  -> HUB75 reset to NONE (no transform)");
                    }
                    // Reset OLED orientation via hardware
                    composer.setOledOrientation(OledOrientation::NORMAL);
                    ESP_LOGI(TAG, "  -> OLED reset to NORMAL orientation");
                    break;
                    
                case 11:
                    // Phase 12: Final - all running, no transforms
                    if (!oledDemo) oledDemo = buildOledDemoScene();
                    if (oledDemo) {
                        oledDemo->paused = false;
                        composer.setActiveScene(DisplayTarget::OLED, oledDemo);
                    }
                    activeHub75 = composer.getActiveScene(DisplayTarget::HUB75);
                    if (activeHub75) {
                        activeHub75->paused = false;
                        composer.setSceneTransform(activeHub75, PanelTransform::NONE);
                    }
                    ESP_LOGI(TAG, "  -> All animations running, transforms reset");
                    break;
            }
        }
        
        // Update animations
        composer.update(currentTime);
        
        // Render scenes
        composer.renderAndPresent();
        
        // FPS tracking
        frameCount++;
        if (currentTime - lastFpsTime >= FPS_REPORT_INTERVAL_MS) {
            measuredFps = frameCount * 1000.0f / (currentTime - lastFpsTime);
            
            // Color-coded FPS output
            if (measuredFps >= TARGET_FPS - 5) {
                ESP_LOGI(TAG, "FPS: %.1f [OK]", measuredFps);
            } else if (measuredFps >= MIN_FPS) {
                ESP_LOGW(TAG, "FPS: %.1f [BELOW TARGET]", measuredFps);
            } else {
                ESP_LOGE(TAG, "FPS: %.1f [CRITICAL - BELOW MINIMUM]", measuredFps);
            }
            
            frameCount = 0;
            lastFpsTime = currentTime;
        }
        
        // Frame timing for 60 FPS (adaptive delay)
        uint32_t frameElapsed = (uint32_t)(esp_timer_get_time() / 1000) - frameStartTime;
        if (frameElapsed < FRAME_TIME_MS) {
            vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS - frameElapsed));
        } else {
            vTaskDelay(1);  // Minimum yield
        }
    }
}
