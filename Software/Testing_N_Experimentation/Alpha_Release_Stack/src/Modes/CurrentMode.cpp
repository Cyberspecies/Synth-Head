/**
 * @file CurrentMode.cpp
 * @brief Current mode implementation using SystemAPI
 * 
 * This is the main application mode that orchestrates:
 * - Hardware drivers (GPS, Mic, IMU, Fan) via modular driver files
 * - GPU communication via GpuCommands
 * - Web server and captive portal
 * - Dual-core application layer (animation on Core 1)
 * 
 * Hardware drivers are now in separate files under src/Drivers/ and include/Drivers/
 */

#include "CurrentMode.hpp"
#include "SystemAPI/SystemAPI.hpp"
#include "SystemAPI/Web/CaptivePortal.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "SystemAPI/Utils/FileSystemService.hpp"
#include "SystemAPI/GPU/GpuDriver.h"  // SAME as WifiSpriteUploadTest
#include "Application/Application.hpp"  // Dual-core application layer
#include "Application/Pipeline/SceneRenderer.hpp"  // Scene renderer for Core 1

// Animation System
#include "AnimationSystem/AnimationSystem.hpp"
#include "AnimationSystem/AnimationSandbox.hpp"
#include "Modes/AnimationHandler.hpp"

// Modular hardware drivers
#include "Drivers/GpsDriver.hpp"
#include "Drivers/MicDriver.hpp"
#include "Drivers/ImuDriver.hpp"
#include "Drivers/FanDriver.hpp"

#include "esp_timer.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>

namespace Modes {

//=============================================================================
// GPU UART Driver - ESP-to-ESP Communication (Proper Protocol)
//=============================================================================
// Use the SAME GpuDriver as WifiSpriteUploadTest (from SystemAPI/GPU/GpuDriver.h)
// Import types from SystemAPI namespace for cleaner usage
using SystemAPI::GpuDriver;
using SystemAPI::GpuConfig;
using SystemAPI::GpuTarget;
using SystemAPI::SpriteFormat;
using SystemAPI::Color;

static GpuDriver g_gpu;  // Global GpuDriver instance - SAME as working test

namespace GpuDriverState {
    // GPU UART pins (from PIN_MAPPING_CPU.md)
    static constexpr int GPU_TX_PIN = 12;   // CPU TX -> GPU RX
    static constexpr int GPU_RX_PIN = 11;   // CPU RX <- GPU TX
    
    static bool initialized = false;
    static bool connected = false;
    static uint32_t gpuUptimeMs = 0;    // GPU uptime from PONG response
    static uint32_t lastPingTime = 0;
    static uint32_t lastStatsTime = 0;
    static constexpr uint32_t PING_INTERVAL_MS = 5000;  // Ping every 5s (reduced to avoid congestion)
    static constexpr uint32_t STATS_INTERVAL_MS = 10000; // Fetch stats every 10s
    
    // GPU stats
    static float gpuFps = 0.0f;
    static uint32_t gpuFreeHeap = 0;
    static uint32_t gpuMinHeap = 0;
    static uint8_t gpuLoad = 0;
    static uint32_t gpuTotalFrames = 0;
    static bool gpuHub75Ok = false;
    static bool gpuOledOk = false;
    
    // ====== SPRITE RENDERING STATE (like WifiSpriteUploadTest) ======
    // This allows continuous rendering at ~60fps from the update loop
    static bool spriteReady = false;        // Is sprite uploaded and ready to render?
    static uint8_t activeSpriteId = 0;      // GPU sprite cache slot
    static float spriteX = 64.0f;           // Center X position
    static float spriteY = 16.0f;           // Center Y position
    static float spriteAngle = 0.0f;        // Rotation angle (degrees)
    static uint8_t bgR = 0, bgG = 0, bgB = 0;  // Background color
    static uint32_t lastRenderTime = 0;     // For fps throttle
    static constexpr uint32_t RENDER_INTERVAL_MS = 22;  // ~45fps (GPU-safe, prevents buffer overflow)
    static bool autoRotate = false;         // Auto-rotate flag (true for test, false for web uploads)
    
    // ====== SANDBOX MODE STATE ======
    static bool sandboxEnabled = false;
    static float sandboxPitch = 0.0f;
    static float sandboxRoll = 0.0f;
    static float sandboxAudioLevel = 0.0f;
    
    // ====== HIGH-FREQUENCY IMU TASK ======
    static TaskHandle_t imuTaskHandle = nullptr;
    static bool imuTaskRunning = false;
    
