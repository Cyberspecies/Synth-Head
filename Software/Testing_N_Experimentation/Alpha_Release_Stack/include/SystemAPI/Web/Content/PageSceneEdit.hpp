/*****************************************************************
 * @file PageSceneEdit.hpp
 * @brief Scene Editor Page with LED/Display/Both sections
 * 
 * Features:
 * - Back button to scene list
 * - LED/Display/Both toggle selector
 * - Display Section: Animation, Shader, Transition subsections
 * - LED Section: Color picker for all LEDs
 * - Effects flag toggle
 * 
 * Animation Types:
 * - Gyro Eyes: Device gyro driven, uses sprites
 * - Static Image: Fixed sprite with size/rotation/position
 * - Sway: Sprite with sine/cos motion
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char* PAGE_SCENE_EDIT = R"rawpage(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Edit Scene</title>
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

    .scene-title {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 16px;
    }
    .scene-title input {
      flex: 1;
      padding: 10px 14px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-size: 1.1rem;
      font-weight: 600;
    }
    .scene-title input:focus { outline: none; border-color: var(--accent); }

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

    /* Target Selector */
    .target-selector {
      display: flex;
      gap: 10px;
      margin-bottom: 20px;
    }
    .target-option {
      flex: 1;
      padding: 16px;
      background: var(--bg-tertiary);
      border: 2px solid var(--border);
      border-radius: 10px;
      cursor: pointer;
      text-align: center;
      transition: all 0.2s;
    }
    .target-option:hover { border-color: var(--text-muted); }
    .target-option.selected { border-color: var(--accent); background: var(--accent-subtle); }
    .target-option .icon { font-size: 1.8rem; display: block; margin-bottom: 6px; }
    .target-option .label { font-size: 0.9rem; font-weight: 600; }

    /* Section Container */
    .section-container { display: none; margin-bottom: 20px; }
    .section-container.visible { display: block; }

    /* Panel */
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

    /* Subsection */
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

    /* Transition Grid */
    .trans-grid {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 8px;
    }
    @media (max-width: 600px) { .trans-grid { grid-template-columns: repeat(2, 1fr); } }
    .trans-card {
      background: var(--bg-secondary);
      border: 2px solid transparent;
      border-radius: 8px;
      padding: 10px 8px;
      cursor: pointer;
      text-align: center;
      transition: all 0.2s;
    }
    .trans-card:hover { border-color: var(--border); }
    .trans-card.selected { border-color: var(--accent); background: rgba(255,107,0,0.1); }
    .trans-card .icon { font-size: 16px; margin-bottom: 4px; color: var(--accent); }
    .trans-card .name { font-size: 0.7rem; font-weight: 600; }

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
    input[type="range"] {
      flex: 1;
      height: 6px;
      -webkit-appearance: none;
      background: var(--bg-secondary);
      border-radius: 3px;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 14px;
      height: 14px;
      background: var(--accent);
      border-radius: 50%;
      cursor: pointer;
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

    /* Sprite Picker */
    .sprite-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(56px, 1fr));
      gap: 6px;
      max-height: 150px;
      overflow-y: auto;
    }
    .sprite-item {
      aspect-ratio: 1;
      background: var(--bg-secondary);
      border: 2px solid transparent;
      border-radius: 6px;
      cursor: pointer;
      overflow: hidden;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .sprite-item:hover { border-color: var(--border); }
    .sprite-item.selected { border-color: var(--accent); }
    .sprite-item img { max-width: 100%; max-height: 100%; object-fit: contain; }
    .sprite-item .placeholder { color: var(--text-muted); font-size: 16px; }

    /* Color Picker */
    .color-picker-row {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .color-swatch {
      width: 48px;
      height: 48px;
      border-radius: 8px;
      border: 2px solid var(--border);
      cursor: pointer;
    }
    .color-inputs { display: flex; gap: 8px; }
    .color-inputs input {
      width: 50px;
      padding: 6px;
      text-align: center;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 4px;
      color: var(--text-primary);
      font-size: 0.8rem;
    }

    /* Effects Flag */
    .effects-flag {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 12px 16px;
      background: rgba(59, 130, 246, 0.1);
      border: 1px solid rgba(59, 130, 246, 0.3);
      border-radius: 8px;
      margin-bottom: 16px;
    }
    .effects-flag .info {
      display: flex;
      flex-direction: column;
      gap: 2px;
    }
    .effects-flag .title { font-size: 0.85rem; font-weight: 600; }
    .effects-flag .hint { font-size: 0.7rem; color: var(--text-secondary); }

    /* Save Bar */
    .save-bar {
      position: sticky;
      bottom: 0;
      background: var(--bg-card);
      border-top: 1px solid var(--border);
      padding: 12px 16px;
      display: flex;
      justify-content: flex-end;
      gap: 10px;
      margin: 0 -16px -16px -16px;
    }

    /* Toast */
    .toast {
      position: fixed;
      bottom: 80px;
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
      <a href="/advanced/scenes" class="back-link">&#x2190; Back to Scenes</a>
      
      <div class="card">
        <div class="card-body">
          <!-- Scene Name -->
          <div class="scene-title">
            <input type="text" id="sceneName" placeholder="Scene Name" maxlength="32">
          </div>

          <!-- Target Selector (LED/Display/Both) -->
          <div class="target-selector">
            <div class="target-option" data-target="display" onclick="toggleTarget('display')">
              <span class="icon">&#x25A6;</span>
              <span class="label">Display</span>
            </div>
            <div class="target-option" data-target="leds" onclick="toggleTarget('leds')">
              <span class="icon">&#x2606;</span>
              <span class="label">LEDs</span>
            </div>
          </div>

          <!-- Effects Only Flag (shown when Display is enabled) -->
          <div class="effects-flag" id="effectsFlag" style="display:none;">
            <div class="info">
              <span class="title">Effects Only</span>
              <span class="hint">This scene provides effects overlay, not main animation</span>
            </div>
            <label class="toggle">
              <input type="checkbox" id="effectsOnly" onchange="updateEffectsFlag()">
              <span class="toggle-slider"></span>
            </label>
          </div>

          <!-- DISPLAY SECTION -->
          <div class="section-container" id="sectionDisplay">
            <div class="panel">
              <div class="panel-header">
                <h3><span class="icon">&#x25A6;</span> Display Configuration</h3>
              </div>
              <div class="panel-body">
                
                <!-- Animation Subsection -->
                <div class="subsection">
                  <div class="subsection-title">Animation Type</div>
                  <div class="param-row">
                    <span class="param-label">Animation</span>
                    <div class="param-control">
                      <select id="animationType" onchange="selectAnimation(this.value)" style="flex:1;padding:10px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:6px;color:var(--text-primary);font-size:0.9rem;">
                        <!-- Populated dynamically from /api/registry/animations -->
                      </select>
                    </div>
                  </div>

                  <!-- Animation Parameters (dynamic) -->
                  <div id="animParams" style="margin-top: 16px;"></div>
                </div>

                <!-- Shader Subsection -->
                <div class="subsection">
                  <div class="subsection-title">Shader Effects</div>
                  <div class="param-row">
                    <span class="param-label">Enable AA</span>
                    <div class="param-control">
                      <label class="toggle">
                        <input type="checkbox" id="shaderAA" onchange="updateShader()">
                        <span class="toggle-slider"></span>
                      </label>
                    </div>
                  </div>
                  <div class="param-row">
                    <span class="param-label">Invert Colors</span>
                    <div class="param-control">
                      <label class="toggle">
                        <input type="checkbox" id="shaderInvert" onchange="updateShader()">
                        <span class="toggle-slider"></span>
                      </label>
                    </div>
                  </div>
                  <div class="param-row">
                    <span class="param-label">Color Override</span>
                    <div class="param-control">
                      <select id="shaderColorMode" onchange="updateShader()" style="flex:1;padding:8px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:6px;color:var(--text-primary);">
                        <option value="none">None</option>
                        <option value="solid">Solid Color</option>
                        <option value="rainbow">Rainbow Animation</option>
                      </select>
                    </div>
                  </div>
                  <div class="param-row" id="solidColorRow" style="display:none;">
                    <span class="param-label">Solid Color</span>
                    <div class="param-control color-picker-row">
                      <input type="color" id="shaderColor" value="#ffffff" onchange="updateShader()">
                    </div>
                  </div>
                </div>

                <!-- Transition Subsection -->
                <div class="subsection">
                  <div class="subsection-title">Intro Transition</div>
                  <div class="trans-grid" id="transGrid">
                    <div class="trans-card" data-trans="none" onclick="selectTransition('none')">
                      <div class="icon">&#x2794;</div>
                      <div class="name">None</div>
                    </div>
                    <div class="trans-card" data-trans="glitch" onclick="selectTransition('glitch')">
                      <div class="icon">&#x2593;</div>
                      <div class="name">Glitch</div>
                    </div>
                    <div class="trans-card" data-trans="sdf" onclick="selectTransition('sdf')">
                      <div class="icon">&#x25CF;</div>
                      <div class="name">SDF Morph</div>
                    </div>
                    <div class="trans-card" data-trans="particle" onclick="selectTransition('particle')">
                      <div class="icon">&#x2727;</div>
                      <div class="name">Particles</div>
                    </div>
                  </div>
                </div>

              </div>
            </div>
          </div>

          <!-- LED SECTION -->
          <div class="section-container" id="sectionLeds">
            <div class="panel">
              <div class="panel-header">
                <h3><span class="icon">&#x2606;</span> LED Configuration</h3>
              </div>
              <div class="panel-body">
                <div class="param-row">
                  <span class="param-label">LED Color</span>
                  <div class="param-control color-picker-row">
                    <input type="color" id="ledColor" value="#ff00ff" class="color-swatch" onchange="updateLedColor()">
                    <div class="color-inputs">
                      <input type="number" id="ledR" min="0" max="255" value="255" onchange="updateLedFromRgb()">
                      <input type="number" id="ledG" min="0" max="255" value="0" onchange="updateLedFromRgb()">
                      <input type="number" id="ledB" min="0" max="255" value="255" onchange="updateLedFromRgb()">
                    </div>
                  </div>
                </div>
                <div class="param-row">
                  <span class="param-label">Brightness</span>
                  <div class="param-control">
                    <input type="range" id="ledBrightness" min="0" max="100" value="80" oninput="updateLedBrightness()">
                    <span class="param-value" id="ledBrightnessVal">80%</span>
                  </div>
                </div>
              </div>
            </div>
          </div>

          <!-- Save Bar -->
          <div class="save-bar">
            <button class="btn btn-secondary" onclick="window.location.href='/advanced/scenes'">Cancel</button>
            <button class="btn btn-success" onclick="saveScene()">&#x2713; Save Scene</button>
          </div>

        </div>
      </div>
    </section>
  </div>

  <div id="toast" class="toast"></div>

  <script>
  // Scene state
  var sceneId = null;
  var scene = {
    name: '',
    displayEnabled: true,
    ledsEnabled: false,
    effectsOnly: false,
    animationType: 'static',
    animParams: {},
    shaderAA: true,
    shaderInvert: false,
    shaderColorMode: 'none',
    shaderColor: '#ffffff',
    transition: 'none',
    ledColor: {r: 255, g: 0, b: 255},
    ledBrightness: 80
  };
  var sprites = [];
  var animations = [];

  // Get scene ID from URL
  function getSceneId() {
    var params = new URLSearchParams(window.location.search);
    return params.get('id') ? parseInt(params.get('id')) : null;
  }

  // Load scene data
  function loadScene() {
    sceneId = getSceneId();
    if (!sceneId) {
      showToast('No scene ID', 'error');
      return;
    }
    
    fetch('/api/scene/get?id=' + sceneId)
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.scene) {
          scene = Object.assign(scene, data.scene);
          renderScene();
        } else {
          showToast('Scene not found', 'error');
        }
      })
      .catch(function(err) {
        showToast('Failed to load scene', 'error');
      });
  }

  // Load sprites
  function loadSprites() {
    fetch('/api/sprites')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        sprites = data.sprites || [];
      })
      .catch(function() {});
  }

  // Load animations from registry
  function loadAnimations() {
    fetch('/api/registry/animations')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        animations = data.animations || [];
        populateAnimationDropdown();
      })
      .catch(function() {
        console.error('Failed to load animations');
      });
  }

  // Populate animation dropdown from API data
  function populateAnimationDropdown() {
    var select = document.getElementById('animationType');
    select.innerHTML = '';
    animations.forEach(function(anim) {
      var opt = document.createElement('option');
      opt.value = anim.id;
      opt.textContent = anim.name;
      select.appendChild(opt);
    });
    // Set current value if scene already has one
    if (scene.animationType) {
      select.value = scene.animationType;
    }
  }

  // Render scene state to UI
  function renderScene() {
    document.getElementById('sceneName').value = scene.name || '';
    
    // Target selectors
    if (scene.displayEnabled) {
      document.querySelector('[data-target="display"]').classList.add('selected');
      document.getElementById('sectionDisplay').classList.add('visible');
      document.getElementById('effectsFlag').style.display = 'flex';
    }
    if (scene.ledsEnabled) {
      document.querySelector('[data-target="leds"]').classList.add('selected');
      document.getElementById('sectionLeds').classList.add('visible');
    }
    
    // Effects only
    document.getElementById('effectsOnly').checked = scene.effectsOnly || false;
    
    // Animation
    selectAnimation(scene.animationType || 'static');
    
    // Shader
    document.getElementById('shaderAA').checked = scene.shaderAA !== false;
    document.getElementById('shaderInvert').checked = scene.shaderInvert || false;
    document.getElementById('shaderColorMode').value = scene.shaderColorMode || 'none';
    document.getElementById('shaderColor').value = scene.shaderColor || '#ffffff';
    updateShaderColorRowVisibility();
    
    // Transition
    selectTransition(scene.transition || 'none');
    
    // LED
    var c = scene.ledColor || {r:255, g:0, b:255};
    document.getElementById('ledR').value = c.r;
    document.getElementById('ledG').value = c.g;
    document.getElementById('ledB').value = c.b;
    document.getElementById('ledColor').value = rgbToHex(c.r, c.g, c.b);
    document.getElementById('ledBrightness').value = scene.ledBrightness || 80;
    document.getElementById('ledBrightnessVal').textContent = (scene.ledBrightness || 80) + '%';
  }

  // Toggle target (display/leds)
  function toggleTarget(target) {
    var opt = document.querySelector('[data-target="' + target + '"]');
    var section = document.getElementById('section' + capitalize(target));
    
    if (opt.classList.contains('selected')) {
      opt.classList.remove('selected');
      section.classList.remove('visible');
      if (target === 'display') {
        scene.displayEnabled = false;
        document.getElementById('effectsFlag').style.display = 'none';
      } else {
        scene.ledsEnabled = false;
      }
    } else {
      opt.classList.add('selected');
      section.classList.add('visible');
      if (target === 'display') {
        scene.displayEnabled = true;
        document.getElementById('effectsFlag').style.display = 'flex';
      } else {
        scene.ledsEnabled = true;
      }
    }
  }

  function capitalize(s) { return s.charAt(0).toUpperCase() + s.slice(1); }

  // Update effects flag
  function updateEffectsFlag() {
    scene.effectsOnly = document.getElementById('effectsOnly').checked;
  }

  // Select animation type
  function selectAnimation(type) {
    scene.animationType = type;
    
    // Update dropdown
    document.getElementById('animationType').value = type;
    
    // Render animation-specific parameters
    renderAnimParams(type);
  }

  // Render animation parameters
  function renderAnimParams(type) {
    var container = document.getElementById('animParams');
    var html = '';
    
    if (type === 'static_sprite' || type === 'static_image' || type === 'static') {
      html = '<div class="subsection-title" style="margin-top:0;">Static Image Settings</div>' +
        '<div class="param-row"><span class="param-label">Sprite</span><div class="param-control"><select id="staticSprite" onchange="updateAnimParam(\'sprite\', this.value)" style="flex:1;padding:8px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:6px;color:var(--text-primary);">' +
        sprites.map(function(s) { return '<option value="' + s.id + '">' + s.name + '</option>'; }).join('') +
        '</select></div></div>' +
        '<div class="param-row"><span class="param-label">Scale</span><div class="param-control"><input type="range" id="staticScale" min="0.5" max="4" step="0.1" value="1" oninput="updateAnimParam(\'scale\', this.value);document.getElementById(\'staticScaleVal\').textContent=this.value+\'x\'"><span class="param-value" id="staticScaleVal">1x</span></div></div>' +
        '<div class="param-row"><span class="param-label">Rotation</span><div class="param-control"><input type="range" id="staticRotation" min="0" max="360" value="0" oninput="updateAnimParam(\'rotation\', this.value);document.getElementById(\'staticRotVal\').textContent=this.value+\'°\'"><span class="param-value" id="staticRotVal">0°</span></div></div>' +
        '<div class="param-row"><span class="param-label">Position X</span><div class="param-control"><input type="range" id="staticX" min="0" max="128" value="64" oninput="updateAnimParam(\'posX\', this.value);document.getElementById(\'staticXVal\').textContent=this.value"><span class="param-value" id="staticXVal">64</span></div></div>' +
        '<div class="param-row"><span class="param-label">Position Y</span><div class="param-control"><input type="range" id="staticY" min="0" max="32" value="16" oninput="updateAnimParam(\'posY\', this.value);document.getElementById(\'staticYVal\').textContent=this.value"><span class="param-value" id="staticYVal">16</span></div></div>';
    } else if (type === 'static_mirrored') {
      html = '<div class="subsection-title" style="margin-top:0;">Mirrored Display Settings</div>' +
        '<div class="param-row"><span class="param-label">Sprite</span><div class="param-control"><select id="mirroredSprite" onchange="updateAnimParam(\'sprite\', this.value)" style="flex:1;padding:8px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:6px;color:var(--text-primary);">' +
        sprites.map(function(s) { return '<option value="' + s.id + '">' + s.name + '</option>'; }).join('') +
        '</select></div></div>' +
        '<div class="subsection-title" style="margin-top:12px;">Left Panel</div>' +
        '<div class="param-row"><span class="param-label">Position X</span><div class="param-control"><input type="range" id="leftX" min="0" max="64" value="32" oninput="updateAnimParam(\'left_x\', this.value);document.getElementById(\'leftXVal\').textContent=this.value"><span class="param-value" id="leftXVal">32</span></div></div>' +
        '<div class="param-row"><span class="param-label">Position Y</span><div class="param-control"><input type="range" id="leftY" min="0" max="32" value="16" oninput="updateAnimParam(\'left_y\', this.value);document.getElementById(\'leftYVal\').textContent=this.value"><span class="param-value" id="leftYVal">16</span></div></div>' +
        '<div class="param-row"><span class="param-label">Rotation</span><div class="param-control"><input type="range" id="leftRot" min="0" max="360" value="0" oninput="updateAnimParam(\'left_rotation\', this.value);document.getElementById(\'leftRotVal\').textContent=this.value+\'°\'"><span class="param-value" id="leftRotVal">0°</span></div></div>' +
        '<div class="param-row"><span class="param-label">Scale</span><div class="param-control"><input type="range" id="leftScale" min="0.5" max="4" step="0.1" value="1" oninput="updateAnimParam(\'left_scale\', this.value);document.getElementById(\'leftScaleVal\').textContent=this.value+\'x\'"><span class="param-value" id="leftScaleVal">1x</span></div></div>' +
        '<div class="subsection-title" style="margin-top:12px;">Right Panel</div>' +
        '<div class="param-row"><span class="param-label">Position X</span><div class="param-control"><input type="range" id="rightX" min="64" max="128" value="96" oninput="updateAnimParam(\'right_x\', this.value);document.getElementById(\'rightXVal\').textContent=this.value"><span class="param-value" id="rightXVal">96</span></div></div>' +
        '<div class="param-row"><span class="param-label">Position Y</span><div class="param-control"><input type="range" id="rightY" min="0" max="32" value="16" oninput="updateAnimParam(\'right_y\', this.value);document.getElementById(\'rightYVal\').textContent=this.value"><span class="param-value" id="rightYVal">16</span></div></div>' +
        '<div class="param-row"><span class="param-label">Rotation</span><div class="param-control"><input type="range" id="rightRot" min="0" max="360" value="180" oninput="updateAnimParam(\'right_rotation\', this.value);document.getElementById(\'rightRotVal\').textContent=this.value+\'°\'"><span class="param-value" id="rightRotVal">180°</span></div></div>' +
        '<div class="param-row"><span class="param-label">Scale</span><div class="param-control"><input type="range" id="rightScale" min="0.5" max="4" step="0.1" value="1" oninput="updateAnimParam(\'right_scale\', this.value);document.getElementById(\'rightScaleVal\').textContent=this.value+\'x\'"><span class="param-value" id="rightScaleVal">1x</span></div></div>';
    }
    
    container.innerHTML = html;
    
    // Apply saved values if present
    if (scene.animParams) {
      // Restore values from scene.animParams
    }
  }

  function updateAnimParam(key, value) {
    if (!scene.animParams) scene.animParams = {};
    scene.animParams[key] = value;
  }

  // Select transition
  function selectTransition(type) {
    scene.transition = type;
    document.querySelectorAll('.trans-card').forEach(function(el) {
      el.classList.toggle('selected', el.dataset.trans === type);
    });
  }

  // Update shader settings
  function updateShader() {
    scene.shaderAA = document.getElementById('shaderAA').checked;
    scene.shaderInvert = document.getElementById('shaderInvert').checked;
    scene.shaderColorMode = document.getElementById('shaderColorMode').value;
    scene.shaderColor = document.getElementById('shaderColor').value;
    updateShaderColorRowVisibility();
  }

  function updateShaderColorRowVisibility() {
    var mode = document.getElementById('shaderColorMode').value;
    document.getElementById('solidColorRow').style.display = mode === 'solid' ? 'flex' : 'none';
  }

  // LED color updates
  function updateLedColor() {
    var hex = document.getElementById('ledColor').value;
    var rgb = hexToRgb(hex);
    document.getElementById('ledR').value = rgb.r;
    document.getElementById('ledG').value = rgb.g;
    document.getElementById('ledB').value = rgb.b;
    scene.ledColor = rgb;
  }

  function updateLedFromRgb() {
    var r = parseInt(document.getElementById('ledR').value) || 0;
    var g = parseInt(document.getElementById('ledG').value) || 0;
    var b = parseInt(document.getElementById('ledB').value) || 0;
    document.getElementById('ledColor').value = rgbToHex(r, g, b);
    scene.ledColor = {r: r, g: g, b: b};
  }

  function updateLedBrightness() {
    var val = document.getElementById('ledBrightness').value;
    document.getElementById('ledBrightnessVal').textContent = val + '%';
    scene.ledBrightness = parseInt(val);
  }

  // Color helpers
  function hexToRgb(hex) {
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : {r: 0, g: 0, b: 0};
  }

  function rgbToHex(r, g, b) {
    return '#' + [r, g, b].map(function(x) {
      var hex = x.toString(16);
      return hex.length === 1 ? '0' + hex : hex;
    }).join('');
  }

  // Save scene
  function saveScene() {
    scene.name = document.getElementById('sceneName').value.trim();
    if (!scene.name) {
      showToast('Please enter a scene name', 'error');
      return;
    }
    
    // Flatten scene data for backend
    var payload = {
      id: sceneId,
      name: scene.name,
      displayEnabled: scene.displayEnabled,
      ledsEnabled: scene.ledsEnabled,
      effectsOnly: scene.effectsOnly,
      animType: scene.animationType || 'static',
      transition: scene.transition || 'none',
      shaderAA: scene.shaderAA,
      shaderInvert: scene.shaderInvert,
      shaderColorMode: scene.shaderColorMode,
      shaderColor: scene.shaderColor,
      ledColor: scene.ledColor,
      ledBrightness: scene.ledBrightness,
      animParams: scene.animParams || {}
    };
    
    console.log('Saving scene:', payload);
    
    fetch('/api/scene/update', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(payload)
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        showToast('Scene saved', 'success');
        setTimeout(function() {
          window.location.href = '/advanced/scenes';
        }, 1000);
      } else {
        showToast(data.error || 'Failed to save', 'error');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }

  // Toast
  function showToast(msg, type) {
    var t = document.getElementById('toast');
    t.textContent = msg;
    t.className = 'toast ' + type + ' show';
    setTimeout(function() { t.classList.remove('show'); }, 3000);
  }

  // Initialize
  loadAnimations();
  loadSprites();
  loadScene();
  </script>
</body>
</html>
)rawpage";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
