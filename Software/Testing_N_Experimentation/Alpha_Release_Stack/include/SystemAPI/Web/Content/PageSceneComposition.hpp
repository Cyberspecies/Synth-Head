/*****************************************************************
 * @file PageSceneComposition.hpp
 * @brief Scene Composition Web Page - Simplified UI with Effects
 * 
 * Features:
 * - Create, edit, delete scenes via dropdown
 * - Animation types: Static Images, Gyro Eyes
 * - Transitions: SDF Morph, Glitch, Particles
 * - Effects section for constant effects (glitch, etc.)
 * - Sprite selection with mirror option
 * 
 * @author ARCOS
 * @version 5.0
 *****************************************************************/

#pragma once

#include <string>

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char* PAGE_SCENE_COMPOSITION = R"rawpage(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Scenes</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    /* Scene Bar */
    .scene-bar {
      background: var(--bg-tertiary);
      border-radius: 10px;
      padding: 14px 16px;
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 16px;
      flex-wrap: wrap;
    }
    .scene-bar label {
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-secondary);
    }
    .scene-select {
      flex: 1;
      min-width: 180px;
      padding: 10px 14px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-size: 0.9rem;
      cursor: pointer;
    }
    .scene-select:focus { outline: none; border-color: var(--accent); }
    .scene-actions { display: flex; gap: 6px; }
    
    /* Buttons */
    .btn {
      padding: 9px 16px;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      font-size: 0.8rem;
      font-weight: 600;
      transition: all 0.15s;
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    .btn:hover { filter: brightness(1.1); }
    .btn:active { transform: scale(0.98); }
    .btn-primary { background: var(--accent); color: var(--bg-primary); }
    .btn-secondary { background: var(--bg-tertiary); color: var(--text-primary); border: 1px solid var(--border); }
    .btn-success { background: var(--success); color: #fff; }
    .btn-danger { background: var(--danger); color: #fff; }
    .btn-sm { padding: 7px 12px; font-size: 0.75rem; }

    /* Config Grid */
    .config-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 16px;
    }
    @media (max-width: 900px) {
      .config-grid { grid-template-columns: 1fr; }
    }
    
    /* Panels */
    .panel {
      background: var(--bg-tertiary);
      border-radius: 10px;
      overflow: hidden;
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
    .panel-header h3 .icon { color: var(--accent); font-family: monospace; }
    .panel-body { padding: 16px; }
    
    /* Animation Type Grid */
    .anim-type-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }
    .anim-type-card {
      background: var(--bg-secondary);
      border: 2px solid transparent;
      border-radius: 8px;
      padding: 16px 12px;
      cursor: pointer;
      transition: all 0.2s;
      text-align: center;
    }
    .anim-type-card:hover { border-color: var(--border); }
    .anim-type-card.selected { border-color: var(--accent); background: rgba(255,107,0,0.08); }
    .anim-type-card .icon { font-size: 24px; margin-bottom: 8px; font-family: monospace; color: var(--accent); }
    .anim-type-card .name { font-size: 0.85rem; font-weight: 600; }
    .anim-type-card .desc { font-size: 0.7rem; color: var(--text-secondary); margin-top: 4px; }

    /* Transition Grid */
    .transition-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
    }
    .transition-card {
      background: var(--bg-secondary);
      border: 2px solid transparent;
      border-radius: 8px;
      padding: 12px 8px;
      cursor: pointer;
      transition: all 0.2s;
      text-align: center;
    }
    .transition-card:hover { border-color: var(--border); }
    .transition-card.selected { border-color: var(--accent); background: rgba(255,107,0,0.08); }
    .transition-card .icon { font-size: 18px; margin-bottom: 4px; font-family: monospace; color: var(--accent); }
    .transition-card .name { font-size: 0.75rem; font-weight: 600; }

    /* Effects Grid */
    .effect-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 12px;
      background: var(--bg-secondary);
      border-radius: 8px;
      margin-bottom: 8px;
    }
    .effect-row:last-child { margin-bottom: 0; }
    .effect-info { display: flex; align-items: center; gap: 10px; }
    .effect-info .icon { font-family: monospace; color: var(--accent); }
    .effect-info .name { font-size: 0.85rem; font-weight: 600; }
    .effect-control { display: flex; align-items: center; gap: 12px; }
    .effect-slider {
      width: 100px;
      height: 6px;
      -webkit-appearance: none;
      background: var(--bg-tertiary);
      border-radius: 3px;
      outline: none;
    }
    .effect-slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 14px;
      height: 14px;
      background: var(--accent);
      border-radius: 50%;
      cursor: pointer;
    }
    .effect-value {
      min-width: 35px;
      font-size: 0.75rem;
      color: var(--text-secondary);
      text-align: right;
      font-family: 'SF Mono', Monaco, monospace;
    }

    /* Parameter Controls */
    .param-section { margin-bottom: 16px; }
    .param-section:last-child { margin-bottom: 0; }
    .param-section-title {
      font-size: 0.7rem;
      font-weight: 700;
      color: var(--accent);
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 10px;
      padding-bottom: 6px;
      border-bottom: 1px solid var(--border);
    }
    .param-row {
      display: flex;
      align-items: center;
      padding: 10px 0;
      border-bottom: 1px solid var(--bg-tertiary);
    }
    .param-row:last-child { border-bottom: none; }
    .param-label {
      flex: 0 0 45%;
      font-size: 0.8rem;
      color: var(--text-primary);
    }
    .param-control {
      flex: 1;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .param-value {
      min-width: 45px;
      text-align: right;
      font-size: 0.75rem;
      color: var(--text-secondary);
      font-family: 'SF Mono', Monaco, monospace;
    }
    input[type="range"] {
      flex: 1;
      height: 6px;
      -webkit-appearance: none;
      background: var(--bg-tertiary);
      border-radius: 3px;
      outline: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 14px;
      height: 14px;
      background: var(--accent);
      border-radius: 50%;
      cursor: pointer;
    }
    input[type="color"] {
      width: 36px;
      height: 28px;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      padding: 0;
    }

    /* Toggle Switch */
    .toggle-switch {
      position: relative;
      width: 40px;
      height: 22px;
    }
    .toggle-switch input { display: none; }
    .toggle-slider {
      position: absolute;
      inset: 0;
      background: var(--bg-tertiary);
      border-radius: 11px;
      cursor: pointer;
      transition: background 0.2s;
    }
    .toggle-slider:before {
      content: '';
      position: absolute;
      width: 16px;
      height: 16px;
      left: 3px;
      bottom: 3px;
      background: #fff;
      border-radius: 50%;
      transition: transform 0.2s;
    }
    .toggle-switch input:checked + .toggle-slider { background: var(--accent); }
    .toggle-switch input:checked + .toggle-slider:before { transform: translateX(18px); }

    /* Sprite Grid */
    .sprite-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(60px, 1fr));
      gap: 6px;
      max-height: 160px;
      overflow-y: auto;
      padding: 2px;
    }
    .sprite-item {
      aspect-ratio: 1;
      background: var(--bg-secondary);
      border: 2px solid transparent;
      border-radius: 6px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.15s;
      overflow: hidden;
    }
    .sprite-item:hover { border-color: var(--border); }
    .sprite-item.selected { border-color: var(--accent); }
    .sprite-item img { max-width: 100%; max-height: 100%; object-fit: contain; }
    .sprite-item .placeholder { color: var(--text-muted); font-size: 18px; font-family: monospace; }

    /* Mirror Toggle Row */
    .mirror-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 10px 12px;
      background: var(--bg-secondary);
      border-radius: 6px;
      margin-top: 10px;
    }
    .mirror-row .label { font-size: 0.8rem; }
    .mirror-row .hint { font-size: 0.7rem; color: var(--text-secondary); }

    /* Preview Bar */
    .preview-bar {
      background: var(--bg-tertiary);
      border-radius: 10px;
      padding: 12px 16px;
      display: flex;
      align-items: center;
      gap: 12px;
      margin-top: 16px;
    }
    .preview-status {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 0.8rem;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--text-muted);
    }
    .status-dot.active { background: var(--success); animation: pulse 1.5s infinite; }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
    .preview-spacer { flex: 1; }

    /* Modals */
    .modal-overlay {
      position: fixed;
      inset: 0;
      background: rgba(0,0,0,0.8);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 1000;
    }
    .modal-overlay.show { display: flex; }
    .modal {
      background: var(--bg-tertiary);
      border-radius: 12px;
      padding: 24px;
      width: 90%;
      max-width: 380px;
    }
    .modal h3 { font-size: 1.1rem; margin-bottom: 16px; }
    .modal-input {
      width: 100%;
      padding: 10px 14px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-primary);
      font-size: 0.9rem;
      margin-bottom: 16px;
    }
    .modal-input:focus { outline: none; border-color: var(--accent); }
    .modal-actions { display: flex; gap: 8px; justify-content: flex-end; }

    /* Toast */
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
      animation: slideIn 0.3s ease;
    }
    .toast.success { background: var(--success); }
    .toast.error { background: var(--danger); }
    .toast.info { background: #3b82f6; }
    @keyframes slideIn { from { transform: translateY(20px); opacity: 0; } }

    /* Empty State */
    .empty-state {
      text-align: center;
      padding: 30px 16px;
      color: var(--text-secondary);
    }
    .empty-state .icon { font-size: 32px; margin-bottom: 8px; opacity: 0.5; font-family: monospace; }
    .empty-state p { font-size: 0.85rem; }
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
      <a href="/advanced" class="tab">Advanced</a>
      <a href="/system" class="tab">System</a>
      <a href="/scene" class="tab active">Scenes</a>
      <a href="/sprites" class="tab">Sprites</a>
      <a href="/settings" class="tab">Settings</a>
    </nav>
    
    <section class="tab-content active">
      <!-- Scene Selector Bar -->
      <div class="scene-bar">
        <label>Scene:</label>
        <select class="scene-select" id="sceneSelect" onchange="onSceneChange()">
          <option value="">-- Select Scene --</option>
        </select>
        <div class="scene-actions">
          <button class="btn btn-primary btn-sm" onclick="showNewSceneModal()">+ New</button>
          <button class="btn btn-secondary btn-sm" onclick="showRenameModal()" id="btnRename" disabled>Rename</button>
          <button class="btn btn-danger btn-sm" onclick="showDeleteModal()" id="btnDelete" disabled>Delete</button>
        </div>
      </div>

      <!-- Configuration Grid -->
      <div class="config-grid">
        <!-- Left Column: Animation Type & Parameters -->
        <div>
          <div class="panel">
            <div class="panel-header">
              <h3><span class="icon">&#x25B6;</span> Animation Type</h3>
            </div>
            <div class="panel-body">
              <div class="anim-type-grid" id="animTypeGrid"></div>
            </div>
          </div>

          <div class="panel" style="margin-top: 16px;">
            <div class="panel-header">
              <h3><span class="icon">&#x2699;</span> Animation Settings</h3>
              <button class="btn btn-secondary btn-sm" onclick="resetParams()">Reset</button>
            </div>
            <div class="panel-body" id="paramsContainer">
              <div class="empty-state">
                <div class="icon">&#x2630;</div>
                <p>Select an animation type to configure</p>
              </div>
            </div>
          </div>

          <div class="panel" style="margin-top: 16px;">
            <div class="panel-header">
              <h3><span class="icon">&#x1F308;</span> Shader</h3>
            </div>
            <div class="panel-body">
              <p style="font-size:0.75rem;color:var(--text-secondary);margin-bottom:10px;">
                Post-processing effect applied to the display.
              </p>
              <select class="scene-select" id="shaderSelect" onchange="onShaderChange()" style="width:100%;margin-bottom:12px;">
                <option value="none">None</option>
                <option value="hue_cycle">RGB Hue Cycle</option>
                <option value="glitch">Glitch</option>
                <option value="hue_gradient">Hue Gradient</option>
              </select>
              <div id="shaderParamsContainer">
                <!-- Shader-specific parameters appear here -->
              </div>
            </div>
          </div>

          <div class="panel" style="margin-top: 16px;">
            <div class="panel-header">
              <h3><span class="icon">&#x2736;</span> Effects</h3>
            </div>
            <div class="panel-body" id="effectsContainer">
              <!-- Effects will be rendered here -->
            </div>
          </div>
        </div>

        <!-- Right Column: Transition & Sprites -->
        <div>
          <div class="panel">
            <div class="panel-header">
              <h3><span class="icon">&#x21C4;</span> Transition</h3>
            </div>
            <div class="panel-body">
              <p style="font-size:0.75rem;color:var(--text-secondary);margin-bottom:10px;">
                Plays when switching to this scene.
              </p>
              <div class="transition-grid" id="transitionGrid"></div>
            </div>
          </div>

          <div class="panel" style="margin-top: 16px;">
            <div class="panel-header">
              <h3><span class="icon">&#x25A0;</span> Sprite</h3>
              <button class="btn btn-secondary btn-sm" onclick="clearSprite()">Clear</button>
            </div>
            <div class="panel-body">
              <p style="font-size:0.75rem;color:var(--text-secondary);margin-bottom:10px;">
                Select a saved sprite to display.
              </p>
              <div class="sprite-grid" id="spriteGrid">
                <div class="empty-state" style="grid-column:1/-1;padding:16px;">
                  <p>No sprites saved</p>
                </div>
              </div>
              <div class="mirror-row">
                <div>
                  <div class="label">Mirror to Right Panel</div>
                  <div class="hint">Flip sprite on right display</div>
                </div>
                <label class="toggle-switch">
                  <input type="checkbox" id="mirrorSprite" onchange="onMirrorChange()">
                  <span class="toggle-slider"></span>
                </label>
              </div>
            </div>
          </div>
        </div>
      </div>

      <!-- Preview Bar -->
      <div class="preview-bar">
        <div class="preview-status">
          <span class="status-dot" id="statusDot"></span>
          <span id="statusText">Stopped</span>
        </div>
        <div class="preview-spacer"></div>
        <button class="btn btn-primary" id="btnPreview" onclick="togglePreview()">&#x25B6; Preview</button>
        <button class="btn btn-secondary" onclick="stopPreview()">&#x25A0; Stop</button>
        <button class="btn btn-success" onclick="saveScene()">&#x2713; Save</button>
      </div>
    </section>
    
    <footer>
      <p>Lucidius - ARCOS Framework</p>
    </footer>
  </div>

  <!-- New Scene Modal -->
  <div class="modal-overlay" id="modalNew">
    <div class="modal">
      <h3>Create New Scene</h3>
      <input type="text" class="modal-input" id="newSceneName" placeholder="Enter scene name..." maxlength="32">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalNew')">Cancel</button>
        <button class="btn btn-primary" onclick="createScene()">Create</button>
      </div>
    </div>
  </div>

  <!-- Rename Modal -->
  <div class="modal-overlay" id="modalRename">
    <div class="modal">
      <h3>Rename Scene</h3>
      <input type="text" class="modal-input" id="renameSceneName" placeholder="New name..." maxlength="32">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalRename')">Cancel</button>
        <button class="btn btn-primary" onclick="confirmRename()">Rename</button>
      </div>
    </div>
  </div>

  <!-- Delete Modal -->
  <div class="modal-overlay" id="modalDelete">
    <div class="modal">
      <h3>Delete Scene?</h3>
      <p style="color:var(--text-secondary);margin-bottom:16px;">This action cannot be undone.</p>
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalDelete')">Cancel</button>
        <button class="btn btn-danger" onclick="confirmDelete()">Delete</button>
      </div>
    </div>
  </div>

  <div id="toast-container"></div>

  <script>
    // ====== DATA (loaded dynamically from backend) ======
    let ANIMATION_TYPES = [];
    let TRANSITION_TYPES = [];
    let SHADER_TYPES = [];
    
    // Fallback animation types if API fails
    const FALLBACK_ANIMATION_TYPES = [
      { id: 'static_sprite', name: 'Static Sprite', icon: '&#x25A0;', desc: 'Display a sprite at fixed position' },
      { id: 'static_mirrored', name: 'Static Mirrored', icon: '&#x25A3;', desc: 'Display sprites on both panels with independent controls' }
    ];
    
    // Fallback transition types if API fails
    const FALLBACK_TRANSITION_TYPES = [
      { id: 'none', name: 'None', icon: '&#x2192;' },
      { id: 'fade', name: 'Fade', icon: '&#x25D0;' }
    ];
    
    // Fallback shader types if API fails
    const FALLBACK_SHADER_TYPES = [
      { id: 'none', name: 'None', params: [] }
    ];

    const EFFECTS = [
      { id: 'glitch', name: 'Glitch Effect', icon: '&#x2593;', hasIntensity: true },
      { id: 'scanlines', name: 'Scanlines', icon: '&#x2261;', hasIntensity: true },
      { id: 'color_shift', name: 'Color Shift', icon: '&#x25D0;', hasIntensity: true }
    ];

    // Animation-specific parameters
    const ANIM_PARAMS = {
      static_sprite: [
        { section: 'Position', params: [
          { id: 'x', name: 'X Position', type: 'slider', min: 0, max: 128, step: 1, default: 64 },
          { id: 'y', name: 'Y Position', type: 'slider', min: 0, max: 32, step: 1, default: 16 },
          { id: 'rotation', name: 'Rotation', type: 'slider', min: 0, max: 360, step: 1, default: 0 },
          { id: 'scale', name: 'Scale', type: 'slider', min: 0.1, max: 4.0, step: 0.1, default: 1.0 }
        ]}
      ],
      static_mirrored: [
        { section: 'Left Panel (0-63)', params: [
          { id: 'left_x', name: 'Left X', type: 'slider', min: 0, max: 64, step: 1, default: 32 },
          { id: 'left_y', name: 'Left Y', type: 'slider', min: 0, max: 32, step: 1, default: 16 },
          { id: 'left_rotation', name: 'Left Rotation', type: 'slider', min: 0, max: 360, step: 1, default: 0 },
          { id: 'left_scale', name: 'Left Scale', type: 'slider', min: 0.1, max: 4.0, step: 0.1, default: 1.0 }
        ]},
        { section: 'Right Panel (64-127)', params: [
          { id: 'right_x', name: 'Right X', type: 'slider', min: 64, max: 128, step: 1, default: 96 },
          { id: 'right_y', name: 'Right Y', type: 'slider', min: 0, max: 32, step: 1, default: 16 },
          { id: 'right_rotation', name: 'Right Rotation', type: 'slider', min: 0, max: 360, step: 1, default: 180 },
          { id: 'right_scale', name: 'Right Scale', type: 'slider', min: 0.1, max: 4.0, step: 0.1, default: 1.0 }
        ]}
      ]
    };

    // State
    let scenes = [];
    let sprites = [];
    let currentSceneId = null;
    let currentScene = {
      name: '',
      animType: null,
      transition: 'none',
      shader: 'none',
      shaderParams: {},
      spriteId: null,
      mirrorSprite: false,
      params: {},
      effects: {}
    };
    let isPlaying = false;

    // ====== LOAD TYPES FROM BACKEND ======
    async function loadAnimationTypes() {
      try {
        const res = await fetch('/api/registry/animations');
        if (!res.ok) throw new Error('Failed to load animations');
        const data = await res.json();
        
        if (data.animations && data.animations.length > 0) {
          // Map backend format to frontend format with icons
          const iconMap = {
            'static_sprite': '&#x25A0;',
            'static_mirrored': '&#x25A3;',
            'default': '&#x25A1;'
          };
          ANIMATION_TYPES = data.animations.map(a => ({
            id: a.id,
            name: a.name,
            icon: iconMap[a.id] || iconMap.default,
            desc: a.description || ''
          }));
        } else {
          ANIMATION_TYPES = FALLBACK_ANIMATION_TYPES;
        }
      } catch (e) {
        console.warn('Could not load animations from API:', e);
        ANIMATION_TYPES = FALLBACK_ANIMATION_TYPES;
      }
    }
    
    async function loadTransitionTypes() {
      try {
        const res = await fetch('/api/registry/transitions');
        if (!res.ok) throw new Error('Failed to load transitions');
        const data = await res.json();
        
        if (data.transitions && data.transitions.length > 0) {
          TRANSITION_TYPES = data.transitions.map(t => ({
            id: t.id,
            name: t.name,
            icon: t.icon || '&#x2192;',
            params: t.params || []
          }));
        } else {
          TRANSITION_TYPES = FALLBACK_TRANSITION_TYPES;
        }
      } catch (e) {
        console.warn('Could not load transitions from API:', e);
        TRANSITION_TYPES = FALLBACK_TRANSITION_TYPES;
      }
    }
    
    async function loadShaderTypes() {
      try {
        const res = await fetch('/api/registry/shaders');
        if (!res.ok) throw new Error('Failed to load shaders');
        const data = await res.json();
        
        if (data.shaders && data.shaders.length > 0) {
          SHADER_TYPES = data.shaders.map(s => ({
            id: s.id,
            name: s.name,
            params: (s.params || []).map(p => ({
              id: p.id,
              name: p.name,
              type: p.type || 'range',
              min: p.min || 0,
              max: p.max || 1,
              step: (p.max - p.min) / 20 || 0.05,
              default: p.default || 0,
              hint: p.hint || p.description || ''
            }))
          }));
        } else {
          SHADER_TYPES = FALLBACK_SHADER_TYPES;
        }
      } catch (e) {
        console.warn('Could not load shaders from API:', e);
        SHADER_TYPES = FALLBACK_SHADER_TYPES;
      }
    }

    // ====== INITIALIZATION ======
    document.addEventListener('DOMContentLoaded', async () => {
      // Load types from backend first
      await Promise.all([
        loadAnimationTypes(),
        loadTransitionTypes(),
        loadShaderTypes()
      ]);
      
      // Then render UI
      renderAnimTypes();
      renderTransitions();
      renderEffects();
      renderShaderSelect();
      renderShaderParams();
      await loadScenes();
      await loadSprites();
    });

    // ====== RENDERING ======
    function renderAnimTypes() {
      const grid = document.getElementById('animTypeGrid');
      grid.innerHTML = ANIMATION_TYPES.map(a => `
        <div class="anim-type-card ${currentScene.animType === a.id ? 'selected' : ''}" 
             onclick="selectAnimType('${a.id}')">
          <div class="icon">${a.icon}</div>
          <div class="name">${a.name}</div>
          <div class="desc">${a.desc}</div>
        </div>
      `).join('');
    }

    function renderTransitions() {
      const grid = document.getElementById('transitionGrid');
      grid.innerHTML = TRANSITION_TYPES.map(t => `
        <div class="transition-card ${currentScene.transition === t.id ? 'selected' : ''}"
             onclick="selectTransition('${t.id}')">
          <div class="icon">${t.icon}</div>
          <div class="name">${t.name}</div>
        </div>
      `).join('');
    }

    function renderEffects() {
      const container = document.getElementById('effectsContainer');
      container.innerHTML = EFFECTS.map(e => {
        const enabled = currentScene.effects[e.id]?.enabled || false;
        const intensity = currentScene.effects[e.id]?.intensity ?? 0.5;
        return `
          <div class="effect-row">
            <div class="effect-info">
              <span class="icon">${e.icon}</span>
              <span class="name">${e.name}</span>
            </div>
            <div class="effect-control">
              ${e.hasIntensity ? `
                <input type="range" class="effect-slider" id="effect_${e.id}_intensity" 
                       min="0" max="1" step="0.05" value="${intensity}"
                       oninput="updateEffect('${e.id}', 'intensity', this.value)" ${!enabled ? 'disabled' : ''}>
                <span class="effect-value" id="val_effect_${e.id}">${(intensity * 100).toFixed(0)}%</span>
              ` : ''}
              <label class="toggle-switch">
                <input type="checkbox" id="effect_${e.id}" ${enabled ? 'checked' : ''} 
                       onchange="updateEffect('${e.id}', 'enabled', this.checked)">
                <span class="toggle-slider"></span>
              </label>
            </div>
          </div>
        `;
      }).join('');
    }

    function renderShaderSelect() {
      const select = document.getElementById('shaderSelect');
      if (!select) return;
      
      select.innerHTML = SHADER_TYPES.map(s => 
        `<option value="${s.id}" ${currentScene.shader === s.id ? 'selected' : ''}>${s.name}</option>`
      ).join('');
    }

    function renderParams() {
      const container = document.getElementById('paramsContainer');
      if (!currentScene.animType || !ANIM_PARAMS[currentScene.animType]) {
        container.innerHTML = `
          <div class="empty-state">
            <div class="icon">&#x2630;</div>
            <p>Select an animation type to configure</p>
          </div>`;
        return;
      }

      const sections = ANIM_PARAMS[currentScene.animType];
      container.innerHTML = sections.map(sec => `
        <div class="param-section">
          <div class="param-section-title">${sec.section}</div>
          ${sec.params.map(p => renderParamRow(p)).join('')}
        </div>
      `).join('');
    }

    function renderParamRow(p) {
      const value = currentScene.params[p.id] !== undefined ? currentScene.params[p.id] : p.default;
      let control = '';

      switch(p.type) {
        case 'slider':
          control = `
            <input type="range" id="param_${p.id}" min="${p.min}" max="${p.max}" step="${p.step}" 
                   value="${value}" oninput="updateParam('${p.id}', this.value, 'slider')">
            <span class="param-value" id="val_${p.id}">${Number(value).toFixed(p.step < 1 ? 1 : 0)}</span>`;
          break;
        case 'toggle':
          control = `
            <label class="toggle-switch">
              <input type="checkbox" id="param_${p.id}" ${value ? 'checked' : ''} 
                     onchange="updateParam('${p.id}', this.checked, 'toggle')">
              <span class="toggle-slider"></span>
            </label>`;
          break;
        case 'color':
          control = `<input type="color" id="param_${p.id}" value="${value}" 
                            onchange="updateParam('${p.id}', this.value, 'color')">`;
          break;
      }

      return `
        <div class="param-row">
          <label class="param-label">${p.name}</label>
          <div class="param-control">${control}</div>
        </div>`;
    }

    function renderSprites() {
      const grid = document.getElementById('spriteGrid');
      if (!sprites.length) {
        grid.innerHTML = `
          <div class="empty-state" style="grid-column:1/-1;padding:16px;">
            <p>No sprites saved. <a href="/sprites" style="color:var(--accent)">Upload some</a></p>
          </div>`;
        return;
      }

      grid.innerHTML = sprites.map(s => `
        <div class="sprite-item ${currentScene.spriteId === s.id ? 'selected' : ''}"
             onclick="selectSprite('${s.id}')" title="${esc(s.name)}">
          <img src="/api/sprite/thumb?id=${s.id}" alt="${esc(s.name)}" 
               onerror="this.style.display='none';this.nextElementSibling.style.display='block'">
          <span class="placeholder" style="display:none">&#x25A0;</span>
        </div>
      `).join('');
    }

    function renderSceneSelect() {
      const sel = document.getElementById('sceneSelect');
      sel.innerHTML = '<option value="">-- Select Scene --</option>' +
        scenes.map(s => `<option value="${s.id}" ${s.id === currentSceneId ? 'selected' : ''}>${esc(s.name)}</option>`).join('');
      
      document.getElementById('btnRename').disabled = !currentSceneId;
      document.getElementById('btnDelete').disabled = !currentSceneId;
    }

    // ====== EFFECTS ======
    function updateEffect(effectId, prop, value) {
      if (!currentScene.effects[effectId]) {
        currentScene.effects[effectId] = { enabled: false, intensity: 0.5 };
      }
      
      if (prop === 'enabled') {
        currentScene.effects[effectId].enabled = value;
        // Enable/disable the intensity slider
        const slider = document.getElementById(`effect_${effectId}_intensity`);
        if (slider) slider.disabled = !value;
      } else if (prop === 'intensity') {
        currentScene.effects[effectId].intensity = parseFloat(value);
        const valEl = document.getElementById(`val_effect_${effectId}`);
        if (valEl) valEl.textContent = (parseFloat(value) * 100).toFixed(0) + '%';
      }
      
      sendSceneUpdate();
    }

    // ====== API CALLS ======
    async function loadScenes() {
      try {
        const r = await fetch('/api/scenes');
        const d = await r.json();
        scenes = d.scenes || [];
        renderSceneSelect();
      } catch(e) {
        toast('Failed to load scenes', 'error');
      }
    }

    async function loadSprites() {
      try {
        const r = await fetch('/api/sprites');
        const d = await r.json();
        sprites = d.sprites || [];
        renderSprites();
      } catch(e) {
        console.log('Failed to load sprites');
      }
    }

    async function loadSceneData(id) {
      try {
        const r = await fetch(`/api/scene/get?id=${id}`);
        const d = await r.json();
        if (d.scene) {
          currentScene = {
            name: d.scene.name || '',
            animType: d.scene.animType || null,
            transition: d.scene.transition || 'none',
            shader: d.scene.shader || 'none',
            spriteId: d.scene.spriteId || null,
            mirrorSprite: d.scene.mirrorSprite || false,
            params: d.scene.params || {},
            shaderParams: d.scene.shaderParams || {},
            effects: d.scene.effects || {}
          };
          renderAnimTypes();
          renderTransitions();
          renderParams();
          renderSprites();
          renderEffects();
          document.getElementById('mirrorSprite').checked = currentScene.mirrorSprite;
          document.getElementById('shaderSelect').value = currentScene.shader;
          renderShaderParams();
        }
      } catch(e) {
        toast('Failed to load scene', 'error');
      }
    }

    // ====== EVENT HANDLERS ======
    function onSceneChange() {
      const sel = document.getElementById('sceneSelect');
      currentSceneId = sel.value || null;
      
      if (currentSceneId) {
        loadSceneData(currentSceneId);
      } else {
        currentScene = { name: '', animType: null, transition: 'none', shader: 'none', spriteId: null, mirrorSprite: false, params: {}, shaderParams: {}, effects: {} };
        renderAnimTypes();
        renderTransitions();
        renderParams();
        renderSprites();
        renderEffects();
        document.getElementById('shaderSelect').value = 'none';
        renderShaderParams();
      }
      
      document.getElementById('btnRename').disabled = !currentSceneId;
      document.getElementById('btnDelete').disabled = !currentSceneId;
    }

    function selectAnimType(id) {
      currentScene.animType = id;
      currentScene.params = {};  // Reset params for new type
      renderAnimTypes();
      renderParams();
      sendSceneUpdate();
    }

    function selectTransition(id) {
      currentScene.transition = id;
      renderTransitions();
      sendSceneUpdate();
    }

    function selectSprite(id) {
      currentScene.spriteId = currentScene.spriteId === id ? null : id;
      renderSprites();
      sendSceneUpdate();
    }

    function clearSprite() {
      currentScene.spriteId = null;
      renderSprites();
      sendSceneUpdate();
    }

    function onMirrorChange() {
      currentScene.mirrorSprite = document.getElementById('mirrorSprite').checked;
      sendSceneUpdate();
    }

    // ====== SHADER HANDLING ======
    function onShaderChange() {
      const select = document.getElementById('shaderSelect');
      currentScene.shader = select.value;
      // Reset shader params to defaults
      const shaderDef = SHADER_TYPES.find(s => s.id === currentScene.shader);
      currentScene.shaderParams = {};
      if (shaderDef && shaderDef.params) {
        shaderDef.params.forEach(p => {
          currentScene.shaderParams[p.id] = p.default;
        });
      }
      renderShaderParams();
      sendSceneUpdate();
    }

    function renderShaderParams() {
      const container = document.getElementById('shaderParamsContainer');
      const shaderDef = SHADER_TYPES.find(s => s.id === currentScene.shader);
      
      if (!shaderDef || !shaderDef.params || shaderDef.params.length === 0) {
        container.innerHTML = '';
        return;
      }
      
      container.innerHTML = shaderDef.params.map(p => {
        const val = currentScene.shaderParams[p.id] ?? p.default;
        return `
          <div class="param-row">
            <div class="param-info">
              <div class="label">${p.name}</div>
              <div class="hint">${p.hint || ''}</div>
            </div>
            <div class="param-control">
              <input type="range" class="slider" 
                     min="${p.min}" max="${p.max}" step="${p.step}" 
                     value="${val}"
                     oninput="updateShaderParam('${p.id}', this.value)">
              <span class="param-value" id="shader_val_${p.id}">${Number(val).toFixed(1)}</span>
            </div>
          </div>
        `;
      }).join('');
    }

    function updateShaderParam(id, value) {
      currentScene.shaderParams[id] = parseFloat(value);
      const valEl = document.getElementById(`shader_val_${id}`);
      if (valEl) valEl.textContent = Number(value).toFixed(1);
      sendSceneUpdate();
    }

    function updateParam(id, value, type) {
      if (type === 'slider') {
        currentScene.params[id] = parseFloat(value);
        const valEl = document.getElementById(`val_${id}`);
        if (valEl) valEl.textContent = Number(value).toFixed(value % 1 ? 1 : 0);
      } else if (type === 'toggle') {
        currentScene.params[id] = value;
      } else if (type === 'color') {
        currentScene.params[id] = value;
      }
      sendParamUpdate(id, currentScene.params[id]);
    }

    async function sendParamUpdate(paramId, value) {
      if (!currentScene.animType) return;
      try {
        await fetch('/api/scene/param', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            sceneId: currentSceneId,
            animType: currentScene.animType,
            param: paramId,
            value: value
          })
        });
      } catch(e) {}
    }

    async function sendSceneUpdate() {
      if (!currentSceneId) return;
      try {
        await fetch('/api/scene/update', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            id: parseInt(currentSceneId),
            animType: currentScene.animType,
            transition: currentScene.transition,
            shader: currentScene.shader,
            spriteId: currentScene.spriteId,
            mirrorSprite: currentScene.mirrorSprite,
            params: currentScene.params,
            shaderParams: currentScene.shaderParams,
            effects: currentScene.effects
          })
        });
      } catch(e) {}
    }

    function resetParams() {
      if (!currentScene.animType) return;
      currentScene.params = {};
      renderParams();
      toast('Parameters reset', 'info');
    }

    // ====== SCENE CRUD ======
    function showNewSceneModal() {
      document.getElementById('newSceneName').value = '';
      showModal('modalNew');
      document.getElementById('newSceneName').focus();
    }

    async function createScene() {
      const name = document.getElementById('newSceneName').value.trim();
      if (!name) { toast('Enter a name', 'error'); return; }

      try {
        const r = await fetch('/api/scene/create', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ name, type: 0 })
        });
        const d = await r.json();
        if (d.success) {
          hideModal('modalNew');
          toast('Scene created!', 'success');
          await loadScenes();
          currentSceneId = String(d.id);
          document.getElementById('sceneSelect').value = currentSceneId;
          onSceneChange();
        } else {
          toast(d.error || 'Failed to create', 'error');
        }
      } catch(e) {
        toast('Error creating scene', 'error');
      }
    }

    function showRenameModal() {
      if (!currentSceneId) return;
      const scene = scenes.find(s => s.id === currentSceneId);
      document.getElementById('renameSceneName').value = scene ? scene.name : '';
      showModal('modalRename');
      document.getElementById('renameSceneName').focus();
    }

    async function confirmRename() {
      const name = document.getElementById('renameSceneName').value.trim();
      if (!name) { toast('Enter a name', 'error'); return; }

      try {
        const r = await fetch('/api/scene/rename', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id: parseInt(currentSceneId), name })
        });
        const d = await r.json();
        if (d.success) {
          hideModal('modalRename');
          toast('Renamed!', 'success');
          await loadScenes();
        } else {
          toast(d.error || 'Failed', 'error');
        }
      } catch(e) {
        toast('Error', 'error');
      }
    }

    function showDeleteModal() {
      if (!currentSceneId) return;
      showModal('modalDelete');
    }

    async function confirmDelete() {
      try {
        const r = await fetch('/api/scene/delete', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id: parseInt(currentSceneId) })
        });
        const d = await r.json();
        if (d.success) {
          hideModal('modalDelete');
          toast('Deleted', 'success');
          currentSceneId = null;
          currentScene = { name: '', animType: null, transition: 'none', spriteId: null, mirrorSprite: false, params: {}, effects: {} };
          await loadScenes();
          renderAnimTypes();
          renderTransitions();
          renderParams();
          renderEffects();
        } else {
          toast(d.error || 'Failed', 'error');
        }
      } catch(e) {
        toast('Error', 'error');
      }
    }

    async function saveScene() {
      if (!currentSceneId) {
        showNewSceneModal();
        return;
      }

      try {
        const r = await fetch('/api/scene/save', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            id: parseInt(currentSceneId),
            animType: currentScene.animType,
            transition: currentScene.transition,
            spriteId: currentScene.spriteId,
            mirrorSprite: currentScene.mirrorSprite,
            params: currentScene.params,
            effects: currentScene.effects
          })
        });
        const d = await r.json();
        if (d.success) {
          toast('Scene saved!', 'success');
        } else {
          toast(d.error || 'Failed to save', 'error');
        }
      } catch(e) {
        toast('Error saving', 'error');
      }
    }

    // ====== PREVIEW ======
    async function togglePreview() {
      if (!currentScene.animType) {
        toast('Select an animation type', 'error');
        return;
      }

      isPlaying = !isPlaying;
      const endpoint = isPlaying ? '/api/scene/preview' : '/api/scene/stop';
      
      try {
        await fetch(endpoint, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            animType: currentScene.animType,
            transition: currentScene.transition,
            spriteId: currentScene.spriteId,
            mirrorSprite: currentScene.mirrorSprite,
            params: currentScene.params,
            effects: currentScene.effects
          })
        });
        updatePreviewUI();
      } catch(e) {
        toast('Preview error', 'error');
      }
    }

    async function stopPreview() {
      isPlaying = false;
      try {
        await fetch('/api/scene/stop', { method: 'POST' });
      } catch(e) {}
      updatePreviewUI();
    }

    function updatePreviewUI() {
      const btn = document.getElementById('btnPreview');
      const dot = document.getElementById('statusDot');
      const text = document.getElementById('statusText');
      
      btn.innerHTML = isPlaying ? '&#x2016; Pause' : '&#x25B6; Preview';
      dot.classList.toggle('active', isPlaying);
      text.textContent = isPlaying ? 'Playing' : 'Stopped';
    }

    // ====== UTILITIES ======
    function showModal(id) { document.getElementById(id).classList.add('show'); }
    function hideModal(id) { document.getElementById(id).classList.remove('show'); }
    
    function toast(msg, type = 'success') {
      const t = document.createElement('div');
      t.className = `toast ${type}`;
      t.textContent = msg;
      document.body.appendChild(t);
      setTimeout(() => t.remove(), 3000);
    }
    
    function esc(s) {
      const d = document.createElement('div');
      d.textContent = s;
      return d.innerHTML;
    }

    // Enter key handlers
    document.getElementById('newSceneName').addEventListener('keypress', e => { if(e.key === 'Enter') createScene(); });
    document.getElementById('renameSceneName').addEventListener('keypress', e => { if(e.key === 'Enter') confirmRename(); });
  </script>
</body>
</html>
)rawpage";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
