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

    /* Mic Reactivity Button */
    .mic-btn {
      width: 28px;
      height: 28px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 0.85rem;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.2s;
      flex-shrink: 0;
      margin-left: 6px;
    }
    .mic-btn:hover {
      background: var(--accent-subtle);
      border-color: var(--accent);
      color: var(--accent);
    }
    .mic-btn.active {
      background: var(--accent);
      border-color: var(--accent);
      color: var(--bg-primary);
    }

    /* Mic Reactivity Modal */
    .mic-modal-overlay {
      position: fixed;
      top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(0,0,0,0.85);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 3000;
      padding: 20px;
    }
    .mic-modal-overlay.show { display: flex; }
    .mic-modal {
      background: var(--bg-card);
      border-radius: 16px;
      padding: 24px;
      width: 100%;
      max-width: 380px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.5);
    }
    .mic-modal-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
    }
    .mic-modal-title {
      font-size: 1.1rem;
      font-weight: 600;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .mic-modal-title .icon { color: var(--accent); }
    .mic-modal-close {
      width: 32px;
      height: 32px;
      background: var(--bg-secondary);
      border: none;
      border-radius: 8px;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 1rem;
    }
    .mic-modal-close:hover { background: var(--bg-tertiary); color: var(--text-primary); }

    /* Mic Modal Toggle */
    .mic-enable-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 12px 16px;
      background: var(--bg-secondary);
      border-radius: 10px;
      margin-bottom: 16px;
    }
    .mic-enable-label { font-size: 0.9rem; font-weight: 500; }

    /* Mic Modal Formula Display */
    .mic-formula {
      background: var(--bg-secondary);
      padding: 14px;
      border-radius: 10px;
      margin-bottom: 16px;
      text-align: center;
    }
    .mic-formula-title {
      font-size: 0.7rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
      margin-bottom: 6px;
    }
    .mic-formula-expr {
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 1rem;
      color: var(--accent);
    }
    .mic-formula-expr .var { color: var(--info); }

    /* Mic Modal Param Rows */
    .mic-param-row {
      display: flex;
      align-items: center;
      padding: 12px 0;
      border-bottom: 1px solid var(--border);
    }
    .mic-param-row:last-child { border-bottom: none; }
    .mic-param-label {
      flex: 0 0 45%;
      font-size: 0.85rem;
    }
    .mic-param-label .hint {
      font-size: 0.7rem;
      color: var(--text-muted);
      display: block;
    }
    .mic-param-input {
      flex: 1;
      padding: 10px 12px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-size: 0.9rem;
      font-family: 'SF Mono', Monaco, monospace;
    }
    .mic-param-input:focus {
      border-color: var(--accent);
      outline: none;
    }

    /* Mic Live Preview */
    .mic-preview {
      background: linear-gradient(135deg, var(--bg-secondary), var(--bg-tertiary));
      padding: 14px;
      border-radius: 10px;
      margin-top: 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .mic-preview-item {
      text-align: center;
    }
    .mic-preview-label {
      font-size: 0.65rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .mic-preview-value {
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 1.1rem;
      font-weight: 600;
      color: var(--text-primary);
      margin-top: 2px;
    }
    .mic-preview-value.accent { color: var(--accent); }

    /* Mic Modal Actions */
    .mic-modal-actions {
      display: flex;
      gap: 10px;
      margin-top: 20px;
      padding-top: 16px;
      border-top: 1px solid var(--border);
    }
    .mic-modal-actions .btn { flex: 1; }
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

  <!-- Mic Reactivity Modal -->
  <div id="micModal" class="mic-modal-overlay" onclick="if(event.target===this)closeMicModal()">
    <div class="mic-modal">
      <div class="mic-modal-header">
        <div class="mic-modal-title">
          <span class="icon">&#x1F3A4;</span>
          <span id="micModalTitle">Mic Reactivity</span>
        </div>
        <button class="mic-modal-close" onclick="closeMicModal()">&#x2715;</button>
      </div>
      
      <div class="mic-enable-row">
        <span class="mic-enable-label">Enable Mic Control</span>
        <label class="toggle">
          <input type="checkbox" id="micEnabled" onchange="updateMicPreview()">
          <span class="toggle-slider"></span>
        </label>
      </div>
      
      <div class="mic-formula">
        <div class="mic-formula-title">Formula</div>
        <div class="mic-formula-expr">(<span class="var">Y</span> × (Mic - <span class="var">X</span>)) + <span class="var">Z</span></div>
      </div>
      
      <div class="mic-param-row">
        <div class="mic-param-label">
          X - Offset
          <span class="hint">Subtracted from mic (0-255)</span>
        </div>
        <input type="number" id="micOffsetX" class="mic-param-input" value="0" min="-255" max="255" step="1" oninput="updateMicPreview()">
      </div>
      
      <div class="mic-param-row">
        <div class="mic-param-label">
          Y - Multiplier
          <span class="hint">Scales the result</span>
        </div>
        <input type="number" id="micMultY" class="mic-param-input" value="1" min="-10" max="10" step="0.01" oninput="updateMicPreview()">
      </div>
      
      <div class="mic-param-row">
        <div class="mic-param-label">
          Z - Base Offset
          <span class="hint">Added to final value</span>
        </div>
        <input type="number" id="micOffsetZ" class="mic-param-input" value="0" step="0.1" oninput="updateMicPreview()">
      </div>
      
      <div class="mic-preview">
        <div class="mic-preview-item">
          <div class="mic-preview-label">Mic Level</div>
          <div class="mic-preview-value" id="micPreviewLevel">--</div>
        </div>
        <div class="mic-preview-item">
          <div class="mic-preview-label">&#x2192;</div>
          <div class="mic-preview-value">&#x2192;</div>
        </div>
        <div class="mic-preview-item">
          <div class="mic-preview-label">Output</div>
          <div class="mic-preview-value accent" id="micPreviewOutput">--</div>
        </div>
      </div>
      
      <div class="mic-modal-actions">
        <button class="btn btn-secondary" onclick="closeMicModal()">Cancel</button>
        <button class="btn btn-primary" onclick="saveMicSettings()">&#x2713; Apply</button>
      </div>
    </div>
  </div>

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

  // ========== Mic Reactivity System ==========
  var currentMicParam = null;  // Which param is being edited
  var micLevel = 0;           // Current mic level (0-255)
  var micReactSettings = {};  // Settings per param: { enabled, x, y, z }
  var micPollInterval = null;
  
  // Helper to create param row with mic button - MUST be defined before renderAnimParams
  function makeParamRow(label, inputId, min, max, step, defVal, paramKey, suffix, valueId) {
    var micBtnClass = (micReactSettings[paramKey] && micReactSettings[paramKey].enabled) ? 'mic-btn active' : 'mic-btn';
    return '<div class="param-row">' +
      '<span class="param-label">' + label + '</span>' +
      '<div class="param-control">' +
        '<input type="range" id="' + inputId + '" min="' + min + '" max="' + max + '" step="' + step + '" value="' + defVal + '" ' +
          'oninput="updateAnimParam(\'' + paramKey + '\', this.value);document.getElementById(\'' + valueId + '\').textContent=this.value+\'' + suffix + '\'">' +
        '<span class="param-value" id="' + valueId + '">' + defVal + suffix + '</span>' +
        '<button class="' + micBtnClass + '" onclick="openMicModal(\'' + paramKey + '\', \'' + label + '\')" title="Mic Reactivity">&#x1F3A4;</button>' +
      '</div>' +
    '</div>';
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
    
    console.log('renderAnimParams called with type:', type);
    
    if (type === 'static_sprite' || type === 'static_image' || type === 'static') {
      html = '<div class="subsection-title" style="margin-top:0;">Static Image Settings</div>' +
        '<div class="param-row"><span class="param-label">Sprite</span><div class="param-control"><select id="staticSprite" onchange="updateAnimParam(\'sprite\', this.value)" style="flex:1;padding:8px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:6px;color:var(--text-primary);">' +
        sprites.map(function(s) { return '<option value="' + s.id + '">' + s.name + '</option>'; }).join('') +
        '</select></div></div>' +
        makeParamRow('Scale', 'staticScale', 0.5, 4, 0.1, 1, 'scale', 'x', 'staticScaleVal') +
        makeParamRow('Rotation', 'staticRotation', 0, 360, 1, 0, 'rotation', '°', 'staticRotVal') +
        makeParamRow('Position X', 'staticX', 0, 128, 1, 64, 'posX', '', 'staticXVal') +
        makeParamRow('Position Y', 'staticY', 0, 32, 1, 16, 'posY', '', 'staticYVal');
    } else if (type === 'static_mirrored') {
      html = '<div class="subsection-title" style="margin-top:0;">Mirrored Display Settings</div>' +
        '<div class="param-row"><span class="param-label">Sprite</span><div class="param-control"><select id="mirroredSprite" onchange="updateAnimParam(\'sprite\', this.value)" style="flex:1;padding:8px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:6px;color:var(--text-primary);">' +
        sprites.map(function(s) { return '<option value="' + s.id + '">' + s.name + '</option>'; }).join('') +
        '</select></div></div>' +
        '<div class="subsection-title" style="margin-top:12px;">Left Panel</div>' +
        makeParamRow('Position X', 'leftX', 0, 64, 1, 32, 'left_x', '', 'leftXVal') +
        makeParamRow('Position Y', 'leftY', 0, 32, 1, 16, 'left_y', '', 'leftYVal') +
        makeParamRow('Rotation', 'leftRot', 0, 360, 1, 0, 'left_rotation', '°', 'leftRotVal') +
        makeParamRow('Scale', 'leftScale', 0.5, 4, 0.1, 1, 'left_scale', 'x', 'leftScaleVal') +
        '<div class="subsection-title" style="margin-top:12px;">Right Panel</div>' +
        makeParamRow('Position X', 'rightX', 64, 128, 1, 96, 'right_x', '', 'rightXVal') +
        makeParamRow('Position Y', 'rightY', 0, 32, 1, 16, 'right_y', '', 'rightYVal') +
        makeParamRow('Rotation', 'rightRot', 0, 360, 1, 180, 'right_rotation', '°', 'rightRotVal') +
        makeParamRow('Scale', 'rightScale', 0.5, 4, 0.1, 1, 'right_scale', 'x', 'rightScaleVal');
    } else if (type === 'reactive_eyes') {
      html = '<div class="subsection-title" style="margin-top:0;">Reactive Eyes Settings</div>' +
        '<div class="param-row"><span class="param-label">Sprite</span><div class="param-control"><select id="reactiveSprite" onchange="updateAnimParam(\'sprite\', this.value)" style="flex:1;padding:8px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:6px;color:var(--text-primary);">' +
        sprites.map(function(s) { return '<option value="' + s.id + '">' + s.name + '</option>'; }).join('') +
        '</select></div></div>' +
        makeParamRow('Y Sensitivity', 'reactiveSensY', 0, 50, 1, 15, 'reactive_y_sensitivity', '', 'reactiveSensYVal') +
        makeParamRow('Smoothing', 'reactiveSmooth', 0.01, 1, 0.01, 0.15, 'reactive_smoothing', '', 'reactiveSmoothVal');
    } else {
      // Fallback: show generic controls for unknown animation types
      html = '<div class="subsection-title" style="margin-top:0;">Animation: ' + type + '</div>' +
        '<p style="color:var(--text-muted);font-size:0.85rem;">No custom parameters for this animation type.</p>';
    }
    
    container.innerHTML = html;
    
    // Apply saved values if present and load mic settings
    if (scene.animParams) {
      // Restore values from scene.animParams
      loadMicReactSettings();
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
    
    // Ensure mic react settings are in animParams
    if (!scene.animParams) scene.animParams = {};
    scene.animParams.micReact = micReactSettings;
    
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

  // Open mic modal for a parameter
  function openMicModal(paramKey, label) {
    currentMicParam = paramKey;
    document.getElementById('micModalTitle').textContent = 'Mic: ' + label;
    
    // Load saved settings for this param
    var settings = micReactSettings[paramKey] || { enabled: false, x: 0, y: 1, z: 0 };
    document.getElementById('micEnabled').checked = settings.enabled;
    document.getElementById('micOffsetX').value = settings.x;
    document.getElementById('micMultY').value = settings.y;
    document.getElementById('micOffsetZ').value = settings.z;
    
    // Start polling mic level for preview
    startMicPolling();
    updateMicPreview();
    
    document.getElementById('micModal').classList.add('show');
  }
  
  // Close mic modal
  function closeMicModal() {
    document.getElementById('micModal').classList.remove('show');
    currentMicParam = null;
    stopMicPolling();
  }
  
  // Save mic settings for current param
  function saveMicSettings() {
    if (!currentMicParam) return;
    
    micReactSettings[currentMicParam] = {
      enabled: document.getElementById('micEnabled').checked,
      x: parseFloat(document.getElementById('micOffsetX').value) || 0,
      y: parseFloat(document.getElementById('micMultY').value) || 1,
      z: parseFloat(document.getElementById('micOffsetZ').value) || 0
    };
    
    // Store in scene for persistence
    if (!scene.animParams) scene.animParams = {};
    scene.animParams.micReact = micReactSettings;
    
    // Update mic button appearance
    updateMicButtonStates();
    
    closeMicModal();
    showToast('Mic reactivity applied', 'success');
  }
  
  // Update all mic button states (active/inactive)
  function updateMicButtonStates() {
    document.querySelectorAll('.mic-btn').forEach(function(btn) {
      var onclick = btn.getAttribute('onclick');
      if (onclick) {
        var match = onclick.match(/openMicModal\('([^']+)'/);
        if (match) {
          var param = match[1];
          if (micReactSettings[param] && micReactSettings[param].enabled) {
            btn.classList.add('active');
          } else {
            btn.classList.remove('active');
          }
        }
      }
    });
  }
  
  // Poll mic level from API
  function startMicPolling() {
    if (micPollInterval) return;
    micPollInterval = setInterval(function() {
      fetch('/api/sensors')
        .then(function(r) { return r.json(); })
        .then(function(data) {
          // mic_db is typically -60 to 0 dB, convert to 0-255
          var db = data.mic_db || -60;
          micLevel = Math.round(((db + 60) / 60) * 255);
          micLevel = Math.max(0, Math.min(255, micLevel));
          updateMicPreview();
          applyMicReactivity();
        })
        .catch(function() {});
    }, 50);  // 50ms = 20Hz update rate
  }
  
  function stopMicPolling() {
    if (micPollInterval) {
      clearInterval(micPollInterval);
      micPollInterval = null;
    }
  }
  
  // Update live preview in modal
  function updateMicPreview() {
    var x = parseFloat(document.getElementById('micOffsetX').value) || 0;
    var y = parseFloat(document.getElementById('micMultY').value) || 1;
    var z = parseFloat(document.getElementById('micOffsetZ').value) || 0;
    
    // Calculate output: (Y * (mic - X)) + Z
    var output = (y * (micLevel - x)) + z;
    
    document.getElementById('micPreviewLevel').textContent = micLevel;
    document.getElementById('micPreviewOutput').textContent = output.toFixed(1);
  }
  
  // Apply mic reactivity to all enabled params
  function applyMicReactivity() {
    Object.keys(micReactSettings).forEach(function(paramKey) {
      var settings = micReactSettings[paramKey];
      if (!settings || !settings.enabled) return;
      
      // Calculate value: (Y * (mic - X)) + Z
      var output = (settings.y * (micLevel - settings.x)) + settings.z;
      
      // Find the corresponding slider and update it
      var slider = findSliderForParam(paramKey);
      if (slider) {
        // Clamp to slider range
        var min = parseFloat(slider.min) || 0;
        var max = parseFloat(slider.max) || 100;
        output = Math.max(min, Math.min(max, output));
        
        slider.value = output;
        // Trigger the input event to update display and scene
        slider.dispatchEvent(new Event('input'));
      }
    });
  }
  
  // Find slider element for a param key
  function findSliderForParam(paramKey) {
    // Map param keys to slider IDs
    var sliderMap = {
      'scale': 'staticScale',
      'rotation': 'staticRotation',
      'posX': 'staticX',
      'posY': 'staticY',
      'left_x': 'leftX',
      'left_y': 'leftY',
      'left_rotation': 'leftRot',
      'left_scale': 'leftScale',
      'right_x': 'rightX',
      'right_y': 'rightY',
      'right_rotation': 'rightRot',
      'right_scale': 'rightScale',
      'reactive_y_sensitivity': 'reactiveSensY',
      'reactive_smoothing': 'reactiveSmooth'
    };
    
    var sliderId = sliderMap[paramKey];
    if (sliderId) {
      return document.getElementById(sliderId);
    }
    return null;
  }
  
  // Load mic react settings from scene
  function loadMicReactSettings() {
    if (scene.animParams && scene.animParams.micReact) {
      micReactSettings = scene.animParams.micReact;
      updateMicButtonStates();
      
      // Start polling if any are enabled
      var anyEnabled = Object.keys(micReactSettings).some(function(k) {
        return micReactSettings[k] && micReactSettings[k].enabled;
      });
      if (anyEnabled) {
        startMicPolling();
      }
    }
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
