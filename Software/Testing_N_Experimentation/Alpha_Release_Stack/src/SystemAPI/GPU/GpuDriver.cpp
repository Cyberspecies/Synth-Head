/*****************************************************************
 * GpuDriver.cpp - Implementation of CPU-side GPU Command Driver
 * 
 * Based on working code from CPU_SpriteDemo and CPU_PolygonDemo.
 * 
 * Weighted Pixel Rendering (Anti-Aliasing):
 * - Uses 8.8 fixed-point coordinates for sub-pixel precision
 * - GPU calculates pixel coverage to control opacity
 * - Reduces aliasing artifacts on lines and moving sprites
 * - Creates smoother animation with less stuttering
 *****************************************************************/

#include "GpuDriver.h"
#include <cmath>

namespace SystemAPI {

constexpr char GpuDriver::TAG[];

// ============== Destructor ==============
GpuDriver::~GpuDriver() {
    shutdown();
}

// ============== Initialization ==============
bool GpuDriver::init(const GpuConfig& config) {
    if (m_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    m_config = config;
    
    // Create mutex for thread safety
    m_mutex = xSemaphoreCreateMutex();
    if (!m_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }
    
    // Configure UART
    uart_config_t uartConfig = {
        .baud_rate = m_config.baudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Install UART driver FIRST (like in working SpriteDemo)
    esp_err_t err = uart_driver_install(m_config.uartPort, m_config.rxBufferSize, 
                                        m_config.txBufferSize, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
        return false;
    }
    
    // Then configure parameters
    err = uart_param_config(m_config.uartPort, &uartConfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        uart_driver_delete(m_config.uartPort);
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
        return false;
    }
    
    // Set pins
    err = uart_set_pin(m_config.uartPort, m_config.txPin, m_config.rxPin, -1, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        uart_driver_delete(m_config.uartPort);
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
        return false;
    }
    
    ESP_LOGI(TAG, "UART%d initialized: TX=GPIO%d, RX=GPIO%d, BAUD=%d",
             m_config.uartPort, m_config.txPin, m_config.rxPin, m_config.baudRate);
    
    // CRITICAL: Wait for GPU to boot up before sending commands
    ESP_LOGI(TAG, "Waiting %dms for GPU to boot...", m_config.gpuBootDelayMs);
    vTaskDelay(pdMS_TO_TICKS(m_config.gpuBootDelayMs));
    
    // Initialize weighted pixel mode from config
    m_weightedPixels = m_config.weightedPixels;
    
    m_initialized = true;
    ESP_LOGI(TAG, "GPU Driver initialized (weighted pixels: %s)", 
             m_weightedPixels ? "ON" : "OFF");
    
    return true;
}

void GpuDriver::shutdown() {
    if (!m_initialized) return;
    
    stopKeepAlive();
    
    uart_driver_delete(m_config.uartPort);
    
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    
    m_initialized = false;
    ESP_LOGI(TAG, "GPU Driver shutdown");
}

// ============== Low-Level Command Sending ==============
void GpuDriver::sendCommand(GpuCommand cmd, const uint8_t* payload, uint16_t length) {
    if (!m_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    MutexLock lock(m_mutex);
    
    // Build header: SYNC0, SYNC1, CMD, LEN_LO, LEN_HI
    uint8_t header[5] = {
        SYNC_BYTE_0,
        SYNC_BYTE_1,
        static_cast<uint8_t>(cmd),
        static_cast<uint8_t>(length & 0xFF),
        static_cast<uint8_t>((length >> 8) & 0xFF)
    };
    
    // Send header
    uart_write_bytes(m_config.uartPort, header, 5);
    
    // Send payload if any
    if (length > 0 && payload != nullptr) {
        uart_write_bytes(m_config.uartPort, payload, length);
    }
    
    // Wait for transmission to complete
    uart_wait_tx_done(m_config.uartPort, pdMS_TO_TICKS(50));
}

int GpuDriver::readResponse(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs) {
    if (!m_initialized) return -1;
    
    MutexLock lock(m_mutex);
    return uart_read_bytes(m_config.uartPort, buffer, maxLen, pdMS_TO_TICKS(timeoutMs));
}

// ============== Target Control ==============
void GpuDriver::setTarget(GpuTarget target) {
    uint8_t t = static_cast<uint8_t>(target);
    sendCommand(GpuCommand::SET_TARGET, &t, 1);
}

void GpuDriver::present() {
    sendCommand(GpuCommand::PRESENT, nullptr, 0);
}

// ============== Screen Operations ==============
void GpuDriver::clear(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t payload[3] = {r, g, b};
    sendCommand(GpuCommand::CLEAR, payload, 3);
}

// ============== Drawing Primitives ==============
void GpuDriver::drawPixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t payload[7] = {
        static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>((x >> 8) & 0xFF),
        static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>((y >> 8) & 0xFF),
        r, g, b
    };
    sendCommand(GpuCommand::DRAW_PIXEL, payload, 7);
}

void GpuDriver::drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b) {
    if (m_weightedPixels) {
        // Use float version for anti-aliased rendering
        drawLineF(static_cast<float>(x1), static_cast<float>(y1),
                  static_cast<float>(x2), static_cast<float>(y2), r, g, b);
        return;
    }
    
    uint8_t payload[11] = {
        static_cast<uint8_t>(x1 & 0xFF), static_cast<uint8_t>((x1 >> 8) & 0xFF),
        static_cast<uint8_t>(y1 & 0xFF), static_cast<uint8_t>((y1 >> 8) & 0xFF),
        static_cast<uint8_t>(x2 & 0xFF), static_cast<uint8_t>((x2 >> 8) & 0xFF),
        static_cast<uint8_t>(y2 & 0xFF), static_cast<uint8_t>((y2 >> 8) & 0xFF),
        r, g, b
    };
    sendCommand(GpuCommand::DRAW_LINE, payload, 11);
}

void GpuDriver::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
    if (m_weightedPixels) {
        // Use float version for anti-aliased rendering
        drawRectF(static_cast<float>(x), static_cast<float>(y),
                  static_cast<float>(w), static_cast<float>(h), r, g, b);
        return;
    }
    
    uint8_t payload[11] = {
        static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>((x >> 8) & 0xFF),
        static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>((y >> 8) & 0xFF),
        static_cast<uint8_t>(w & 0xFF), static_cast<uint8_t>((w >> 8) & 0xFF),
        static_cast<uint8_t>(h & 0xFF), static_cast<uint8_t>((h >> 8) & 0xFF),
        r, g, b
    };
    sendCommand(GpuCommand::DRAW_RECT, payload, 11);
}

void GpuDriver::drawFilledRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t payload[11] = {
        static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>((x >> 8) & 0xFF),
        static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>((y >> 8) & 0xFF),
        static_cast<uint8_t>(w & 0xFF), static_cast<uint8_t>((w >> 8) & 0xFF),
        static_cast<uint8_t>(h & 0xFF), static_cast<uint8_t>((h >> 8) & 0xFF),
        r, g, b
    };
    sendCommand(GpuCommand::DRAW_FILL, payload, 11);
}

