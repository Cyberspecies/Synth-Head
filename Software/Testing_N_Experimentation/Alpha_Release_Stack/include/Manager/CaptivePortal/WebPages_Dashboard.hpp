/*****************************************************************
 * File:      WebPages_Dashboard.hpp
 * Category:  Manager/WiFi
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Main dashboard page HTML generation for captive portal.
 *    Complete dashboard with sensor display, buttons, configuration, and restart controls.
 *****************************************************************/

#ifndef WEB_PAGES_DASHBOARD_HPP
#define WEB_PAGES_DASHBOARD_HPP

namespace arcos::manager {

inline String CaptivePortalManager::generateDashboardPage() {
  String html = R"HTMLDELIM(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SynthHead - Dashboard</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: #1a1a2e;
      color: #eee;
      padding: 20px;
    }
    .header {
      text-align: center;
      margin-bottom: 30px;
      padding: 20px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      border-radius: 15px;
      position: relative;
    }
    h1 {
      font-size: 32px;
      margin-bottom: 5px;
    }
    .status {
      font-size: 14px;
      opacity: 0.9;
    }
    .live-indicator {
      position: absolute;
      top: 20px;
      right: 20px;
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 12px;
      font-weight: 600;
    }
    .live-dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: #48bb78;
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 20px;
      margin-bottom: 20px;
    }
    .card {
      background: #16213e;
      border-radius: 15px;
      padding: 20px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.3);
    }
    .card h2 {
      color: #667eea;
      font-size: 18px;
      margin-bottom: 15px;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .sensor-row {
      display: flex;
      justify-content: space-between;
      padding: 8px 0;
      border-bottom: 1px solid #0f3460;
    }
    .sensor-row:last-child {
      border-bottom: none;
    }
    .sensor-label {
      color: #aaa;
      font-size: 14px;
    }
    .sensor-value {
      color: #fff;
      font-weight: 600;
      font-family: monospace;
      font-size: 14px;
    }
    .button-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }
    .button-indicator {
      padding: 15px;
      border-radius: 10px;
      text-align: center;
      font-weight: 600;
      transition: all 0.15s;
      cursor: pointer;
      user-select: none;
      -webkit-user-select: none;
      -moz-user-select: none;
      -ms-user-select: none;
      border: 2px solid transparent;
    }
    .button-off {
      background: #2d3748;
      color: #718096;
    }
    .button-off:hover {
      background: #3d4758;
      border-color: #667eea;
    }
    .button-off:active {
      transform: scale(0.95);
    }
    .button-on {
      background: #48bb78;
      color: white;
      box-shadow: 0 0 20px rgba(72,187,120,0.5);
      border-color: #48bb78;
      transform: scale(1.05);
    }
    .button-on:active {
      transform: scale(1.0);
    }
    .update-time {
      text-align: center;
      color: #666;
      font-size: 12px;
      margin-top: 20px;
    }
    .config-section {
      margin-top: 20px;
      padding: 20px;
      background: #16213e;
      border-radius: 15px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.3);
    }
    .config-section h2 {
      color: #667eea;
      font-size: 18px;
      margin-bottom: 15px;
    }
    .config-form {
      display: flex;
      flex-direction: column;
      gap: 15px;
    }
    .form-group {
      display: flex;
      flex-direction: column;
      gap: 5px;
    }
    .form-group label {
      color: #aaa;
      font-size: 14px;
    }
    .form-group input {
      padding: 10px;
      border-radius: 8px;
      border: 2px solid #0f3460;
      background: #0a1929;
      color: white;
      font-size: 14px;
    }
    .form-group input:focus {
      outline: none;
      border-color: #667eea;
    }
    .button-row {
      display: flex;
      gap: 10px;
      margin-top: 10px;
    }
    .config-btn {
      flex: 1;
      padding: 12px;
      border: none;
      border-radius: 8px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
    }
    .config-btn-primary {
      background: #667eea;
      color: white;
    }
    .config-btn-primary:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102,126,234,0.4);
    }
    .config-btn-secondary {
      background: #48bb78;
      color: white;
    }
    .config-btn-secondary:hover {
      background: #38a169;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(72,187,120,0.4);
    }
    .config-btn:active {
      transform: translateY(0);
    }
    .config-message {
      padding: 10px;
      border-radius: 8px;
      margin-top: 10px;
      text-align: center;
      font-size: 14px;
      display: none;
    }
    .config-message.success {
      background: rgba(72,187,120,0.2);
      color: #48bb78;
      border: 1px solid #48bb78;
    }
    .config-message.error {
      background: rgba(245,101,101,0.2);
      color: #f56565;
      border: 1px solid #f56565;
    }
    .current-info {
      padding: 10px;
      background: #0a1929;
      border-radius: 8px;
      margin-bottom: 15px;
      color: #aaa;
      font-size: 13px;
    }
    .current-info strong {
      color: white;
    }
    .restart-section {
      margin-top: 20px;
      padding: 20px;
      background: #16213e;
      border-radius: 15px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.3);
      text-align: center;
    }
    .restart-btn {
      padding: 12px 30px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
      background: #f56565;
      color: white;
    }
    .restart-btn:hover {
      background: #e53e3e;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(245,101,101,0.4);
    }
    .confirm-buttons {
      display: none;
      gap: 10px;
      justify-content: center;
      margin-top: 15px;
    }
    .confirm-buttons.show {
      display: flex;
    }
    .confirm-btn {
      padding: 10px 25px;
      border: none;
      border-radius: 8px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
    }
    .confirm-yes {
      background: #f56565;
      color: white;
    }
    .confirm-yes:hover {
      background: #e53e3e;
    }
    .confirm-no {
      background: #4a5568;
      color: white;
    }
    .confirm-no:hover {
      background: #2d3748;
    }
  </style>