    // ====== EYE SPRITE CREATION ======
    // Create a filled circle sprite for AA eye rendering
    static void createCircleSprite(uint8_t* data, int size, uint8_t r, uint8_t g, uint8_t b) {
        float cx = size / 2.0f;
        float cy = size / 2.0f;
        float radius = (size / 2.0f) - 1.0f;
        
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int idx = (y * size + x) * 3;
                float dx = x - cx + 0.5f;
                float dy = y - cy + 0.5f;
                float dist = sqrtf(dx * dx + dy * dy);
                
                if (dist <= radius) {
                    // Inside circle - full color with slight gradient for depth
                    float shade = 1.0f - (dist / radius) * 0.2f;
                    data[idx + 0] = (uint8_t)(r * shade);
                    data[idx + 1] = (uint8_t)(g * shade);
                    data[idx + 2] = (uint8_t)(b * shade);
                } else {
                    // Outside circle - transparent (black for now, GPU handles transparency)
                    data[idx + 0] = 0;
                    data[idx + 1] = 0;
                    data[idx + 2] = 0;
                }
            }
        }
    }
    
    // Upload eye sprites to GPU for AA rendering
    static void uploadEyeSprites() {
        const int EYE_SIZE = 24;  // 24x24 pixel circle sprites
        uint8_t spriteData[EYE_SIZE * EYE_SIZE * 3];
        
        printf("  uploadEyeSprites: Creating %dx%d circle sprites...\n", EYE_SIZE, EYE_SIZE);
        
        // Sprite 0: Left eye (white circle)
        createCircleSprite(spriteData, EYE_SIZE, 255, 255, 255);
        
        // Debug: print first 12 bytes of sprite data
        printf("  Sprite 0 first 12 bytes: ");
        for (int i = 0; i < 12; i++) {
            printf("%02X ", spriteData[i]);
        }
        printf("\n");
        
        bool result0 = g_gpu.uploadSprite(0, EYE_SIZE, EYE_SIZE, spriteData, SpriteFormat::RGB888);
        printf("  Eye sprite 0 upload: %s\n", result0 ? "SUCCESS" : "FAILED");
        vTaskDelay(pdMS_TO_TICKS(100));  // Give GPU time to process
        
        // Sprite 1: Right eye (same as left - white circle)
        createCircleSprite(spriteData, EYE_SIZE, 255, 255, 255);
        bool result1 = g_gpu.uploadSprite(1, EYE_SIZE, EYE_SIZE, spriteData, SpriteFormat::RGB888);
        printf("  Eye sprite 1 upload: %s\n", result1 ? "SUCCESS" : "FAILED");
        vTaskDelay(pdMS_TO_TICKS(100));
        
        printf("  uploadEyeSprites: Done!\n");
    }
    
    // IMU task: Polls gyro at 100Hz (10ms) for better responsiveness
    static void imuTask(void* param) {
        printf("  GPU: IMU high-frequency task started (100Hz)\n");
        while (imuTaskRunning) {
            // Update IMU at 100Hz
            Drivers::ImuDriver::update();
            vTaskDelay(pdMS_TO_TICKS(10));  // 10ms = 100Hz
        }
        vTaskDelete(nullptr);
    }
    
    bool init() {
        if (initialized) return true;
        
        // Initialize GPU using SAME pattern as WifiSpriteUploadTest
        GpuConfig gpuConfig;
        gpuConfig.uartPort = UART_NUM_1;
        gpuConfig.txPin = (gpio_num_t)GPU_TX_PIN;
        gpuConfig.rxPin = (gpio_num_t)GPU_RX_PIN;
        gpuConfig.baudRate = 10000000;  // 10 Mbps
        gpuConfig.gpuBootDelayMs = 500; // Wait for GPU to boot
        gpuConfig.weightedPixels = true; // Enable AA
        
        if (!g_gpu.init(gpuConfig)) {
            printf("  GPU: Init failed\n");
            return false;
        }
        
        // Start keep-alive (SAME as working test)
        g_gpu.startKeepAlive(1000);
        g_gpu.reset();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Clear display on init - no startup animation
        g_gpu.setTarget(GpuTarget::HUB75);
        g_gpu.clear(0, 0, 0);
        g_gpu.present();
        
        initialized = true;
        connected = true;  // Assume connected after successful init
        lastPingTime = 0;
        printf("  GPU: Initialized via GpuDriver (TX:%d, RX:%d @ 10Mbps)\n", GPU_TX_PIN, GPU_RX_PIN);
        printf("  GPU: Keep-alive started, display initialized\n");
        
        // Start high-frequency IMU task (100Hz for better gyro responsiveness)
        imuTaskRunning = true;
        xTaskCreatePinnedToCore(
            imuTask,
            "IMU_Task",
            2048,  // Stack size
            nullptr,
            5,     // Priority (higher than main loop)
            &imuTaskHandle,
            0      // Core 0 (same as main)
        );
        
        return true;
    }
    
    // Create and upload test sprite on startup (same as SimpleSpriteTest)
    void uploadTestSprite() {
        if (!initialized) return;
        
        printf("\n  ====== UPLOADING STARTUP TEST SPRITE ======\n");
        
        // Create 16x16 test sprite with X pattern (same as SimpleSpriteTest)
        constexpr int SPRITE_W = 16;
        constexpr int SPRITE_H = 16;
        std::vector<uint8_t> spriteData(SPRITE_W * SPRITE_H * 3);  // RGB888
        
        for (int y = 0; y < SPRITE_H; y++) {
            for (int x = 0; x < SPRITE_W; x++) {
                int idx = (y * SPRITE_W + x) * 3;
                
                // Draw X pattern with green border and blue fill
                bool isEdge = (x == 0 || x == SPRITE_W-1 || y == 0 || y == SPRITE_H-1);
                bool isDiag1 = (x == y);
                bool isDiag2 = (x == SPRITE_W - 1 - y);
                
                if (isEdge) {
                    // Green border
                    spriteData[idx] = 0;
                    spriteData[idx+1] = 255;
                    spriteData[idx+2] = 0;
                } else if (isDiag1 || isDiag2) {
                    // Red X pattern
                    spriteData[idx] = 255;
                    spriteData[idx+1] = 0;
                    spriteData[idx+2] = 0;
                } else {
                    // Blue fill
                    spriteData[idx] = 0;
                    spriteData[idx+1] = 0;
                    spriteData[idx+2] = 128;
                }
            }
        }
        
        // Upload to GPU slot 0
        uint8_t spriteId = 0;
        g_gpu.deleteSprite(spriteId);
        vTaskDelay(pdMS_TO_TICKS(50));
        
        if (g_gpu.uploadSprite(spriteId, SPRITE_W, SPRITE_H, spriteData.data(), SpriteFormat::RGB888)) {
            printf("  Test sprite uploaded to GPU slot %d\n", spriteId);
            vTaskDelay(pdMS_TO_TICKS(200));  // Wait for GPU to process
            
            // Enable continuous rotation rendering
            spriteReady = true;
            activeSpriteId = spriteId;
            spriteX = 64.0f;
            spriteY = 16.0f;
            spriteAngle = 0.0f;
            bgR = 5; bgG = 5; bgB = 15;  // Dark blue background
            autoRotate = true;  // TEST sprite rotates
            
            printf("  Continuous rotation rendering ENABLED\n");
            printf("  ====== TEST SPRITE READY ======\n\n");
        } else {
            printf("  ERROR: Failed to upload test sprite!\n");
        }
    }
    
    // Increment rotation angle each frame (for test sprite)
    void incrementAngle() {
        if (spriteReady) {
            spriteAngle += 1.0f;
            if (spriteAngle >= 360.0f) spriteAngle -= 360.0f;
        }
    }
    
    // Non-blocking update - periodically ping GPU
    // Track render loop for debugging
    static uint32_t renderFrameCount = 0;
    static uint32_t lastRenderDebugTime = 0;
    
    void update(uint32_t currentTimeMs) {
        if (!initialized) return;
        
        // Send periodic ping (GpuDriver has built-in keep-alive, so this is just for status tracking)
        if (currentTimeMs - lastPingTime >= PING_INTERVAL_MS) {
            lastPingTime = currentTimeMs;
            
            // Use simple ping - GpuDriver handles keep-alive internally
            if (g_gpu.ping(100)) {  // 100ms timeout
                connected = true;
            } else {
                connected = false;
            }
        }
        
        // ====== CONTINUOUS RENDERING AT ~30fps ======
        if (connected && (currentTimeMs - lastRenderTime >= RENDER_INTERVAL_MS)) {
            lastRenderTime = currentTimeMs;
            renderFrameCount++;
            
            g_gpu.setTarget(GpuTarget::HUB75);
            
            // Check if sandbox mode is enabled
            if (sandboxEnabled) {
                // Sandbox handles its own rendering through the loop
                auto& sandbox = AnimationSystem::Sandbox::getSandbox();
                sandbox.gyroX = sandboxPitch;
                sandbox.gyroY = sandboxRoll;
                sandbox.gyroZ = sandboxAudioLevel;
                sandbox.update(RENDER_INTERVAL_MS);
                
                // Sandbox renders directly to GPU
                // Note: sandbox.render() calls clear/draw/present internally
                sandbox.render();
                
                // Debug every 30 frames
                if (renderFrameCount % 30 == 0) {
                    printf("SANDBOX: Frame %lu - anim=%d gyroX=%.1f gyroY=%.1f gyroZ=%.1f\n",
                           renderFrameCount, (int)sandbox.currentAnim, sandboxPitch, sandboxRoll, sandboxAudioLevel);
                }
            }
            else if (spriteReady) {
                // Sprite rendering mode (like WifiSpriteUploadTest)
                if (autoRotate) {
                    incrementAngle();
                }
                
                g_gpu.clear(bgR, bgG, bgB);
                g_gpu.blitSpriteRotated(activeSpriteId, spriteX, spriteY, spriteAngle);
                g_gpu.present();
                
                if (renderFrameCount % 30 == 0) {
                    printf("DEBUG RENDER: Frame %lu - sprite=%u pos=(%.1f,%.1f) angle=%.1f\n",
                           renderFrameCount, activeSpriteId, spriteX, spriteY, spriteAngle);
                }
            }
        }
        
        // Debug: Print render state every 5 seconds
        if (currentTimeMs - lastRenderDebugTime >= 5000) {
            lastRenderDebugTime = currentTimeMs;
            printf("DEBUG STATE: sandbox=%d spriteReady=%d connected=%d frames=%lu\n",
                   sandboxEnabled, spriteReady, connected, renderFrameCount);
        }
        
        // Note: GpuDriver from SystemAPI doesn't have stats/alerts like GpuCommands
        // Those would need to be added separately if needed
    }
    
    // Set sprite scene for continuous rendering (web uploads use this - no auto-rotate)
    void setSpriteScene(uint8_t spriteId, float x, float y, float angle, uint8_t r, uint8_t g, uint8_t b) {
        activeSpriteId = spriteId;
        spriteX = x;
        spriteY = y;
        spriteAngle = angle;
        bgR = r;
        bgG = g;
        bgB = b;
        spriteReady = true;
        autoRotate = false;  // Web uploads don't auto-rotate
        printf("DEBUG: Sprite scene set (static) - ID=%u pos=(%.1f,%.1f) angle=%.1f bg=(%u,%u,%u)\n",
               spriteId, x, y, angle, r, g, b);
    }
    
    // Clear sprite scene (stop rendering)
    void clearSpriteScene() {
        spriteReady = false;
        printf("DEBUG: Sprite scene cleared\n");
    }
    
    // ====== SANDBOX MODE ======
    // When enabled, sandbox takes over the render loop
    void enableSandbox(bool enable) {
        sandboxEnabled = enable;
        if (enable) {
            spriteReady = false;  // Disable sprite rendering when sandbox is on
        }
    }
    
    bool isSandboxEnabled() { return sandboxEnabled; }
    
    void updateSandboxSensors(float gyroX, float gyroY, float gyroZ) {
        sandboxPitch = gyroX;
        sandboxRoll = gyroY;
        sandboxAudioLevel = gyroZ;
    }
    
    // Getters - provide access to the global GpuDriver
    SystemAPI::GpuDriver& getGpu() { return g_gpu; }
    uint32_t getGpuUptime() { return gpuUptimeMs; }
    bool isConnected() { return connected; }
}

