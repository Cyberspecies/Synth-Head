/*****************************************************************
 * CPU_SpriteDemo.cpp - Sprite caching, movement, and rotation demo
 * 
 * Demonstrates:
 * - Uploading sprites to GPU (cached in GPU memory)
 * - Moving sprites around the display
 * - Simulated rotation by pre-computing rotated sprite versions
 *****************************************************************/

#include <cstdio>
#include <cstring>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "SPRITE_DEMO";

// ============== UART Config ==============
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int UART_TX_PIN = 12;
constexpr int UART_RX_PIN = 11;
constexpr int UART_BAUD = 10000000;

// ============== Protocol ==============
constexpr uint8_t SYNC0 = 0xAA;
constexpr uint8_t SYNC1 = 0x55;

enum CmdType : uint8_t {
    NOP           = 0x00,
    UPLOAD_SPRITE = 0x20,
    DELETE_SPRITE = 0x21,
    DRAW_PIXEL    = 0x40,
    BLIT_SPRITE   = 0x46,
    CLEAR         = 0x47,
    SET_TARGET    = 0x50,
    PRESENT       = 0x51,
    OLED_CLEAR    = 0x60,
    OLED_PRESENT  = 0x65,
    PING          = 0xF0,
};

// ============== Sprite Definitions ==============
// 8x8 arrow sprite pointing RIGHT (we'll rotate it for other directions)
// Format: RGB888 (3 bytes per pixel)
constexpr int SPRITE_SIZE = 8;
constexpr int SPRITE_BYTES = SPRITE_SIZE * SPRITE_SIZE * 3; // 192 bytes

