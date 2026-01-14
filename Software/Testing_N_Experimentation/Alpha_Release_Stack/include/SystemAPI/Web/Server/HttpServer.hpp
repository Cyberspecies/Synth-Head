/*****************************************************************
 * @file HttpServer.hpp
 * @brief HTTP Server for Captive Portal
 * 
 * Implements the HTTP server with URI routing and handlers.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "SystemAPI/Web/WebTypes.hpp"
#include "SystemAPI/Web/Interfaces/ICommandHandler.hpp"
#include "SystemAPI/Web/Content/WebContent.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"
#include "SystemAPI/Animation/AnimationConfig.hpp"
#include "SystemAPI/Utils/FileSystemService.hpp"
#include "SystemAPI/Storage/StorageManager.hpp"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "cJSON.h"

// Socket includes for getpeername
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include <cstring>
#include <vector>
#include <functional>
#include <string>
#include <sys/stat.h>

namespace SystemAPI {
namespace Web {

static const char* HTTP_TAG = "HttpServer";

/**
 * @brief Saved sprite metadata (pixel data stored separately)
 */
struct SavedSprite {
    int id = 0;
    std::string name;
    int width = 64;
    int height = 32;
    int scale = 100;
    std::string preview;  // base64 PNG thumbnail
    // Note: Actual pixel data stored in NVS/SPIFFS for persistence
};

/**
 * @brief Variable definition for equations
 */
struct EquationVariable {
    std::string name;
    std::string type;   // "static", "sensor", "equation"
    std::string value;  // static value, sensor id, or equation id
};

/**
 * @brief Saved equation definition
 */
struct SavedEquation {
    int id = 0;
    std::string name;
    std::string expression;
    std::vector<EquationVariable> variables;
};

// Sprite storage (persisted to SD card)
static std::vector<SavedSprite> savedSprites_;
static int nextSpriteId_ = 1;

// Equation storage (persisted to SD card)
static std::vector<SavedEquation> savedEquations_;
static int nextEquationId_ = 1;

static bool spiffs_initialized_ = false;  // Still used for legacy SPIFFS
static bool sdcard_storage_ready_ = false;

// SD Card paths (primary storage)
static const char* SPRITE_DIR = "/sdcard/sprites";
static const char* SPRITE_INDEX_FILE = "/sdcard/sprites/index.json";
static const char* EQUATION_DIR = "/sdcard/equations";
static const char* EQUATION_INDEX_FILE = "/sdcard/equations/index.json";

// Legacy SPIFFS paths (fallback)
static const char* SPRITE_DIR_SPIFFS = "/spiffs/sprites";
static const char* SPRITE_INDEX_FILE_SPIFFS = "/spiffs/sprites/index.json";
static const char* EQUATION_INDEX_FILE_SPIFFS = "/spiffs/equations.json";

/**
 * @brief HTTP Server for Web Portal
 * 
 * Handles all HTTP requests including API endpoints,
 * static content, and captive portal detection.
 */
class HttpServer {
public:
    using CommandCallback = std::function<void(CommandType, cJSON*)>;
    
    /**
     * @brief Get singleton instance
     */
    static HttpServer& instance() {
        static HttpServer inst;
        return inst;
    }
    
    /**
     * @brief Start the HTTP server
     * @return true on success
     */
    bool start() {
        if (server_) return true;
        
        // Initialize SD card storage (primary storage for sprites/equations)
        initSdCardStorage();
        
        // Initialize SPIFFS as fallback only if SD card not ready
        if (!sdcard_storage_ready_) {
            initSpiffs();
        }
        
        // Load saved sprites and equations from SD card (or SPIFFS fallback)
        loadSpritesFromStorage();
        loadEquationsFromStorage();
        
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 80;
        config.stack_size = 8192;
        config.lru_purge_enable = true;
        config.uri_match_fn = httpd_uri_match_wildcard;
        
        if (httpd_start(&server_, &config) != ESP_OK) {
            ESP_LOGE(HTTP_TAG, "Failed to start HTTP server");
            return false;
        }
        
        registerHandlers();
        
        ESP_LOGI(HTTP_TAG, "HTTP server started on port %d", HTTP_PORT);
        return true;
    }
    
    /**
     * @brief Stop the HTTP server
     */
    void stop() {
        if (server_) {
            httpd_stop(server_);
            server_ = nullptr;
            ESP_LOGI(HTTP_TAG, "HTTP server stopped");
        }
    }
    
    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return server_ != nullptr; }
    
    /**
     * @brief Set command callback
     */
    void setCommandCallback(CommandCallback callback) {
        command_callback_ = callback;
    }
    
    /**
     * @brief Get the httpd handle (for advanced use)
     */
    httpd_handle_t getHandle() const { return server_; }

