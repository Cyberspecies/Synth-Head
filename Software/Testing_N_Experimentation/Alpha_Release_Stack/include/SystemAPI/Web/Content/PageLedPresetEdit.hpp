/*****************************************************************
 * @file PageLedPresetEdit.hpp
 * @brief LED Preset Editor with YAML Configuration
 * 
 * Features:
 * - YAML-based configuration editor
 * - Color picker with RGB sliders
 * - Animation type selection (Solid, Breathe, Rainbow, Pulse, etc.)
 * - Animation speed and brightness controls
 * - Live preview toggle
 * - Save and Apply buttons
 * 
 * YAML Format:
 *   name: "My Preset"
 *   animation: breathe
 *   color:
 *     r: 255
 *     g: 100
 *     b: 0
 *   brightness: 80
 *   speed: 50
 *   params:
 *     min_brightness: 20
 *     max_brightness: 100
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char* PAGE_LED_PRESET_EDIT = R"rawpage(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Edit LED Preset</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    .back-link {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      color: var(--text-muted);
      text-decoration: none;
      font-size: 0.85rem;
      margin-bottom: 12px;
    }
    .back-link:hover { color: var(--accent); }

    .preset-title {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 16px;
    }
    .preset-title input {
      flex: 1;
      padding: 10px 14px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-size: 1.1rem;
      font-weight: 600;
    }
    .preset-title input:focus { outline: none; border-color: var(--accent); }

    .btn {
      padding: 10px 18px;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      font-size: 0.85rem;
      font-weight: 600;
      transition: all 0.15s;
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    .btn:hover { filter: brightness(1.1); }
    .btn-primary { background: var(--accent); color: var(--bg-primary); }
    .btn-secondary { background: var(--bg-tertiary); color: var(--text-primary); border: 1px solid var(--border); }
    .btn-success { background: var(--success); color: #fff; }
    .btn-sm { padding: 7px 12px; font-size: 0.75rem; }

    .editor-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
    }
    @media (max-width: 768px) { .editor-grid { grid-template-columns: 1fr; } }

    .panel {
      background: var(--bg-tertiary);
      border-radius: 10px;
      overflow: hidden;
      margin-bottom: 16px;
    }
    .panel-header {
      padding: 14px 16px;
      border-bottom: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .panel-header h3 {
      font-size: 0.9rem;
      font-weight: 600;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .panel-header h3 .icon { color: var(--accent); }
    .panel-body { padding: 16px; }

    .subsection {
      border-bottom: 1px solid var(--border);
      padding-bottom: 16px;
      margin-bottom: 16px;
    }
    .subsection:last-child { border-bottom: none; margin-bottom: 0; padding-bottom: 0; }
    .subsection-title {
      font-size: 0.75rem;
      font-weight: 700;
      color: var(--accent);
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 12px;
    }

    /* Animation Type Grid */
    .anim-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
    }
    @media (max-width: 600px) { .anim-grid { grid-template-columns: repeat(2, 1fr); } }
    .anim-card {
      background: var(--bg-secondary);
      border: 2px solid transparent;
      border-radius: 8px;
      padding: 14px 10px;
      cursor: pointer;
      text-align: center;
      transition: all 0.2s;
    }
    .anim-card:hover { border-color: var(--border); }
    .anim-card.selected { border-color: var(--accent); background: rgba(255,107,0,0.1); }
    .anim-card .icon { font-size: 22px; margin-bottom: 6px; color: var(--accent); }
    .anim-card .name { font-size: 0.8rem; font-weight: 600; }

    /* Color Picker */
    .color-section {
      display: flex;
      gap: 16px;
      align-items: flex-start;
    }
    .color-preview-large {
      width: 80px;
      height: 80px;
      border-radius: 12px;
      border: 3px solid var(--border);
      flex-shrink: 0;
    }
    .color-sliders { flex: 1; }
    .color-slider-row {
      display: flex;
      align-items: center;
      gap: 10px;
      margin-bottom: 10px;
    }
    .color-slider-row:last-child { margin-bottom: 0; }
    .color-label {
      width: 20px;
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-secondary);
    }
    .color-label.r { color: #f87171; }
    .color-label.g { color: #4ade80; }
    .color-label.b { color: #60a5fa; }
    
    input[type="range"] {
      flex: 1;
      height: 8px;
      -webkit-appearance: none;
      background: var(--bg-secondary);
      border-radius: 4px;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 18px;
      height: 18px;
      background: var(--accent);
      border-radius: 50%;
      cursor: pointer;
    }
    .color-value {
      width: 45px;
      padding: 6px;
      text-align: center;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 4px;
      color: var(--text-primary);
      font-size: 0.8rem;
      font-family: monospace;
    }

    /* Param Row */
    .param-row {
      display: flex;
      align-items: center;
      padding: 10px 0;
      border-bottom: 1px solid var(--bg-secondary);
    }
    .param-row:last-child { border-bottom: none; }
    .param-label { flex: 0 0 40%; font-size: 0.85rem; }
    .param-control { flex: 1; display: flex; align-items: center; gap: 10px; }
    .param-value {
      min-width: 45px;
      text-align: right;
      font-size: 0.8rem;
      color: var(--text-secondary);
      font-family: monospace;
    }

    /* Toggle Switch */
    .toggle {
      position: relative;
      width: 44px;
      height: 24px;
    }
    .toggle input { display: none; }
    .toggle-slider {
      position: absolute;
      inset: 0;
      background: var(--bg-secondary);
      border-radius: 12px;
      cursor: pointer;
      transition: background 0.2s;
    }
    .toggle-slider:before {
      content: '';
      position: absolute;
      width: 18px;
      height: 18px;
      left: 3px;
      bottom: 3px;
      background: #fff;
      border-radius: 50%;
      transition: transform 0.2s;
    }
    .toggle input:checked + .toggle-slider { background: var(--accent); }
    .toggle input:checked + .toggle-slider:before { transform: translateX(20px); }

    /* YAML Editor */
    .yaml-editor {
      width: 100%;
      min-height: 300px;
      padding: 12px;
      background: #1a1a2e;
      border: 1px solid var(--border);
      border-radius: 8px;
      color: #e0e0e0;
      font-family: 'SF Mono', 'Consolas', monospace;
      font-size: 0.85rem;
      line-height: 1.5;
      resize: vertical;
    }
    .yaml-editor:focus { outline: none; border-color: var(--accent); }

    .yaml-status {
      display: flex;
      align-items: center;
      gap: 8px;
      margin-top: 8px;
      font-size: 0.75rem;
    }
    .yaml-status.valid { color: var(--success); }
    .yaml-status.invalid { color: var(--danger); }

    /* Actions Bar */
    .actions-bar {
      display: flex;
      gap: 12px;
      justify-content: flex-end;
      margin-top: 20px;
    }

    .toast {
      position: fixed;
      bottom: 20px;
      right: 20px;
      padding: 12px 20px;
      border-radius: 8px;
      color: #fff;
      font-size: 0.85rem;
      font-weight: 500;
      z-index: 2000;
      transform: translateY(100px);
      opacity: 0;
      transition: all 0.3s;
    }
    .toast.show { transform: translateY(0); opacity: 1; }
    .toast.success { background: var(--success); }
    .toast.error { background: var(--danger); }
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
            <span class="model-tag">DX.3</span>
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
      <a href="/advanced/ledpresets" class="back-link">&#x2190; Back to LED Presets</a>
      
      <div class="preset-title">
        <input type="text" id="presetName" placeholder="Preset Name">
        <button class="btn btn-success" onclick="saveAndApply()">&#x25B6; Save &amp; Apply</button>
      </div>
      
      <div class="editor-grid">
        <!-- Visual Editor Column -->
        <div>
          <div class="panel">
            <div class="panel-header">
              <h3><span class="icon">&#x1F3A8;</span> Animation Type</h3>
            </div>
            <div class="panel-body">
              <div class="anim-grid" id="animGrid">
                <div class="anim-card selected" data-anim="solid" onclick="selectAnim('solid')">
                  <div class="icon">&#x25A0;</div>
                  <div class="name">Solid</div>
                </div>
                <div class="anim-card" data-anim="breathe" onclick="selectAnim('breathe')">
                  <div class="icon">&#x1F4A8;</div>
                  <div class="name">Breathe</div>
                </div>
                <div class="anim-card" data-anim="rainbow" onclick="selectAnim('rainbow')">
                  <div class="icon">&#x1F308;</div>
                  <div class="name">Rainbow</div>
                </div>
                <div class="anim-card" data-anim="pulse" onclick="selectAnim('pulse')">
                  <div class="icon">&#x2764;</div>
                  <div class="name">Pulse</div>
                </div>
                <div class="anim-card" data-anim="chase" onclick="selectAnim('chase')">
                  <div class="icon">&#x27A1;</div>
                  <div class="name">Chase</div>
                </div>
                <div class="anim-card" data-anim="sparkle" onclick="selectAnim('sparkle')">
                  <div class="icon">&#x2728;</div>
                  <div class="name">Sparkle</div>
                </div>
                <div class="anim-card" data-anim="fire" onclick="selectAnim('fire')">
                  <div class="icon">&#x1F525;</div>
                  <div class="name">Fire</div>
                </div>
                <div class="anim-card" data-anim="wave" onclick="selectAnim('wave')">
                  <div class="icon">&#x1F30A;</div>
                  <div class="name">Wave</div>
                </div>
                <div class="anim-card" data-anim="gradient" onclick="selectAnim('gradient')">
                  <div class="icon">&#x1F3A8;</div>
                  <div class="name">Gradient</div>
                </div>
              </div>
            </div>
          </div>
          
          <div class="panel">
            <div class="panel-header">
              <h3><span class="icon">&#x1F308;</span> Colors</h3>
            </div>
            <div class="panel-body">
              <!-- Color Count (shown for multi-color animations) -->
              <div class="subsection" id="colorCountSection" style="display:none;">
                <div class="subsection-title">Number of Colors</div>
                <div class="param-row">
                  <span class="param-label">Color Count</span>
                  <div class="param-control">
                    <input type="number" id="colorCount" min="1" max="8" value="1" style="width:60px;padding:6px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:4px;color:var(--text-primary);" onchange="updateColorCount(parseInt(this.value))">
                  </div>
                </div>
              </div>
              
              <!-- Dynamic Color Pickers Container -->
              <div id="colorPickersContainer">
                <div class="color-section" id="colorPicker0">
                  <div class="color-preview-large" id="colorPreview0"></div>
                  <div class="color-sliders">
                    <div class="color-slider-row">
                      <span class="color-label r">R</span>
                      <input type="range" data-color-idx="0" data-channel="r" min="0" max="255" value="255" oninput="updateColorSlider(0)">
                      <input type="text" class="color-value" id="valueR0" value="255" onchange="updateFromColorInput(0,'r')">
                    </div>
                    <div class="color-slider-row">
                      <span class="color-label g">G</span>
                      <input type="range" data-color-idx="0" data-channel="g" min="0" max="255" value="100" oninput="updateColorSlider(0)">
                      <input type="text" class="color-value" id="valueG0" value="100" onchange="updateFromColorInput(0,'g')">
                    </div>
                    <div class="color-slider-row">
                      <span class="color-label b">B</span>
                      <input type="range" data-color-idx="0" data-channel="b" min="0" max="255" value="0" oninput="updateColorSlider(0)">
                      <input type="text" class="color-value" id="valueB0" value="0" onchange="updateFromColorInput(0,'b')">
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
          
          <div class="panel">
            <div class="panel-header">
              <h3><span class="icon">&#x2699;</span> Parameters</h3>
            </div>
            <div class="panel-body">
              <div class="param-row">
                <span class="param-label">Brightness</span>
                <div class="param-control">
                  <input type="range" id="brightness" min="0" max="100" value="80" oninput="updateParam('brightness')">
                  <span class="param-value" id="brightnessVal">80%</span>
                </div>
              </div>
              <div class="param-row">
                <span class="param-label">Speed</span>
                <div class="param-control">
                  <input type="range" id="speed" min="-100" max="100" value="50" oninput="updateParam('speed')">
                  <span class="param-value" id="speedVal">50</span>
                </div>
              </div>
              <div class="param-row" id="minBrightnessRow" style="display:none;">
                <span class="param-label">Min Brightness</span>
                <div class="param-control">
                  <input type="range" id="minBrightness" min="0" max="100" value="20" oninput="updateParam('minBrightness')">
                  <span class="param-value" id="minBrightnessVal">20%</span>
                </div>
              </div>
              <div class="param-row">
                <span class="param-label">Live Preview</span>
                <div class="param-control">
                  <label class="toggle">
                    <input type="checkbox" id="livePreview" checked onchange="togglePreview()">
                    <span class="toggle-slider"></span>
                  </label>
                </div>
              </div>
            </div>
          </div>
        </div>
        
        <!-- YAML Editor Column -->
        <div>
          <div class="panel">
            <div class="panel-header">
              <h3><span class="icon">&#x1F4DD;</span> YAML Configuration</h3>
              <button class="btn btn-sm btn-secondary" onclick="syncFromYaml()">Apply YAML</button>
            </div>
            <div class="panel-body">
              <textarea class="yaml-editor" id="yamlEditor" oninput="validateYaml()" spellcheck="false"></textarea>
              <div class="yaml-status" id="yamlStatus">
                <span>&#x2713;</span> Valid YAML
              </div>
            </div>
          </div>
        </div>
      </div>
      
      <div class="actions-bar">
        <button class="btn btn-secondary" onclick="window.location.href='/advanced/ledpresets'">Cancel</button>
        <button class="btn btn-primary" onclick="savePreset()">Save Preset</button>
      </div>
    </section>
    
    <footer><p>Lucidius - ARCOS Framework</p></footer>
  </div>
  
  <div class="toast" id="toast"></div>
  
  <script>
  var presetId = null;
  var currentPreset = {
    name: 'New Preset',
    animation: 'solid',
    r: 255,
    g: 100,
    b: 0,
    brightness: 80,
    speed: 50,
    colorCount: 1,
    colors: [{r: 255, g: 100, b: 0}],
    params: {}
  };
  var previewEnabled = true;
  var previewDebounce = null;

  // Parse URL params
  var urlParams = new URLSearchParams(window.location.search);
  presetId = urlParams.get('id');

  function init() {
    if (presetId) {
      loadPreset(presetId);
    } else {
      rebuildColorPickers();
      updateUI();
      syncToYaml();
    }
  }

  function loadPreset(id) {
    fetch('/api/ledpreset/get?id=' + id)
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.success && data.preset) {
          currentPreset = data.preset;
          updateUI();
          syncToYaml();
          updateColor();
        } else {
          showToast('Preset not found', 'error');
        }
      })
      .catch(function(err) {
        showToast('Load error: ' + err, 'error');
      });
  }

  function updateUI() {
    document.getElementById('presetName').value = currentPreset.name || '';
    document.getElementById('brightness').value = currentPreset.brightness || 80;
    document.getElementById('brightnessVal').textContent = (currentPreset.brightness || 80) + '%';
    document.getElementById('speed').value = currentPreset.speed || 50;
    document.getElementById('speedVal').textContent = currentPreset.speed || 50;
    
    // Initialize colors array if needed
    if (!currentPreset.colors || currentPreset.colors.length === 0) {
      currentPreset.colors = [{r: currentPreset.r || 255, g: currentPreset.g || 100, b: currentPreset.b || 0}];
      currentPreset.colorCount = 1;
    }
    currentPreset.colorCount = currentPreset.colors.length;
    document.getElementById('colorCount').value = currentPreset.colorCount;
    
    // Update animation selection
    document.querySelectorAll('.anim-card').forEach(function(c) {
      c.classList.remove('selected');
      if (c.dataset.anim === currentPreset.animation) {
        c.classList.add('selected');
      }
    });
    
    // Rebuild color pickers with loaded data
    rebuildColorPickers();
    updateAnimParams();
  }

  function selectAnim(anim) {
    currentPreset.animation = anim;
    document.querySelectorAll('.anim-card').forEach(function(c) {
      c.classList.toggle('selected', c.dataset.anim === anim);
    });
    updateAnimParams();
    syncToYaml();
    sendPreview();
  }

  function updateAnimParams() {
    var showMinBrightness = ['breathe', 'pulse'].includes(currentPreset.animation);
    document.getElementById('minBrightnessRow').style.display = showMinBrightness ? 'flex' : 'none';
    
    // Show color count for multi-color animations
    var multiColorAnims = ['rainbow', 'gradient', 'wave'];
    var showColorCount = multiColorAnims.includes(currentPreset.animation);
    document.getElementById('colorCountSection').style.display = showColorCount ? 'block' : 'none';
    
    // Reset to 1 color if switching to single-color animation
    if (!showColorCount && currentPreset.colorCount > 1) {
      updateColorCount(1);
    }
  }

  // Multi-color support
  function updateColorCount(count) {
    count = Math.max(1, Math.min(8, count));
    currentPreset.colorCount = count;
    document.getElementById('colorCount').value = count;
    
    // Ensure colors array has the right size
    while (currentPreset.colors.length < count) {
      // Add new colors with different hues
      var hueShift = currentPreset.colors.length * 45;
      currentPreset.colors.push({r: 255, g: Math.floor(hueShift % 256), b: Math.floor((hueShift * 2) % 256)});
    }
    currentPreset.colors = currentPreset.colors.slice(0, count);
    
    // Rebuild color pickers
    rebuildColorPickers();
    syncToYaml();
    sendPreview();
  }

  function rebuildColorPickers() {
    var container = document.getElementById('colorPickersContainer');
    container.innerHTML = '';
    
    for (var i = 0; i < currentPreset.colorCount; i++) {
      var color = currentPreset.colors[i] || {r: 255, g: 255, b: 255};
      var html = '<div class="color-section" id="colorPicker' + i + '" style="margin-bottom:16px;padding-bottom:16px;border-bottom:1px solid var(--border);">';
      if (currentPreset.colorCount > 1) {
        html += '<div style="margin-bottom:8px;font-size:0.75rem;color:var(--accent);font-weight:600;">COLOR ' + (i+1) + '</div>';
      }
      html += '<div style="display:flex;gap:16px;align-items:flex-start;">';
      html += '<div class="color-preview-large" id="colorPreview' + i + '" style="background:rgb(' + color.r + ',' + color.g + ',' + color.b + ');"></div>';
      html += '<div class="color-sliders" style="flex:1;">';
      html += '<div class="color-slider-row"><span class="color-label r">R</span>';
      html += '<input type="range" data-color-idx="' + i + '" data-channel="r" min="0" max="255" value="' + color.r + '" oninput="updateColorSlider(' + i + ')">';
      html += '<input type="text" class="color-value" id="valueR' + i + '" value="' + color.r + '" onchange="updateFromColorInput(' + i + ',\'r\')"></div>';
      html += '<div class="color-slider-row"><span class="color-label g">G</span>';
      html += '<input type="range" data-color-idx="' + i + '" data-channel="g" min="0" max="255" value="' + color.g + '" oninput="updateColorSlider(' + i + ')">';
      html += '<input type="text" class="color-value" id="valueG' + i + '" value="' + color.g + '" onchange="updateFromColorInput(' + i + ',\'g\')"></div>';
      html += '<div class="color-slider-row"><span class="color-label b">B</span>';
      html += '<input type="range" data-color-idx="' + i + '" data-channel="b" min="0" max="255" value="' + color.b + '" oninput="updateColorSlider(' + i + ')">';
      html += '<input type="text" class="color-value" id="valueB' + i + '" value="' + color.b + '" onchange="updateFromColorInput(' + i + ',\'b\')"></div>';
      html += '</div></div></div>';
      container.innerHTML += html;
    }
  }

  function updateColorSlider(colorIdx) {
    var sliders = document.querySelectorAll('[data-color-idx="' + colorIdx + '"]');
    var r = 0, g = 0, b = 0;
    sliders.forEach(function(s) {
      if (s.dataset.channel === 'r') r = parseInt(s.value);
      if (s.dataset.channel === 'g') g = parseInt(s.value);
      if (s.dataset.channel === 'b') b = parseInt(s.value);
    });
    
    currentPreset.colors[colorIdx] = {r: r, g: g, b: b};
    if (colorIdx === 0) {
      currentPreset.r = r;
      currentPreset.g = g;
      currentPreset.b = b;
    }
    
    document.getElementById('valueR' + colorIdx).value = r;
    document.getElementById('valueG' + colorIdx).value = g;
    document.getElementById('valueB' + colorIdx).value = b;
    document.getElementById('colorPreview' + colorIdx).style.background = 'rgb(' + r + ',' + g + ',' + b + ')';
    
    syncToYaml();
    sendPreview();
  }

  function updateFromColorInput(colorIdx, channel) {
    var inputId = 'value' + channel.toUpperCase() + colorIdx;
    var val = parseInt(document.getElementById(inputId).value) || 0;
    val = Math.max(0, Math.min(255, val));
    
    var slider = document.querySelector('[data-color-idx="' + colorIdx + '"][data-channel="' + channel + '"]');
    if (slider) slider.value = val;
    
    updateColorSlider(colorIdx);
  }

  // Legacy single-color functions for compatibility
  function updateColor() {
    updateColorSlider(0);
  }

  function updateFromInput(channel) {
    updateFromColorInput(0, channel);
  }

  function updateParam(param) {
    var val = parseInt(document.getElementById(param).value);
    currentPreset[param] = val;
    
    if (param === 'brightness') {
      document.getElementById('brightnessVal').textContent = val + '%';
    } else if (param === 'speed') {
      document.getElementById('speedVal').textContent = val;
    } else if (param === 'minBrightness') {
      document.getElementById('minBrightnessVal').textContent = val + '%';
      currentPreset.params.min_brightness = val;
    }
    
    syncToYaml();
    sendPreview();
  }

  function syncToYaml() {
    var yaml = 'name: "' + (currentPreset.name || 'New Preset') + '"\n';
    yaml += 'animation: ' + currentPreset.animation + '\n';
    yaml += 'brightness: ' + currentPreset.brightness + '\n';
    yaml += 'speed: ' + currentPreset.speed + '\n';
    yaml += 'colorCount: ' + currentPreset.colorCount + '\n';
    yaml += 'colors:\n';
    for (var i = 0; i < currentPreset.colors.length; i++) {
      var c = currentPreset.colors[i];
      yaml += '  - r: ' + c.r + '\n';
      yaml += '    g: ' + c.g + '\n';
      yaml += '    b: ' + c.b + '\n';
    }
    
    if (Object.keys(currentPreset.params || {}).length > 0) {
      yaml += 'params:\n';
      for (var key in currentPreset.params) {
        yaml += '  ' + key + ': ' + currentPreset.params[key] + '\n';
      }
    }
    
    document.getElementById('yamlEditor').value = yaml;
    validateYaml();
  }

  function validateYaml() {
    var yaml = document.getElementById('yamlEditor').value;
    var statusEl = document.getElementById('yamlStatus');
    
    try {
      // Simple YAML validation (just check for basic structure)
      if (yaml.includes('animation:') && yaml.includes('colors:')) {
        statusEl.innerHTML = '<span>&#x2713;</span> Valid YAML';
        statusEl.className = 'yaml-status valid';
        return true;
      } else {
        statusEl.innerHTML = '<span>&#x2717;</span> Missing required fields';
        statusEl.className = 'yaml-status invalid';
        return false;
      }
    } catch (e) {
      statusEl.innerHTML = '<span>&#x2717;</span> Invalid YAML: ' + e.message;
      statusEl.className = 'yaml-status invalid';
      return false;
    }
  }

  function syncFromYaml() {
    var yaml = document.getElementById('yamlEditor').value;
    
    // Simple YAML parser for our format
    try {
      var lines = yaml.split('\n');
      var inColor = false;
      var inParams = false;
      
      lines.forEach(function(line) {
        line = line.trim();
        if (!line || line.startsWith('#')) return;
        
        if (line === 'color:') { inColor = true; inParams = false; return; }
        if (line === 'params:') { inParams = true; inColor = false; return; }
        
        var match = line.match(/^(\w+):\s*(.+)$/);
        if (!match) return;
        
        var key = match[1];
        var val = match[2].replace(/^["']|["']$/g, '');
        
        if (inColor) {
          if (key === 'r') currentPreset.r = parseInt(val);
          if (key === 'g') currentPreset.g = parseInt(val);
          if (key === 'b') currentPreset.b = parseInt(val);
        } else if (inParams) {
          currentPreset.params = currentPreset.params || {};
          currentPreset.params[key] = parseFloat(val);
        } else {
          if (key === 'name') currentPreset.name = val;
          if (key === 'animation') currentPreset.animation = val;
          if (key === 'brightness') currentPreset.brightness = parseInt(val);
          if (key === 'speed') currentPreset.speed = parseInt(val);
        }
        
        // Check for end of section (non-indented line)
        if (!line.startsWith(' ') && !line.startsWith('\t') && line !== 'color:' && line !== 'params:') {
          inColor = false;
          inParams = false;
        }
      });
      
      updateUI();
      updateColor();
      showToast('YAML applied', 'success');
    } catch (e) {
      showToast('YAML parse error: ' + e.message, 'error');
    }
  }

  function togglePreview() {
    previewEnabled = document.getElementById('livePreview').checked;
  }

  function sendPreview() {
    if (!previewEnabled) return;
    
    clearTimeout(previewDebounce);
    previewDebounce = setTimeout(function() {
      fetch('/api/ledpreset/preview', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(currentPreset)
      }).catch(function() {});
    }, 100);
  }

  function savePreset() {
    currentPreset.name = document.getElementById('presetName').value.trim() || 'Unnamed Preset';
    
    var endpoint = presetId ? '/api/ledpreset/update' : '/api/ledpreset/create';
    var body = Object.assign({}, currentPreset);
    if (presetId) body.id = parseInt(presetId);
    
    fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        showToast('Preset saved', 'success');
        if (!presetId && data.id) {
          presetId = data.id;
          history.replaceState(null, '', '/advanced/ledpresets/edit?id=' + presetId);
        }
      } else {
        showToast(data.error || 'Save failed', 'error');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }

  function saveAndApply() {
    currentPreset.name = document.getElementById('presetName').value.trim() || 'Unnamed Preset';
    
    var endpoint = presetId ? '/api/ledpreset/update' : '/api/ledpreset/create';
    var body = Object.assign({}, currentPreset);
    if (presetId) body.id = parseInt(presetId);
    
    fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        var id = presetId || data.id;
        return fetch('/api/ledpreset/activate', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id: parseInt(id) })
        });
      } else {
        throw new Error(data.error || 'Save failed');
      }
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        showToast('Preset applied!', 'success');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }

  function showToast(message, type) {
    var toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className = 'toast ' + type + ' show';
    setTimeout(function() { toast.className = 'toast'; }, 3000);
  }

  init();
  </script>
</body>
</html>
)rawpage";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
