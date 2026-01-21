/*****************************************************************
 * CPU_WebConfigTest.cpp - Auto-Generated Web Configuration System
 *
 * This test demonstrates:
 * 1. Auto-generated web variables that control animations
 * 2. Scene management (create, remove, edit scenes)
 * 3. Sprite library with upload and selection
 * 4. Real-time parameter updates via web
 *
 * Connect to WiFi: "ConfigTest-AP" (no password)
 * Open browser: http://192.168.4.1
 *****************************************************************/

#include "SystemAPI/GPU/GpuDriver.h"
#include "SystemAPI/Web/Server/WifiManager.hpp"
#include "SystemAPI/Web/Server/DnsServer.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "Drivers/ImuDriver.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <algorithm>

static const char* TAG = "WEB_CONFIG_TEST";

using namespace SystemAPI;
using namespace SystemAPI::Web;

// ============================================================================
// CONFIG PARAMETER SYSTEM - Auto-generates web UI from variable definitions
// ============================================================================

enum class ParamType {
    SLIDER,     // Range slider with min/max/step
    DROPDOWN,   // Select from predefined options
    TOGGLE,     // On/off switch
    COLOR       // Color picker
};

struct DropdownOption {
    std::string id;
    std::string label;
};

struct ConfigParam {
    std::string id;           // Unique identifier
    std::string name;         // Display name
    std::string group;        // Group/section name
    ParamType type;
    
    // For SLIDER
    float minVal = 0;
    float maxVal = 100;
    float step = 1;
    float value = 50;
    
    // For DROPDOWN
    std::vector<DropdownOption> options;
    std::string selectedOption;
    
    // For TOGGLE
    bool enabled = false;
    
    // For COLOR
    uint8_t r = 255, g = 255, b = 255;
    
    // Callback when value changes
    std::function<void()> onChange;
};

// ============================================================================
// SPRITE LIBRARY SYSTEM
// ============================================================================

struct SavedSprite {
    int id;
    std::string name;
    int width;
    int height;
    std::vector<uint8_t> pixels;  // RGB888 data
    uint8_t gpuSlot;              // GPU sprite slot (0-15)
    bool uploaded;                // Whether uploaded to GPU
};

static std::vector<SavedSprite> g_sprites;
static int g_nextSpriteId = 1;
static uint8_t g_nextGpuSlot = 0;  // Start from 0

// ============================================================================
// SCENE SYSTEM
// ============================================================================

struct Scene {
    int id;
    std::string name;
    std::string animationType;  // Which animation this scene uses
    std::map<std::string, float> paramValues;  // Parameter overrides
    std::map<std::string, std::string> paramStrings;  // For dropdowns
    std::map<std::string, bool> paramBools;    // For toggles
};

// ============================================================================
// GLOBAL STATE
// ============================================================================

static GpuDriver g_gpu;
static httpd_handle_t g_server = nullptr;

// Config parameters - auto-generate web UI from these
static std::vector<ConfigParam> g_params;

// Scene management
static std::vector<Scene> g_scenes;
static int g_nextSceneId = 1;
static int g_activeSceneId = -1;

// Animation types available
static std::vector<std::string> g_animationTypes = {"gyro_eyes", "static_image"};

// IMU status
static bool g_imuInitialized = false;

// Gyro eye animation state
struct GyroEyeState {
    float leftX = 32.0f, leftY = 16.0f;
    float rightX = 32.0f, rightY = 16.0f;
    
    // Configurable params (bound to web UI)
    float eyeSize = 12.0f;
    int eyeShape = 0;         // 0=circle, 1=square, 2=diamond, 3+=sprite
    int eyeSpriteId = -1;     // If using sprite, which sprite ID
    float jiggleMultiplier = 1.0f;
    bool useSprite = false;   // Whether to use sprite instead of shape
    
    // Gyro smoothing
    static constexpr int WINDOW = 5;
    float histX[WINDOW] = {0}, histY[WINDOW] = {0}, histZ[WINDOW] = {0};
    float sumX = 0, sumY = 0, sumZ = 0;
    int idx = 0;
    
    void update(float gx, float gy, float gz) {
        // Rolling average for smoothing
        sumX -= histX[idx]; sumY -= histY[idx]; sumZ -= histZ[idx];
        histX[idx] = gx; histY[idx] = gy; histZ[idx] = gz;
        sumX += gx; sumY += gy; sumZ += gz;
        idx = (idx + 1) % WINDOW;
        
        float avgX = sumX / WINDOW;
        float avgY = sumY / WINDOW;
        float avgZ = sumZ / WINDOW;
        
        // Map gyro data to eye position
        // Gyro values are in degrees/second, we accumulate for position
        float scale = 0.15f * jiggleMultiplier;
        
        // Map to eye offset (clamp to reasonable range)
        float eyeOffsetX = avgZ * scale;  // Z axis -> left/right
        float eyeOffsetY = avgY * scale;  // Y axis -> up/down
        
        // Clamp to panel bounds
        float maxOffset = 14.0f;
        eyeOffsetX = fmaxf(-maxOffset, fminf(maxOffset, eyeOffsetX));
        eyeOffsetY = fmaxf(-maxOffset, fminf(maxOffset, eyeOffsetY));
        
        // Both eyes move together
        leftX = 32.0f + eyeOffsetX;
        leftY = 16.0f + eyeOffsetY;
        rightX = 32.0f + eyeOffsetX;
        rightY = 16.0f + eyeOffsetY;
    }
} g_gyroEyes;

// ============================================================================
// PARAMETER REGISTRATION - Define what shows up in the web UI
// ============================================================================

// Helper to rebuild eye shape dropdown with sprites
void updateEyeShapeOptions() {
    for (auto& p : g_params) {
        if (p.id == "eye_shape") {
            p.options.clear();
            p.options.push_back({"circle", "Circle"});
            p.options.push_back({"square", "Square"});
            p.options.push_back({"diamond", "Diamond"});
            
            // Add saved sprites as options
            for (const auto& sprite : g_sprites) {
                p.options.push_back({"sprite_" + std::to_string(sprite.id), sprite.name});
            }
            break;
        }
    }
}

