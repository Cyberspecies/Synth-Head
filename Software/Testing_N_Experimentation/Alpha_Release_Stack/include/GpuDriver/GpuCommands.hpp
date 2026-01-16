/*****************************************************************
 * GpuCommands.hpp - Stable GPU Command Interface
 * 
 * A clean, well-tested wrapper for CPU->GPU communication.
 * Based on CPU_Programmable_Test.cpp which is confirmed working.
 * 
 * Protocol: [0xAA][0x55][CmdType:1][Length:2][Payload:N]
 * 
 * CRITICAL TIMING REQUIREMENTS:
 * ============================
 * 1. Wait 500ms after UART init before sending first command
 *    - GPU needs time to start its uart_task after boot
 *    - See CPU_PolygonDemo.cpp for working reference
 * 
 * 2. Use uart_wait_tx_done() after sending commands
 *    - Ensures bytes are fully transmitted before continuing
 * 
 * 3. Flush RX buffer before expecting responses
 *    - Clears any stale data from GPU boot messages
 * 
 * If GPU connection is intermittent, increase startup delay.
 * PolygonDemo uses 500ms and works reliably every time.
 * 
 * Usage:
 *   #include "GpuDriver/GpuCommands.hpp"
 *   
 *   GpuCommands gpu;
 *   gpu.init();  // Includes 500ms startup delay
 *   
 *   // Draw on HUB75
 *   gpu.hub75Clear(0, 0, 0);
 *   gpu.hub75Line(0, 0, 127, 31, 255, 0, 0);
 *   gpu.hub75Present();
 *   
 *   // Draw on OLED
 *   gpu.oledClear();
 *   gpu.oledText(10, 10, "Hello");
 *   gpu.oledPresent();
 *****************************************************************/

#ifndef GPU_COMMANDS_HPP
#define GPU_COMMANDS_HPP

#include <cstdint>
#include <cstring>
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"

class GpuCommands {
public:
    // ============================================================
    // Configuration
    // ============================================================
    static constexpr uart_port_t DEFAULT_UART_PORT = UART_NUM_1;
    static constexpr int DEFAULT_TX_PIN = 12;  // CPU TX -> GPU RX (GPIO 13)
    static constexpr int DEFAULT_RX_PIN = 11;  // CPU RX <- GPU TX (GPIO 12)
    static constexpr int DEFAULT_BAUD = 10000000;  // 10 Mbps
    
    // Display dimensions
    static constexpr int HUB75_WIDTH = 128;
    static constexpr int HUB75_HEIGHT = 32;
    static constexpr int OLED_WIDTH = 128;
    static constexpr int OLED_HEIGHT = 128;
    
private:
    // Protocol constants
    static constexpr uint8_t SYNC0 = 0xAA;
    static constexpr uint8_t SYNC1 = 0x55;
    
    // Command types (must match GPU_Programmable.cpp)
    enum class CmdType : uint8_t {
        NOP = 0x00,
        
        // Shader commands
        UPLOAD_SHADER = 0x10,
        DELETE_SHADER = 0x11,
        EXEC_SHADER = 0x12,
        
        // Sprite commands
        UPLOAD_SPRITE = 0x20,
        DELETE_SPRITE = 0x21,
        CLEAR_ALL_SPRITES = 0x22,  // Clear all sprites in one command
        
        // Variable commands
        SET_VAR = 0x30,
        SET_VARS = 0x31,
        
        // HUB75 drawing commands
        DRAW_PIXEL = 0x40,
        DRAW_LINE = 0x41,
        DRAW_RECT = 0x42,
        DRAW_FILL = 0x43,
        DRAW_CIRCLE = 0x44,
        DRAW_POLY = 0x45,
        BLIT_SPRITE = 0x46,
        CLEAR = 0x47,
        
        // Float coordinate versions
        DRAW_LINE_F = 0x48,
        DRAW_CIRCLE_F = 0x49,
        DRAW_RECT_F = 0x4A,
        
        // Target/present
        SET_TARGET = 0x50,
        PRESENT = 0x51,
        
        // OLED-specific commands
        OLED_CLEAR = 0x60,
        OLED_LINE = 0x61,
        OLED_RECT = 0x62,
        OLED_FILL = 0x63,
        OLED_CIRCLE = 0x64,
        OLED_PRESENT = 0x65,
        OLED_PIXEL = 0x66,
        OLED_VLINE = 0x67,
        OLED_HLINE = 0x68,
        OLED_FILL_CIRCLE = 0x69,
        
        // System commands
        PING = 0xF0,           // Send ping to GPU
        PONG = 0xF1,           // GPU response with uptime
        REQUEST_CONFIG = 0xF2, // Request GPU configuration
        CONFIG_RESPONSE = 0xF3,// GPU configuration response
        REQUEST_STATS = 0xF4,  // Request GPU performance stats
        STATS_RESPONSE = 0xF5, // GPU stats response (FPS, RAM, load)
        
        // Alert system (GPU->CPU notifications)
        ALERT = 0xF6,          // GPU sends alert to CPU
        CLEAR_ALERT = 0xF7,    // CPU clears alert on GPU
        REQUEST_ALERTS = 0xF8, // CPU requests alert status
        ALERTS_RESPONSE = 0xF9,// GPU responds with alert status
        
        RESET = 0xFF,
    };
    
public:
    // Alert severity levels (from GPU)
    enum class AlertLevel : uint8_t {
        INFO = 0,
        WARNING = 1,
        ERROR = 2,
        CRITICAL = 3,
    };
    
    // Alert types (from GPU)
    enum class AlertType : uint8_t {
        NONE = 0x00,
        BUFFER_WARNING = 0x01,   // RX buffer > 50% full
        BUFFER_CRITICAL = 0x02, // RX buffer > 75% full
        BUFFER_OVERFLOW = 0x03, // RX buffer overflow detected
        FRAME_DROP = 0x10,      // Frames being dropped
        FRAME_DROP_SEVERE = 0x11, // Severe frame drops
        HEAP_LOW = 0x20,        // Free heap < 50KB
        HEAP_CRITICAL = 0x21,   // Free heap < 20KB
        HUB75_ERROR = 0x30,     // HUB75 display error
        OLED_ERROR = 0x31,      // OLED display error
        UART_ERROR = 0x40,      // UART error
        PARSER_ERROR = 0x41,    // Command parser error
        RECOVERED = 0xF0,       // Previous alert condition recovered
    };
    
