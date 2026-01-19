/*****************************************************************
 * CPU_WifiSpriteUploadTest.cpp - WiFi Sprite Upload & Display Test
 *
 * This test demonstrates:
 * 1. WiFi Captive Portal - Creates "SpriteTest-AP" access point
 * 2. Simple web page for image upload
 * 3. PNG/image decoding (client-side JavaScript)
 * 4. RGB888 pixel data sent to CPU via HTTP POST
 * 5. CPU uploads sprite to GPU
 * 6. GPU displays sprite rotating with AA enabled
 *
 * Flow:
 * Phone → WiFi AP → Upload PNG → JS decodes to RGB888 →
 * POST to /api/sprite → CPU receives → GPU upload → Display rotating
 *
 * Connect to WiFi: "SpriteTest-AP" (no password)
 * Open browser: http://192.168.4.1
 *****************************************************************/

#include "SystemAPI/GPU/GpuDriver.h"
#include "SystemAPI/Web/Server/WifiManager.hpp"
#include "SystemAPI/Web/Server/DnsServer.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h" // Required for WiFi configuration storage
#include "cJSON.h"
#include <cstring>
#include <cmath>
#include <vector>

static const char* TAG = "WIFI_SPRITE_TEST";

using namespace SystemAPI;
using namespace SystemAPI::Web;

// ============== Global State ==============
static GpuDriver g_gpu;
static httpd_handle_t g_server = nullptr;

// Sprite data received from web upload
static std::vector<uint8_t> g_spritePixels;
static int g_spriteWidth = 0;
static int g_spriteHeight = 0;
static bool g_spriteReady = false;
static bool g_newSpriteUploaded = false;

// Animation state
static float g_spriteX = 64.0f;
static float g_spriteY = 16.0f;
static float g_spriteAngle = 0.0f;
static const uint8_t SPRITE_ID = 0;