// Simulated sensor values (for sensors not yet implemented)
static float simTemp = 22.5f;
static float simHumidity = 45.0f;
static float simPressure = 1013.25f;

//=============================================================================
// CurrentMode Implementation
//=============================================================================

void CurrentMode::onStart() {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║        CURRENT MODE STARTED        ║\n");
    printf("  ╚════════════════════════════════════╝\n\n");
    
    // Initialize GPS driver (using modular driver)
    if (Drivers::GpsDriver::init()) {
        printf("  GPS: Ready\n");
    } else {
        printf("  GPS: Init failed - will show N/C\n");
    }
    
    // Initialize Microphone driver (using modular driver)
    if (Drivers::MicDriver::init()) {
        printf("  MIC: Ready\n");
    } else {
        printf("  MIC: Init failed - will use simulation\n");
    }
    
    // Initialize IMU driver (using modular driver)
    if (Drivers::ImuDriver::init()) {
        printf("  IMU: Ready\n");
    } else {
        printf("  IMU: Init failed - will use simulation\n");
    }

    // Initialize Fan driver (using modular driver)
    if (Drivers::FanDriver::init()) {
        printf("  FAN: Ready\n");
    } else {
        printf("  FAN: Init failed\n");
    }

    // Initialize GPU UART driver
    if (GpuDriverState::init()) {
        printf("  GPU: UART Ready - waiting for connection\n");
        
        // NOTE: Test sprite upload removed - web upload pipeline verified working!
        // GpuDriverState::uploadTestSprite();  // Uncomment to test GPU on startup
    } else {
        printf("  GPU: UART init failed - will show N/C\n");
    }
    
    // Initialize SD Card
    auto& sdCard = SystemAPI::Utils::FileSystemService::instance();
    SystemAPI::Utils::SdCardPins sdPins = {
        .miso = 14,
        .mosi = 47,
        .clk = 21,
        .cs = 48
    };
    if (sdCard.init(sdPins)) {
        printf("  SD Card: Ready (%llu MB total, %llu MB free)\n", 
               sdCard.getTotalBytes() / (1024 * 1024),
               sdCard.getFreeBytes() / (1024 * 1024));
    } else {
        printf("  SD Card: Not available\n");
    }
    
    // =====================================================
    // DISABLED: Dual-Core Application Layer
    // The Application layer has its own GpuPipeline on Core 1 which
    // conflicts with the GpuDriver we're using. To match the working
    // WifiSpriteUploadTest, we use only GpuDriver from Core 0.
    // =====================================================
    printf("\n  ┌────────────────────────────────────┐\n");
    printf("  │   SINGLE-CORE GPU MODE (TEST)      │\n");
    printf("  └────────────────────────────────────┘\n");
    printf("  Using GpuDriver from Core 0 only (like WifiSpriteUploadTest)\n");
    printf("  Application layer DISABLED to avoid UART conflict\n\n");
    
    // =====================================================
    // Initialize Animation Handler
    // =====================================================
    auto& animHandler = Modes::getAnimationHandler();
    if (animHandler.init()) {
        printf("  AnimationHandler: Initialized\n");
        
        // Wire GPU callbacks to AnimationHandler
        animHandler.wireGpuCallbacks(
            // Clear function
            [](uint8_t r, uint8_t g, uint8_t b) {
                GpuDriverState::getGpu().setTarget(GpuTarget::HUB75);
                GpuDriverState::getGpu().clear(r, g, b);
            },
            // Blit sprite function (use float for smooth sub-pixel positioning)
            [](int id, float x, float y) {
                GpuDriverState::getGpu().blitSpriteF(static_cast<uint8_t>(id), x, y);
            },
            // Blit sprite rotated function
            [](int id, float x, float y, float angle) {
                GpuDriverState::getGpu().blitSpriteRotated(static_cast<uint8_t>(id), x, y, angle);
            },
            // Fill circle function (use drawCircle - no filled variant in GpuDriver)
            [](int cx, int cy, int r, uint8_t red, uint8_t green, uint8_t blue) {
                GpuDriverState::getGpu().drawCircle(static_cast<int16_t>(cx), 
                                                     static_cast<int16_t>(cy), 
                                                     static_cast<int16_t>(r), 
                                                     red, green, blue);
            },
            // Fill rect function (use drawFilledRect)
            [](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
                GpuDriverState::getGpu().drawFilledRect(static_cast<int16_t>(x), 
                                                         static_cast<int16_t>(y), 
                                                         static_cast<int16_t>(w), 
                                                         static_cast<int16_t>(h), 
                                                         r, g, b);
            },
            // Present function
            []() {
                GpuDriverState::getGpu().present();
            }
        );
        printf("  AnimationHandler: GPU callbacks wired\n");
    } else {
        printf("  AnimationHandler: Init failed\n");
    }
    
    // =====================================================
    // Initialize Animation Sandbox (EXPERIMENTAL)
    // =====================================================
    auto& sandbox = AnimationSystem::Sandbox::getSandbox();
    sandbox.clear = [](uint8_t r, uint8_t g, uint8_t b) {
        GpuDriverState::getGpu().setTarget(GpuTarget::HUB75);
        GpuDriverState::getGpu().clear(r, g, b);
    };
    sandbox.fillRect = [](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
        GpuDriverState::getGpu().drawFilledRect(static_cast<int16_t>(x), 
                                                 static_cast<int16_t>(y), 
                                                 static_cast<int16_t>(w), 
                                                 static_cast<int16_t>(h), 
                                                 r, g, b);
    };
    sandbox.drawPixel = [](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        GpuDriverState::getGpu().drawPixel(static_cast<int16_t>(x), 
                                            static_cast<int16_t>(y), 
                                            r, g, b);
    };
    sandbox.blitSprite = [](int id, float x, float y) {
        GpuDriverState::getGpu().blitSpriteF(static_cast<uint8_t>(id), x, y);
    };
    sandbox.blitSpriteRotated = [](int id, float x, float y, float angle) {
        GpuDriverState::getGpu().blitSpriteRotated(static_cast<uint8_t>(id), x, y, angle);
    };
    sandbox.drawCircleF = [](float x, float y, float radius, uint8_t r, uint8_t g, uint8_t b) {
        GpuDriverState::getGpu().drawCircleF(x, y, radius, r, g, b);
    };
    sandbox.present = []() {
        GpuDriverState::getGpu().present();
        // Small yield to let GPU process (prevents UART buffer overflow at high FPS)
        vTaskDelay(1);  // 1 tick = ~1ms - gives GPU time to drain command buffer
    };
    sandbox.setEnabled(true);  // Mark sandbox as enabled
    GpuDriverState::enableSandbox(true);  // Enable sandbox in render loop
    printf("  AnimationSandbox: Enabled (5s cycle: GYRO_EYES -> GLITCH_TV -> SDF_MORPH)\n");
    
    // Upload eye sprites for AA rendering
    printf("  Uploading eye sprites for AA rendering...\n");
    GpuDriverState::uploadEyeSprites();
    printf("  Eye sprites ready!\n");
    
    /*
    // DISABLED: This creates a conflicting UART driver on Core 1
    if (Application::init()) {
        printf("  App Layer: Initialized\\n");
        
        // Configure eye controller
        auto& eye = Application::eye();
        Application::EyeControllerConfig eyeConfig;
        eyeConfig.autoBlinkEnabled = true;
        eyeConfig.autoBlinkIntervalMin = 2.5f;
        eyeConfig.autoBlinkIntervalMax = 5.0f;
        eyeConfig.idleLookEnabled = true;
        eyeConfig.idleLookRange = 0.3f;
        eyeConfig.imuLookEnabled = true;  // Enable IMU-driven eye movement
        eyeConfig.imuSensitivity = 0.03f;
        eyeConfig.imuDeadzone = 8.0f;
        eyeConfig.defaultShader = 1;      // Rainbow
        eyeConfig.defaultBrightness = 80;
        eyeConfig.mirrorMode = true;
        eye.configure(eyeConfig);
        printf("  Eye Controller: Configured\\n");
        
        // Start dual-core execution (GPU task on Core 1)
        if (Application::start()) {
            printf("  Core 1 GPU Task: Started\\n");
            printf("  Animation Pipeline: Running at 60 FPS\\n");
        } else {
            printf("  Core 1 GPU Task: FAILED TO START\\n");
        }
    } else {
        printf("  App Layer: INIT FAILED\\n");
    }
    */
    
    // =====================================================
    // Set up Web -> GPU Pipeline Callbacks
    // These connect web UI actions to GPU rendering
    // =====================================================
    auto& httpServer = SystemAPI::Web::HttpServer::instance();
    
    // Callback for displaying a sprite scene on HUB75
    // This sets up CONTINUOUS rendering at ~30fps via GpuDriverState::update()
    // (Exactly like WifiSpriteUploadTest approach)
    httpServer.setSpriteDisplayCallback([](const SystemAPI::Web::StaticSpriteSceneConfig& config) {
        printf("\n  ========================================\n");
        printf("  SPRITE DISPLAY - Setting Scene Config\n");
        printf("  Sprite ID: %d\n", config.spriteId);
        printf("  Position: (%d, %d)\n", config.posX, config.posY);
        printf("  Background: RGB(%d, %d, %d)\n", config.bgR, config.bgG, config.bgB);
        
        // Look up the sprite to get pixel data
        auto* sprite = SystemAPI::Web::HttpServer::findSpriteById(config.spriteId);
        if (sprite) {
            printf("  Sprite found: '%s' (%dx%d), %zu bytes\n", 
                   sprite->name.c_str(), sprite->width, sprite->height,
                   sprite->pixelData.size());
            
            // Always re-upload sprite to GPU to ensure fresh data
            if (!sprite->pixelData.empty()) {
                printf("  Uploading sprite to GPU cache...\n");
                uint8_t gpuSpriteId = 0;  // Use slot 0 for web-uploaded sprites
                
                // Delete old sprite first to ensure clean state
                GpuDriverState::getGpu().deleteSprite(gpuSpriteId);
                vTaskDelay(pdMS_TO_TICKS(10));  // Small delay
                
                // Upload sprite (id, width, height, data, format)
                if (GpuDriverState::getGpu().uploadSprite(gpuSpriteId, 
                                                          sprite->width, 
                                                          sprite->height,
                                                          sprite->pixelData.data(),
                                                          SpriteFormat::RGB888)) {
                    // CRITICAL: Wait for GPU to fully process sprite upload
                    vTaskDelay(pdMS_TO_TICKS(200));  // 200ms for safety
                    
                    // Set up continuous rendering scene (like WifiSpriteUploadTest)
                    // Use blitSpriteRotated which centers the sprite at (x, y)
                    // Center of 128x32 display is (64, 16)
                    float centerX = 64.0f;
                    float centerY = 16.0f;
                    float angle = 0.0f;  // No rotation
                    
                    GpuDriverState::setSpriteScene(
                        gpuSpriteId,
                        centerX, centerY, angle,
                        config.bgR, config.bgG, config.bgB
                    );
                    
                    SystemAPI::Web::HttpServer::markSpriteUploaded(config.spriteId);
                    printf("  Sprite uploaded to GPU slot %d (%zu bytes)\n", 
                           gpuSpriteId, sprite->pixelData.size());
                    printf("  Continuous rendering enabled at ~30fps\n");
                } else {
                    printf("  ERROR: Failed to upload sprite to GPU!\n");
                }
            } else {
                printf("  WARNING: No pixel data - showing test pattern\n");
            }
        } else {
            printf("  WARNING: Sprite ID %d not found!\n", config.spriteId);
        }
        printf("  ========================================\n\n");
    });
    
    // Callback for clearing the display (returns to animation mode)
    httpServer.setDisplayClearCallback([]() {
        printf("  Clearing display via GpuDriver\n");
        
        // Stop continuous sprite rendering
        GpuDriverState::clearSpriteScene();
        
        // Clear the HUB75 display
        GpuDriverState::getGpu().setTarget(GpuTarget::HUB75);
        GpuDriverState::getGpu().clear(0, 0, 0);
        GpuDriverState::getGpu().present();
        
        printf("  Display cleared\n");
    });
    
    printf("  Web-GPU Callbacks: Registered\n");
    
    // Print sprite storage summary
    {
        auto& httpServer = SystemAPI::Web::HttpServer::instance();
        const auto& sprites = httpServer.getSprites();
        printf("\n  ┌────────────────────────────────────┐\n");
        printf("  │   SPRITE STORAGE SUMMARY           │\n");
        printf("  └────────────────────────────────────┘\n");
        printf("  Total Sprites Loaded: %zu\n", sprites.size());
        int builtIn = 0, storage = 0;
        for (const auto& sp : sprites) {
            if (sp.id < 100) builtIn++;  // IDs 0-99 are built-in
            else storage++;               // IDs 100+ are from storage
        }
        printf("  Built-in Sprites: %d\n", builtIn);
        printf("  From Storage: %d\n", storage);
        if (!sprites.empty()) {
            printf("  Sprite List:\n");
            for (const auto& sp : sprites) {
                printf("    [%d] %s (%dx%d, %zu bytes)%s\n", 
                       sp.id, sp.name.c_str(), sp.width, sp.height, sp.pixelData.size(),
                       sp.id >= 100 ? " [SAVED]" : "");
            }
        }
        printf("\n");
    }
    
    // Print initial credentials
    auto& security = arcos::security::SecurityDriver::instance();
    printf("  WiFi SSID: %s\n", security.getSSID());
    printf("  WiFi Pass: %s\n", security.getPassword());
    printf("  Portal IP: 192.168.4.1\n");
    printf("  Easy URL:  Type ANY domain (e.g. go.to, a.a)\n");
    printf("\n");
    
    m_updateCount = 0;
    m_totalTime = 0;
    m_credentialPrintTime = 0;
    
    // Initialize SyncState with simulated data
    auto& state = SystemAPI::SYNC_STATE.state();
    state.mode = SystemAPI::SystemMode::RUNNING;
    snprintf(state.statusText, sizeof(state.statusText), "Running");
}