void registerGyroEyeParams() {
    // Eye Size slider
    ConfigParam sizeParam;
    sizeParam.id = "eye_size";
    sizeParam.name = "Eye Size";
    sizeParam.group = "Gyro Eyes";
    sizeParam.type = ParamType::SLIDER;
    sizeParam.minVal = 4;
    sizeParam.maxVal = 20;
    sizeParam.step = 1;
    sizeParam.value = 12;
    sizeParam.onChange = []() {
        for (auto& p : g_params) {
            if (p.id == "eye_size") {
                g_gyroEyes.eyeSize = p.value;
                break;
            }
        }
    };
    g_params.push_back(sizeParam);
    
    // Eye Shape/Sprite dropdown
    ConfigParam shapeParam;
    shapeParam.id = "eye_shape";
    shapeParam.name = "Eye Shape";
    shapeParam.group = "Gyro Eyes";
    shapeParam.type = ParamType::DROPDOWN;
    shapeParam.options = {
        {"circle", "Circle"},
        {"square", "Square"},
        {"diamond", "Diamond"}
    };
    shapeParam.selectedOption = "circle";
    shapeParam.onChange = []() {
        for (auto& p : g_params) {
            if (p.id == "eye_shape") {
                ESP_LOGI("SHAPE", "Shape callback: selectedOption='%s'", p.selectedOption.c_str());
                
                // Check if it's a sprite selection
                if (p.selectedOption.find("sprite_") == 0) {
                    // Extract sprite ID
                    std::string idStr = p.selectedOption.substr(7);
                    int spriteId = std::stoi(idStr);
                    g_gyroEyes.useSprite = true;
                    g_gyroEyes.eyeSpriteId = spriteId;
                    ESP_LOGI("SHAPE", "Using sprite ID %d", spriteId);
                } else {
                    // Shape-based rendering
                    g_gyroEyes.useSprite = false;
                    g_gyroEyes.eyeSpriteId = -1;
                    
                    if (p.selectedOption == "circle" || p.selectedOption == "0") g_gyroEyes.eyeShape = 0;
                    else if (p.selectedOption == "square" || p.selectedOption == "1") g_gyroEyes.eyeShape = 1;
                    else if (p.selectedOption == "diamond" || p.selectedOption == "2") g_gyroEyes.eyeShape = 2;
                }
                ESP_LOGI("SHAPE", "useSprite=%d, eyeShape=%d", g_gyroEyes.useSprite, g_gyroEyes.eyeShape);
                break;
            }
        }
    };
    g_params.push_back(shapeParam);
    
    // Jiggle Multiplier slider
    ConfigParam jiggleParam;
    jiggleParam.id = "jiggle_mult";
    jiggleParam.name = "Jiggle Multiplier";
    jiggleParam.group = "Gyro Eyes";
    jiggleParam.type = ParamType::SLIDER;
    jiggleParam.minVal = 0.1f;
    jiggleParam.maxVal = 3.0f;
    jiggleParam.step = 0.1f;
    jiggleParam.value = 1.0f;
    jiggleParam.onChange = []() {
        for (auto& p : g_params) {
            if (p.id == "jiggle_mult") {
                g_gyroEyes.jiggleMultiplier = p.value;
                break;
            }
        }
    };
    g_params.push_back(jiggleParam);
}

// ============================================================================
// WEB PAGE - Auto-generated from parameters
// ============================================================================