// Arrow shape (8x8) - pointing right
static const uint8_t ARROW_SHAPE[8][8] = {
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
static const uint8_t SMILEY_SHAPE[8][8] = {
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
static const uint8_t HEART_SHAPE[8][8] = {
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
static const uint8_t STAR_SHAPE[8][8] = {
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {1,1,1,1,1,1,1,1},
    {0,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {1,1,0,0,0,0,1,1},
    {1,0,0,0,0,0,0,1},
};

// ============== GPU Communication ==============
static void sendCmd(CmdType type, const uint8_t* payload, uint16_t len) {
    uint8_t header[5] = {SYNC0, SYNC1, (uint8_t)type, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
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

// ============== Sprite Functions ==============

// Convert 1-bit shape to RGB888 sprite data
static void shapeToRgb888(const uint8_t shape[8][8], uint8_t* outData, uint8_t r, uint8_t g, uint8_t b) {
    int idx = 0;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (shape[y][x]) {
                outData[idx++] = r;
                outData[idx++] = g;
                outData[idx++] = b;
            } else {
                outData[idx++] = 0;  // Transparent = black
                outData[idx++] = 0;
                outData[idx++] = 0;
            }
        }
    }
}

// Rotate a shape 90 degrees clockwise
static void rotateShape90(const uint8_t src[8][8], uint8_t dst[8][8]) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            dst[x][7-y] = src[y][x];
        }
    }
}

// Upload an 8x8 RGB888 sprite to GPU
static void uploadSprite(uint8_t spriteId, const uint8_t* rgbData) {
    // Header: spriteId, width, height, format (0=RGB888)
    uint8_t payload[4 + SPRITE_BYTES];
    payload[0] = spriteId;
    payload[1] = 8;  // width
    payload[2] = 8;  // height
    payload[3] = 0;  // RGB888 format
    memcpy(payload + 4, rgbData, SPRITE_BYTES);
    
    sendCmd(UPLOAD_SPRITE, payload, sizeof(payload));
    ESP_LOGI(TAG, "Uploaded sprite %d (%d bytes)", spriteId, (int)sizeof(payload));
}

// Blit sprite at position
static void blitSprite(uint8_t spriteId, int16_t x, int16_t y) {
    uint8_t payload[5] = {
        spriteId,
        (uint8_t)(x & 0xFF), (uint8_t)(x >> 8),
        (uint8_t)(y & 0xFF), (uint8_t)(y >> 8)
    };
    sendCmd(BLIT_SPRITE, payload, 5);
}

// ============== Animation State ==============
struct SpriteInstance {
    uint8_t baseSpriteId;  // Base sprite ID (for rotation, we use baseSpriteId + rotationStep)
    float x, y;
    float vx, vy;           // Velocity
    float angle;            // Current rotation angle (degrees)
    float rotationSpeed;    // Degrees per frame
    bool hasRotation;       // Whether this sprite has rotation variants
};

static SpriteInstance sprites[4];

// ============== Main ==============
extern "C" void app_main() {
    ESP_LOGI(TAG, "=== SPRITE DEMO STARTING ===");
    
    // Initialize UART
    uart_config_t uartCfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    uart_driver_install(UART_PORT, 1024, 1024, 0, nullptr, 0);
    uart_param_config(UART_PORT, &uartCfg);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, -1, -1);
    
    ESP_LOGI(TAG, "UART initialized at %d baud", UART_BAUD);
    vTaskDelay(pdMS_TO_TICKS(500));  // Let GPU boot
    
    // ============== Upload all sprites to GPU (CACHED) ==============
    ESP_LOGI(TAG, "Uploading sprites to GPU cache...");
    
    uint8_t spriteData[SPRITE_BYTES];
    
    // Sprite 0-3: Arrow in 4 rotations (Right, Down, Left, Up)
    uint8_t rotated[8][8];
    uint8_t currentShape[8][8];
    memcpy(currentShape, ARROW_SHAPE, sizeof(ARROW_SHAPE));
    
    // Rotation 0: Right (original)
    shapeToRgb888(currentShape, spriteData, 0, 255, 0);  // Green arrow
    uploadSprite(0, spriteData);
    
    // Rotation 1: Down (90° CW)
    rotateShape90(currentShape, rotated);
    memcpy(currentShape, rotated, sizeof(rotated));
    shapeToRgb888(currentShape, spriteData, 0, 255, 0);
    uploadSprite(1, spriteData);
    
    // Rotation 2: Left (180°)
    rotateShape90(currentShape, rotated);
    memcpy(currentShape, rotated, sizeof(rotated));
    shapeToRgb888(currentShape, spriteData, 0, 255, 0);
    uploadSprite(2, spriteData);
    
    // Rotation 3: Up (270°)
    rotateShape90(currentShape, rotated);
    memcpy(currentShape, rotated, sizeof(rotated));
    shapeToRgb888(currentShape, spriteData, 0, 255, 0);
    uploadSprite(3, spriteData);
    
    // Sprite 4: Smiley (yellow)
    shapeToRgb888(SMILEY_SHAPE, spriteData, 255, 255, 0);
    uploadSprite(4, spriteData);
    
    // Sprite 5: Heart (red)
    shapeToRgb888(HEART_SHAPE, spriteData, 255, 0, 80);
    uploadSprite(5, spriteData);
    
    // Sprite 6: Star (white/yellow)
    shapeToRgb888(STAR_SHAPE, spriteData, 255, 255, 200);
    uploadSprite(6, spriteData);
    
    ESP_LOGI(TAG, "All sprites uploaded and cached on GPU!");
    
    // ============== Initialize sprite instances ==============
    // Sprite 0: Rotating arrow bouncing around
    sprites[0] = {
        .baseSpriteId = 0,
        .x = 20.0f, .y = 12.0f,
        .vx = 0.8f, .vy = 0.5f,
        .angle = 0.0f,
        .rotationSpeed = 3.0f,
        .hasRotation = true
    };
    
    // Sprite 1: Smiley moving horizontally
    sprites[1] = {
        .baseSpriteId = 4,
        .x = 60.0f, .y = 12.0f,
        .vx = 0.6f, .vy = 0.0f,
        .angle = 0.0f,
        .rotationSpeed = 0.0f,
        .hasRotation = false
    };
    
    // Sprite 2: Heart bouncing
    sprites[2] = {
        .baseSpriteId = 5,
        .x = 100.0f, .y = 20.0f,
        .vx = -0.4f, .vy = 0.7f,
        .angle = 0.0f,
        .rotationSpeed = 0.0f,
        .hasRotation = false
    };
    
    // Sprite 3: Star orbiting center
    sprites[3] = {
        .baseSpriteId = 6,
        .x = 64.0f, .y = 16.0f,
        .vx = 0.0f, .vy = 0.0f,  // Orbital motion calculated separately
        .angle = 0.0f,
        .rotationSpeed = 2.0f,
        .hasRotation = false
    };
    
    // ============== Animation Loop ==============
    ESP_LOGI(TAG, "Starting animation loop at 60fps...");
    
    float orbitAngle = 0.0f;
    const float orbitRadius = 20.0f;
    const float orbitCenterX = 64.0f;
    const float orbitCenterY = 16.0f;
    
    uint32_t frameCount = 0;
    int64_t startTime = esp_timer_get_time();
    
    while (true) {
        // === Update sprite positions ===
        
        // Bouncing sprites (0, 1, 2)
        for (int i = 0; i < 3; i++) {
            sprites[i].x += sprites[i].vx;
            sprites[i].y += sprites[i].vy;
            
            // Bounce off walls
            if (sprites[i].x < 0) { sprites[i].x = 0; sprites[i].vx = -sprites[i].vx; }
            if (sprites[i].x > 120) { sprites[i].x = 120; sprites[i].vx = -sprites[i].vx; }
            if (sprites[i].y < 0) { sprites[i].y = 0; sprites[i].vy = -sprites[i].vy; }
            if (sprites[i].y > 24) { sprites[i].y = 24; sprites[i].vy = -sprites[i].vy; }
            
            // Update rotation angle
            sprites[i].angle += sprites[i].rotationSpeed;
            if (sprites[i].angle >= 360.0f) sprites[i].angle -= 360.0f;
        }
        
        // Orbiting star (sprite 3)
        orbitAngle += 1.5f;
        if (orbitAngle >= 360.0f) orbitAngle -= 360.0f;
        sprites[3].x = orbitCenterX + cosf(orbitAngle * M_PI / 180.0f) * orbitRadius;
        sprites[3].y = orbitCenterY + sinf(orbitAngle * M_PI / 180.0f) * (orbitRadius * 0.5f);  // Ellipse
        
        // === Render frame ===
        setTarget(0);  // HUB75
        clear(0, 0, 20);  // Dark blue background
        
        // Draw all sprites
        for (int i = 0; i < 4; i++) {
            uint8_t spriteId = sprites[i].baseSpriteId;
            
            // If sprite has rotation variants, select based on angle
            if (sprites[i].hasRotation) {
                // 4 rotation steps: 0=Right(0-89°), 1=Down(90-179°), 2=Left(180-269°), 3=Up(270-359°)
                int rotStep = ((int)(sprites[i].angle / 90.0f)) % 4;
                spriteId = sprites[i].baseSpriteId + rotStep;
            }
            
            blitSprite(spriteId, (int16_t)sprites[i].x, (int16_t)sprites[i].y);
        }
        
        present();
        
        // Clear OLED (keep it dark)
        oledClear();
        oledPresent();
        
        // Stats every 5 seconds
        frameCount++;
        if (frameCount % 300 == 0) {
            int64_t elapsed = esp_timer_get_time() - startTime;
            float fps = (float)frameCount / ((float)elapsed / 1000000.0f);
            ESP_LOGI(TAG, "Frame %lu, FPS: %.1f", frameCount, fps);
        }
        
        vTaskDelay(pdMS_TO_TICKS(16));  // ~60 FPS
    }
}
