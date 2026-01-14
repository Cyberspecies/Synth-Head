/*****************************************************************
 * @file PageAdvanced.hpp
 * @brief Advanced tab page content - Configuration Editor
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_ADVANCED[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Advanced</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    .config-actions-bar { display: flex; gap: 10px; margin-bottom: 16px; padding: 12px; background: var(--bg-tertiary); border-radius: 10px; }
    .action-btn { display: flex; align-items: center; gap: 6px; padding: 10px 16px; background: var(--bg-secondary); border: 1px solid var(--border); border-radius: 8px; color: var(--text-primary); cursor: pointer; font-size: 0.85rem; font-weight: 500; transition: all 0.2s; }
    .action-btn:hover { border-color: var(--accent); background: var(--accent-subtle); }
    .action-btn.primary { background: var(--accent); color: var(--bg-primary); border-color: var(--accent); }
    .action-btn.primary:hover { background: var(--accent-hover); }
    .action-btn.danger { color: var(--text-secondary); }
    .action-btn.danger:hover { border-color: var(--danger); color: var(--danger); background: rgba(255,59,48,0.1); }
    .action-btn .btn-icon { font-size: 1rem; line-height: 1; }
    
    .editor-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; }
    .config-selector { display: flex; align-items: center; gap: 12px; flex: 1; }
    .config-dropdown { flex: 1; padding: 12px 16px; background: var(--bg-tertiary); border: 1px solid var(--border); border-radius: 10px; color: var(--text-primary); font-size: 0.95rem; cursor: pointer; }
    .config-dropdown:focus { outline: none; border-color: var(--accent); }
    .header-actions { display: flex; gap: 8px; }
    .icon-btn { width: 40px; height: 40px; background: var(--bg-tertiary); border: 1px solid var(--border); border-radius: 8px; color: var(--text-secondary); cursor: pointer; font-size: 1.1rem; display: flex; align-items: center; justify-content: center; transition: all 0.2s; }
    .icon-btn:hover { border-color: var(--accent); color: var(--accent); }
    .icon-btn.danger:hover { border-color: var(--danger); color: var(--danger); }
    
    .section-tabs { display: flex; gap: 8px; margin-bottom: 16px; }
    .section-tab { flex: 1; padding: 12px 16px; background: var(--bg-tertiary); border: 2px solid var(--border); border-radius: 10px; cursor: pointer; text-align: center; transition: all 0.2s; }
    .section-tab:hover { border-color: var(--accent); }
    .section-tab.active { background: var(--accent-subtle); border-color: var(--accent); }
    .section-tab.disabled { opacity: 0.5; cursor: not-allowed; }
    .section-tab .tab-icon { font-size: 1.5rem; display: block; margin-bottom: 4px; }
    .section-tab .tab-label { font-size: 0.85rem; font-weight: 600; }
    .section-tab .tab-status { font-size: 0.7rem; color: var(--text-muted); margin-top: 2px; }
    .section-tab.enabled .tab-status { color: var(--success); }
    
    .editor-section { background: var(--bg-tertiary); border-radius: 12px; padding: 20px; margin-bottom: 16px; display: none; }
    .editor-section.active { display: block; animation: fadeIn 0.3s ease; }
    .section-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; padding-bottom: 12px; border-bottom: 1px solid var(--border); }
    .section-title { font-size: 1rem; font-weight: 600; }
    
    .form-row { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 16px; }
    .form-row.single { grid-template-columns: 1fr; }
    .form-field { display: flex; flex-direction: column; gap: 6px; }
    .form-field label { font-size: 0.8rem; color: var(--text-secondary); font-weight: 500; }
    .form-field select, .form-field input { padding: 10px 12px; background: var(--bg-secondary); border: 1px solid var(--border); border-radius: 8px; color: var(--text-primary); font-size: 0.9rem; }
    .form-field select:focus, .form-field input:focus { outline: none; border-color: var(--accent); }
    
    .color-picker-row { display: flex; gap: 12px; align-items: center; }
    .color-swatch { width: 40px; height: 40px; border-radius: 8px; border: 2px solid var(--border); cursor: pointer; }
    .color-inputs { display: flex; gap: 8px; flex: 1; }
    .color-inputs input { width: 60px; text-align: center; }
    
    .slider-field { display: flex; flex-direction: column; gap: 8px; }
    .slider-row { display: flex; align-items: center; gap: 12px; }
    .slider-row input[type="range"] { flex: 1; -webkit-appearance: none; height: 6px; background: var(--bg-secondary); border-radius: 3px; }
    .slider-row input[type="range"]::-webkit-slider-thumb { -webkit-appearance: none; width: 18px; height: 18px; background: var(--accent); border-radius: 50%; cursor: pointer; }
    .slider-value { min-width: 40px; text-align: right; font-family: 'SF Mono', Monaco, monospace; font-size: 0.85rem; }
    
    .config-name-edit { display: flex; gap: 8px; align-items: center; }
    .config-name-edit input { flex: 1; }
    .config-name-edit .icon-btn { flex-shrink: 0; }
    
    .target-selector { display: flex; gap: 8px; margin-bottom: 16px; }
    .target-option { flex: 1; padding: 12px; background: var(--bg-secondary); border: 2px solid var(--border); border-radius: 8px; cursor: pointer; text-align: center; transition: all 0.2s; }
    .target-option:hover { border-color: var(--accent); }
    .target-option.selected { border-color: var(--accent); background: var(--accent-subtle); }
    .target-option .target-icon { font-size: 1.2rem; display: block; margin-bottom: 4px; }
    .target-option .target-label { font-size: 0.8rem; font-weight: 600; }
    
    .action-bar { display: flex; gap: 12px; padding-top: 16px; border-top: 1px solid var(--border); }
    .action-bar .btn { flex: 1; }
    
    .modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.7); display: none; align-items: center; justify-content: center; z-index: 1000; }
    .modal-overlay.show { display: flex; }
    .modal { background: var(--bg-card); border-radius: 16px; padding: 24px; max-width: 400px; width: 90%; }
    .modal-header { margin-bottom: 16px; }
    .modal-header h3 { font-size: 1.1rem; margin: 0; }
    .modal-body { margin-bottom: 20px; }
    .modal-actions { display: flex; gap: 12px; }
    .modal-actions .btn { flex: 1; }
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
      <a href="/advanced" style="display: inline-flex; align-items: center; gap: 6px; color: var(--text-muted); text-decoration: none; font-size: 0.85rem; margin-bottom: 12px;">&#x2190; Back to Advanced</a>
      <div class="card">
        <div class="card-header">
          <h2>Configuration Editor</h2>
        </div>
        <div class="card-body">
          <!-- Quick Actions Bar -->
          <div class="config-actions-bar">
            <button class="action-btn primary" onclick="createNewConfig()">
              <span class="btn-icon">+</span>
              <span>New Config</span>
            </button>
            <button class="action-btn" onclick="duplicateConfig()">
              <span class="btn-icon">&#x29C9;</span>
              <span>Duplicate</span>
            </button>
            <button class="action-btn danger" onclick="confirmDelete()">
              <span class="btn-icon">&times;</span>
              <span>Delete</span>
            </button>
          </div>
          
          <!-- Configuration Selector -->
          <div class="editor-header">
            <div class="config-selector">
              <select class="config-dropdown" id="config-select" onchange="loadConfig()">
                <option value="-1">Select a configuration...</option>
              </select>
            </div>
          </div>
          
          <!-- Configuration Name & Target -->
          <div id="config-editor" style="display: none;">
            <div class="form-row single">
              <div class="form-field">
                <label>Configuration Name</label>
                <div class="config-name-edit">
                  <input type="text" id="config-name" placeholder="Configuration name">
                  <button class="icon-btn" onclick="renameConfig()" title="Save Name">&#x2713;</button>
                </div>
              </div>
            </div>
            
            <div class="form-field" style="margin-bottom: 16px;">
              <label>Configuration Target</label>
              <div class="target-selector">
                <div class="target-option" data-target="1" onclick="setTarget(1)">
                  <span class="target-icon">&#x25A6;</span>
                  <span class="target-label">Display Only</span>
                </div>
                <div class="target-option" data-target="2" onclick="setTarget(2)">
                  <span class="target-icon">&#x2606;</span>
                  <span class="target-label">LEDs Only</span>
                </div>
                <div class="target-option" data-target="3" onclick="setTarget(3)">
                  <span class="target-icon">&#x29C9;</span>
                  <span class="target-label">Both</span>
                </div>
              </div>
            </div>
            
            <!-- Section Tabs -->
            <div class="section-tabs">
              <div class="section-tab" id="tab-display" onclick="showSection('display')">
                <span class="tab-icon">&#x25A6;</span>
                <span class="tab-label">HUB75 Display</span>
                <span class="tab-status" id="status-display">Disabled</span>
              </div>
              <div class="section-tab" id="tab-leds" onclick="showSection('leds')">
                <span class="tab-icon">&#x2606;</span>
                <span class="tab-label">LED Strip</span>
                <span class="tab-status" id="status-leds">Disabled</span>
              </div>
            </div>
            
            <!-- Display Editor Section -->
            <div class="editor-section" id="section-display">
              <div class="section-header">
                <span class="section-title">HUB75 Display Settings</span>
              </div>
              
              <div class="form-row">
                <div class="form-field">
                  <label>Animation</label>
                  <select id="display-animation">
                    <option value="0">None</option>
                    <option value="1">Solid Color</option>
                    <option value="2">Rainbow (Horizontal)</option>
                    <option value="3">Rainbow (Vertical)</option>
                    <option value="4">Gradient</option>
                    <option value="5">Pulse</option>
                    <option value="6">Sparkle</option>
                    <option value="7">Wave</option>
                    <option value="8">Fire</option>
                    <option value="9">Matrix</option>
                  </select>
                </div>
                <div class="form-field slider-field">
                  <label>Speed</label>
                  <div class="slider-row">
                    <input type="range" id="display-speed" min="0" max="255" value="128" oninput="updateSliderValue('display-speed')">
                    <span class="slider-value" id="display-speed-val">128</span>
                  </div>
                </div>
              </div>
              
              <div class="form-row">
                <div class="form-field slider-field">
                  <label>Brightness</label>
                  <div class="slider-row">
                    <input type="range" id="display-brightness" min="0" max="255" value="255" oninput="updateSliderValue('display-brightness')">
                    <span class="slider-value" id="display-brightness-val">255</span>
                  </div>
                </div>
              </div>
              
              <div class="form-row">
                <div class="form-field">
                  <label>Primary Color</label>
                  <div class="color-picker-row">
                    <input type="color" id="display-color1" class="color-swatch" value="#ff0000" onchange="syncColorInputs('display-color1')">
                    <div class="color-inputs">
                      <input type="number" id="display-color1-r" min="0" max="255" value="255" placeholder="R" onchange="syncColorPicker('display-color1')">
                      <input type="number" id="display-color1-g" min="0" max="255" value="0" placeholder="G" onchange="syncColorPicker('display-color1')">
                      <input type="number" id="display-color1-b" min="0" max="255" value="0" placeholder="B" onchange="syncColorPicker('display-color1')">
                    </div>
                  </div>
                </div>
                <div class="form-field">
                  <label>Secondary Color</label>
                  <div class="color-picker-row">
                    <input type="color" id="display-color2" class="color-swatch" value="#0000ff" onchange="syncColorInputs('display-color2')">
                    <div class="color-inputs">
                      <input type="number" id="display-color2-r" min="0" max="255" value="0" placeholder="R" onchange="syncColorPicker('display-color2')">
                      <input type="number" id="display-color2-g" min="0" max="255" value="0" placeholder="G" onchange="syncColorPicker('display-color2')">
                      <input type="number" id="display-color2-b" min="0" max="255" value="255" placeholder="B" onchange="syncColorPicker('display-color2')">
                    </div>
                  </div>
                </div>
              </div>
            </div>
            
            <!-- LED Editor Section -->
            <div class="editor-section" id="section-leds">
              <div class="section-header">
                <span class="section-title">LED Strip Settings</span>
              </div>
              
              <div class="form-row">
                <div class="form-field">
                  <label>Animation</label>
                  <select id="leds-animation">
                    <option value="0">None</option>
                    <option value="1">Solid Color</option>
                    <option value="2">Rainbow</option>
                    <option value="3">Breathing</option>
                    <option value="4">Wave</option>
                    <option value="5">Fire</option>
                    <option value="6">Theater Chase</option>
                    <option value="7">Sparkle</option>
                  </select>
                </div>
                <div class="form-field slider-field">
                  <label>Speed</label>
                  <div class="slider-row">
                    <input type="range" id="leds-speed" min="0" max="255" value="128" oninput="updateSliderValue('leds-speed')">
                    <span class="slider-value" id="leds-speed-val">128</span>
                  </div>
                </div>
              </div>
              
              <div class="form-row">
                <div class="form-field slider-field">
                  <label>Brightness</label>
                  <div class="slider-row">
                    <input type="range" id="leds-brightness" min="0" max="255" value="255" oninput="updateSliderValue('leds-brightness')">
                    <span class="slider-value" id="leds-brightness-val">255</span>
                  </div>
                </div>
              </div>
              
              <div class="form-row">
                <div class="form-field">
                  <label>Primary Color</label>
                  <div class="color-picker-row">
                    <input type="color" id="leds-color1" class="color-swatch" value="#ff0000" onchange="syncColorInputs('leds-color1')">
                    <div class="color-inputs">
                      <input type="number" id="leds-color1-r" min="0" max="255" value="255" placeholder="R" onchange="syncColorPicker('leds-color1')">
                      <input type="number" id="leds-color1-g" min="0" max="255" value="0" placeholder="G" onchange="syncColorPicker('leds-color1')">
                      <input type="number" id="leds-color1-b" min="0" max="255" value="0" placeholder="B" onchange="syncColorPicker('leds-color1')">
                    </div>
                  </div>
                </div>
                <div class="form-field">
                  <label>Secondary Color</label>
                  <div class="color-picker-row">
                    <input type="color" id="leds-color2" class="color-swatch" value="#0000ff" onchange="syncColorInputs('leds-color2')">
                    <div class="color-inputs">
                      <input type="number" id="leds-color2-r" min="0" max="255" value="0" placeholder="R" onchange="syncColorPicker('leds-color2')">
                      <input type="number" id="leds-color2-g" min="0" max="255" value="0" placeholder="G" onchange="syncColorPicker('leds-color2')">
                      <input type="number" id="leds-color2-b" min="0" max="255" value="255" placeholder="B" onchange="syncColorPicker('leds-color2')">
                    </div>
                  </div>
                </div>
              </div>
            </div>
            
            <!-- Action Bar -->
            <div class="action-bar">
              <button class="btn btn-secondary" onclick="resetConfig()">Reset</button>
              <button class="btn btn-primary" onclick="saveConfig()">Save Changes</button>
              <button class="btn btn-primary" onclick="saveAndApply()">Save &amp; Apply</button>
            </div>
          </div>
          
          <!-- Empty State -->
          <div id="empty-state" class="placeholder-card" style="margin-top: 20px;">
            <div class="card-body center">
              <div class="placeholder-icon">&#x25A1;</div>
              <p class="placeholder-text">Select a configuration to edit, or create a new one</p>
            </div>
          </div>
        </div>
      </div>
    </section>
    
    <footer>
      <p>Lucidius - ARCOS Framework</p>
    </footer>
  </div>
  
  <!-- Delete Confirmation Modal -->
  <div class="modal-overlay" id="delete-modal">
    <div class="modal">
      <div class="modal-header">
        <h3>Delete Configuration?</h3>
      </div>
      <div class="modal-body">
        <p>Are you sure you want to delete "<span id="delete-name"></span>"? This action cannot be undone.</p>
      </div>
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="closeModal('delete-modal')">Cancel</button>
        <button class="btn btn-danger" onclick="deleteConfig()">Delete</button>
      </div>
    </div>
  </div>
  
  <!-- New Config Modal -->
  <div class="modal-overlay" id="new-modal">
    <div class="modal">
      <div class="modal-header">
        <h3>Create New Configuration</h3>
      </div>
      <div class="modal-body">
        <div class="form-field">
          <label>Configuration Name</label>
          <input type="text" id="new-config-name" placeholder="My Configuration">
        </div>
      </div>
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="closeModal('new-modal')">Cancel</button>
        <button class="btn btn-primary" onclick="doCreateConfig()">Create</button>
      </div>
    </div>
  </div>
  
  <div id="toast" class="toast"></div>
  
  <script>
  var configs = [];
  var currentConfig = null;
  var currentIndex = -1;
  var activeSection = 'display';
  
  // Initialize
  document.addEventListener('DOMContentLoaded', function() {
    fetchConfigs();
  });
  
  function fetchConfigs() {
    fetch('/api/configs')
      .then(r => r.json())
      .then(data => {
        configs = data.configs || [];
        populateDropdown();
      })
      .catch(err => {
        console.error('Config fetch error:', err);
        // Fallback demo data
        configs = [
          {name: 'Rainbow', target: 1, index: 0, display: {animation: 2, speed: 128, brightness: 200, color1: {r:255,g:0,b:0}, color2: {r:0,g:0,b:255}}, leds: {animation: 0, speed: 128, brightness: 255, color1: {r:255,g:0,b:0}, color2: {r:0,g:0,b:255}}},
          {name: 'Solid Red', target: 1, index: 1, display: {animation: 1, speed: 128, brightness: 255, color1: {r:255,g:0,b:0}, color2: {r:0,g:0,b:0}}, leds: {animation: 0, speed: 128, brightness: 255, color1: {r:255,g:0,b:0}, color2: {r:0,g:0,b:0}}},
          {name: 'LED Rainbow', target: 2, index: 2, display: {animation: 0, speed: 128, brightness: 255, color1: {r:255,g:0,b:0}, color2: {r:0,g:0,b:255}}, leds: {animation: 2, speed: 128, brightness: 200, color1: {r:255,g:0,b:0}, color2: {r:0,g:0,b:255}}},
          {name: 'Fire Effect', target: 3, index: 4, display: {animation: 8, speed: 180, brightness: 220, color1: {r:255,g:100,b:0}, color2: {r:255,g:0,b:0}}, leds: {animation: 5, speed: 180, brightness: 220, color1: {r:255,g:100,b:0}, color2: {r:255,g:0,b:0}}}
        ];
        populateDropdown();
      });
  }
  
  function populateDropdown() {
    var select = document.getElementById('config-select');
    select.innerHTML = '<option value="-1">Select a configuration...</option>';
    configs.forEach(function(cfg, idx) {
      var opt = document.createElement('option');
      opt.value = cfg.index !== undefined ? cfg.index : idx;
      opt.textContent = cfg.name;
      select.appendChild(opt);
    });
  }
  
  function loadConfig() {
    var select = document.getElementById('config-select');
    var idx = parseInt(select.value);
    
    if (idx < 0) {
      document.getElementById('config-editor').style.display = 'none';
      document.getElementById('empty-state').style.display = 'block';
      currentConfig = null;
      currentIndex = -1;
      return;
    }
    
    // Find config by index
    currentConfig = configs.find(c => (c.index !== undefined ? c.index : configs.indexOf(c)) === idx);
    currentIndex = idx;
    
    if (!currentConfig) {
      showToast('Configuration not found', 'error');
      return;
    }
    
    document.getElementById('config-editor').style.display = 'block';
    document.getElementById('empty-state').style.display = 'none';
    
    // Populate form
    document.getElementById('config-name').value = currentConfig.name;
    
    // Set target
    setTarget(currentConfig.target, false);
    
    // Display settings
    if (currentConfig.display) {
      document.getElementById('display-animation').value = currentConfig.display.animation || 0;
      document.getElementById('display-speed').value = currentConfig.display.speed || 128;
      document.getElementById('display-brightness').value = currentConfig.display.brightness || 255;
      updateSliderValue('display-speed');
      updateSliderValue('display-brightness');
      
      if (currentConfig.display.color1) {
        setColorInputs('display-color1', currentConfig.display.color1.r, currentConfig.display.color1.g, currentConfig.display.color1.b);
      }
      if (currentConfig.display.color2) {
        setColorInputs('display-color2', currentConfig.display.color2.r, currentConfig.display.color2.g, currentConfig.display.color2.b);
      }
    }
    
    // LED settings
    if (currentConfig.leds) {
      document.getElementById('leds-animation').value = currentConfig.leds.animation || 0;
      document.getElementById('leds-speed').value = currentConfig.leds.speed || 128;
      document.getElementById('leds-brightness').value = currentConfig.leds.brightness || 255;
      updateSliderValue('leds-speed');
      updateSliderValue('leds-brightness');
      
      if (currentConfig.leds.color1) {
        setColorInputs('leds-color1', currentConfig.leds.color1.r, currentConfig.leds.color1.g, currentConfig.leds.color1.b);
      }
      if (currentConfig.leds.color2) {
        setColorInputs('leds-color2', currentConfig.leds.color2.r, currentConfig.leds.color2.g, currentConfig.leds.color2.b);
      }
    }
    
    // Show appropriate section
    if (currentConfig.target === 2) {
      showSection('leds');
    } else {
      showSection('display');
    }
  }
  
  function setTarget(target, updateConfig) {
    if (updateConfig === undefined) updateConfig = true;
    
    document.querySelectorAll('.target-option').forEach(function(el) {
      el.classList.remove('selected');
      if (parseInt(el.dataset.target) === target) {
        el.classList.add('selected');
      }
    });
    
    // Update section tabs
    var displayTab = document.getElementById('tab-display');
    var ledsTab = document.getElementById('tab-leds');
    var statusDisplay = document.getElementById('status-display');
    var statusLeds = document.getElementById('status-leds');
    
    displayTab.classList.remove('disabled', 'enabled');
    ledsTab.classList.remove('disabled', 'enabled');
    
    if (target === 1) {
      displayTab.classList.add('enabled');
      ledsTab.classList.add('disabled');
      statusDisplay.textContent = 'Enabled';
      statusLeds.textContent = 'Disabled';
    } else if (target === 2) {
      displayTab.classList.add('disabled');
      ledsTab.classList.add('enabled');
      statusDisplay.textContent = 'Disabled';
      statusLeds.textContent = 'Enabled';
    } else if (target === 3) {
      displayTab.classList.add('enabled');
      ledsTab.classList.add('enabled');
      statusDisplay.textContent = 'Enabled';
      statusLeds.textContent = 'Enabled';
    }
    
    if (updateConfig && currentConfig) {
      currentConfig.target = target;
    }
  }
  
  function showSection(section) {
    activeSection = section;
    
    document.querySelectorAll('.section-tab').forEach(function(el) {
      el.classList.remove('active');
    });
    document.querySelectorAll('.editor-section').forEach(function(el) {
      el.classList.remove('active');
    });
    
    document.getElementById('tab-' + section).classList.add('active');
    document.getElementById('section-' + section).classList.add('active');
  }
  
  function updateSliderValue(id) {
    var slider = document.getElementById(id);
    var valueSpan = document.getElementById(id + '-val');
    if (slider && valueSpan) {
      valueSpan.textContent = slider.value;
    }
  }
  
  function setColorInputs(prefix, r, g, b) {
    document.getElementById(prefix + '-r').value = r;
    document.getElementById(prefix + '-g').value = g;
    document.getElementById(prefix + '-b').value = b;
    document.getElementById(prefix).value = rgbToHex(r, g, b);
  }
  
  function syncColorInputs(prefix) {
    var hex = document.getElementById(prefix).value;
    var rgb = hexToRgb(hex);
    document.getElementById(prefix + '-r').value = rgb.r;
    document.getElementById(prefix + '-g').value = rgb.g;
    document.getElementById(prefix + '-b').value = rgb.b;
  }
  
  function syncColorPicker(prefix) {
    var r = parseInt(document.getElementById(prefix + '-r').value) || 0;
    var g = parseInt(document.getElementById(prefix + '-g').value) || 0;
    var b = parseInt(document.getElementById(prefix + '-b').value) || 0;
    document.getElementById(prefix).value = rgbToHex(r, g, b);
  }
  
  function rgbToHex(r, g, b) {
    return '#' + [r, g, b].map(function(x) {
      var hex = x.toString(16);
      return hex.length === 1 ? '0' + hex : hex;
    }).join('');
  }
  
  function hexToRgb(hex) {
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : {r: 0, g: 0, b: 0};
  }
  
  function gatherConfigData() {
    return {
      index: currentIndex,
      name: document.getElementById('config-name').value,
      target: currentConfig ? currentConfig.target : 3,
      display: {
        animation: parseInt(document.getElementById('display-animation').value),
        speed: parseInt(document.getElementById('display-speed').value),
        brightness: parseInt(document.getElementById('display-brightness').value),
        color1: {
          r: parseInt(document.getElementById('display-color1-r').value),
          g: parseInt(document.getElementById('display-color1-g').value),
          b: parseInt(document.getElementById('display-color1-b').value)
        },
        color2: {
          r: parseInt(document.getElementById('display-color2-r').value),
          g: parseInt(document.getElementById('display-color2-g').value),
          b: parseInt(document.getElementById('display-color2-b').value)
        }
      },
      leds: {
        animation: parseInt(document.getElementById('leds-animation').value),
        speed: parseInt(document.getElementById('leds-speed').value),
        brightness: parseInt(document.getElementById('leds-brightness').value),
        color1: {
          r: parseInt(document.getElementById('leds-color1-r').value),
          g: parseInt(document.getElementById('leds-color1-g').value),
          b: parseInt(document.getElementById('leds-color1-b').value)
        },
        color2: {
          r: parseInt(document.getElementById('leds-color2-r').value),
          g: parseInt(document.getElementById('leds-color2-g').value),
          b: parseInt(document.getElementById('leds-color2-b').value)
        }
      }
    };
  }
  
  function saveConfig() {
    var data = gatherConfigData();
    
    fetch('/api/config/save', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(data)
    })
    .then(r => r.json())
    .then(res => {
      if (res.success) {
        showToast('Configuration saved', 'success');
        fetchConfigs();
      } else {
        showToast('Failed to save: ' + (res.error || 'Unknown error'), 'error');
      }
    })
    .catch(err => {
      showToast('Error: ' + err, 'error');
    });
  }
  
  function saveAndApply() {
    var data = gatherConfigData();
    data.apply = true;
    
    fetch('/api/config/save', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(data)
    })
    .then(r => r.json())
    .then(res => {
      if (res.success) {
        showToast('Configuration saved and applied', 'success');
        fetchConfigs();
      } else {
        showToast('Failed: ' + (res.error || 'Unknown error'), 'error');
      }
    })
    .catch(err => {
      showToast('Error: ' + err, 'error');
    });
  }
  
  function resetConfig() {
    loadConfig();
    showToast('Changes reset', 'info');
  }
  
  function renameConfig() {
    var newName = document.getElementById('config-name').value.trim();
    if (!newName) {
      showToast('Name cannot be empty', 'error');
      return;
    }
    
    fetch('/api/config/rename', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({index: currentIndex, name: newName})
    })
    .then(r => r.json())
    .then(res => {
      if (res.success) {
        showToast('Renamed to "' + newName + '"', 'success');
        fetchConfigs();
      } else {
        showToast('Failed to rename', 'error');
      }
    })
    .catch(err => {
      showToast('Error: ' + err, 'error');
    });
  }
  
  function createNewConfig() {
    document.getElementById('new-config-name').value = '';
    document.getElementById('new-modal').classList.add('show');
  }
  
  function doCreateConfig() {
    var name = document.getElementById('new-config-name').value.trim();
    if (!name) name = 'New Configuration';
    
    fetch('/api/config/create', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({name: name})
    })
    .then(r => r.json())
    .then(res => {
      closeModal('new-modal');
      if (res.success) {
        showToast('Configuration created', 'success');
        fetchConfigs();
        // Select the new config
        setTimeout(function() {
          document.getElementById('config-select').value = res.index;
          loadConfig();
        }, 300);
      } else {
        showToast('Failed to create', 'error');
      }
    })
    .catch(err => {
      closeModal('new-modal');
      showToast('Error: ' + err, 'error');
    });
  }
  
  function duplicateConfig() {
    if (currentIndex < 0) {
      showToast('Select a configuration first', 'warning');
      return;
    }
    
    fetch('/api/config/duplicate', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({index: currentIndex})
    })
    .then(r => r.json())
    .then(res => {
      if (res.success) {
        showToast('Configuration duplicated', 'success');
        fetchConfigs();
      } else {
        showToast('Failed to duplicate', 'error');
      }
    })
    .catch(err => {
      showToast('Error: ' + err, 'error');
    });
  }
  
  function confirmDelete() {
    if (currentIndex < 0 || !currentConfig) {
      showToast('Select a configuration first', 'warning');
      return;
    }
    
    document.getElementById('delete-name').textContent = currentConfig.name;
    document.getElementById('delete-modal').classList.add('show');
  }
  
  function deleteConfig() {
    closeModal('delete-modal');
    
    fetch('/api/config/delete', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({index: currentIndex})
    })
    .then(r => r.json())
    .then(res => {
      if (res.success) {
        showToast('Configuration deleted', 'success');
        document.getElementById('config-select').value = -1;
        loadConfig();
        fetchConfigs();
      } else {
        showToast('Failed to delete', 'error');
      }
    })
    .catch(err => {
      showToast('Error: ' + err, 'error');
    });
  }
  
  function closeModal(id) {
    document.getElementById(id).classList.remove('show');
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