static const char* HTML_TEMPLATE = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Scene Manager</title>
  <style>
    :root {
      --bg-primary: #0a0a0f;
      --bg-secondary: #12121a;
      --bg-tertiary: #1a1a24;
      --border: #2a2a3a;
      --accent: #ff6b00;
      --text-primary: #ffffff;
      --text-secondary: #888899;
      --success: #00cc66;
      --danger: #ff4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: var(--bg-primary);
      color: var(--text-primary);
      min-height: 100vh;
    }
    .container { max-width: 800px; margin: 0 auto; padding: 12px; }
    
    header {
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 12px 0;
      border-bottom: 1px solid var(--border);
      margin-bottom: 16px;
    }
    .logo { font-size: 20px; color: var(--accent); }
    h1 { font-size: 1.1rem; font-weight: 600; }
    #imuStatus {
      margin-left: auto;
      font-size: 0.65rem;
      padding: 4px 8px;
      border-radius: 4px;
      background: var(--bg-tertiary);
      color: var(--text-secondary);
    }
    
    .tabs {
      display: flex;
      gap: 4px;
      margin-bottom: 16px;
      background: var(--bg-secondary);
      padding: 4px;
      border-radius: 8px;
    }
    .tab {
      flex: 1;
      padding: 10px;
      background: transparent;
      border: none;
      border-radius: 6px;
      color: var(--text-secondary);
      font-size: 0.8rem;
      font-weight: 600;
      cursor: pointer;
    }
    .tab:hover { color: var(--text-primary); }
    .tab.active { background: var(--accent); color: #fff; }
    
    .card {
      background: var(--bg-secondary);
      border-radius: 10px;
      margin-bottom: 12px;
      overflow: hidden;
    }
    .card-header {
      padding: 12px 14px;
      border-bottom: 1px solid var(--border);
      font-weight: 600;
      font-size: 0.85rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .card-body { padding: 14px; }
    
    .btn {
      padding: 8px 14px;
      border: none;
      border-radius: 6px;
      font-size: 0.75rem;
      font-weight: 600;
      cursor: pointer;
    }
    .btn:hover { filter: brightness(1.1); }
    .btn-primary { background: var(--accent); color: #fff; }
    .btn-secondary { background: var(--bg-tertiary); color: var(--text-primary); border: 1px solid var(--border); }
    .btn-danger { background: var(--danger); color: #fff; }
    .btn-success { background: var(--success); color: #fff; }
    .btn-sm { padding: 6px 10px; font-size: 0.7rem; }
    
    .scene-row {
      display: flex;
      gap: 8px;
      align-items: center;
      flex-wrap: wrap;
    }
    .scene-select {
      flex: 1;
      min-width: 120px;
      padding: 10px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-primary);
      font-size: 0.85rem;
    }
    
    .param-group { margin-bottom: 12px; }
    .param-group-title {
      font-size: 0.65rem;
      font-weight: 700;
      color: var(--accent);
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 8px;
      padding-bottom: 6px;
      border-bottom: 1px solid var(--border);
    }
    .param-row {
      display: flex;
      align-items: center;
      padding: 8px 0;
      border-bottom: 1px solid var(--bg-tertiary);
    }
    .param-row:last-child { border-bottom: none; }
    .param-label { flex: 0 0 38%; font-size: 0.8rem; }
    .param-control { flex: 1; display: flex; align-items: center; gap: 8px; }
    .param-value {
      min-width: 40px;
      text-align: right;
      font-size: 0.7rem;
      color: var(--text-secondary);
      font-family: monospace;
    }
    
    input[type="range"] {
      flex: 1;
      height: 6px;
      -webkit-appearance: none;
      background: var(--bg-tertiary);
      border-radius: 3px;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 16px;
      height: 16px;
      background: var(--accent);
      border-radius: 50%;
      cursor: pointer;
    }
    
    .param-select {
      flex: 1;
      padding: 8px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-primary);
      font-size: 0.8rem;
    }
    
    .anim-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; }
    .anim-card {
      background: var(--bg-tertiary);
      border: 2px solid transparent;
      border-radius: 8px;
      padding: 14px;
      text-align: center;
      cursor: pointer;
    }
    .anim-card:hover { border-color: var(--border); }
    .anim-card.selected { border-color: var(--accent); background: rgba(255,107,0,0.1); }
    .anim-card .icon { font-size: 20px; margin-bottom: 6px; }
    .anim-card .name { font-size: 0.8rem; font-weight: 600; }
    
    /* Sprite Library */
    .sprite-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(80px, 1fr)); gap: 10px; }
    .sprite-item {
      background: var(--bg-tertiary);
      border: 2px solid transparent;
      border-radius: 8px;
      padding: 8px;
      text-align: center;
      cursor: pointer;
      position: relative;
    }
    .sprite-item:hover { border-color: var(--border); }
    .sprite-item.selected { border-color: var(--accent); }
    .sprite-preview {
      width: 100%;
      aspect-ratio: 1;
      background: #000;
      border-radius: 4px;
      margin-bottom: 6px;
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    }
    .sprite-preview img, .sprite-preview canvas {
      image-rendering: pixelated;
      max-width: 100%;
      max-height: 100%;
    }
    .sprite-name { font-size: 0.7rem; word-break: break-all; }
    .sprite-delete {
      position: absolute;
      top: 4px;
      right: 4px;
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: var(--danger);
      color: #fff;
      border: none;
      font-size: 10px;
      cursor: pointer;
      display: none;
    }
    .sprite-item:hover .sprite-delete { display: block; }
    
    .upload-zone {
      border: 2px dashed var(--border);
      border-radius: 8px;
      padding: 20px;
      text-align: center;
      cursor: pointer;
      transition: all 0.2s;
    }
    .upload-zone:hover { border-color: var(--accent); background: rgba(255,107,0,0.05); }
    .upload-zone .icon { font-size: 24px; margin-bottom: 8px; opacity: 0.6; }
    .upload-zone p { font-size: 0.8rem; color: var(--text-secondary); }
    
    .modal-overlay {
      position: fixed;
      inset: 0;
      background: rgba(0,0,0,0.85);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 1000;
    }
    .modal-overlay.show { display: flex; }
    .modal {
      background: var(--bg-secondary);
      border-radius: 12px;
      padding: 20px;
      width: 90%;
      max-width: 320px;
    }
    .modal h3 { margin-bottom: 14px; font-size: 1rem; }
    .modal-input {
      width: 100%;
      padding: 10px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-primary);
      font-size: 0.85rem;
      margin-bottom: 14px;
    }
    .modal-actions { display: flex; gap: 8px; justify-content: flex-end; }
    
    .toast {
      position: fixed;
      bottom: 16px;
      right: 16px;
      padding: 10px 16px;
      border-radius: 6px;
      color: #fff;
      font-size: 0.8rem;
      z-index: 2000;
      animation: slideIn 0.3s ease;
    }
    .toast.success { background: var(--success); }
    .toast.error { background: var(--danger); }
    @keyframes slideIn { from { transform: translateY(20px); opacity: 0; } }
    
    .tab-content { display: none; }
    .tab-content.active { display: block; }
    
    input[type="file"] { display: none; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="logo">&#x25C8;</div>
      <h1>Scene Manager</h1>
      <div id="imuStatus">IMU: --</div>
    </header>
    
    <div class="tabs">
      <button class="tab active" onclick="showTab('scenes')">Scenes</button>
      <button class="tab" onclick="showTab('sprites')">Sprites</button>
      <button class="tab" onclick="showTab('params')">Live</button>
    </div>
    
    <!-- SCENES TAB -->
    <div id="tab-scenes" class="tab-content active">
      <div class="card">
        <div class="card-header">
          <span>Active Scene</span>
          <button class="btn btn-primary btn-sm" onclick="showNewSceneModal()">+ New</button>
        </div>
        <div class="card-body">
          <div class="scene-row">
            <select class="scene-select" id="sceneSelect" onchange="onSceneChange()">
              <option value="">-- None --</option>
            </select>
            <button class="btn btn-secondary btn-sm" onclick="showRenameModal()" id="btnRename" disabled>Rename</button>
            <button class="btn btn-danger btn-sm" onclick="deleteScene()" id="btnDelete" disabled>Delete</button>
          </div>
        </div>
      </div>
      
      <div class="card" id="animTypeCard" style="display:none;">
        <div class="card-header">Animation Type</div>
        <div class="card-body">
          <div class="anim-grid" id="animGrid"></div>
        </div>
      </div>
      
      <div id="sceneParamsContainer"></div>
    </div>
    
    <!-- SPRITES TAB -->
    <div id="tab-sprites" class="tab-content">
      <div class="card">
        <div class="card-header">
          <span>Sprite Library</span>
          <span style="font-size:0.7rem;color:var(--text-secondary);" id="spriteCount">0 sprites</span>
        </div>
        <div class="card-body">
          <div class="upload-zone" id="uploadZone" onclick="document.getElementById('fileInput').click()">
            <div class="icon">+</div>
            <p>Tap to upload sprite</p>
          </div>
          <input type="file" id="fileInput" accept="image/*">
          <div class="sprite-grid" id="spriteGrid" style="margin-top:12px;"></div>
        </div>
      </div>
    </div>
    
    <!-- LIVE PARAMS TAB -->
    <div id="tab-params" class="tab-content">
      <div class="card">
        <div class="card-header">Live Parameters</div>
        <div class="card-body" id="liveParamsContainer">
          <p style="color:var(--text-secondary);font-size:0.8rem;">Loading...</p>
        </div>
      </div>
    </div>
  </div>
  
  <!-- Modals -->
  <div class="modal-overlay" id="modalNew">
    <div class="modal">
      <h3>New Scene</h3>
      <input type="text" class="modal-input" id="newSceneName" placeholder="Scene name..." maxlength="32">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalNew')">Cancel</button>
        <button class="btn btn-primary" onclick="createScene()">Create</button>
      </div>
    </div>
  </div>
  
  <div class="modal-overlay" id="modalRename">
    <div class="modal">
      <h3>Rename Scene</h3>
      <input type="text" class="modal-input" id="renameInput" placeholder="New name..." maxlength="32">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalRename')">Cancel</button>
        <button class="btn btn-primary" onclick="renameScene()">Rename</button>
      </div>
    </div>
  </div>
  
  <div class="modal-overlay" id="modalSpriteName">
    <div class="modal">
      <h3>Name Your Sprite</h3>
      <input type="text" class="modal-input" id="spriteNameInput" placeholder="Sprite name..." maxlength="24">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalSpriteName');pendingSprite=null;">Cancel</button>
        <button class="btn btn-primary" onclick="uploadPendingSprite()">Save</button>
      </div>
    </div>
  </div>

<script>
let scenes = [], params = [], sprites = [], animTypes = [];
let activeSceneId = null, currentScene = null;
let pendingSprite = null;

document.addEventListener('DOMContentLoaded', async () => {
  await loadAnimTypes();
  await loadParams();
  await loadScenes();
  await loadSprites();
  renderLiveParams();
});

function showTab(name) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
  event.target.classList.add('active');
  document.getElementById('tab-' + name).classList.add('active');
  if (name === 'sprites') loadSprites();
}

