/*****************************************************************
 * @file PageEquations.hpp
 * @brief Equation/Algorithm Editor - Create equations with sensor inputs
 * 
 * Features:
 * - Create, edit, delete equations
 * - Variables: static values, sensor inputs, time, other equations
 * - Dependency checking with warnings
 * - Live sensor value display
 * - Persistence to flash storage
 * 
 * Used for animation system parameter generation
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_EQUATIONS[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Equation Editor</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    .back-link {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      color: var(--text-muted);
      text-decoration: none;
      font-size: 0.85rem;
      margin-bottom: 8px;
      transition: color 0.2s;
    }
    .back-link:hover { color: var(--accent); }
    
    /* Sensor Panel - Collapsible */
    .sensor-panel {
      background: var(--bg-tertiary);
      border-radius: 12px;
      margin-bottom: 16px;
      overflow: hidden;
    }
    .sensor-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px 16px;
      cursor: pointer;
      user-select: none;
      transition: background 0.2s;
    }
    .sensor-header:hover {
      background: var(--bg-secondary);
    }
    .sensor-header-left {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .sensor-title {
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-secondary);
    }
    .sensor-count {
      font-size: 0.7rem;
      color: var(--text-muted);
      background: var(--bg-secondary);
      padding: 2px 8px;
      border-radius: 10px;
    }
    .sensor-toggle {
      font-size: 0.8rem;
      color: var(--text-muted);
      transition: transform 0.2s;
    }
    .sensor-toggle.expanded {
      transform: rotate(180deg);
    }
    .sensor-body {
      max-height: 0;
      overflow: hidden;
      transition: max-height 0.3s ease-out;
    }
    .sensor-body.expanded {
      max-height: 500px;
      overflow-y: auto;
    }
    .sensor-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(100px, 1fr));
      gap: 4px;
      padding: 0 12px 12px 12px;
    }
    .sensor-item {
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 6px 8px;
      display: flex;
      flex-direction: row;
      align-items: center;
      justify-content: space-between;
      gap: 4px;
    }
    .sensor-name {
      font-size: 0.6rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.3px;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      max-width: 60px;
    }
    .sensor-value {
      font-size: 0.75rem;
      font-weight: 600;
      color: var(--accent);
      font-family: 'SF Mono', Monaco, monospace;
    }
    
    /* Equation List - Compact */
    .equation-list {
      display: flex;
      flex-direction: column;
      gap: 6px;
      margin-bottom: 16px;
    }
    .equation-item {
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 10px 12px;
      transition: all 0.2s;
    }
    .equation-item:hover {
      border-color: var(--accent);
    }
    .equation-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .equation-info {
      flex: 1;
      min-width: 0;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .equation-name {
      font-size: 0.9rem;
      font-weight: 600;
      color: var(--text-primary);
      white-space: nowrap;
    }
    .equation-expr {
      font-size: 0.75rem;
      font-family: 'SF Mono', Monaco, monospace;
      color: var(--text-muted);
      background: var(--bg-secondary);
      padding: 4px 8px;
      border-radius: 4px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      flex: 1;
    }
    .equation-actions {
      display: flex;
      gap: 4px;
      flex-shrink: 0;
      margin-left: 8px;
    }
    .eq-action-btn {
      width: 28px;
      height: 28px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 0.8rem;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.2s;
    }
    .eq-action-btn:hover {
      background: var(--accent-subtle);
      border-color: var(--accent);
      color: var(--accent);
    }
    .eq-action-btn.delete:hover {
      background: rgba(255, 82, 82, 0.1);
      border-color: var(--danger);
      color: var(--danger);
    }
    .no-equations {
      text-align: center;
      padding: 40px 20px;
      color: var(--text-muted);
    }
    .no-equations-icon {
      font-size: 2.5rem;
      margin-bottom: 12px;
      opacity: 0.5;
    }
    
    /* Create/Edit Form */
    .equation-form {
      background: var(--bg-tertiary);
      border-radius: 12px;
      padding: 20px;
    }
    .form-section {
      margin-bottom: 20px;
    }
    .form-section:last-child {
      margin-bottom: 0;
    }
    .form-label {
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-secondary);
      margin-bottom: 8px;
      display: block;
    }
    .form-hint {
      font-size: 0.7rem;
      color: var(--text-muted);
      margin-top: 4px;
    }
    .form-input {
      width: 100%;
      padding: 10px 12px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-size: 0.9rem;
    }
    .form-input:focus {
      border-color: var(--accent);
      outline: none;
    }
    .form-input.mono {
      font-family: 'SF Mono', Monaco, monospace;
    }
    
    /* Variables Section */
    .variables-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
      margin-bottom: 12px;
    }
    .variable-row {
      display: grid;
      grid-template-columns: 1fr 1fr auto;
      gap: 6px;
      align-items: center;
      background: var(--bg-secondary);
      padding: 8px 10px;
      border-radius: 8px;
      border: 1px solid var(--border);
    }
    .variable-row .var-value {
      grid-column: 1 / 3;
    }
    .var-input {
      padding: 8px 10px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-primary);
      font-size: 0.85rem;
    }
    .var-input:focus {
      border-color: var(--accent);
      outline: none;
    }
    .var-select {
      padding: 8px 10px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-primary);
      font-size: 0.85rem;
      cursor: pointer;
    }
    .var-select:focus {
      border-color: var(--accent);
      outline: none;
    }
    .var-remove-btn {
      width: 28px;
      height: 28px;
      background: transparent;
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 0.85rem;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.2s;
    }
    .var-remove-btn:hover {
      background: rgba(255, 82, 82, 0.1);
      border-color: var(--danger);
      color: var(--danger);
    }
    .add-var-btn {
      padding: 10px 16px;
      background: var(--bg-secondary);
      border: 1px dashed var(--border);
      border-radius: 8px;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 0.85rem;
      transition: all 0.2s;
      text-align: center;
    }
    .add-var-btn:hover {
      border-color: var(--accent);
      color: var(--accent);
      background: var(--accent-subtle);
    }
    
    /* Expression Builder */
    .expr-preview {
      margin-top: 12px;
      padding: 12px;
      background: var(--bg-secondary);
      border-radius: 8px;
      border: 1px solid var(--border);
    }
    .expr-preview-label {
      font-size: 0.7rem;
      color: var(--text-muted);
      text-transform: uppercase;
      margin-bottom: 6px;
    }
    .expr-preview-value {
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 0.9rem;
      color: var(--success);
      font-weight: 600;
    }
    .expr-error {
      color: var(--danger);
    }
    
    /* Insert buttons */
    .insert-buttons {
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
      margin-top: 8px;
    }
    .insert-btn {
      padding: 6px 10px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 0.75rem;
      font-family: 'SF Mono', Monaco, monospace;
      transition: all 0.2s;
    }
    .insert-btn:hover {
      border-color: var(--accent);
      color: var(--accent);
    }
    .insert-btn.op {
      color: var(--warning);
      border-color: rgba(255, 179, 0, 0.3);
    }
    .insert-btn.func {
      color: var(--info);
      border-color: rgba(0, 122, 255, 0.3);
    }
    
    /* Form Actions */
    .form-actions {
      display: flex;
      gap: 10px;
      padding-top: 16px;
      border-top: 1px solid var(--border);
      margin-top: 20px;
    }
    .form-actions .btn {
      flex: 1;
    }
    
    /* Dependencies Warning */
    .deps-warning {
      background: rgba(255, 179, 0, 0.1);
      border: 1px solid rgba(255, 179, 0, 0.3);
      border-radius: 8px;
      padding: 12px 16px;
      margin-bottom: 16px;
    }
    .deps-warning-title {
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--warning);
      margin-bottom: 6px;
    }
    .deps-warning-list {
      font-size: 0.8rem;
      color: var(--text-secondary);
    }
    .deps-warning-list span {
      font-family: 'SF Mono', Monaco, monospace;
      color: var(--accent);
    }
    
    /* Modal */
    .modal-overlay {
      position: fixed;
      top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(0,0,0,0.8);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 1000;
      padding: 20px;
    }
    .modal-overlay.show { display: flex; }
    .modal-box {
      background: var(--bg-card);
      border-radius: 16px;
      padding: 24px;
      width: 100%;
      max-width: 500px;
      max-height: 80vh;
      overflow-y: auto;
    }
    .modal-title {
      font-size: 1.1rem;
      font-weight: 600;
      margin-bottom: 16px;
    }
    .modal-buttons {
      display: flex;
      gap: 8px;
      margin-top: 20px;
    }
    .modal-buttons .btn { flex: 1; }
    
    /* View Toggle */
    .view-toggle {
      display: flex;
      gap: 8px;
      margin-bottom: 16px;
    }
    .view-btn {
      flex: 1;
      padding: 10px 16px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 0.85rem;
      font-weight: 500;
      transition: all 0.2s;
      text-align: center;
    }
    .view-btn:hover {
      border-color: var(--accent);
      color: var(--text-primary);
    }
    .view-btn.active {
      background: var(--accent);
      border-color: var(--accent);
      color: var(--bg-primary);
    }
    
    /* Section divider */
    .section-divider {
      height: 1px;
      background: var(--border);
      margin: 20px 0;
    }
  </style>
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
      <a href="/advanced" class="tab active">Advanced</a>
      <a href="/system" class="tab">System</a>
      <a href="/settings" class="tab">Settings</a>
    </nav>
    
    <section class="tab-content active">
      <a href="/advanced" class="back-link">&#x2190; Back to Advanced</a>
      
      <!-- Sensor Values Panel - Collapsible -->
      <div class="sensor-panel">
        <div class="sensor-header" onclick="toggleSensorPanel()">
          <div class="sensor-header-left">
            <span class="sensor-title">Available Inputs</span>
            <span class="sensor-count" id="sensor-count">52 inputs</span>
          </div>
          <span class="sensor-toggle" id="sensor-toggle">&#x25BC;</span>
        </div>
        <div class="sensor-body" id="sensor-body">
          <div class="sensor-grid" id="sensor-grid">
            <!-- Populated by JS -->
          </div>
        </div>
      </div>
      
      <!-- View Toggle -->
      <div class="view-toggle">
        <button class="view-btn active" id="view-list-btn" onclick="showView('list')">
          &#x2630; Equations
        </button>
        <button class="view-btn" id="view-create-btn" onclick="showView('create')">
          &#x002B; Create New
        </button>
      </div>
      
      <!-- Equations List View -->
      <div id="list-view">
        <div class="equation-list" id="equation-list">
          <div class="no-equations" id="no-equations">
            <div class="no-equations-icon">&#x222B;</div>
            <div>No equations created yet</div>
            <div style="font-size: 0.75rem; margin-top: 6px;">Create an equation to process sensor inputs</div>
          </div>
        </div>
      </div>
      
      <!-- Create/Edit View -->
      <div id="create-view" style="display: none;">
        <div class="card">
          <div class="card-header">
            <h2 id="form-title">Create Equation</h2>
          </div>
          <div class="card-body">
            <div class="equation-form">
              
              <!-- Equation Name -->
              <div class="form-section">
                <label class="form-label">Equation Name</label>
                <input type="text" class="form-input" id="eq-name" placeholder="e.g., brightness_level" maxlength="32">
                <div class="form-hint">Alphanumeric and underscores only. Used to reference in other equations.</div>
              </div>
              
              <!-- Variables -->
              <div class="form-section">
                <label class="form-label">Variables</label>
                <div class="variables-list" id="variables-list">
                  <!-- Variable rows added here -->
                </div>
                <button class="add-var-btn" onclick="addVariable()">+ Add Variable</button>
                <div class="form-hint">Define input variables for your equation. Variables can be static values, sensor readings, time, or other equations.</div>
              </div>
              
              <!-- Expression -->
              <div class="form-section">
                <label class="form-label">Expression</label>
                <input type="text" class="form-input mono" id="eq-expression" placeholder="e.g., (a + b) * 0.5">
                <div class="insert-buttons">
                  <button class="insert-btn op" onclick="insertExpr('+')">+</button>
                  <button class="insert-btn op" onclick="insertExpr('-')">-</button>
                  <button class="insert-btn op" onclick="insertExpr('*')">*</button>
                  <button class="insert-btn op" onclick="insertExpr('/')">/</button>
                  <button class="insert-btn op" onclick="insertExpr('%')">%</button>
                  <button class="insert-btn op" onclick="insertExpr('(')">(</button>
                  <button class="insert-btn op" onclick="insertExpr(')')">)</button>
                  <button class="insert-btn func" onclick="insertExpr('sin(')">sin</button>
                  <button class="insert-btn func" onclick="insertExpr('cos(')">cos</button>
                  <button class="insert-btn func" onclick="insertExpr('abs(')">abs</button>
                  <button class="insert-btn func" onclick="insertExpr('min(')">min</button>
                  <button class="insert-btn func" onclick="insertExpr('max(')">max</button>
                  <button class="insert-btn func" onclick="insertExpr('clamp(')">clamp</button>
                  <button class="insert-btn func" onclick="insertExpr('lerp(')">lerp</button>
                  <button class="insert-btn func" onclick="insertExpr('map(')">map</button>
                </div>
                <div class="form-hint">
                  Use variable names in your expression. Available: +, -, *, /, %, sin, cos, abs, min, max, clamp(v,min,max), lerp(a,b,t), map(v,inMin,inMax,outMin,outMax)
                </div>
                
                <div class="expr-preview">
                  <div class="expr-preview-label">Live Output</div>
                  <div class="expr-preview-value" id="expr-result">--</div>
                </div>
              </div>
              
              <!-- Dependencies Warning (shown when editing) -->
              <div class="deps-warning" id="deps-warning" style="display: none;">
                <div class="deps-warning-title">&#x26A0; This equation is used by others</div>
                <div class="deps-warning-list" id="deps-list"></div>
              </div>
              
              <!-- Form Actions -->
              <div class="form-actions">
                <button class="btn btn-secondary" onclick="cancelEdit()">Cancel</button>
                <button class="btn btn-primary" onclick="saveEquation()" id="save-btn">Save Equation</button>
              </div>
              
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
  
  <!-- Delete Confirmation Modal -->
  <div class="modal-overlay" id="delete-modal">
    <div class="modal-box">
      <div class="modal-title">Delete Equation?</div>
      <div id="delete-deps-warning" style="display: none;">
        <div class="deps-warning" style="margin-bottom: 0;">
          <div class="deps-warning-title">&#x26A0; Warning: This equation is referenced by others</div>
          <div class="deps-warning-list" id="delete-deps-list"></div>
          <div style="margin-top: 8px; font-size: 0.8rem; color: var(--text-muted);">
            Deleting this may cause errors in dependent equations.
          </div>
        </div>
      </div>
      <p id="delete-msg" style="color: var(--text-secondary); margin-top: 12px;">Are you sure you want to delete this equation?</p>
      <div class="modal-buttons">
        <button class="btn btn-secondary" onclick="closeDeleteModal()">Cancel</button>
        <button class="btn btn-danger" onclick="confirmDelete()">Delete</button>
      </div>
    </div>
  </div>
  
  <script>
  // ========== State ==========
  var equations = [];
  var editingId = null;
  var deletingId = null;
  var sensorValues = {};
  var variableCounter = 0;
  
  // Available sensors/inputs
  var availableSensors = [
    // System
    { id: 'millis', name: 'Time (ms)', type: 'system' },
    // Environment
    { id: 'temperature', name: 'Temperature', type: 'env' },
    { id: 'humidity', name: 'Humidity', type: 'env' },
    { id: 'pressure', name: 'Pressure', type: 'env' },
    // IMU - Accelerometer
    { id: 'accel_x', name: 'Accel X', type: 'imu' },
    { id: 'accel_y', name: 'Accel Y', type: 'imu' },
    { id: 'accel_z', name: 'Accel Z', type: 'imu' },
    // IMU - Gyroscope
    { id: 'gyro_x', name: 'Gyro X', type: 'imu' },
    { id: 'gyro_y', name: 'Gyro Y', type: 'imu' },
    { id: 'gyro_z', name: 'Gyro Z', type: 'imu' },
    // GPS
    { id: 'gps_lat', name: 'GPS Lat', type: 'gps' },
    { id: 'gps_lon', name: 'GPS Lon', type: 'gps' },
    { id: 'gps_alt', name: 'GPS Alt', type: 'gps' },
    { id: 'gps_speed', name: 'GPS Speed', type: 'gps' },
    { id: 'gps_sats', name: 'GPS Sats', type: 'gps' },
    { id: 'gps_unix', name: 'GPS Unix', type: 'gps' },
    { id: 'gps_hour', name: 'GPS Hour', type: 'gps' },
    { id: 'gps_min', name: 'GPS Min', type: 'gps' },
    { id: 'gps_sec', name: 'GPS Sec', type: 'gps' },
    // Microphone
    { id: 'mic_db', name: 'Mic dB', type: 'mic' }
  ];
  
  // Initialize
  loadEquations();
  renderSensorPanel();
  updateSensorValues();
  setInterval(updateSensorValues, 250);
  setInterval(evaluateExpression, 100);
  
  // ========== Sensor Panel ==========
  var sensorPanelExpanded = false;
  
  function toggleSensorPanel() {
    sensorPanelExpanded = !sensorPanelExpanded;
    var body = document.getElementById('sensor-body');
    var toggle = document.getElementById('sensor-toggle');
    if (sensorPanelExpanded) {
      body.classList.add('expanded');
      toggle.classList.add('expanded');
    } else {
      body.classList.remove('expanded');
      toggle.classList.remove('expanded');
    }
  }
  
  function renderSensorPanel() {
    var grid = document.getElementById('sensor-grid');
    grid.innerHTML = '';
    document.getElementById('sensor-count').textContent = availableSensors.length + ' inputs';
    
    availableSensors.forEach(function(sensor) {
      var item = document.createElement('div');
      item.className = 'sensor-item';
      item.title = sensor.name + ' (' + sensor.type + ')';
      item.innerHTML = 
        '<span class="sensor-name">' + sensor.id + '</span>' +
        '<span class="sensor-value" id="sensor-' + sensor.id + '">--</span>';
      grid.appendChild(item);
    });
  }
  
  function updateSensorValues() {
    // Get real sensor values from API
    fetch('/api/sensors')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      sensorValues = data;
      
      // Update display
      availableSensors.forEach(function(sensor) {
        var el = document.getElementById('sensor-' + sensor.id);
        if (el) {
          var val = sensorValues[sensor.id];
          if (val !== undefined) {
            if (typeof val === 'number') {
              // Format based on value magnitude
              if (Math.abs(val) >= 10000) {
                el.textContent = (val / 1000).toFixed(0) + 'k';
              } else if (Math.abs(val) >= 100) {
                el.textContent = val.toFixed(0);
              } else if (Math.abs(val) >= 1) {
                el.textContent = val.toFixed(1);
              } else {
                el.textContent = val.toFixed(2);
              }
            } else {
              el.textContent = val;
            }
          }
        }
      });
    })
    .catch(function() {
      // API not available - show placeholder values
      var t = Date.now();
      availableSensors.forEach(function(sensor) {
        var el = document.getElementById('sensor-' + sensor.id);
        if (el) {
          el.textContent = '--';
        }
      });
    });
  }
  
  // ========== View Switching ==========
  function showView(view) {
    document.getElementById('list-view').style.display = view === 'list' ? 'block' : 'none';
    document.getElementById('create-view').style.display = view === 'create' ? 'block' : 'none';
    document.getElementById('view-list-btn').classList.toggle('active', view === 'list');
    document.getElementById('view-create-btn').classList.toggle('active', view === 'create');
    
    if (view === 'create' && editingId === null) {
      resetForm();
    }
  }
  
  // ========== Equations CRUD ==========
  function loadEquations() {
    fetch('/api/equations')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.equations) {
        equations = data.equations;
        renderEquationList();
      }
    })
    .catch(function(err) {
      console.error('Failed to load equations:', err);
    });
  }
  
  function renderEquationList() {
    var list = document.getElementById('equation-list');
    var noEq = document.getElementById('no-equations');
    
    if (equations.length === 0) {
      noEq.style.display = 'block';
      var items = list.querySelectorAll('.equation-item');
      items.forEach(function(item) { item.remove(); });
      return;
    }
    
    noEq.style.display = 'none';
    list.innerHTML = '';
    
    equations.forEach(function(eq) {
      var item = document.createElement('div');
      item.className = 'equation-item';
      item.innerHTML = 
        '<div class="equation-header">' +
          '<div class="equation-info">' +
            '<span class="equation-name">' + escapeHtml(eq.name) + '</span>' +
            '<span class="equation-expr">' + escapeHtml(eq.expression) + '</span>' +
          '</div>' +
          '<div class="equation-actions">' +
            '<button class="eq-action-btn" onclick="editEquation(' + eq.id + ')" title="Edit">&#x270E;</button>' +
            '<button class="eq-action-btn delete" onclick="openDeleteModal(' + eq.id + ')" title="Delete">&#x2715;</button>' +
          '</div>' +
        '</div>';
      list.appendChild(item);
    });
  }
  
  function evaluateEquation(eq) {
    try {
      var vars = {};
      
      // Build variable values
      eq.variables.forEach(function(v) {
        if (v.type === 'static') {
          vars[v.name] = parseFloat(v.value) || 0;
        } else if (v.type === 'sensor') {
          vars[v.name] = sensorValues[v.value] || 0;
        } else if (v.type === 'equation') {
          var refEq = equations.find(function(e) { return e.id === parseInt(v.value); });
          if (refEq) {
            vars[v.name] = evaluateEquation(refEq);
          } else {
            vars[v.name] = 0;
          }
        }
      });
      
      return safeEval(eq.expression, vars);
    } catch (e) {
      return NaN;
    }
  }
  
  // Safe expression evaluator
  function safeEval(expr, vars) {
    // Replace variable names with values
    var evalExpr = expr;
    
    // Sort variables by name length (longest first) to avoid partial replacements
    var varNames = Object.keys(vars).sort(function(a, b) { return b.length - a.length; });
    
    varNames.forEach(function(name) {
      var regex = new RegExp('\\b' + name + '\\b', 'g');
      evalExpr = evalExpr.replace(regex, '(' + vars[name] + ')');
    });
    
    // Add math functions
    var mathFuncs = {
      sin: Math.sin,
      cos: Math.cos,
      tan: Math.tan,
      abs: Math.abs,
      floor: Math.floor,
      ceil: Math.ceil,
      round: Math.round,
      sqrt: Math.sqrt,
      pow: Math.pow,
      min: Math.min,
      max: Math.max,
      clamp: function(v, min, max) { return Math.min(Math.max(v, min), max); },
      lerp: function(a, b, t) { return a + (b - a) * t; },
      map: function(v, inMin, inMax, outMin, outMax) { 
        return (v - inMin) * (outMax - outMin) / (inMax - inMin) + outMin; 
      }
    };
    
    // Create safe eval context
    var func = new Function(
      'sin', 'cos', 'tan', 'abs', 'floor', 'ceil', 'round', 'sqrt', 'pow', 'min', 'max', 'clamp', 'lerp', 'map',
      'return ' + evalExpr
    );
    
    return func(
      mathFuncs.sin, mathFuncs.cos, mathFuncs.tan, mathFuncs.abs, 
      mathFuncs.floor, mathFuncs.ceil, mathFuncs.round, mathFuncs.sqrt, mathFuncs.pow,
      mathFuncs.min, mathFuncs.max, mathFuncs.clamp, mathFuncs.lerp, mathFuncs.map
    );
  }
  
  // ========== Variables ==========
  function addVariable() {
    var list = document.getElementById('variables-list');
    var id = variableCounter++;
    
    var row = document.createElement('div');
    row.className = 'variable-row';
    row.id = 'var-row-' + id;
    row.innerHTML = 
      '<input type="text" class="var-input" id="var-name-' + id + '" placeholder="name" maxlength="16" onchange="updateVarButtons()">' +
      '<select class="var-select" id="var-type-' + id + '" onchange="updateVarValue(' + id + ')">' +
        '<option value="static">Static</option>' +
        '<option value="sensor">Sensor</option>' +
        '<option value="equation">Equation</option>' +
      '</select>' +
      '<div class="var-value" id="var-value-container-' + id + '">' +
        '<input type="number" class="var-input" id="var-value-' + id + '" placeholder="value" step="any">' +
      '</div>' +
      '<button class="var-remove-btn" onclick="removeVariable(' + id + ')">&#x2715;</button>';
    
    list.appendChild(row);
    updateVarButtons();
  }
  
  function removeVariable(id) {
    var row = document.getElementById('var-row-' + id);
    if (row) row.remove();
    updateVarButtons();
  }
  
  function updateVarValue(id) {
    var type = document.getElementById('var-type-' + id).value;
    var container = document.getElementById('var-value-container-' + id);
    
    if (type === 'static') {
      container.innerHTML = '<input type="number" class="var-input" id="var-value-' + id + '" placeholder="value" step="any">';
    } else if (type === 'sensor') {
      var options = availableSensors.map(function(s) {
        return '<option value="' + s.id + '">' + s.name + '</option>';
      }).join('');
      container.innerHTML = '<select class="var-select" id="var-value-' + id + '">' + options + '</select>';
    } else if (type === 'equation') {
      var eqOptions = equations
        .filter(function(e) { return editingId === null || e.id !== editingId; })
        .map(function(e) { return '<option value="' + e.id + '">' + e.name + '</option>'; })
        .join('');
      if (eqOptions === '') {
        eqOptions = '<option value="">No equations available</option>';
      }
      container.innerHTML = '<select class="var-select" id="var-value-' + id + '">' + eqOptions + '</select>';
    }
  }
  
  function updateVarButtons() {
    var list = document.getElementById('variables-list');
    var rows = list.querySelectorAll('.variable-row');
    var insertArea = document.querySelector('.insert-buttons');
    
    // Remove old variable buttons
    var oldVarBtns = insertArea.querySelectorAll('.insert-btn.var');
    oldVarBtns.forEach(function(btn) { btn.remove(); });
    
    // Add buttons for current variables
    rows.forEach(function(row) {
      var nameInput = row.querySelector('input[id^="var-name-"]');
      if (nameInput && nameInput.value) {
        var btn = document.createElement('button');
        btn.className = 'insert-btn var';
        btn.textContent = nameInput.value;
        btn.onclick = function() { insertExpr(nameInput.value); };
        insertArea.appendChild(btn);
      }
    });
  }
  
  function getVariables() {
    var list = document.getElementById('variables-list');
    var rows = list.querySelectorAll('.variable-row');
    var vars = [];
    
    rows.forEach(function(row) {
      var id = row.id.replace('var-row-', '');
      var name = document.getElementById('var-name-' + id);
      var type = document.getElementById('var-type-' + id);
      var value = document.getElementById('var-value-' + id);
      
      if (name && type && value && name.value) {
        vars.push({
          name: name.value,
          type: type.value,
          value: value.value
        });
      }
    });
    
    return vars;
  }
  
  function setVariables(vars) {
    var list = document.getElementById('variables-list');
    list.innerHTML = '';
    variableCounter = 0;
    
    vars.forEach(function(v) {
      addVariable();
      var id = variableCounter - 1;
      document.getElementById('var-name-' + id).value = v.name;
      document.getElementById('var-type-' + id).value = v.type;
      updateVarValue(id);
      document.getElementById('var-value-' + id).value = v.value;
    });
    
    updateVarButtons();
  }
  
  // ========== Expression ==========
  function insertExpr(text) {
    var input = document.getElementById('eq-expression');
    var start = input.selectionStart;
    var end = input.selectionEnd;
    var value = input.value;
    input.value = value.substring(0, start) + text + value.substring(end);
    input.selectionStart = input.selectionEnd = start + text.length;
    input.focus();
  }
  
  function evaluateExpression() {
    var expr = document.getElementById('eq-expression').value;
    var resultEl = document.getElementById('expr-result');
    
    if (!expr) {
      resultEl.textContent = '--';
      resultEl.classList.remove('expr-error');
      return;
    }
    
    try {
      var vars = {};
      getVariables().forEach(function(v) {
        if (v.type === 'static') {
          vars[v.name] = parseFloat(v.value) || 0;
        } else if (v.type === 'sensor') {
          vars[v.name] = sensorValues[v.value] || 0;
        } else if (v.type === 'equation') {
          var refEq = equations.find(function(e) { return e.id === parseInt(v.value); });
          if (refEq) {
            vars[v.name] = evaluateEquation(refEq);
          } else {
            vars[v.name] = 0;
          }
        }
      });
      
      var result = safeEval(expr, vars);
      
      if (isNaN(result) || !isFinite(result)) {
        resultEl.textContent = 'Invalid';
        resultEl.classList.add('expr-error');
      } else {
        resultEl.textContent = result.toFixed(4);
        resultEl.classList.remove('expr-error');
      }
    } catch (e) {
      resultEl.textContent = 'Error: ' + e.message;
      resultEl.classList.add('expr-error');
    }
  }
  
  // ========== Form ==========
  function resetForm() {
    editingId = null;
    document.getElementById('form-title').textContent = 'Create Equation';
    document.getElementById('eq-name').value = '';
    document.getElementById('eq-expression').value = '';
    document.getElementById('variables-list').innerHTML = '';
    document.getElementById('deps-warning').style.display = 'none';
    document.getElementById('save-btn').textContent = 'Save Equation';
    variableCounter = 0;
    updateVarButtons();
  }
  
  function editEquation(id) {
    var eq = equations.find(function(e) { return e.id === id; });
    if (!eq) return;
    
    editingId = id;
    document.getElementById('form-title').textContent = 'Edit Equation';
    document.getElementById('eq-name').value = eq.name;
    document.getElementById('eq-expression').value = eq.expression;
    setVariables(eq.variables);
    document.getElementById('save-btn').textContent = 'Update Equation';
    
    // Check dependencies
    var deps = getEquationDependents(id);
    if (deps.length > 0) {
      document.getElementById('deps-warning').style.display = 'block';
      document.getElementById('deps-list').innerHTML = 
        'Changing this equation will affect: <span>' + deps.map(function(d) { return d.name; }).join('</span>, <span>') + '</span>';
    } else {
      document.getElementById('deps-warning').style.display = 'none';
    }
    
    showView('create');
  }
  
  function cancelEdit() {
    resetForm();
    showView('list');
  }
  
  function saveEquation() {
    var name = document.getElementById('eq-name').value.trim();
    var expression = document.getElementById('eq-expression').value.trim();
    var variables = getVariables();
    
    // Validation
    if (!name) {
      showToast('Please enter an equation name', 'warning');
      document.getElementById('eq-name').focus();
      return;
    }
    
    if (!/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(name)) {
      showToast('Name must start with letter/underscore, contain only alphanumeric and underscore', 'warning');
      document.getElementById('eq-name').focus();
      return;
    }
    
    if (!expression) {
      showToast('Please enter an expression', 'warning');
      document.getElementById('eq-expression').focus();
      return;
    }
    
    // Check for duplicate names
    var duplicate = equations.find(function(e) { 
      return e.name === name && e.id !== editingId; 
    });
    if (duplicate) {
      showToast('An equation with this name already exists', 'warning');
      return;
    }
    
    var payload = {
      id: editingId,
      name: name,
      expression: expression,
      variables: variables
    };
    
    fetch('/api/equation/save', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(payload)
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        showToast(editingId ? 'Equation updated' : 'Equation created', 'success');
        loadEquations();
        resetForm();
        showView('list');
      } else {
        showToast('Failed: ' + (data.error || 'Unknown error'), 'error');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }
  
  // ========== Delete ==========
  function getEquationDependents(id) {
    return equations.filter(function(eq) {
      return eq.variables.some(function(v) {
        return v.type === 'equation' && parseInt(v.value) === id;
      });
    });
  }
  
  function openDeleteModal(id) {
    deletingId = id;
    var eq = equations.find(function(e) { return e.id === id; });
    
    var deps = getEquationDependents(id);
    if (deps.length > 0) {
      document.getElementById('delete-deps-warning').style.display = 'block';
      document.getElementById('delete-deps-list').innerHTML = 
        'Used by: <span>' + deps.map(function(d) { return d.name; }).join('</span>, <span>') + '</span>';
    } else {
      document.getElementById('delete-deps-warning').style.display = 'none';
    }
    
    document.getElementById('delete-msg').textContent = 
      'Are you sure you want to delete "' + eq.name + '"?';
    document.getElementById('delete-modal').classList.add('show');
  }
  
  function closeDeleteModal() {
    deletingId = null;
    document.getElementById('delete-modal').classList.remove('show');
  }
  
  function confirmDelete() {
    if (deletingId === null) return;
    
    fetch('/api/equation/delete', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ id: deletingId })
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      closeDeleteModal();
      if (data.success) {
        showToast('Equation deleted', 'success');
        loadEquations();
      } else {
        showToast('Failed: ' + (data.error || 'Unknown error'), 'error');
      }
    })
    .catch(function(err) {
      closeDeleteModal();
      showToast('Error: ' + err, 'error');
    });
  }
  
  // ========== Utilities ==========
  function escapeHtml(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }
  
  function showToast(message, type) {
    var toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className = 'toast ' + (type || 'info') + ' show';
    setTimeout(function() {
      toast.className = 'toast';
    }, 3000);
  }
  </script>
</body>
</html>
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