</head>
<body>
  <div class="header">
    <div class="live-indicator">
      <div class="live-dot"></div>
      <span>LIVE</span>
    </div>
    <h1>🎭 SynthHead Dashboard</h1>
    <div class="status">Real-time Sensor Monitoring @ 10.0.0.1</div>
    <div class="status" style="margin-top: 10px; font-size: 18px; font-weight: bold; color: #48bb78;">
      Uptime: <span id="device_uptime">--</span>
    </div>
  </div>
  
  <div class="grid">
    <!-- IMU Data -->
    <div class="card">
      <h2>📐 IMU Sensor</h2>
      <div class="sensor-row">
        <span class="sensor-label">Accel X</span>
        <span class="sensor-value" id="accel_x">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Accel Y</span>
        <span class="sensor-value" id="accel_y">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Accel Z</span>
        <span class="sensor-value" id="accel_z">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Gyro X</span>
        <span class="sensor-value" id="gyro_x">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Gyro Y</span>
        <span class="sensor-value" id="gyro_y">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Gyro Z</span>
        <span class="sensor-value" id="gyro_z">--</span>
      </div>
    </div>
    
    <!-- Environmental Data -->
    <div class="card">
      <h2>🌡️ Environment</h2>
      <div class="sensor-row">
        <span class="sensor-label">Temperature</span>
        <span class="sensor-value" id="temperature">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Humidity</span>
        <span class="sensor-value" id="humidity">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Pressure</span>
        <span class="sensor-value" id="pressure">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Altitude</span>
        <span class="sensor-value" id="altitude">--</span>
      </div>
    </div>
    
    <!-- GPS Data -->
    <div class="card">
      <h2>🛰️ GPS</h2>
      <div class="sensor-row">
        <span class="sensor-label">Latitude</span>
        <span class="sensor-value" id="gps_lat">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Longitude</span>
        <span class="sensor-value" id="gps_lon">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Speed</span>
        <span class="sensor-value" id="gps_speed">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">Satellites</span>
        <span class="sensor-value" id="gps_sats">--</span>
      </div>
      <div class="sensor-row">
        <span class="sensor-label">UTC Time</span>
        <span class="sensor-value" id="gps_time">--</span>
      </div>
    </div>
    
    <!-- Buttons -->
    <div class="card">
      <h2>🎮 Buttons</h2>
      <div class="button-grid">
        <div class="button-indicator button-off" id="btn_d" style="grid-column: 1 / -1;">MODE</div>
        <div class="button-indicator button-off" id="btn_b">UP</div>
        <div class="button-indicator button-off" id="btn_c">DOWN</div>
        <div class="button-indicator button-off" id="btn_a" style="grid-column: 1 / -1;">SET</div>
      </div>
    </div>
  </div>
  
  <!-- WiFi Configuration -->
  <div class="config-section">
    <h2>📡 WiFi Configuration</h2>
    <div class="current-info">
      Current SSID: <strong>)HTMLDELIM";
  html += current_ssid_;
  html += R"HTMLDELIM(</strong><br>
      Current Password: <strong>)HTMLDELIM";
  html += current_password_;
  html += R"HTMLDELIM(</strong>
    </div>
    <div class="config-form">
      <div class="form-group">
        <label for="ssid_input">SSID (Network Name)</label>
        <input type="text" id="ssid_input" placeholder="Enter custom SSID" maxlength="32">
      </div>
      <div class="form-group">
        <label for="password_input">Password (min 8 characters)</label>
        <input type="password" id="password_input" placeholder="Enter password" maxlength="63">
      </div>
      <div class="button-row">
        <button class="config-btn config-btn-primary" onclick="saveCustomConfig()">Save Custom Credentials</button>
        <button class="config-btn config-btn-secondary" onclick="useDefaultConfig()">Use Random (Clear Flash)</button>
      </div>
      <div class="config-message" id="config_message"></div>
    </div>
  </div>
  
  <!-- Image Upload Section -->
  <div class="restart-section" style="background: #1e293b; border: 2px solid #667eea;">
    <h2 style="color: #667eea; margin-bottom: 15px;">🎨 Custom Sprite Upload</h2>
    <p style="color: #94a3b8; font-size: 14px; margin-bottom: 15px;">Upload PNG image for HUB75 display (recommended: 16x24 to 64x32 pixels)</p>
    
    <input type="file" id="image_upload" accept="image/png,image/jpeg,image/jpg" 
           style="display: none;" onchange="handleImageUpload(event)">
    
    <button class="restart-btn" style="background: #667eea; margin-bottom: 10px;" onclick="document.getElementById('image_upload').click()">
      📁 Choose Image
    </button>
    
    <div id="preview_container" style="display: none; margin: 15px 0; text-align: center;">
      <canvas id="image_preview" style="border: 2px solid #667eea; max-width: 100%; image-rendering: pixelated;"></canvas>
      <div style="margin-top: 10px; color: #94a3b8; font-size: 13px;">
        <span id="image_dimensions"></span>
      </div>
    </div>
    
    <button class="restart-btn" id="upload_sprite_btn" style="background: #48bb78; display: none;" onclick="uploadSprite()">
      ⬆️ Upload to GPU
    </button>
    
    <div class="config-message" id="upload_status" style="display: none;"></div>
    <div class="config-message" id="transfer_message"></div>
    <div style="margin-top: 15px; color: #94a3b8; font-size: 13px;">
      <div>Transfer Status: <span id="transfer_status" style="color: #48bb78; font-weight: 600;">Idle</span></div>
      <div style="margin-top: 5px;">Last Transfer: <span id="last_transfer" style="color: white; font-weight: 600;">Never</span></div>
    </div>
  </div>
  
  <!-- Display Settings Section -->
  <div class="restart-section" style="background: #1e293b; border: 2px solid #f59e0b;">
    <h2 style="color: #f59e0b; margin-bottom: 15px;">🎨 Display Settings</h2>
    <p style="color: #94a3b8; font-size: 14px; margin-bottom: 15px;">Configure HUB75 display and LED strip behavior</p>
    
    <div style="display: grid; gap: 15px;">
      <!-- Display Face -->
      <div>
        <label style="color: #94a3b8; font-size: 14px; display: block; margin-bottom: 8px;">Display Face:</label>
        <select id="display_face" style="width: 100%; padding: 10px; background: #2d3748; color: white; border: 1px solid #4a5568; border-radius: 5px; font-size: 14px;">
          <option value="0">Custom Image</option>
          <option value="1">Panel Number</option>
          <option value="2">Orientation</option>
        </select>
      </div>
      
      <!-- Display Effect -->
      <div>
        <label style="color: #94a3b8; font-size: 14px; display: block; margin-bottom: 8px;">Display Effect:</label>
        <select id="display_effect" style="width: 100%; padding: 10px; background: #2d3748; color: white; border: 1px solid #4a5568; border-radius: 5px; font-size: 14px;">
          <option value="0">None</option>
          <option value="1">Particles</option>
          <option value="2">Trails</option>
          <option value="3">Grid</option>
          <option value="4">Wave</option>
        </select>
      </div>
      
      <!-- Display Shader -->
      <div>
        <label style="color: #94a3b8; font-size: 14px; display: block; margin-bottom: 8px;">Display Shader:</label>
        <select id="display_shader" style="width: 100%; padding: 10px; background: #2d3748; color: white; border: 1px solid #4a5568; border-radius: 5px; font-size: 14px;" onchange="updateShaderOptions()">
          <option value="0">None</option>
          <option value="1">Hue Cycle (Sprite Rows)</option>
          <option value="2">Hue Cycle (Override All)</option>
          <option value="3">Color Override (Static)</option>
          <option value="4">Color Override (Breathe)</option>
          <option value="5">RGB Split</option>
          <option value="6">Scanlines</option>
          <option value="7">Pixelate</option>
          <option value="8">Invert</option>
          <option value="9">Dither</option>
        </select>
      </div>
      
      <!-- Shader Color Options (shown for color override shaders) -->
      <div id="shader_color_options" style="display: none; background: #2d3748; padding: 15px; border-radius: 5px; border: 1px solid #4a5568;">
        <label style="color: #94a3b8; font-size: 14px; display: block; margin-bottom: 8px;">Color 1:</label>
        <input type="color" id="shader_color1" value="#ff0000" style="width: 100%; height: 40px; border: none; border-radius: 5px; cursor: pointer;">
        
        <div id="color2_container" style="margin-top: 15px;">
          <label style="color: #94a3b8; font-size: 14px; display: block; margin-bottom: 8px;">Color 2 (for breathing):</label>
          <input type="color" id="shader_color2" value="#0000ff" style="width: 100%; height: 40px; border: none; border-radius: 5px; cursor: pointer;">
        </div>
        
        <label style="color: #94a3b8; font-size: 14px; display: block; margin-top: 15px; margin-bottom: 8px;">Animation Speed:</label>
        <input type="range" id="shader_speed" min="1" max="255" value="128" style="width: 100%;">
        <div style="color: #94a3b8; font-size: 12px; text-align: center;"><span id="shader_speed_value">128</span></div>
      </div>
    </div>
    
    <button class="restart-btn" style="background: #f59e0b; margin-top: 20px;" onclick="applyDisplaySettings()">
      ✨ Apply Display Settings
    </button>
    
    <div class="config-message" id="display_settings_message"></div>
  </div>
  
  <!-- LED Strip Settings Section -->
  <div class="restart-section" style="background: #1e293b; border: 2px solid #10b981;">
    <h2 style="color: #10b981; margin-bottom: 15px;">💡 LED Strip Settings</h2>
    <p style="color: #94a3b8; font-size: 14px; margin-bottom: 15px;">Configure each LED body part individually (13 Left Fin, 9 Tongue, 13 Right Fin, 14 Scale)</p>
    
    <!-- Left Fin LEDs (13 LEDs) -->
    <details style="background: #2d3748; padding: 15px; border-radius: 8px; margin-bottom: 10px; border: 1px solid #4a5568;">
      <summary style="cursor: pointer; font-weight: 600; color: #10b981; font-size: 16px; user-select: none;">🦎 Left Fin (13 LEDs)</summary>
      <div style="margin-top: 15px; display: grid; gap: 12px;">
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Mode:</label>
          <select id="led_left_fin_mode" style="width: 100%; padding: 8px; background: #1a202c; color: white; border: 1px solid #4a5568; border-radius: 5px; font-size: 13px;">
            <option value="0">Dynamic (HUB75)</option>
            <option value="1">Rainbow</option>
            <option value="2">Breathing</option>
            <option value="3">Solid Color</option>
            <option value="4">Off</option>
          </select>
        </div>
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Color:</label>
          <input type="color" id="led_left_fin_color" value="#ff0000" style="width: 100%; height: 35px; border: none; border-radius: 5px; cursor: pointer;">
        </div>
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Brightness: <span id="led_left_fin_brightness_val">255</span></label>
          <input type="range" id="led_left_fin_brightness" min="0" max="255" value="255" style="width: 100%;" oninput="document.getElementById('led_left_fin_brightness_val').textContent=this.value">
        </div>
      </div>
    </details>
    
    <!-- Tongue LEDs (9 LEDs) -->
    <details style="background: #2d3748; padding: 15px; border-radius: 8px; margin-bottom: 10px; border: 1px solid #4a5568;">
      <summary style="cursor: pointer; font-weight: 600; color: #10b981; font-size: 16px; user-select: none;">👅 Tongue (9 LEDs)</summary>
      <div style="margin-top: 15px; display: grid; gap: 12px;">
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Mode:</label>
          <select id="led_tongue_mode" style="width: 100%; padding: 8px; background: #1a202c; color: white; border: 1px solid #4a5568; border-radius: 5px; font-size: 13px;">
            <option value="0">Dynamic (HUB75)</option>
            <option value="1">Rainbow</option>
            <option value="2">Breathing</option>
            <option value="3">Solid Color</option>
            <option value="4">Off</option>
          </select>
        </div>
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Color:</label>
          <input type="color" id="led_tongue_color" value="#00ff00" style="width: 100%; height: 35px; border: none; border-radius: 5px; cursor: pointer;">
        </div>
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Brightness: <span id="led_tongue_brightness_val">255</span></label>
          <input type="range" id="led_tongue_brightness" min="0" max="255" value="255" style="width: 100%;" oninput="document.getElementById('led_tongue_brightness_val').textContent=this.value">
        </div>
      </div>
    </details>
    
    <!-- Right Fin LEDs (13 LEDs) -->
    <details style="background: #2d3748; padding: 15px; border-radius: 8px; margin-bottom: 10px; border: 1px solid #4a5568;">
      <summary style="cursor: pointer; font-weight: 600; color: #10b981; font-size: 16px; user-select: none;">🦎 Right Fin (13 LEDs)</summary>
      <div style="margin-top: 15px; display: grid; gap: 12px;">
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Mode:</label>
          <select id="led_right_fin_mode" style="width: 100%; padding: 8px; background: #1a202c; color: white; border: 1px solid #4a5568; border-radius: 5px; font-size: 13px;">
            <option value="0">Dynamic (HUB75)</option>
            <option value="1">Rainbow</option>
            <option value="2">Breathing</option>
            <option value="3">Solid Color</option>
            <option value="4">Off</option>
          </select>
        </div>
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Color:</label>
          <input type="color" id="led_right_fin_color" value="#0000ff" style="width: 100%; height: 35px; border: none; border-radius: 5px; cursor: pointer;">
        </div>
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Brightness: <span id="led_right_fin_brightness_val">255</span></label>
          <input type="range" id="led_right_fin_brightness" min="0" max="255" value="255" style="width: 100%;" oninput="document.getElementById('led_right_fin_brightness_val').textContent=this.value">
        </div>
      </div>
    </details>
    
    <!-- Scale LEDs (14 LEDs) -->
    <details style="background: #2d3748; padding: 15px; border-radius: 8px; margin-bottom: 15px; border: 1px solid #4a5568;">
      <summary style="cursor: pointer; font-weight: 600; color: #10b981; font-size: 16px; user-select: none;">⚡ Scale (14 LEDs)</summary>
      <div style="margin-top: 15px; display: grid; gap: 12px;">
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Mode:</label>
          <select id="led_scale_mode" style="width: 100%; padding: 8px; background: #1a202c; color: white; border: 1px solid #4a5568; border-radius: 5px; font-size: 13px;">
            <option value="0">Dynamic (HUB75)</option>
            <option value="1">Rainbow</option>
            <option value="2">Breathing</option>
            <option value="3">Solid Color</option>
            <option value="4">Off</option>
          </select>
        </div>
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Color:</label>
          <input type="color" id="led_scale_color" value="#ff00ff" style="width: 100%; height: 35px; border: none; border-radius: 5px; cursor: pointer;">
        </div>
        <div>
          <label style="color: #94a3b8; font-size: 13px; display: block; margin-bottom: 6px;">Brightness: <span id="led_scale_brightness_val">255</span></label>
          <input type="range" id="led_scale_brightness" min="0" max="255" value="255" style="width: 100%;" oninput="document.getElementById('led_scale_brightness_val').textContent=this.value">
        </div>
      </div>
    </details>
    
    <button class="restart-btn" style="background: #10b981;" onclick="applyLedSettings()">
      💡 Apply LED Settings to All Parts
    </button>
    
    <div class="config-message" id="led_settings_message"></div>
  </div>
  
  <!-- Fan Speed Control Section -->
  <div class="config-section">
    <h2 style="color: #667eea; margin-bottom: 15px;">🌀 Fan Speed Control</h2>
    <p style="color: #94a3b8; font-size: 14px; margin-bottom: 15px;">Adjust cooling fan speed (0% = Off, 100% = Maximum)</p>
    
    <div style="display: flex; gap: 10px; margin-bottom: 15px;">
      <button class="restart-btn" style="flex: 1; background: #64748b; padding: 10px;" onclick="setFanSpeed(0)">
        ❄️ Off (0%)
      </button>
      <button class="restart-btn" style="flex: 1; background: #10b981; padding: 10px;" onclick="setFanSpeed(100)">
        🔥 Max (100%)
      </button>
    </div>
    
    <div style="background: #1a202c; padding: 15px; border-radius: 10px; border: 1px solid #4a5568;">
      <label style="color: #94a3b8; font-size: 14px; display: block; margin-bottom: 10px;">
        Fan Speed: <span id="fan_speed_val" style="color: #10b981; font-weight: 600;">50%</span>
      </label>
      <input type="range" id="fan_speed" min="0" max="100" value="50" 
             style="width: 100%; height: 8px; border-radius: 5px; background: linear-gradient(to right, #64748b 0%, #10b981 100%); outline: none; -webkit-appearance: none;"
             oninput="updateFanSpeedDisplay(this.value)">
      <div style="display: flex; justify-content: space-between; color: #64748b; font-size: 12px; margin-top: 5px;">
        <span>0%</span>
        <span>25%</span>
        <span>50%</span>
        <span>75%</span>
        <span>100%</span>
      </div>
    </div>
    
    <button class="restart-btn" style="background: #667eea; margin-top: 15px;" onclick="applyFanSpeed()">
      🌀 Apply Fan Speed
    </button>
    
    <div class="config-message" id="fan_speed_message"></div>
  </div>
  
  <!-- Restart Section -->
  <div class="restart-section">
    <h2 style="color: #667eea; margin-bottom: 15px;">Device Control</h2>
    <button class="restart-btn" onclick="showRestartConfirm()">Restart Device</button>
    <div class="confirm-buttons" id="restart_confirm">
      <button class="confirm-btn confirm-yes" onclick="confirmRestart()">Confirm Restart</button>
      <button class="confirm-btn confirm-no" onclick="cancelRestart()">Cancel</button>
    </div>
  </div>
  
  <div class="update-time">
    Last update: <span id="last_update">Never</span>
  </div>
  
  <script>
    let updateCount = 0;
    let isConnected = true;
    
    // Button state tracking for interactive buttons
    const buttonStates = {
      a: { pressed: false, holdTimer: null },
      b: { pressed: false, holdTimer: null },
      c: { pressed: false, holdTimer: null },
      d: { pressed: false, holdTimer: null }
    };
    
    let isFetching = false;
    let lastUpdateTime = 0;
    
    function updateSensorData() {
      // Prevent overlapping fetches
      if (isFetching) {
        console.log('Fetch already in progress, skipping...');
        return;
      }
      
      isFetching = true;
      const startTime = Date.now();
      
      // Create abort controller for timeout
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 2000);
      
      fetch('/api/sensors', {
        method: 'GET',
        cache: 'no-cache',
        signal: controller.signal,
        headers: {
          'Content-Type': 'application/json',
        }
      })
        .then(response => {
          clearTimeout(timeoutId);
          if (!response.ok) {
            throw new Error('Network response was not ok');
          }
          return response.json();
        })
        .then(data => {
          updateCount++;
          isConnected = true;
          lastUpdateTime = Date.now();
          
          // Update device uptime (this should ALWAYS change if polling works)
          if (data.uptime !== undefined) {
            const uptimeMs = data.uptime;
            const seconds = Math.floor(uptimeMs / 1000);
            const minutes = Math.floor(seconds / 60);
            const hours = Math.floor(minutes / 60);
            const days = Math.floor(hours / 24);
            const years = Math.floor(days / 365);
            
            let uptimeStr = '';
            if (years > 0) {
              uptimeStr = years + 'y ' + (days % 365) + 'd ' + (hours % 24) + 'h ' + (minutes % 60) + 'm ' + (seconds % 60) + 's';
            } else if (days > 0) {
              uptimeStr = days + 'd ' + (hours % 24) + 'h ' + (minutes % 60) + 'm ' + (seconds % 60) + 's';
            } else if (hours > 0) {
              uptimeStr = hours + 'h ' + (minutes % 60) + 'm ' + (seconds % 60) + 's';
            } else if (minutes > 0) {
              uptimeStr = minutes + 'm ' + (seconds % 60) + 's';
            } else {
              uptimeStr = seconds + 's';
            }
            
            document.getElementById('device_uptime').textContent = uptimeStr;
          }
          
          // IMU
          document.getElementById('accel_x').textContent = data.accel_x.toFixed(2) + ' g';
          document.getElementById('accel_y').textContent = data.accel_y.toFixed(2) + ' g';
          document.getElementById('accel_z').textContent = data.accel_z.toFixed(2) + ' g';
          document.getElementById('gyro_x').textContent = data.gyro_x.toFixed(1) + ' °/s';
          document.getElementById('gyro_y').textContent = data.gyro_y.toFixed(1) + ' °/s';
          document.getElementById('gyro_z').textContent = data.gyro_z.toFixed(1) + ' °/s';
          
          // Environment
          document.getElementById('temperature').textContent = data.temperature.toFixed(1) + ' °C';
          document.getElementById('humidity').textContent = data.humidity.toFixed(1) + ' %';
          document.getElementById('pressure').textContent = data.pressure.toFixed(0) + ' hPa';
          document.getElementById('altitude').textContent = data.altitude.toFixed(1) + ' m';
          
          // GPS
          document.getElementById('gps_lat').textContent = data.gps_lat.toFixed(6) + '°';
          document.getElementById('gps_lon').textContent = data.gps_lon.toFixed(6) + '°';
          document.getElementById('gps_speed').textContent = data.gps_speed.toFixed(1) + ' km/h';
          document.getElementById('gps_sats').textContent = data.gps_satellites;
          
          // GPS Time - format as HH:MM:SS UTC
          if (data.gps_hour !== undefined && data.gps_minute !== undefined && data.gps_second !== undefined) {
            const time = String(data.gps_hour).padStart(2, '0') + ':' + 
                        String(data.gps_minute).padStart(2, '0') + ':' + 
                        String(data.gps_second).padStart(2, '0') + ' UTC';
            document.getElementById('gps_time').textContent = time;
          } else {
            document.getElementById('gps_time').textContent = '--:--:--';
          }
          
          // Buttons - update only if not being interacted with
          updateButtonDisplay('btn_a', data.button_a, 'a');
          updateButtonDisplay('btn_b', data.button_b, 'b');
          updateButtonDisplay('btn_c', data.button_c, 'c');
          updateButtonDisplay('btn_d', data.button_d, 'd');
          
          // Update time with frame count and latency
          const latency = Date.now() - startTime;
          const now = new Date();
          document.getElementById('last_update').textContent = 
            now.toLocaleTimeString() + ' (' + updateCount + ') [' + latency + 'ms]';
          
          // Log data to console for debugging
          if (updateCount % 10 === 0) {
            console.log('Update #' + updateCount + ':', {
              accel: [data.accel_x.toFixed(2), data.accel_y.toFixed(2), data.accel_z.toFixed(2)],
              temp: data.temperature.toFixed(1),
              buttons: [data.button_a, data.button_b, data.button_c, data.button_d],
              latency: latency + 'ms'
            });
          }
          
          isFetching = false;
        })
        .catch(error => {
          clearTimeout(timeoutId);
          console.error('Error fetching sensor data:', error);
          isConnected = false;
          document.getElementById('last_update').textContent = 'Connection Error: ' + error.message;
          isFetching = false;
        });
    }
    
    function updateButtonDisplay(btnId, state, key) {
      const btn = document.getElementById(btnId);
      // Don't override if user is holding the button
      if (!buttonStates[key].pressed) {
        btn.className = 'button-indicator ' + (state ? 'button-on' : 'button-off');
      }
    }
    
    function sendButtonCommand(button, state) {
      // Send button command to device
      fetch('/api/button', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ button: button, state: state })
      })
      .catch(error => console.error('Error sending button command:', error));
    }
    
    // Setup interactive buttons
    function setupButton(btnId, btnKey, btnName) {
      const btn = document.getElementById(btnId);
      
      // Mouse events
      btn.addEventListener('mousedown', (e) => {
        e.preventDefault();
        buttonStates[btnKey].pressed = true;
        btn.className = 'button-indicator button-on';
        sendButtonCommand(btnName, true);
        
        // Hold detection after 2 seconds
        buttonStates[btnKey].holdTimer = setTimeout(() => {
          btn.style.boxShadow = '0 0 30px rgba(72,187,120,1)';
        }, 2000);
      });
      
      btn.addEventListener('mouseup', (e) => {
        e.preventDefault();
        buttonStates[btnKey].pressed = false;
        btn.className = 'button-indicator button-off';
        btn.style.boxShadow = 'none';
        sendButtonCommand(btnName, false);
        if (buttonStates[btnKey].holdTimer) {
          clearTimeout(buttonStates[btnKey].holdTimer);
        }
      });
      
      btn.addEventListener('mouseleave', () => {
        if (buttonStates[btnKey].pressed) {
          buttonStates[btnKey].pressed = false;
          btn.className = 'button-indicator button-off';
          btn.style.boxShadow = 'none';
          sendButtonCommand(btnName, false);
          if (buttonStates[btnKey].holdTimer) {
            clearTimeout(buttonStates[btnKey].holdTimer);
          }
        }
      });
      
      // Touch events for mobile
      btn.addEventListener('touchstart', (e) => {
        e.preventDefault();
        buttonStates[btnKey].pressed = true;
        btn.className = 'button-indicator button-on';
        sendButtonCommand(btnName, true);
        
        buttonStates[btnKey].holdTimer = setTimeout(() => {
          btn.style.boxShadow = '0 0 30px rgba(72,187,120,1)';
        }, 2000);
      });
      
      btn.addEventListener('touchend', (e) => {
        e.preventDefault();
        buttonStates[btnKey].pressed = false;
        btn.className = 'button-indicator button-off';
        btn.style.boxShadow = 'none';
        sendButtonCommand(btnName, false);
        if (buttonStates[btnKey].holdTimer) {
          clearTimeout(buttonStates[btnKey].holdTimer);
        }
      });
      
      // Make button appear clickable
      btn.style.cursor = 'pointer';
      btn.style.userSelect = 'none';
    }
    
    // Initialize buttons
    setupButton('btn_a', 'a', 'A');
    setupButton('btn_b', 'b', 'B');
    setupButton('btn_c', 'c', 'C');
    setupButton('btn_d', 'd', 'D');
    
    // Start updates immediately and repeat every 100ms (10 times per second)
    updateSensorData();
    setInterval(updateSensorData, 100);
    
    // Debug: Log connection status
    setInterval(() => {
      const timeSinceUpdate = Date.now() - lastUpdateTime;
      console.log('Connected:', isConnected, 'Updates:', updateCount, 'Last update:', timeSinceUpdate, 'ms ago');
    }, 2000);
    
    // WiFi Configuration Functions
    function saveCustomConfig() {
      const ssid = document.getElementById('ssid_input').value;
      const password = document.getElementById('password_input').value;
      const messageEl = document.getElementById('config_message');
      
      if (ssid.length === 0 || password.length < 8) {
        messageEl.textContent = 'SSID required and password must be at least 8 characters';
        messageEl.className = 'config-message error';
        messageEl.style.display = 'block';
        return;
      }
      
      fetch('/api/config', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          ssid: ssid,
          password: password,
          useDefault: false
        })
      })
        .then(response => response.json())
        .then(data => {
          messageEl.textContent = data.message || 'Configuration saved! Restart device to apply.';
          messageEl.className = 'config-message success';
          messageEl.style.display = 'block';
          
          // Clear inputs
          document.getElementById('ssid_input').value = null;
          document.getElementById('password_input').value = null;
        })
        .catch(error => {
          messageEl.textContent = 'Error saving configuration';
          messageEl.className = 'config-message error';
          messageEl.style.display = 'block';
          console.error('Config error:', error);
        });
    }
    
    function useDefaultConfig() {
      const messageEl = document.getElementById('config_message');
      
      if (!confirm('This will erase custom credentials from flash and use random SSID/password. Continue?')) {
        return;
      }
      
      fetch('/api/config', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          useDefault: true
        })
      })
        .then(response => response.json())
        .then(data => {
          messageEl.textContent = data.message || 'Reverted to random credentials! Restart device to apply.';
          messageEl.className = 'config-message success';
          messageEl.style.display = 'block';
          
          // Clear inputs
          document.getElementById('ssid_input').value = null;
          document.getElementById('password_input').value = null;
        })
        .catch(error => {
          messageEl.textContent = 'Error reverting configuration';
          messageEl.className = 'config-message error';
          messageEl.style.display = 'block';
          console.error('Config error:', error);
        });
    }
    
    // File Transfer Function
    function startFileTransfer() {
      const statusEl = document.getElementById('transfer_status');
      const messageEl = document.getElementById('transfer_message');
      const lastTransferEl = document.getElementById('last_transfer');
      
      // Update UI
      statusEl.textContent = 'Sending...';
      statusEl.style.color = '#fbbf24';
      messageEl.textContent = 'Sending 16x24 RGB sprite (1.1KB)...';
      messageEl.className = 'config-message';
      messageEl.style.display = 'block';
      messageEl.style.background = 'rgba(251,191,36,0.2)';
      messageEl.style.color = '#fbbf24';
      messageEl.style.border = '1px solid #fbbf24';
      
      fetch('/api/file-transfer', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        }
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            statusEl.textContent = 'Success';
            statusEl.style.color = '#48bb78';
            messageEl.textContent = data.message || 'File transfer started successfully!';
            messageEl.className = 'config-message success';
            messageEl.style.display = 'block';
            const now = new Date();
            lastTransferEl.textContent = now.toLocaleTimeString();
          } else {
            statusEl.textContent = 'Failed';
            statusEl.style.color = '#f56565';
            messageEl.textContent = data.message || 'File transfer failed';
            messageEl.className = 'config-message error';
            messageEl.style.display = 'block';
          }
          
          // Hide message after 5 seconds
          setTimeout(() => {
            messageEl.style.display = 'none';
            if (data.success) {
              statusEl.textContent = 'Idle';
              statusEl.style.color = '#48bb78';
            }
          }, 5000);
        })
        .catch(error => {
          statusEl.textContent = 'Error';
          statusEl.style.color = '#f56565';
          messageEl.textContent = 'Network error during file transfer';
          messageEl.className = 'config-message error';
          messageEl.style.display = 'block';
          console.error('File transfer error:', error);
        });
    }
    
    // Image Upload Functions
    function handleImageUpload(event) {
      const file = event.target.files[0];
      if (!file) return;
      
      const statusEl = document.getElementById('upload_status');
      const uploadBtn = document.getElementById('upload_sprite_btn');
      const previewContainer = document.getElementById('preview_container');
      const canvas = document.getElementById('image_preview');
      const ctx = canvas.getContext('2d');
      const dimensionsEl = document.getElementById('image_dimensions');
      
      // Show loading
      statusEl.textContent = 'Loading...';
      statusEl.style.display = 'block';
      
      const reader = new FileReader();
      reader.onload = function(e) {
        const img = new Image();
        img.onload = function() {
          // Set canvas size
          canvas.width = img.width;
          canvas.height = img.height;
          
          // Draw image
          ctx.imageSmoothingEnabled = false;
          ctx.drawImage(img, 0, 0);
          
          // Show preview
          previewContainer.style.display = 'block';
          dimensionsEl.textContent = img.width + 'x' + img.height;
          uploadBtn.style.display = 'inline-block';
          statusEl.textContent = 'Image loaded - ready to upload';
          statusEl.style.color = '#48bb78';
          
          // Validate dimensions
          if (img.width > 64 || img.height > 32) {
            statusEl.textContent = 'Warning: Image exceeds 64x32! May be scaled down.';
            statusEl.style.color = '#fbbf24';
          }
        };
        img.onerror = function() {
          statusEl.textContent = 'Error loading image';
          statusEl.style.color = '#f56565';
        };
        img.src = e.target.result;
      };
      reader.readAsDataURL(file);
    }
    
    function uploadSprite() {
      const canvas = document.getElementById('image_preview');
      const statusEl = document.getElementById('upload_status');
      const uploadBtn = document.getElementById('upload_sprite_btn');
      const transferStatusEl = document.getElementById('transfer_status');
      const transferMsgEl = document.getElementById('transfer_message');
      const lastTransferEl = document.getElementById('last_transfer');
      
      if (!canvas.width || !canvas.height) {
        statusEl.textContent = 'No image loaded';
        statusEl.style.color = '#f56565';
        return;
      }
      
      // Disable button during upload
      uploadBtn.disabled = true;
      statusEl.textContent = 'Converting to sprite format...';
      statusEl.style.color = '#fbbf24';
      
      try {
        const ctx = canvas.getContext('2d');
        const width = canvas.width;
        const height = canvas.height;
        const imageData = ctx.getImageData(0, 0, width, height);
        const pixels = imageData.data;
        
        // Create sprite data: [width:2][height:2][RGB pixels]
        const spriteSize = 4 + (width * height * 3);
        const spriteData = new Uint8Array(spriteSize);
        
        // Write width and height (little-endian)
        spriteData[0] = width & 0xFF;
        spriteData[1] = (width >> 8) & 0xFF;
        spriteData[2] = height & 0xFF;
        spriteData[3] = (height >> 8) & 0xFF;
        
        // Write RGB pixel data
        let offset = 4;
        for (let i = 0; i < pixels.length; i += 4) {
          spriteData[offset++] = pixels[i];     // R
          spriteData[offset++] = pixels[i + 1]; // G
          spriteData[offset++] = pixels[i + 2]; // B
        }
        
        // Convert to base64
        const base64 = btoa(String.fromCharCode.apply(null, spriteData));
        
        statusEl.textContent = 'Uploading to device...';
        
        // Update transfer status UI
        transferStatusEl.textContent = 'Sending...';
        transferStatusEl.style.color = '#fbbf24';
        transferMsgEl.textContent = 'Sending ' + width + 'x' + height + ' sprite (' + (spriteSize/1024).toFixed(1) + 'KB)...';
        transferMsgEl.className = 'config-message';
        transferMsgEl.style.display = 'block';
        transferMsgEl.style.background = 'rgba(251,191,36,0.2)';
        transferMsgEl.style.color = '#fbbf24';
        transferMsgEl.style.border = '1px solid #fbbf24';
        
        // Send to device
        fetch('/api/upload-sprite', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            data: base64,
            width: width,
            height: height
          })
        })
          .then(response => response.json())
          .then(data => {
            uploadBtn.disabled = false;
            if (data.success) {
              statusEl.textContent = 'Upload successful!';
              statusEl.style.color = '#48bb78';
              
              transferStatusEl.textContent = 'Success';
              transferStatusEl.style.color = '#48bb78';
              transferMsgEl.textContent = data.message || 'Sprite uploaded successfully!';
              transferMsgEl.className = 'config-message success';
              transferMsgEl.style.display = 'block';
              const now = new Date();
              lastTransferEl.textContent = now.toLocaleTimeString();
              
              // Hide messages after 5 seconds
              setTimeout(() => {
                statusEl.style.display = 'none';
                transferMsgEl.style.display = 'none';
                transferStatusEl.textContent = 'Idle';
              }, 5000);
            } else {
              statusEl.textContent = 'Upload failed: ' + (data.message || 'Unknown error');
              statusEl.style.color = '#f56565';
              
              transferStatusEl.textContent = 'Failed';
              transferStatusEl.style.color = '#f56565';
              transferMsgEl.textContent = data.message || 'Sprite upload failed';
              transferMsgEl.className = 'config-message error';
              transferMsgEl.style.display = 'block';
            }
          })
          .catch(error => {
            uploadBtn.disabled = false;
            statusEl.textContent = 'Network error: ' + error.message;
            statusEl.style.color = '#f56565';
            
            transferStatusEl.textContent = 'Error';
            transferStatusEl.style.color = '#f56565';
            transferMsgEl.textContent = 'Network error during upload';
            transferMsgEl.className = 'config-message error';
            transferMsgEl.style.display = 'block';
            console.error('Upload error:', error);
          });
      } catch (error) {
        uploadBtn.disabled = false;
        statusEl.textContent = 'Error processing image: ' + error.message;
        statusEl.style.color = '#f56565';
        console.error('Image processing error:', error);
      }
    }
    
    // Display Settings Functions
    function updateShaderOptions() {
      const shader = parseInt(document.getElementById('display_shader').value);
      const colorOptions = document.getElementById('shader_color_options');
      const color2Container = document.getElementById('color2_container');
      
      // Show color options for color override shaders (3=static, 4=breathe)
      if (shader === 3 || shader === 4) {
        colorOptions.style.display = 'block';
        // Show color 2 only for breathing
        color2Container.style.display = (shader === 4) ? 'block' : 'none';
      } else {
        colorOptions.style.display = 'none';
      }
    }
    
    // Update speed value display
    document.addEventListener('DOMContentLoaded', function() {
      const speedSlider = document.getElementById('shader_speed');
      const speedValue = document.getElementById('shader_speed_value');
      if (speedSlider && speedValue) {
        speedSlider.addEventListener('input', function() {
          speedValue.textContent = this.value;
        });
      }
    });
    
    // LED Settings Functions
    function applyLedSettings() {
      const messageEl = document.getElementById('led_settings_message');
      
      // Helper to convert hex color to RGB
      const hexToRgb = (hex) => {
        const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
        return result ? {
          r: parseInt(result[1], 16),
          g: parseInt(result[2], 16),
          b: parseInt(result[3], 16)
        } : {r: 0, g: 0, b: 0};
      };
      
      // Collect settings for each body part
      const leftFin = {
        mode: parseInt(document.getElementById('led_left_fin_mode').value),
        color: hexToRgb(document.getElementById('led_left_fin_color').value),
        brightness: parseInt(document.getElementById('led_left_fin_brightness').value)
      };
      
      const tongue = {
        mode: parseInt(document.getElementById('led_tongue_mode').value),
        color: hexToRgb(document.getElementById('led_tongue_color').value),
        brightness: parseInt(document.getElementById('led_tongue_brightness').value)
      };
      
      const rightFin = {
        mode: parseInt(document.getElementById('led_right_fin_mode').value),
        color: hexToRgb(document.getElementById('led_right_fin_color').value),
        brightness: parseInt(document.getElementById('led_right_fin_brightness').value)
      };
      
      const scale = {
        mode: parseInt(document.getElementById('led_scale_mode').value),
        color: hexToRgb(document.getElementById('led_scale_color').value),
        brightness: parseInt(document.getElementById('led_scale_brightness').value)
      };
      
      // Show loading
      messageEl.textContent = 'Applying LED settings to all body parts...';
      messageEl.className = 'config-message';
      messageEl.style.display = 'block';
      messageEl.style.background = 'rgba(251,191,36,0.2)';
      messageEl.style.color = '#fbbf24';
      messageEl.style.border = '1px solid #fbbf24';
      
      // Send to device
      fetch('/api/led-sections', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          leftFin: leftFin,
          tongue: tongue,
          rightFin: rightFin,
          scale: scale
        })
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            messageEl.textContent = data.message || 'LED settings applied to all parts!';
            messageEl.className = 'config-message success';
            messageEl.style.display = 'block';
            
            setTimeout(() => {
              messageEl.style.display = 'none';
            }, 3000);
          } else {
            messageEl.textContent = data.message || 'Failed to apply LED settings';
            messageEl.className = 'config-message error';
            messageEl.style.display = 'block';
          }
        })
        .catch(error => {
          messageEl.textContent = 'Network error: ' + error.message;
          messageEl.className = 'config-message error';
          messageEl.style.display = 'block';
          console.error('LED settings error:', error);
        });
    }
    
    // Fan speed control functions
    function updateFanSpeedDisplay(value) {
      document.getElementById('fan_speed_val').textContent = value + '%';
    }
    
    function setFanSpeed(percent) {
      document.getElementById('fan_speed').value = percent;
      updateFanSpeedDisplay(percent);
    }
    
    function applyFanSpeed() {
      const messageEl = document.getElementById('fan_speed_message');
      const speed = parseInt(document.getElementById('fan_speed').value);
      
      // Show loading
      messageEl.textContent = 'Applying fan speed...';
      messageEl.className = 'config-message';
      messageEl.style.display = 'block';
      messageEl.style.background = 'rgba(251,191,36,0.2)';
      messageEl.style.color = '#fbbf24';
      messageEl.style.border = '1px solid #fbbf24';
      
      // Send to device
      fetch('/api/fan-speed', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          speed: speed
        })
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            messageEl.textContent = `Fan speed set to ${speed}%!`;
            messageEl.className = 'config-message success';
            messageEl.style.display = 'block';
            
            setTimeout(() => {
              messageEl.style.display = 'none';
            }, 3000);
          } else {
            messageEl.textContent = data.message || 'Failed to set fan speed';
            messageEl.className = 'config-message error';
            messageEl.style.display = 'block';
          }
        })
        .catch(error => {
          messageEl.textContent = 'Network error: ' + error.message;
          messageEl.className = 'config-message error';
          messageEl.style.display = 'block';
          console.error('Fan speed error:', error);
        });
    }
    
    function applyDisplaySettings() {
      const messageEl = document.getElementById('display_settings_message');
      
      // Get values
      const face = parseInt(document.getElementById('display_face').value);
      const effect = parseInt(document.getElementById('display_effect').value);
      const shader = parseInt(document.getElementById('display_shader').value);
      
      // Get color values (hex to RGB)
      const color1 = document.getElementById('shader_color1').value;
      const color2 = document.getElementById('shader_color2').value;
      const speed = parseInt(document.getElementById('shader_speed').value);
      
      // Convert hex colors to RGB
      const hexToRgb = (hex) => {
        const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
        return result ? {
          r: parseInt(result[1], 16),
          g: parseInt(result[2], 16),
          b: parseInt(result[3], 16)
        } : {r: 0, g: 0, b: 0};
      };
      
      const rgb1 = hexToRgb(color1);
      const rgb2 = hexToRgb(color2);
      
      // Show loading
      messageEl.textContent = 'Applying settings...';
      messageEl.className = 'config-message';
      messageEl.style.display = 'block';
      messageEl.style.background = 'rgba(251,191,36,0.2)';
      messageEl.style.color = '#fbbf24';
      messageEl.style.border = '1px solid #fbbf24';
      
      // Send to device
      fetch('/api/display-settings', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          face: face,
          effect: effect,
          shader: shader,
          color1: rgb1,
          color2: rgb2,
          speed: speed
        })
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            messageEl.textContent = data.message || 'Settings applied successfully!';
            messageEl.className = 'config-message success';
            messageEl.style.display = 'block';
            
            // Hide message after 3 seconds
            setTimeout(() => {
              messageEl.style.display = 'none';
            }, 3000);
          } else {
            messageEl.textContent = data.message || 'Failed to apply settings';
            messageEl.className = 'config-message error';
            messageEl.style.display = 'block';
          }
        })
        .catch(error => {
          messageEl.textContent = 'Network error: ' + error.message;
          messageEl.className = 'config-message error';
          messageEl.style.display = 'block';
          console.error('Display settings error:', error);
        });
    }
    
    // Restart Functions
    function showRestartConfirm() {
      document.getElementById('restart_confirm').classList.add('show');
    }
    
    function cancelRestart() {
      document.getElementById('restart_confirm').classList.remove('show');
    }
    
    function confirmRestart() {
      fetch('/api/restart', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        }
      })
        .then(response => response.json())
        .then(data => {
          alert('Device is restarting... Please reconnect in a few seconds.');
        })
        .catch(error => {
          console.error('Restart error:', error);
          alert('Device is restarting...');
        });
    }
  </script>
</body>
</html>
)HTMLDELIM";
  
  return html;
}

} // namespace arcos::manager

#endif // WEB_PAGES_DASHBOARD_HPP
