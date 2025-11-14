/*****************************************************************
 * File:      WebServer_Routes.hpp
 * Category:  Manager/WiFi
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Web server route handlers and API endpoints for captive portal.
 *    Handles HTTP requests, button commands, configuration, and restart.
 *    
 * Note:
 *    This file directly accesses the double buffer from CPU.cpp to 
 *    update the portal with fresh sensor data on each web request.
 *****************************************************************/

#ifndef WEBSERVER_ROUTES_HPP
#define WEBSERVER_ROUTES_HPP

#include <atomic>
#include "Drivers/UART Comms/CpuUartBidirectional.hpp"
#include "Drivers/UART Comms/FileTransferManager.hpp"

// Forward declare global variables from CPU.cpp
extern std::atomic<uint8_t> active_buffer_index;
extern arcos::communication::SensorDataPayload sensor_data_buffers[2];
extern arcos::communication::FileTransferManager file_transfer;
extern arcos::communication::CpuUartBidirectional uart_comm;

namespace arcos::manager {

// Web server setup with all routes and API endpoints
// Extracted from CaptivePortalManager.cpp lines 159-340

inline void CaptivePortalManager::setupWebServer() {
  server_ = new AsyncWebServer(80);
  
  // Captive portal detection endpoints (redirect to setup if not configured)
  server_->on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!use_custom_credentials_) {
      request->redirect("/setup");
    } else {
      request->redirect("/");
    }
  });
  
  server_->on("/hotspot-detect.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!use_custom_credentials_) {
      request->redirect("/setup");
    } else {
      request->redirect("/");
    }
  });
  
  // Setup page
  server_->on("/setup", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "text/html", generateSetupPage());
  });
  
  // Setup form submission
  server_->on("/setup", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSetupSubmit(request);
  });
  
  // Main dashboard
  server_->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "text/html", generateDashboardPage());
  });
  
  // API endpoint for sensor data (AJAX updates)
  server_->on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest* request) {
    static uint32_t api_call_count = 0;
    
    // Debug every 8 calls (4Hz polling = every 2 seconds)
    if(++api_call_count % 8 == 0) {
      Serial.printf("DEBUG [WEB-API]: /api/sensors request #%u - Web client polling for data\n", api_call_count);
    }
    
    // CRITICAL FIX: Read sensor data directly from double buffer HERE
    // instead of relying on webServerTask which may not be running
    // Use global variables declared at file scope above
    uint8_t read_index = ::active_buffer_index.load(std::memory_order_acquire);
    ::arcos::communication::SensorDataPayload sensor_copy;
    memcpy(&sensor_copy, &::sensor_data_buffers[read_index], sizeof(sensor_copy));
    
    if(api_call_count % 8 == 0) {
      Serial.printf("DEBUG [WEB-API]: Read buffer[%u] - Temp=%.1f°C, Accel=(%.2f,%.2f,%.2f)\n",
        read_index, sensor_copy.temperature,
        sensor_copy.accel_x, sensor_copy.accel_y, sensor_copy.accel_z);
      Serial.printf("DEBUG [WEB-API]: Updating portal with fresh data...\n");
    }
    
    // Update portal with the fresh sensor data we just read
    updateSensorData(sensor_copy);
    
    String json_response = getSensorDataJSON();
    
    if(api_call_count % 8 == 0) {
      Serial.printf("DEBUG [WEB-API]: JSON generated (%d bytes), sending to client\n", json_response.length());
    }
    
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json_response);
    // Prevent caching for real-time updates
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });
  
  // API endpoint for button commands (POST)
  server_->on("/api/button", HTTP_POST, 
    [](AsyncWebServerRequest* request) {}, 
    NULL,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      // Parse JSON body
      String body = String((char*)data).substring(0, len);
      Serial.printf("BUTTON: Received body: %s\n", body.c_str());
      
      // Simple JSON parsing for button commands
      if (body.indexOf("\"button\":\"A\"") > 0 && body.indexOf("\"state\":true") > 0) {
        Serial.println("BUTTON: A pressed");
        if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensor_data_.setButtonA(true);
          xSemaphoreGive(sensor_data_mutex_);
        }
      } else if (body.indexOf("\"button\":\"A\"") > 0 && body.indexOf("\"state\":false") > 0) {
        if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensor_data_.setButtonA(false);
          xSemaphoreGive(sensor_data_mutex_);
        }
      } else if (body.indexOf("\"button\":\"B\"") > 0 && body.indexOf("\"state\":true") > 0) {
        if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensor_data_.setButtonB(true);
          xSemaphoreGive(sensor_data_mutex_);
        }
      } else if (body.indexOf("\"button\":\"B\"") > 0 && body.indexOf("\"state\":false") > 0) {
        if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensor_data_.setButtonB(false);
          xSemaphoreGive(sensor_data_mutex_);
        }
      } else if (body.indexOf("\"button\":\"C\"") > 0 && body.indexOf("\"state\":true") > 0) {
        if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensor_data_.setButtonC(true);
          xSemaphoreGive(sensor_data_mutex_);
        }
      } else if (body.indexOf("\"button\":\"C\"") > 0 && body.indexOf("\"state\":false") > 0) {
        if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensor_data_.setButtonC(false);
          xSemaphoreGive(sensor_data_mutex_);
        }
      } else if (body.indexOf("\"button\":\"D\"") > 0 && body.indexOf("\"state\":true") > 0) {
        if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensor_data_.setButtonD(true);
          xSemaphoreGive(sensor_data_mutex_);
        }
      } else if (body.indexOf("\"button\":\"D\"") > 0 && body.indexOf("\"state\":false") > 0) {
        if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensor_data_.setButtonD(false);
          xSemaphoreGive(sensor_data_mutex_);
        }
      }
      
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    }
  );
  
  // API endpoint for WiFi configuration (POST)
  server_->on("/api/config", HTTP_POST,
    [](AsyncWebServerRequest* request) {},
    NULL,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      String body = String((char*)data).substring(0, len);
      
      // Parse SSID and password from JSON
      String new_ssid = "";
      String new_password = "";
      bool use_default = false;
      
      int ssid_start = body.indexOf("\"ssid\":\"") + 8;
      int ssid_end = body.indexOf("\"", ssid_start);
      if (ssid_start > 7 && ssid_end > ssid_start) {
        new_ssid = body.substring(ssid_start, ssid_end);
      }
      
      int pass_start = body.indexOf("\"password\":\"") + 12;
      int pass_end = body.indexOf("\"", pass_start);
      if (pass_start > 11 && pass_end > pass_start) {
        new_password = body.substring(pass_start, pass_end);
      }
      
      if (body.indexOf("\"useDefault\":true") > 0) {
        use_default = true;
      }
      
      if (use_default) {
        // Clear custom credentials, use random
        preferences_.begin("wifi", false);
        preferences_.clear();
        preferences_.end();
        
        // Generate new random credentials
        current_ssid_ = device_base_name_ + "_" + generateRandomSuffix();
        current_password_ = generateRandomPassword();
        use_custom_credentials_ = false;
        
        Serial.println("WIFI: Reverted to random credentials (cleared from flash)");
        
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Reverted to random credentials\"}");
      } else if (new_ssid.length() > 0 && new_password.length() >= 8) {
        // Save custom credentials
        current_ssid_ = new_ssid;
        current_password_ = new_password;
        use_custom_credentials_ = true;
        saveCredentials();
        
        Serial.printf("WIFI: Custom credentials saved - SSID: %s\n", current_ssid_.c_str());
        
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Credentials saved. Restart to apply.\"}");
      } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid SSID or password (min 8 chars)\"}");
      }
    }
  );
  
  // API endpoint for file transfer test (POST) - Sends custom sprite
  server_->on("/api/file-transfer", HTTP_POST, [](AsyncWebServerRequest* request) {
    Serial.println("WIFI: Sprite transfer requested via web interface");
    
    // Check if transfer already in progress
    if(::file_transfer.isActive()) {
      request->send(409, "application/json", "{\"success\":false,\"message\":\"File transfer already in progress\"}");
      return;
    }
    
    // Create 16x24 RGB sprite (same as auto-sent sprite)
    constexpr uint16_t sprite_width = 16;
    constexpr uint16_t sprite_height = 24;
    constexpr uint32_t sprite_size = 4 + (sprite_width * sprite_height * 3);  // 1156 bytes
    
    uint8_t* sprite_data = new uint8_t[sprite_size];
    
    if(!sprite_data) {
      request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to allocate memory\"}");
      return;
    }
    
    // Write sprite header: width and height (little-endian)
    sprite_data[0] = sprite_width & 0xFF;
    sprite_data[1] = (sprite_width >> 8) & 0xFF;
    sprite_data[2] = sprite_height & 0xFF;
    sprite_data[3] = (sprite_height >> 8) & 0xFF;
    
    // Fill with RGB gradient pattern
    uint8_t* pixel_data = sprite_data + 4;
    for(int y = 0; y < sprite_height; y++){
      for(int x = 0; x < sprite_width; x++){
        int pixel_index = (y * sprite_width + x) * 3;
        
        // Create colorful gradient:
        // Red increases left to right
        pixel_data[pixel_index + 0] = (x * 255) / sprite_width;
        // Green increases top to bottom
        pixel_data[pixel_index + 1] = (y * 255) / sprite_height;
        // Blue is inverse of red
        pixel_data[pixel_index + 2] = 255 - ((x * 255) / sprite_width);
      }
    }
    
    Serial.printf("WIFI: Created %dx%d sprite (%lu bytes)\n", sprite_width, sprite_height, sprite_size);
    Serial.printf("WIFI: First 16 bytes (header + pixels): ");
    for(int i = 0; i < 16; i++){
      Serial.printf("%02X ", sprite_data[i]);
    }
    Serial.println();
    
    // Start file transfer
    bool started = ::file_transfer.startTransfer(sprite_data, sprite_size, "web_sprite.img");
    
    if(started) {
      Serial.println("WIFI: Sprite transfer started successfully!");
      char response[128];
      snprintf(response, sizeof(response), 
               "{\"success\":true,\"message\":\"Sprite transfer started (%dx%d, %lu bytes)\"}", 
               sprite_width, sprite_height, sprite_size);
      request->send(200, "application/json", response);
    } else {
      Serial.println("WIFI: ERROR - Failed to start sprite transfer!");
      delete[] sprite_data;
      request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to start sprite transfer\"}");
    }
    
    // Note: sprite_data will be deleted when transfer completes
    // For production, handle cleanup in file transfer completion callback
  });
  
  // API endpoint for custom sprite upload (POST) - Receives user PNG as RGB bitmap
  server_->on(
    "/api/upload-sprite", 
    HTTP_POST,
    [](AsyncWebServerRequest* request) {}, // Request handler (empty)
    nullptr, // Upload handler (not used)
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      // Body handler - receives JSON with base64 sprite data
      static uint8_t* sprite_buffer = nullptr;
      static size_t sprite_size = 0;
      
      // First chunk - parse JSON and allocate buffer
      if(index == 0) {
        Serial.printf("WIFI: Receiving sprite upload (%u bytes total)...\n", total);
        
        // Parse JSON to extract base64 data
        String body_str((char*)data, len);
        
        // Find "data":"..." field
        int data_start = body_str.indexOf("\"data\":\"");
        if(data_start == -1) {
          request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON format\"}");
          return;
        }
        data_start += 8; // Skip past "data":"
        
        int data_end = body_str.indexOf("\"", data_start);
        if(data_end == -1) {
          request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON format\"}");
          return;
        }
        
        String base64_data = body_str.substring(data_start, data_end);
        Serial.printf("WIFI: Base64 data length: %d\n", base64_data.length());
        
        // Decode base64
        sprite_size = base64_data.length() * 3 / 4;
        sprite_buffer = new uint8_t[sprite_size];
        
        if(!sprite_buffer) {
          request->send(500, "application/json", "{\"success\":false,\"message\":\"Memory allocation failed\"}");
          return;
        }
        
        // Base64 decode
        const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        size_t out_idx = 0;
        uint32_t val = 0;
        int val_b = -8;
        
        for(size_t i = 0; i < base64_data.length(); i++) {
          char c = base64_data[i];
          if(c == '=') break;
          
          const char* p = strchr(base64_chars, c);
          if(!p) continue;
          
          val = (val << 6) | (p - base64_chars);
          val_b += 6;
          
          if(val_b >= 0) {
            sprite_buffer[out_idx++] = (val >> val_b) & 0xFF;
            val_b -= 8;
          }
        }
        sprite_size = out_idx;
        
        // Validate sprite format
        if(sprite_size < 4) {
          delete[] sprite_buffer;
          sprite_buffer = nullptr;
          request->send(400, "application/json", "{\"success\":false,\"message\":\"Sprite data too small\"}");
          return;
        }
        
        // Extract dimensions
        uint16_t width = sprite_buffer[0] | (sprite_buffer[1] << 8);
        uint16_t height = sprite_buffer[2] | (sprite_buffer[3] << 8);
        
        Serial.printf("WIFI: Decoded sprite: %dx%d (%u bytes)\n", width, height, sprite_size);
        
        // Validate dimensions and size
        uint32_t expected_size = 4 + (width * height * 3);
        if(sprite_size != expected_size) {
          Serial.printf("WIFI: ERROR - Size mismatch! Expected %u, got %u\n", expected_size, sprite_size);
          delete[] sprite_buffer;
          sprite_buffer = nullptr;
          request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid sprite size\"}");
          return;
        }
        
        if(width > 64 || height > 32) {
          Serial.printf("WIFI: WARNING - Sprite exceeds recommended size (64x32)\n");
        }
        
        // Start file transfer
        if(::file_transfer.isActive()) {
          delete[] sprite_buffer;
          sprite_buffer = nullptr;
          request->send(409, "application/json", "{\"success\":false,\"message\":\"Transfer already in progress\"}");
          return;
        }
        
        bool started = ::file_transfer.startTransfer(sprite_buffer, sprite_size, "user_sprite.img");
        
        if(started) {
          Serial.println("WIFI: User sprite transfer started successfully!");
          char response[128];
          snprintf(response, sizeof(response), 
                   "{\"success\":true,\"message\":\"Sprite uploaded (%dx%d, %u bytes)\"}", 
                   width, height, sprite_size);
          request->send(200, "application/json", response);
          
          // Don't delete sprite_buffer - it's owned by file transfer now
          sprite_buffer = nullptr;
        } else {
          Serial.println("WIFI: ERROR - Failed to start user sprite transfer!");
          delete[] sprite_buffer;
          sprite_buffer = nullptr;
          request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to start transfer\"}");
        }
      }
    }
  );
  
  // API endpoint for display settings (POST) - Sends display configuration to GPU
  server_->on(
    "/api/display-settings",
    HTTP_POST,
    [](AsyncWebServerRequest* request) {}, // Request handler (empty)
    nullptr, // Upload handler (not used)
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      // Body handler - receives JSON with display settings
      if(index == 0) {
        Serial.printf("WIFI: Display settings received (%u bytes)\n", len);
        
        // Parse JSON
        String body_str((char*)data, len);
        Serial.printf("WIFI: Body: %s\n", body_str.c_str());
        
        // Extract values (simple parsing)
        int face = -1, effect = -1, shader = -1;
        int r1 = 0, g1 = 0, b1 = 0, r2 = 0, g2 = 0, b2 = 0;
        int speed = 128;
        
        // Parse face
        int face_pos = body_str.indexOf("\"face\":");
        if(face_pos != -1) {
          face = body_str.substring(face_pos + 7).toInt();
        }
        
        // Parse effect
        int effect_pos = body_str.indexOf("\"effect\":");
        if(effect_pos != -1) {
          effect = body_str.substring(effect_pos + 9).toInt();
        }
        
        // Parse shader
        int shader_pos = body_str.indexOf("\"shader\":");
        if(shader_pos != -1) {
          shader = body_str.substring(shader_pos + 9).toInt();
        }
        
        // Parse color1.r
        int r1_pos = body_str.indexOf("\"color1\":{\"r\":");
        if(r1_pos != -1) {
          r1 = body_str.substring(r1_pos + 14).toInt();
        }
        
        // Parse color1.g
        int g1_pos = body_str.indexOf(",\"g\":", r1_pos);
        if(g1_pos != -1) {
          g1 = body_str.substring(g1_pos + 5).toInt();
        }
        
        // Parse color1.b
        int b1_pos = body_str.indexOf(",\"b\":", g1_pos);
        if(b1_pos != -1) {
          b1 = body_str.substring(b1_pos + 5).toInt();
        }
        
        // Parse color2.r
        int r2_pos = body_str.indexOf("\"color2\":{\"r\":");
        if(r2_pos != -1) {
          r2 = body_str.substring(r2_pos + 14).toInt();
        }
        
        // Parse color2.g
        int g2_pos = body_str.indexOf(",\"g\":", r2_pos);
        if(g2_pos != -1) {
          g2 = body_str.substring(g2_pos + 5).toInt();
        }
        
        // Parse color2.b
        int b2_pos = body_str.indexOf(",\"b\":", g2_pos);
        if(b2_pos != -1) {
          b2 = body_str.substring(b2_pos + 5).toInt();
        }
        
        // Parse speed
        int speed_pos = body_str.indexOf("\"speed\":");
        if(speed_pos != -1) {
          speed = body_str.substring(speed_pos + 8).toInt();
        }
        
        Serial.printf("WIFI: Parsed - Face:%d Effect:%d Shader:%d\n", face, effect, shader);
        Serial.printf("WIFI: Color1 RGB:(%d,%d,%d) Color2 RGB:(%d,%d,%d) Speed:%d\n", 
                      r1, g1, b1, r2, g2, b2, speed);
        
        // Create display settings packet
        arcos::communication::DisplaySettings settings;
        settings.display_face = (uint8_t)face;
        settings.display_effect = (uint8_t)effect;
        settings.display_shader = (uint8_t)shader;
        settings._reserved_byte = 0;
        settings.color1_r = (uint8_t)r1;
        settings.color1_g = (uint8_t)g1;
        settings.color1_b = (uint8_t)b1;
        settings.color2_r = (uint8_t)r2;
        settings.color2_g = (uint8_t)g2;
        settings.color2_b = (uint8_t)b2;
        settings.shader_speed = (uint8_t)speed;
        settings._reserved[0] = 0;
        settings._reserved[1] = 0;
        
        // Send packet to GPU via UART (using global uart_comm declared in CPU.cpp)
        bool sent = ::uart_comm.sendPacket(
          arcos::communication::MessageType::DISPLAY_SETTINGS,
          (const uint8_t*)&settings,
          sizeof(settings)
        );
        
        if(sent) {
          Serial.println("WIFI: Display settings sent to GPU successfully!");
          request->send(200, "application/json", "{\"success\":true,\"message\":\"Settings applied\"}");
        } else {
          Serial.println("WIFI: ERROR - Failed to send display settings to GPU!");
          request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to send to GPU\"}");
        }
      }
    }
  );
  
  // API endpoint for LED settings (POST) - Sends LED strip configuration to GPU
  server_->on(
    "/api/led-settings",
    HTTP_POST,
    [](AsyncWebServerRequest* request) {}, // Request handler (empty)
    nullptr, // Upload handler (not used)
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      // Body handler - receives JSON with LED settings
      if(index == 0) {
        Serial.printf("WIFI: LED settings received (%u bytes)\n", len);
        
        // Parse JSON
        String body_str((char*)data, len);
        Serial.printf("WIFI: Body: %s\n", body_str.c_str());
        
        // Extract values (simple parsing)
        int ledMode = 0, speed = 128, brightness = 255;
        int r1 = 0, g1 = 0, b1 = 0, r2 = 0, g2 = 0, b2 = 0;
        
        // Parse ledMode
        int led_pos = body_str.indexOf("\"ledMode\":");
        if(led_pos != -1) {
          ledMode = body_str.substring(led_pos + 10).toInt();
        }
        
        // Parse speed
        int speed_pos = body_str.indexOf("\"speed\":");
        if(speed_pos != -1) {
          speed = body_str.substring(speed_pos + 8).toInt();
        }
        
        // Parse brightness
        int bright_pos = body_str.indexOf("\"brightness\":");
        if(bright_pos != -1) {
          brightness = body_str.substring(bright_pos + 13).toInt();
        }
        
        // Parse color1.r
        int r1_pos = body_str.indexOf("\"color1\":{\"r\":");
        if(r1_pos != -1) {
          r1 = body_str.substring(r1_pos + 14).toInt();
        }
        
        // Parse color1.g
        int g1_pos = body_str.indexOf(",\"g\":", r1_pos);
        if(g1_pos != -1) {
          g1 = body_str.substring(g1_pos + 5).toInt();
        }
        
        // Parse color1.b
        int b1_pos = body_str.indexOf(",\"b\":", g1_pos);
        if(b1_pos != -1) {
          b1 = body_str.substring(b1_pos + 5).toInt();
        }
        
        // Parse color2.r
        int r2_pos = body_str.indexOf("\"color2\":{\"r\":");
        if(r2_pos != -1) {
          r2 = body_str.substring(r2_pos + 14).toInt();
        }
        
        // Parse color2.g
        int g2_pos = body_str.indexOf(",\"g\":", r2_pos);
        if(g2_pos != -1) {
          g2 = body_str.substring(g2_pos + 5).toInt();
        }
        
        // Parse color2.b
        int b2_pos = body_str.indexOf(",\"b\":", g2_pos);
        if(b2_pos != -1) {
          b2 = body_str.substring(b2_pos + 5).toInt();
        }
        
        Serial.printf("WIFI: Parsed - LED Mode:%d Speed:%d Brightness:%d\n", ledMode, speed, brightness);
        Serial.printf("WIFI: Color1 RGB:(%d,%d,%d) Color2 RGB:(%d,%d,%d)\n", r1, g1, b1, r2, g2, b2);
        
        // Create LED settings packet
        arcos::communication::LedSettings settings;
        settings.led_strip_mode = (uint8_t)ledMode;
        settings.color1_r = (uint8_t)r1;
        settings.color1_g = (uint8_t)g1;
        settings.color1_b = (uint8_t)b1;
        settings.color2_r = (uint8_t)r2;
        settings.color2_g = (uint8_t)g2;
        settings.color2_b = (uint8_t)b2;
        settings.speed = (uint8_t)speed;
        settings.brightness = (uint8_t)brightness;
        settings._reserved[0] = 0;
        settings._reserved[1] = 0;
        
        // Send packet to GPU via UART
        bool sent = ::uart_comm.sendPacket(
          arcos::communication::MessageType::LED_SETTINGS,
          (const uint8_t*)&settings,
          sizeof(settings)
        );
        
        if(sent) {
          Serial.println("WIFI: LED settings sent to GPU successfully!");
          request->send(200, "application/json", "{\"success\":true,\"message\":\"LED settings applied\"}");
        } else {
          Serial.println("WIFI: ERROR - Failed to send LED settings to GPU!");
          request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to send to GPU\"}");
        }
      }
    }
  );
  
  // API endpoint for device restart (POST)
  server_->on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* request) {
    Serial.println("WIFI: Restart requested via web interface");
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Restarting device...\"}");
    delay(500);
    ESP.restart();
  });
  
  // Catch-all for captive portal (redirect to setup or dashboard)
  server_->onNotFound([this](AsyncWebServerRequest* request) {
    if (!use_custom_credentials_) {
      request->redirect("/setup");
    } else {
      request->redirect("/");
    }
  });
  
  server_->begin();
  Serial.println("WIFI: Web server started on port 80");
}

} // namespace arcos::manager

#endif // WEBSERVER_ROUTES_HPP