private:
    HttpServer() = default;
    ~HttpServer() { stop(); }
    
    // Prevent copying
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    
    /**
     * @brief Register all URI handlers
     */
    void registerHandlers() {
        // Login page (always accessible)
        registerHandler("/login", HTTP_GET, handleLoginPage);
        registerHandler("/api/login", HTTP_POST, handleApiLogin);
        registerHandler("/api/logout", HTTP_POST, handleApiLogout);
        
        // Page routes - each tab is a separate page
        registerHandler("/", HTTP_GET, handlePageBasic);
        registerHandler("/system", HTTP_GET, handlePageSystem);
        registerHandler("/advanced", HTTP_GET, handlePageAdvancedMenu);
        registerHandler("/advanced/sprites", HTTP_GET, handlePageSprite);
        registerHandler("/advanced/configs", HTTP_GET, handlePageAdvancedConfigs);
        registerHandler("/sprites", HTTP_GET, handlePageSprite);  // Legacy redirect
        registerHandler("/settings", HTTP_GET, handlePageSettings);
        
        // Static content handlers
        registerHandler("/style.css", HTTP_GET, handleCss);
        
        // API endpoints
        registerHandler("/api/state", HTTP_GET, handleApiState);
        registerHandler("/api/command", HTTP_POST, handleApiCommand);
        registerHandler("/api/scan", HTTP_GET, handleApiScan);
        
        // Sprite API endpoints
        registerHandler("/api/sprites", HTTP_GET, handleApiSprites);
        registerHandler("/api/sprite/save", HTTP_POST, handleApiSpriteSave);
        registerHandler("/api/sprite/rename", HTTP_POST, handleApiSpriteRename);
        registerHandler("/api/sprite/delete", HTTP_POST, handleApiSpriteDelete);
        registerHandler("/api/sprite/apply", HTTP_POST, handleApiSpriteApply);
        registerHandler("/api/storage", HTTP_GET, handleApiStorage);
        
        // Configuration API endpoints
        registerHandler("/api/configs", HTTP_GET, handleApiConfigs);
        registerHandler("/api/config/apply", HTTP_POST, handleApiConfigApply);
        registerHandler("/api/config/save", HTTP_POST, handleApiConfigSave);
        registerHandler("/api/config/create", HTTP_POST, handleApiConfigCreate);
        registerHandler("/api/config/rename", HTTP_POST, handleApiConfigRename);
        registerHandler("/api/config/duplicate", HTTP_POST, handleApiConfigDuplicate);
        registerHandler("/api/config/delete", HTTP_POST, handleApiConfigDelete);
        
        // Equation Editor page and API endpoints
        registerHandler("/advanced/equations", HTTP_GET, handlePageEquations);
        registerHandler("/api/equations", HTTP_GET, handleApiEquations);
        registerHandler("/api/equation/save", HTTP_POST, handleApiEquationSave);
        registerHandler("/api/equation/delete", HTTP_POST, handleApiEquationDelete);
        registerHandler("/api/sensors", HTTP_GET, handleApiSensors);
        
        // IMU Calibration API endpoints
        registerHandler("/api/imu/calibrate", HTTP_POST, handleApiImuCalibrate);
        registerHandler("/api/imu/status", HTTP_GET, handleApiImuStatus);
        registerHandler("/api/imu/clear", HTTP_POST, handleApiImuClear);
        
        // SD Card API endpoints
        registerHandler("/api/sdcard/status", HTTP_GET, handleApiSdCardStatus);
        registerHandler("/api/sdcard/format", HTTP_POST, handleApiSdCardFormat);
        registerHandler("/api/sdcard/clear", HTTP_POST, handleApiSdCardClear);
        registerHandler("/api/sdcard/list", HTTP_GET, handleApiSdCardList);
        
        // Captive portal detection endpoints (comprehensive list for all devices)
        const char* redirect_paths[] = {
            // Android (various versions & OEMs)
            "/generate_204", "/gen_204",
            "/connectivitycheck.gstatic.com",
            "/mobile/status.php",
            "/wifi/test.html",
            "/check_network_status.txt",
            "/connectivitycheck.android.com",
            // Samsung
            "/generate_204_samsung",
            // Huawei/Honor
            "/generate_204_huawei", 
            // Xiaomi
            "/generate_204_xiaomi",
            // Windows
            "/connecttest.txt", "/fwlink", "/redirect",
            "/ncsi.txt", "/connecttest.html",
            "/msftconnecttest.com",
            "/msftncsi.com",
            // Apple iOS/macOS (multiple variants)
            "/library/test/success.html",
            "/hotspot-detect.html",
            "/captive.apple.com",
            "/library/test/success",
            "/hotspot-detect",
            // Amazon Kindle/Fire
            "/kindle-wifi/wifistub.html",
            "/kindle-wifi/test",
            // Firefox
            "/success.txt", "/canonical.html",
            "/detectportal.firefox.com",
            // Generic/Other
            "/chat", "/favicon.ico",
            "/portal.html", "/portal",
            "/login", "/login.html"
        };
        
        for (const char* path : redirect_paths) {
            registerHandler(path, HTTP_GET, handleRedirect);
        }
        
        // Wildcard catch-all (must be last) - handle all HTTP methods
        registerHandler("/*", HTTP_GET, handleCatchAll);
        registerHandler("/*", HTTP_POST, handleCatchAll);
        registerHandler("/*", HTTP_PUT, handleCatchAll);
        registerHandler("/*", HTTP_DELETE, handleCatchAll);
        registerHandler("/*", HTTP_HEAD, handleCatchAll);
    }
    
    /**
     * @brief Helper to register a handler
     */
    void registerHandler(const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t uri_handler = {
            .uri = uri,
            .method = method,
            .handler = handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(server_, &uri_handler);
    }
    
    // ========== SD Card / SPIFFS Storage for Sprites ==========
    
    /**
     * @brief Initialize SD card storage for sprites and equations
     * Primary storage - uses FileSystemService
     */
    static void initSdCardStorage() {
        if (sdcard_storage_ready_) return;
        
        auto& fs = Utils::FileSystemService::instance();
        
        // Check if SD card is ready
        if (!fs.isReady() || !fs.isMounted()) {
            ESP_LOGW(HTTP_TAG, "SD card not available, will use SPIFFS fallback");
            return;
        }
        
        // Create directory structure using StorageManager paths
        struct stat st;
        if (stat(SPRITE_DIR, &st) != 0) {
            fs.createDir(SPRITE_DIR);
            ESP_LOGI(HTTP_TAG, "Created SD card sprites directory");
        }
        if (stat(EQUATION_DIR, &st) != 0) {
            fs.createDir(EQUATION_DIR);
            ESP_LOGI(HTTP_TAG, "Created SD card equations directory");
        }
        
        sdcard_storage_ready_ = true;
        ESP_LOGI(HTTP_TAG, "SD card storage initialized. Total: %llu MB, Free: %llu MB",
            fs.getTotalBytes() / (1024 * 1024),
            fs.getFreeBytes() / (1024 * 1024));
    }
    
    /**
     * @brief Initialize SPIFFS filesystem (fallback storage)
     */
    static void initSpiffs() {
        if (spiffs_initialized_) return;
        
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 10,
            .format_if_mount_failed = true
        };
        
        esp_err_t ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK) {
            if (ret == ESP_FAIL) {
                ESP_LOGE(HTTP_TAG, "Failed to mount SPIFFS");
            } else if (ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGE(HTTP_TAG, "SPIFFS partition not found");
            } else {
                ESP_LOGE(HTTP_TAG, "SPIFFS init failed: %s", esp_err_to_name(ret));
            }
            return;
        }
        
        // Create sprites directory if it doesn't exist (only for SPIFFS fallback)
        struct stat st;
        if (stat(SPRITE_DIR_SPIFFS, &st) != 0) {
            mkdir(SPRITE_DIR_SPIFFS, 0755);
            ESP_LOGI(HTTP_TAG, "Created SPIFFS sprites directory");
        }
        
        spiffs_initialized_ = true;
        
        size_t total = 0, used = 0;
        esp_spiffs_info(NULL, &total, &used);
        ESP_LOGI(HTTP_TAG, "SPIFFS initialized as fallback. Total: %d KB, Used: %d KB", total/1024, used/1024);
    }
    
    /**
     * @brief Get the active sprite index file path
     * Prefers SD card, falls back to SPIFFS
     */
    static const char* getSpriteIndexPath() {
        return sdcard_storage_ready_ ? SPRITE_INDEX_FILE : SPRITE_INDEX_FILE_SPIFFS;
    }
    
    /**
     * @brief Get the active equation index file path
     * Prefers SD card, falls back to SPIFFS
     */
    static const char* getEquationIndexPath() {
        return sdcard_storage_ready_ ? EQUATION_INDEX_FILE : EQUATION_INDEX_FILE_SPIFFS;
    }
    
    /**
     * @brief Save sprite index to storage (SD card preferred)
     */
    static void saveSpritesToStorage() {
        if (!sdcard_storage_ready_ && !spiffs_initialized_) return;
        
        const char* indexPath = getSpriteIndexPath();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "nextId", nextSpriteId_);
        cJSON_AddStringToObject(root, "storage", sdcard_storage_ready_ ? "sdcard" : "spiffs");
        
        cJSON* sprites = cJSON_CreateArray();
        for (const auto& sprite : savedSprites_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", sprite.id);
            cJSON_AddStringToObject(item, "name", sprite.name.c_str());
            cJSON_AddNumberToObject(item, "width", sprite.width);
            cJSON_AddNumberToObject(item, "height", sprite.height);
            cJSON_AddNumberToObject(item, "scale", sprite.scale);
            cJSON_AddStringToObject(item, "preview", sprite.preview.c_str());
            cJSON_AddItemToArray(sprites, item);
        }
        cJSON_AddItemToObject(root, "sprites", sprites);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        if (json) {
            FILE* f = fopen(indexPath, "w");
            if (f) {
                fprintf(f, "%s", json);
                fclose(f);
                ESP_LOGI(HTTP_TAG, "Saved %d sprites to %s", savedSprites_.size(),
                    sdcard_storage_ready_ ? "SD card" : "SPIFFS");
            } else {
                ESP_LOGE(HTTP_TAG, "Failed to open sprite index for writing: %s", indexPath);
            }
            free(json);
        }
    }
    
    /**
     * @brief Load sprite index from storage (SD card or SPIFFS)
     */
    static void loadSpritesFromStorage() {
        if (!sdcard_storage_ready_ && !spiffs_initialized_) return;
        
        const char* indexPath = getSpriteIndexPath();
        
        // Also try to migrate from SPIFFS to SD card if we have SD but data is in SPIFFS
        if (sdcard_storage_ready_) {
            struct stat sdStat, spiffsStat;
            bool hasSpiffsData = (stat(SPRITE_INDEX_FILE_SPIFFS, &spiffsStat) == 0);
            bool hasSDData = (stat(SPRITE_INDEX_FILE, &sdStat) == 0);
            
            // If we have SPIFFS data but no SD data, migrate it
            if (hasSpiffsData && !hasSDData) {
                ESP_LOGI(HTTP_TAG, "Migrating sprites from SPIFFS to SD card...");
                indexPath = SPRITE_INDEX_FILE_SPIFFS;  // Load from SPIFFS
                // After loading we'll save to SD automatically since SD is ready
            }
        }
        
        struct stat st;
        if (stat(indexPath, &st) != 0) {
            ESP_LOGI(HTTP_TAG, "No sprite index found at %s, starting fresh", indexPath);
            return;
        }
        
        FILE* f = fopen(indexPath, "r");
        if (!f) {
            ESP_LOGE(HTTP_TAG, "Failed to open sprite index for reading: %s", indexPath);
            return;
        }
        
        char* buf = (char*)malloc(st.st_size + 1);
        if (!buf) {
            fclose(f);
            return;
        }
        
        size_t bytesRead = fread(buf, 1, st.st_size, f);
        buf[bytesRead] = '\0';
        fclose(f);
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            ESP_LOGE(HTTP_TAG, "Failed to parse sprite index JSON");
            return;
        }
        
        cJSON* nextId = cJSON_GetObjectItem(root, "nextId");
        if (nextId && cJSON_IsNumber(nextId)) {
            nextSpriteId_ = nextId->valueint;
        }
        
        cJSON* sprites = cJSON_GetObjectItem(root, "sprites");
        if (sprites && cJSON_IsArray(sprites)) {
            savedSprites_.clear();
            
            cJSON* item = NULL;
            cJSON_ArrayForEach(item, sprites) {
                SavedSprite sprite;
                
                cJSON* id = cJSON_GetObjectItem(item, "id");
                cJSON* name = cJSON_GetObjectItem(item, "name");
                cJSON* width = cJSON_GetObjectItem(item, "width");
                cJSON* height = cJSON_GetObjectItem(item, "height");
                cJSON* scale = cJSON_GetObjectItem(item, "scale");
                cJSON* preview = cJSON_GetObjectItem(item, "preview");
                
                if (id && cJSON_IsNumber(id)) sprite.id = id->valueint;
                if (name && cJSON_IsString(name)) sprite.name = name->valuestring;
                if (width && cJSON_IsNumber(width)) sprite.width = width->valueint;
                if (height && cJSON_IsNumber(height)) sprite.height = height->valueint;
                if (scale && cJSON_IsNumber(scale)) sprite.scale = scale->valueint;
                if (preview && cJSON_IsString(preview)) sprite.preview = preview->valuestring;
                
                savedSprites_.push_back(sprite);
            }
            
            ESP_LOGI(HTTP_TAG, "Loaded %d sprites from %s", savedSprites_.size(),
                sdcard_storage_ready_ ? "SD card" : "SPIFFS");
                
            // If we loaded from SPIFFS but SD is now ready, migrate to SD
            if (sdcard_storage_ready_ && strcmp(indexPath, SPRITE_INDEX_FILE_SPIFFS) == 0) {
                ESP_LOGI(HTTP_TAG, "Saving sprites to SD card after migration");
                saveSpritesToStorage();
            }
        }
        
        cJSON_Delete(root);
    }
    
    /**
     * @brief Save equations to storage (SD card preferred)
     */
    static void saveEquationsToStorage() {
        if (!sdcard_storage_ready_ && !spiffs_initialized_) return;
        
        const char* indexPath = getEquationIndexPath();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "nextId", nextEquationId_);
        cJSON_AddStringToObject(root, "storage", sdcard_storage_ready_ ? "sdcard" : "spiffs");
        
        cJSON* equations = cJSON_CreateArray();
        for (const auto& eq : savedEquations_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", eq.id);
            cJSON_AddStringToObject(item, "name", eq.name.c_str());
            cJSON_AddStringToObject(item, "expression", eq.expression.c_str());
            
            cJSON* vars = cJSON_CreateArray();
            for (const auto& v : eq.variables) {
                cJSON* varItem = cJSON_CreateObject();
                cJSON_AddStringToObject(varItem, "name", v.name.c_str());
                cJSON_AddStringToObject(varItem, "type", v.type.c_str());
                cJSON_AddStringToObject(varItem, "value", v.value.c_str());
                cJSON_AddItemToArray(vars, varItem);
            }
            cJSON_AddItemToObject(item, "variables", vars);
            
            cJSON_AddItemToArray(equations, item);
        }
        cJSON_AddItemToObject(root, "equations", equations);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        if (json) {
            FILE* f = fopen(indexPath, "w");
            if (f) {
                fprintf(f, "%s", json);
                fclose(f);
                ESP_LOGI(HTTP_TAG, "Saved %d equations to %s", savedEquations_.size(),
                    sdcard_storage_ready_ ? "SD card" : "SPIFFS");
            } else {
                ESP_LOGE(HTTP_TAG, "Failed to open equation index for writing: %s", indexPath);
            }
            free(json);
        }
    }
    
    /**
     * @brief Load equations from storage (SD card or SPIFFS)
     */
    static void loadEquationsFromStorage() {
        if (!sdcard_storage_ready_ && !spiffs_initialized_) return;
        
        const char* indexPath = getEquationIndexPath();
        
        // Also try to migrate from SPIFFS to SD card if we have SD but data is in SPIFFS
        if (sdcard_storage_ready_) {
            struct stat sdStat, spiffsStat;
            bool hasSpiffsData = (stat(EQUATION_INDEX_FILE_SPIFFS, &spiffsStat) == 0);
            bool hasSDData = (stat(EQUATION_INDEX_FILE, &sdStat) == 0);
            
            // If we have SPIFFS data but no SD data, migrate it
            if (hasSpiffsData && !hasSDData) {
                ESP_LOGI(HTTP_TAG, "Migrating equations from SPIFFS to SD card...");
                indexPath = EQUATION_INDEX_FILE_SPIFFS;  // Load from SPIFFS
            }
        }
        
        struct stat st;
        if (stat(indexPath, &st) != 0) {
            ESP_LOGI(HTTP_TAG, "No equation index found at %s, starting fresh", indexPath);
            return;
        }
        
        FILE* f = fopen(indexPath, "r");
        if (!f) {
            ESP_LOGE(HTTP_TAG, "Failed to open equation index for reading: %s", indexPath);
            return;
        }
        
        char* buf = (char*)malloc(st.st_size + 1);
        if (!buf) {
            fclose(f);
            return;
        }
        
        size_t bytesRead = fread(buf, 1, st.st_size, f);
        buf[bytesRead] = '\0';
        fclose(f);
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            ESP_LOGE(HTTP_TAG, "Failed to parse equation index JSON");
            return;
        }
        
        cJSON* nextId = cJSON_GetObjectItem(root, "nextId");
        if (nextId && cJSON_IsNumber(nextId)) {
            nextEquationId_ = nextId->valueint;
        }
        
        cJSON* equations = cJSON_GetObjectItem(root, "equations");
        if (equations && cJSON_IsArray(equations)) {
            savedEquations_.clear();
            
            cJSON* item = NULL;
            cJSON_ArrayForEach(item, equations) {
                SavedEquation eq;
                
                cJSON* id = cJSON_GetObjectItem(item, "id");
                cJSON* name = cJSON_GetObjectItem(item, "name");
                cJSON* expression = cJSON_GetObjectItem(item, "expression");
                cJSON* variables = cJSON_GetObjectItem(item, "variables");
                
                if (id && cJSON_IsNumber(id)) eq.id = id->valueint;
                if (name && cJSON_IsString(name)) eq.name = name->valuestring;
                if (expression && cJSON_IsString(expression)) eq.expression = expression->valuestring;
                
                if (variables && cJSON_IsArray(variables)) {
                    cJSON* varItem = NULL;
                    cJSON_ArrayForEach(varItem, variables) {
                        EquationVariable v;
                        cJSON* vname = cJSON_GetObjectItem(varItem, "name");
                        cJSON* vtype = cJSON_GetObjectItem(varItem, "type");
                        cJSON* vvalue = cJSON_GetObjectItem(varItem, "value");
                        
                        if (vname && cJSON_IsString(vname)) v.name = vname->valuestring;
                        if (vtype && cJSON_IsString(vtype)) v.type = vtype->valuestring;
                        if (vvalue && cJSON_IsString(vvalue)) v.value = vvalue->valuestring;
                        
                        eq.variables.push_back(v);
                    }
                }
                
                savedEquations_.push_back(eq);
            }
            
            ESP_LOGI(HTTP_TAG, "Loaded %d equations from %s", savedEquations_.size(),
                sdcard_storage_ready_ ? "SD card" : "SPIFFS");
            
            // If we loaded from SPIFFS but SD is now ready, migrate to SD
            if (sdcard_storage_ready_ && strcmp(indexPath, EQUATION_INDEX_FILE_SPIFFS) == 0) {
                ESP_LOGI(HTTP_TAG, "Saving equations to SD card after migration");
                saveEquationsToStorage();
            }
        }
        
        cJSON_Delete(root);
    }
    
    // ========== Authentication Helpers ==========
    
    /**
     * @brief Check if request is coming from external network (not direct AP)
     * 
     * If the device is connected to an external network (APSTA mode),
     * requests can come either from:
     * - AP clients (192.168.4.x) - these are direct device connections
     * - STA network (the external network IP range) - these need auth
     */
    static bool isExternalNetworkRequest(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // If external WiFi is not connected, all requests are from AP
        if (!state.extWifiIsConnected) {
            return false;
        }
        
        // Get client IP from the request
        int sockfd = httpd_req_to_sockfd(req);
        struct sockaddr_in6 addr;
        socklen_t addr_len = sizeof(addr);
        
        if (getpeername(sockfd, (struct sockaddr*)&addr, &addr_len) != 0) {
            return false;  // Can't determine, assume safe
        }
        
        // Convert to IPv4 if mapped
        uint32_t client_ip = 0;
        if (addr.sin6_family == AF_INET) {
            client_ip = ((struct sockaddr_in*)&addr)->sin_addr.s_addr;
        } else if (addr.sin6_family == AF_INET6) {
            // Check for IPv4-mapped address
            if (IN6_IS_ADDR_V4MAPPED(&addr.sin6_addr)) {
                memcpy(&client_ip, &addr.sin6_addr.s6_addr[12], 4);
            }
        }
        
        // AP subnet is 192.168.4.0/24 = 0xC0A80400
        // Mask: 255.255.255.0 = 0xFFFFFF00
        uint32_t ap_network = 0x0404A8C0;  // 192.168.4.0 in little-endian
        uint32_t ap_mask = 0x00FFFFFF;     // 255.255.255.0 in little-endian
        
        // If client is on AP subnet, it's a direct connection
        if ((client_ip & ap_mask) == (ap_network & ap_mask)) {
            ESP_LOGD(HTTP_TAG, "Request from AP client (direct connection)");
            return false;
        }
        
        // Otherwise, it's from the external network
        ESP_LOGI(HTTP_TAG, "Request from external network client");
        return true;
    }
    
    /**
     * @brief Check if request has valid authentication token (cookie)
     */
    static bool isAuthenticated(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // If auth not enabled, everyone is authenticated
        if (!state.authEnabled || strlen(state.authPassword) == 0) {
            return true;
        }
        
        // Get cookie header
        char cookie[128] = {0};
        if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) {
            return false;
        }
        
        // Look for auth token in cookie
        // Format: auth_token=<token>
        char* token_start = strstr(cookie, "auth_token=");
        if (!token_start) {
            return false;
        }
        
        token_start += 11;  // Skip "auth_token="
        char* token_end = strchr(token_start, ';');
        
        char token[65] = {0};
        size_t token_len = token_end ? (token_end - token_start) : strlen(token_start);
        if (token_len >= sizeof(token)) token_len = sizeof(token) - 1;
        strncpy(token, token_start, token_len);
        
        // Compare with stored session token
        return strcmp(token, state.authSessionToken) == 0 && strlen(token) > 0;
    }
    
    /**
     * @brief Check if auth is required and redirect to login if needed
     * @return true if request should be blocked (redirected to login)
     */
    static bool requiresAuthRedirect(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // Auth only required when:
        // 1. External WiFi is connected
        // 2. Auth is enabled
        // 3. Request is from external network
        // 4. User is not authenticated
        
        if (!state.extWifiIsConnected) return false;
        if (!state.authEnabled) return false;
        if (strlen(state.authPassword) == 0) return false;
        if (!isExternalNetworkRequest(req)) return false;
        if (isAuthenticated(req)) return false;
        
        return true;
    }
    
    /**
     * @brief Redirect to login page
     */
    static esp_err_t redirectToLogin(httpd_req_t* req) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    // ========== Login Page ==========
    
    static const char* getLoginPage() {
        return R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Login - Lucidius</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: #0a0a0a; color: #fff; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .login-container { background: #141414; border-radius: 16px; padding: 40px; width: 100%; max-width: 400px; margin: 20px; border: 1px solid #222; }
    h1 { text-align: center; margin-bottom: 8px; color: #ff6b00; }
    .subtitle { text-align: center; color: #888; margin-bottom: 32px; font-size: 14px; }
    .warning { background: rgba(255, 59, 48, 0.1); border: 1px solid rgba(255, 59, 48, 0.3); border-radius: 8px; padding: 12px 16px; margin-bottom: 24px; color: #ff6b6b; font-size: 13px; text-align: center; }
    .form-group { margin-bottom: 20px; }
    label { display: block; color: #888; font-size: 13px; margin-bottom: 8px; }
    input { width: 100%; padding: 14px 16px; background: #1a1a1a; border: 1px solid #333; border-radius: 8px; color: #fff; font-size: 16px; transition: border-color 0.2s; }
    input:focus { outline: none; border-color: #ff6b00; }
    .btn { width: 100%; padding: 14px; background: linear-gradient(135deg, #ff6b00, #ff8533); color: #fff; border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; transition: transform 0.2s, box-shadow 0.2s; }
    .btn:hover { transform: translateY(-2px); box-shadow: 0 4px 20px rgba(255, 107, 0, 0.3); }
    .btn:active { transform: translateY(0); }
    .error { color: #ff6b6b; font-size: 13px; margin-top: 16px; text-align: center; display: none; }
    .error.show { display: block; }
  </style>
</head>
<body>
  <div class="login-container">
    <h1>Lucidius</h1>
    <p class="subtitle">External Network Access</p>
    <div class="warning">
      You are connecting via an external network.<br>
      Authentication is required for security.
    </div>
    <form id="login-form">
      <div class="form-group">
        <label for="username">Username</label>
        <input type="text" id="username" name="username" autocomplete="username" required>
      </div>
      <div class="form-group">
        <label for="password">Password</label>
        <input type="password" id="password" name="password" autocomplete="current-password" required>
      </div>
      <button type="submit" class="btn">Log In</button>
      <p class="error" id="error-msg">Invalid username or password</p>
    </form>
  </div>
  <script>
    document.getElementById('login-form').addEventListener('submit', function(e) {
      e.preventDefault();
      var username = document.getElementById('username').value;
      var password = document.getElementById('password').value;
      
      fetch('/api/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username: username, password: password })
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          window.location.href = '/';
        } else {
          document.getElementById('error-msg').classList.add('show');
        }
      })
      .catch(err => {
        document.getElementById('error-msg').textContent = 'Connection error';
        document.getElementById('error-msg').classList.add('show');
      });
    });
  </script>
</body>
</html>)rawliteral";
    }
    
    static esp_err_t handleLoginPage(httpd_req_t* req) {
        // If already authenticated or not from external network, redirect to home
        auto& state = SYNC_STATE.state();
        if (!state.extWifiIsConnected || !state.authEnabled || 
            !isExternalNetworkRequest(req) || isAuthenticated(req)) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "/");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, getLoginPage(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleApiLogin(httpd_req_t* req) {
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* username = cJSON_GetObjectItem(root, "username");
        cJSON* password = cJSON_GetObjectItem(root, "password");
        
        auto& state = SYNC_STATE.state();
        bool success = false;
        
        if (username && password && username->valuestring && password->valuestring) {
            // Check credentials
            if (strcmp(username->valuestring, state.authUsername) == 0 &&
                strcmp(password->valuestring, state.authPassword) == 0) {
                
                // Generate session token
                uint32_t r1 = esp_random();
                uint32_t r2 = esp_random();
                uint32_t r3 = esp_random();
                uint32_t r4 = esp_random();
                snprintf(state.authSessionToken, sizeof(state.authSessionToken),
                        "%08lx%08lx%08lx%08lx", 
                        (unsigned long)r1, (unsigned long)r2, 
                        (unsigned long)r3, (unsigned long)r4);
                
                success = true;
                ESP_LOGI(HTTP_TAG, "Login successful for user: %s", state.authUsername);
            } else {
                ESP_LOGW(HTTP_TAG, "Login failed for user: %s", username->valuestring);
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        
        if (success) {
            // Set auth cookie
            char cookie[128];
            snprintf(cookie, sizeof(cookie), "auth_token=%s; Path=/; HttpOnly; SameSite=Strict",
                    state.authSessionToken);
            httpd_resp_set_hdr(req, "Set-Cookie", cookie);
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        } else {
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid credentials\"}", HTTPD_RESP_USE_STRLEN);
        }
        
        return ESP_OK;
    }
    
    static esp_err_t handleApiLogout(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // Clear session token
        memset(state.authSessionToken, 0, sizeof(state.authSessionToken));
        
        // Clear cookie
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Set-Cookie", "auth_token=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "User logged out");
        return ESP_OK;
    }
    
    // ========== Page Handlers ==========
    
    static esp_err_t handlePageBasic(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Basic page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_BASIC, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSystem(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving System page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SYSTEM, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageAdvancedMenu(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Advanced Menu page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_ADVANCED_MENU, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageAdvancedConfigs(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Advanced Configs page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_ADVANCED, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSprite(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Sprite page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SPRITE, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageEquations(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Equations page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_EQUATIONS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSettings(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Settings page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SETTINGS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Static Content Handlers ==========
    
    static esp_err_t handleCss(httpd_req_t* req) {
        httpd_resp_set_type(req, "text/css");
        httpd_resp_send(req, Content::STYLE_CSS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Utility Functions ==========
    
    /**
     * @brief Decode base64 string to binary data
     * @param input Base64 encoded string
     * @param output Output buffer
     * @param maxOutputLen Maximum output buffer size
     * @param outputLen Actual decoded length
     * @return true on success
     */
    static bool decodeBase64(const char* input, uint8_t* output, size_t maxOutputLen, size_t* outputLen) {
        static const uint8_t b64table[256] = {
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        };
        
        size_t inputLen = strlen(input);
        if (inputLen == 0) {
            *outputLen = 0;
            return true;
        }
        
        // Remove padding from length calculation
        size_t padding = 0;
        if (inputLen >= 1 && input[inputLen - 1] == '=') padding++;
        if (inputLen >= 2 && input[inputLen - 2] == '=') padding++;
        
        size_t expectedLen = (inputLen * 3) / 4 - padding;
        if (expectedLen > maxOutputLen) {
            return false;
        }
        
        size_t outIdx = 0;
        uint32_t buf = 0;
        int bits = 0;
        
        for (size_t i = 0; i < inputLen; i++) {
            uint8_t c = (uint8_t)input[i];
            if (c == '=') break;
            
            uint8_t v = b64table[c];
            if (v == 64) continue; // Skip invalid chars
            
            buf = (buf << 6) | v;
            bits += 6;
            
            if (bits >= 8) {
                bits -= 8;
                if (outIdx < maxOutputLen) {
                    output[outIdx++] = (buf >> bits) & 0xFF;
                }
            }
        }
        
        *outputLen = outIdx;
        return true;
    }
    
    // ========== API Handlers ==========
    
    /**
     * @brief Return 401 Unauthorized for API requests
     */
    static esp_err_t sendUnauthorized(httpd_req_t* req) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"Unauthorized\",\"login_required\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleApiState(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "ssid", state.ssid);
        cJSON_AddStringToObject(root, "ip", state.ipAddress);
        cJSON_AddNumberToObject(root, "clients", state.wifiClients);
        cJSON_AddNumberToObject(root, "uptime", state.uptime);
        cJSON_AddNumberToObject(root, "freeHeap", state.freeHeap);
        cJSON_AddNumberToObject(root, "brightness", state.brightness);
        cJSON_AddNumberToObject(root, "cpuUsage", state.cpuUsage);
        cJSON_AddNumberToObject(root, "fps", state.fps);
        
        // Sensor data
        cJSON* sensors = cJSON_CreateObject();
        cJSON_AddNumberToObject(sensors, "temperature", state.temperature);
        cJSON_AddNumberToObject(sensors, "humidity", state.humidity);
        cJSON_AddNumberToObject(sensors, "pressure", state.pressure);
        cJSON_AddItemToObject(root, "sensors", sensors);
        
        // IMU data
        cJSON* imu = cJSON_CreateObject();
        cJSON_AddNumberToObject(imu, "accelX", state.accelX);
        cJSON_AddNumberToObject(imu, "accelY", state.accelY);
        cJSON_AddNumberToObject(imu, "accelZ", state.accelZ);
        cJSON_AddNumberToObject(imu, "gyroX", state.gyroX);
        cJSON_AddNumberToObject(imu, "gyroY", state.gyroY);
        cJSON_AddNumberToObject(imu, "gyroZ", state.gyroZ);
        cJSON_AddItemToObject(root, "imu", imu);
        
        // GPS data
        cJSON* gps = cJSON_CreateObject();
        cJSON_AddNumberToObject(gps, "latitude", state.latitude);
        cJSON_AddNumberToObject(gps, "longitude", state.longitude);
        cJSON_AddNumberToObject(gps, "altitude", state.altitude);
        cJSON_AddNumberToObject(gps, "satellites", state.satellites);
        cJSON_AddBoolToObject(gps, "valid", state.gpsValid);
        cJSON_AddNumberToObject(gps, "speed", state.gpsSpeed);
        cJSON_AddNumberToObject(gps, "heading", state.gpsHeading);
        cJSON_AddNumberToObject(gps, "hdop", state.gpsHdop);
        
        // GPS Time as formatted string
        char timeStr[20];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                 state.gpsHour, state.gpsMinute, state.gpsSecond);
        cJSON_AddStringToObject(gps, "time", timeStr);
        
        char dateStr[16];
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", 
                 state.gpsYear, state.gpsMonth, state.gpsDay);
        cJSON_AddStringToObject(gps, "date", dateStr);
        cJSON_AddItemToObject(root, "gps", gps);
        
        // Connection status
        cJSON_AddBoolToObject(root, "gpuConnected", state.gpuConnected);
        
        // GPU stats
        cJSON* gpuStats = cJSON_CreateObject();
        cJSON_AddNumberToObject(gpuStats, "fps", state.gpuFps);
        cJSON_AddNumberToObject(gpuStats, "freeHeap", state.gpuFreeHeap);
        cJSON_AddNumberToObject(gpuStats, "minHeap", state.gpuMinHeap);
        cJSON_AddNumberToObject(gpuStats, "load", state.gpuLoad);
        cJSON_AddNumberToObject(gpuStats, "totalFrames", state.gpuTotalFrames);
        cJSON_AddNumberToObject(gpuStats, "uptime", state.gpuUptime);
        cJSON_AddBoolToObject(gpuStats, "hub75Ok", state.gpuHub75Ok);
        cJSON_AddBoolToObject(gpuStats, "oledOk", state.gpuOledOk);
        cJSON_AddItemToObject(root, "gpu", gpuStats);
        
        // Microphone
        cJSON_AddNumberToObject(root, "mic", state.micLevel);
        cJSON_AddBoolToObject(root, "micConnected", state.micConnected);
        cJSON_AddNumberToObject(root, "micDb", state.micDb);
        
        // System mode
        const char* modeStr = "idle";
        switch (state.mode) {
            case SystemMode::RUNNING: modeStr = "running"; break;
            case SystemMode::PAUSED: modeStr = "paused"; break;
            case SystemMode::ERROR: modeStr = "error"; break;
            default: modeStr = "idle"; break;
        }
        cJSON_AddStringToObject(root, "mode", modeStr);
        cJSON_AddStringToObject(root, "statusText", state.statusText);
        
        // External WiFi state
        cJSON_AddBoolToObject(root, "extWifiEnabled", state.extWifiEnabled);
        cJSON_AddBoolToObject(root, "extWifiConnected", state.extWifiConnected);
        cJSON_AddBoolToObject(root, "extWifiIsConnected", state.extWifiIsConnected);
        cJSON_AddStringToObject(root, "extWifiSSID", state.extWifiSSID);
        cJSON_AddStringToObject(root, "extWifiIP", state.extWifiIP);
        cJSON_AddNumberToObject(root, "extWifiRSSI", state.extWifiRSSI);
        
        // Authentication state (don't send password!)
        cJSON_AddBoolToObject(root, "authEnabled", state.authEnabled);
        cJSON_AddStringToObject(root, "authUsername", state.authUsername);
        
        char* json = cJSON_PrintUnformatted(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        free(json);
        cJSON_Delete(root);
        
        return ESP_OK;
    }
    
    static esp_err_t handleApiCommand(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        // Read body
        char buf[HTTP_BUFFER_SIZE];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* cmd = cJSON_GetObjectItem(root, "cmd");
        if (cmd && cmd->valuestring) {
            CommandType type = stringToCommand(cmd->valuestring);
            self->processCommand(type, root);
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        
        return ESP_OK;
    }
    
    /**
     * @brief Handle WiFi network scanning
     */
    static esp_err_t handleApiScan(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "Starting WiFi scan...");
        
        // Get current WiFi mode
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        
        // Ensure STA interface exists for scanning
        bool wasAPOnly = (mode == WIFI_MODE_AP);
        if (wasAPOnly) {
            ESP_LOGI(HTTP_TAG, "Switching to APSTA mode for scan");
            
            // Create STA netif if not exists
            esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (!sta_netif) {
                sta_netif = esp_netif_create_default_wifi_sta();
                ESP_LOGI(HTTP_TAG, "Created STA netif for scanning");
            }
            
            esp_wifi_set_mode(WIFI_MODE_APSTA);
            vTaskDelay(pdMS_TO_TICKS(200)); // Longer delay for mode switch
        }
        
        // Configure scan - use passive scan which works better in APSTA mode
        wifi_scan_config_t scan_config = {
            .ssid = nullptr,
            .bssid = nullptr,
            .channel = 0,
            .show_hidden = false,
            .scan_type = WIFI_SCAN_TYPE_PASSIVE,
            .scan_time = {
                .passive = 200
            }
        };
        
        // Start scan (blocking)
        esp_err_t err = esp_wifi_scan_start(&scan_config, true);
        if (err != ESP_OK) {
            ESP_LOGE(HTTP_TAG, "WiFi scan failed: %s", esp_err_to_name(err));
            // Restore AP-only mode if we switched
            if (wasAPOnly) {
                esp_wifi_set_mode(WIFI_MODE_AP);
            }
            httpd_resp_set_type(req, "application/json");
            char errJson[100];
            snprintf(errJson, sizeof(errJson), "{\"networks\":[], \"error\":\"Scan failed: %s\"}", esp_err_to_name(err));
            httpd_resp_send(req, errJson, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get scan results
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        
        if (ap_count == 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"networks\":[]}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Limit to reasonable number
        if (ap_count > 20) ap_count = 20;
        
        wifi_ap_record_t* ap_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (!ap_records) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
            return ESP_FAIL;
        }
        
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        
        // Build JSON response
        cJSON* root = cJSON_CreateObject();
        cJSON* networks = cJSON_CreateArray();
        
        for (int i = 0; i < ap_count; i++) {
            // Skip networks with empty SSID
            if (strlen((char*)ap_records[i].ssid) == 0) continue;
            
            cJSON* net = cJSON_CreateObject();
            cJSON_AddStringToObject(net, "ssid", (char*)ap_records[i].ssid);
            cJSON_AddNumberToObject(net, "rssi", ap_records[i].rssi);
            cJSON_AddNumberToObject(net, "channel", ap_records[i].primary);
            cJSON_AddBoolToObject(net, "secure", ap_records[i].authmode != WIFI_AUTH_OPEN);
            
            // Auth mode string
            const char* authStr = "Unknown";
            switch (ap_records[i].authmode) {
                case WIFI_AUTH_OPEN: authStr = "Open"; break;
                case WIFI_AUTH_WEP: authStr = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: authStr = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: authStr = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: authStr = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA3_PSK: authStr = "WPA3"; break;
                case WIFI_AUTH_WPA2_WPA3_PSK: authStr = "WPA2/WPA3"; break;
                default: authStr = "Enterprise"; break;
            }
            cJSON_AddStringToObject(net, "auth", authStr);
            
            cJSON_AddItemToArray(networks, net);
        }
        
        cJSON_AddItemToObject(root, "networks", networks);
        
        free(ap_records);
        
        char* json = cJSON_PrintUnformatted(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        free(json);
        cJSON_Delete(root);
        
        ESP_LOGI(HTTP_TAG, "WiFi scan complete, found %d networks", ap_count);
        return ESP_OK;
    }
    
    // ========== Configuration API Handlers ==========
    
    /**
     * @brief Get all animation configurations
     */
    static esp_err_t handleApiConfigs(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        auto& mgr = self->animConfigManager_;
        
        cJSON* root = cJSON_CreateObject();
        cJSON* configs = cJSON_CreateArray();
        
        for (int i = 0; i < mgr.getConfigCount(); i++) {
            const Animation::AnimationConfiguration* cfg = mgr.getConfig(i);
            if (!cfg) continue;
            
            cJSON* item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "name", cfg->name);
            cJSON_AddNumberToObject(item, "index", i);
            cJSON_AddNumberToObject(item, "target", static_cast<int>(cfg->target));
            
            // Display config
            cJSON* display = cJSON_CreateObject();
            cJSON_AddNumberToObject(display, "animation", static_cast<int>(cfg->display.animation));
            cJSON_AddNumberToObject(display, "speed", cfg->display.speed);
            cJSON_AddNumberToObject(display, "brightness", cfg->display.brightness);
            cJSON* color1d = cJSON_CreateObject();
            cJSON_AddNumberToObject(color1d, "r", cfg->display.color1_r);
            cJSON_AddNumberToObject(color1d, "g", cfg->display.color1_g);
            cJSON_AddNumberToObject(color1d, "b", cfg->display.color1_b);
            cJSON_AddItemToObject(display, "color1", color1d);
            cJSON* color2d = cJSON_CreateObject();
            cJSON_AddNumberToObject(color2d, "r", cfg->display.color2_r);
            cJSON_AddNumberToObject(color2d, "g", cfg->display.color2_g);
            cJSON_AddNumberToObject(color2d, "b", cfg->display.color2_b);
            cJSON_AddItemToObject(display, "color2", color2d);
            cJSON_AddItemToObject(item, "display", display);
            
            // LED config
            cJSON* leds = cJSON_CreateObject();
            cJSON_AddNumberToObject(leds, "animation", static_cast<int>(cfg->leds.animation));
            cJSON_AddNumberToObject(leds, "speed", cfg->leds.speed);
            cJSON_AddNumberToObject(leds, "brightness", cfg->leds.brightness);
            cJSON* color1l = cJSON_CreateObject();
            cJSON_AddNumberToObject(color1l, "r", cfg->leds.color1_r);
            cJSON_AddNumberToObject(color1l, "g", cfg->leds.color1_g);
            cJSON_AddNumberToObject(color1l, "b", cfg->leds.color1_b);
            cJSON_AddItemToObject(leds, "color1", color1l);
            cJSON* color2l = cJSON_CreateObject();
            cJSON_AddNumberToObject(color2l, "r", cfg->leds.color2_r);
            cJSON_AddNumberToObject(color2l, "g", cfg->leds.color2_g);
            cJSON_AddNumberToObject(color2l, "b", cfg->leds.color2_b);
            cJSON_AddItemToObject(leds, "color2", color2l);
            cJSON_AddItemToObject(item, "leds", leds);
            
            cJSON_AddItemToArray(configs, item);
        }
        
        cJSON_AddItemToObject(root, "configs", configs);
        cJSON_AddNumberToObject(root, "activeDisplay", mgr.getActiveDisplayConfig());
        cJSON_AddNumberToObject(root, "activeLeds", mgr.getActiveLedConfig());
        
        char* json = cJSON_PrintUnformatted(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        free(json);
        cJSON_Delete(root);
        return ESP_OK;
    }
    
    /**
     * @brief Apply a configuration
     */
    static esp_err_t handleApiConfigApply(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        if (!index || !cJSON_IsNumber(index)) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Missing index\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        int applied = self->animConfigManager_.applyConfig(index->valueint);
        cJSON_Delete(root);
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"applied\":%d}", applied);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "Applied config %d, result: %d", index->valueint, applied);
        return ESP_OK;
    }
    
    /**
     * @brief Save configuration changes
     */
    static esp_err_t handleApiConfigSave(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[1024];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        if (!index || !cJSON_IsNumber(index)) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Missing index\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        Animation::AnimationConfiguration* cfg = self->animConfigManager_.getConfig(index->valueint);
        if (!cfg) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Config not found\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Update name
        cJSON* name = cJSON_GetObjectItem(root, "name");
        if (name && name->valuestring) {
            cfg->setName(name->valuestring);
        }
        
        // Update target
        cJSON* target = cJSON_GetObjectItem(root, "target");
        if (target && cJSON_IsNumber(target)) {
            cfg->target = static_cast<Animation::ConfigTarget>(target->valueint);
        }
        
        // Update display config
        cJSON* display = cJSON_GetObjectItem(root, "display");
        if (display) {
            cJSON* anim = cJSON_GetObjectItem(display, "animation");
            if (anim) cfg->display.animation = static_cast<Animation::DisplayAnimation>(anim->valueint);
            
            cJSON* speed = cJSON_GetObjectItem(display, "speed");
            if (speed) cfg->display.speed = speed->valueint;
            
            cJSON* brightness = cJSON_GetObjectItem(display, "brightness");
            if (brightness) cfg->display.brightness = brightness->valueint;
            
            cJSON* color1 = cJSON_GetObjectItem(display, "color1");
            if (color1) {
                cJSON* r = cJSON_GetObjectItem(color1, "r");
                cJSON* g = cJSON_GetObjectItem(color1, "g");
                cJSON* b = cJSON_GetObjectItem(color1, "b");
                if (r) cfg->display.color1_r = r->valueint;
                if (g) cfg->display.color1_g = g->valueint;
                if (b) cfg->display.color1_b = b->valueint;
            }
            
            cJSON* color2 = cJSON_GetObjectItem(display, "color2");
            if (color2) {
                cJSON* r = cJSON_GetObjectItem(color2, "r");
                cJSON* g = cJSON_GetObjectItem(color2, "g");
                cJSON* b = cJSON_GetObjectItem(color2, "b");
                if (r) cfg->display.color2_r = r->valueint;
                if (g) cfg->display.color2_g = g->valueint;
                if (b) cfg->display.color2_b = b->valueint;
            }
        }
        
        // Update LED config
        cJSON* leds = cJSON_GetObjectItem(root, "leds");
        if (leds) {
            cJSON* anim = cJSON_GetObjectItem(leds, "animation");
            if (anim) cfg->leds.animation = static_cast<Animation::LedAnimation>(anim->valueint);
            
            cJSON* speed = cJSON_GetObjectItem(leds, "speed");
            if (speed) cfg->leds.speed = speed->valueint;
            
            cJSON* brightness = cJSON_GetObjectItem(leds, "brightness");
            if (brightness) cfg->leds.brightness = brightness->valueint;
            
            cJSON* color1 = cJSON_GetObjectItem(leds, "color1");
            if (color1) {
                cJSON* r = cJSON_GetObjectItem(color1, "r");
                cJSON* g = cJSON_GetObjectItem(color1, "g");
                cJSON* b = cJSON_GetObjectItem(color1, "b");
                if (r) cfg->leds.color1_r = r->valueint;
                if (g) cfg->leds.color1_g = g->valueint;
                if (b) cfg->leds.color1_b = b->valueint;
            }
            
            cJSON* color2 = cJSON_GetObjectItem(leds, "color2");
            if (color2) {
                cJSON* r = cJSON_GetObjectItem(color2, "r");
                cJSON* g = cJSON_GetObjectItem(color2, "g");
                cJSON* b = cJSON_GetObjectItem(color2, "b");
                if (r) cfg->leds.color2_r = r->valueint;
                if (g) cfg->leds.color2_g = g->valueint;
                if (b) cfg->leds.color2_b = b->valueint;
            }
        }
        
        // Check if we should also apply
        cJSON* apply = cJSON_GetObjectItem(root, "apply");
        int applied = 0;
        if (apply && cJSON_IsTrue(apply)) {
            applied = self->animConfigManager_.applyConfig(index->valueint);
        }
        
        cJSON_Delete(root);
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"applied\":%d}", applied);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "Saved config %d", index->valueint);
        return ESP_OK;
    }
    
    /**
     * @brief Create a new configuration
     */
    static esp_err_t handleApiConfigCreate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* name = cJSON_GetObjectItem(root, "name");
        const char* configName = (name && name->valuestring) ? name->valuestring : "New Configuration";
        
        int newIndex = self->animConfigManager_.createConfig(configName, Animation::ConfigTarget::BOTH);
        cJSON_Delete(root);
        
        if (newIndex < 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Max configs reached\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"index\":%d}", newIndex);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "Created config '%s' at index %d", configName, newIndex);
        return ESP_OK;
    }
    
    /**
     * @brief Rename a configuration
     */
    static esp_err_t handleApiConfigRename(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        cJSON* name = cJSON_GetObjectItem(root, "name");
        
        bool success = false;
        if (index && cJSON_IsNumber(index) && name && name->valuestring) {
            success = self->animConfigManager_.renameConfig(index->valueint, name->valuestring);
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Duplicate a configuration
     */
    static esp_err_t handleApiConfigDuplicate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        int newIndex = -1;
        
        if (index && cJSON_IsNumber(index)) {
            newIndex = self->animConfigManager_.duplicateConfig(index->valueint);
        }
        
        cJSON_Delete(root);
        
        if (newIndex < 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Failed to duplicate\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"index\":%d}", newIndex);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Delete a configuration
     */
    static esp_err_t handleApiConfigDelete(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        bool success = false;
        
        if (index && cJSON_IsNumber(index)) {
            success = self->animConfigManager_.deleteConfig(index->valueint);
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Sprite API Handlers ==========
    
    /**
     * @brief Get list of saved sprites
     */
    static esp_err_t handleApiSprites(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        cJSON* root = cJSON_CreateObject();
        cJSON* sprites = cJSON_CreateArray();
        
        for (const auto& sprite : savedSprites_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", sprite.id);
            cJSON_AddStringToObject(item, "name", sprite.name.c_str());
            cJSON_AddNumberToObject(item, "width", sprite.width);
            cJSON_AddNumberToObject(item, "height", sprite.height);
            cJSON_AddNumberToObject(item, "scale", sprite.scale);
            // Calculate size in bytes (RGB888)
            int sizeBytes = sprite.width * sprite.height * 3;
            cJSON_AddNumberToObject(item, "sizeBytes", sizeBytes);
            cJSON_AddStringToObject(item, "preview", sprite.preview.c_str());
            cJSON_AddItemToArray(sprites, item);
        }
        
        cJSON_AddItemToObject(root, "sprites", sprites);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Save a new sprite
     */
    static esp_err_t handleApiSpriteSave(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "Sprite save request, content length: %d", req->content_len);
        
        if (req->content_len > 128 * 1024) {
            httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Payload too large");
            return ESP_FAIL;
        }
        
        char* buf = (char*)malloc(req->content_len + 1);
        if (!buf) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return ESP_FAIL;
        }
        
        int total = 0, remaining = req->content_len;
        while (remaining > 0) {
            int ret = httpd_req_recv(req, buf + total, remaining);
            if (ret <= 0) { free(buf); return ESP_FAIL; }
            total += ret;
            remaining -= ret;
        }
        buf[total] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* name = cJSON_GetObjectItem(root, "name");
        cJSON* width = cJSON_GetObjectItem(root, "width");
        cJSON* height = cJSON_GetObjectItem(root, "height");
        cJSON* scale = cJSON_GetObjectItem(root, "scale");
        cJSON* preview = cJSON_GetObjectItem(root, "preview");
        
        bool success = false;
        
        if (name && cJSON_IsString(name)) {
            SavedSprite sprite;
            sprite.id = nextSpriteId_++;
            sprite.name = name->valuestring;
            sprite.width = width ? width->valueint : 64;
            sprite.height = height ? height->valueint : 32;
            sprite.scale = scale ? scale->valueint : 100;
            sprite.preview = preview && cJSON_IsString(preview) ? preview->valuestring : "";
            
            savedSprites_.push_back(sprite);
            ESP_LOGI(HTTP_TAG, "Saved sprite '%s' with id %d", sprite.name.c_str(), sprite.id);
            saveSpritesToStorage();  // Persist to flash
            success = true;
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Rename a sprite
     */
    static esp_err_t handleApiSpriteRename(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[512];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        cJSON* name = cJSON_GetObjectItem(root, "name");
        
        bool success = false;
        
        if (id && cJSON_IsNumber(id) && name && cJSON_IsString(name)) {
            int spriteId = id->valueint;
            for (auto& sprite : savedSprites_) {
                if (sprite.id == spriteId) {
                    sprite.name = name->valuestring;
                    ESP_LOGI(HTTP_TAG, "Renamed sprite %d to '%s'", spriteId, sprite.name.c_str());
                    saveSpritesToStorage();  // Persist to flash
                    success = true;
                    break;
                }
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Delete a sprite
     */
    static esp_err_t handleApiSpriteDelete(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        bool success = false;
        
        if (id && cJSON_IsNumber(id)) {
            int spriteId = id->valueint;
            for (auto it = savedSprites_.begin(); it != savedSprites_.end(); ++it) {
                if (it->id == spriteId) {
                    ESP_LOGI(HTTP_TAG, "Deleted sprite %d ('%s')", spriteId, it->name.c_str());
                    savedSprites_.erase(it);
                    saveSpritesToStorage();  // Persist to flash
                    success = true;
                    break;
                }
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Handle sprite upload and apply
     * Receives RGB888 pixel data for both panels
     */
    static esp_err_t handleApiSpriteApply(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "Sprite apply request, content length: %d", req->content_len);
        
        // Sprite data can be large (64*32*3*2 = 12KB), allocate buffer
        if (req->content_len > 64 * 1024) {
            httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Payload too large");
            return ESP_FAIL;
        }
        
        char* buf = (char*)malloc(req->content_len + 1);
        if (!buf) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return ESP_FAIL;
        }
        
        int total_received = 0;
        int remaining = req->content_len;
        
        while (remaining > 0) {
            int ret = httpd_req_recv(req, buf + total_received, remaining);
            if (ret <= 0) {
                free(buf);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
                return ESP_FAIL;
            }
            total_received += ret;
            remaining -= ret;
        }
        buf[total_received] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        // Extract parameters
        cJSON* width = cJSON_GetObjectItem(root, "width");
        cJSON* height = cJSON_GetObjectItem(root, "height");
        cJSON* offsetX = cJSON_GetObjectItem(root, "offsetX");
        cJSON* offsetY = cJSON_GetObjectItem(root, "offsetY");
        cJSON* scale = cJSON_GetObjectItem(root, "scale");
        cJSON* mirror = cJSON_GetObjectItem(root, "mirror");
        cJSON* leftPanel = cJSON_GetObjectItem(root, "leftPanel");
        cJSON* rightPanel = cJSON_GetObjectItem(root, "rightPanel");
        
        bool success = false;
        
        if (width && height && leftPanel && rightPanel && 
            cJSON_IsString(leftPanel) && cJSON_IsString(rightPanel)) {
            
            int w = width->valueint;
            int h = height->valueint;
            int expectedSize = w * h * 3;  // RGB888
            
            const char* leftB64 = leftPanel->valuestring;
            const char* rightB64 = rightPanel->valuestring;
            
            ESP_LOGI(HTTP_TAG, "Sprite: %dx%d, decoding base64...", w, h);
            
            // Decode base64 pixel data
            uint8_t* leftPixels = (uint8_t*)malloc(expectedSize);
            uint8_t* rightPixels = (uint8_t*)malloc(expectedSize);
            
            if (leftPixels && rightPixels) {
                size_t leftDecoded = 0, rightDecoded = 0;
                
                if (decodeBase64(leftB64, leftPixels, expectedSize, &leftDecoded) &&
                    decodeBase64(rightB64, rightPixels, expectedSize, &rightDecoded) &&
                    leftDecoded == expectedSize && rightDecoded == expectedSize) {
                    
                    // TODO: Send sprite data to GPU via command system
                    ESP_LOGI(HTTP_TAG, "Sprite data received successfully");
                    ESP_LOGI(HTTP_TAG, "  Offset: (%d, %d), Scale: %d%%, Mirror: %s",
                             offsetX ? offsetX->valueint : 0,
                             offsetY ? offsetY->valueint : 0,
                             scale ? scale->valueint : 100,
                             mirror && mirror->type == cJSON_True ? "yes" : "no");
                    
                    success = true;
                } else {
                    ESP_LOGE(HTTP_TAG, "Base64 decode failed or size mismatch: expected %d, got left=%d right=%d",
                             expectedSize, (int)leftDecoded, (int)rightDecoded);
                }
                
                free(leftPixels);
                free(rightPixels);
            } else {
                if (leftPixels) free(leftPixels);
                if (rightPixels) free(rightPixels);
                ESP_LOGE(HTTP_TAG, "Failed to allocate pixel buffers");
            }
        } else {
            ESP_LOGE(HTTP_TAG, "Missing required sprite fields or wrong type");
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false,\"error\":\"Invalid data\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get storage information for sprite storage
     */
    static esp_err_t handleApiStorage(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        // Get actual SPIFFS stats
        size_t totalBytes = 0, usedBytes = 0;
        if (spiffs_initialized_) {
            esp_spiffs_info(NULL, &totalBytes, &usedBytes);
        } else {
            // Fallback estimate if SPIFFS not initialized
            totalBytes = 4 * 1024 * 1024;  // 4MB
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "total", totalBytes);
        cJSON_AddNumberToObject(root, "used", usedBytes);
        cJSON_AddNumberToObject(root, "free", totalBytes > usedBytes ? totalBytes - usedBytes : 0);
        cJSON_AddNumberToObject(root, "spriteCount", savedSprites_.size());
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    // ========== Equation API Handlers ==========
    
    /**
     * @brief Get all equations
     */
    static esp_err_t handleApiEquations(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        cJSON* root = cJSON_CreateObject();
        cJSON* equations = cJSON_CreateArray();
        
        for (const auto& eq : savedEquations_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", eq.id);
            cJSON_AddStringToObject(item, "name", eq.name.c_str());
            cJSON_AddStringToObject(item, "expression", eq.expression.c_str());
            
            cJSON* vars = cJSON_CreateArray();
            for (const auto& v : eq.variables) {
                cJSON* varItem = cJSON_CreateObject();
                cJSON_AddStringToObject(varItem, "name", v.name.c_str());
                cJSON_AddStringToObject(varItem, "type", v.type.c_str());
                cJSON_AddStringToObject(varItem, "value", v.value.c_str());
                cJSON_AddItemToArray(vars, varItem);
            }
            cJSON_AddItemToObject(item, "variables", vars);
            
            cJSON_AddItemToArray(equations, item);
        }
        
        cJSON_AddItemToObject(root, "equations", equations);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Save (create or update) an equation
     */
    static esp_err_t handleApiEquationSave(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[4096];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        cJSON* name = cJSON_GetObjectItem(root, "name");
        cJSON* expression = cJSON_GetObjectItem(root, "expression");
        cJSON* variables = cJSON_GetObjectItem(root, "variables");
        
        bool success = false;
        
        if (name && cJSON_IsString(name) && expression && cJSON_IsString(expression)) {
            SavedEquation eq;
            eq.name = name->valuestring;
            eq.expression = expression->valuestring;
            
            // Parse variables
            if (variables && cJSON_IsArray(variables)) {
                cJSON* varItem = NULL;
                cJSON_ArrayForEach(varItem, variables) {
                    EquationVariable v;
                    cJSON* vname = cJSON_GetObjectItem(varItem, "name");
                    cJSON* vtype = cJSON_GetObjectItem(varItem, "type");
                    cJSON* vvalue = cJSON_GetObjectItem(varItem, "value");
                    
                    if (vname && cJSON_IsString(vname)) v.name = vname->valuestring;
                    if (vtype && cJSON_IsString(vtype)) v.type = vtype->valuestring;
                    if (vvalue && cJSON_IsString(vvalue)) v.value = vvalue->valuestring;
                    
                    eq.variables.push_back(v);
                }
            }
            
            // Check if updating existing or creating new
            if (id && cJSON_IsNumber(id) && id->valueint > 0) {
                // Update existing
                for (auto& existing : savedEquations_) {
                    if (existing.id == id->valueint) {
                        existing.name = eq.name;
                        existing.expression = eq.expression;
                        existing.variables = eq.variables;
                        ESP_LOGI(HTTP_TAG, "Updated equation %d: '%s'", existing.id, existing.name.c_str());
                        success = true;
                        break;
                    }
                }
            } else {
                // Create new
                eq.id = nextEquationId_++;
                savedEquations_.push_back(eq);
                ESP_LOGI(HTTP_TAG, "Created equation %d: '%s'", eq.id, eq.name.c_str());
                success = true;
            }
            
            if (success) {
                saveEquationsToStorage();
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Delete an equation
     */
    static esp_err_t handleApiEquationDelete(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        bool success = false;
        
        if (id && cJSON_IsNumber(id)) {
            int eqId = id->valueint;
            for (auto it = savedEquations_.begin(); it != savedEquations_.end(); ++it) {
                if (it->id == eqId) {
                    ESP_LOGI(HTTP_TAG, "Deleted equation %d ('%s')", eqId, it->name.c_str());
                    savedEquations_.erase(it);
                    saveEquationsToStorage();
                    success = true;
                    break;
                }
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get sensor values for equation editor
     */
    static esp_err_t handleApiSensors(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        cJSON* root = cJSON_CreateObject();
        
        // System Time
        cJSON_AddNumberToObject(root, "millis", (double)(esp_timer_get_time() / 1000));
        
        // Environment Sensors
        cJSON_AddNumberToObject(root, "temperature", state.temperature);
        cJSON_AddNumberToObject(root, "humidity", state.humidity);
        cJSON_AddNumberToObject(root, "pressure", state.pressure);
        
        // IMU - Accelerometer
        cJSON_AddNumberToObject(root, "accel_x", state.accelX);
        cJSON_AddNumberToObject(root, "accel_y", state.accelY);
        cJSON_AddNumberToObject(root, "accel_z", state.accelZ);
        
        // IMU - Gyroscope
        cJSON_AddNumberToObject(root, "gyro_x", state.gyroX);
        cJSON_AddNumberToObject(root, "gyro_y", state.gyroY);
        cJSON_AddNumberToObject(root, "gyro_z", state.gyroZ);
        
        // GPS
        cJSON_AddNumberToObject(root, "gps_lat", state.latitude);
        cJSON_AddNumberToObject(root, "gps_lon", state.longitude);
        cJSON_AddNumberToObject(root, "gps_alt", state.altitude);
        cJSON_AddNumberToObject(root, "gps_speed", state.gpsSpeed);
        cJSON_AddNumberToObject(root, "gps_sats", state.satellites);
        
        // GPS Time - calculate unix timestamp
        // Simple approximation (not accounting for leap years perfectly)
        uint32_t unixTime = 0;
        if (state.gpsYear >= 1970) {
            uint32_t years = state.gpsYear - 1970;
            uint32_t days = years * 365 + (years + 1) / 4; // Approximate leap years
            static const uint16_t daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
            if (state.gpsMonth >= 1 && state.gpsMonth <= 12) {
                days += daysBeforeMonth[state.gpsMonth - 1];
            }
            days += state.gpsDay - 1;
            unixTime = days * 86400 + state.gpsHour * 3600 + state.gpsMinute * 60 + state.gpsSecond;
        }
        cJSON_AddNumberToObject(root, "gps_unix", unixTime);
        cJSON_AddNumberToObject(root, "gps_hour", state.gpsHour);
        cJSON_AddNumberToObject(root, "gps_min", state.gpsMinute);
        cJSON_AddNumberToObject(root, "gps_sec", state.gpsSecond);
        
        // Microphone
        cJSON_AddNumberToObject(root, "mic_db", state.micDb);
        
        // Utility - random value between -1 and 1
        float randomVal = ((float)(esp_random() % 20001) - 10000.0f) / 10000.0f;
        cJSON_AddNumberToObject(root, "random", randomVal);
        
        // Device-corrected IMU (after calibration applied)
        cJSON_AddNumberToObject(root, "device_accel_x", state.deviceAccelX);
        cJSON_AddNumberToObject(root, "device_accel_y", state.deviceAccelY);
        cJSON_AddNumberToObject(root, "device_accel_z", state.deviceAccelZ);
        cJSON_AddNumberToObject(root, "device_gyro_x", state.deviceGyroX);
        cJSON_AddNumberToObject(root, "device_gyro_y", state.deviceGyroY);
        cJSON_AddNumberToObject(root, "device_gyro_z", state.deviceGyroZ);
        cJSON_AddBoolToObject(root, "imu_calibrated", state.imuCalibrated);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    // ========== IMU Calibration Handlers ==========
    
    // Static calibration state
    static inline bool imuCalibrationInProgress_ = false;
    static inline uint32_t imuCalibrationStartTime_ = 0;
    static inline float imuCalibAccumX_ = 0, imuCalibAccumY_ = 0, imuCalibAccumZ_ = 0;
    static inline uint32_t imuCalibSampleCount_ = 0;
    static constexpr uint32_t IMU_CALIB_DURATION_MS = 3000;
    static constexpr float GRAVITY = 9.81f;
    
    /**
     * @brief Start IMU calibration - record for 3 seconds
     */
    static esp_err_t handleApiImuCalibrate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        // Start calibration
        imuCalibrationInProgress_ = true;
        imuCalibrationStartTime_ = (uint32_t)(esp_timer_get_time() / 1000);
        imuCalibAccumX_ = 0;
        imuCalibAccumY_ = 0;
        imuCalibAccumZ_ = 0;
        imuCalibSampleCount_ = 0;
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true,\"message\":\"Calibration started. Keep device still for 3 seconds.\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get IMU calibration status
     */
    static esp_err_t handleApiImuStatus(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "calibrating", imuCalibrationInProgress_);
        cJSON_AddBoolToObject(root, "calibrated", state.imuCalibrated);
        
        if (imuCalibrationInProgress_) {
            uint32_t elapsed = (uint32_t)(esp_timer_get_time() / 1000) - imuCalibrationStartTime_;
            uint32_t remaining = elapsed < IMU_CALIB_DURATION_MS ? IMU_CALIB_DURATION_MS - elapsed : 0;
            cJSON_AddNumberToObject(root, "remainingMs", remaining);
            cJSON_AddNumberToObject(root, "progress", (float)elapsed / IMU_CALIB_DURATION_MS * 100.0f);
        }
        
        // Add current calibration matrix
        cJSON* matrix = cJSON_CreateArray();
        for (int i = 0; i < 9; i++) {
            cJSON_AddItemToArray(matrix, cJSON_CreateNumber(state.imuCalibMatrix[i]));
        }
        cJSON_AddItemToObject(root, "matrix", matrix);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Clear IMU calibration
     */
    static esp_err_t handleApiImuClear(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        // Reset to identity matrix
        state.imuCalibMatrix[0] = 1; state.imuCalibMatrix[1] = 0; state.imuCalibMatrix[2] = 0;
        state.imuCalibMatrix[3] = 0; state.imuCalibMatrix[4] = 1; state.imuCalibMatrix[5] = 0;
        state.imuCalibMatrix[6] = 0; state.imuCalibMatrix[7] = 0; state.imuCalibMatrix[8] = 1;
        state.imuCalibrated = false;
        
        // Clear from SD card via StorageManager
        auto& storageManager = Storage::StorageManager::instance();
        storageManager.clearImuCalibration();
        
        // Also clear from NVS (for migration cleanup)
        nvs_handle_t nvs;
        if (nvs_open("imu_calib", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_erase_all(nvs);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
        
        ESP_LOGI(HTTP_TAG, "IMU calibration cleared from all storage");
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true,\"message\":\"Calibration cleared\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
public:
    /**
     * @brief Process IMU calibration - call this in the main loop
     * 
     * This should be called periodically to accumulate samples and
     * compute the calibration matrix when complete.
     */
    static void processImuCalibration() {
        if (!imuCalibrationInProgress_) return;
        
        auto& state = SYNC_STATE.state();
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t elapsed = now - imuCalibrationStartTime_;
        
        // Accumulate samples
        imuCalibAccumX_ += state.accelX;
        imuCalibAccumY_ += state.accelY;
        imuCalibAccumZ_ += state.accelZ;
        imuCalibSampleCount_++;
        
        // Check if calibration is complete
        if (elapsed >= IMU_CALIB_DURATION_MS && imuCalibSampleCount_ > 0) {
            imuCalibrationInProgress_ = false;
            
            // Average the accumulated values
            float avgX = imuCalibAccumX_ / imuCalibSampleCount_;
            float avgY = imuCalibAccumY_ / imuCalibSampleCount_;
            float avgZ = imuCalibAccumZ_ / imuCalibSampleCount_;
            
            // Normalize to get gravity direction in IMU frame
            float mag = sqrtf(avgX * avgX + avgY * avgY + avgZ * avgZ);
            if (mag < 0.1f) {
                ESP_LOGW(HTTP_TAG, "IMU calibration failed - magnitude too low");
                return;
            }
            
            // Gravity vector in IMU coordinates (normalized)
            float gx = avgX / mag;
            float gy = avgY / mag;
            float gz = avgZ / mag;
            
            // We want to rotate so that gravity points in +Z direction (device down = positive Z)
            // Target gravity direction: (0, 0, 1)
            // After calibration: device_x ≈ 0, device_y ≈ 0, device_z ≈ gravity magnitude
            // Build rotation matrix that transforms (gx, gy, gz) to (0, 0, 1)
            
            // Use Rodrigues' rotation formula
            // rotation axis = gravity x target = (gx,gy,gz) x (0,0,1) = (gy*1-gz*0, gz*0-gx*1, gx*0-gy*0) = (gy, -gx, 0)
            // rotation angle = acos(dot(gravity, target)) = acos(gz)
            
            float ax = gy;
            float ay = -gx;
            float az = 0;
            float axisMag = sqrtf(ax * ax + ay * ay);
            
            if (axisMag < 0.001f) {
                // Gravity already aligned with Z axis
                if (gz > 0) {
                    // Already pointing in +Z - identity matrix
                    state.imuCalibMatrix[0] = 1; state.imuCalibMatrix[1] = 0; state.imuCalibMatrix[2] = 0;
                    state.imuCalibMatrix[3] = 0; state.imuCalibMatrix[4] = 1; state.imuCalibMatrix[5] = 0;
                    state.imuCalibMatrix[6] = 0; state.imuCalibMatrix[7] = 0; state.imuCalibMatrix[8] = 1;
                } else {
                    // Pointing in -Z - 180 degree rotation around X to flip
                    state.imuCalibMatrix[0] = 1; state.imuCalibMatrix[1] = 0; state.imuCalibMatrix[2] = 0;
                    state.imuCalibMatrix[3] = 0; state.imuCalibMatrix[4] = -1; state.imuCalibMatrix[5] = 0;
                    state.imuCalibMatrix[6] = 0; state.imuCalibMatrix[7] = 0; state.imuCalibMatrix[8] = -1;
                }
            } else {
                // Normalize rotation axis
                ax /= axisMag;
                ay /= axisMag;
                
                // Rotation angle
                float cosAngle = gz;  // dot(gravity, (0,0,1))
                float angle = acosf(cosAngle > 1.0f ? 1.0f : (cosAngle < -1.0f ? -1.0f : cosAngle));
                float sinAngle = sinf(angle);
                float oneMinusCos = 1.0f - cosAngle;
                
                // Rodrigues' rotation formula: R = I + sin(θ)K + (1-cos(θ))K²
                // where K is the skew-symmetric cross-product matrix of the axis
                state.imuCalibMatrix[0] = cosAngle + ax * ax * oneMinusCos;
                state.imuCalibMatrix[1] = ax * ay * oneMinusCos - az * sinAngle;
                state.imuCalibMatrix[2] = ax * az * oneMinusCos + ay * sinAngle;
                
                state.imuCalibMatrix[3] = ay * ax * oneMinusCos + az * sinAngle;
                state.imuCalibMatrix[4] = cosAngle + ay * ay * oneMinusCos;
                state.imuCalibMatrix[5] = ay * az * oneMinusCos - ax * sinAngle;
                
                state.imuCalibMatrix[6] = az * ax * oneMinusCos - ay * sinAngle;
                state.imuCalibMatrix[7] = az * ay * oneMinusCos + ax * sinAngle;
                state.imuCalibMatrix[8] = cosAngle + az * az * oneMinusCos;
            }
            
            state.imuCalibrated = true;
            
            // Save to SD card (primary) via StorageManager
            auto& storageManager = Storage::StorageManager::instance();
            Storage::ImuCalibrationData calibData;
            calibData.valid = true;
            memcpy(calibData.matrix, state.imuCalibMatrix, sizeof(calibData.matrix));
            calibData.timestamp = (uint32_t)(esp_timer_get_time() / 1000000);  // seconds
            
            if (storageManager.saveImuCalibration(calibData)) {
                ESP_LOGI(HTTP_TAG, "IMU calibration saved to SD card");
            } else {
                // Fallback to NVS if SD card save fails
                nvs_handle_t nvs;
                if (nvs_open("imu_calib", NVS_READWRITE, &nvs) == ESP_OK) {
                    nvs_set_blob(nvs, "matrix", state.imuCalibMatrix, sizeof(state.imuCalibMatrix));
                    nvs_set_u8(nvs, "valid", 1);
                    nvs_commit(nvs);
                    nvs_close(nvs);
                    ESP_LOGI(HTTP_TAG, "IMU calibration saved to NVS (SD card unavailable)");
                }
            }
            
            ESP_LOGI(HTTP_TAG, "IMU calibration complete. Gravity: (%.2f, %.2f, %.2f)", gx, gy, gz);
        }
    }
    
    /**
     * @brief Apply IMU calibration to get device-frame values
     * Call this after reading raw IMU values
     */
    static void applyImuCalibration() {
        auto& state = SYNC_STATE.state();
        
        if (!state.imuCalibrated) {
            // No calibration - use raw values
            state.deviceAccelX = state.accelX;
            state.deviceAccelY = state.accelY;
            state.deviceAccelZ = state.accelZ;
            state.deviceGyroX = state.gyroX;
            state.deviceGyroY = state.gyroY;
            state.deviceGyroZ = state.gyroZ;
            return;
        }
        
        // Apply rotation matrix: device = R * imu
        float* R = state.imuCalibMatrix;
        
        // Transform accelerometer
        state.deviceAccelX = R[0] * state.accelX + R[1] * state.accelY + R[2] * state.accelZ;
        state.deviceAccelY = R[3] * state.accelX + R[4] * state.accelY + R[5] * state.accelZ;
        state.deviceAccelZ = R[6] * state.accelX + R[7] * state.accelY + R[8] * state.accelZ;
        
        // Transform gyroscope
        state.deviceGyroX = R[0] * state.gyroX + R[1] * state.gyroY + R[2] * state.gyroZ;
        state.deviceGyroY = R[3] * state.gyroX + R[4] * state.gyroY + R[5] * state.gyroZ;
        state.deviceGyroZ = R[6] * state.gyroX + R[7] * state.gyroY + R[8] * state.gyroZ;
    }
    
    /**
     * @brief Load IMU calibration from storage (SD card preferred, NVS fallback)
     */
    static void loadImuCalibration() {
        auto& state = SYNC_STATE.state();
        
        // Try SD card first via StorageManager
        auto& storageManager = Storage::StorageManager::instance();
        Storage::ImuCalibrationData calibData;
        
        if (storageManager.loadImuCalibration(calibData) && calibData.valid) {
            memcpy(state.imuCalibMatrix, calibData.matrix, sizeof(state.imuCalibMatrix));
            state.imuCalibrated = true;
            ESP_LOGI(HTTP_TAG, "IMU calibration loaded from SD card (timestamp: %u)", calibData.timestamp);
            return;
        }
        
        // Fallback to NVS
        nvs_handle_t nvs;
        if (nvs_open("imu_calib", NVS_READONLY, &nvs) == ESP_OK) {
            uint8_t valid = 0;
            if (nvs_get_u8(nvs, "valid", &valid) == ESP_OK && valid == 1) {
                size_t len = sizeof(state.imuCalibMatrix);
                if (nvs_get_blob(nvs, "matrix", state.imuCalibMatrix, &len) == ESP_OK) {
                    state.imuCalibrated = true;
                    ESP_LOGI(HTTP_TAG, "IMU calibration loaded from NVS");
                    
                    // Migrate to SD card if available
                    Storage::ImuCalibrationData migrateData;
                    migrateData.valid = true;
                    memcpy(migrateData.matrix, state.imuCalibMatrix, sizeof(migrateData.matrix));
                    migrateData.timestamp = 0;  // Unknown timestamp for migrated data
                    
                    if (storageManager.saveImuCalibration(migrateData)) {
                        ESP_LOGI(HTTP_TAG, "Migrated IMU calibration from NVS to SD card");
                    }
                }
            }
            nvs_close(nvs);
        }
    }
    
    // ========== SD Card API Handlers ==========
    
    /**
     * @brief Get SD card status
     */
    static esp_err_t handleApiSdCardStatus(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "initialized", sdCard.isReady());
        cJSON_AddBoolToObject(root, "mounted", sdCard.isMounted());
        
        if (sdCard.isMounted()) {
            cJSON_AddStringToObject(root, "name", sdCard.getCardName());
            cJSON_AddNumberToObject(root, "total_mb", sdCard.getTotalBytes() / (1024 * 1024));
            cJSON_AddNumberToObject(root, "free_mb", sdCard.getFreeBytes() / (1024 * 1024));
            cJSON_AddNumberToObject(root, "used_mb", sdCard.getUsedBytes() / (1024 * 1024));
        }
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Format SD card (erase and create new filesystem)
     */
    static esp_err_t handleApiSdCardFormat(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isReady()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not initialized\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        ESP_LOGW(HTTP_TAG, "Formatting SD card...");
        
        bool success = sdCard.format();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", success);
        if (success) {
            cJSON_AddStringToObject(root, "message", "SD card formatted successfully");
            cJSON_AddNumberToObject(root, "total_mb", sdCard.getTotalBytes() / (1024 * 1024));
            cJSON_AddNumberToObject(root, "free_mb", sdCard.getFreeBytes() / (1024 * 1024));
        } else {
            cJSON_AddStringToObject(root, "error", "Failed to format SD card");
        }
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Clear all files from SD card (keep filesystem)
     */
    static esp_err_t handleApiSdCardClear(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isMounted()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not mounted\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        ESP_LOGW(HTTP_TAG, "Clearing all files from SD card...");
        
        bool success = sdCard.clearAll();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", success);
        if (success) {
            cJSON_AddStringToObject(root, "message", "All files cleared");
            cJSON_AddNumberToObject(root, "free_mb", sdCard.getFreeBytes() / (1024 * 1024));
        } else {
            cJSON_AddStringToObject(root, "error", "Failed to clear some files");
        }
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief List directory contents
     */
    static esp_err_t handleApiSdCardList(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isMounted()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not mounted\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get path from query string
        char path[128] = "/";
        char query[256];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            char value[128];
            if (httpd_query_key_value(query, "path", value, sizeof(value)) == ESP_OK) {
                strncpy(path, value, sizeof(path) - 1);
            }
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "path", path);
        
        cJSON* files = cJSON_AddArrayToObject(root, "files");
        
        sdCard.listDir(path, [&](const Utils::FileInfo& info) -> bool {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "name", info.name);
            cJSON_AddStringToObject(item, "path", info.path);
            cJSON_AddBoolToObject(item, "isDir", info.isDirectory);
            cJSON_AddNumberToObject(item, "size", info.size);
            cJSON_AddItemToArray(files, item);
            return true;
        });
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    // ========== Captive Portal Handlers ==========

    /**
     * @brief Handle captive portal detection endpoints
     * 
     * Different OS have different captive portal detection:
     * - Android: Expects 204 from connectivity check, anything else = captive
     * - iOS: Expects specific "Success" response, anything else = captive
     * - Windows: Expects specific content from ncsi.txt
     * 
     * We return HTML that triggers the captive portal popup
     */
    static esp_err_t handleRedirect(httpd_req_t* req) {
        const char* uri = req->uri;
        
        // Android connectivity checks - return non-204 to trigger captive portal
        if (strstr(uri, "generate_204") || strstr(uri, "gen_204") || 
            strstr(uri, "connectivitycheck")) {
            // Return a redirect instead of 204 to trigger Android captive portal
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        }
        
        // iOS/Apple captive portal - return non-Success to trigger popup
        if (strstr(uri, "hotspot-detect") || strstr(uri, "captive.apple") || 
            strstr(uri, "library/test/success")) {
            // Return HTML that will show in iOS captive portal popup
            static const char* ios_response = 
                "<!DOCTYPE html><html><head>"
                "<meta http-equiv=\"refresh\" content=\"0;url=http://192.168.4.1/\">"
                "</head><body><a href=\"http://192.168.4.1/\">Click here</a></body></html>";
            httpd_resp_set_type(req, "text/html");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, ios_response, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Windows NCSI check
        if (strstr(uri, "ncsi.txt") || strstr(uri, "connecttest") || strstr(uri, "msft")) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        }
        
        // Default: redirect to portal
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, nullptr, 0);
        return ESP_OK;
    }
    
    static esp_err_t handleCatchAll(httpd_req_t* req) {
        char host_header[MAX_HOST_HEADER_LENGTH] = {0};
        char user_agent[128] = {0};
        httpd_req_get_hdr_value_str(req, "Host", host_header, sizeof(host_header));
        httpd_req_get_hdr_value_str(req, "User-Agent", user_agent, sizeof(user_agent));
        const char* uri = req->uri;
        
        ESP_LOGI(HTTP_TAG, "Catch-all: Host=%s URI=%s UA=%s", host_header, uri, user_agent);
        
        // Check for captive portal detection user agents
        bool isCaptiveCheck = (
            strstr(user_agent, "CaptiveNetworkSupport") ||  // iOS
            strstr(user_agent, "Microsoft NCSI") ||          // Windows
            strstr(user_agent, "Dalvik") ||                  // Android apps checking connectivity
            strstr(user_agent, "captive") ||
            strstr(user_agent, "NetWorkProbe")               // Various Android OEMs
        );
        
        // Check for captive portal URIs we might have missed
        bool isCaptiveUri = (
            strstr(uri, "generate") ||
            strstr(uri, "connectivity") ||
            strstr(uri, "hotspot") ||
            strstr(uri, "captive") ||
            strstr(uri, "success") ||
            strstr(uri, "ncsi") ||
            strstr(uri, "connect")
        );
        
        // Check if this is a request to an external domain (DNS hijacked)
        bool isExternalHost = (
            strlen(host_header) > 0 && 
            strstr(host_header, "192.168.4.1") == nullptr &&
            strstr(host_header, "lucidius") == nullptr
        );
        
        // If any captive portal indicators, redirect
        if (isCaptiveCheck || isCaptiveUri || isExternalHost) {
            // Request to external domain - redirect to captive portal
            static const char* captive_response = 
                "<!DOCTYPE html><html><head>"
                "<meta http-equiv=\"refresh\" content=\"0;url=http://192.168.4.1/\">"
                "<title>Redirecting...</title>"
                "</head><body>"
                "<h1>Redirecting to Lucidius...</h1>"
                "<p><a href=\"http://192.168.4.1/\">Click here if not redirected</a></p>"
                "</body></html>";
            httpd_resp_set_type(req, "text/html");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, captive_response, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // For non-GET requests, respond with redirect
        if (req->method != HTTP_GET) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        }
        
        // Serve Basic page for any unmatched GET requests
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Expires", "0");
        httpd_resp_send(req, Content::PAGE_BASIC, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Command Processing ==========
    
    void processCommand(CommandType type, cJSON* params) {
        // Invoke callback if set
        if (command_callback_) {
            command_callback_(type, params);
        }
        
        // Default handling
        switch (type) {
            case CommandType::SET_BRIGHTNESS: {
                cJSON* val = cJSON_GetObjectItem(params, "value");
                if (val) SYNC_STATE.setBrightness(val->valueint);
                break;
            }
            
            case CommandType::SET_WIFI_CREDENTIALS: {
                cJSON* ssid = cJSON_GetObjectItem(params, "ssid");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                if (ssid && password && ssid->valuestring && password->valuestring) {
                    ESP_LOGI(HTTP_TAG, "WiFi credentials update: %s", ssid->valuestring);
                    
                    auto& security = arcos::security::SecurityDriver::instance();
                    if (security.setCustomCredentials(ssid->valuestring, password->valuestring)) {
                        ESP_LOGI(HTTP_TAG, "Custom credentials saved successfully");
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        esp_restart();
                    } else {
                        ESP_LOGE(HTTP_TAG, "Failed to save credentials");
                    }
                }
                break;
            }
            
            case CommandType::RESET_WIFI_TO_AUTO: {
                ESP_LOGI(HTTP_TAG, "WiFi reset to auto requested");
                auto& security = arcos::security::SecurityDriver::instance();
                if (security.resetToAuto()) {
                    ESP_LOGI(HTTP_TAG, "Reset to auto credentials successful");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
                break;
            }
            
            case CommandType::RESTART:
                ESP_LOGI(HTTP_TAG, "Restart requested");
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart();
                break;
                
            case CommandType::KICK_CLIENTS: {
                ESP_LOGI(HTTP_TAG, "Kick clients requested");
                wifi_sta_list_t sta_list;
                esp_wifi_ap_get_sta_list(&sta_list);
                
                ESP_LOGI(HTTP_TAG, "Found %d connected clients", sta_list.num);
                
                int kicked = 0;
                for (int i = 0; i < sta_list.num; i++) {
                    uint16_t aid = i + 1;
                    if (esp_wifi_deauth_sta(aid) == ESP_OK) {
                        kicked++;
                        ESP_LOGI(HTTP_TAG, "Kicked client AID=%d", aid);
                    }
                }
                ESP_LOGI(HTTP_TAG, "Kicked %d clients total", kicked);
                break;
            }
            
            case CommandType::SET_EXT_WIFI: {
                cJSON* enabled = cJSON_GetObjectItem(params, "enabled");
                cJSON* ssid = cJSON_GetObjectItem(params, "ssid");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                
                auto& state = SYNC_STATE.state();
                
                if (enabled) {
                    state.extWifiEnabled = cJSON_IsTrue(enabled);
                }
                if (ssid && ssid->valuestring) {
                    strncpy(state.extWifiSSID, ssid->valuestring, sizeof(state.extWifiSSID) - 1);
                    state.extWifiSSID[sizeof(state.extWifiSSID) - 1] = '\0';
                }
                if (password && password->valuestring) {
                    strncpy(state.extWifiPassword, password->valuestring, sizeof(state.extWifiPassword) - 1);
                    state.extWifiPassword[sizeof(state.extWifiPassword) - 1] = '\0';
                }
                
                ESP_LOGI(HTTP_TAG, "External WiFi config: enabled=%d, ssid=%s", 
                         state.extWifiEnabled, state.extWifiSSID);
                
                // Save to NVS for persistence
                auto& security = arcos::security::SecurityDriver::instance();
                security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                            state.extWifiPassword, state.authEnabled,
                                            state.authUsername, state.authPassword);
                break;
            }
            
            case CommandType::EXT_WIFI_CONNECT: {
                cJSON* connect = cJSON_GetObjectItem(params, "connect");
                auto& state = SYNC_STATE.state();
                
                if (connect) {
                    bool shouldConnect = cJSON_IsTrue(connect);
                    state.extWifiConnected = shouldConnect;
                    
                    ESP_LOGI(HTTP_TAG, "External WiFi connect: %s", shouldConnect ? "true" : "false");
                    
                    // Save connect state to NVS for persistence across boots
                    auto& security = arcos::security::SecurityDriver::instance();
                    security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                                state.extWifiPassword, state.authEnabled,
                                                state.authUsername, state.authPassword);
                    
                    if (shouldConnect && state.extWifiEnabled && strlen(state.extWifiSSID) > 0) {
                        // Ensure STA netif exists
                        esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                        if (!sta_netif) {
                            sta_netif = esp_netif_create_default_wifi_sta();
                            ESP_LOGI(HTTP_TAG, "Created STA netif for connection");
                        }
                        
                        // Initiate station connection
                        wifi_config_t sta_config = {};
                        strncpy((char*)sta_config.sta.ssid, state.extWifiSSID, sizeof(sta_config.sta.ssid));
                        strncpy((char*)sta_config.sta.password, state.extWifiPassword, sizeof(sta_config.sta.password));
                        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;  // More permissive
                        sta_config.sta.pmf_cfg.capable = true;
                        sta_config.sta.pmf_cfg.required = false;
                        
                        // Switch to AP+STA mode
                        esp_wifi_set_mode(WIFI_MODE_APSTA);
                        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
                        esp_wifi_connect();
                        
                        ESP_LOGI(HTTP_TAG, "Connecting to external network: %s", state.extWifiSSID);
                    } else if (!shouldConnect) {
                        // Disconnect and switch back to AP-only
                        esp_wifi_disconnect();
                        esp_wifi_set_mode(WIFI_MODE_AP);
                        state.extWifiIsConnected = false;
                        memset(state.extWifiIP, 0, sizeof(state.extWifiIP));
                        state.extWifiRSSI = -100;
                        
                        ESP_LOGI(HTTP_TAG, "Disconnected from external network");
                    }
                }
                break;
            }
            
            case CommandType::SET_AUTH: {
                cJSON* enabled = cJSON_GetObjectItem(params, "enabled");
                cJSON* username = cJSON_GetObjectItem(params, "username");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                
                auto& state = SYNC_STATE.state();
                
                if (enabled) {
                    state.authEnabled = cJSON_IsTrue(enabled);
                }
                if (username && username->valuestring) {
                    strncpy(state.authUsername, username->valuestring, sizeof(state.authUsername) - 1);
                    state.authUsername[sizeof(state.authUsername) - 1] = '\0';
                }
                if (password && password->valuestring && strlen(password->valuestring) > 0) {
                    // In production, this should be hashed!
                    strncpy(state.authPassword, password->valuestring, sizeof(state.authPassword) - 1);
                    state.authPassword[sizeof(state.authPassword) - 1] = '\0';
                }
                
                ESP_LOGI(HTTP_TAG, "Auth config: enabled=%d, username=%s", 
                         state.authEnabled, state.authUsername);
                
                // Save to NVS for persistence
                auto& security = arcos::security::SecurityDriver::instance();
                security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                            state.extWifiPassword, state.authEnabled,
                                            state.authUsername, state.authPassword);
                break;
            }
            
            default:
                ESP_LOGW(HTTP_TAG, "Unknown command type");
                break;
        }
    }
    
    // State
    httpd_handle_t server_ = nullptr;
    CommandCallback command_callback_ = nullptr;
    Animation::AnimationConfigManager animConfigManager_;
    
public:
    /**
     * @brief Get the animation configuration manager
     */
    Animation::AnimationConfigManager& getConfigManager() { return animConfigManager_; }
};

// Convenience macro
#define HTTP_SERVER HttpServer::instance()

} // namespace Web
} // namespace SystemAPI
