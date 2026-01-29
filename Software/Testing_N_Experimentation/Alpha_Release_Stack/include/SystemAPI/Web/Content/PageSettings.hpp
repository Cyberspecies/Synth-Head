/*****************************************************************
 * @file PageSettings.hpp
 * @brief Settings tab page content
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_SETTINGS[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Settings</title>
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
      <a href="/system" class="tab">System</a>
      <a href="/settings" class="tab active">Settings</a>
    </nav>
    
    <section class="tab-content active">
      <div class="card-grid">
      <div class="card">
        <div class="card-header">
          <h2>WiFi Configuration</h2>
        </div>
        <div class="card-body">
          <div class="current-wifi">
            <span class="wifi-label">Current Network:</span>
            <span class="wifi-value" id="current-ssid">Loading...</span>
            <span class="wifi-badge" id="wifi-mode-badge">Auto</span>
          </div>
          
          <div class="form-group">
            <label for="custom-ssid">Network Name (SSID)</label>
            <input type="text" id="custom-ssid" class="input" placeholder="Enter custom SSID" maxlength="32">
          </div>
          
          <div class="form-group">
            <label for="custom-password">Password</label>
            <div class="password-input-wrapper">
              <input type="password" id="custom-password" class="input" placeholder="Enter password (8-12 chars)" minlength="8" maxlength="12">
              <button type="button" class="password-toggle" id="toggle-password">Show</button>
            </div>
            <span class="input-hint">Password must be 8-12 characters</span>
          </div>
          
          <div class="button-group">
            <button id="save-wifi-btn" class="btn btn-primary">Save Changes</button>
            <button id="reset-wifi-btn" class="btn btn-secondary">Reset to Auto</button>
          </div>
          
          <div class="warning-box" id="restart-warning" style="display: none;">
            <span class="warning-icon"></span>
            <span class="warning-text">Restart required to apply WiFi changes</span>
          </div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header">
          <h2>Device Info</h2>
        </div>
        <div class="card-body">
          <div class="info-list">
            <div class="info-row">
              <span class="info-label">Firmware</span>
              <span class="info-value">v1.0.0</span>
            </div>
            <div class="info-row">
              <span class="info-label">Uptime</span>
              <span class="info-value" id="device-uptime">00:00:00</span>
            </div>
            <div class="info-row">
              <span class="info-label">Free Memory</span>
              <span class="info-value" id="device-heap">-- KB</span>
            </div>
          </div>
        </div>
      </div>
      
      <div class="card warning-card">
        <div class="card-header">
          <h2>External Network Connection</h2>
        </div>
        <div class="card-body">
          <div class="warning-box critical">
            <span class="warning-icon"></span>
            <div class="warning-content">
              <strong>Security Warning</strong>
              <p>Enabling this exposes your device to everyone on the external network.</p>
            </div>
          </div>
          
          <div class="toggle-row master-toggle">
            <div class="toggle-info">
              <span class="toggle-label">Enable External Network Mode</span>
              <span class="toggle-hint">Connect to external WiFi with authentication</span>
            </div>
            <label class="toggle-switch">
              <input type="checkbox" id="ext-mode-enable">
              <span class="toggle-slider"></span>
            </label>
          </div>
          
          <!-- Expandable Setup Steps -->
          <div class="setup-wizard" id="setup-wizard" style="display:none;">
            
            <!-- Step 1: Authentication -->
            <div class="wizard-step" id="step-1">
              <div class="step-header">
                <span class="step-number">1</span>
                <span class="step-title">Set Up Authentication</span>
                <span class="step-status" id="step-1-status"></span>
              </div>
              <div class="step-content" id="step-1-content">
                <p class="step-desc">Create login credentials to protect your device.</p>
                <div class="form-group">
                  <label for="auth-username">Username</label>
                  <input type="text" id="auth-username" class="input" placeholder="Username" maxlength="32" value="admin">
                </div>
                <div class="form-group">
                  <label for="auth-password">Password</label>
                  <div class="password-input-wrapper">
                    <input type="password" id="auth-password" class="input" placeholder="Min 6 characters" maxlength="32">
                    <button type="button" class="password-toggle" id="toggle-auth-password">Show</button>
                  </div>
                </div>
                <div class="form-group">
                  <label for="auth-password-confirm">Confirm Password</label>
                  <input type="password" id="auth-password-confirm" class="input" placeholder="Confirm password" maxlength="32">
                </div>
                <button class="btn btn-primary btn-sm" id="save-auth-btn">Save Credentials</button>
              </div>
            </div>
            
            <!-- Step 2: Network Selection -->
            <div class="wizard-step" id="step-2">
              <div class="step-header">
                <span class="step-number">2</span>
                <span class="step-title">Select Network</span>
                <span class="step-status" id="step-2-status"></span>
              </div>
              <div class="step-content" id="step-2-content" style="display:none;">
                <p class="step-desc">Choose the WiFi network to connect to.</p>
                <div class="scan-controls">
                  <button id="scan-networks-btn" class="btn btn-secondary btn-sm">Scan Networks</button>
                </div>
                <div class="network-list" id="network-list">
                  <div class="network-empty">Click scan to find networks</div>
                </div>
                <div class="form-group" style="margin-top:12px;">
                  <label for="ext-ssid">Or enter manually</label>
                  <input type="text" id="ext-ssid" class="input" placeholder="Network SSID" maxlength="32">
                </div>
                <div class="form-group">
                  <label for="ext-password">Network Password</label>
                  <div class="password-input-wrapper">
                    <input type="password" id="ext-password" class="input" placeholder="WiFi password" maxlength="64">
                    <button type="button" class="password-toggle" id="toggle-ext-password">Show</button>
                  </div>
                </div>
                <button class="btn btn-primary btn-sm" id="save-network-btn">Save Network</button>
              </div>
            </div>
            
            <!-- Step 3: Connect -->
            <div class="wizard-step" id="step-3">
              <div class="step-header">
                <span class="step-number">3</span>
                <span class="step-title">Connect</span>
                <span class="step-status" id="step-3-status"></span>
              </div>
              <div class="step-content" id="step-3-content" style="display:none;">
                <p class="step-desc">Ready to connect to external network.</p>
                <div class="connection-summary" id="connection-summary">
                  <div class="summary-row"><span>Network:</span><span id="summary-ssid">--</span></div>
                  <div class="summary-row"><span>Auth:</span><span id="summary-auth">Not configured</span></div>
                </div>
                <div class="toggle-row" style="margin-top:16px;">
                  <div class="toggle-info">
                    <span class="toggle-label">Connect Now</span>
                    <span class="toggle-hint">Toggle to connect/disconnect instantly</span>
                  </div>
                  <label class="toggle-switch">
                    <input type="checkbox" id="ext-wifi-connect">
                    <span class="toggle-slider"></span>
                  </label>
                </div>
                <div class="ext-wifi-status" id="ext-wifi-status" style="display:none;margin-top:12px;">
                  <div class="status-row"><span>Status:</span><span id="ext-status-text">Disconnected</span></div>
                  <div class="status-row"><span>IP:</span><span id="ext-ip-addr">--</span></div>
                  <div class="status-row"><span>Signal:</span><span id="ext-rssi">--</span></div>
                </div>
              </div>
            </div>
            
          </div>
        </div>
      </div>
      
      <!-- IMU Calibration Card -->
      <div class="card">
        <div class="card-header">
          <h2>IMU Calibration</h2>
        </div>
        <div class="card-body">
          <p style="font-size: 0.85rem; color: var(--text-muted); margin-bottom: 12px;">
            Calibrate the IMU to align sensor readings with device orientation. 
            Place device flat and still during calibration.
          </p>
          
          <div class="info-list" style="margin-bottom: 16px;">
            <div class="info-row">
              <span class="info-label">Status</span>
              <span class="info-value" id="imu-calib-status">Checking...</span>
            </div>
            <div class="info-row" id="imu-live-row" style="display: none;">
              <span class="info-label">Device Accel</span>
              <span class="info-value" id="imu-device-accel">X: 0 Y: 0 Z: 0</span>
            </div>
            <div class="info-row" id="imu-gyro-row" style="display: none;">
              <span class="info-label">Device Gyro</span>
              <span class="info-value" id="imu-device-gyro">X: 0 Y: 0 Z: 0</span>
            </div>
          </div>
          
          <!-- Progress bar (hidden by default) -->
          <div id="imu-calib-progress" style="display: none; margin-bottom: 16px;">
            <div style="display: flex; justify-content: space-between; font-size: 0.75rem; margin-bottom: 4px;">
              <span>Recording...</span>
              <span id="imu-calib-countdown">3.0s</span>
            </div>
            <div style="height: 8px; background: var(--bg-secondary); border-radius: 4px; overflow: hidden;">
              <div id="imu-calib-bar" style="height: 100%; background: var(--accent); width: 0%; transition: width 0.1s;"></div>
            </div>
          </div>
          
          <div class="button-group">
            <button id="imu-calibrate-btn" class="btn btn-primary">Start Calibration</button>
            <button id="imu-clear-btn" class="btn btn-secondary">Clear Calibration</button>
          </div>
        </div>
      </div>
      
      <!-- SD Card Management -->
      <div class="card">
        <div class="card-header">
          <h2>SD Card Storage</h2>
        </div>
        <div class="card-body">
          <div class="info-list" style="margin-bottom: 16px;">
            <div class="info-row">
              <span class="info-label">Status</span>
              <span class="info-value" id="sd-status">Checking...</span>
            </div>
            <div class="info-row" id="sd-name-row" style="display: none;">
              <span class="info-label">Card Name</span>
              <span class="info-value" id="sd-name">-</span>
            </div>
            <div class="info-row" id="sd-size-row" style="display: none;">
              <span class="info-label">Total Size</span>
              <span class="info-value" id="sd-total">0 MB</span>
            </div>
            <div class="info-row" id="sd-usage-row" style="display: none;">
              <span class="info-label">Usage</span>
              <span class="info-value" id="sd-usage">0 / 0 MB</span>
            </div>
          </div>
          
          <!-- SD Progress bar (for operations) -->
          <div id="sd-progress" style="display: none; margin-bottom: 16px;">
            <div style="height: 8px; background: var(--bg-secondary); border-radius: 4px; overflow: hidden;">
              <div id="sd-progress-bar" style="height: 100%; background: var(--accent); width: 0%; transition: width 0.3s;"></div>
            </div>
            <div style="text-align: center; font-size: 0.75rem; margin-top: 4px;" id="sd-progress-text">Working...</div>
          </div>
          
          <div class="button-group" id="sd-buttons" style="display: none;">
            <button id="sd-format-btn" class="btn btn-danger">Format &amp; Initialize Card</button>
          </div>
          
          <!-- Format confirmation dialog -->
          <div id="sd-confirm-format" class="danger-confirm" style="display: none; margin-top: 16px; padding: 16px; background: var(--danger-bg, rgba(239, 68, 68, 0.1)); border: 1px solid var(--danger, #ef4444); border-radius: 8px;">
            <p style="margin: 0 0 8px 0; font-weight: 600; color: var(--danger, #ef4444);">🚨 Complete Format &amp; Initialize</p>
            <p style="margin: 0 0 8px 0; font-size: 0.875rem;">This will:</p>
            <ul style="margin: 0 0 12px 0; padding-left: 20px; font-size: 0.875rem;">
              <li>Completely erase the SD card</li>
              <li>Create fresh folder structure (Scenes, Sprites, etc.)</li>
              <li>Populate with default scene configurations</li>
            </ul>
            <p style="margin: 0 0 12px 0; font-size: 0.875rem; color: var(--danger, #ef4444);"><strong>ALL existing data will be permanently destroyed.</strong></p>
            <div class="button-group">
              <button id="sd-confirm-format-btn" class="btn btn-danger">Yes, Format &amp; Initialize</button>
              <button id="sd-cancel-format-btn" class="btn btn-secondary">Cancel</button>
            </div>
          </div>
        </div>
      </div>
      
      <div class="card danger-card">
        <div class="card-header">
          <h2>Danger Zone</h2>
        </div>
        <div class="card-body">
          <button id="kick-clients-btn" class="btn btn-warning">Kick All Other Clients</button>
          <button id="restart-btn" class="btn btn-danger">Restart Device</button>
        </div>
      </div>
      </div>
    </section>
    
    <footer>
      <p>Lucidius - ARCOS Framework</p>
    </footer>
  </div>
  
  <div id="toast" class="toast"></div>
  
  <script>
  function formatUptime(seconds) {
    var h = Math.floor(seconds / 3600);
    var m = Math.floor((seconds % 3600) / 60);
    var s = Math.floor(seconds % 60);
    return h.toString().padStart(2,'0') + ':' + m.toString().padStart(2,'0') + ':' + s.toString().padStart(2,'0');
  }
  
  function showToast(message, type) {
    type = type || 'info';
    var toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className = 'toast ' + type + ' show';
    setTimeout(function() { toast.className = 'toast'; }, 3000);
  }
  
  function sendCommand(cmd, data) {
    data = data || {};
    data.cmd = cmd;
    fetch('/api/command', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(data)
    })
    .then(r => r.json())
    .then(res => { if (res.success) showToast('Command sent', 'success'); })
    .catch(err => showToast('Error: ' + err, 'error'));
  }
  
  function fetchState() {
    fetch('/api/state')
      .then(r => r.json())
      .then(data => {
        if (data.ssid) document.getElementById('current-ssid').textContent = data.ssid;
        if (data.uptime !== undefined) document.getElementById('device-uptime').textContent = formatUptime(data.uptime);
        if (data.freeHeap !== undefined) document.getElementById('device-heap').textContent = Math.round(data.freeHeap / 1024) + ' KB';
      })
      .catch(err => console.error('Fetch error:', err));
  }
  
  // Password toggle
  document.getElementById('toggle-password').addEventListener('click', function() {
    var input = document.getElementById('custom-password');
    var btn = document.getElementById('toggle-password');
    if (input.type === 'password') {
      input.type = 'text';
      btn.innerHTML = 'Hide';
    } else {
      input.type = 'password';
      btn.innerHTML = 'Show';
    }
  });
  
  // Save WiFi
  document.getElementById('save-wifi-btn').addEventListener('click', function() {
    var ssid = document.getElementById('custom-ssid').value.trim();
    var password = document.getElementById('custom-password').value;
    if (!ssid) { showToast('Please enter an SSID', 'error'); return; }
    if (password.length < 8 || password.length > 12) { showToast('Password must be 8-12 characters', 'error'); return; }
    sendCommand('setWifiCredentials', { ssid: ssid, password: password });
    document.getElementById('restart-warning').style.display = 'flex';
  });
  
  // Reset WiFi
  document.getElementById('reset-wifi-btn').addEventListener('click', function() {
    if (confirm('Reset to auto-generated WiFi credentials?')) {
      sendCommand('resetWifiToAuto');
      document.getElementById('custom-ssid').value = '';
      document.getElementById('custom-password').value = '';
      document.getElementById('restart-warning').style.display = 'flex';
    }
  });
  
  // Restart
  document.getElementById('restart-btn').addEventListener('click', function() {
    if (confirm('Are you sure you want to restart the device?')) {
      sendCommand('restart');
      showToast('Restarting device...', 'warning');
    }
  });
  
  // Kick clients
  document.getElementById('kick-clients-btn').addEventListener('click', function() {
    if (confirm('Disconnect all other devices from this network?')) {
      sendCommand('kickOtherClients');
      showToast('Kicking other clients...', 'warning');
    }
  });
  
  // ===== External Network Wizard Logic =====
  var wizardState = {
    authConfigured: false,
    networkConfigured: false,
    username: 'admin',
    password: '',
    ssid: '',
    netPassword: ''
  };
  
  // Master toggle - expand/collapse wizard
  document.getElementById('ext-mode-enable').addEventListener('change', function() {
    var wizard = document.getElementById('setup-wizard');
    if (this.checked) {
      wizard.style.display = 'block';
      updateWizardState();
    } else {
      wizard.style.display = 'none';
      // Disconnect if connected
      var connectToggle = document.getElementById('ext-wifi-connect');
      if (connectToggle.checked) {
        connectToggle.checked = false;
        sendCommand('extWifiConnect', { connect: false });
      }
      sendCommand('setExtWifi', { enabled: false });
    }
  });
  
  // Step header clicks to expand/collapse
  document.querySelectorAll('.step-header').forEach(function(header) {
    header.addEventListener('click', function() {
      var step = this.parentElement;
      var content = step.querySelector('.step-content');
      var isVisible = content.style.display !== 'none';
      content.style.display = isVisible ? 'none' : 'block';
    });
  });
  
  // Step 1: Save Auth
  document.getElementById('save-auth-btn').addEventListener('click', function() {
    var username = document.getElementById('auth-username').value.trim();
    var password = document.getElementById('auth-password').value;
    var confirm = document.getElementById('auth-password-confirm').value;
    
    if (!username) { showToast('Please enter a username', 'error'); return; }
    if (!password || password.length < 6) { showToast('Password must be at least 6 characters', 'error'); return; }
    if (password !== confirm) { showToast('Passwords do not match', 'error'); return; }
    
    wizardState.authConfigured = true;
    wizardState.username = username;
    wizardState.password = password;
    
    sendCommand('setAuth', { enabled: true, username: username, password: password });
    showToast('Credentials saved!', 'success');
    
    updateWizardState();
    // Auto-expand next step
    document.getElementById('step-1-content').style.display = 'none';
    document.getElementById('step-2-content').style.display = 'block';
  });
  
  // Step 1: Password toggle
  document.getElementById('toggle-auth-password').addEventListener('click', function() {
    var p1 = document.getElementById('auth-password');
    var p2 = document.getElementById('auth-password-confirm');
    var show = p1.type === 'password';
    p1.type = show ? 'text' : 'password';
    p2.type = show ? 'text' : 'password';
    this.innerHTML = show ? 'Hide' : 'Show';
  });
  
  // Step 2: Scan networks
  document.getElementById('scan-networks-btn').addEventListener('click', function() {
    var listEl = document.getElementById('network-list');
    listEl.innerHTML = '<div class="scanning"><div class="scan-spinner"></div><div>Scanning...</div></div>';
    
    fetch('/api/scan')
      .then(r => r.json())
      .then(data => {
        if (!data.networks || data.networks.length === 0) {
          listEl.innerHTML = '<div class="network-empty">No networks found</div>';
          return;
        }
        var html = '';
        data.networks.forEach(function(net) {
          var signalBars = getSignalBars(net.rssi);
          html += '<div class="network-item" data-ssid="' + escapeHtml(net.ssid) + '">';
          html += '<div class="network-info"><span class="network-ssid">' + escapeHtml(net.ssid) + '</span>';
          html += '<span class="network-security">' + (net.secure ? 'Secure' : 'Open') + ' ' + net.rssi + 'dB</span></div>';
          html += '<div class="network-signal">' + signalBars + '</div></div>';
        });
        listEl.innerHTML = html;
        
        listEl.querySelectorAll('.network-item').forEach(function(item) {
          item.addEventListener('click', function() {
            listEl.querySelectorAll('.network-item').forEach(el => el.classList.remove('selected'));
            item.classList.add('selected');
            document.getElementById('ext-ssid').value = item.getAttribute('data-ssid');
          });
        });
      })
      .catch(function(err) {
        listEl.innerHTML = '<div class="network-empty">Scan failed</div>';
      });
  });
  
  function getSignalBars(rssi) {
    var bars = '<div class="signal-bars">';
    bars += '<div class="signal-bar' + (rssi > -90 ? ' active' : '') + '"></div>';
    bars += '<div class="signal-bar' + (rssi > -75 ? ' active' : '') + '"></div>';
    bars += '<div class="signal-bar' + (rssi > -60 ? ' active' : '') + '"></div>';
    bars += '<div class="signal-bar' + (rssi > -45 ? ' active' : '') + '"></div>';
    bars += '</div>';
    return bars;
  }
  
  function escapeHtml(text) {
    var div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }
  
  // Step 2: Save Network
  document.getElementById('save-network-btn').addEventListener('click', function() {
    var ssid = document.getElementById('ext-ssid').value.trim();
    var password = document.getElementById('ext-password').value;
    
    if (!ssid) { showToast('Please enter or select a network', 'error'); return; }
    
    wizardState.networkConfigured = true;
    wizardState.ssid = ssid;
    wizardState.netPassword = password;
    
    sendCommand('setExtWifi', { enabled: true, ssid: ssid, password: password });
    showToast('Network saved!', 'success');
    
    updateWizardState();
    // Auto-expand next step
    document.getElementById('step-2-content').style.display = 'none';
    document.getElementById('step-3-content').style.display = 'block';
  });
  
  // Step 2: Password toggle
  document.getElementById('toggle-ext-password').addEventListener('click', function() {
    var input = document.getElementById('ext-password');
    var show = input.type === 'password';
    input.type = show ? 'text' : 'password';
    this.innerHTML = show ? 'Hide' : 'Show';
  });
  
  // Step 3: Connect toggle
  document.getElementById('ext-wifi-connect').addEventListener('change', function() {
    var shouldConnect = this.checked;
    sendCommand('extWifiConnect', { connect: shouldConnect });
    
    var statusEl = document.getElementById('ext-wifi-status');
    var textEl = document.getElementById('ext-status-text');
    
    if (shouldConnect) {
      statusEl.style.display = 'block';
      textEl.textContent = 'Connecting...';
      textEl.className = 'status-value';
    } else {
      textEl.textContent = 'Disconnected';
      textEl.className = 'status-value disconnected';
    }
  });
  
  function updateWizardState() {
    // Update step indicators
    var s1 = document.getElementById('step-1-status');
    var s2 = document.getElementById('step-2-status');
    var s3 = document.getElementById('step-3-status');
    
    s1.className = 'step-status' + (wizardState.authConfigured ? ' done' : '');
    
    s2.className = 'step-status' + (wizardState.networkConfigured ? ' done' : '');
    
    var ready = wizardState.authConfigured && wizardState.networkConfigured;
    s3.className = 'step-status' + (ready ? '' : ' locked');
    
    // Update summary
    document.getElementById('summary-ssid').textContent = wizardState.ssid || '--';
    document.getElementById('summary-auth').textContent = wizardState.authConfigured ? 
      wizardState.username + ' (configured)' : 'Not configured';
    
    // Enable/disable connect toggle
    document.getElementById('ext-wifi-connect').disabled = !ready;
  }
  
  // Update from server state
  function updateExtWifiState(data) {
    if (data.extWifiEnabled !== undefined && data.extWifiEnabled) {
      document.getElementById('ext-mode-enable').checked = true;
      document.getElementById('setup-wizard').style.display = 'block';
    }
    if (data.extWifiSSID && data.extWifiSSID.length > 0) {
      wizardState.networkConfigured = true;
      wizardState.ssid = data.extWifiSSID;
      document.getElementById('ext-ssid').value = data.extWifiSSID;
    }
    if (data.authEnabled && data.authUsername) {
      wizardState.authConfigured = true;
      wizardState.username = data.authUsername;
      document.getElementById('auth-username').value = data.authUsername;
    }
    if (data.extWifiConnected !== undefined) {
      document.getElementById('ext-wifi-connect').checked = data.extWifiConnected;
    }
    if (data.extWifiIsConnected !== undefined && data.extWifiConnected) {
      var statusEl = document.getElementById('ext-wifi-status');
      var textEl = document.getElementById('ext-status-text');
      statusEl.style.display = 'block';
      if (data.extWifiIsConnected) {
        textEl.textContent = 'Connected';
        textEl.className = 'status-value connected';
        document.getElementById('ext-ip-addr').textContent = data.extWifiIP || '--';
        document.getElementById('ext-rssi').textContent = data.extWifiRSSI ? data.extWifiRSSI + ' dB' : '--';
      } else {
        textEl.textContent = 'Connecting...';
      }
    }
    updateWizardState();
  }
  
  // ===== IMU Calibration Logic =====
  var imuCalibrating = false;
  var imuCalibPollInterval = null;
  
  function updateImuStatus() {
    fetch('/api/imu/status')
      .then(r => r.json())
      .then(data => {
        var statusEl = document.getElementById('imu-calib-status');
        var progressEl = document.getElementById('imu-calib-progress');
        var calibBtn = document.getElementById('imu-calibrate-btn');
        
        if (data.calibrating) {
          imuCalibrating = true;
          statusEl.textContent = 'Calibrating...';
          statusEl.style.color = 'var(--warning)';
          progressEl.style.display = 'block';
          calibBtn.disabled = true;
          
          var progress = data.progress || 0;
          var remaining = (data.remainingMs || 0) / 1000;
          document.getElementById('imu-calib-bar').style.width = progress + '%';
          document.getElementById('imu-calib-countdown').textContent = remaining.toFixed(1) + 's';
          
          if (!imuCalibPollInterval) {
            imuCalibPollInterval = setInterval(updateImuStatus, 100);
          }
        } else {
          if (imuCalibrating) {
            // Just finished
            showToast('IMU calibration complete!', 'success');
            imuCalibrating = false;
          }
          
          if (imuCalibPollInterval) {
            clearInterval(imuCalibPollInterval);
            imuCalibPollInterval = null;
          }
          
          progressEl.style.display = 'none';
          calibBtn.disabled = false;
          
          if (data.calibrated) {
            statusEl.textContent = 'Calibrated ✓';
            statusEl.style.color = 'var(--success)';
            document.getElementById('imu-live-row').style.display = 'flex';
            document.getElementById('imu-gyro-row').style.display = 'flex';
          } else {
            statusEl.textContent = 'Not calibrated';
            statusEl.style.color = 'var(--text-muted)';
            document.getElementById('imu-live-row').style.display = 'none';
            document.getElementById('imu-gyro-row').style.display = 'none';
          }
        }
      })
      .catch(err => {
        document.getElementById('imu-calib-status').textContent = 'Error';
      });
  }
  
  function updateImuLiveValues() {
    fetch('/api/sensors')
      .then(r => r.json())
      .then(data => {
        if (data.imu_calibrated) {
          var ax = (data.device_accel_x || 0).toFixed(1);
          var ay = (data.device_accel_y || 0).toFixed(1);
          var az = (data.device_accel_z || 0).toFixed(1);
          document.getElementById('imu-device-accel').textContent = 'X:' + ax + ' Y:' + ay + ' Z:' + az;
          var gx = (data.device_gyro_x || 0).toFixed(1);
          var gy = (data.device_gyro_y || 0).toFixed(1);
          var gz = (data.device_gyro_z || 0).toFixed(1);
          document.getElementById('imu-device-gyro').textContent = 'X:' + gx + ' Y:' + gy + ' Z:' + gz;
        }
      })
      .catch(function() {});
  }
  
  document.getElementById('imu-calibrate-btn').addEventListener('click', function() {
    if (confirm('Place the device flat and keep it still. Start calibration?')) {
      fetch('/api/imu/calibrate', { method: 'POST' })
        .then(r => r.json())
        .then(data => {
          showToast('Calibration started - keep device still!', 'info');
          updateImuStatus();
        })
        .catch(err => showToast('Failed to start calibration', 'error'));
    }
  });
  
  document.getElementById('imu-clear-btn').addEventListener('click', function() {
    if (confirm('Clear IMU calibration?')) {
      fetch('/api/imu/clear', { method: 'POST' })
        .then(r => r.json())
        .then(data => {
          showToast('Calibration cleared', 'success');
          updateImuStatus();
        })
        .catch(err => showToast('Failed to clear calibration', 'error'));
    }
  });
  
  // Initial IMU status check
  updateImuStatus();
  setInterval(updateImuLiveValues, 500);
  
  // ========== SD Card Management ==========
  var sdOperationInProgress = false;
  
  function updateSdCardStatus() {
    fetch('/api/sdcard/status')
      .then(r => r.json())
      .then(data => {
        var statusEl = document.getElementById('sd-status');
        var nameRow = document.getElementById('sd-name-row');
        var sizeRow = document.getElementById('sd-size-row');
        var usageRow = document.getElementById('sd-usage-row');
        var buttons = document.getElementById('sd-buttons');
        
        if (!data.initialized) {
          statusEl.textContent = 'Not initialized';
          statusEl.style.color = 'var(--danger)';
          nameRow.style.display = 'none';
          sizeRow.style.display = 'none';
          usageRow.style.display = 'none';
          buttons.style.display = 'none';
        } else if (!data.mounted) {
          statusEl.textContent = 'Not mounted';
          statusEl.style.color = 'var(--warning)';
          nameRow.style.display = 'none';
          sizeRow.style.display = 'none';
          usageRow.style.display = 'none';
          buttons.style.display = 'none';
        } else {
          statusEl.textContent = 'Ready ✓';
          statusEl.style.color = 'var(--success)';
          nameRow.style.display = 'flex';
          sizeRow.style.display = 'flex';
          usageRow.style.display = 'flex';
          buttons.style.display = 'flex';
          
          document.getElementById('sd-name').textContent = data.name || 'Unknown';
          document.getElementById('sd-total').textContent = data.total_mb + ' MB';
          document.getElementById('sd-usage').textContent = data.used_mb + ' / ' + data.total_mb + ' MB (' + Math.round(data.used_mb / data.total_mb * 100) + '%)';
        }
      })
      .catch(err => {
        document.getElementById('sd-status').textContent = 'Error';
        document.getElementById('sd-status').style.color = 'var(--danger)';
      });
  }
  
  function showSdProgress(text) {
    sdOperationInProgress = true;
    document.getElementById('sd-progress').style.display = 'block';
    document.getElementById('sd-progress-text').textContent = text;
    document.getElementById('sd-progress-bar').style.width = '0%';
    document.getElementById('sd-buttons').style.display = 'none';
    
    // Animate progress bar
    var progress = 0;
    var interval = setInterval(function() {
      if (!sdOperationInProgress) {
        clearInterval(interval);
        return;
      }
      progress += Math.random() * 15;
      if (progress > 90) progress = 90;
      document.getElementById('sd-progress-bar').style.width = progress + '%';
    }, 200);
  }
  
  function hideSdProgress() {
    sdOperationInProgress = false;
    document.getElementById('sd-progress-bar').style.width = '100%';
    setTimeout(function() {
      document.getElementById('sd-progress').style.display = 'none';
      document.getElementById('sd-buttons').style.display = 'flex';
    }, 500);
  }
  
  // Format & Initialize button click - show confirmation
  document.getElementById('sd-format-btn').addEventListener('click', function() {
    document.getElementById('sd-confirm-format').style.display = 'block';
  });
  
  // Cancel format
  document.getElementById('sd-cancel-format-btn').addEventListener('click', function() {
    document.getElementById('sd-confirm-format').style.display = 'none';
  });
  
  // Confirm format & initialize
  document.getElementById('sd-confirm-format-btn').addEventListener('click', function() {
    document.getElementById('sd-confirm-format').style.display = 'none';
    showSdProgress('Formatting and initializing SD card...');
    
    fetch('/api/sdcard/format-init', { method: 'POST' })
      .then(r => r.json())
      .then(data => {
        hideSdProgress();
        if (data.success) {
          showToast('SD card formatted and initialized with default data', 'success');
        } else {
          showToast(data.error || 'Failed to format/initialize', 'error');
        }
        updateSdCardStatus();
      })
      .catch(err => {
        hideSdProgress();
        showToast('Error: ' + err, 'error');
      });
  });
  
  // Initial SD card status check
  updateSdCardStatus();
  
  // Extend fetchState
  fetchState = function() {
    fetch('/api/state')
      .then(r => r.json())
      .then(data => {
        if (data.ssid) document.getElementById('current-ssid').textContent = data.ssid;
        if (data.uptime !== undefined) document.getElementById('device-uptime').textContent = formatUptime(data.uptime);
        if (data.freeHeap !== undefined) document.getElementById('device-heap').textContent = Math.round(data.freeHeap / 1024) + ' KB';
        updateExtWifiState(data);
      })
      .catch(err => console.error('Fetch error:', err));
  };
  
  fetchState();
  setInterval(fetchState, 1000);
  </script>
</body>
</html>
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
