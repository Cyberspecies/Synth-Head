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
  width: 80px;
  padding: 10px 12px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 8px;
  color: var(--text-primary);
  font-size: 13px;
  font-family: 'SF Mono', Monaco, monospace;
  text-align: right;
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
        // Fallback to built-in types
        self.animationsList = [
          { id: 'static_sprite', name: 'Static Sprite' },
          { id: 'static_mirrored', name: 'Static Mirrored' }
        ];
      });
  },
  
  // Populate animation type dropdown
  populateAnimationDropdown: function() {
    const select = document.getElementById('animation-type-select');
    if (!select) return;
    const currentValue = select.value || this.sceneData?.Display?.animation_type || 'static_sprite';
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
          this.renderConfigForm(data.config);
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
    html += this.renderAnimParamsSection(animType, config.Display);
    
    // Add remaining sections
    const otherSections = ['LEDS', 'Audio'];
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
      // Typed number input
      const min = schema.min ?? '';
      const max = schema.max ?? '';
      html += `<input type="number" class="yaml-input-number" data-path="${path}"
        value="${value}" ${min !== '' ? 'min="' + min + '"' : ''} ${max !== '' ? 'max="' + max + '"' : ''}>`;
      if (unit) {
        html += `<span class="yaml-input-unit">${unit}</span>`;
      }
    } else {
      // Slider (default)
      const min = schema.min ?? 0;
      const max = schema.max ?? 100;
      const step = schema.step ?? 1;
      const pathId = path.replace(/\./g, '-');
      
      html += `<div class="yaml-slider-container">
        <input type="range" class="yaml-slider" data-path="${path}"
          min="${min}" max="${max}" step="${step}" value="${value}">
        <span class="yaml-slider-value" id="val-${pathId}">${value}${unit ? ' ' + unit : ''}</span>
      </div>`;
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
      'LEDS': {
        icon: '◈',
        label: 'LED Strips',
        desc: 'WS2812 LED strip configuration',
        fields: {
          enabled: { type: 'toggle', label: 'Enable LEDs' },
          brightness: { type: 'slider', label: 'Brightness', min: 0, max: 255, unit: '%' },
          color: { type: 'color', label: 'Global Color' },
          strips: {
            type: 'group',
            label: 'Individual Strips',
            fields: {
              left_fin: { type: 'group', label: 'Left Fin' },
              right_fin: { type: 'group', label: 'Right Fin' },
              tongue: { type: 'group', label: 'Tongue' },
              scales: { type: 'group', label: 'Scales' }
            }
          }
        }
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
        const value = type === 'number' ? parseInt(select.value) : select.value;
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
      'Global.name': 'name',
      'LEDS.enabled': 'ledsEnabled',
      'LEDS.brightness': 'ledBrightness',
      'LEDS.color': 'ledColor'
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
    // Handle background color specially (needs bgR, bgG, bgB)
    else if (fieldName === 'bgColor' && typeof value === 'object') {
      payload.bgR = value.r || 0;
      payload.bgG = value.g || 0;
      payload.bgB = value.b || 0;
    }
    // Handle ledColor specially (needs r,g,b object)
    else if (fieldName === 'ledColor' && typeof value === 'object') {
      payload.ledColor = { r: value.r || 0, g: value.g || 0, b: value.b || 0 };
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
