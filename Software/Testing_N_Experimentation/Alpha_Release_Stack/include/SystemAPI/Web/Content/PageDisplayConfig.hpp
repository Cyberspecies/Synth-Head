/*****************************************************************
 * @file PageDisplayConfig.hpp
 * @brief Display Configuration Page using YAML-driven UI
 * 
 * This page auto-generates a settings UI based on the active
 * scene's YAML configuration. All controls are defined by the
 * schema metadata in the YAML file itself.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

/**
 * @brief Display Configuration Page HTML
 * 
 * The page loads the current scene YAML and renders UI controls
 * based on the schema definitions. All updates are sent to
 * /api/scene/update and saved back to the YAML file.
 */
inline const char PAGE_DISPLAY_CONFIG[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Display Config</title>
  <link rel="stylesheet" href="/style.css">
  <style>
/* ============================================
   YAML UI Generator Styles
   ============================================ */

.yaml-form {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

/* Section Cards */
.yaml-section {
  background: var(--bg-card);
  border-radius: 12px;
  border: 1px solid var(--border);
  overflow: hidden;
}

.yaml-section-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px 18px;
  background: var(--bg-tertiary);
  cursor: pointer;
  user-select: none;
}

.yaml-section-header:hover {
  background: var(--bg-secondary);
}

.yaml-section-title {
  display: flex;
  align-items: center;
  gap: 10px;
  font-weight: 600;
  font-size: 14px;
  color: var(--text-primary);
}

.yaml-section-icon {
  font-size: 16px;
  color: var(--accent);
  opacity: 0.9;
}

.yaml-section-chevron {
  transition: transform 0.2s ease;
  color: var(--text-muted);
}

.yaml-section.collapsed .yaml-section-chevron {
  transform: rotate(-90deg);
}

.yaml-section-body {
  padding: 18px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.yaml-section.collapsed .yaml-section-body {
  display: none;
}

/* Field Rows */
.yaml-field-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 0;
  border-bottom: 1px solid var(--border);
}

.yaml-field-row:last-child {
  border-bottom: none;
}

.yaml-field-label {
  flex: 0 0 40%;
  font-size: 13px;
  color: var(--text-secondary);
  display: flex;
  align-items: center;
  gap: 6px;
}

.yaml-field-label .help-icon {
  width: 14px;
  height: 14px;
  font-size: 10px;
  background: var(--bg-tertiary);
  border-radius: 50%;
  text-align: center;
  line-height: 14px;
  color: var(--text-muted);
  cursor: help;
}

.yaml-field-control {
  flex: 0 0 55%;
  display: flex;
  align-items: center;
  gap: 8px;
}

/* Text Input */
.yaml-input-text {
  width: 100%;
  padding: 10px 14px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 8px;
  color: var(--text-primary);
  font-size: 13px;
  transition: border-color 0.2s;
}

.yaml-input-text:focus {
  outline: none;
  border-color: var(--accent);
  box-shadow: 0 0 0 3px var(--accent-glow);
}

/* Number Input */
.yaml-input-number {
  width: 90px;
  padding: 10px 30px 10px 12px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 8px;
  color: var(--text-primary);
  font-size: 13px;
  font-family: 'SF Mono', Monaco, monospace;
  text-align: left;
  -moz-appearance: textfield;
}

.yaml-input-number::-webkit-inner-spin-button,
.yaml-input-number::-webkit-outer-spin-button {
  -webkit-appearance: none;
  margin: 0;
}

.yaml-input-number:focus {
  outline: none;
  border-color: var(--accent);
}

.yaml-input-unit {
  font-size: 12px;
  color: var(--text-muted);
  min-width: 24px;
}

/* Slider */
.yaml-slider-container {
  display: flex;
  align-items: center;
  gap: 10px;
  width: 100%;
}

.yaml-slider {
  flex: 1;
  height: 6px;
  -webkit-appearance: none;
  background: var(--bg-tertiary);
  border-radius: 3px;
  outline: none;
}

.yaml-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 16px;
  height: 16px;
  background: var(--accent);
  border-radius: 50%;
  cursor: pointer;
}

.yaml-slider-value {
  min-width: 50px;
  text-align: right;
  font-size: 12px;
  font-family: 'SF Mono', Monaco, monospace;
  color: var(--text-muted);
}

/* Toggle Switch */
.yaml-toggle {
  position: relative;
  width: 44px;
  height: 24px;
  flex-shrink: 0;
}

.yaml-toggle input {
  opacity: 0;
  width: 0;
  height: 0;
}

.yaml-toggle-slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: var(--bg-tertiary);
  border: 1px solid var(--border);
  transition: 0.2s;
  border-radius: 24px;
}

.yaml-toggle-slider:before {
  position: absolute;
  content: "";
  height: 18px;
  width: 18px;
  left: 2px;
  bottom: 2px;
  background-color: var(--text-muted);
  transition: 0.2s;
  border-radius: 50%;
}

.yaml-toggle input:checked + .yaml-toggle-slider {
  background-color: var(--accent);
  border-color: var(--accent);
}

.yaml-toggle input:checked + .yaml-toggle-slider:before {
  transform: translateX(20px);
  background-color: white;
}

/* Dropdown Select */
.yaml-select {
  width: 100%;
  padding: 10px 14px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 8px;
  color: var(--text-primary);
  font-size: 13px;
  cursor: pointer;
}

.yaml-select:focus {
  outline: none;
  border-color: var(--accent);
}

/* Color Picker */
.yaml-color-container {
  display: flex;
  align-items: center;
  gap: 10px;
}

.yaml-color-picker {
  width: 40px;
  height: 30px;
  padding: 0;
  border: 1px solid var(--border);
  border-radius: 6px;
  cursor: pointer;
  background: transparent;
}

.yaml-color-picker::-webkit-color-swatch-wrapper {
  padding: 2px;
}

.yaml-color-picker::-webkit-color-swatch {
  border-radius: 4px;
  border: none;
}

.yaml-color-value {
  font-size: 12px;
  font-family: 'SF Mono', Monaco, monospace;
  color: var(--text-muted);
}

/* File Selector */
.yaml-file-select {
  display: flex;
  align-items: center;
  gap: 8px;
  width: 100%;
}

.yaml-file-select select {
  flex: 1;
}

/* Readonly */
.yaml-readonly {
  font-size: 13px;
  color: var(--text-muted);
  font-family: 'SF Mono', Monaco, monospace;
}

/* Nested Groups */
.yaml-nested-group {
  background: var(--bg-tertiary);
  border-radius: 10px;
  padding: 14px;
  margin-top: 8px;
}

.yaml-nested-title {
  font-size: 12px;
  font-weight: 600;
  color: var(--text-muted);
  margin-bottom: 10px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

/* Section Description */
.yaml-section-desc {
  font-size: 12px;
  color: var(--text-muted);
  margin-bottom: 12px;
  padding-bottom: 12px;
  border-bottom: 1px solid var(--border);
}

/* Save indicator */
.save-indicator {
  margin-left: 8px;
  font-size: 14px;
  animation: fadeInOut 1.5s ease;
}

.save-indicator.success { color: var(--success); }
.save-indicator.error { color: var(--danger); }

@keyframes fadeInOut {
  0% { opacity: 0; }
  20% { opacity: 1; }
  80% { opacity: 1; }
  100% { opacity: 0; }
}

/* Scene selector header */
.scene-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 20px;
  padding: 16px;
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: 12px;
}

.scene-selector {
  display: flex;
  align-items: center;
  gap: 12px;
}

.scene-selector label {
  font-size: 14px;
  color: var(--text-secondary);
}

.scene-selector select {
  padding: 10px 16px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 8px;
  color: var(--text-primary);
  font-size: 14px;
  min-width: 200px;
}

.scene-selector select:focus {
  outline: none;
  border-color: var(--accent);
}

.scene-actions {
  display: flex;
  gap: 10px;
}

.scene-actions button {
  padding: 10px 18px;
  border: none;
  border-radius: 8px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-apply {
  background: var(--accent);
  color: var(--bg-primary);
}

.btn-apply:hover {
  background: var(--accent-hover);
  box-shadow: 0 0 15px var(--accent-glow);
}

.btn-save {
  background: var(--success);
  color: white;
}

.btn-reset {
  background: var(--bg-tertiary);
  border: 1px solid var(--border) !important;
  color: var(--text-secondary);
}

.btn-reset:hover {
  border-color: var(--accent) !important;
}

/* Loading state */
.loading-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0,0,0,0.8);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
}

.loading-spinner {
  width: 40px;
  height: 40px;
  border: 3px solid var(--border);
  border-top-color: var(--accent);
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

/* ============================================
   Mic Reactivity Styles
   ============================================ */
.mic-btn {
  width: 28px;
  height: 28px;
  border: none;
  border-radius: 6px;
  background: var(--bg-tertiary);
  color: var(--text-muted);
  cursor: pointer;
  font-size: 14px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s;
  flex-shrink: 0;
  margin-left: 6px;
}
.mic-btn:hover {
  background: var(--accent);
  color: white;
}
.mic-btn.active {
  background: #ff9500;
  color: white;
}
.mic-modal-overlay {
  position: fixed;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(0,0,0,0.7);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 2000;
}
.mic-modal {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: 16px;
  padding: 24px;
  width: 90%;
  max-width: 400px;
  box-shadow: 0 20px 60px rgba(0,0,0,0.5);
}
.mic-modal h3 {
  margin: 0 0 8px 0;
  color: var(--text-primary);
  font-size: 18px;
}
.mic-modal-subtitle {
  color: var(--text-muted);
  font-size: 12px;
  margin-bottom: 20px;
}
.mic-param-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16px;
}
.mic-param-row label {
  color: var(--text-secondary);
  font-size: 13px;
  flex: 0 0 45%;
}
.mic-param-row input {
  flex: 0 0 50%;
  padding: 10px 12px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 8px;
  color: var(--text-primary);
  font-size: 13px;
}
.mic-formula {
  background: var(--bg-tertiary);
  border-radius: 8px;
  padding: 12px;
  margin: 16px 0;
  text-align: center;
  font-family: 'SF Mono', Monaco, monospace;
  font-size: 13px;
  color: var(--accent);
}
.mic-enable-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 0;
  border-top: 1px solid var(--border);
  margin-top: 12px;
}
.mic-enable-row label {
  color: var(--text-primary);
  font-weight: 600;
}
.mic-preview {
  background: var(--bg-tertiary);
  border-radius: 8px;
  padding: 12px;
  margin: 12px 0;
  text-align: center;
}
.mic-preview-label {
  font-size: 11px;
  color: var(--text-muted);
  margin-bottom: 4px;
}
.mic-preview-value {
  font-size: 24px;
  font-weight: 600;
  color: var(--accent);
  font-family: 'SF Mono', Monaco, monospace;
}
.mic-modal-actions {
  display: flex;
  gap: 10px;
  margin-top: 20px;
}
.mic-modal-actions button {
  flex: 1;
  padding: 12px;
  border: none;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
}
.mic-modal-actions .btn-cancel {
  background: var(--bg-tertiary);
  color: var(--text-secondary);
}
.mic-modal-actions .btn-save {
  background: var(--accent);
  color: white;
}