// API
async function loadAnimTypes() {
  try { const r = await fetch('/api/animtypes'); const d = await r.json(); animTypes = d.types || []; } catch(e) {}
}
async function loadParams() {
  try { const r = await fetch('/api/params'); const d = await r.json(); params = d.params || []; } catch(e) {}
}
async function loadScenes() {
  try {
    const r = await fetch('/api/scenes');
    const d = await r.json();
    scenes = d.scenes || [];
    activeSceneId = d.activeId || null;
    renderSceneSelect();
  } catch(e) {}
}
async function loadSprites() {
  try {
    const r = await fetch('/api/sprites');
    const d = await r.json();
    sprites = d.sprites || [];
    renderSpriteGrid();
    document.getElementById('spriteCount').textContent = sprites.length + ' sprite' + (sprites.length !== 1 ? 's' : '');
  } catch(e) {}
}

// Rendering
function renderSceneSelect() {
  const sel = document.getElementById('sceneSelect');
  sel.innerHTML = '<option value="">-- None --</option>' +
    scenes.map(s => '<option value="'+s.id+'" '+(s.id==activeSceneId?'selected':'')+'>'+esc(s.name)+'</option>').join('');
  document.getElementById('btnRename').disabled = !activeSceneId;
  document.getElementById('btnDelete').disabled = !activeSceneId;
  if (activeSceneId) {
    currentScene = scenes.find(s => s.id == activeSceneId);
    document.getElementById('animTypeCard').style.display = 'block';
    renderAnimTypes();
    renderSceneParams();
  } else {
    currentScene = null;
    document.getElementById('animTypeCard').style.display = 'none';
    document.getElementById('sceneParamsContainer').innerHTML = '';
  }
}

function renderAnimTypes() {
  const grid = document.getElementById('animGrid');
  const icons = { gyro_eyes: '&#x25C9;', static_image: '&#x25A0;' };
  const names = { gyro_eyes: 'Gyro Eyes', static_image: 'Static' };
  grid.innerHTML = animTypes.map(t =>
    '<div class="anim-card '+(currentScene?.animationType===t?'selected':'')+'" onclick="selectAnimType(\''+t+'\')">'+
    '<div class="icon">'+(icons[t]||'?')+'</div><div class="name">'+(names[t]||t)+'</div></div>'
  ).join('');
}

function renderSceneParams() {
  const container = document.getElementById('sceneParamsContainer');
  if (!currentScene || !currentScene.animationType) { container.innerHTML = ''; return; }
  const relevantParams = params.filter(p => {
    if (currentScene.animationType === 'gyro_eyes') return p.group === 'Gyro Eyes';
    return false;
  });
  if (!relevantParams.length) { container.innerHTML = ''; return; }
  const groups = {};
  relevantParams.forEach(p => { if (!groups[p.group]) groups[p.group] = []; groups[p.group].push(p); });
  let html = '<div class="card"><div class="card-header">Parameters</div><div class="card-body">';
  for (const [gn, gp] of Object.entries(groups)) {
    html += '<div class="param-group"><div class="param-group-title">' + gn + '</div>';
    gp.forEach(p => { html += renderParamRow(p, 'scene'); });
    html += '</div>';
  }
  html += '</div></div>';
  container.innerHTML = html;
}

function renderLiveParams() {
  const container = document.getElementById('liveParamsContainer');
  if (!params.length) { container.innerHTML = '<p style="color:var(--text-secondary)">No parameters</p>'; return; }
  const groups = {};
  params.forEach(p => { if (!groups[p.group]) groups[p.group] = []; groups[p.group].push(p); });
  let html = '';
  for (const [gn, gp] of Object.entries(groups)) {
    html += '<div class="param-group"><div class="param-group-title">' + gn + '</div>';
    gp.forEach(p => { html += renderParamRow(p, 'live'); });
    html += '</div>';
  }
  container.innerHTML = html;
}

function renderParamRow(p, prefix) {
  let control = '';
  const id = prefix + '_' + p.id;
  if (p.type === 'slider') {
    const val = currentScene?.paramValues?.[p.id] ?? p.value;
    const dec = p.step < 1 ? 1 : 0;
    control = '<input type="range" id="'+id+'" min="'+p.min+'" max="'+p.max+'" step="'+p.step+'" value="'+val+
      '" oninput="updateParam(\''+p.id+'\',this.value,\'slider\',\''+id+'\')">'+
      '<span class="param-value" id="val_'+id+'">'+Number(val).toFixed(dec)+'</span>';
  } else if (p.type === 'dropdown') {
    const sel = currentScene?.paramStrings?.[p.id] ?? p.selectedOption;
    control = '<select class="param-select" id="'+id+'" onchange="updateParam(\''+p.id+'\',this.value,\'dropdown\',\''+id+'\')">'+
      p.options.map(o => '<option value="'+o.id+'" '+(o.id===sel?'selected':'')+'>'+o.label+'</option>').join('')+'</select>';
  } else if (p.type === 'toggle') {
    const checked = currentScene?.paramBools?.[p.id] ?? p.enabled;
    control = '<label style="cursor:pointer;"><input type="checkbox" id="'+id+'" '+(checked?'checked':'')+
      ' onchange="updateParam(\''+p.id+'\',this.checked,\'toggle\',\''+id+'\')"> '+(checked?'On':'Off')+'</label>';
  }
  return '<div class="param-row"><label class="param-label">'+p.name+'</label><div class="param-control">'+control+'</div></div>';
}

function renderSpriteGrid() {
  const grid = document.getElementById('spriteGrid');
  if (!sprites.length) { grid.innerHTML = ''; return; }
  grid.innerHTML = sprites.map(s =>
    '<div class="sprite-item" data-id="'+s.id+'">'+
    '<button class="sprite-delete" onclick="event.stopPropagation();deleteSprite('+s.id+')">x</button>'+
    '<div class="sprite-preview"><canvas id="sp_'+s.id+'" width="'+s.width+'" height="'+s.height+'"></canvas></div>'+
    '<div class="sprite-name">'+esc(s.name)+'</div></div>'
  ).join('');
  // Draw sprites on canvases
  sprites.forEach(s => {
    if (s.preview) {
      const canvas = document.getElementById('sp_' + s.id);
      if (canvas) {
        const ctx = canvas.getContext('2d');
        const img = new Image();
        img.onload = () => ctx.drawImage(img, 0, 0);
        img.src = s.preview;
      }
    }
  });
}

// Actions
async function selectAnimType(type) {
  if (!currentScene) return;
  currentScene.animationType = type;
  await fetch('/api/scene/update', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ id: currentScene.id, animationType: type })
  });
  renderAnimTypes();
  renderSceneParams();
}

let updateTimers = {};
function updateParam(paramId, value, type, elemId) {
  if (type === 'slider') {
    const p = params.find(x => x.id === paramId);
    const dec = p && p.step < 1 ? 1 : 0;
    document.getElementById('val_' + elemId).textContent = Number(value).toFixed(dec);
  }
  if (type === 'slider') {
    if (updateTimers[paramId]) clearTimeout(updateTimers[paramId]);
    updateTimers[paramId] = setTimeout(() => sendParamUpdate(paramId, value, type), 50);
  } else {
    sendParamUpdate(paramId, value, type);
  }
}

