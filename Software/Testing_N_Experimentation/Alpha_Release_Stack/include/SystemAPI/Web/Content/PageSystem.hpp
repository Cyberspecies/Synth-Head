/*****************************************************************
 * @file PageSystem.hpp
 * @brief System tab page content - sensor and system status
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_SYSTEM[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - System</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <div class="container">
    <header>
      <div class="header-content">
        <div class="logo-section">
          <div class="logo-icon">&#x25C8;</div>
          <div class="logo-text">
            <h1>Lucidius</h1>
            <span class="model-tag" id="device-model">DX.3</span>
          </div>
        </div>
      </div>
    </header>
    
    <nav class="tabs">
      <a href="/" class="tab">Basic</a>
      <a href="/advanced" class="tab">Advanced</a>
      <a href="/system" class="tab active">System</a>
      <a href="/settings" class="tab">Settings</a>
    </nav>
    
    <section class="tab-content active">
      <div class="card-grid">
      <div class="card">
        <div class="card-header"><h2>System</h2></div>
        <div class="card-body compact">
          <div class="sys-row"><span>Mode</span><span id="sys-mode">Idle</span></div>
          <div class="sys-row"><span>Status</span><span id="sys-status-text">Ready</span></div>
          <div class="sys-row"><span>Uptime</span><span id="sys-uptime">00:00:00</span></div>
          <div class="sys-row"><span>Memory</span><span id="sys-heap">--</span></div>
          <div class="sys-row"><span>CPU</span><span id="sys-cpu-usage">--</span></div>
          <div class="sys-row"><span>FPS</span><span id="sys-fps">--</span></div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header"><h2>Connections</h2></div>
        <div class="card-body compact">
          <div class="sys-row"><span>CPU</span><span class="status-dot connected"></span><span id="conn-cpu">Connected</span></div>
          <div class="sys-row"><span>GPU</span><span class="status-dot" id="conn-gpu-dot"></span><span id="conn-gpu">N/C</span></div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header"><h2>GPU Status</h2></div>
        <div class="card-body compact">
          <div class="sys-row"><span>FPS</span><span id="gpu-fps">--</span></div>
          <div class="sys-row"><span>Load</span><span id="gpu-load">--</span></div>
          <div class="sys-row"><span>Memory</span><span id="gpu-heap">--</span></div>
          <div class="sys-row"><span>Min Memory</span><span id="gpu-min-heap">--</span></div>
          <div class="sys-row"><span>Uptime</span><span id="gpu-uptime">--</span></div>
          <div class="sys-row"><span>Frames</span><span id="gpu-frames">--</span></div>
          <div class="sys-row"><span>HUB75</span><span class="status-dot" id="gpu-hub75-dot"></span><span id="gpu-hub75">--</span></div>
          <div class="sys-row"><span>OLED</span><span class="status-dot" id="gpu-oled-dot"></span><span id="gpu-oled">--</span></div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header"><h2>Environment</h2></div>
        <div class="card-body compact">
          <div class="sys-row"><span>Temp</span><span id="sens-temp">N/C</span></div>
          <div class="sys-row"><span>Humidity</span><span id="sens-humidity">N/C</span></div>
          <div class="sys-row"><span>Pressure</span><span id="sens-pressure">N/C</span></div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header"><h2>IMU</h2></div>
        <div class="card-body compact">
          <div class="imu-section">
            <span class="imu-section-label">Accelerometer</span>
            <div class="imu-bar-row">
              <span class="axis-label">X</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="accel-x-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="imu-ax">0</span>
            </div>
            <div class="imu-bar-row">
              <span class="axis-label">Y</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="accel-y-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="imu-ay">0</span>
            </div>
            <div class="imu-bar-row">
              <span class="axis-label">Z</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="accel-z-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="imu-az">0</span>
            </div>
          </div>
          <div class="imu-section">
            <span class="imu-section-label">Gyroscope</span>
            <div class="imu-bar-row">
              <span class="axis-label">X</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="gyro-x-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="imu-gx">0</span>
            </div>
            <div class="imu-bar-row">
              <span class="axis-label">Y</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="gyro-y-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="imu-gy">0</span>
            </div>
            <div class="imu-bar-row">
              <span class="axis-label">Z</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="gyro-z-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="imu-gz">0</span>
            </div>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-header"><h2>Device IMU (Calibrated)</h2></div>
        <div class="card-body compact">
          <div class="imu-section">
            <span class="imu-section-label">Accelerometer</span>
            <div class="imu-bar-row">
              <span class="axis-label">X</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="dev-accel-x-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="dev-ax">0.00</span>
            </div>
            <div class="imu-bar-row">
              <span class="axis-label">Y</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="dev-accel-y-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="dev-ay">0.00</span>
            </div>
            <div class="imu-bar-row">
              <span class="axis-label">Z</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="dev-accel-z-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="dev-az">0.00</span>
            </div>
          </div>
          <div class="imu-section">
            <span class="imu-section-label">Gyroscope</span>
            <div class="imu-bar-row">
              <span class="axis-label">X</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="dev-gyro-x-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="dev-gx">0.00</span>
            </div>
            <div class="imu-bar-row">
              <span class="axis-label">Y</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="dev-gyro-y-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="dev-gy">0.00</span>
            </div>
            <div class="imu-bar-row">
              <span class="axis-label">Z</span>
              <div class="imu-bar"><div class="imu-bar-fill" id="dev-gyro-z-bar"></div><div class="imu-bar-center"></div></div>
              <span class="imu-value" id="dev-gz">0.00</span>
            </div>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-header"><h2>GPS</h2></div>
        <div class="card-body compact">
          <div class="sys-row"><span>Status</span><span id="gps-status-text">N/C</span></div>
          <div class="sys-row"><span>Satellites</span><span id="gps-sats">0</span></div>
          <div class="sys-row"><span>HDOP</span><span id="gps-hdop">--</span></div>
          <div class="sys-row"><span>Lat</span><span id="gps-lat">--</span></div>
          <div class="sys-row"><span>Lon</span><span id="gps-lon">--</span></div>
          <div class="sys-row"><span>Alt</span><span id="gps-alt">--</span></div>
          <div class="sys-row"><span>Speed</span><span id="gps-speed">--</span></div>
          <div class="sys-row"><span>Heading</span><span id="gps-heading">--</span></div>
          <div class="sys-row"><span>Time (UTC)</span><span id="gps-time">--</span></div>
          <div class="sys-row"><span>Date</span><span id="gps-date">--</span></div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header"><h2>Microphone</h2></div>
        <div class="card-body compact">
          <div class="sys-row"><span>Status</span><span id="mic-status">N/C</span></div>
          <div class="mic-level-container">
            <div class="mic-level-bar">
              <div class="mic-level-fill" id="mic-level-fill"></div>
            </div>
            <span class="mic-db" id="mic-db">-- dB</span>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-header"><h2>Controls</h2></div>
        <div class="card-body compact">
          <div class="sys-row">
            <span>Cooling Fans</span>
            <button class="toggle-btn" id="fan-toggle-btn" onclick="toggleFan()">
              <span id="fan-status-text">OFF</span>
            </button>
          </div>
        </div>
      </div>
      </div>
    </section>
    
    <footer>
      <p>Lucidius - ARCOS Framework</p>
    </footer>
  </div>
  
  <div id="toast" class="toast"></div>
  
  <style>
  .mic-level-container { display: flex; align-items: center; gap: 12px; padding: 8px 0; }
  .mic-level-bar { flex: 1; height: 16px; background: #1a1a1a; border-radius: 8px; overflow: hidden; border: 1px solid #2a2a2a; }
  .mic-level-fill { height: 100%; width: 0%; background: linear-gradient(90deg, #00cc66 0%, #ffaa00 60%, #ff3333 90%); transition: width 0.1s ease-out; border-radius: 8px; }
  .mic-db { min-width: 60px; text-align: right; font-family: 'SF Mono', Monaco, monospace; font-size: 0.85rem; color: #fff; }
  
  /* IMU Bar Styles */
  .imu-section { margin-bottom: 12px; }
  .imu-section:last-child { margin-bottom: 0; }
  .imu-section-label { display: block; font-size: 0.8rem; color: #888; margin-bottom: 6px; text-transform: uppercase; letter-spacing: 0.5px; }
  .imu-bar-row { display: flex; align-items: center; gap: 8px; margin-bottom: 6px; }
  .axis-label { width: 16px; font-size: 0.85rem; color: #aaa; font-weight: 500; }
  .imu-bar { flex: 1; height: 12px; background: #1a1a1a; border-radius: 6px; position: relative; overflow: hidden; border: 1px solid #2a2a2a; }
  .imu-bar-fill { position: absolute; height: 100%; background: linear-gradient(90deg, #3b82f6 0%, #60a5fa 100%); transition: left 0.1s ease-out, width 0.1s ease-out; border-radius: 6px; }
  .imu-bar-center { position: absolute; left: 50%; top: 0; bottom: 0; width: 2px; background: #444; transform: translateX(-50%); }
  .imu-value { min-width: 50px; text-align: right; font-family: 'SF Mono', Monaco, monospace; font-size: 0.8rem; color: #fff; }

  /* Toggle Button Styles */
  .toggle-btn { padding: 6px 16px; border-radius: 6px; border: 1px solid #444; background: #2a2a2a; color: #888; font-size: 0.85rem; font-weight: 500; cursor: pointer; transition: all 0.2s ease; min-width: 60px; }
  .toggle-btn:hover { background: #333; border-color: #555; }
  .toggle-btn.active { background: #22c55e; border-color: #22c55e; color: #fff; }
  .toggle-btn.active:hover { background: #16a34a; border-color: #16a34a; }
  </style>
  
  <script>
  function toggleFan() {
    var btn = document.getElementById('fan-toggle-btn');
    btn.disabled = true;
    fetch('/api/fan/toggle', { method: 'POST' })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          updateFanUI(data.fanEnabled);
        }
        btn.disabled = false;
      })
      .catch(err => {
        console.error('Fan toggle error:', err);
        btn.disabled = false;
      });
  }

  function updateFanUI(enabled) {
    var btn = document.getElementById('fan-toggle-btn');
    var text = document.getElementById('fan-status-text');
    if (enabled) {
      btn.classList.add('active');
      text.textContent = 'ON';
    } else {
      btn.classList.remove('active');
      text.textContent = 'OFF';
    }
  }

  function formatUptime(seconds) {
    var h = Math.floor(seconds / 3600);
    var m = Math.floor((seconds % 3600) / 60);
    var s = Math.floor(seconds % 60);
    return h.toString().padStart(2,'0') + ':' + m.toString().padStart(2,'0') + ':' + s.toString().padStart(2,'0');
  }

  // Update IMU bar - centered at 0, grows left for negative, right for positive
  // Uses square root scale - gentler than log, still shows small and large changes
  function updateImuBar(barId, valueId, value, maxVal) {
    var bar = document.getElementById(barId);
    var valueEl = document.getElementById(valueId);
    if (!bar || !valueEl) return;
    
    valueEl.textContent = Math.round(value);
    
    // Square root scale: gentler compression than log
    // pct = sqrt(|value|) / sqrt(maxVal) * 50
    var absVal = Math.abs(value);
    var pct = Math.sqrt(absVal) / Math.sqrt(maxVal) * 50;
    pct = Math.min(pct, 50); // Cap at 50%
    
    if (value >= 0) {
      // Positive: bar starts at center (50%), grows right
      bar.style.left = '50%';
      bar.style.width = pct + '%';
      bar.style.background = 'linear-gradient(90deg, #3b82f6 0%, #60a5fa 100%)';
    } else {
      // Negative: bar ends at center, grows left
      bar.style.left = (50 - pct) + '%';
      bar.style.width = pct + '%';
      bar.style.background = 'linear-gradient(90deg, #f59e0b 0%, #fbbf24 100%)';
    }
  }

  // Update Device IMU bar - similar to updateImuBar but displays floats with 2 decimal places
  function updateDeviceImuBar(barId, valueId, value, maxVal) {
    var bar = document.getElementById(barId);
    var valueEl = document.getElementById(valueId);
    if (!bar || !valueEl) return;

    valueEl.textContent = value.toFixed(2);

    // Square root scale for visual representation
    var absVal = Math.abs(value);
    var pct = Math.sqrt(absVal) / Math.sqrt(maxVal) * 50;
    pct = Math.min(pct, 50);

    if (value >= 0) {
      bar.style.left = '50%';
      bar.style.width = pct + '%';
      bar.style.background = 'linear-gradient(90deg, #10b981 0%, #34d399 100%)';
    } else {
      bar.style.left = (50 - pct) + '%';
      bar.style.width = pct + '%';
      bar.style.background = 'linear-gradient(90deg, #8b5cf6 0%, #a78bfa 100%)';
    }
  }

  var pollDelay = 200;
  function fetchState() {
    fetch('/api/state')
      .then(r => r.json())
      .then(data => {
        // System
        if (data.mode) document.getElementById('sys-mode').textContent = data.mode.charAt(0).toUpperCase() + data.mode.slice(1);
        if (data.statusText) document.getElementById('sys-status-text').textContent = data.statusText;
        if (data.uptime !== undefined) document.getElementById('sys-uptime').textContent = formatUptime(data.uptime);
        if (data.freeHeap !== undefined) document.getElementById('sys-heap').textContent = Math.round(data.freeHeap / 1024) + ' KB';
        if (data.cpuUsage !== undefined) document.getElementById('sys-cpu-usage').textContent = data.cpuUsage.toFixed(1) + '%';
        if (data.fps !== undefined) document.getElementById('sys-fps').textContent = data.fps.toFixed(1);
        
        // GPU
        if (data.gpuConnected !== undefined) {
          var gpuDot = document.getElementById('conn-gpu-dot');
          var gpuText = document.getElementById('conn-gpu');
          gpuDot.className = data.gpuConnected ? 'status-dot connected' : 'status-dot';
          gpuText.textContent = data.gpuConnected ? 'Connected' : 'N/C';
        }
        
        // GPU Stats
        if (data.gpu) {
          var g = data.gpu;
          document.getElementById('gpu-fps').textContent = g.fps !== undefined ? g.fps.toFixed(1) : '--';
          document.getElementById('gpu-load').textContent = g.load !== undefined ? g.load + '%' : '--';
          document.getElementById('gpu-heap').textContent = g.freeHeap !== undefined ? Math.round(g.freeHeap / 1024) + ' KB' : '--';
          document.getElementById('gpu-min-heap').textContent = g.minHeap !== undefined ? Math.round(g.minHeap / 1024) + ' KB' : '--';
          document.getElementById('gpu-uptime').textContent = g.uptime !== undefined ? formatUptime(Math.floor(g.uptime / 1000)) : '--';
          document.getElementById('gpu-frames').textContent = g.totalFrames !== undefined ? g.totalFrames.toLocaleString() : '--';
          
          var hub75Dot = document.getElementById('gpu-hub75-dot');
          var hub75Text = document.getElementById('gpu-hub75');
          hub75Dot.className = g.hub75Ok ? 'status-dot connected' : 'status-dot';
          hub75Text.textContent = g.hub75Ok ? 'OK' : 'N/C';
          
          var oledDot = document.getElementById('gpu-oled-dot');
          var oledText = document.getElementById('gpu-oled');
          oledDot.className = g.oledOk ? 'status-dot connected' : 'status-dot';
          oledText.textContent = g.oledOk ? 'OK' : 'N/C';
        }
        
        // Environment
        if (data.sensors) {
          var s = data.sensors;
          document.getElementById('sens-temp').textContent = s.temperature !== 0 ? s.temperature.toFixed(1) + ' C' : 'N/C';
          document.getElementById('sens-humidity').textContent = s.humidity !== 0 ? s.humidity.toFixed(1) + '%' : 'N/C';
          document.getElementById('sens-pressure').textContent = s.pressure !== 0 ? s.pressure.toFixed(0) + ' hPa' : 'N/C';
        }
        
        // IMU with visual bars (raw values)
        if (data.imu) {
          var ACCEL_MAX = 2000;  // Max accelerometer range
          var GYRO_MAX = 250;    // Max gyroscope range
          
          // Update accelerometer with raw values
          updateImuBar('accel-x-bar', 'imu-ax', data.imu.accelX, ACCEL_MAX);
          updateImuBar('accel-y-bar', 'imu-ay', data.imu.accelY, ACCEL_MAX);
          updateImuBar('accel-z-bar', 'imu-az', data.imu.accelZ, ACCEL_MAX);
          
          // Update gyroscope with raw values
          updateImuBar('gyro-x-bar', 'imu-gx', data.imu.gyroX, GYRO_MAX);
          updateImuBar('gyro-y-bar', 'imu-gy', data.imu.gyroY, GYRO_MAX);
          updateImuBar('gyro-z-bar', 'imu-gz', data.imu.gyroZ, GYRO_MAX);
        }

        // Device IMU (calibrated values - floats in g and deg/s)
        if (data.deviceImu) {
          var DEV_ACCEL_MAX = 2.0;   // Max 2g for device accel
          var DEV_GYRO_MAX = 250.0;  // Max 250 deg/s for device gyro

          // Update device accelerometer
          updateDeviceImuBar('dev-accel-x-bar', 'dev-ax', data.deviceImu.accelX, DEV_ACCEL_MAX);
          updateDeviceImuBar('dev-accel-y-bar', 'dev-ay', data.deviceImu.accelY, DEV_ACCEL_MAX);
          updateDeviceImuBar('dev-accel-z-bar', 'dev-az', data.deviceImu.accelZ, DEV_ACCEL_MAX);

          // Update device gyroscope
          updateDeviceImuBar('dev-gyro-x-bar', 'dev-gx', data.deviceImu.gyroX, DEV_GYRO_MAX);
          updateDeviceImuBar('dev-gyro-y-bar', 'dev-gy', data.deviceImu.gyroY, DEV_GYRO_MAX);
          updateDeviceImuBar('dev-gyro-z-bar', 'dev-gz', data.deviceImu.gyroZ, DEV_GYRO_MAX);
        }

        // GPS
        if (data.gps) {
          var g = data.gps;
          var gpsStatus = 'N/C';
          if (g.valid) gpsStatus = 'Lock';
          else if (g.satellites > 0) gpsStatus = 'Searching (' + g.satellites + ')';
          document.getElementById('gps-status-text').textContent = gpsStatus;
          document.getElementById('gps-sats').textContent = g.satellites;
          document.getElementById('gps-hdop').textContent = g.valid ? g.hdop.toFixed(1) : '--';
          document.getElementById('gps-lat').textContent = g.valid ? g.latitude.toFixed(6) : '--';
          document.getElementById('gps-lon').textContent = g.valid ? g.longitude.toFixed(6) : '--';
          document.getElementById('gps-alt').textContent = g.valid ? g.altitude.toFixed(1) + ' m' : '--';
          document.getElementById('gps-speed').textContent = g.valid ? g.speed.toFixed(1) + ' km/h' : '--';
          document.getElementById('gps-heading').textContent = g.valid ? g.heading.toFixed(0) + '\u00B0' : '--';
          document.getElementById('gps-time').textContent = (g.valid && g.time !== '00:00:00') ? g.time : '--';
          document.getElementById('gps-date').textContent = (g.valid && g.date !== '0000-00-00') ? g.date : '--';
        }
        
        // Mic with level bar
        if (data.micConnected !== undefined) {
          document.getElementById('mic-status').textContent = data.micConnected ? 'OK' : 'N/C';
          if (data.micConnected && data.micDb !== undefined) {
            var db = data.micDb;
            var pct = Math.max(0, Math.min(100, (db + 60) / 60 * 100)); // -60dB to 0dB range
            document.getElementById('mic-level-fill').style.width = pct + '%';
            document.getElementById('mic-db').textContent = db.toFixed(1) + ' dB';
          } else {
            document.getElementById('mic-level-fill').style.width = '0%';
            document.getElementById('mic-db').textContent = '-- dB';
          }
        }

        // Fan state
        if (data.fanEnabled !== undefined) {
          updateFanUI(data.fanEnabled);
        }
        setTimeout(fetchState, pollDelay);
      })
      .catch(err => {
        console.error('Fetch error:', err);
        setTimeout(fetchState, pollDelay * 2);
      });
  }
  fetchState();
  </script>
</body>
</html>
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