void GpuDriver::drawCircle(int16_t cx, int16_t cy, int16_t radius, uint8_t r, uint8_t g, uint8_t b) {
    if (m_weightedPixels) {
        // Use float version for anti-aliased rendering
        drawCircleF(static_cast<float>(cx), static_cast<float>(cy),
                    static_cast<float>(radius), r, g, b);
        return;
    }
    
    uint8_t payload[9] = {
        static_cast<uint8_t>(cx & 0xFF), static_cast<uint8_t>((cx >> 8) & 0xFF),
        static_cast<uint8_t>(cy & 0xFF), static_cast<uint8_t>((cy >> 8) & 0xFF),
        static_cast<uint8_t>(radius & 0xFF), static_cast<uint8_t>((radius >> 8) & 0xFF),
        r, g, b
    };
    sendCommand(GpuCommand::DRAW_CIRCLE, payload, 9);
}

// ============== Polygon Drawing ==============
void GpuDriver::drawFilledPolygon(const int16_t* xPoints, const int16_t* yPoints, uint8_t numVertices,
                                   uint8_t r, uint8_t g, uint8_t b) {
    if (numVertices > 16) numVertices = 16;  // Max 16 vertices
    
    // Payload: numVerts, R, G, B, then X,Y pairs
    size_t payloadSize = 4 + (numVertices * 4);  // 4 bytes header + 4 bytes per vertex
    uint8_t payload[68];  // Max: 4 + 16*4 = 68 bytes
    
    payload[0] = numVertices;
    payload[1] = r;
    payload[2] = g;
    payload[3] = b;
    
    for (int i = 0; i < numVertices; i++) {
        int offset = 4 + (i * 4);
        payload[offset + 0] = static_cast<uint8_t>(xPoints[i] & 0xFF);
        payload[offset + 1] = static_cast<uint8_t>((xPoints[i] >> 8) & 0xFF);
        payload[offset + 2] = static_cast<uint8_t>(yPoints[i] & 0xFF);
        payload[offset + 3] = static_cast<uint8_t>((yPoints[i] >> 8) & 0xFF);
    }
    
    sendCommand(GpuCommand::DRAW_POLY, payload, payloadSize);
}