function sendParamUpdate(paramId, value, type) {
  let sendValue = value;
  if (type === 'slider') sendValue = parseFloat(value);
  fetch('/api/param/update', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ id: paramId, value: sendValue, type: type })
  }).catch(e => console.error(e));
}

function onSceneChange() {
  const sel = document.getElementById('sceneSelect');
  activeSceneId = sel.value ? parseInt(sel.value) : null;
  fetch('/api/scene/activate', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ id: activeSceneId })
  });
  renderSceneSelect();
}

function showNewSceneModal() {
  document.getElementById('newSceneName').value = '';
  showModal('modalNew');
  document.getElementById('newSceneName').focus();
}

async function createScene() {
  const name = document.getElementById('newSceneName').value.trim();
  if (!name) { toast('Enter a name', 'error'); return; }
  const r = await fetch('/api/scene/create', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ name })
  });
  const d = await r.json();
  if (d.success) {
    hideModal('modalNew');
    toast('Created!', 'success');
    activeSceneId = d.id;
    await loadScenes();
  } else {
    toast(d.error || 'Failed', 'error');
  }
}

function showRenameModal() {
  if (!currentScene) return;
  document.getElementById('renameInput').value = currentScene.name;
  showModal('modalRename');
}

async function renameScene() {
  const name = document.getElementById('renameInput').value.trim();
  if (!name) { toast('Enter a name', 'error'); return; }
  await fetch('/api/scene/rename', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ id: activeSceneId, name })
  });
  hideModal('modalRename');
  await loadScenes();
  toast('Renamed', 'success');
}

async function deleteScene() {
  if (!activeSceneId) return;
  if (!confirm('Delete scene?')) return;
  await fetch('/api/scene/delete', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ id: activeSceneId })
  });
  activeSceneId = null;
  await loadScenes();
  toast('Deleted', 'success');
}

// Sprite upload
document.getElementById('fileInput').addEventListener('change', function(e) {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = function(ev) {
    const img = new Image();
    img.onload = function() {
      // Scale to max 32x32
      let w = img.width, h = img.height;
      const maxSize = 32;
      if (w > maxSize || h > maxSize) {
        const scale = Math.min(maxSize / w, maxSize / h);
        w = Math.round(w * scale);
        h = Math.round(h * scale);
      }
      const canvas = document.createElement('canvas');
      canvas.width = w;
      canvas.height = h;
      const ctx = canvas.getContext('2d');
      ctx.imageSmoothingEnabled = false;
      ctx.drawImage(img, 0, 0, w, h);
      const imageData = ctx.getImageData(0, 0, w, h);
      // Convert to RGB888
      const pixels = new Uint8Array(w * h * 3);
      let idx = 0;
      for (let i = 0; i < imageData.data.length; i += 4) {
        pixels[idx++] = imageData.data[i];
        pixels[idx++] = imageData.data[i+1];
        pixels[idx++] = imageData.data[i+2];
      }
      // Base64 encode
      let binary = '';
      for (let i = 0; i < pixels.length; i++) binary += String.fromCharCode(pixels[i]);
      const b64 = btoa(binary);
      pendingSprite = { width: w, height: h, pixels: b64, preview: canvas.toDataURL() };
      document.getElementById('spriteNameInput').value = file.name.replace(/\.[^.]+$/, '').substring(0, 24);
      showModal('modalSpriteName');
      document.getElementById('spriteNameInput').focus();
    };
    img.src = ev.target.result;
  };
  reader.readAsDataURL(file);
  e.target.value = '';
});

async function uploadPendingSprite() {
  if (!pendingSprite) return;
  const name = document.getElementById('spriteNameInput').value.trim() || 'Sprite';
  const payload = { name, width: pendingSprite.width, height: pendingSprite.height, pixels: pendingSprite.pixels };
  try {
    const r = await fetch('/api/sprite/upload', {
      method: 'POST', headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(payload)
    });
    const d = await r.json();
    if (d.success) {
      hideModal('modalSpriteName');
      toast('Sprite saved!', 'success');
      await loadSprites();
      await loadParams(); // Refresh params (eye shape dropdown updated)
      renderLiveParams();
      renderSceneParams();
    } else {
      toast(d.error || 'Upload failed', 'error');
    }
  } catch(e) {
    toast('Upload failed', 'error');
  }
  pendingSprite = null;
}

async function deleteSprite(id) {
  if (!confirm('Delete sprite?')) return;
  await fetch('/api/sprite/delete', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ id })
  });
  await loadSprites();
  await loadParams();
  renderLiveParams();
  renderSceneParams();
  toast('Deleted', 'success');
}

// Helpers
function showModal(id) { document.getElementById(id).classList.add('show'); }
function hideModal(id) { document.getElementById(id).classList.remove('show'); }
function toast(msg, type) {
  const t = document.createElement('div');
  t.className = 'toast ' + type;
  t.textContent = msg;
  document.body.appendChild(t);
  setTimeout(() => t.remove(), 2500);
}
function esc(s) { const d = document.createElement('div'); d.textContent = s; return d.innerHTML; }

document.getElementById('newSceneName').addEventListener('keypress', e => { if(e.key==='Enter') createScene(); });
document.getElementById('renameInput').addEventListener('keypress', e => { if(e.key==='Enter') renameScene(); });
document.getElementById('spriteNameInput').addEventListener('keypress', e => { if(e.key==='Enter') uploadPendingSprite(); });

// IMU polling
async function pollImuStatus() {
  try {
    const r = await fetch('/api/imu');
    const d = await r.json();
    const el = document.getElementById('imuStatus');
    if (d.ok) {
      el.textContent = 'IMU: ' + d.gy + ',' + d.gz;
      el.style.color = 'var(--success)';
    } else {
      el.textContent = 'IMU: Off';
      el.style.color = 'var(--danger)';
    }
  } catch(e) {}
}
setInterval(pollImuStatus, 600);
pollImuStatus();
</script>
</body>
</html>
)rawliteral";

// ============================================================================
// HTTP HANDLERS
// ============================================================================

