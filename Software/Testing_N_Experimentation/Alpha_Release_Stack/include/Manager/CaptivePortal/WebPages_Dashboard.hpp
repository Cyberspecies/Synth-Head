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