// ============== Sprite Operations ==============
bool GpuDriver::uploadSprite(uint8_t spriteId, uint8_t width, uint8_t height,
                              const uint8_t* pixelData, SpriteFormat format) {
    if (spriteId > 63) {
        ESP_LOGE(TAG, "Sprite ID must be 0-63");
        return false;
    }
    
    // Calculate data size
    size_t dataSize;
    if (format == SpriteFormat::RGB888) {
        dataSize = width * height * 3;
    } else {
        dataSize = ((width + 7) / 8) * height;  // 1bpp packed
    }
    
    // Build payload: spriteId, width, height, format, data
    size_t payloadSize = 4 + dataSize;
    uint8_t* payload = new uint8_t[payloadSize];
    if (!payload) {
        ESP_LOGE(TAG, "Failed to allocate sprite payload");
        return false;
    }
    
    payload[0] = spriteId;
    payload[1] = width;
    payload[2] = height;
    payload[3] = static_cast<uint8_t>(format);
    memcpy(payload + 4, pixelData, dataSize);
    
    sendCommand(GpuCommand::UPLOAD_SPRITE, payload, payloadSize);
    
    delete[] payload;
    
    ESP_LOGI(TAG, "Uploaded sprite %d: %dx%d, %zu bytes", spriteId, width, height, dataSize);
    return true;
}

void GpuDriver::deleteSprite(uint8_t spriteId) {
    sendCommand(GpuCommand::DELETE_SPRITE, &spriteId, 1);
}

void GpuDriver::blitSprite(uint8_t spriteId, int16_t x, int16_t y) {
    uint8_t payload[5] = {
        spriteId,
        static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>((x >> 8) & 0xFF),
        static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>((y >> 8) & 0xFF)
    };
    sendCommand(GpuCommand::BLIT_SPRITE, payload, 5);
}

// ============== OLED Specific ==============
void GpuDriver::oledClear() {
    sendCommand(GpuCommand::OLED_CLEAR, nullptr, 0);
}

void GpuDriver::oledPresent() {
    sendCommand(GpuCommand::OLED_PRESENT, nullptr, 0);
}

void GpuDriver::oledDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool on) {
    uint8_t payload[9] = {
        static_cast<uint8_t>(x1 & 0xFF), static_cast<uint8_t>((x1 >> 8) & 0xFF),
        static_cast<uint8_t>(y1 & 0xFF), static_cast<uint8_t>((y1 >> 8) & 0xFF),
        static_cast<uint8_t>(x2 & 0xFF), static_cast<uint8_t>((x2 >> 8) & 0xFF),
        static_cast<uint8_t>(y2 & 0xFF), static_cast<uint8_t>((y2 >> 8) & 0xFF),
        static_cast<uint8_t>(on ? 1 : 0)
    };
    sendCommand(GpuCommand::OLED_LINE, payload, 9);
}

void GpuDriver::oledDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
    uint8_t payload[9] = {
        static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>((x >> 8) & 0xFF),
        static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>((y >> 8) & 0xFF),
        static_cast<uint8_t>(w & 0xFF), static_cast<uint8_t>((w >> 8) & 0xFF),
        static_cast<uint8_t>(h & 0xFF), static_cast<uint8_t>((h >> 8) & 0xFF),
        static_cast<uint8_t>(on ? 1 : 0)
    };
    sendCommand(GpuCommand::OLED_RECT, payload, 9);
}

void GpuDriver::oledDrawFilledRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
    uint8_t payload[9] = {
        static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>((x >> 8) & 0xFF),
        static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>((y >> 8) & 0xFF),
        static_cast<uint8_t>(w & 0xFF), static_cast<uint8_t>((w >> 8) & 0xFF),
        static_cast<uint8_t>(h & 0xFF), static_cast<uint8_t>((h >> 8) & 0xFF),
        static_cast<uint8_t>(on ? 1 : 0)
    };
    sendCommand(GpuCommand::OLED_FILL, payload, 9);
}

void GpuDriver::oledDrawCircle(int16_t cx, int16_t cy, int16_t radius, bool on) {
    uint8_t payload[7] = {
        static_cast<uint8_t>(cx & 0xFF), static_cast<uint8_t>((cx >> 8) & 0xFF),
        static_cast<uint8_t>(cy & 0xFF), static_cast<uint8_t>((cy >> 8) & 0xFF),
        static_cast<uint8_t>(radius & 0xFF), static_cast<uint8_t>((radius >> 8) & 0xFF),
        static_cast<uint8_t>(on ? 1 : 0)
    };
    sendCommand(GpuCommand::OLED_CIRCLE, payload, 7);
}