// ============== HTML Page ==============
static const char HTML_PAGE[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sprite Upload Test</title>
  <style>
    * { box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #1a1a2e;
      color: #eee;
      margin: 0;
      padding: 20px;
      min-height: 100vh;
    }
    .container {
      max-width: 500px;
      margin: 0 auto;
    }
    h1 {
      color: #ff6b00;
      font-size: 1.5rem;
      margin-bottom: 8px;
    }
    .subtitle {
      color: #888;
      font-size: 0.85rem;
      margin-bottom: 24px;
    }
    .card {
      background: #252540;
      border-radius: 12px;
      padding: 20px;
      margin-bottom: 16px;
    }
    .upload-zone {
      border: 2px dashed #444;
      border-radius: 12px;
      padding: 40px 20px;
      text-align: center;
      cursor: pointer;
      transition: all 0.3s;
    }
    .upload-zone:hover, .upload-zone.dragover {
      border-color: #ff6b00;
      background: rgba(255, 107, 0, 0.1);
    }
    .upload-zone.has-image {
      border-style: solid;
      border-color: #00cc66;
    }
    .upload-icon { font-size: 3rem; margin-bottom: 12px; }
    .upload-title { font-weight: 600; margin-bottom: 6px; }
    .upload-hint { font-size: 0.8rem; color: #888; }
    input[type="file"] { display: none; }

    .preview-container {
      display: flex;
      gap: 16px;
      margin-top: 16px;
    }
    .preview-box {
      flex: 1;
      text-align: center;
    }
    .preview-label {
      font-size: 0.75rem;
      color: #888;
      margin-bottom: 8px;
    }
    .preview-frame {
      background: #000;
      border: 2px solid #333;
      border-radius: 8px;
      padding: 10px;
      min-height: 100px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .preview-frame img, .preview-frame canvas {
      image-rendering: pixelated;
      max-width: 100%;
      max-height: 150px;
    }
    .info { font-size: 0.8rem; color: #888; margin-top: 8px; }

    .scale-control {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-top: 16px;
    }
    .scale-control label { font-size: 0.85rem; }
    .scale-control input {
      width: 80px;
      padding: 8px;
      background: #1a1a2e;
      border: 1px solid #444;
      border-radius: 6px;
      color: #eee;
      text-align: center;
    }
    .auto-btn {
      padding: 8px 16px;
      background: transparent;
      border: 1px solid #ff6b00;
      border-radius: 6px;
      color: #ff6b00;
      cursor: pointer;
    }

    .upload-btn {
      width: 100%;
      padding: 16px;
      background: #ff6b00;
      border: none;
      border-radius: 8px;
      color: #fff;
      font-size: 1rem;
      font-weight: 600;
      cursor: pointer;
      margin-top: 16px;
      transition: background 0.2s;
    }
    .upload-btn:hover { background: #ff8533; }
    .upload-btn:disabled {
      background: #444;
      cursor: not-allowed;
    }

    .status {
      margin-top: 16px;
      padding: 12px;
      border-radius: 8px;
      font-size: 0.85rem;
      display: none;
    }
    .status.success {
      display: block;
      background: rgba(0, 204, 102, 0.2);
      color: #00cc66;
    }
    .status.error {
      display: block;
      background: rgba(255, 68, 68, 0.2);
      color: #ff4444;
    }
    .status.info {
      display: block;
      background: rgba(255, 107, 0, 0.2);
      color: #ff6b00;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎨 Sprite Upload Test</h1>
    <p class="subtitle">Upload an image to display on the LED matrix (128x32)</p>

    <div class="card">
      <div class="upload-zone" id="dropZone" onclick="document.getElementById('fileInput').click()">
        <div class="upload-icon">📁</div>
        <div class="upload-title">Drop image here or tap to select</div>
        <div class="upload-hint">PNG, JPG, GIF - any size (will be scaled)</div>
      </div>
      <input type="file" id="fileInput" accept="image/*">

      <div class="preview-container" id="previewContainer" style="display: none;">
        <div class="preview-box">
          <div class="preview-label">Original</div>
          <div class="preview-frame">
            <img id="originalPreview">
          </div>
          <div class="info" id="originalInfo"></div>
        </div>
        <div class="preview-box">
          <div class="preview-label">Scaled</div>
          <div class="preview-frame">
            <canvas id="scaledPreview"></canvas>
          </div>
          <div class="info" id="scaledInfo"></div>
        </div>
      </div>

      <div class="scale-control" id="scaleControl" style="display: none;">
        <label for="scaleInput">Scale:</label>
        <input type="number" id="scaleInput" value="100" min="1" max="800">
        <button class="auto-btn" id="autoScaleBtn" onclick="autoScale()">Auto</button>
      </div>

      <button class="upload-btn" id="uploadBtn" onclick="uploadSprite()">
        Upload to Display
      </button>

      <div class="status" id="status"></div>
    </div>

    <div class="card">
      <h2 style="color: #ff6b00; font-size: 1.2rem; margin-bottom: 12px;">Instructions</h2>
      <ol class="subtitle" style="color: #ddd; font-size: 0.9rem; line-height: 1.4;">
        <li>Connect to WiFi network: <strong>SpriteTest-AP</strong> (no password)</li>
        <li>Open this URL in your browser: <strong>http://192.168.4.1</strong></li>
        <li>Upload a PNG, JPG, or GIF image file</li>
        <li>Watch the magic happen! 🎉</li>
      </ol>
    </div>
  </div>

  <script>
    /*****************************************************************
     * CPU_WifiSpriteUploadTest.js - WiFi Sprite Upload & Display Test
     *
     * This script handles:
     * 1. Image file selection and preview
     * 2. Image scaling and adjustment
     * 3. Sprite upload via HTTP POST
     * 4. Status display and notifications
     *****************************************************************/

    const TAG = "WIFI_SPRITE_TEST";

    // ============== Global State ==============
    let originalImage = null;
    let scaledWidth = 0;
    let scaledHeight = 0;

    // ============== Image Upload & Preview ==============
    document.getElementById('fileInput').addEventListener('change', function(event) {
      const file = event.target.files[0];
      if (!file) return;

      const reader = new FileReader();
      reader.onload = function(e) {
        const img = new Image();
        img.onload = function() {
          // Original image info
          originalImage = img;
          const info = `${img.width} x ${img.height} (${file.size} bytes)`;
          document.getElementById('originalInfo').textContent = info;

          // Show preview container
          document.getElementById('previewContainer').style.display = 'flex';

          // Update scaled preview
          updateScaledPreview();
        };
        img.src = e.target.result;
      };
      reader.readAsDataURL(file);
    });

    function updateScaledPreview() {
      if (!originalImage) {
        document.getElementById('uploadBtn').disabled = true;
        return;
      }

      var scale = parseInt(document.getElementById('scaleInput').value) || 100;
      scaledWidth = Math.round(originalImage.width * scale / 100);
      scaledHeight = Math.round(originalImage.height * scale / 100);

      if (scaledWidth < 1) scaledWidth = 1;
      if (scaledHeight < 1) scaledHeight = 1;
      if (scaledWidth > 128) scaledWidth = 128;
      if (scaledHeight > 32) scaledHeight = 32;

      var canvas = document.getElementById('scaledPreview');
      canvas.width = scaledWidth;
      canvas.height = scaledHeight;
      var ctx = canvas.getContext('2d');
      ctx.imageSmoothingEnabled = false;
      ctx.drawImage(originalImage, 0, 0, scaledWidth, scaledHeight);

      // Scale display
      canvas.style.width = (scaledWidth * 4) + 'px';
      canvas.style.height = (scaledHeight * 4) + 'px';

      var bytes = scaledWidth * scaledHeight * 3;
      document.getElementById('scaledInfo').textContent =
        scaledWidth + ' x ' + scaledHeight + ' (' + bytes + ' bytes)';

      // Only disable if dimensions are invalid
      document.getElementById('uploadBtn').disabled = (scaledWidth < 1 || scaledHeight < 1);
    }

    // ============== Image Scaling ==============
    document.getElementById('scaleInput').addEventListener('input', updateScaledPreview);

    function autoScale() {
      if (!originalImage) return;

      // Auto scale to fit 128x32
      var aspectRatio = originalImage.width / originalImage.height;
      if (aspectRatio > (128 / 32)) {
        // Fit to width
        scaledWidth = 128;
        scaledHeight = Math.round(128 / aspectRatio);
      } else {
        // Fit to height
        scaledWidth = Math.round(32 * aspectRatio);
        scaledHeight = 32;
      }

      document.getElementById('scaleInput').value = Math.round(scaledWidth / originalImage.width * 100);
      updateScaledPreview();
    }

    // ============== Sprite Upload ==============
    function uploadSprite() {
      if (!originalImage) return;

      showStatus('Processing...', 'info');

      // Get pixel data from canvas
      var canvas = document.getElementById('scaledPreview');
      var ctx = canvas.getContext('2d');
      var imageData = ctx.getImageData(0, 0, scaledWidth, scaledHeight);

      // Convert to RGB888
      var pixels = new Uint8Array(scaledWidth * scaledHeight * 3);
      var data = imageData.data;
      var idx = 0;
      for (var i = 0; i < data.length; i += 4) {
        pixels[idx++] = data[i];     // R
        pixels[idx++] = data[i + 1]; // G
        pixels[idx++] = data[i + 2]; // B
        // Skip alpha
      }

      // Convert to base64
      var binary = '';
      for (var i = 0; i < pixels.byteLength; i++) {
        binary += String.fromCharCode(pixels[i]);
      }
      var pixelsBase64 = btoa(binary);

      // Send to server
      var payload = {
        width: scaledWidth,
        height: scaledHeight,
        pixels: pixelsBase64
      };

      fetch('/api/sprite/upload', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      })
      .then(function(response) { return response.json(); })
      .then(function(data) {
        if (data.success) {
          showStatus('Sprite uploaded! Watch the display!', 'success');
        } else {
          showStatus('Upload failed: ' + (data.error || 'Unknown error'), 'error');
        }
      })
      .catch(function(err) {
        showStatus('Upload failed: ' + err.message, 'error');
      });
    }

    function showStatus(msg, type) {
      var el = document.getElementById('status');
      el.textContent = msg;
      el.className = 'status ' + type;
    }
  </script>
</body>
</html>
)rawliteral";

// ============== Base64 Decode ==============
static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static std::vector<uint8_t> base64_decode(const char* input, size_t len) {
    std::vector<uint8_t> output;
    output.reserve(len * 3 / 4);

    uint32_t buffer = 0;
    int bits = 0;

    for (size_t i = 0; i < len; i++) {
        char c = input[i];
        if (c == '=' || c == '\n' || c == '\r') continue;

        int val = b64_decode_char(c);
        if (val < 0) continue;

        buffer = (buffer << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            output.push_back((buffer >> bits) & 0xFF);
        }
    }

    return output;
}

// ============== HTTP Handlers ==============

// Serve main HTML page
static esp_err_t handle_root(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
    return ESP_OK;
}

// Handle sprite upload
static esp_err_t handle_sprite_upload(httpd_req_t* req) {
    ESP_LOGI(TAG, "Sprite upload request received");

    // Read request body
    int total_len = req->content_len;
    if (total_len > 65536) {
        ESP_LOGE(TAG, "Request too large: %d bytes", total_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }

    char* buf = (char*)malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[total_len] = '\0';

    // Parse JSON
    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Extract fields
    cJSON* widthJson = cJSON_GetObjectItem(json, "width");
    cJSON* heightJson = cJSON_GetObjectItem(json, "height");
    cJSON* pixelsJson = cJSON_GetObjectItem(json, "pixels");

    if (!widthJson || !heightJson || !pixelsJson ||
        !cJSON_IsNumber(widthJson) || !cJSON_IsNumber(heightJson) || !cJSON_IsString(pixelsJson)) {
        ESP_LOGE(TAG, "Missing or invalid fields in JSON");
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");
        return ESP_FAIL;
    }

    int width = widthJson->valueint;
    int height = heightJson->valueint;
    const char* pixelsBase64 = pixelsJson->valuestring;

    ESP_LOGI(TAG, "Sprite: %dx%d, base64 len=%d", width, height, (int)strlen(pixelsBase64));

    // Validate dimensions
    if (width < 1 || width > 128 || height < 1 || height > 64) {
        ESP_LOGE(TAG, "Invalid dimensions: %dx%d", width, height);
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid dimensions");
        return ESP_FAIL;
    }

    // Decode base64 pixels
    std::vector<uint8_t> pixels = base64_decode(pixelsBase64, strlen(pixelsBase64));
    cJSON_Delete(json);

    size_t expectedSize = width * height * 3;
    if (pixels.size() != expectedSize) {
        ESP_LOGE(TAG, "Pixel data size mismatch: got %d, expected %d", (int)pixels.size(), (int)expectedSize);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Pixel data size mismatch");
        return ESP_FAIL;
    }

    // Store sprite data
    g_spritePixels = std::move(pixels);
    g_spriteWidth = width;
    g_spriteHeight = height;
    g_newSpriteUploaded = true;

    ESP_LOGI(TAG, "Sprite stored: %dx%d, %d bytes", g_spriteWidth, g_spriteHeight, (int)g_spritePixels.size());

    // Send success response
    httpd_resp_set_type(req, "application/json");
    const char* response = "{\"success\":true}";
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

// Captive portal redirect
static esp_err_t handle_captive_redirect(httpd_req_t* req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ============== HTTP Server Setup ==============
static void start_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&g_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    // Register handlers
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(g_server, &uri_root);

    httpd_uri_t uri_upload = {
        .uri = "/api/sprite/upload",
        .method = HTTP_POST,
        .handler = handle_sprite_upload,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(g_server, &uri_upload);

    // Captive portal redirects
    const char* redirect_paths[] = {
        "/generate_204", "/gen_204", "/connecttest.txt", "/fwlink",
        "/hotspot-detect.html", "/library/test/success.html",
        "/canonical.html", "/success.txt", "/ncsi.txt"
    };

    for (const char* path : redirect_paths) {
        httpd_uri_t uri_redirect = {
            .uri = path,
            .method = HTTP_GET,
            .handler = handle_captive_redirect,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(g_server, &uri_redirect);
    }

    ESP_LOGI(TAG, "HTTP server started on port 80");
}

// ============== Main Application ==============
extern "C" void app_main() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   WiFi Sprite Upload & Display Test        ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    // ========== Initialize NVS (Required for WiFi) ==========
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // If NVS partition was truncated or has a new version, erase and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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

    // Initial display - show waiting message with graphics
    g_gpu.setTarget(GpuTarget::HUB75);
    g_gpu.clear(5, 5, 20);
    g_gpu.drawRect(10, 5, 108, 22, 255, 128, 0);  // Orange border
    // Note: Removed boot center line - was causing confusion with sprite display
    g_gpu.present();

    ESP_LOGI(TAG, "GPU initialized and ready!");

    // ========== Initialize WiFi AP ==========
    ESP_LOGI(TAG, "Starting WiFi Access Point...");

    PortalConfig wifiConfig;
    strncpy(wifiConfig.ssid, "SpriteTest-AP", sizeof(wifiConfig.ssid));
    wifiConfig.password[0] = '\0';  // Open network

    if (!WIFI_MANAGER.init(wifiConfig)) {
        ESP_LOGE(TAG, "Failed to start WiFi AP!");
        return;
    }

    // Update sync state
    auto& state = SYNC_STATE.state();
    strncpy(state.ssid, wifiConfig.ssid, sizeof(state.ssid) - 1);
    snprintf(state.ipAddress, sizeof(state.ipAddress), "192.168.4.1");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "  WiFi AP: SpriteTest-AP (no password)");
    ESP_LOGI(TAG, "  URL: http://192.168.4.1");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "");

    // ========== Start DNS Server (for captive portal) ==========
    if (!DNS_SERVER.start()) {
        ESP_LOGW(TAG, "DNS server failed to start (captive portal may not auto-open)");
    }

    // ========== Start HTTP Server ==========
    start_http_server();

    ESP_LOGI(TAG, "Ready! Connect to WiFi and upload a sprite.");

    // ========== Main Loop ==========
    uint32_t frameCount = 0;
    int64_t lastFrameTime = esp_timer_get_time();

    while (true) {
        // Check for new sprite upload
        if (g_newSpriteUploaded) {
            g_newSpriteUploaded = false;

            ESP_LOGI(TAG, "New sprite received! Uploading to GPU: %dx%d", g_spriteWidth, g_spriteHeight);
            // Debug: print first 16 bytes of pixel data
            if (!g_spritePixels.empty()) {
                char pixelDebug[128] = {0};
                int dbgLen = g_spritePixels.size() < 16 ? g_spritePixels.size() : 16;
                for (int i = 0; i < dbgLen; ++i) {
                    snprintf(pixelDebug + i * 3, 4, "%02X ", g_spritePixels[i]);
                }
                ESP_LOGI(TAG, "Pixel data (first 16 bytes): %s", pixelDebug);
            } else {
                ESP_LOGW(TAG, "Pixel data is empty!");
            }
            ESP_LOGI(TAG, "Pixel data size: %d", (int)g_spritePixels.size());

            // ASCII art debug: print sprite as O (black) and X (white)
            ESP_LOGI(TAG, "=== SPRITE ASCII ART (%dx%d) ===", g_spriteWidth, g_spriteHeight);
            for (int y = 0; y < g_spriteHeight; y++) {
                char rowBuf[256] = {0};  // Max 128 width + null
                int bufIdx = 0;
                for (int x = 0; x < g_spriteWidth && bufIdx < 250; x++) {
                    size_t pixelIdx = (y * g_spriteWidth + x) * 3;  // RGB888
                    if (pixelIdx + 2 < g_spritePixels.size()) {
                        uint8_t r = g_spritePixels[pixelIdx];
                        uint8_t g = g_spritePixels[pixelIdx + 1];
                        uint8_t b = g_spritePixels[pixelIdx + 2];
                        // Calculate brightness (simple average)
                        int brightness = (r + g + b) / 3;
                        rowBuf[bufIdx++] = (brightness > 127) ? 'O' : '_';
                    } else {
                        rowBuf[bufIdx++] = '?';
                    }
                }
                rowBuf[bufIdx] = '\0';
                ESP_LOGI(TAG, "Row %02d: %s", y, rowBuf);
            }
            ESP_LOGI(TAG, "=== END SPRITE ===");

            // Upload to GPU
            bool uploadResult = g_gpu.uploadSprite(SPRITE_ID, g_spriteWidth, g_spriteHeight,
                                   g_spritePixels.data(), SpriteFormat::RGB888);
            ESP_LOGI(TAG, "uploadSprite() result: %s", uploadResult ? "SUCCESS" : "FAIL");
            g_spriteReady = uploadResult;

            // Position sprite center at screen center (64, 16)
            // blitSpriteRotated uses (dx, dy) as the CENTER of the sprite
            g_spriteX = 64.0f;
            g_spriteY = 16.0f;
            g_spriteAngle = 0.0f;

            ESP_LOGI(TAG, "Sprite uploaded to GPU! Starting rotation animation.");
        }

        // Render frame
        g_gpu.setTarget(GpuTarget::HUB75);

        if (g_spriteReady) {
            // Clear with dark background
            g_gpu.clear(5, 5, 15);
            g_gpu.blitSpriteRotated(SPRITE_ID, g_spriteX, g_spriteY, g_spriteAngle);

            // Update rotation angle (slow rotation for smooth demo)
            g_spriteAngle += 0.5f;
            if (g_spriteAngle >= 360.0f) g_spriteAngle -= 360.0f;

            // Note: Removed debug crosshairs that were overwriting the sprite
            // g_gpu.drawLineF(64.0f, 0.0f, 64.0f, 31.0f, 20, 20, 40);
            // g_gpu.drawLineF(0.0f, 16.0f, 127.0f, 16.0f, 20, 20, 40);

        } else {
            // Waiting for sprite - show animated pattern
            g_gpu.clear(5, 5, 20);
            // Animated waiting indicator
            float t = frameCount * 0.05f;
            for (int i = 0; i < 4; i++) {
                float angle = t + i * 1.57f;
                float x = 64.0f + cosf(angle) * 10.0f;
                float y = 16.0f + sinf(angle) * 6.0f;
                uint8_t brightness = 100 + (uint8_t)(100 * sinf(t + i));
                g_gpu.drawCircleF(x, y, 2.0f, brightness, brightness / 2, 0);
            }
            // Border
            g_gpu.drawRect(5, 2, 118, 28, 255, 128, 0);
        }
        g_gpu.present();

        // Log status periodically
        frameCount++;
        if (frameCount % 100 == 0) {  // Every ~3 seconds at 30fps
            int64_t now = esp_timer_get_time();
            float fps = 100.0f / ((now - lastFrameTime) / 1000000.0f);
            lastFrameTime = now;

            ESP_LOGI(TAG, "Frame %lu | FPS: %.1f | Sprite: %s | Angle: %.1f°",
                     frameCount, fps, g_spriteReady ? "READY" : "waiting", g_spriteAngle);
        }

        // ~30 FPS - match GPU processing rate to avoid buffer overflow
        // GPU runs at ~35 FPS, so 30 FPS gives headroom
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}