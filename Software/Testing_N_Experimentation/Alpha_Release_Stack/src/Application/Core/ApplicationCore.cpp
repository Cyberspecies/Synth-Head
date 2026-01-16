/*****************************************************************
 * File:      ApplicationCore.cpp
 * Category:  Application/Core
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Implementation of the dual-core application core.
 *    Contains the GPU task that runs on Core 1.
 *    
 *    Render priority:
 *    1. SceneRenderer (manual scenes from web UI)
 *    2. SpriteManager (animated sprite scenes)
 *    3. AnimationPipeline (automatic eye animations)
 *****************************************************************/

#include "ApplicationCore.hpp"
#include "SyncBuffer.hpp"
#include "../Pipeline/GpuPipeline.hpp"
#include "../Pipeline/AnimationPipeline.hpp"
#include "../Pipeline/SceneRenderer.hpp"
#include "../Pipeline/SpriteSystem.hpp"

namespace Application {

// Forward declaration of global animation buffer accessor
// (defined in Application.hpp, but we can't include it here due to circular deps)
AnimationBuffer& getAnimationBuffer();

// ============================================================
// Global Animation Buffer Instance (defined here for linker)
// ============================================================

static AnimationBuffer s_animBuffer;

AnimationBuffer& getAnimationBuffer() {
  return s_animBuffer;
}

// ============================================================
// Global SceneRenderer and GpuProtocol Instances
// ============================================================

static GpuProtocol s_gpuProtocol;
static SceneRenderer s_sceneRenderer;
static SpriteGpuProtocol s_spriteGpuProtocol;

GpuProtocol& getGpuProtocol() {
  return s_gpuProtocol;
}

SceneRenderer& getSceneRenderer() {
  return s_sceneRenderer;
}

SpriteGpuProtocol& getSpriteGpuProtocol() {
  return s_spriteGpuProtocol;
}

// ============================================================
// GPU Pipeline Instances (Core 1 only)
// ============================================================

static GpuPipeline g_gpuPipeline;
static AnimationPipeline g_animPipeline;

// ============================================================
// GPU Task Implementation
// ============================================================

void ApplicationCore::gpuTask() {
  ESP_LOGI(TAG, ">>> GPU task ENTRY on Core %d <<<", xPortGetCoreID());
  printf("GPU TASK STARTED ON CORE %d\n", xPortGetCoreID());
  
  // Initialize GPU pipeline FIRST (installs UART driver)
  GpuPipeline::Config gpuConfig;
  gpuConfig.uartPort = UART_NUM_1;
  gpuConfig.txPin = 12;
  gpuConfig.rxPin = 11;
  gpuConfig.baudRate = 10000000;  // 10 Mbps
  gpuConfig.targetFps = 60;
  gpuConfig.mirrorMode = true;
  
  if (!g_gpuPipeline.init(gpuConfig)) {
    ESP_LOGE(TAG, "Failed to initialize GPU pipeline!");
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(TAG, "GPU pipeline initialized (UART driver installed)");
  
  // Initialize GPU protocol for SceneRenderer (uses existing UART driver)
  auto& gpuProto = getGpuProtocol();
  if (!gpuProto.init(UART_NUM_1)) {
    ESP_LOGE(TAG, "Failed to initialize GPU protocol!");
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(TAG, "GPU protocol initialized");
  
  // Initialize Sprite GPU Protocol (uses existing UART driver)
  auto& spriteGpu = getSpriteGpuProtocol();
  if (!spriteGpu.init(UART_NUM_1)) {
    ESP_LOGE(TAG, "Failed to initialize Sprite GPU protocol!");
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(TAG, "Sprite GPU protocol initialized");
  
  // Initialize Sprite Manager
  auto& spriteManager = getSpriteManager();
  if (!spriteManager.init(&spriteGpu)) {
    ESP_LOGE(TAG, "Failed to initialize Sprite Manager!");
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(TAG, "Sprite Manager initialized");
  
  // Pre-load some built-in sprites for testing
  // These will be cached on the GPU
  preloadBuiltinSprites(spriteManager);
  
  // Initialize Scene Renderer
  auto& sceneRenderer = getSceneRenderer();
  sceneRenderer.init();
  sceneRenderer.setGpuProtocol(&gpuProto);
  ESP_LOGI(TAG, "Scene renderer initialized");
  
  // Initialize animation pipeline
  if (!g_animPipeline.init(&g_gpuPipeline, &getAnimationBuffer())) {
    ESP_LOGE(TAG, "Failed to initialize animation pipeline!");
    vTaskDelete(nullptr);
    return;
  }
  
  // Mark GPU as ready
  if (lockState()) {
    state_.gpuReady = true;
    unlockState();
  }
  
  ESP_LOGI(TAG, "GPU pipeline ready");
  
  // Frame timing
  const uint32_t targetFrameTimeUs = 1000000 / 60;  // 60 FPS
  uint64_t lastFrameTime = esp_timer_get_time();
  uint64_t frameTimeAccum = 0;
  uint32_t frameCount = 0;
  uint32_t fpsUpdateCounter = 0;
  
  // Main render loop - runs at 60fps
  while (running_) {
    uint64_t frameStart = esp_timer_get_time();
    
    // Calculate delta time
    uint64_t elapsed = frameStart - lastFrameTime;
    float deltaTime = elapsed / 1000000.0f;  // Convert to seconds
    lastFrameTime = frameStart;
    
    // Clamp delta time to prevent huge jumps
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    if (deltaTime < 0.001f) deltaTime = 0.001f;
    
    // =========================================================
    // RENDER PRIORITY:
    // 1. SceneRenderer handles manual scenes (web UI controlled)
    // 2. AnimationPipeline handles automatic eye animations
    // =========================================================
    
    // Try SceneRenderer first
    bool sceneRendered = sceneRenderer.renderFrame();
    
    // If no manual scene, use AnimationPipeline
    if (!sceneRendered) {
      g_animPipeline.update(deltaTime);
    }
    
    // Frame timing and stats
    uint64_t frameEnd = esp_timer_get_time();
    uint64_t frameTime = frameEnd - frameStart;
    frameTimeAccum += frameTime;
    frameCount++;
    fpsUpdateCounter++;
    
    // Update stats every 60 frames (~1 second)
    if (fpsUpdateCounter >= 60) {
      gpuStats_.framesRendered = g_animPipeline.getFrameCount();
      gpuStats_.avgFrameTimeUs = frameTimeAccum / frameCount;
      gpuStats_.currentFps = 1000000.0f / gpuStats_.avgFrameTimeUs;
      
      if (frameTime > gpuStats_.maxFrameTimeUs) {
        gpuStats_.maxFrameTimeUs = frameTime;
      }
      
      // Check stack usage
      gpuStats_.freeStackWords = uxTaskGetStackHighWaterMark(nullptr);
      
      frameTimeAccum = 0;
      frameCount = 0;
      fpsUpdateCounter = 0;
    }
    
    // Frame rate limiting
    if (frameTime < targetFrameTimeUs) {
      uint32_t sleepTime = targetFrameTimeUs - frameTime;
      vTaskDelay(pdMS_TO_TICKS(sleepTime / 1000));
    } else {
      // Frame took too long, yield to prevent watchdog
      vTaskDelay(1);
      gpuStats_.droppedFrames++;
    }
  }
  
  ESP_LOGI(TAG, "GPU task stopping");
  vTaskDelete(nullptr);
}

// ============================================================
// Pre-load Built-in Sprites
// ============================================================

void preloadBuiltinSprites(SpriteManager& mgr) {
  static constexpr const char* TAG = "SpritePreload";
  ESP_LOGI(TAG, "Pre-loading built-in sprites...");
  
  // Arrow pointing right (8x8) - and its 4 rotations
  static const uint8_t arrowRight[8][8] = {
    {0,0,0,1,0,0,0,0},
    {0,0,0,1,1,0,0,0},
    {1,1,1,1,1,1,0,0},
    {1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,0,0,0,0},
  };
  
  // Smiley face (8x8)
  static const uint8_t smiley[8][8] = {
    {0,0,1,1,1,1,0,0},
    {0,1,0,0,0,0,1,0},
    {1,0,1,0,0,1,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,1,0,0,1,0,1},
    {1,0,0,1,1,0,0,1},
    {0,1,0,0,0,0,1,0},
    {0,0,1,1,1,1,0,0},
  };
  
  // Heart shape (8x8)
  static const uint8_t heart[8][8] = {
    {0,1,1,0,0,1,1,0},
    {1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1},
    {0,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,0,0,0,0,0},
  };
  
  // Star shape (8x8)
  static const uint8_t star[8][8] = {
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {1,1,1,1,1,1,1,1},
    {0,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {1,1,0,0,0,0,1,1},
    {1,0,0,0,0,0,0,1},
  };
  
  // Create arrow rotations for smooth rotation animation
  // Sprite 0: Arrow Right (green)
  mgr.createFromShape(arrowRight, 8, 8, 0, 255, 0, "arrow_right");
  
  // Sprite 1: Arrow Down (rotate 90°)
  uint8_t arrowDown[8][8];
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      arrowDown[x][7-y] = arrowRight[y][x];
    }
  }
  mgr.createFromShape(arrowDown, 8, 8, 0, 255, 0, "arrow_down");
  
  // Sprite 2: Arrow Left (rotate 180°)
  uint8_t arrowLeft[8][8];
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      arrowLeft[x][7-y] = arrowDown[y][x];
    }
  }
  mgr.createFromShape(arrowLeft, 8, 8, 0, 255, 0, "arrow_left");
  
  // Sprite 3: Arrow Up (rotate 270°)
  uint8_t arrowUp[8][8];
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      arrowUp[x][7-y] = arrowLeft[y][x];
    }
  }
  mgr.createFromShape(arrowUp, 8, 8, 0, 255, 0, "arrow_up");
  
  // Sprite 4: Smiley (yellow)
  mgr.createFromShape(smiley, 8, 8, 255, 255, 0, "smiley");
  
  // Sprite 5: Heart (red/pink)
  mgr.createFromShape(heart, 8, 8, 255, 0, 80, "heart");
  
  // Sprite 6: Star (white/yellow)
  mgr.createFromShape(star, 8, 8, 255, 255, 200, "star");
  
  // Sprite 7-10: Solid color blocks for testing
  mgr.createSolidSprite(8, 8, 255, 0, 0, "red_block");
  mgr.createSolidSprite(8, 8, 0, 255, 0, "green_block");
  mgr.createSolidSprite(8, 8, 0, 0, 255, "blue_block");
  mgr.createSolidSprite(8, 8, 255, 255, 255, "white_block");
  
  ESP_LOGI(TAG, "Pre-loaded %d built-in sprites", mgr.getSpriteCount());
}

} // namespace Application