/* Responsive */
@media (max-width: 600px) {
  .yaml-field-row {
    flex-direction: column;
    align-items: flex-start;
    gap: 8px;
  }
  
  .yaml-field-label,
  .yaml-field-control {
    flex: 1;
    width: 100%;
  }
  
  .scene-header {
    flex-direction: column;
    gap: 12px;
  }
  
  .scene-selector {
    width: 100%;
  }
  
  .scene-selector select {
    flex: 1;
  }
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
      <a href="/advanced/scenes" style="display: inline-flex; align-items: center; gap: 6px; color: var(--text-muted); text-decoration: none; font-size: 0.85rem; margin-bottom: 12px;">&#x2190; Back to Scenes</a>
      
      <!-- Scene Selection Header -->
      <div class="scene-header">
        <div class="scene-selector">
          <label for="scene-select">Active Scene:</label>
          <select id="scene-select">
            <option value="1">Loading...</option>
          </select>
        </div>
        <div class="scene-actions">
          <button class="btn-reset" id="btn-reset">Reset</button>
          <button class="btn-apply" id="btn-apply">Apply Scene</button>
        </div>
      </div>
      
      <!-- Dynamic Form Container -->
      <div id="config-form-container">
        <!-- Form will be generated here from YAML -->
        <div class="loading-overlay" id="loading">
          <div class="loading-spinner"></div>
        </div>
      </div>
      
      <!-- Mic Reactivity Modal -->
      <div id="mic-modal" class="mic-modal-overlay" style="display:none;">
        <div class="mic-modal">
          <h3>&#x1F3A4; Mic Reactivity</h3>
          <div class="mic-modal-subtitle">Parameter: <span id="mic-modal-param"></span></div>
          <div class="mic-formula">Result = (Y × (Mic - X)) + Z</div>
          <div class="mic-param-row">
            <label>X (Threshold)</label>
            <input type="number" id="mic-x" value="0" step="0.1">
          </div>
          <div class="mic-param-row">
            <label>Y (Multiplier)</label>
            <input type="number" id="mic-y" value="1" step="0.1">
          </div>
          <div class="mic-param-row">
            <label>Z (Offset)</label>
            <input type="number" id="mic-z" value="0" step="0.1">
          </div>
          <div class="mic-preview">
            <div class="mic-preview-label">Mic Level (dB)</div>
            <div class="mic-preview-value" id="mic-raw-val">--</div>
          </div>
          <div class="mic-preview">
            <div class="mic-preview-label">Equation Output: (Y×(Mic-X))+Z</div>
            <div class="mic-preview-value" id="mic-preview-val">--</div>
          </div>
          <div class="mic-enable-row">
            <label>Enable Mic Reactivity</label>
            <label class="yaml-toggle">
              <input type="checkbox" id="mic-enable">
              <span class="yaml-toggle-slider"></span>
            </label>
          </div>
          <div class="mic-modal-actions">
            <button class="btn-cancel" onclick="DisplayConfig.closeMicModal()">Cancel</button>
            <button class="btn-save" onclick="DisplayConfig.saveMicSettings()">Save</button>
          </div>
        </div>
      </div>
    </section>
  </div>

  <script>
/* ============================================
   Display Config Page JavaScript
   ============================================ */

const DisplayConfig = {
  currentSceneId: null,
  sceneData: null,
  requestedSceneId: null,
  animationsList: [],
  
  // Mic reactivity state
  micReactSettings: {},
  currentMicParam: null,
  micPollingInterval: null,
  currentMicDb: -60,
  
  // Initialize
  init: function() {
    // Check for ?id= query parameter
    const urlParams = new URLSearchParams(window.location.search);
    const idParam = urlParams.get('id');
    if (idParam) {
      this.requestedSceneId = parseInt(idParam);
    }
    this.loadAnimations();
    this.loadSceneList();
    this.setupEventHandlers();
  },
  
  // Load available animations from registry
  loadAnimations: function() {
    const self = this;
    fetch('/api/registry/animations')
      .then(r => r.json())
      .then(data => {
        self.animationsList = data.animations || [];
        console.log('[loadAnimations] Loaded:', self.animationsList);
        
        // Re-populate animation dropdown if it exists
        self.populateAnimationDropdown();
      })
      .catch(err => {
        console.error('Failed to load animations:', err);
        // Fallback to built-in types on error
        self.animationsList = [
          { id: 'static_sprite', name: 'Static Sprite' },
          { id: 'static_mirrored', name: 'Static Mirrored' },
          { id: 'reactive_eyes', name: 'Reactive Eyes' }
        ];
        self.populateAnimationDropdown();
      });
  },
  
  // Populate animation type dropdown
  populateAnimationDropdown: function() {
    const select = document.getElementById('animation-type-select');
    if (!select) return;
    // ALWAYS prioritize sceneData over current select value
    const currentValue = this.sceneData?.Display?.animation_type || select.value || 'static_sprite';
    console.log('[populateAnimationDropdown] currentValue=' + currentValue + ', sceneData.Display.animation_type=' + this.sceneData?.Display?.animation_type);
    select.innerHTML = '';
    this.animationsList.forEach(anim => {
      const opt = document.createElement('option');
      opt.value = anim.id;
      opt.textContent = anim.name;
      if (anim.id === currentValue) opt.selected = true;
      select.appendChild(opt);
    });
  },
  
  // Load available scenes
  loadSceneList: function() {
    fetch('/api/scenes')
      .then(r => r.json())
      .then(data => {
        const select = document.getElementById('scene-select');
        select.innerHTML = '';
        
        if (data.scenes && data.scenes.length > 0) {
          let targetSceneId = this.requestedSceneId;
          
          data.scenes.forEach(scene => {
            const opt = document.createElement('option');
            opt.value = scene.id;
            opt.textContent = scene.name || ('Scene ' + scene.id);
            
            // If we have a requested scene ID, select it; otherwise select active
            if (this.requestedSceneId && scene.id == this.requestedSceneId) {
              opt.selected = true;
              this.currentSceneId = scene.id;
            } else if (!this.requestedSceneId && scene.active) {
              opt.selected = true;
              this.currentSceneId = scene.id;
            }
            select.appendChild(opt);
          });
          
          // Load the target scene
          if (this.currentSceneId) {
            this.loadSceneConfig(this.currentSceneId);
          } else {
            this.loadSceneConfig(data.scenes[0].id);
          }
        } else {
          select.innerHTML = '<option value="">No scenes found</option>';
          this.hideLoading();
        }
      })
      .catch(err => {
        console.error('Failed to load scenes:', err);
        this.hideLoading();
      });
  },
  
  // Load scene configuration
  loadSceneConfig: function(sceneId) {
    this.showLoading();
    this.currentSceneId = sceneId;
    
    fetch('/api/scene/config?id=' + sceneId)
      .then(r => r.json())
      .then(data => {
        if (data.success && data.config) {
          this.sceneData = data.config;
          // Load mic reactivity settings from server
          if (data.config.MicReact) {
            this.micReactSettings = data.config.MicReact;
            console.log('[loadSceneConfig] Loaded MicReact settings:', this.micReactSettings);
          } else {
            this.micReactSettings = {};
          }
          console.log('[loadSceneConfig] Loaded animation_type=' + data.config?.Display?.animation_type);
          this.renderConfigForm(data.config);
          // Re-populate dropdown after form is rendered to ensure correct selection
          this.populateAnimationDropdown();
        } else {
          console.error('Failed to load config:', data.error);
        }
        this.hideLoading();
      })
      .catch(err => {
        console.error('Failed to load scene config:', err);
        this.hideLoading();
      });
  },
  
  // Render configuration form from YAML data
  renderConfigForm: function(config) {
    const container = document.getElementById('config-form-container');
    let html = `<form class="yaml-form" id="scene-form" data-scene="${this.currentSceneId}">`;
    
    // Process each top-level section
    const sections = ['Global', 'Display'];
    
    for (const sectionKey of sections) {
      if (config[sectionKey]) {
        html += this.renderSection(sectionKey, config[sectionKey]);
      }
    }
    
    // Add animation-specific parameters section based on current animation type
    const animType = config.Display?.animation_type || 'static_mirrored';
    console.log('[renderConfigForm] animType=' + animType + ', animationFields keys=' + Object.keys(this.animationFields).join(','));
    console.log('[renderConfigForm] fields for animType=' + JSON.stringify(this.animationFields[animType]));
    
    // Add shader settings section (between Display and Animation Settings)
    html += this.renderShaderSection();
    
    html += this.renderAnimParamsSection(animType, config.Display);
    
    // Add remaining sections (Audio only - LEDS removed)
    const otherSections = ['Audio'];
    for (const sectionKey of otherSections) {
      if (config[sectionKey]) {
        html += this.renderSection(sectionKey, config[sectionKey]);
      }
    }
    
    html += '</form>';
    container.innerHTML = html;
    
    // Load sprites for file selectors
    this.loadSpritesForSelectors();
    
    // Setup form handlers
    this.setupFormHandlers();
  },
  
  // Load sprites for file selectors
  loadSpritesForSelectors: function() {
    fetch('/api/sprites')
      .then(r => r.json())
      .then(data => {
        const sprites = data.sprites || [];
        document.querySelectorAll('.yaml-select[data-file-type="sprite"]').forEach(select => {
          const currentValue = parseInt(select.value);
          
          // Keep the "None" option if it exists
          const noneOption = select.querySelector('option[value="-1"]');
          select.innerHTML = '';
          if (noneOption) {
            select.appendChild(noneOption);
          } else {
            const opt = document.createElement('option');
            opt.value = '-1';
            opt.textContent = 'Default Vector Shape';
            select.appendChild(opt);
          }
          
          // Add sprites
          sprites.forEach(sprite => {
            const opt = document.createElement('option');
            opt.value = sprite.id;
            opt.textContent = sprite.name || ('Sprite ' + sprite.id);
            if (sprite.id === currentValue) opt.selected = true;
            select.appendChild(opt);
          });
          
          // Ensure current value is selected
          if (currentValue >= 0 && !select.querySelector(`option[value="${currentValue}"]`)) {
            const opt = document.createElement('option');
            opt.value = currentValue;
            opt.textContent = 'Unknown Sprite ' + currentValue;
            opt.selected = true;
            select.appendChild(opt);
          }
        });
      })
      .catch(err => console.error('Failed to load sprites:', err));
  },
  
  // Animation-specific field definitions with smart control types
  // Control type is auto-determined by min/max/step:
  //   - min=0, max=1, step=1 → toggle button
  //   - any other range → slider
  // Each param can also specify 'typed: true' for manual input
  animationFields: {
    'static': {
      // Single static sprite - no mirroring
      position_x: { label: 'Position X', min: 0, max: 128, step: 1, unit: 'px', param: 'x' },
      position_y: { label: 'Position Y', min: 0, max: 32, step: 1, unit: 'px', param: 'y' },
      rotation: { label: 'Rotation', min: 0, max: 360, step: 1, unit: '°', param: 'rotation' },
      scale: { label: 'Scale', min: 0.1, max: 4.0, step: 0.1, param: 'scale' },
      flip_x: { label: 'Flip Horizontal', min: 0, max: 1, step: 1, param: 'flip_x' }
    },
    'static_sprite': {
      // Single static sprite - no mirroring
      position_x: { label: 'Position X', min: 0, max: 128, step: 1, unit: 'px', param: 'x' },
      position_y: { label: 'Position Y', min: 0, max: 32, step: 1, unit: 'px', param: 'y' },
      rotation: { label: 'Rotation', min: 0, max: 360, step: 1, unit: '°', param: 'rotation' },
      scale: { label: 'Scale', min: 0.1, max: 4.0, step: 0.1, param: 'scale' },
      flip_x: { label: 'Flip Horizontal', min: 0, max: 1, step: 1, param: 'flip_x' }
    },
    'static_mirrored': {
      // Mirrored display - separate controls for left and right panels
      left_x: { label: 'Left X', min: 0, max: 64, step: 1, unit: 'px', param: 'left_x' },
      left_y: { label: 'Left Y', min: 0, max: 32, step: 1, unit: 'px', param: 'left_y' },
      left_rotation: { label: 'Left Rotation', min: 0, max: 360, step: 1, unit: '°', param: 'left_rotation' },
      left_scale: { label: 'Left Scale', min: 0.1, max: 4.0, step: 0.1, param: 'left_scale' },
      left_flip_x: { label: 'Left Flip', min: 0, max: 1, step: 1, param: 'left_flip_x' },
      right_x: { label: 'Right X', min: 64, max: 128, step: 1, unit: 'px', param: 'right_x' },
      right_y: { label: 'Right Y', min: 0, max: 32, step: 1, unit: 'px', param: 'right_y' },
      right_rotation: { label: 'Right Rotation', min: 0, max: 360, step: 1, unit: '°', param: 'right_rotation' },
      right_scale: { label: 'Right Scale', min: 0.1, max: 4.0, step: 0.1, param: 'right_scale' },
      right_flip_x: { label: 'Right Flip', min: 0, max: 1, step: 1, param: 'right_flip_x' }
    },
    'reactive_eyes': {
      // IMU-reactive eyes with asymmetric movement
      reactive_y_sensitivity: { label: 'Y Sensitivity', typed: true, step: 0.1, param: 'reactive_y_sensitivity', desc: '+Y: Right up, Left down' },
      reactive_z_sensitivity: { label: 'Z Sensitivity', typed: true, step: 0.1, param: 'reactive_z_sensitivity', desc: '+Z: Right forward, Left backward' },
      reactive_x_sensitivity: { label: 'X Sensitivity', typed: true, step: 0.1, param: 'reactive_x_sensitivity', desc: '+X: Both down' },
      reactive_rot_sensitivity: { label: 'Rotation Sensitivity', typed: true, step: 0.1, param: 'reactive_rot_sensitivity', desc: 'Both rotate same direction' },
      reactive_smoothing: { label: 'Smoothing', min: 0.01, max: 1.0, step: 0.01, param: 'reactive_smoothing', desc: 'Lower = smoother' },
      reactive_base_left_x: { label: 'Left Base X', min: 0, max: 64, step: 1, unit: 'px', param: 'reactive_base_left_x' },
      reactive_base_left_y: { label: 'Left Base Y', min: 0, max: 32, step: 1, unit: 'px', param: 'reactive_base_left_y' },
      reactive_left_rot_offset: { label: 'Left Rotation Offset', typed: true, step: 1, unit: '°', param: 'reactive_left_rot_offset', desc: 'Base rotation offset (0 = default)' },
      reactive_left_flip_x: { label: 'Left Flip', min: 0, max: 1, step: 1, param: 'reactive_left_flip_x' },
      reactive_base_right_x: { label: 'Right Base X', min: 64, max: 128, step: 1, unit: 'px', param: 'reactive_base_right_x' },
      reactive_base_right_y: { label: 'Right Base Y', min: 0, max: 32, step: 1, unit: 'px', param: 'reactive_base_right_y' },
      reactive_right_rot_offset: { label: 'Right Rotation Offset', typed: true, step: 1, unit: '°', param: 'reactive_right_rot_offset', desc: 'Base rotation offset (0 = 180° default)' },
      reactive_right_flip_x: { label: 'Right Flip', min: 0, max: 1, step: 1, param: 'reactive_right_flip_x' },
      reactive_scale: { label: 'Scale', min: 0.1, max: 4.0, step: 0.1, param: 'reactive_scale' }
    }
  },
  
  // Determine control type based on min/max/step
  getControlType: function(schema) {
    // If min=0, max=1, step=1 → toggle
    if (schema.min === 0 && schema.max === 1 && schema.step === 1) {
      return 'toggle';
    }
    // If typed flag is set → number input
    if (schema.typed) {
      return 'number';
    }
    // Otherwise → slider
    return 'slider';
  },
  
  // Get current values from sceneData for animation fields
  getAnimFieldValue: function(param, animType) {
    // Try to get from Display section params
    const display = this.sceneData?.Display || {};
    
    // Check direct properties first
    if (param === 'center_x' && display.position?.x !== undefined) return display.position.x;
    if (param === 'center_y' && display.position?.y !== undefined) return display.position.y;
    if (param === 'rotation' && display.rotation !== undefined) return display.rotation;
    if (param === 'intensity' && display.sensitivity !== undefined) return display.sensitivity;
    if (param === 'mirror' && display.mirror !== undefined) return display.mirror ? 1 : 0;
    
    // Check params map
    if (display.params && display.params[param] !== undefined) {
      return display.params[param];
    }
    
    // Check mirrorSprite flag for legacy support
    if (param === 'mirror' && display.mirrorSprite !== undefined) {
      return display.mirrorSprite ? 1 : 0;
    }
    
    // Return defaults based on param
    const defaults = {
      'mirror': 0,
      'x': 64, 'y': 16, 'rotation': 0, 'scale': 1.0, 'flip_x': 0,
      'left_x': 32, 'left_y': 16, 'left_rotation': 0, 'left_scale': 1.0, 'left_flip_x': 0,
      'right_x': 96, 'right_y': 16, 'right_rotation': 180, 'right_scale': 1.0, 'right_flip_x': 0
    };
    return defaults[param] ?? 0;
  },
  
  // Shader type definitions
  shaderTypes: [
    { id: 'none', name: 'None', value: 0 },
    { id: 'color_override', name: 'Color Override', value: 1 },
    { id: 'hue_cycle', name: 'RGB Hue Cycle', value: 2 },
    { id: 'gradient_cycle', name: 'Gradient Cycle', value: 3 },
    { id: 'glitch', name: 'Glitch', value: 4 }
  ],
  
  // Get shader field value from sceneData
  getShaderFieldValue: function(param) {
    const shader = this.sceneData?.Shader || {};
    const defaults = {
      'type': 0,
      'invert': 0,
      'mask_enabled': 1,
      'mask_r': 0,
      'mask_g': 0,
      'mask_b': 0,
      'override_r': 255,
      'override_g': 255,
      'override_b': 255,
      // Hue/Gradient cycle defaults
      'hue_speed': 1000,
      'hue_color_count': 5,
      'hue_color_0_r': 255, 'hue_color_0_g': 0,   'hue_color_0_b': 0,    // Red
      'hue_color_1_r': 255, 'hue_color_1_g': 255, 'hue_color_1_b': 0,    // Yellow
      'hue_color_2_r': 0,   'hue_color_2_g': 255, 'hue_color_2_b': 0,    // Green
      'hue_color_3_r': 0,   'hue_color_3_g': 0,   'hue_color_3_b': 255,  // Blue
      'hue_color_4_r': 128, 'hue_color_4_g': 0,   'hue_color_4_b': 255,  // Purple
      'hue_color_5_r': 255, 'hue_color_5_g': 0,   'hue_color_5_b': 128,  // Pink
      'hue_color_6_r': 0,   'hue_color_6_g': 255, 'hue_color_6_b': 255,  // Cyan
      'hue_color_7_r': 255, 'hue_color_7_g': 128, 'hue_color_7_b': 0,    // Orange
      // Gradient cycle specific
      'gradient_distance': 20,
      'gradient_angle': 0,
      'gradient_mirror': 0,
      // Glitch shader specific
      'glitch_speed': 50,
      'glitch_intensity': 30,
      'glitch_chromatic': 20
    };
    
    // For colors beyond 7, default to red
    if (param.startsWith('hue_color_') && shader[param] === undefined && defaults[param] === undefined) {
      if (param.endsWith('_r')) return 255;
      return 0;
    }
    
    if (shader[param] !== undefined) return shader[param];
    return defaults[param] ?? 0;
  },
  
  // Render shader settings section
  renderShaderSection: function() {
    let html = `<div class="yaml-section" id="shader-section">`;
    html += `<div class="yaml-section-header" onclick="YamlUI.toggleSection(this.parentElement)">`;
    html += `<div class="yaml-section-title">`;
    html += `<span class="yaml-section-icon">◆</span>`;
    html += `<span>Shader Settings</span>`;
    html += `</div>`;
    html += `<span class="yaml-section-chevron">▼</span>`;
    html += `</div>`;
    html += `<div class="yaml-section-body" id="shader-section-body">`;
    html += `<div class="yaml-section-desc">GPU-based pixel effects applied during rendering</div>`;
    
    // Shader Type dropdown
    const shaderType = this.getShaderFieldValue('type');
    html += `<div class="yaml-field-row" data-field-path="Shader.type">`;
    html += `<label class="yaml-field-label">Shader Effect</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<select class="yaml-select" id="shader-type-select" data-path="Shader.type">`;
    this.shaderTypes.forEach(st => {
      const selected = st.value === shaderType ? 'selected' : '';
      html += `<option value="${st.value}" ${selected}>${st.name}</option>`;
    });
    html += `</select></div></div>`;
    
    // Invert Colors toggle
    const invert = this.getShaderFieldValue('invert');
    html += `<div class="yaml-field-row" data-field-path="Shader.invert">`;
    html += `<label class="yaml-field-label">Invert Colors</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<label class="yaml-toggle">`;
    html += `<input type="checkbox" data-path="Shader.invert" ${invert ? 'checked' : ''}>`;
    html += `<span class="yaml-toggle-slider"></span>`;
    html += `</label></div></div>`;
    
    // Mask Enabled toggle
    const maskEnabled = this.getShaderFieldValue('mask_enabled');
    html += `<div class="yaml-field-row" data-field-path="Shader.mask_enabled">`;
    html += `<label class="yaml-field-label">Mask Transparency</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<label class="yaml-toggle">`;
    html += `<input type="checkbox" data-path="Shader.mask_enabled" ${maskEnabled ? 'checked' : ''}>`;
    html += `<span class="yaml-toggle-slider"></span>`;
    html += `</label></div></div>`;
    
    // Mask Color picker
    const maskR = this.getShaderFieldValue('mask_r');
    const maskG = this.getShaderFieldValue('mask_g');
    const maskB = this.getShaderFieldValue('mask_b');
    const maskHex = '#' + [maskR, maskG, maskB].map(c => c.toString(16).padStart(2, '0')).join('');
    html += `<div class="yaml-field-row" data-field-path="Shader.mask_color">`;
    html += `<label class="yaml-field-label">Mask Color</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<div class="yaml-color-container">`;
    html += `<input type="color" class="yaml-color-picker" value="${maskHex}" data-path="Shader.mask_color">`;
    html += `<span class="yaml-color-value" id="val-Shader-mask_color">${maskHex}</span>`;
    html += `</div></div></div>`;
    
    // Color Override parameters (only shown when shader type is color_override)
    html += `<div id="shader-color-override-params" style="display: ${shaderType === 1 ? 'block' : 'none'};">`;
    const overrideR = this.getShaderFieldValue('override_r');
    const overrideG = this.getShaderFieldValue('override_g');
    const overrideB = this.getShaderFieldValue('override_b');
    const overrideHex = '#' + [overrideR, overrideG, overrideB].map(c => c.toString(16).padStart(2, '0')).join('');
    html += `<div class="yaml-field-row" data-field-path="Shader.override_color">`;
    html += `<label class="yaml-field-label">Override Color</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<div class="yaml-color-container">`;
    html += `<input type="color" class="yaml-color-picker" value="${overrideHex}" data-path="Shader.override_color">`;
    html += `<span class="yaml-color-value" id="val-Shader-override_color">${overrideHex}</span>`;
    html += `</div></div></div>`;
    html += `</div>`; // close shader-color-override-params
    
    // Hue Cycle parameters (only shown when shader type is hue_cycle)
    html += `<div id="shader-hue-cycle-params" style="display: ${shaderType === 2 ? 'block' : 'none'};">`;
    
    // Speed slider
    const hueSpeed = this.getShaderFieldValue('hue_speed');
    html += `<div class="yaml-field-row" data-field-path="Shader.hue_speed">`;
    html += `<label class="yaml-field-label">Cycle Speed (ms)</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="range" class="yaml-range" min="100" max="5000" step="50" value="${hueSpeed}" data-path="Shader.hue_speed" oninput="document.getElementById('val-Shader-hue_speed').textContent = this.value + 'ms'">`;
    html += `<span class="yaml-range-value" id="val-Shader-hue_speed">${hueSpeed}ms</span>`;
    html += `</div></div>`;
    
    // Color count number input
    const colorCount = this.getShaderFieldValue('hue_color_count');
    html += `<div class="yaml-field-row" data-field-path="Shader.hue_color_count">`;
    html += `<label class="yaml-field-label">Number of Colors</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="number" class="yaml-input-number" min="1" max="32" step="1" value="${colorCount}" data-path="Shader.hue_color_count" id="hue-color-count-input" onchange="DisplayConfig.updateHuePaletteColors(parseInt(this.value))">`;
    html += `</div></div>`;
    
    // Dynamic color pickers container
    html += `<div id="hue-palette-colors">`;
    for (let i = 0; i < colorCount; i++) {
      const r = this.getShaderFieldValue('hue_color_' + i + '_r');
      const g = this.getShaderFieldValue('hue_color_' + i + '_g');
      const b = this.getShaderFieldValue('hue_color_' + i + '_b');
      const hex = '#' + [r, g, b].map(c => c.toString(16).padStart(2, '0')).join('');
      html += `<div class="yaml-field-row" data-field-path="Shader.hue_color_${i}" id="hue-color-row-${i}">`;
      html += `<label class="yaml-field-label">Color ${i + 1}</label>`;
      html += `<div class="yaml-field-control">`;
      html += `<div class="yaml-color-container">`;
      html += `<input type="color" class="yaml-color-picker" value="${hex}" data-path="Shader.hue_color_${i}" data-color-index="${i}">`;
      html += `<span class="yaml-color-value" id="val-Shader-hue_color_${i}">${hex}</span>`;
      html += `</div></div></div>`;
    }
    html += `</div>`; // close hue-palette-colors
    html += `</div>`; // close shader-hue-cycle-params
    
    // Gradient Cycle parameters (only shown when shader type is gradient_cycle)
    html += `<div id="shader-gradient-cycle-params" style="display: ${shaderType === 3 ? 'block' : 'none'};">`;
    
    // Speed slider (shared with hue cycle concept)
    const gradSpeed = this.getShaderFieldValue('hue_speed');
    html += `<div class="yaml-field-row" data-field-path="Shader.hue_speed">`;
    html += `<label class="yaml-field-label">Scroll Speed (ms)</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="range" class="yaml-range" min="100" max="5000" step="50" value="${gradSpeed}" data-path="Shader.hue_speed" id="gradient-speed-slider" oninput="document.getElementById('val-gradient-speed').textContent = this.value + 'ms'">`;
    html += `<span class="yaml-range-value" id="val-gradient-speed">${gradSpeed}ms</span>`;
    html += `</div></div>`;
    
    // Distance slider
    const distance = this.getShaderFieldValue('gradient_distance');
    html += `<div class="yaml-field-row" data-field-path="Shader.gradient_distance">`;
    html += `<label class="yaml-field-label">Color Distance (px)</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="range" class="yaml-range" min="5" max="128" step="1" value="${distance}" data-path="Shader.gradient_distance" oninput="document.getElementById('val-Shader-gradient_distance').textContent = this.value + 'px'">`;
    html += `<span class="yaml-range-value" id="val-Shader-gradient_distance">${distance}px</span>`;
    html += `</div></div>`;
    
    // Angle slider (-180 to 180)
    const angle = this.getShaderFieldValue('gradient_angle');
    html += `<div class="yaml-field-row" data-field-path="Shader.gradient_angle">`;
    html += `<label class="yaml-field-label">Travel Angle (°)</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="range" class="yaml-range" min="-180" max="180" step="1" value="${angle}" data-path="Shader.gradient_angle" oninput="document.getElementById('val-Shader-gradient_angle').textContent = this.value + '°'">`;
    html += `<span class="yaml-range-value" id="val-Shader-gradient_angle">${angle}°</span>`;
    html += `</div></div>`;
    
    // Mirror gradient toggle
    const gradientMirror = this.getShaderFieldValue('gradient_mirror') || 0;
    html += `<div class="yaml-field-row" data-field-path="Shader.gradient_mirror">`;
    html += `<label class="yaml-field-label">Mirror Gradient</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<label class="yaml-toggle">`;
    html += `<input type="checkbox" data-path="Shader.gradient_mirror" ${gradientMirror ? 'checked' : ''}>`;
    html += `<span class="yaml-toggle-slider"></span>`;
    html += `</label></div></div>`;
    
    // Color count number input (shared with hue cycle)
    const gradColorCount = this.getShaderFieldValue('hue_color_count');
    html += `<div class="yaml-field-row" data-field-path="Shader.hue_color_count">`;
    html += `<label class="yaml-field-label">Number of Colors</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="number" class="yaml-input-number" min="1" max="32" step="1" value="${gradColorCount}" data-path="Shader.hue_color_count" id="gradient-color-count-input" onchange="DisplayConfig.updateGradientPaletteColors(parseInt(this.value))">`;
    html += `</div></div>`;
    
    // Dynamic color pickers container for gradient
    html += `<div id="gradient-palette-colors">`;
    for (let i = 0; i < gradColorCount; i++) {
      const r = this.getShaderFieldValue('hue_color_' + i + '_r');
      const g = this.getShaderFieldValue('hue_color_' + i + '_g');
      const b = this.getShaderFieldValue('hue_color_' + i + '_b');
      const hex = '#' + [r, g, b].map(c => c.toString(16).padStart(2, '0')).join('');
      html += `<div class="yaml-field-row" data-field-path="Shader.hue_color_${i}" id="gradient-color-row-${i}">`;
      html += `<label class="yaml-field-label">Color ${i + 1}</label>`;
      html += `<div class="yaml-field-control">`;
      html += `<div class="yaml-color-container">`;
      html += `<input type="color" class="yaml-color-picker" value="${hex}" data-path="Shader.hue_color_${i}" data-color-index="${i}">`;
      html += `<span class="yaml-color-value" id="val-gradient-color_${i}">${hex}</span>`;
      html += `</div></div></div>`;
    }
    html += `</div>`; // close gradient-palette-colors
    html += `</div>`; // close shader-gradient-cycle-params
    
    // Glitch shader parameters (only shown when shader type is glitch)
    html += `<div id="shader-glitch-params" style="display: ${shaderType === 4 ? 'block' : 'none'};">`;
    
    // Glitch speed slider
    const glitchSpeed = this.getShaderFieldValue('glitch_speed');
    html += `<div class="yaml-field-row" data-field-path="Shader.glitch_speed">`;
    html += `<label class="yaml-field-label">Glitch Speed</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="range" class="yaml-range" min="0" max="100" step="1" value="${glitchSpeed}" data-path="Shader.glitch_speed" oninput="document.getElementById('val-Shader-glitch_speed').textContent = this.value">`;
    html += `<span class="yaml-range-value" id="val-Shader-glitch_speed">${glitchSpeed}</span>`;
    html += `</div></div>`;
    
    // Glitch intensity slider
    const glitchIntensity = this.getShaderFieldValue('glitch_intensity');
    html += `<div class="yaml-field-row" data-field-path="Shader.glitch_intensity">`;
    html += `<label class="yaml-field-label">Displacement Intensity</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="range" class="yaml-range" min="0" max="100" step="1" value="${glitchIntensity}" data-path="Shader.glitch_intensity" oninput="document.getElementById('val-Shader-glitch_intensity').textContent = this.value">`;
    html += `<span class="yaml-range-value" id="val-Shader-glitch_intensity">${glitchIntensity}</span>`;
    html += `</div></div>`;
    
    // Chromatic aberration slider
    const glitchChromatic = this.getShaderFieldValue('glitch_chromatic');
    html += `<div class="yaml-field-row" data-field-path="Shader.glitch_chromatic">`;
    html += `<label class="yaml-field-label">Chromatic Aberration</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="range" class="yaml-range" min="0" max="100" step="1" value="${glitchChromatic}" data-path="Shader.glitch_chromatic" oninput="document.getElementById('val-Shader-glitch_chromatic').textContent = this.value">`;
    html += `<span class="yaml-range-value" id="val-Shader-glitch_chromatic">${glitchChromatic}</span>`;
    html += `</div></div>`;
    
    // Glitch quantity slider
    const glitchQuantity = this.getShaderFieldValue('glitch_quantity');
    html += `<div class="yaml-field-row" data-field-path="Shader.glitch_quantity">`;
    html += `<label class="yaml-field-label">Glitch Quantity</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<input type="range" class="yaml-range" min="0" max="100" step="1" value="${glitchQuantity}" data-path="Shader.glitch_quantity" oninput="document.getElementById('val-Shader-glitch_quantity').textContent = this.value">`;
    html += `<span class="yaml-range-value" id="val-Shader-glitch_quantity">${glitchQuantity}</span>`;
    html += `</div></div>`;
    
    html += `</div>`; // close shader-glitch-params
    
    html += `</div></div>`; // close body and section
    return html;
  },
  
  // Render animation parameters section based on selected animation type
  renderAnimParamsSection: function(animType, displayConfig) {
    const fields = this.animationFields[animType] || this.animationFields['static'] || {};
    
    let html = `<div class="yaml-section" id="anim-params-section">`;
    html += `<div class="yaml-section-header">`;
    html += `<div class="yaml-section-title">`;
    html += `<span class="yaml-section-icon">◑</span>`;
    html += `<span>Animation Settings</span>`;
    html += `</div>`;
    html += `<span class="yaml-section-chevron">▼</span>`;
    html += `</div>`;
    html += `<div class="yaml-section-body" id="anim-params-body">`;
    
    // Animation type dropdown
    html += `<div class="yaml-field-row" data-field-path="Display.animation_type">`;
    html += `<label class="yaml-field-label">Animation Type</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<select class="yaml-select" id="animation-type-select" data-path="Display.animation_type">`;
    // Options will be populated dynamically from animationsList
    this.animationsList.forEach(anim => {
      const selected = anim.id === animType ? 'selected' : '';
      html += `<option value="${anim.id}" ${selected}>${anim.name}</option>`;
    });
    html += `</select></div></div>`;
    
    if (Object.keys(fields).length === 0) {
      html += `<div class="yaml-section-desc">No adjustable settings for this animation type.</div>`;
    } else {
      for (const [key, fieldSchema] of Object.entries(fields)) {
        const paramName = fieldSchema.param || key;
        const value = this.getAnimFieldValue(paramName, animType);
        const path = 'AnimParams.' + paramName;
        
        // Determine control type from schema (smart rendering)
        const controlType = this.getControlType(fieldSchema);
        const schemaWithType = { ...fieldSchema, type: controlType };
        
        html += this.renderSmartField(path, fieldSchema.label || key, value, schemaWithType);
      }
    }
    
    html += `</div></div>`;
    return html;
  },
  
  // Update animation params section when animation type changes
  updateAnimParamsSection: function(animType) {
    const body = document.getElementById('anim-params-body');
    if (!body) return;
    
    const fields = this.animationFields[animType] || this.animationFields['static'] || {};
    
    // Start with the animation type dropdown
    let html = `<div class="yaml-field-row" data-field-path="Display.animation_type">`;
    html += `<label class="yaml-field-label">Animation Type</label>`;
    html += `<div class="yaml-field-control">`;
    html += `<select class="yaml-select" id="animation-type-select" data-path="Display.animation_type">`;
    this.animationsList.forEach(anim => {
      const selected = anim.id === animType ? 'selected' : '';
      html += `<option value="${anim.id}" ${selected}>${anim.name}</option>`;
    });
    html += `</select></div></div>`;
    
    if (Object.keys(fields).length === 0) {
      html += `<div class="yaml-section-desc">No adjustable settings for this animation type.</div>`;
    } else {
      for (const [key, fieldSchema] of Object.entries(fields)) {
        const paramName = fieldSchema.param || key;
        const value = this.getAnimFieldValue(paramName, animType);
        const path = 'AnimParams.' + paramName;
        
        // Determine control type from schema (smart rendering)
        const controlType = this.getControlType(fieldSchema);
        const schemaWithType = { ...fieldSchema, type: controlType };
        
        html += this.renderSmartField(path, fieldSchema.label || key, value, schemaWithType);
      }
    }
    
    body.innerHTML = html;
    
    // Re-attach handlers for new fields
    this.setupFormHandlers();
  },
  
  // Render a smart field based on min/max/step (toggle vs slider)
  renderSmartField: function(path, label, value, schema) {
    const controlType = schema.type || this.getControlType(schema);
    const desc = schema.desc || '';
    const unit = schema.unit || '';
    const paramKey = path.split('.').pop();
    const micActive = this.micReactSettings[paramKey] && this.micReactSettings[paramKey].enabled;
    const micBtnClass = micActive ? 'mic-btn active' : 'mic-btn';
    
    let html = `<div class="yaml-field-row" data-field-path="${path}">`;
    html += `<label class="yaml-field-label">${label}`;
    if (desc) {
      html += `<span class="help-icon" title="${desc}">?</span>`;
    }
    html += `</label>`;
    html += `<div class="yaml-field-control">`;
    
    if (controlType === 'toggle') {
      // Toggle button (boolean 0/1)
      const checked = value ? 'checked' : '';
      html += `<label class="yaml-toggle">
        <input type="checkbox" data-path="${path}" ${checked}>
        <span class="yaml-toggle-slider"></span>
      </label>`;
    } else if (controlType === 'number') {
      // Typed number input with mic button
      const min = schema.min ?? '';
      const max = schema.max ?? '';
      html += `<input type="number" class="yaml-input-number" data-path="${path}"
        value="${value}" ${min !== '' ? 'min="' + min + '"' : ''} ${max !== '' ? 'max="' + max + '"' : ''}>`;
      if (unit) {
        html += `<span class="yaml-input-unit">${unit}</span>`;
      }
      html += `<button type="button" class="${micBtnClass}" data-param="${paramKey}" onclick="event.preventDefault(); DisplayConfig.openMicModal('${paramKey}', '${label.replace(/'/g, "\\'")}'); return false;" title="Mic Reactivity">&#x1F3A4;</button>`;
    } else {
      // Slider (default) with mic button
      const min = schema.min ?? 0;
      const max = schema.max ?? 100;
      const step = schema.step ?? 1;
      const pathId = path.replace(/\./g, '-');
      
      html += `<div class="yaml-slider-container">
        <input type="range" class="yaml-slider" data-path="${path}"
          min="${min}" max="${max}" step="${step}" value="${value}">
        <span class="yaml-slider-value" id="val-${pathId}">${value}${unit ? ' ' + unit : ''}</span>
      </div>`;
      html += `<button type="button" class="${micBtnClass}" data-param="${paramKey}" onclick="event.preventDefault(); DisplayConfig.openMicModal('${paramKey}', '${label.replace(/'/g, "\\'")}'); return false;" title="Mic Reactivity">&#x1F3A4;</button>`;
    }
    
    html += `</div></div>`;
    return html;
  },
  
  // Get default param value
  getDefaultParamValue: function(schema) {
    switch (schema.type) {
      case 'toggle': return false;
      case 'slider': return (schema.min + schema.max) / 2;
      default: return 0;
    }
  },
  
  // Fields that should be in Animation Settings, not Display section
  // These are SKIPPED when rendering the Display section
  animationSpecificFields: ['enabled', 'position', 'rotation', 'sensitivity', 'mirror', 'params', 'animation_type', 'mirrorSprite'],
  
  // Render a section
  renderSection: function(key, section) {
    const schema = this.getSchemaForSection(key);
    const icon = schema.icon || '○';
    const label = schema.label || key;
    const desc = schema.desc || '';
    
    let html = `<div class="yaml-section">`;
    html += `<div class="yaml-section-header">`;
    html += `<div class="yaml-section-title">`;
    html += `<span class="yaml-section-icon">${icon}</span>`;
    html += `<span>${label}</span>`;
    html += `</div>`;
    html += `<span class="yaml-section-chevron">▼</span>`;
    html += `</div>`;
    html += `<div class="yaml-section-body">`;
    
    if (desc) {
      html += `<div class="yaml-section-desc">${desc}</div>`;
    }
    
    // Render fields from the actual data (not hardcoded)
    for (const [fieldKey, fieldValue] of Object.entries(section)) {
      // Skip internal metadata
      if (fieldKey.startsWith('_')) continue;
      
      // For Display section, skip animation-specific fields (they go in Animation Settings)
      if (key === 'Display' && this.animationSpecificFields.includes(fieldKey)) {
        continue;
      }
      
      // Get schema if exists, otherwise infer from data
      const fieldSchema = schema.fields?.[fieldKey] || this.inferFieldSchema(fieldKey, fieldValue);
      
      // Handle nested objects (like position, background)
      if (typeof fieldValue === 'object' && fieldValue !== null && !Array.isArray(fieldValue)) {
        // Check if it's a color (has r, g, b)
        if ('r' in fieldValue && 'g' in fieldValue && 'b' in fieldValue) {
          html += this.renderField(key + '.' + fieldKey, fieldKey, fieldValue, { type: 'color', label: this.formatLabel(fieldKey) });
        } else {
          // Skip nested animation-specific objects in Display (like position)
          if (key === 'Display' && this.animationSpecificFields.includes(fieldKey)) {
            continue;
          }
          // Render as nested group (like position.x, position.y)
          for (const [subKey, subValue] of Object.entries(fieldValue)) {
            const subPath = key + '.' + fieldKey + '.' + subKey;
            const subSchema = this.inferFieldSchema(subKey, subValue);
            subSchema.label = this.formatLabel(fieldKey) + ' ' + subKey.toUpperCase();
            html += this.renderField(subPath, subKey, subValue, subSchema);
          }
        }
      } else {
        html += this.renderField(key + '.' + fieldKey, fieldKey, fieldValue, fieldSchema);
      }
    }
    
    html += `</div></div>`;
    return html;
  },
  
  // Render a field
  renderField: function(path, key, value, schema) {
    const label = schema.label || this.formatLabel(key);
    const desc = schema.desc || '';
    
    let html = `<div class="yaml-field-row" data-field-path="${path}">`;
    html += `<label class="yaml-field-label">${label}`;
    if (desc) {
      html += `<span class="help-icon" title="${desc}">?</span>`;
    }
    html += `</label>`;
    html += `<div class="yaml-field-control">`;
    
    switch (schema.type) {
      case 'toggle':
        html += this.renderToggle(path, value);
        break;
      case 'dropdown':
        html += this.renderDropdown(path, value, schema.options || []);
        break;
      case 'slider':
        html += this.renderSlider(path, value, schema);
        break;
      case 'color':
        html += this.renderColor(path, value);
        break;
      case 'number':
        html += this.renderNumber(path, value, schema);
        break;
      case 'text':
        html += this.renderText(path, value);
        break;
      case 'file':
        html += this.renderFileSelect(path, value, schema);
        break;
      case 'readonly':
        html += `<span class="yaml-readonly">${value}</span>`;
        break;
      case 'group':
        html = this.renderNestedGroup(path, key, value, schema);
        return html;
      default:
        // Auto-detect type
        html += this.renderAutoField(path, key, value);
    }
    
    html += `</div></div>`;
    return html;
  },
  
  // Render toggle
  renderToggle: function(path, value) {
    const checked = value ? 'checked' : '';
    return `<label class="yaml-toggle">
      <input type="checkbox" data-path="${path}" ${checked}>
      <span class="yaml-toggle-slider"></span>
    </label>`;
  },
  
  // Render dropdown
  renderDropdown: function(path, value, options) {
    let html = `<select class="yaml-select" data-path="${path}">`;
    for (const opt of options) {
      const selected = (opt.value === value) ? 'selected' : '';
      html += `<option value="${opt.value}" ${selected}>${opt.label}</option>`;
    }
    html += `</select>`;
    return html;
  },
  
  // Render slider
  renderSlider: function(path, value, schema) {
    const min = schema.min ?? 0;
    const max = schema.max ?? 100;
    const step = schema.step ?? 1;
    const unit = schema.unit || '';
    const pathId = path.replace(/\./g, '-');
    
    return `<div class="yaml-slider-container">
      <input type="range" class="yaml-slider" data-path="${path}"
        min="${min}" max="${max}" step="${step}" value="${value}">
      <span class="yaml-slider-value" id="val-${pathId}">${value}${unit ? ' ' + unit : ''}</span>
    </div>`;
  },
  
  // Render color picker
  renderColor: function(path, value) {
    let hexColor = '#000000';
    if (typeof value === 'object') {
      hexColor = '#' + [value.r || 0, value.g || 0, value.b || 0]
        .map(x => x.toString(16).padStart(2, '0')).join('');
    } else if (typeof value === 'string') {
      hexColor = value;
    }
    const pathId = path.replace(/\./g, '-');
    
    return `<div class="yaml-color-container">
      <input type="color" class="yaml-color-picker" data-path="${path}" value="${hexColor}">
      <span class="yaml-color-value" id="val-${pathId}">${hexColor.toUpperCase()}</span>
    </div>`;
  },
  
  // Render number input
  renderNumber: function(path, value, schema) {
    const min = schema.min ?? '';
    const max = schema.max ?? '';
    const unit = schema.unit || '';
    
    let html = `<input type="number" class="yaml-input-number" data-path="${path}"
      value="${value}" ${min !== '' ? 'min="' + min + '"' : ''} ${max !== '' ? 'max="' + max + '"' : ''}>`;
    if (unit) {
      html += `<span class="yaml-input-unit">${unit}</span>`;
    }
    return html;
  },
  
  // Render text input
  renderText: function(path, value) {
    return `<input type="text" class="yaml-input-text" data-path="${path}" value="${value || ''}">`;
  },
  
  // Render file/sprite selector
  renderFileSelect: function(path, value, schema) {
    const fileType = schema.fileType || 'sprite';
    const allowNone = schema.allowNone !== false;
    
    let html = `<div class="yaml-file-select">
      <select class="yaml-select" data-path="${path}" data-file-type="${fileType}">`;
    
    if (allowNone) {
      const selected = (value < 0) ? 'selected' : '';
      html += `<option value="-1" ${selected}>None (default)</option>`;
    }
    
    // Sprites will be populated dynamically
    if (value >= 0) {
      html += `<option value="${value}" selected>Sprite ${value}</option>`;
    }
    
    html += `</select></div>`;
    return html;
  },
  
  // Render nested group
  renderNestedGroup: function(path, key, value, schema) {
    const label = schema.label || this.formatLabel(key);
    
    let html = `<div class="yaml-nested-group">`;
    html += `<div class="yaml-nested-title">${label}</div>`;
    
    if (typeof value === 'object') {
      for (const [subKey, subValue] of Object.entries(value)) {
        if (subKey.startsWith('_')) continue;
        const subSchema = schema.fields?.[subKey] || this.inferFieldSchema(subKey, subValue);
        html += this.renderField(path + '.' + subKey, subKey, subValue, subSchema);
      }
    }
    
    html += `</div>`;
    return html;
  },
  
  // Auto-detect field type
  renderAutoField: function(path, key, value) {
    if (typeof value === 'boolean') {
      return this.renderToggle(path, value);
    } else if (typeof value === 'number') {
      return this.renderNumber(path, value, {});
    } else if (typeof value === 'object' && value !== null) {
      // Check if it's a color
      if ('r' in value && 'g' in value && 'b' in value) {
        return this.renderColor(path, value);
      }
      // It's a nested object
      return `<span class="yaml-readonly">[object]</span>`;
    } else {
      return this.renderText(path, value);
    }
  },
  
  // Get schema for section
  getSchemaForSection: function(key) {
    const schemas = {
      'Global': {
        icon: '○',
        label: 'Scene Info',
        desc: 'Basic scene identification',
        fields: {
          name: { type: 'text', label: 'Scene Name' },
          id: { type: 'readonly', label: 'Scene ID' },
          description: { type: 'text', label: 'Description' },
          version: { type: 'readonly', label: 'Version' },
          author: { type: 'text', label: 'Author' }
        }
      },
      'Display': {
        icon: '◐',
        label: 'Display Settings',
        desc: 'HUB75 matrix display configuration',
        fields: {
          // Animation type is now hidden - always static with mirror toggle in Animation Settings
          main_sprite_id: { type: 'file', label: 'Main Sprite', fileType: 'sprite', allowNone: true },
          background: { type: 'color', label: 'Background Color' }
        }
      },
      'AnimationParams': {
        icon: '◑',
        label: 'Animation Parameters',
        desc: 'Settings specific to the selected animation type',
        dynamic: true,
        fields: {}
      },
      'Audio': {
        icon: '◉',
        label: 'Audio Reactive',
        desc: 'Audio-reactive animation settings',
        fields: {
          enabled: { type: 'toggle', label: 'Enable Audio Reactive' },
          source: {
            type: 'dropdown',
            label: 'Audio Source',
            options: [
              { label: 'Built-in Microphone', value: 'mic' },
              { label: 'Line Input', value: 'line' }
            ]
          },
          sensitivity: { type: 'slider', label: 'Audio Sensitivity', min: 0, max: 2, step: 0.1 }
        }
      },
      'Sprites': {
        icon: '◧',
        label: 'Sprites',
        desc: 'Sprite definitions for this scene',
        fields: {}
      }
    };
    
    return schemas[key] || { icon: '○', label: key, fields: {} };
  },
  
  // Get animation-specific parameter schema
  getAnimationParamsSchema: function(animType) {
    const animSchemas = {
      'static_sprite': {
        position_x: { type: 'slider', label: 'Position X', min: 0, max: 128, step: 1, unit: 'px', param: 'x' },
        position_y: { type: 'slider', label: 'Position Y', min: 0, max: 32, step: 1, unit: 'px', param: 'y' },
        rotation: { type: 'slider', label: 'Rotation', min: 0, max: 360, step: 1, unit: '°', param: 'rotation' },
        scale: { type: 'slider', label: 'Scale', min: 0.1, max: 4.0, step: 0.1, param: 'scale' }
      },
      'static_mirrored': {
        left_x: { type: 'slider', label: 'Left X', min: 0, max: 64, step: 1, unit: 'px', param: 'left_x' },
        left_y: { type: 'slider', label: 'Left Y', min: 0, max: 32, step: 1, unit: 'px', param: 'left_y' },
        left_rotation: { type: 'slider', label: 'Left Rotation', min: 0, max: 360, step: 1, unit: '°', param: 'left_rotation' },
        left_scale: { type: 'slider', label: 'Left Scale', min: 0.1, max: 4.0, step: 0.1, param: 'left_scale' },
        right_x: { type: 'slider', label: 'Right X', min: 64, max: 128, step: 1, unit: 'px', param: 'right_x' },
        right_y: { type: 'slider', label: 'Right Y', min: 0, max: 32, step: 1, unit: 'px', param: 'right_y' },
        right_rotation: { type: 'slider', label: 'Right Rotation', min: 0, max: 360, step: 1, unit: '°', param: 'right_rotation' },
        right_scale: { type: 'slider', label: 'Right Scale', min: 0.1, max: 4.0, step: 0.1, param: 'right_scale' }
      }
    };
    return animSchemas[animType] || {};
  },
  
  // Render animation-specific parameters section
  renderAnimationParams: function(animType, currentParams) {
    const schema = this.getAnimationParamsSchema(animType);
    if (Object.keys(schema).length === 0) {
      return '<div class="yaml-section-desc">No additional parameters for this animation type.</div>';
    }
    
    let html = '';
    for (const [key, fieldSchema] of Object.entries(schema)) {
      const paramName = fieldSchema.param || key;
      const value = currentParams?.[paramName] ?? this.getDefaultValue(fieldSchema);
      const path = 'AnimParams.' + paramName;
      html += this.renderField(path, key, value, fieldSchema);
    }
    return html;
  },
  
  // Get default value for a field
  getDefaultValue: function(schema) {
    switch (schema.type) {
      case 'toggle': return false;
      case 'slider': return schema.min || 0;
      case 'number': return 0;
      case 'color': return { r: 0, g: 0, b: 0 };
      default: return '';
    }
  },

  // Infer schema from key and value
  inferFieldSchema: function(key, value) {
    // Common naming patterns
    if (key.includes('enable') || key.includes('active') || key === 'mirror') {
      return { type: 'toggle' };
    }
    if (key.includes('color') || key === 'background') {
      return { type: 'color' };
    }
    if (key === 'main_sprite_id') {
      return { type: 'file', fileType: 'sprite', label: 'Main Sprite', allowNone: true };
    }
    if (key.includes('_id') || key === 'id') {
      return { type: 'readonly' };
    }
    if (key.includes('brightness')) {
      return { type: 'slider', min: 0, max: 255 };
    }
    if (key.includes('rotation')) {
      return { type: 'slider', min: -180, max: 180, step: 1, unit: '°' };
    }
    if (key === 'x') {
      return { type: 'slider', min: 0, max: 128, step: 1, unit: 'px' };
    }
    if (key === 'y') {
      return { type: 'slider', min: 0, max: 32, step: 1, unit: 'px' };
    }
    if (key.includes('sensitivity') || key.includes('intensity')) {
      return { type: 'slider', min: 0, max: 3, step: 0.1 };
    }
    if (key.includes('animation_type')) {
      return { 
        type: 'dropdown',
        label: 'Animation Type',
        options: [
          { label: 'Static (with Mirror Toggle)', value: 'static' }
        ]
      };
    }
    if (key === 'left_x' || key === 'right_x') {
      return { type: 'slider', min: 0, max: 128, step: 1, unit: 'px' };
    }
    if (key === 'left_y' || key === 'right_y') {
      return { type: 'slider', min: 0, max: 32, step: 1, unit: 'px' };
    }
    if (key === 'left_rotation' || key === 'right_rotation') {
      return { type: 'slider', min: 0, max: 360, step: 1, unit: '°' };
    }
    if (key === 'left_scale' || key === 'right_scale' || key === 'scale') {
      return { type: 'slider', min: 0.1, max: 4.0, step: 0.1 };
    }
    
    // Type-based inference
    if (typeof value === 'boolean') return { type: 'toggle' };
    if (typeof value === 'number') return { type: 'slider', min: 0, max: 100, step: 1 };
    if (typeof value === 'object' && value?.r !== undefined) return { type: 'color' };
    if (typeof value === 'object') return { type: 'group' };
    
    return { type: 'text' };
  },
  
  // Format field key as label
  formatLabel: function(key) {
    return key.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
  },
  
  // Setup form handlers
  setupFormHandlers: function() {
    const self = this;
    
    // Section toggles
    document.querySelectorAll('.yaml-section-header').forEach(header => {
      header.addEventListener('click', () => {
        header.parentElement.classList.toggle('collapsed');
      });
    });
    
    // Toggle inputs
    document.querySelectorAll('.yaml-toggle input').forEach(toggle => {
      toggle.addEventListener('change', (e) => {
        const path = toggle.dataset.path;
        const value = toggle.checked;
        self.updateField(path, value, 'boolean');
        
        // If this is the mirror toggle, refresh the animation settings section
        if (path === 'AnimParams.mirror') {
          const animType = self.sceneData?.Display?.animation_type || 'static';
          // Small delay to let the update complete
          setTimeout(() => {
            self.updateAnimParamsSection(animType, value);
          }, 100);
        }
      });
    });
    
    // Slider inputs
    document.querySelectorAll('.yaml-slider').forEach(slider => {
      const pathId = slider.dataset.path.replace(/\./g, '-');
      const valueSpan = document.getElementById('val-' + pathId);
      
      slider.addEventListener('input', (e) => {
        if (valueSpan) {
          valueSpan.textContent = slider.value;
        }
      });
      
      slider.addEventListener('change', (e) => {
        self.updateField(slider.dataset.path, parseFloat(slider.value), 'number');
      });
    });
    
    // Select inputs
    document.querySelectorAll('.yaml-select').forEach(select => {
      select.addEventListener('change', (e) => {
        const type = select.dataset.fileType === 'sprite' ? 'number' : 'string';
        let value = type === 'number' ? parseInt(select.value) : select.value;
        
        // Shader type is a number
        if (select.dataset.path === 'Shader.type') {
          value = parseInt(select.value);
          // Show/hide color override params based on shader type
          const overrideParams = document.getElementById('shader-color-override-params');
          if (overrideParams) {
            overrideParams.style.display = (value === 1) ? 'block' : 'none';
          }
          // Show/hide hue cycle params based on shader type
          const hueCycleParams = document.getElementById('shader-hue-cycle-params');
          if (hueCycleParams) {
            hueCycleParams.style.display = (value === 2) ? 'block' : 'none';
          }
          // Show/hide gradient cycle params based on shader type
          const gradientCycleParams = document.getElementById('shader-gradient-cycle-params');
          if (gradientCycleParams) {
            gradientCycleParams.style.display = (value === 3) ? 'block' : 'none';
          }
          // Show/hide glitch params based on shader type
          const glitchParams = document.getElementById('shader-glitch-params');
          if (glitchParams) {
            glitchParams.style.display = (value === 4) ? 'block' : 'none';
          }
        }
        
        this.updateField(select.dataset.path, value, type);
        
        // If animation type changed, update the animation params section
        if (select.dataset.path === 'Display.animation_type') {
          this.updateAnimParamsSection(value);
        }
      });
    });
    
    // Color pickers
    document.querySelectorAll('.yaml-color-picker').forEach(picker => {
      const pathId = picker.dataset.path.replace(/\./g, '-');
      const valueSpan = document.getElementById('val-' + pathId);
      
      picker.addEventListener('input', (e) => {
        if (valueSpan) {
          valueSpan.textContent = picker.value.toUpperCase();
        }
        const rgb = this.hexToRgb(picker.value);
        this.updateField(picker.dataset.path, rgb, 'color');
      });
    });
    
    // Range inputs (yaml-range class - for hue/gradient sliders)
    document.querySelectorAll('.yaml-range').forEach(slider => {
      slider.addEventListener('change', (e) => {
        const value = parseFloat(slider.value);
        console.log('[yaml-range] ' + slider.dataset.path + ' = ' + value);
        this.updateField(slider.dataset.path, value, 'number');
      });
    });
    
    // Text/Number inputs (debounced)
    document.querySelectorAll('.yaml-input-text, .yaml-input-number').forEach(input => {
      let timeout;
      input.addEventListener('input', (e) => {
        clearTimeout(timeout);
        timeout = setTimeout(() => {
          const type = input.type === 'number' ? 'number' : 'string';
          const value = type === 'number' ? parseFloat(input.value) : input.value;
          this.updateField(input.dataset.path, value, type);
        }, 500);
      });
    });
    
    // Populate sprite selectors
    this.populateSpriteSelectors();
  },
  
  // Populate sprite selectors
  populateSpriteSelectors: function() {
    fetch('/api/sprites')
      .then(r => r.json())
      .then(data => {
        if (data.sprites) {
          document.querySelectorAll('select[data-file-type="sprite"]').forEach(select => {
            const currentValue = select.value;
            // Keep first option
            const firstOption = select.options[0];
            select.innerHTML = '';
            select.appendChild(firstOption);
            
            data.sprites.forEach(sprite => {
              const opt = document.createElement('option');
              opt.value = sprite.id;
              opt.textContent = sprite.name || ('Sprite ' + sprite.id);
              if (sprite.id == currentValue) opt.selected = true;
              select.appendChild(opt);
            });
          });
        }
      })
      .catch(err => console.log('Could not load sprites:', err));
  },
  
  // Map UI paths to server field names
  pathToFieldName: function(path) {
    // Handle AnimParams paths - all go to animParams in the payload
    if (path.startsWith('AnimParams.')) {
      const paramName = path.split('.')[1];
      return 'animParams.' + paramName;
    }
    // Handle Display.params paths - also go to animParams
    if (path.startsWith('Display.params.')) {
      const paramName = path.split('.')[2];  // Get 'left_x' from 'Display.params.left_x'
      return 'animParams.' + paramName;
    }
    // Handle Shader paths - all go to shaderParams in the payload
    if (path.startsWith('Shader.')) {
      const paramName = path.split('.')[1];
      return 'shaderParams.' + paramName;
    }
    
    const mapping = {
      'Display.enabled': 'displayEnabled',
      'Display.animation_type': 'animType',
      'Display.main_sprite_id': 'spriteId',
      'Display.rotation': 'animParams.rotation',
      'Display.sensitivity': 'animParams.intensity',
      'Display.mirror': 'mirrorSprite',
      'Display.position.x': 'animParams.center_x',
      'Display.position.y': 'animParams.center_y',
      'Display.background': 'bgColor',
      'Global.name': 'name'
    };
    return mapping[path] || path;
  },
  
  // Build server payload from field update
  buildUpdatePayload: function(path, value, type) {
    const payload = { id: this.currentSceneId };
    const fieldName = this.pathToFieldName(path);
    
    // Handle nested animParams fields
    if (fieldName.startsWith('animParams.')) {
      const paramName = fieldName.split('.')[1];
      payload.animParams = {};
      payload.animParams[paramName] = value;
    }
    // Handle shaderParams fields
    else if (fieldName.startsWith('shaderParams.')) {
      const paramName = fieldName.split('.')[1];
      payload.shaderParams = {};
      // Handle color fields specially (mask_color, override_color, hue_color_N)
      if ((paramName === 'mask_color' || paramName === 'override_color' || paramName.startsWith('hue_color_')) && typeof value === 'object') {
        payload.shaderParams[paramName] = value;  // {r, g, b}
      } else {
        payload.shaderParams[paramName] = value;
      }
    }
    // Handle background color specially (needs bgR, bgG, bgB)
    else if (fieldName === 'bgColor' && typeof value === 'object') {
      payload.bgR = value.r || 0;
      payload.bgG = value.g || 0;
      payload.bgB = value.b || 0;
    }
    // Handle other fields directly
    else {
      payload[fieldName] = value;
    }
    
    return payload;
  },
  
  // Update field via API
  updateField: function(path, value, type) {
    const payload = this.buildUpdatePayload(path, value, type);
    console.log('[updateField] path=' + path + ' value=' + value + ' payload=' + JSON.stringify(payload));
    
    fetch('/api/scene/update', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    })
    .then(r => {
      if (!r.ok) {
        throw new Error('HTTP ' + r.status);
      }
      return r.json();
    })
    .then(data => {
      if (data.success) {
        this.showSaveIndicator(path, true);
      } else {
        console.error('Update failed:', data.error);
        this.showSaveIndicator(path, false);
      }
    })
    .catch(err => {
      console.error('Update error:', err);
      this.showSaveIndicator(path, false);
    });
  },
  
  // Show save indicator
  showSaveIndicator: function(path, success) {
    const row = document.querySelector(`[data-field-path="${path}"]`);
    if (row) {
      let indicator = row.querySelector('.save-indicator');
      if (!indicator) {
        indicator = document.createElement('span');
        indicator.className = 'save-indicator';
        row.querySelector('.yaml-field-control').appendChild(indicator);
      }
      indicator.textContent = success ? '✓' : '✗';
      indicator.className = 'save-indicator ' + (success ? 'success' : 'error');
      
      setTimeout(() => indicator.remove(), 1500);
    }
  },
  
  // Update the dynamic hue palette color pickers when count changes
  updateHuePaletteColors: function(count) {
    if (count < 1) count = 1;
    if (count > 32) count = 32;
    
    const container = document.getElementById('hue-palette-colors');
    if (!container) return;
    
    // Clear existing
    container.innerHTML = '';
    
    // Build color pickers
    for (let i = 0; i < count; i++) {
      const r = this.getShaderFieldValue('hue_color_' + i + '_r');
      const g = this.getShaderFieldValue('hue_color_' + i + '_g');
      const b = this.getShaderFieldValue('hue_color_' + i + '_b');
      const hex = '#' + [r, g, b].map(c => c.toString(16).padStart(2, '0')).join('');
      
      let html = `<div class="yaml-field-row" data-field-path="Shader.hue_color_${i}" id="hue-color-row-${i}">`;
      html += `<label class="yaml-field-label">Color ${i + 1}</label>`;
      html += `<div class="yaml-field-control">`;
      html += `<div class="yaml-color-container">`;
      html += `<input type="color" class="yaml-color-picker" value="${hex}" data-path="Shader.hue_color_${i}" data-color-index="${i}">`;
      html += `<span class="yaml-color-value" id="val-Shader-hue_color_${i}">${hex}</span>`;
      html += `</div></div></div>`;
      
      container.innerHTML += html;
    }
    
    // Re-attach event handlers to new color pickers
    const self = this;
    container.querySelectorAll('.yaml-color-picker').forEach(picker => {
      const pathId = picker.dataset.path.replace(/\./g, '-');
      const valueSpan = document.getElementById('val-' + pathId);
      
      picker.addEventListener('input', (e) => {
        if (valueSpan) {
          valueSpan.textContent = picker.value.toUpperCase();
        }
        const rgb = self.hexToRgb(picker.value);
        self.updateField(picker.dataset.path, rgb, 'color');
      });
    });
    
    // Also send the color count update
    this.updateField('Shader.hue_color_count', count, 'number');
  },
  
  // Update the dynamic gradient palette color pickers when count changes
  updateGradientPaletteColors: function(count) {
    if (count < 1) count = 1;
    if (count > 32) count = 32;
    
    const container = document.getElementById('gradient-palette-colors');
    if (!container) return;
    
    // Clear existing
    container.innerHTML = '';
    
    // Build color pickers
    for (let i = 0; i < count; i++) {
      const r = this.getShaderFieldValue('hue_color_' + i + '_r');
      const g = this.getShaderFieldValue('hue_color_' + i + '_g');
      const b = this.getShaderFieldValue('hue_color_' + i + '_b');
      const hex = '#' + [r, g, b].map(c => c.toString(16).padStart(2, '0')).join('');
      
      let html = `<div class="yaml-field-row" data-field-path="Shader.hue_color_${i}" id="gradient-color-row-${i}">`;
      html += `<label class="yaml-field-label">Color ${i + 1}</label>`;
      html += `<div class="yaml-field-control">`;
      html += `<div class="yaml-color-container">`;
      html += `<input type="color" class="yaml-color-picker" value="${hex}" data-path="Shader.hue_color_${i}" data-color-index="${i}">`;
      html += `<span class="yaml-color-value" id="val-gradient-color_${i}">${hex}</span>`;
      html += `</div></div></div>`;
      
      container.innerHTML += html;
    }
    
    // Re-attach event handlers to new color pickers
    const self = this;
    container.querySelectorAll('.yaml-color-picker').forEach(picker => {
      const pathId = picker.dataset.path.replace(/\./g, '-');
      const valueSpan = document.getElementById('val-gradient-color_' + picker.dataset.colorIndex);
      
      picker.addEventListener('input', (e) => {
        if (valueSpan) {
          valueSpan.textContent = picker.value.toUpperCase();
        }
        const rgb = self.hexToRgb(picker.value);
        self.updateField(picker.dataset.path, rgb, 'color');
      });
    });
    
    // Also send the color count update
    this.updateField('Shader.hue_color_count', count, 'number');
  },
  
  // Hex to RGB
  hexToRgb: function(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : { r: 0, g: 0, b: 0 };
  },
  
  // Setup event handlers
  setupEventHandlers: function() {
    // Scene selector
    document.getElementById('scene-select').addEventListener('change', (e) => {
      this.loadSceneConfig(e.target.value);
    });
    
    // Apply button
    document.getElementById('btn-apply').addEventListener('click', () => {
      this.applyScene();
    });
    
    // Reset button
    document.getElementById('btn-reset').addEventListener('click', () => {
      if (confirm('Reset all changes?')) {
        this.loadSceneConfig(this.currentSceneId);
      }
    });
  },
  
  // Apply scene
  applyScene: function() {
    fetch('/api/scene/activate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: this.currentSceneId })
    })
    .then(r => {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return r.json();
    })
    .then(data => {
      if (data.success) {
        alert('Scene applied!');
      } else {
        alert('Failed to apply scene: ' + (data.error || 'Unknown error'));
      }
    })
    .catch(err => {
      alert('Error applying scene: ' + err);
    });
  },
  
  // Show loading
  showLoading: function() {
    const loading = document.getElementById('loading');
    if (loading) loading.style.display = 'flex';
  },
  
  // Hide loading
  hideLoading: function() {
    const loading = document.getElementById('loading');
    if (loading) loading.style.display = 'none';
  },
  
  // ============================================
  // Mic Reactivity Functions
  // ============================================
  
  openMicModal: function(paramKey, label) {
    this.currentMicParam = paramKey;
    document.getElementById('mic-modal-param').textContent = label;
    
    // Load existing settings for this param
    const settings = this.micReactSettings[paramKey] || { x: 0, y: 1, z: 0, enabled: false };
    document.getElementById('mic-x').value = settings.x;
    document.getElementById('mic-y').value = settings.y;
    document.getElementById('mic-z').value = settings.z;
    document.getElementById('mic-enable').checked = settings.enabled;
    
    document.getElementById('mic-modal').style.display = 'flex';
    this.startMicPolling();
  },
  
  closeMicModal: function() {
    document.getElementById('mic-modal').style.display = 'none';
    this.stopMicPolling();
    this.currentMicParam = null;
  },
  
  saveMicSettings: function() {
    if (!this.currentMicParam) return;
    
    const xVal = document.getElementById('mic-x').value;
    const yVal = document.getElementById('mic-y').value;
    const zVal = document.getElementById('mic-z').value;
    
    console.log('[saveMicSettings] Raw values - x:', xVal, 'y:', yVal, 'z:', zVal);
    
    const settings = {
      x: parseFloat(xVal) || 0,
      y: parseFloat(yVal) || 1,
      z: parseFloat(zVal) || 0,
      enabled: document.getElementById('mic-enable').checked
    };
    
    console.log('[saveMicSettings] Parsed settings:', settings);
    
    this.micReactSettings[this.currentMicParam] = settings;
    
    // Update button state
    const btn = document.querySelector('.mic-btn[data-param="' + this.currentMicParam + '"]');
    if (btn) {
      if (settings.enabled) {
        btn.classList.add('active');
      } else {
        btn.classList.remove('active');
      }
    }
    
    // Save to server - send full micReact object
    const payload = {
      id: this.currentSceneId,
      micReact: this.micReactSettings
    };
    
    console.log('[saveMicSettings] Sending payload:', JSON.stringify(payload));
    
    fetch('/api/scene/update', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    })
    .then(r => r.json())
    .then(data => {
      if (data.success) {
        console.log('[saveMicSettings] Saved successfully');
        // Reload scene to verify saved values
        console.log('[saveMicSettings] Reloading scene to verify...');
        return fetch('/api/scene/config?id=' + this.currentSceneId);
      } else {
        console.error('[saveMicSettings] Failed:', data.error);
      }
    })
    .then(r => r ? r.json() : null)
    .then(data => {
      if (data && data.config && data.config.MicReact) {
        console.log('[saveMicSettings] Server has MicReact:', data.config.MicReact);
      }
    })
    .catch(err => console.error('[saveMicSettings] Error:', err));
    
    this.closeMicModal();
  },
  
  startMicPolling: function() {
    const self = this;
    if (this.micPollingInterval) return;
    
    this.micPollingInterval = setInterval(() => {
      fetch('/api/sensors')
        .then(r => r.json())
        .then(data => {
          self.currentMicDb = data.mic_db || -60;
          self.updateMicPreview();
          // NOTE: Live routing to variables removed for debugging
          // self.applyMicReactivity();
        })
        .catch(() => {});
    }, 100);
  },
  
  stopMicPolling: function() {
    if (this.micPollingInterval) {
      clearInterval(this.micPollingInterval);
      this.micPollingInterval = null;
    }
  },
  
  updateMicPreview: function() {
    const x = parseFloat(document.getElementById('mic-x')?.value) || 0;
    const y = parseFloat(document.getElementById('mic-y')?.value) || 1;
    const z = parseFloat(document.getElementById('mic-z')?.value) || 0;
    
    // Show raw mic value
    const rawEl = document.getElementById('mic-raw-val');
    if (rawEl) {
      rawEl.textContent = this.currentMicDb.toFixed(2) + ' dB';
    }
    
    // Show equation result
    const result = (y * (this.currentMicDb - x)) + z;
    const previewEl = document.getElementById('mic-preview-val');
    if (previewEl) {
      previewEl.textContent = result.toFixed(2);
    }
  },
  
  updateMicReactivityPolling: function() {
    // Check if any mic reactivity is enabled
    const anyEnabled = Object.values(this.micReactSettings).some(s => s.enabled);
    if (anyEnabled && !this.micPollingInterval) {
      this.startMicPolling();
    } else if (!anyEnabled && this.micPollingInterval && !document.getElementById('mic-modal').style.display !== 'none') {
      this.stopMicPolling();
    }
  },
  
  applyMicReactivity: function() {
    for (const [paramKey, settings] of Object.entries(this.micReactSettings)) {
      if (!settings.enabled) continue;
      
      const result = (settings.y * (this.currentMicDb - settings.x)) + settings.z;
      
      // Find the slider/input for this param and update it
      const slider = document.querySelector('.yaml-slider[data-path="AnimParams.' + paramKey + '"]');
      if (slider) {
        slider.value = result;
        const pathId = 'AnimParams.' + paramKey;
        const valueSpan = document.getElementById('val-' + pathId.replace(/\\./g, '-'));
        if (valueSpan) {
          valueSpan.textContent = result.toFixed(2);
        }
        // Trigger update to server
        this.updateField(pathId, result, 'number');
      }
      
      const numInput = document.querySelector('.yaml-input-number[data-path="AnimParams.' + paramKey + '"]');
      if (numInput) {
        numInput.value = result.toFixed(2);
        this.updateField('AnimParams.' + paramKey, result, 'number');
      }
    }
  }
};

// Initialize on DOM ready
document.addEventListener('DOMContentLoaded', () => {
  DisplayConfig.init();
});
  </script>
</body>
</html>
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
