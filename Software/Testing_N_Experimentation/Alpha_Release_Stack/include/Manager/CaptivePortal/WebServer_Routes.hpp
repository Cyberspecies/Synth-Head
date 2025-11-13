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

// Forward declare global variables from CPU.cpp
extern std::atomic<uint8_t> active_buffer_index;
extern arcos::communication::SensorDataPayload sensor_data_buffers[2];

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