static esp_err_t handleRoot(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_TEMPLATE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleApiAnimTypes(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON* types = cJSON_CreateArray();
    for (const auto& t : g_animationTypes) {
        cJSON_AddItemToArray(types, cJSON_CreateString(t.c_str()));
    }
    cJSON_AddItemToObject(root, "types", types);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

static esp_err_t handleApiParams(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON* params = cJSON_CreateArray();
    
    for (const auto& p : g_params) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", p.id.c_str());
        cJSON_AddStringToObject(item, "name", p.name.c_str());
        cJSON_AddStringToObject(item, "group", p.group.c_str());
        
        if (p.type == ParamType::SLIDER) {
            cJSON_AddStringToObject(item, "type", "slider");
            cJSON_AddNumberToObject(item, "min", p.minVal);
            cJSON_AddNumberToObject(item, "max", p.maxVal);
            cJSON_AddNumberToObject(item, "step", p.step);
            cJSON_AddNumberToObject(item, "value", p.value);
        } else if (p.type == ParamType::DROPDOWN) {
            cJSON_AddStringToObject(item, "type", "dropdown");
            cJSON* opts = cJSON_CreateArray();
            for (const auto& o : p.options) {
                cJSON* opt = cJSON_CreateObject();
                cJSON_AddStringToObject(opt, "id", o.id.c_str());
                cJSON_AddStringToObject(opt, "label", o.label.c_str());
                cJSON_AddItemToArray(opts, opt);
            }
            cJSON_AddItemToObject(item, "options", opts);
            cJSON_AddStringToObject(item, "selectedOption", p.selectedOption.c_str());
        } else if (p.type == ParamType::TOGGLE) {
            cJSON_AddStringToObject(item, "type", "toggle");
            cJSON_AddBoolToObject(item, "enabled", p.enabled);
        }
        
        cJSON_AddItemToArray(params, item);
    }
    
    cJSON_AddItemToObject(root, "params", params);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

// Flag to indicate parameter changed (for faster response)
static volatile bool g_paramChanged = false;

static esp_err_t handleApiParamUpdate(httpd_req_t* req) {
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    cJSON* valueItem = cJSON_GetObjectItem(root, "value");
    cJSON* typeItem = cJSON_GetObjectItem(root, "type");
    
    if (idItem && cJSON_IsString(idItem)) {
        std::string paramId = idItem->valuestring;
        const char* typeStr = typeItem && cJSON_IsString(typeItem) ? typeItem->valuestring : "";
        
        for (auto& p : g_params) {
            if (p.id == paramId) {
                if (p.type == ParamType::SLIDER && cJSON_IsNumber(valueItem)) {
                    p.value = (float)valueItem->valuedouble;
                    ESP_LOGI(TAG, "Updated %s = %.2f", paramId.c_str(), p.value);
                } else if (p.type == ParamType::DROPDOWN) {
                    // Handle dropdown - value could be string from type field or from value field
                    if (cJSON_IsString(valueItem)) {
                        p.selectedOption = valueItem->valuestring;
                    } else if (cJSON_IsNumber(valueItem)) {
                        // Fallback: value sent as number, use type field hint
                        p.selectedOption = std::to_string((int)valueItem->valuedouble);
                    }
                    ESP_LOGI(TAG, "Updated dropdown %s = %s", paramId.c_str(), p.selectedOption.c_str());
                } else if (p.type == ParamType::TOGGLE && cJSON_IsBool(valueItem)) {
                    p.enabled = cJSON_IsTrue(valueItem);
                    ESP_LOGI(TAG, "Updated %s = %s", paramId.c_str(), p.enabled ? "true" : "false");
                }
                
                // Call onChange callback
                if (p.onChange) {
                    p.onChange();
                }
                g_paramChanged = true;  // Signal main loop
                break;
            }
        }
    }
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handle IMU status request - returns current gyro values
static esp_err_t handleApiImu(httpd_req_t* req) {
    char response[128];
    
    if (g_imuInitialized) {
        snprintf(response, sizeof(response), 
            "{\"ok\":true,\"gx\":%d,\"gy\":%d,\"gz\":%d,\"ax\":%d,\"ay\":%d,\"az\":%d}",
            Drivers::ImuDriver::gyroX, Drivers::ImuDriver::gyroY, Drivers::ImuDriver::gyroZ,
            Drivers::ImuDriver::accelX, Drivers::ImuDriver::accelY, Drivers::ImuDriver::accelZ);
    } else {
        snprintf(response, sizeof(response), "{\"ok\":false}");
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleApiScenes(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON* scenes = cJSON_CreateArray();
    
    for (const auto& s : g_scenes) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", s.id);
        cJSON_AddStringToObject(item, "name", s.name.c_str());
        cJSON_AddStringToObject(item, "animationType", s.animationType.c_str());
        cJSON_AddItemToArray(scenes, item);
    }
    
    cJSON_AddItemToObject(root, "scenes", scenes);
    cJSON_AddNumberToObject(root, "activeId", g_activeSceneId);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

static esp_err_t handleApiSceneCreate(httpd_req_t* req) {
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON* nameItem = cJSON_GetObjectItem(root, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":false,\"error\":\"Name required\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    Scene scene;
    scene.id = g_nextSceneId++;
    scene.name = nameItem->valuestring;
    scene.animationType = "gyro_eyes";  // Default
    g_scenes.push_back(scene);
    
    g_activeSceneId = scene.id;
    
    cJSON_Delete(root);
    
    char response[64];
    snprintf(response, sizeof(response), "{\"success\":true,\"id\":%d}", scene.id);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    
    ESP_LOGI(TAG, "Created scene: %s (id %d)", scene.name.c_str(), scene.id);
    return ESP_OK;
}

static esp_err_t handleApiSceneUpdate(httpd_req_t* req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    if (idItem && cJSON_IsNumber(idItem)) {
        int sceneId = idItem->valueint;
        for (auto& s : g_scenes) {
            if (s.id == sceneId) {
                cJSON* animType = cJSON_GetObjectItem(root, "animationType");
                if (animType && cJSON_IsString(animType)) {
                    s.animationType = animType->valuestring;
                    ESP_LOGI(TAG, "Scene %d animationType = %s", sceneId, s.animationType.c_str());
                }
                break;
            }
        }
    }
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleApiSceneActivate(httpd_req_t* req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    if (idItem) {
        if (cJSON_IsNumber(idItem)) {
            g_activeSceneId = idItem->valueint;
        } else if (cJSON_IsNull(idItem)) {
            g_activeSceneId = -1;
        }
        ESP_LOGI(TAG, "Active scene = %d", g_activeSceneId);
    }
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleApiSceneRename(httpd_req_t* req) {
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    cJSON* nameItem = cJSON_GetObjectItem(root, "name");
    
    if (idItem && cJSON_IsNumber(idItem) && nameItem && cJSON_IsString(nameItem)) {
        for (auto& s : g_scenes) {
            if (s.id == idItem->valueint) {
                s.name = nameItem->valuestring;
                ESP_LOGI(TAG, "Renamed scene %d to %s", s.id, s.name.c_str());
                break;
            }
        }
    }
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleApiSceneDelete(httpd_req_t* req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    if (idItem && cJSON_IsNumber(idItem)) {
        int sceneId = idItem->valueint;
        g_scenes.erase(
            std::remove_if(g_scenes.begin(), g_scenes.end(), 
                [sceneId](const Scene& s) { return s.id == sceneId; }),
            g_scenes.end()
        );
        
        if (g_activeSceneId == sceneId) {
            g_activeSceneId = -1;
        }
        
        ESP_LOGI(TAG, "Deleted scene %d", sceneId);
    }
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ============================================================================
// SPRITE HANDLERS
// ============================================================================

// Base64 decode lookup table
static const uint8_t b64_decode_table[256] = {
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
    64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
};

static std::vector<uint8_t> base64_decode(const std::string& encoded) {
    std::vector<uint8_t> result;
    result.reserve((encoded.size() * 3) / 4);
    
    uint32_t accum = 0;
    int bits = 0;
    
    for (char c : encoded) {
        if (c == '=') break;
        uint8_t val = b64_decode_table[(uint8_t)c];
        if (val == 64) continue;  // Skip invalid characters
        
        accum = (accum << 6) | val;
        bits += 6;
        
        if (bits >= 8) {
            bits -= 8;
            result.push_back((accum >> bits) & 0xFF);
        }
    }
    
    return result;
}

static esp_err_t handleApiSprites(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON* sprites = cJSON_CreateArray();
    
    for (const auto& s : g_sprites) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", s.id);
        cJSON_AddStringToObject(item, "name", s.name.c_str());
        cJSON_AddNumberToObject(item, "width", s.width);
        cJSON_AddNumberToObject(item, "height", s.height);
        // Include base64 preview (create from pixels)
        // For simplicity, we'll regenerate it here
        std::string preview = "data:image/png;base64,";  // Placeholder - client handles preview
        cJSON_AddItemToArray(sprites, item);
    }
    
    cJSON_AddItemToObject(root, "sprites", sprites);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

static esp_err_t handleApiSpriteUpload(httpd_req_t* req) {
    // Read body in chunks (sprites can be large)
    std::string body;
    char buf[512];
    int remaining = req->content_len;
    
    while (remaining > 0) {
        int to_read = std::min(remaining, (int)sizeof(buf));
        int ret = httpd_req_recv(req, buf, to_read);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Receive timeout");
            return ESP_FAIL;
        }
        body.append(buf, ret);
        remaining -= ret;
    }
    
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON* nameItem = cJSON_GetObjectItem(root, "name");
    cJSON* widthItem = cJSON_GetObjectItem(root, "width");
    cJSON* heightItem = cJSON_GetObjectItem(root, "height");
    cJSON* pixelsItem = cJSON_GetObjectItem(root, "pixels");
    
    if (!nameItem || !widthItem || !heightItem || !pixelsItem ||
        !cJSON_IsString(nameItem) || !cJSON_IsNumber(widthItem) ||
        !cJSON_IsNumber(heightItem) || !cJSON_IsString(pixelsItem)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");
        return ESP_FAIL;
    }
    
    int width = widthItem->valueint;
    int height = heightItem->valueint;
    std::string name = nameItem->valuestring;
    std::string pixelsB64 = pixelsItem->valuestring;
    
    // Decode base64
    std::vector<uint8_t> pixels = base64_decode(pixelsB64);
    
    // Verify size (RGB888 = 3 bytes per pixel)
    size_t expectedSize = width * height * 3;
    if (pixels.size() != expectedSize) {
        ESP_LOGW(TAG, "Pixel size mismatch: got %d, expected %d", (int)pixels.size(), (int)expectedSize);
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Pixel size mismatch");
        return ESP_FAIL;
    }
    
    // Check sprite limit
    if (g_sprites.size() >= 8) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":false,\"error\":\"Max 8 sprites\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // Create sprite
    SavedSprite sprite;
    sprite.id = g_nextSpriteId++;
    sprite.name = name;
    sprite.width = width;
    sprite.height = height;
    sprite.pixels = std::move(pixels);
    sprite.gpuSlot = g_nextGpuSlot++;
    sprite.uploaded = false;
    
    // Upload to GPU
    if (g_gpu.uploadSprite(sprite.gpuSlot, sprite.width, sprite.height, 
                           sprite.pixels.data(), SpriteFormat::RGB888)) {
        sprite.uploaded = true;
        ESP_LOGI(TAG, "Sprite %d uploaded to GPU slot %d", sprite.id, sprite.gpuSlot);
    } else {
        ESP_LOGW(TAG, "Failed to upload sprite to GPU");
    }
    
    g_sprites.push_back(std::move(sprite));
    
    // Update eye shape dropdown to include new sprite
    updateEyeShapeOptions();
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleApiSpriteDelete(httpd_req_t* req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    if (idItem && cJSON_IsNumber(idItem)) {
        int spriteId = idItem->valueint;
        
        // If this sprite is selected as eye shape, reset to circle
        if (g_gyroEyes.useSprite && g_gyroEyes.eyeSpriteId == spriteId) {
            g_gyroEyes.useSprite = false;
            g_gyroEyes.eyeSpriteId = -1;
            g_gyroEyes.eyeShape = 0;
        }
        
        g_sprites.erase(
            std::remove_if(g_sprites.begin(), g_sprites.end(), 
                [spriteId](const SavedSprite& s) { return s.id == spriteId; }),
            g_sprites.end()
        );
        
        // Update eye shape dropdown
        updateEyeShapeOptions();
        
        ESP_LOGI(TAG, "Deleted sprite %d", spriteId);
    }
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ============================================================================
// SERVER SETUP
// ============================================================================

static void startWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.stack_size = 8192;
    
    if (httpd_start(&g_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }
    
    // Register handlers
    httpd_uri_t uris[] = {
        { "/", HTTP_GET, handleRoot, nullptr },
        { "/api/animtypes", HTTP_GET, handleApiAnimTypes, nullptr },
        { "/api/params", HTTP_GET, handleApiParams, nullptr },
        { "/api/param/update", HTTP_POST, handleApiParamUpdate, nullptr },
        { "/api/imu", HTTP_GET, handleApiImu, nullptr },
        { "/api/scenes", HTTP_GET, handleApiScenes, nullptr },
        { "/api/scene/create", HTTP_POST, handleApiSceneCreate, nullptr },
        { "/api/scene/update", HTTP_POST, handleApiSceneUpdate, nullptr },
        { "/api/scene/activate", HTTP_POST, handleApiSceneActivate, nullptr },
        { "/api/scene/rename", HTTP_POST, handleApiSceneRename, nullptr },
        { "/api/scene/delete", HTTP_POST, handleApiSceneDelete, nullptr },
        { "/api/sprites", HTTP_GET, handleApiSprites, nullptr },
        { "/api/sprite/upload", HTTP_POST, handleApiSpriteUpload, nullptr },
        { "/api/sprite/delete", HTTP_POST, handleApiSpriteDelete, nullptr },
    };
    
    for (const auto& uri : uris) {
        httpd_register_uri_handler(g_server, &uri);
    }
    
    ESP_LOGI(TAG, "Web server started");
}

// ============================================================================
// ANIMATION RENDERING
// ============================================================================

void renderGyroEyes() {
    // Clear background
    g_gpu.clear(0, 0, 0);
    
    float size = g_gyroEyes.eyeSize;
    int shape = g_gyroEyes.eyeShape;
    
    // Left eye (panel 0)
    float lx = g_gyroEyes.leftX;
    float ly = g_gyroEyes.leftY;
    
    // Right eye (panel 1, offset by 64)
    float rx = 64.0f + g_gyroEyes.rightX;
    float ry = g_gyroEyes.rightY;
    
    // Check if using a sprite
    if (g_gyroEyes.useSprite && g_gyroEyes.eyeSpriteId >= 0) {
        // Find the sprite
        for (const auto& sprite : g_sprites) {
            if (sprite.id == g_gyroEyes.eyeSpriteId && sprite.uploaded) {
                // Center the sprite on the eye position
                float halfW = sprite.width / 2.0f;
                float halfH = sprite.height / 2.0f;
                g_gpu.blitSpriteF(sprite.gpuSlot, lx - halfW, ly - halfH);
                g_gpu.blitSpriteF(sprite.gpuSlot, rx - halfW, ry - halfH);
                return;
            }
        }
        // Sprite not found, fall through to shape rendering
    }
    
    if (shape == 0) {
        // Circle
        g_gpu.drawCircleF(lx, ly, size, 255, 255, 255);
        g_gpu.drawCircleF(rx, ry, size, 255, 255, 255);
    } else if (shape == 1) {
        // Square
        int hs = (int)size;
        g_gpu.drawFilledRect((int)lx - hs, (int)ly - hs, hs*2, hs*2, 255, 255, 255);
        g_gpu.drawFilledRect((int)rx - hs, (int)ry - hs, hs*2, hs*2, 255, 255, 255);
    } else {
        // Diamond (rotated square approximation - use smaller circles for now)
        g_gpu.drawCircleF(lx, ly, size * 0.8f, 255, 255, 255);
        g_gpu.drawCircleF(rx, ry, size * 0.8f, 255, 255, 255);
    }
}

void renderStatic() {
    // Static image - just draw centered eyes that don't move
    g_gpu.clear(0, 0, 0);
    
    float size = g_gyroEyes.eyeSize;
    int shape = g_gyroEyes.eyeShape;
    
    // Fixed center positions
    float lx = 32.0f;
    float ly = 16.0f;
    float rx = 64.0f + 32.0f;
    float ry = 16.0f;
    
    // Check if using a sprite
    if (g_gyroEyes.useSprite && g_gyroEyes.eyeSpriteId >= 0) {
        for (const auto& sprite : g_sprites) {
            if (sprite.id == g_gyroEyes.eyeSpriteId && sprite.uploaded) {
                float halfW = sprite.width / 2.0f;
                float halfH = sprite.height / 2.0f;
                g_gpu.blitSpriteF(sprite.gpuSlot, lx - halfW, ly - halfH);
                g_gpu.blitSpriteF(sprite.gpuSlot, rx - halfW, ry - halfH);
                return;
            }
        }
    }
    
    if (shape == 0) {
        g_gpu.drawCircleF(lx, ly, size, 255, 255, 255);
        g_gpu.drawCircleF(rx, ry, size, 255, 255, 255);
    } else if (shape == 1) {
        int hs = (int)size;
        g_gpu.drawFilledRect((int)lx - hs, (int)ly - hs, hs*2, hs*2, 255, 255, 255);
        g_gpu.drawFilledRect((int)rx - hs, (int)ry - hs, hs*2, hs*2, 255, 255, 255);
    } else {
        g_gpu.drawCircleF(lx, ly, size * 0.8f, 255, 255, 255);
        g_gpu.drawCircleF(rx, ry, size * 0.8f, 255, 255, 255);
    }
}

// ============================================================================
// MAIN
// ============================================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== Web Config Test Starting ===");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Register parameters
    registerGyroEyeParams();
    ESP_LOGI(TAG, "Registered %d parameters", g_params.size());
    
    // Initialize GPU
    if (!g_gpu.init()) {
        ESP_LOGE(TAG, "GPU init failed");
        return;
    }
    
    // Initialize IMU (ICM20948)
    ESP_LOGI(TAG, "Initializing IMU...");
    g_imuInitialized = Drivers::ImuDriver::init();
    if (g_imuInitialized) {
        ESP_LOGI(TAG, "IMU initialized successfully");
    } else {
        ESP_LOGW(TAG, "IMU not found - using simulated gyro");
    }
    
    // Start WiFi AP
    PortalConfig wifiConfig;
    strncpy(wifiConfig.ssid, "ConfigTest-AP", sizeof(wifiConfig.ssid));
    strncpy(wifiConfig.password, "", sizeof(wifiConfig.password));
    WifiManager::instance().init(wifiConfig);
    ESP_LOGI(TAG, "WiFi AP: ConfigTest-AP");
    
    // Start DNS for captive portal
    DnsServer::instance().start();
    
    // Start web server
    startWebServer();
    
    ESP_LOGI(TAG, "Connect to WiFi and open http://192.168.4.1");
    
    // Main loop - run at higher frequency for responsiveness
    uint32_t lastRenderTime = 0;
    const uint32_t renderInterval = 33;  // ~30fps to reduce CPU/GPU load
    
    ESP_LOGI(TAG, "Entering main loop");
    
    while (true) {
        uint32_t now = esp_timer_get_time() / 1000;
        
        // Update IMU if initialized
        if (g_imuInitialized) {
            Drivers::ImuDriver::update();
        }
        
        // Only render at target frame rate (or immediately if param changed)
        if (g_paramChanged || (now - lastRenderTime >= renderInterval)) {
            g_paramChanged = false;
            lastRenderTime = now;
            
            // Get gyro data from IMU or fallback to simulation
            float gyroX = 0, gyroY = 0, gyroZ = 0;
            
            if (g_imuInitialized) {
                // Use real IMU data (values are in degrees/second)
                gyroX = (float)Drivers::ImuDriver::gyroX;
                gyroY = (float)Drivers::ImuDriver::gyroY;
                gyroZ = (float)Drivers::ImuDriver::gyroZ;
            } else {
                // Fallback to simulated movement
                float t = now * 0.001f;
                gyroX = 0;
                gyroY = sinf(t * 0.5f) * 30.0f;
                gyroZ = sinf(t * 0.3f) * 30.0f;
            }
            
            // Update animation
            g_gyroEyes.update(gyroX, gyroY, gyroZ);
            
            // Render based on active scene
            bool rendered = false;
            if (g_activeSceneId > 0) {
                for (const auto& s : g_scenes) {
                    if (s.id == g_activeSceneId) {
                        if (s.animationType == "gyro_eyes") {
                            renderGyroEyes();
                            rendered = true;
                        } else if (s.animationType == "static_image") {
                            renderStatic();
                            rendered = true;
                        }
                        break;
                    }
                }
            }
            
            // Default: show gyro eyes if nothing selected
            if (!rendered) {
                renderGyroEyes();
            }
            
            // Present frame to GPU
            g_gpu.present();
        }
        
        // Yield to other tasks (HTTP server) - short delay for responsiveness
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