// ============== Variables ==============
void GpuDriver::setVar(uint8_t index, int16_t value) {
    uint8_t payload[3] = {
        index,
        static_cast<uint8_t>(value & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF)
    };
    sendCommand(GpuCommand::SET_VAR, payload, 3);
}

void GpuDriver::setVars(uint8_t startIndex, const int16_t* values, uint8_t count) {
    if (count == 0) return;
    
    size_t payloadSize = 2 + (count * 2);
    uint8_t payload[514];  // Max: 2 + 256*2
    if (payloadSize > sizeof(payload)) payloadSize = sizeof(payload);
    
    payload[0] = startIndex;
    payload[1] = count;
    
    for (int i = 0; i < count && (2 + i*2 + 1) < sizeof(payload); i++) {
        payload[2 + i*2] = static_cast<uint8_t>(values[i] & 0xFF);
        payload[2 + i*2 + 1] = static_cast<uint8_t>((values[i] >> 8) & 0xFF);
    }
    
    sendCommand(GpuCommand::SET_VARS, payload, payloadSize);
}

// ============== System Commands ==============
bool GpuDriver::ping(uint32_t timeoutMs) {
    sendCommand(GpuCommand::PING, nullptr, 0);
    
    // Try to read PONG response
    uint8_t buffer[16];
    int len = readResponse(buffer, sizeof(buffer), timeoutMs);
    
    // Look for PONG (0xAA 0x55 0xF1 ...)
    if (len >= 5 && buffer[0] == SYNC_BYTE_0 && buffer[1] == SYNC_BYTE_1 && 
        buffer[2] == static_cast<uint8_t>(GpuCommand::PONG)) {
        ESP_LOGI(TAG, "PONG received");
        return true;
    }
    
    return false;
}

void GpuDriver::reset() {
    sendCommand(GpuCommand::RESET, nullptr, 0);
}

void GpuDriver::nop() {
    sendCommand(GpuCommand::NOP, nullptr, 0);
}

// ============== Keep-Alive ==============
void GpuDriver::keepAliveTaskFunc(void* param) {
    GpuDriver* driver = static_cast<GpuDriver*>(param);
    
    ESP_LOGI(TAG, "Keep-alive task started (interval: %lums)", driver->m_keepAliveInterval);
    
    while (driver->m_keepAliveRunning) {
        vTaskDelay(pdMS_TO_TICKS(driver->m_keepAliveInterval));
        
        if (driver->m_keepAliveRunning && driver->m_initialized) {
            // Send a NOP to keep the connection alive
            driver->nop();
        }
    }
    
    ESP_LOGI(TAG, "Keep-alive task stopped");
    vTaskDelete(nullptr);
}

void GpuDriver::startKeepAlive(uint32_t intervalMs) {
    if (m_keepAliveRunning) {
        ESP_LOGW(TAG, "Keep-alive already running");
        return;
    }
    
    m_keepAliveInterval = intervalMs;
    m_keepAliveRunning = true;
    
    xTaskCreate(keepAliveTaskFunc, "gpu_keepalive", 2048, this, 5, &m_keepAliveTask);
}

void GpuDriver::stopKeepAlive() {
    if (!m_keepAliveRunning) return;
    
    m_keepAliveRunning = false;
    
    // Give the task time to exit
    vTaskDelay(pdMS_TO_TICKS(m_keepAliveInterval + 100));
    m_keepAliveTask = nullptr;
}

// ============== Fixed-Point Conversion ==============
void GpuDriver::floatToFixed88(float val, uint8_t* outFrac, uint8_t* outInt) {
    // 8.8 fixed point: high byte = signed integer, low byte = fraction (0-255)
    int8_t intPart = static_cast<int8_t>(val);
    float fracPart = val - static_cast<float>(intPart);
    if (fracPart < 0) fracPart += 1.0f;  // Handle negative fractions
    
    *outFrac = static_cast<uint8_t>(fracPart * 256.0f);
    *outInt = static_cast<uint8_t>(intPart);
}