    // GPU alert statistics structure
    struct GpuAlertStats {
        uint32_t alertsReceived;     // Total alerts received
        uint32_t droppedFrames;      // Total dropped frames reported
        uint32_t bufferOverflows;    // Total buffer overflows
        bool bufferWarning;          // Current buffer warning state
        bool heapWarning;            // Current heap warning state
        AlertLevel highestLevel;     // Highest alert level seen
    };

private:
    uart_port_t port_;
    bool initialized_;
    
    // Alert tracking
    GpuAlertStats alertStats_;
    
    // RX state machine for non-blocking alert parsing
    uint8_t rxBuf_[64];
    int rxState_;
    uint8_t rxType_;
    uint16_t rxLen_;
    uint16_t rxPos_;
    uint8_t rxPayload_[64];
    
    // 5x7 Font for text rendering (ASCII 32-126)
    // Stored column-wise: each byte is one column, LSB is top
    static constexpr uint8_t FONT_5X7[95][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, // space
        {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
        {0x00, 0x07, 0x00, 0x07, 0x00}, // "
        {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
        {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
        {0x23, 0x13, 0x08, 0x64, 0x62}, // %
        {0x36, 0x49, 0x55, 0x22, 0x50}, // &
        {0x00, 0x05, 0x03, 0x00, 0x00}, // '
        {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
        {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
        {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
        {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
        {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
        {0x08, 0x08, 0x08, 0x08, 0x08}, // -
        {0x00, 0x60, 0x60, 0x00, 0x00}, // .
        {0x20, 0x10, 0x08, 0x04, 0x02}, // /
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
        {0x00, 0x36, 0x36, 0x00, 0x00}, // :
        {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
        {0x08, 0x14, 0x22, 0x41, 0x00}, // <
        {0x14, 0x14, 0x14, 0x14, 0x14}, // =
        {0x00, 0x41, 0x22, 0x14, 0x08}, // >
        {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
        {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
        {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
        {0x46, 0x49, 0x49, 0x49, 0x31}, // S
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
        {0x63, 0x14, 0x08, 0x14, 0x63}, // X
        {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
        {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
        {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
        {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
        {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
        {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
        {0x40, 0x40, 0x40, 0x40, 0x40}, // _
        {0x00, 0x01, 0x02, 0x04, 0x00}, // `
        {0x20, 0x54, 0x54, 0x54, 0x78}, // a
        {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
        {0x38, 0x44, 0x44, 0x44, 0x20}, // c
        {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
        {0x38, 0x54, 0x54, 0x54, 0x18}, // e
        {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
        {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
        {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
        {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
        {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
        {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
        {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
        {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
        {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
        {0x38, 0x44, 0x44, 0x44, 0x38}, // o
        {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
        {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
        {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
        {0x48, 0x54, 0x54, 0x54, 0x20}, // s
        {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
        {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
        {0x44, 0x28, 0x10, 0x28, 0x44}, // x
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
        {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
        {0x00, 0x08, 0x36, 0x41, 0x00}, // {
        {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
        {0x00, 0x41, 0x36, 0x08, 0x00}, // }
        {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
    };
    
    // Send raw command
    void sendCmd(CmdType type, const uint8_t* payload, uint16_t len) {
        uint8_t header[5] = {
            SYNC0, SYNC1,
            static_cast<uint8_t>(type),
            static_cast<uint8_t>(len & 0xFF),
            static_cast<uint8_t>((len >> 8) & 0xFF)
        };
        
        int written = uart_write_bytes(port_, header, 5);
        if (len > 0 && payload) {
            written += uart_write_bytes(port_, payload, len);
        }
        uart_wait_tx_done(port_, pdMS_TO_TICKS(50));
    }
    
    // Encode int16 to payload
    static void encodeI16(uint8_t* buf, int idx, int16_t val) {
        buf[idx] = val & 0xFF;
        buf[idx + 1] = (val >> 8) & 0xFF;
    }
    
public:
    GpuCommands() : port_(DEFAULT_UART_PORT), initialized_(false), 
                    alertStats_{0, 0, 0, false, false, AlertLevel::INFO},
                    rxState_(0), rxType_(0), rxLen_(0), rxPos_(0) {
        memset(rxBuf_, 0, sizeof(rxBuf_));
        memset(rxPayload_, 0, sizeof(rxPayload_));
    }
    
    // ============================================================
    // Initialization
    // ============================================================
    
    /**
     * Initialize UART connection to GPU
     * @param port UART port (default: UART_NUM_1)
     * @param txPin TX pin (default: GPIO 12)
     * @param rxPin RX pin (default: GPIO 11)
     * @param baud Baud rate (default: 10000000)
     * @return true if initialization successful
     */
    bool init(uart_port_t port = DEFAULT_UART_PORT,
              int txPin = DEFAULT_TX_PIN,
              int rxPin = DEFAULT_RX_PIN,
              int baud = DEFAULT_BAUD) {
        port_ = port;
        
        uart_config_t uart_cfg = {
            .baud_rate = baud,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_DEFAULT,
        };
        
        esp_err_t err = uart_param_config(port_, &uart_cfg);
        if (err != ESP_OK) return false;
        
        err = uart_set_pin(port_, txPin, rxPin, -1, -1);
        if (err != ESP_OK) return false;
        
        // Use larger RX buffer to match GPU TX buffer
        err = uart_driver_install(port_, 4096, 2048, 0, nullptr, 0);
        if (err != ESP_OK) return false;
        
        initialized_ = true;
        
        // CRITICAL: Wait 500ms for GPU to fully initialize its UART task
        // This matches the working PolygonDemo timing. Shorter delays cause
        // intermittent connection failures because GPU's uart_task hasn't started yet.
        // See CPU_PolygonDemo.cpp line 193: vTaskDelay(pdMS_TO_TICKS(500))
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Flush any garbage data that accumulated during GPU boot
        uart_flush_input(port_);
        
        // Send reset to clear GPU state
        reset();
        
        // Wait for reset command to be fully transmitted
        uart_wait_tx_done(port_, pdMS_TO_TICKS(50));
        
        return true;
    }
    
    bool isInitialized() const { return initialized_; }
    
    // ============================================================
    // System Commands
    // ============================================================
    
    /** Send ping to GPU */
    void ping() {
        sendCmd(CmdType::PING, nullptr, 0);
    }
    
    /**
     * GPU Configuration Response structure
     * Contains full GPU hardware and runtime information
     */
    struct GpuConfigResponse {
        uint8_t panelCount;           // Number of display panels
        
        // Panel 1 (HUB75)
        uint8_t panel1Type;           // 0=HUB75_RGB, 1=OLED_MONO
        uint16_t panel1Width;
        uint16_t panel1Height;
        uint8_t panel1BitDepth;
        
        // Panel 2 (OLED)
        uint8_t panel2Type;
        uint16_t panel2Width;
        uint16_t panel2Height;
        uint8_t panel2BitDepth;
        
        // Runtime info
        uint32_t uptimeMs;            // GPU uptime in milliseconds
        uint32_t maxDataRateBps;      // Maximum UART baud rate
        uint16_t commandVersion;      // Command protocol version (0x0100 = v1.0)
        
        // Hardware status
        bool hub75Ok;
        bool oledOk;
    };
    
    /**
     * GPU performance statistics response
     * Contains FPS, memory, and load information
     */
    struct GpuStatsResponse {
        float fps;                    // Current FPS (frames per second)
        uint32_t freeHeap;            // Free heap memory in bytes
        uint32_t minHeap;             // Minimum free heap since boot
        uint8_t loadPercent;          // GPU load estimate (0-100%)
        uint32_t totalFrames;         // Total frames rendered since boot
        uint32_t uptimeMs;            // GPU uptime in milliseconds
        bool hub75Ok;                 // HUB75 display status
        bool oledOk;                  // OLED display status
    };
    
    /**
     * Ping GPU and get uptime response
     * @param uptimeMs Output: GPU uptime in milliseconds
     * @param timeoutMs Maximum wait time for response
     * @return true if PONG received, false on timeout
     * 
     * NOTE: If ping fails intermittently, ensure:
     * 1. GPU has had 500ms+ to boot before first ping
     * 2. uart_wait_tx_done is used after sending commands
     * 3. See CPU_PolygonDemo.cpp for working reference implementation
     */
    bool pingWithResponse(uint32_t& uptimeMs, uint32_t timeoutMs = 500) {
        // Flush any pending data before ping
        uart_flush_input(port_);
        vTaskDelay(pdMS_TO_TICKS(5));  // Small delay to let flush complete
        
        // Send PING and wait for TX to complete
        sendCmd(CmdType::PING, nullptr, 0);
        uart_wait_tx_done(port_, pdMS_TO_TICKS(20));
        
        // Wait for PONG response
        uint8_t header[5];
        int64_t startTime = esp_timer_get_time();
        int64_t endTime = startTime + (timeoutMs * 1000);
        
        size_t headerReceived = 0;
        while (esp_timer_get_time() < endTime && headerReceived < 5) {
            int len = uart_read_bytes(port_, header + headerReceived, 
                                      5 - headerReceived, pdMS_TO_TICKS(20));
            if (len > 0) headerReceived += len;
        }
        
        if (headerReceived < 5) {
            ESP_LOGW("GpuCmd", "pingWithResponse: header timeout");
            return false;
        }
        
        // Validate header
        if (header[0] != SYNC0 || header[1] != SYNC1) {
            ESP_LOGW("GpuCmd", "pingWithResponse: bad sync bytes");
            return false;
        }
        
        if (header[2] != static_cast<uint8_t>(CmdType::PONG)) {
            ESP_LOGW("GpuCmd", "pingWithResponse: unexpected command 0x%02X", header[2]);
            return false;
        }
        
        uint16_t payloadLen = header[3] | (header[4] << 8);
        if (payloadLen != 4) {
            ESP_LOGW("GpuCmd", "pingWithResponse: unexpected payload len %d", payloadLen);
            return false;
        }
        
        // Read uptime payload
        uint8_t payload[4];
        size_t payloadReceived = 0;
        while (esp_timer_get_time() < endTime && payloadReceived < 4) {
            int len = uart_read_bytes(port_, payload + payloadReceived,
                                      4 - payloadReceived, pdMS_TO_TICKS(10));
            if (len > 0) payloadReceived += len;
        }
        
        if (payloadReceived < 4) {
            ESP_LOGW("GpuCmd", "pingWithResponse: payload timeout");
            return false;
        }
        
        // Decode little-endian uptime
        uptimeMs = payload[0] | (payload[1] << 8) | 
                   (payload[2] << 16) | (payload[3] << 24);
        
        return true;
    }
    
    /**
     * Request GPU configuration and hardware info
     * @param config Output: GPU configuration response
     * @param timeoutMs Maximum wait time for response
     * @return true if config received, false on timeout
     */
    bool requestConfig(GpuConfigResponse& config, uint32_t timeoutMs = 500) {
        // Flush any pending data
        uart_flush_input(port_);
        
        // Send REQUEST_CONFIG
        sendCmd(CmdType::REQUEST_CONFIG, nullptr, 0);
        
        // Wait for CONFIG_RESPONSE
        uint8_t header[5];
        int64_t startTime = esp_timer_get_time();
        int64_t endTime = startTime + (timeoutMs * 1000);
        
        size_t headerReceived = 0;
        while (esp_timer_get_time() < endTime && headerReceived < 5) {
            int len = uart_read_bytes(port_, header + headerReceived,
                                      5 - headerReceived, pdMS_TO_TICKS(10));
            if (len > 0) headerReceived += len;
        }
        
        if (headerReceived < 5) {
            ESP_LOGW("GpuCmd", "requestConfig: header timeout");
            return false;
        }
        
        // Validate header
        if (header[0] != SYNC0 || header[1] != SYNC1) {
            ESP_LOGW("GpuCmd", "requestConfig: bad sync bytes");
            return false;
        }
        
        if (header[2] != static_cast<uint8_t>(CmdType::CONFIG_RESPONSE)) {
            ESP_LOGW("GpuCmd", "requestConfig: unexpected command 0x%02X", header[2]);
            return false;
        }
        
        uint16_t payloadLen = header[3] | (header[4] << 8);
        if (payloadLen < 25) {  // Minimum expected payload size
            ESP_LOGW("GpuCmd", "requestConfig: unexpected payload len %d", payloadLen);
            return false;
        }
        
        // Read payload
        uint8_t payload[32];
        size_t toRead = (payloadLen > 32) ? 32 : payloadLen;
        size_t payloadReceived = 0;
        while (esp_timer_get_time() < endTime && payloadReceived < toRead) {
            int len = uart_read_bytes(port_, payload + payloadReceived,
                                      toRead - payloadReceived, pdMS_TO_TICKS(10));
            if (len > 0) payloadReceived += len;
        }
        
        if (payloadReceived < toRead) {
            ESP_LOGW("GpuCmd", "requestConfig: payload timeout");
            return false;
        }
        
        // Decode payload into struct
        config.panelCount = payload[0];
        
        config.panel1Type = payload[1];
        config.panel1Width = payload[2] | (payload[3] << 8);
        config.panel1Height = payload[4] | (payload[5] << 8);
        config.panel1BitDepth = payload[6];
        
        config.panel2Type = payload[7];
        config.panel2Width = payload[8] | (payload[9] << 8);
        config.panel2Height = payload[10] | (payload[11] << 8);
        config.panel2BitDepth = payload[12];
        
        config.uptimeMs = payload[13] | (payload[14] << 8) |
                          (payload[15] << 16) | (payload[16] << 24);
        
        config.maxDataRateBps = payload[17] | (payload[18] << 8) |
                                (payload[19] << 16) | (payload[20] << 24);
        
        config.commandVersion = payload[21] | (payload[22] << 8);
        
        config.hub75Ok = payload[23] != 0;
        config.oledOk = payload[24] != 0;
        
        return true;
    }
    
    /**
     * Request GPU performance statistics
     * @param stats Output: GPU performance stats
     * @param timeoutMs Maximum wait time for response
     * @return true if stats received, false on timeout
     */
    bool requestStats(GpuStatsResponse& stats, uint32_t timeoutMs = 500) {
        // Flush any pending data
        uart_flush_input(port_);
        
        // Send REQUEST_STATS
        sendCmd(CmdType::REQUEST_STATS, nullptr, 0);
        
        // Wait for STATS_RESPONSE
        uint8_t header[5];
        int64_t startTime = esp_timer_get_time();
        int64_t endTime = startTime + (timeoutMs * 1000);
        
        size_t headerReceived = 0;
        while (esp_timer_get_time() < endTime && headerReceived < 5) {
            int len = uart_read_bytes(port_, header + headerReceived,
                                      5 - headerReceived, pdMS_TO_TICKS(10));
            if (len > 0) headerReceived += len;
        }
        
        if (headerReceived < 5) {
            ESP_LOGW("GpuCmd", "requestStats: header timeout");
            return false;
        }
        
        // Validate header
        if (header[0] != SYNC0 || header[1] != SYNC1) {
            ESP_LOGW("GpuCmd", "requestStats: bad sync bytes");
            return false;
        }
        
        if (header[2] != static_cast<uint8_t>(CmdType::STATS_RESPONSE)) {
            ESP_LOGW("GpuCmd", "requestStats: unexpected command 0x%02X", header[2]);
            return false;
        }
        
        uint16_t payloadLen = header[3] | (header[4] << 8);
        if (payloadLen < 24) {  // Minimum expected payload size
            ESP_LOGW("GpuCmd", "requestStats: unexpected payload len %d", payloadLen);
            return false;
        }
        
        // Read payload
        uint8_t payload[32];
        size_t toRead = (payloadLen > 32) ? 32 : payloadLen;
        size_t payloadReceived = 0;
        while (esp_timer_get_time() < endTime && payloadReceived < toRead) {
            int len = uart_read_bytes(port_, payload + payloadReceived,
                                      toRead - payloadReceived, pdMS_TO_TICKS(10));
            if (len > 0) payloadReceived += len;
        }
        
        if (payloadReceived < 24) {
            ESP_LOGW("GpuCmd", "requestStats: payload timeout");
            return false;
        }
        
        // Decode payload into struct
        // FPS * 100 (little-endian) -> convert back to float
        uint32_t fps_x100 = payload[0] | (payload[1] << 8) |
                           (payload[2] << 16) | (payload[3] << 24);
        stats.fps = (float)fps_x100 / 100.0f;
        
        // Free heap (little-endian)
        stats.freeHeap = payload[4] | (payload[5] << 8) |
                        (payload[6] << 16) | (payload[7] << 24);
        
        // Min heap (little-endian)
        stats.minHeap = payload[8] | (payload[9] << 8) |
                       (payload[10] << 16) | (payload[11] << 24);
        
        // GPU load
        stats.loadPercent = payload[12];
        
        // Total frames (little-endian)
        stats.totalFrames = payload[13] | (payload[14] << 8) |
                           (payload[15] << 16) | (payload[16] << 24);
        
        // Uptime (little-endian)
        stats.uptimeMs = payload[17] | (payload[18] << 8) |
                        (payload[19] << 16) | (payload[20] << 24);
        
        // Hardware status
        stats.hub75Ok = payload[21] != 0;
        stats.oledOk = payload[22] != 0;
        
        return true;
    }
    
    // ============================================================
    // Alert System - GPU to CPU Notifications
    // ============================================================
    
    /**
     * Get current alert statistics
     * @return Reference to alert stats structure
     */
    const GpuAlertStats& getAlertStats() const {
        return alertStats_;
    }
    
    /**
     * Check for and process any GPU alerts/responses (non-blocking)
     * Call this periodically (e.g., in update loop) to receive GPU feedback.
     * Alerts are automatically parsed and tracked in alertStats_.
     */
    void checkForAlerts() {
        if (!initialized_) return;
        
        // Read available bytes (non-blocking)
        int len = uart_read_bytes(port_, rxBuf_, sizeof(rxBuf_), 0);
        if (len <= 0) return;
        
        // Parse bytes (state machine)
        for (int i = 0; i < len; i++) {
            uint8_t b = rxBuf_[i];
            switch (rxState_) {
                case 0: if (b == SYNC0) rxState_ = 1; break;
                case 1: 
                    if (b == SYNC1) rxState_ = 2; 
                    else rxState_ = (b == SYNC0) ? 1 : 0; 
                    break;
                case 2: rxType_ = b; rxState_ = 3; break;
                case 3: rxLen_ = b; rxState_ = 4; break;
                case 4: 
                    rxLen_ |= (b << 8);
                    if (rxLen_ == 0) {
                        rxState_ = 0;
                    } else if (rxLen_ > sizeof(rxPayload_)) {
                        rxState_ = 0;  // Payload too large, skip
                    } else {
                        rxPos_ = 0;
                        rxState_ = 5;
                    }
                    break;
                case 5:
                    rxPayload_[rxPos_++] = b;
                    if (rxPos_ >= rxLen_) {
                        // Process complete message
                        processResponse(static_cast<CmdType>(rxType_), rxPayload_, rxLen_);
                        rxState_ = 0;
                    }
                    break;
            }
        }
    }
    
    /**
     * Reset alert statistics
     */
    void resetAlertStats() {
        alertStats_ = {0, 0, 0, false, false, AlertLevel::INFO};
    }
    
    /**
     * Check if there are any active warnings
     * @return true if buffer or heap warnings are active
     */
    bool hasActiveWarnings() const {
        return alertStats_.bufferWarning || alertStats_.heapWarning;
    }
    
    /**
     * Check if critical alerts have been received
     * @return true if any CRITICAL level alerts were received
     */
    bool hasCriticalAlerts() const {
        return alertStats_.highestLevel == AlertLevel::CRITICAL;
    }
    
    /**
     * Get string representation of alert level
     */
    static const char* alertLevelToString(AlertLevel level) {
        switch (level) {
            case AlertLevel::INFO: return "INFO";
            case AlertLevel::WARNING: return "WARN";
            case AlertLevel::ERROR: return "ERROR";
            case AlertLevel::CRITICAL: return "CRIT";
            default: return "????";
        }
    }
    
    /**
     * Get string representation of alert type
     */
    static const char* alertTypeToString(AlertType type) {
        switch (type) {
            case AlertType::NONE: return "NONE";
            case AlertType::BUFFER_WARNING: return "BUFFER_WARNING";
            case AlertType::BUFFER_CRITICAL: return "BUFFER_CRITICAL";
            case AlertType::BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
            case AlertType::FRAME_DROP: return "FRAME_DROP";
            case AlertType::FRAME_DROP_SEVERE: return "FRAME_DROP_SEVERE";
            case AlertType::HEAP_LOW: return "HEAP_LOW";
            case AlertType::HEAP_CRITICAL: return "HEAP_CRITICAL";
            case AlertType::HUB75_ERROR: return "HUB75_ERROR";
            case AlertType::OLED_ERROR: return "OLED_ERROR";
            case AlertType::UART_ERROR: return "UART_ERROR";
            case AlertType::PARSER_ERROR: return "PARSER_ERROR";
            case AlertType::RECOVERED: return "RECOVERED";
            default: return "UNKNOWN";
        }
    }

private:
    /**
     * Process a complete response from GPU
     */
    void processResponse(CmdType cmd, const uint8_t* payload, uint16_t len) {
        switch (cmd) {
            case CmdType::ALERT:
                processAlert(payload, len);
                break;
            case CmdType::PONG:
                // PONG received (could add callback if needed)
                break;
            default:
                // Other responses can be handled here
                break;
        }
    }
    
    /**
     * Process an alert from GPU
     * Payload format (16 bytes):
     *   [0]     AlertLevel
     *   [1]     AlertType
     *   [2-5]   value1 (uint32_t)
     *   [6-9]   value2 (uint32_t)
     *   [10-13] timestamp_ms (uint32_t)
     *   [14-15] alert_count (uint16_t)
     */
    void processAlert(const uint8_t* payload, uint16_t len) {
        if (len < 16) return;
        
        AlertLevel level = static_cast<AlertLevel>(payload[0]);
        AlertType type = static_cast<AlertType>(payload[1]);
        uint32_t value1 = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);
        uint32_t value2 = payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24);
        // timestamp and alertCount available if needed
        
        alertStats_.alertsReceived++;
        
        // Track highest alert level
        if (static_cast<uint8_t>(level) > static_cast<uint8_t>(alertStats_.highestLevel)) {
            alertStats_.highestLevel = level;
        }
        
        // Update tracking based on alert type
        switch (type) {
            case AlertType::BUFFER_WARNING:
            case AlertType::BUFFER_CRITICAL:
                alertStats_.bufferWarning = true;
                ESP_LOGW("GpuCmd", "GPU ALERT [%s] %s: buffer %lu/%lu (%.1f%%)",
                         alertLevelToString(level), alertTypeToString(type),
                         value1, value2, (value1 * 100.0f) / value2);
                break;
                
            case AlertType::BUFFER_OVERFLOW:
                alertStats_.bufferOverflows++;
                alertStats_.bufferWarning = true;
                ESP_LOGE("GpuCmd", "GPU ALERT [%s] %s: lost ~%lu bytes, total overflows: %lu",
                         alertLevelToString(level), alertTypeToString(type),
                         value1, alertStats_.bufferOverflows);
                break;
                
            case AlertType::FRAME_DROP:
            case AlertType::FRAME_DROP_SEVERE:
                alertStats_.droppedFrames = value2;  // value2 is total dropped
                ESP_LOGW("GpuCmd", "GPU ALERT [%s] %s: dropped %lu frames this sec, %lu total",
                         alertLevelToString(level), alertTypeToString(type),
                         value1, alertStats_.droppedFrames);
                break;
                
            case AlertType::HEAP_LOW:
            case AlertType::HEAP_CRITICAL:
                alertStats_.heapWarning = true;
                ESP_LOGW("GpuCmd", "GPU ALERT [%s] %s: free heap %lu bytes, min %lu bytes",
                         alertLevelToString(level), alertTypeToString(type),
                         value1, value2);
                break;
                
            case AlertType::RECOVERED:
                if (static_cast<AlertType>(value1) == AlertType::BUFFER_WARNING ||
                    static_cast<AlertType>(value1) == AlertType::BUFFER_CRITICAL) {
                    alertStats_.bufferWarning = false;
                    ESP_LOGI("GpuCmd", "GPU: Buffer recovered, now at %lu bytes", value2);
                } else if (static_cast<AlertType>(value1) == AlertType::HEAP_LOW ||
                           static_cast<AlertType>(value1) == AlertType::HEAP_CRITICAL) {
                    alertStats_.heapWarning = false;
                    ESP_LOGI("GpuCmd", "GPU: Heap recovered, now at %lu bytes", value2);
                }
                break;
                
            default:
                ESP_LOGW("GpuCmd", "GPU ALERT [%s] %s: val1=%lu val2=%lu",
                         alertLevelToString(level), alertTypeToString(type),
                         value1, value2);
                break;
        }
    }

public:
    /** Reset GPU state (clears shaders, sprites, buffers) */
    void reset() {
        sendCmd(CmdType::RESET, nullptr, 0);
    }
    
    // ============================================================
    // HUB75 Commands (128x32 RGB LED matrix)
    // ============================================================
    
    /** Set target framebuffer (0=HUB75, 1=OLED) */
    void setTarget(uint8_t target) {
        sendCmd(CmdType::SET_TARGET, &target, 1);
    }
    
    /** Clear HUB75 display to specified color */
    void hub75Clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) {
        setTarget(0);
        uint8_t payload[3] = {r, g, b};
        sendCmd(CmdType::CLEAR, payload, 3);
    }
    
    /** Draw pixel on HUB75 */
    void hub75Pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
        setTarget(0);
        uint8_t payload[7];
        encodeI16(payload, 0, x);
        encodeI16(payload, 2, y);
        payload[4] = r; payload[5] = g; payload[6] = b;
        sendCmd(CmdType::DRAW_PIXEL, payload, 7);
    }
    
    /** Draw line on HUB75 */
    void hub75Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                   uint8_t r, uint8_t g, uint8_t b) {
        setTarget(0);
        uint8_t payload[11];
        encodeI16(payload, 0, x1);
        encodeI16(payload, 2, y1);
        encodeI16(payload, 4, x2);
        encodeI16(payload, 6, y2);
        payload[8] = r; payload[9] = g; payload[10] = b;
        sendCmd(CmdType::DRAW_LINE, payload, 11);
    }
    
    /** Draw rectangle outline on HUB75 */
    void hub75Rect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint8_t r, uint8_t g, uint8_t b) {
        setTarget(0);
        uint8_t payload[11];
        encodeI16(payload, 0, x);
        encodeI16(payload, 2, y);
        encodeI16(payload, 4, w);
        encodeI16(payload, 6, h);
        payload[8] = r; payload[9] = g; payload[10] = b;
        sendCmd(CmdType::DRAW_RECT, payload, 11);
    }
    
    /** Draw filled rectangle on HUB75 */
    void hub75Fill(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint8_t r, uint8_t g, uint8_t b) {
        setTarget(0);
        uint8_t payload[11];
        encodeI16(payload, 0, x);
        encodeI16(payload, 2, y);
        encodeI16(payload, 4, w);
        encodeI16(payload, 6, h);
        payload[8] = r; payload[9] = g; payload[10] = b;
        sendCmd(CmdType::DRAW_FILL, payload, 11);
    }
    
    /** Draw circle on HUB75 */
    void hub75Circle(int16_t cx, int16_t cy, int16_t radius,
                     uint8_t r, uint8_t g, uint8_t b) {
        setTarget(0);
        uint8_t payload[9];
        encodeI16(payload, 0, cx);
        encodeI16(payload, 2, cy);
        encodeI16(payload, 4, radius);
        payload[6] = r; payload[7] = g; payload[8] = b;
        sendCmd(CmdType::DRAW_CIRCLE, payload, 9);
    }
    
    /** Present HUB75 framebuffer */
    void hub75Present() {
        setTarget(0);
        sendCmd(CmdType::PRESENT, nullptr, 0);
    }
    
    // ============================================================
    // Sprite Commands (Upload and Blit)
    // ============================================================
    
    /**
     * Upload sprite data to GPU memory
     * @param id Sprite ID (0-255)
     * @param data RGB888 pixel data (width * height * 3 bytes)
     * @param width Sprite width in pixels (max 255)
     * @param height Sprite height in pixels (max 255)
     * @return true if upload successful
     * 
     * Note: GPU has limited sprite slots (16 max). Max sprite size is 512 bytes.
     * Protocol: UPLOAD_SPRITE [id:1][w:1][h:1][fmt:1][data:w*h*3]
     * Format: 0 = RGB888 (3 bytes/pixel), 1 = 1bpp mono
     */
    bool uploadSprite(uint8_t id, const uint8_t* data, uint16_t width, uint16_t height) {
        if (!data || width == 0 || height == 0) {
            ESP_LOGW("GpuCmd", "uploadSprite: invalid parameters");
            return false;
        }
        
        // GPU supports max 255x255 sprites (8-bit dimensions)
        if (width > 255 || height > 255) {
            ESP_LOGW("GpuCmd", "uploadSprite: dimensions too large (%d x %d, max 255)", width, height);
            return false;
        }
        
        uint32_t dataSize = width * height * 3;  // RGB888
        
        // GPU has MAX_SPRITE_SIZE = 8192 bytes limit (52x52 RGB max)
        if (dataSize > 8192) {
            ESP_LOGW("GpuCmd", "uploadSprite: sprite data too large for GPU (%lu bytes, max 8192)", dataSize);
            // Try anyway - GPU will reject it but at least we log the issue
        }
        
        // Prepare header: [id:1][width:1][height:1][format:1]
        // Format 0 = RGB888
        uint8_t header[4];
        header[0] = id;
        header[1] = (uint8_t)width;
        header[2] = (uint8_t)height;
        header[3] = 0;  // Format: 0 = RGB888
        
        // Send command with header + pixel data
        uint32_t totalSize = 4 + dataSize;
        uint8_t* payload = (uint8_t*)malloc(totalSize);
        if (!payload) {
            ESP_LOGW("GpuCmd", "uploadSprite: out of memory");
            return false;
        }
        
        memcpy(payload, header, 4);
        memcpy(payload + 4, data, dataSize);
        
        sendCmd(CmdType::UPLOAD_SPRITE, payload, totalSize);
        
        // Wait for transmission to complete (important for large sprites)
        uart_wait_tx_done(port_, pdMS_TO_TICKS(100));
        
        free(payload);
        
        ESP_LOGI("GpuCmd", "Uploaded sprite %d (%dx%d fmt=0, %lu bytes)", 
                 id, width, height, dataSize);
        return true;
    }
    
    /**
     * Blit (draw) uploaded sprite to framebuffer
     * @param id Sprite ID (0-255)
     * @param x X position on display
     * @param y Y position on display
     * 
     * Protocol: BLIT_SPRITE [id:1][x:2][y:2]
     */
    void blitSprite(uint8_t id, int16_t x, int16_t y) {
        uint8_t payload[5];
        payload[0] = id;
        encodeI16(payload, 1, x);
        encodeI16(payload, 3, y);
        sendCmd(CmdType::BLIT_SPRITE, payload, 5);
    }
    
    /**
     * Convenience: Upload and immediately blit sprite
     */
    bool uploadAndBlitSprite(uint8_t id, const uint8_t* data, 
                            uint16_t width, uint16_t height,
                            int16_t x, int16_t y) {
        if (uploadSprite(id, data, width, height)) {
            blitSprite(id, x, y);
            return true;
        }
        return false;
    }
    
    /**
     * Delete a sprite from GPU cache
     * @param id Sprite ID to delete (0-255)
     */
    void deleteSprite(uint8_t id) {
        sendCmd(CmdType::DELETE_SPRITE, &id, 1);
        ESP_LOGI("GpuCmd", "Deleted sprite %d from GPU cache", id);
    }
    
    /**
     * Clear all sprite slots from GPU cache using efficient single command
     * Call this on boot to ensure clean state
     */
    void clearAllSprites() {
        ESP_LOGI("GpuCmd", "Clearing GPU sprite cache (all slots)...");
        // Use efficient single command instead of 16 individual commands
        sendCmd(CmdType::CLEAR_ALL_SPRITES, nullptr, 0);
        vTaskDelay(pdMS_TO_TICKS(10));  // Small delay for GPU to process
        ESP_LOGI("GpuCmd", "GPU sprite cache cleared");
    }
    
    /**
     * Full GPU boot initialization - clear displays and sprite cache
     * Call this after init() to ensure clean state
     */
    void bootClean() {
        ESP_LOGI("GpuCmd", "GPU boot clean - clearing all state...");
        
        // Send reset to clear GPU state
        reset();
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // Clear sprite cache
        clearAllSprites();
        
        // Clear HUB75 to black
        hub75Clear(0, 0, 0);
        hub75Present();
        
        // Clear OLED  
        oledClear();
        oledPresent();
        
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_LOGI("GpuCmd", "GPU boot clean complete");
    }
    
    // ============================================================
    // HUB75 Text Rendering (CPU-side using pixels)
    // ============================================================
    
    /**
     * Draw text on HUB75 using 5x7 font
     * @param x Starting X position
     * @param y Starting Y position  
     * @param text String to draw (null-terminated)
     * @param r Red component
     * @param g Green component
     * @param b Blue component
     * @param scale Font scale (1 = 5x7, 2 = 10x14, etc.)
     */
    void hub75Text(int16_t x, int16_t y, const char* text, 
                   uint8_t r, uint8_t g, uint8_t b, int scale = 1){
        if(!text) return;
        
        setTarget(0);
        int cursorX = x;
        while(*text){
            char c = *text++;
            
            // Map character to font index
            if(c < 32 || c > 126) c = '?';
            int idx = c - 32;
            
            // Draw character pixel by pixel
            for(int col = 0; col < 5; col++){
                uint8_t colData = FONT_5X7[idx][col];
                for(int row = 0; row < 7; row++){
                    if(colData & (1 << row)){
                        if(scale == 1){
                            hub75Pixel(cursorX + col, y + row, r, g, b);
                        }else{
                            hub75Fill(cursorX + col * scale, y + row * scale, 
                                      scale, scale, r, g, b);
                        }
                    }
                }
            }
            
            cursorX += 6 * scale;  // 5 pixels + 1 space
        }
    }
    
    /**
     * Draw text centered on HUB75
     */
    void hub75TextCentered(int16_t y, const char* text, 
                           uint8_t r, uint8_t g, uint8_t b, int scale = 1){
        int w = textWidth(text, scale);
        int x = (HUB75_WIDTH - w) / 2;
        hub75Text(x, y, text, r, g, b, scale);
    }
    
    // ============================================================
    // OLED Commands (128x128 monochrome)
    // ============================================================
    
    /** Clear OLED display */
    void oledClear() {
        sendCmd(CmdType::OLED_CLEAR, nullptr, 0);
    }
    
    /** Draw pixel on OLED */
    void oledPixel(int16_t x, int16_t y, bool on = true) {
        uint8_t payload[5];
        encodeI16(payload, 0, x);
        encodeI16(payload, 2, y);
        payload[4] = on ? 1 : 0;
        sendCmd(CmdType::OLED_PIXEL, payload, 5);
    }
    
    /** Draw line on OLED */
    void oledLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool on = true) {
        uint8_t payload[9];
        encodeI16(payload, 0, x1);
        encodeI16(payload, 2, y1);
        encodeI16(payload, 4, x2);
        encodeI16(payload, 6, y2);
        payload[8] = on ? 1 : 0;
        sendCmd(CmdType::OLED_LINE, payload, 9);
    }
    
    /** Draw rectangle outline on OLED */
    void oledRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true) {
        uint8_t payload[9];
        encodeI16(payload, 0, x);
        encodeI16(payload, 2, y);
        encodeI16(payload, 4, w);
        encodeI16(payload, 6, h);
        payload[8] = on ? 1 : 0;
        sendCmd(CmdType::OLED_RECT, payload, 9);
    }
    
    /** Draw filled rectangle on OLED */
    void oledFill(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true) {
        uint8_t payload[9];
        encodeI16(payload, 0, x);
        encodeI16(payload, 2, y);
        encodeI16(payload, 4, w);
        encodeI16(payload, 6, h);
        payload[8] = on ? 1 : 0;
        sendCmd(CmdType::OLED_FILL, payload, 9);
    }
    
    /** Draw circle on OLED */
    void oledCircle(int16_t cx, int16_t cy, int16_t radius, bool on = true) {
        uint8_t payload[7];
        encodeI16(payload, 0, cx);
        encodeI16(payload, 2, cy);
        encodeI16(payload, 4, radius);
        payload[6] = on ? 1 : 0;
        sendCmd(CmdType::OLED_CIRCLE, payload, 7);
    }
    
    /** Draw filled circle on OLED */
    void oledFillCircle(int16_t cx, int16_t cy, int16_t radius, bool on = true) {
        uint8_t payload[7];
        encodeI16(payload, 0, cx);
        encodeI16(payload, 2, cy);
        encodeI16(payload, 4, radius);
        payload[6] = on ? 1 : 0;
        sendCmd(CmdType::OLED_FILL_CIRCLE, payload, 7);
    }
    
    /** Draw vertical line on OLED (optimized for text) */
    void oledVLine(int16_t x, int16_t y, int16_t len, bool on = true) {
        uint8_t payload[7];
        encodeI16(payload, 0, x);
        encodeI16(payload, 2, y);
        encodeI16(payload, 4, len);
        payload[6] = on ? 1 : 0;
        sendCmd(CmdType::OLED_VLINE, payload, 7);
    }
    
    /** Draw horizontal line on OLED (optimized) */
    void oledHLine(int16_t x, int16_t y, int16_t len, bool on = true) {
        uint8_t payload[7];
        encodeI16(payload, 0, x);
        encodeI16(payload, 2, y);
        encodeI16(payload, 4, len);
        payload[6] = on ? 1 : 0;
        sendCmd(CmdType::OLED_HLINE, payload, 7);
    }
    
    /** Present OLED framebuffer to display */
    void oledPresent() {
        sendCmd(CmdType::OLED_PRESENT, nullptr, 0);
    }
    
    // ============================================================
    // Text Rendering (CPU-side, optimized using vertical lines)
    // ============================================================
    
    /**
     * Draw text on OLED using 5x7 font (optimized with vertical lines)
     * @param x Starting X position
     * @param y Starting Y position  
     * @param text String to draw (null-terminated)
     * @param scale Font scale (1 = 5x7, 2 = 10x14, etc.)
     * @param on Pixel state (true = on, false = off)
     */
    void oledText(int16_t x, int16_t y, const char* text, int scale = 1, bool on = true) {
        if (!text) return;
        
        int cursorX = x;
        while (*text) {
            char c = *text++;
            
            // Map character to font index
            if (c < 32 || c > 126) c = '?';
            int idx = c - 32;
            
            // Draw character column by column using vertical lines for efficiency
            for (int col = 0; col < 5; col++) {
                uint8_t colData = FONT_5X7[idx][col];
                
                if (scale == 1) {
                    // Scale 1: Find consecutive vertical runs and draw as vlines
                    int runStart = -1;
                    for (int row = 0; row <= 7; row++) {
                        bool pixelOn = (row < 7) && (colData & (1 << row));
                        if (pixelOn && runStart < 0) {
                            runStart = row;  // Start new run
                        } else if (!pixelOn && runStart >= 0) {
                            // End run, draw vertical line
                            oledVLine(cursorX + col, y + runStart, row - runStart, on);
                            runStart = -1;
                        }
                    }
                } else {
                    // Scaled: Use filled rectangles for each pixel
                    for (int row = 0; row < 7; row++) {
                        if (colData & (1 << row)) {
                            oledFill(cursorX + col * scale, y + row * scale, 
                                     scale, scale, on);
                        }
                    }
                }
            }
            
            cursorX += 6 * scale;  // 5 pixels + 1 space
        }
    }
    
    /**
     * Draw integer on OLED
     */
    void oledInt(int16_t x, int16_t y, int value, int scale = 1, bool on = true) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d", value);
        oledText(x, y, buf, scale, on);
    }
    
    /**
     * Calculate text width in pixels
     */
    int textWidth(const char* text, int scale = 1) {
        if (!text) return 0;
        int len = strlen(text);
        if (len == 0) return 0;
        return len * 6 * scale - scale;  // 5px + 1 space per char, minus trailing space
    }
    
    /**
     * Draw text centered horizontally on OLED
     */
    void oledTextCentered(int16_t y, const char* text, int scale = 1, bool on = true) {
        int w = textWidth(text, scale);
        int x = (OLED_WIDTH - w) / 2;
        oledText(x, y, text, scale, on);
    }
    
    // ============================================================
    // Higher-level UI primitives
    // ============================================================
    
    /**
     * Draw a progress bar on OLED
     * @param x X position
     * @param y Y position
     * @param w Width
     * @param h Height
     * @param value Progress value (0-100)
     */
    void oledProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, int value) {
        // Draw outline
        oledRect(x, y, w, h, true);
        
        // Calculate fill width
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        int fillW = (w - 4) * value / 100;
        
        // Draw fill
        if (fillW > 0) {
            oledFill(x + 2, y + 2, fillW, h - 4, true);
        }
    }
    
    /**
     * Draw a button-like box with text on OLED
     * @param x X position
     * @param y Y position
     * @param text Button text
     * @param selected Whether button is selected (inverts colors)
     */
    void oledButton(int16_t x, int16_t y, const char* text, bool selected = false) {
        int w = textWidth(text, 1) + 8;
        int h = 11;  // 7px font + 4px padding
        
        if (selected) {
            oledFill(x, y, w, h, true);
            oledText(x + 4, y + 2, text, 1, false);
        } else {
            oledRect(x, y, w, h, true);
            oledText(x + 4, y + 2, text, 1, true);
        }
    }
    
    /**
     * Draw a checkbox on OLED
     * @param x X position
     * @param y Y position
     * @param checked Whether checkbox is checked
     * @param label Label text (can be nullptr)
     */
    void oledCheckbox(int16_t x, int16_t y, bool checked, const char* label = nullptr) {
        // Draw box
        oledRect(x, y, 9, 9, true);
        
        // Draw check mark if checked
        if (checked) {
            oledLine(x + 2, y + 4, x + 4, y + 6, true);
            oledLine(x + 4, y + 6, x + 7, y + 2, true);
        }
        
        // Draw label
        if (label) {
            oledText(x + 12, y + 1, label, 1, true);
        }
    }
    
    /**
     * Draw a slider on OLED
     * @param x X position
     * @param y Y position
     * @param w Width
     * @param value Value (0-100)
     */
    void oledSlider(int16_t x, int16_t y, int16_t w, int value) {
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        
        // Draw track
        oledLine(x, y + 4, x + w - 1, y + 4, true);
        
        // Calculate thumb position
        int thumbX = x + (w - 5) * value / 100;
        
        // Draw thumb
        oledFill(thumbX, y, 5, 9, true);
    }
};

// Define the font array (linker needs this)
constexpr uint8_t GpuCommands::FONT_5X7[95][5];

#endif // GPU_COMMANDS_HPP