void CurrentMode::onUpdate(uint32_t deltaMs) {
    m_updateCount++;
    m_totalTime += deltaMs;
    m_credentialPrintTime += deltaMs;
    
    // Update captive portal (handles DNS, HTTP, WebSocket)
    auto& portal = SystemAPI::Web::CaptivePortal::instance();
    portal.update();
    
    // Update hardware drivers (non-blocking) - using modular drivers
    uint32_t currentTimeMs = (uint32_t)(esp_timer_get_time() / 1000);
    Drivers::GpsDriver::update();
    Drivers::MicDriver::update();
    // IMU now runs in dedicated 100Hz task for better responsiveness
    // Drivers::ImuDriver::update();  // REMOVED - runs in imuTask at 100Hz
    GpuDriverState::update(currentTimeMs);

    // Get SyncState reference
    auto& state = SystemAPI::SYNC_STATE.state();

    // Update fan based on web UI state (using modular driver)
    Drivers::FanDriver::update(state.fanEnabled);
    
    // Update system stats
    state.uptime = esp_timer_get_time() / 1000000; // seconds
    state.freeHeap = esp_get_free_heap_size();
    
    // Smooth CPU and FPS values using exponential moving average to prevent jitter
    static float smoothedCpu = 40.0f;
    static float smoothedFps = 60.0f;
    float targetCpu = 35.0f + (rand() % 200) / 10.0f;
    float targetFps = 58.0f + (rand() % 40) / 10.0f;
    smoothedCpu = smoothedCpu * 0.95f + targetCpu * 0.05f;  // Heavy smoothing
    smoothedFps = smoothedFps * 0.95f + targetFps * 0.05f;
    state.cpuUsage = smoothedCpu;
    state.fps = smoothedFps;
    
    // Simulate environmental sensor with slight drift
    simTemp += ((rand() % 20) - 10) / 100.0f;
    if (simTemp < 18.0f) simTemp = 18.0f;
    if (simTemp > 30.0f) simTemp = 30.0f;
    state.temperature = simTemp;
    
    simHumidity += ((rand() % 20) - 10) / 100.0f;
    if (simHumidity < 30.0f) simHumidity = 30.0f;
    if (simHumidity > 70.0f) simHumidity = 70.0f;
    state.humidity = simHumidity;
    
    simPressure += ((rand() % 10) - 5) / 10.0f;
    if (simPressure < 1000.0f) simPressure = 1000.0f;
    if (simPressure > 1030.0f) simPressure = 1030.0f;
    state.pressure = simPressure;
    
    // Update IMU from real driver (values in mg and deg/s) - using modular driver
    state.accelX = Drivers::ImuDriver::accelX;
    state.accelY = Drivers::ImuDriver::accelY;
    state.accelZ = Drivers::ImuDriver::accelZ;
    state.gyroX = Drivers::ImuDriver::gyroX;
    state.gyroY = Drivers::ImuDriver::gyroY;
    state.gyroZ = Drivers::ImuDriver::gyroZ;
    
    // Process IMU calibration (accumulate samples if calibrating)
    SystemAPI::Web::HttpServer::processImuCalibration();
    // Apply IMU calibration to get device-frame values
    SystemAPI::Web::HttpServer::applyImuCalibration();
    
    // Update microphone from real driver (rolling average for stability) - using modular driver
    state.micConnected = Drivers::MicDriver::initialized;
    state.micLevel = Drivers::MicDriver::level;
    state.micDb = Drivers::MicDriver::avgDb;  // Use averaged dB for stable display
    
    // Update GPS from real driver (full data including speed, heading, time) - using modular driver
    state.gpsValid = Drivers::GpsDriver::valid;
    state.satellites = Drivers::GpsDriver::satellites;
    state.latitude = Drivers::GpsDriver::latitude;
    state.longitude = Drivers::GpsDriver::longitude;
    state.altitude = Drivers::GpsDriver::altitude;
    state.gpsSpeed = Drivers::GpsDriver::speed;
    state.gpsHeading = Drivers::GpsDriver::heading;
    state.gpsHdop = Drivers::GpsDriver::hdop;
    state.gpsHour = Drivers::GpsDriver::hour;
    state.gpsMinute = Drivers::GpsDriver::minute;
    state.gpsSecond = Drivers::GpsDriver::second;
    state.gpsDay = Drivers::GpsDriver::day;
    state.gpsMonth = Drivers::GpsDriver::month;
    state.gpsYear = Drivers::GpsDriver::year;
    
    // Update GPU connection status
    state.gpuConnected = GpuDriverState::connected;
    
    // Update GPU stats - Note: These need real values from the GPU
    // The SystemAPI::GpuDriver doesn't have built-in stats like GpuCommands
    // For now, set defaults. Real stats would need a separate implementation.
    state.gpuFps = 60.0f;  // Assume 60fps when connected
    state.gpuFreeHeap = 0;  // Not available from GpuDriver
    state.gpuMinHeap = 0;   // Not available from GpuDriver
    state.gpuLoad = 0;      // Not available from GpuDriver
    state.gpuTotalFrames = 0;  // Not available from GpuDriver
    state.gpuUptime = GpuDriverState::gpuUptimeMs;
    state.gpuHub75Ok = GpuDriverState::connected;  // Assume HUB75 OK if connected
    state.gpuOledOk = GpuDriverState::connected;   // Assume OLED OK if connected
    
    // GPU alerts - not available from GpuDriver, set defaults
    state.gpuAlertsReceived = 0;
    state.gpuDroppedFrames = 0;
    state.gpuBufferOverflows = 0;
    state.gpuBufferWarning = false;
    state.gpuHeapWarning = false;
    
    // =====================================================
    // DISABLED: Application Layer Update (conflicts with GpuDriver)
    // =====================================================
    
    // Calculate pitch and roll from accelerometer for future use
    // accelX, accelY, accelZ are in milli-g
    float ax = state.accelX / 1000.0f;
    float ay = state.accelY / 1000.0f;
    float az = state.accelZ / 1000.0f;
    
    // Calculate pitch and roll angles (simplified) - kept for sensor data logging
    float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / 3.14159f;
    float roll = atan2f(ay, az) * 180.0f / 3.14159f;
    
    // =====================================================
    // Update AnimationHandler with sensor data
    // =====================================================
    auto& animHandler = Modes::getAnimationHandler();
    if (animHandler.isInitialized()) {
        // Update IMU inputs (pitch, roll, yaw, accel, gyro)
        animHandler.updateSensorInputs(
            pitch, roll, 0.0f,  // yaw not available without magnetometer
            ax, ay, az,
            state.gyroX / 1000.0f, state.gyroY / 1000.0f, state.gyroZ / 1000.0f
        );
        
        // Update GPS inputs
        animHandler.updateGpsInputs(
            state.latitude, state.longitude, state.altitude,
            state.gpsSpeed, state.satellites, state.gpsValid
        );
        
        // Update audio inputs
        animHandler.updateAudioInputs(
            state.micLevel / 100.0f,  // Normalize 0-100 to 0-1
            state.micDb / 100.0f,     // Use dB as peak proxy
            0.0f, 0.0f, 0.0f          // FFT bands not available yet
        );
        
        // Update environment inputs
        animHandler.updateEnvironmentInputs(
            state.temperature, state.humidity, state.pressure
        );
        
        // Update animation system (runs active animation set)
        animHandler.update(deltaMs);
        
        // Render if animation is enabled and GPU connected
        if (animHandler.isAnimationEnabled() && GpuDriverState::connected) {
            animHandler.render();
        }
    }
    
    // =====================================================
    // Update Sandbox Sensors (render happens in GpuDriverState::update)
    // =====================================================
    if (GpuDriverState::isSandboxEnabled()) {
        // Use device gyro (calibrated) X, Y, Z directly
        GpuDriverState::updateSandboxSensors(state.deviceGyroX, state.deviceGyroY, state.deviceGyroZ);
    }
    
    /*
    // DISABLED: Application layer
    // Update eye controller with IMU data for look tracking
    auto& eye = Application::eye();
    
    // Update eye controller from IMU
    eye.updateFromIMU(pitch, roll);
    
    // Update eye controller from audio (for reactive effects)
    eye.updateFromAudio(state.micDb);
    
    // Update application layer (publishes to Core 1)
    Application::update(deltaMs);
    
    // Also publish sensor data to the application buffer
    Application::SensorData sensorData;
    sensorData.accelX = ax;
    sensorData.accelY = ay;
    sensorData.accelZ = az;
    sensorData.gyroX = state.gyroX;
    sensorData.gyroY = state.gyroY;
    sensorData.gyroZ = state.gyroZ;
    sensorData.pitch = pitch;
    sensorData.roll = roll;
    sensorData.temperature = state.temperature;
    sensorData.humidity = state.humidity;
    sensorData.pressure = state.pressure;
    sensorData.latitude = state.latitude;
    sensorData.longitude = state.longitude;
    sensorData.altitude = state.altitude;
    sensorData.speed = state.gpsSpeed;
    sensorData.satellites = state.satellites;
    sensorData.gpsValid = state.gpsValid;
    sensorData.audioLevel = state.micDb;
    sensorData.audioLevelPercent = state.micLevel;
    sensorData.timestampMs = currentTimeMs;
    Application::publishSensorData(sensorData);
    */
    
    // Print credentials every 10 seconds
    if (m_credentialPrintTime >= 10000) {
        auto& security = arcos::security::SecurityDriver::instance();
        printf("  ----------------------------------------\n");
        printf("  WiFi SSID: %s\n", security.getSSID());
        printf("  WiFi Pass: %s\n", security.getPassword());
        printf("  Portal: 192.168.4.1 or type any URL\n");
        printf("  GPS: %s (Sats: %d, RX: %lu bytes)\n", 
               Drivers::GpsDriver::valid ? "Fix" : "Searching", 
               Drivers::GpsDriver::satellites,
               (unsigned long)Drivers::GpsDriver::bytesReceived);
        printf("  GPU: %s\n", GpuDriverState::connected ? "Connected" : "N/C");
        printf("  MIC: %.1f dB (avg)\n", Drivers::MicDriver::avgDb);
        
        // Print sprite summary once (first time after boot) so it's visible in serial
        static bool spriteSummaryPrinted = false;
        if (!spriteSummaryPrinted) {
            spriteSummaryPrinted = true;
            auto& httpServer = SystemAPI::Web::HttpServer::instance();
            const auto& sprites = httpServer.getSprites();
            printf("  ---- SPRITES ----\n");
            printf("  Total: %zu (Built-in: ", sprites.size());
            int builtIn = 0, storage = 0;
            for (const auto& sp : sprites) {
                if (sp.id < 100) builtIn++; else storage++;
            }
            printf("%d, From SD: %d)\n", builtIn, storage);
            if (storage > 0) {
                printf("  Saved sprites from storage:\n");
                for (const auto& sp : sprites) {
                    if (sp.id >= 100) {
                        printf("    [%d] %s (%dx%d)\n", sp.id, sp.name.c_str(), sp.width, sp.height);
                    }
                }
            }
        }
        printf("  ----------------------------------------\n");
        m_credentialPrintTime = 0;
    }
    
    // Example: Print status every 5 seconds
    if (m_totalTime >= 5000) {
        printf("  Update #%lu | Clients: %d\n",
               (unsigned long)m_updateCount,
               portal.getClientCount());
        m_totalTime = 0;
    }
}

void CurrentMode::onStop() {
    printf("  Current mode stopped after %lu updates\\n", (unsigned long)m_updateCount);
    
    // Stop GPU driver keep-alive
    g_gpu.stopKeepAlive();
    g_gpu.shutdown();
    printf("  GpuDriver shutdown complete\\n");
    
    /*
    // DISABLED: Application layer
    Application::stop();
    Application::shutdown();
    printf("  Application layer shutdown complete\\n");
    */
}

} // namespace Modes