// ============== Float Drawing (Anti-Aliased/Weighted Pixels) ==============
void GpuDriver::drawLineF(float x1, float y1, float x2, float y2, uint8_t r, uint8_t g, uint8_t b) {
    // DRAW_LINE_F uses 8.8 fixed-point format
    // Payload: X1_frac, X1_int, Y1_frac, Y1_int, X2_frac, X2_int, Y2_frac, Y2_int, R, G, B
    uint8_t payload[11];
    
    floatToFixed88(x1, &payload[0], &payload[1]);
    floatToFixed88(y1, &payload[2], &payload[3]);
    floatToFixed88(x2, &payload[4], &payload[5]);
    floatToFixed88(y2, &payload[6], &payload[7]);
    payload[8] = r;
    payload[9] = g;
    payload[10] = b;
    
    sendCommand(GpuCommand::DRAW_LINE_F, payload, 11);
}

void GpuDriver::drawRectF(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b) {
    // DRAW_RECT_F: X(8.8), Y(8.8), W(8.8), H(8.8), R, G, B
    uint8_t payload[11];
    
    floatToFixed88(x, &payload[0], &payload[1]);
    floatToFixed88(y, &payload[2], &payload[3]);
    floatToFixed88(w, &payload[4], &payload[5]);
    floatToFixed88(h, &payload[6], &payload[7]);
    payload[8] = r;
    payload[9] = g;
    payload[10] = b;
    
    sendCommand(GpuCommand::DRAW_RECT_F, payload, 11);
}

void GpuDriver::drawFilledRectF(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b) {
    // DRAW_FILL_F: X(8.8), Y(8.8), W(8.8), H(8.8), R, G, B
    // GPU calculates edge coverage for smooth AA edges
    uint8_t payload[11];
    
    floatToFixed88(x, &payload[0], &payload[1]);
    floatToFixed88(y, &payload[2], &payload[3]);
    floatToFixed88(w, &payload[4], &payload[5]);
    floatToFixed88(h, &payload[6], &payload[7]);
    payload[8] = r;
    payload[9] = g;
    payload[10] = b;
    
    sendCommand(GpuCommand::DRAW_FILL_F, payload, 11);
}

void GpuDriver::drawCircleF(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b) {
    // DRAW_CIRCLE_F: CX(8.8), CY(8.8), R(8.8), R, G, B
    uint8_t payload[9];
    
    floatToFixed88(cx, &payload[0], &payload[1]);
    floatToFixed88(cy, &payload[2], &payload[3]);
    floatToFixed88(radius, &payload[4], &payload[5]);
    payload[6] = r;
    payload[7] = g;
    payload[8] = b;
    
    sendCommand(GpuCommand::DRAW_CIRCLE_F, payload, 9);
}

void GpuDriver::blitSpriteF(uint8_t spriteId, float x, float y) {
    // BLIT_SPRITE_F: spriteId, X(8.8), Y(8.8)
    // Sub-pixel sprite positioning for smooth movement
    uint8_t payload[5];
    
    payload[0] = spriteId;
    floatToFixed88(x, &payload[1], &payload[2]);
    floatToFixed88(y, &payload[3], &payload[4]);
    
    sendCommand(GpuCommand::BLIT_SPRITE_F, payload, 5);
}

void GpuDriver::blitSpriteRotated(uint8_t spriteId, float x, float y, float angleDegrees) {
    // BLIT_SPRITE_ROT: spriteId, X(8.8), Y(8.8), Angle(8.8 fixed = degrees)
    // GPU uses transformation matrix for rotation with bilinear interpolation when AA enabled
    uint8_t payload[7];
    
    payload[0] = spriteId;
    floatToFixed88(x, &payload[1], &payload[2]);
    floatToFixed88(y, &payload[3], &payload[4]);
    
    // Angle as 8.8 fixed point (integer part in high byte, fractional in low)
    // Range: -128 to +127 degrees with 0.00390625 degree precision
    int16_t fixedAngle = (int16_t)(angleDegrees * 256.0f);
    payload[5] = fixedAngle & 0xFF;
    payload[6] = (fixedAngle >> 8) & 0xFF;
    
    sendCommand(GpuCommand::BLIT_SPRITE_ROT, payload, 7);
}

void GpuDriver::setAntiAliasing(bool enabled) {
    // SET_AA: 0=off, 1=on
    // Controls GPU-side anti-aliasing for drawing primitives and sprite rotation
    uint8_t payload[1] = { (uint8_t)(enabled ? 1 : 0) };
    sendCommand(GpuCommand::SET_AA, payload, 1);
}

void GpuDriver::syncAntiAliasingState() {
    // Sync the GPU's AA state with our weighted pixels setting
    if (m_initialized) {
        setAntiAliasing(m_weightedPixels);
    }
}

} // namespace SystemAPI
