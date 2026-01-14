/*****************************************************************
 * @file PageSprite.hpp
 * @brief Simplified Sprite Manager - Import, scale, save sprites
 * 
 * Features:
 * - Upload sprite images
 * - Scale control (no min/max caps)
 * - Shows original image and scaled display preview
 * - Storage usage display (KB per sprite, ESP free space)
 * - Sprite manager with rename/delete
 * 
 * Display specs: 64x32 HUB75 panel
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_SPRITE[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Sprite Manager</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    .sprite-editor { display: flex; flex-direction: column; gap: 20px; }
    
    /* Back link */
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
    
    /* Storage bar */
    .storage-bar {
      background: var(--bg-tertiary);
      border-radius: 10px;
      padding: 12px 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 16px;
      flex-wrap: wrap;
    }
    .storage-stat {
      display: flex;
      flex-direction: column;
      gap: 2px;
    }
    .storage-label {
      font-size: 0.7rem;
      color: var(--text-muted);
      text-transform: uppercase;
    }
    .storage-value {
      font-size: 0.95rem;
      font-weight: 600;
      color: var(--text-primary);
      font-family: 'SF Mono', Monaco, monospace;
    }
    .storage-value.accent { color: var(--accent); }
    .storage-progress {
      flex: 1;
      min-width: 120px;
      height: 8px;
      background: var(--bg-secondary);
      border-radius: 4px;
      overflow: hidden;
    }
    .storage-fill {
      height: 100%;
      background: var(--accent);
      border-radius: 4px;
      transition: width 0.3s;
    }
    
    /* Upload Zone */
    .upload-zone {
      position: relative;
      border: 2px dashed var(--border);
      border-radius: 12px;
      padding: 32px 20px;
      text-align: center;
      transition: all 0.3s;
      cursor: pointer;
      background: var(--bg-tertiary);
    }
    .upload-zone:hover, .upload-zone.dragover {
      border-color: var(--accent);
      background: var(--accent-subtle);
    }
    .upload-zone.has-image {
      border-style: solid;
      border-color: var(--success);
      background: rgba(0, 204, 102, 0.05);
    }
    .upload-icon {
      font-size: 2.5rem;
      color: var(--text-muted);
      margin-bottom: 12px;
      line-height: 1;
    }
    .upload-zone.dragover .upload-icon,
    .upload-zone:hover .upload-icon { color: var(--accent); }
    .upload-title {
      font-size: 1rem;
      font-weight: 600;
      margin-bottom: 6px;
      color: var(--text-primary);
    }
    .upload-hint {
      font-size: 0.8rem;
      color: var(--text-muted);
    }
    .upload-input {
      position: absolute;
      top: 0; left: 0; right: 0; bottom: 0;
      opacity: 0;
      cursor: pointer;
    }
    .upload-btn-mobile {
      display: none;
      margin-top: 12px;
      padding: 10px 20px;
      background: var(--accent);
      color: var(--bg-primary);
      border: none;
      border-radius: 8px;
      font-weight: 600;
      cursor: pointer;
    }
    @media (pointer: coarse) {
      .upload-btn-mobile { display: inline-block; }
      .upload-hint.desktop-only { display: none; }
    }
    .clear-image-btn {
      position: absolute;
      top: 8px;
      right: 8px;
      width: 28px;
      height: 28px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 50%;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 1rem;
      z-index: 10;
      display: none;
      align-items: center;
      justify-content: center;
      transition: all 0.2s;
    }
    .upload-zone.has-image .clear-image-btn { display: flex; }
    .clear-image-btn:hover {
      background: var(--danger);
      border-color: var(--danger);
      color: white;
    }
    
    /* Preview Section - Side by Side */
    .preview-section {
      background: var(--bg-tertiary);
      border-radius: 12px;
      padding: 16px;
    }
    .preview-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
    }
    @media (max-width: 550px) {
      .preview-grid { grid-template-columns: 1fr; }
    }
    .preview-panel {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .preview-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .preview-title {
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-secondary);
    }
    .preview-dims {
      font-size: 0.7rem;
      color: var(--text-muted);
      font-family: 'SF Mono', Monaco, monospace;
    }
    .preview-box {
      background: #000;
      border: 2px solid var(--border);
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 100px;
      overflow: hidden;
    }
    .preview-box.original {
      max-height: 200px;
    }
    .preview-box img, .preview-box canvas {
      display: block;
      image-rendering: pixelated;
      image-rendering: crisp-edges;
      max-width: 100%;
      max-height: 100%;
    }
    .preview-placeholder {
      color: var(--text-muted);
      font-size: 0.8rem;
      text-align: center;
      padding: 20px;
    }
    
    /* Controls */
    .controls-section {
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    .control-row {
      display: flex;
      gap: 12px;
      align-items: center;
    }
    @media (max-width: 500px) {
      .control-row { flex-wrap: wrap; }
    }
    .control-card {
      background: var(--bg-tertiary);
      border-radius: 12px;
      padding: 16px;
    }
    .control-label {
      font-size: 0.85rem;
      font-weight: 600;
      margin-bottom: 10px;
      color: var(--text-secondary);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .control-value {
      font-family: 'SF Mono', Monaco, monospace;
      color: var(--accent);
      font-size: 0.8rem;
    }
    
    /* Scale Control */
    .scale-row {
      display: flex;
      gap: 8px;
      align-items: center;
      margin-bottom: 8px;
    }
    .scale-input {
      width: 80px;
      padding: 10px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-primary);
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 0.9rem;
      text-align: center;
    }
    .scale-input:focus {
      border-color: var(--accent);
      outline: none;
    }
    .scale-unit {
      color: var(--text-muted);
      font-size: 0.9rem;
    }
    .auto-fit-btn {
      padding: 10px 16px;
      background: var(--accent-subtle);
      border: 1px solid var(--accent);
      border-radius: 6px;
      color: var(--accent);
      font-size: 0.85rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
      white-space: nowrap;
    }
    .auto-fit-btn:hover {
      background: var(--accent);
      color: var(--bg-primary);
    }
    .slider-container {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .slider {
      flex: 1;
      -webkit-appearance: none;
      height: 6px;
      background: var(--bg-secondary);
      border-radius: 3px;
      outline: none;
    }
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 20px;
      height: 20px;
      background: var(--accent);
      border-radius: 50%;
      cursor: pointer;
    }
    .slider::-moz-range-thumb {
      width: 20px;
      height: 20px;
      background: var(--accent);
      border-radius: 50%;
      cursor: pointer;
      border: none;
    }
    .slider-label {
      font-size: 0.75rem;
      color: var(--text-muted);
      width: 32px;
      text-align: center;
      font-family: 'SF Mono', Monaco, monospace;
    }
    
    /* Toggle Button */
    .toggle-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 10px 12px;
      background: var(--bg-secondary);
      border-radius: 8px;
      margin-top: 10px;
    }
    .toggle-label {
      font-size: 0.85rem;
      color: var(--text-secondary);
    }
    .toggle-switch {
      position: relative;
      width: 48px;
      height: 26px;
      background: var(--border);
      border-radius: 13px;
      cursor: pointer;
      transition: background 0.3s;
    }
    .toggle-switch.active {
      background: var(--accent);
    }
    .toggle-switch::after {
      content: '';
      position: absolute;
      width: 22px;
      height: 22px;
      background: white;
      border-radius: 50%;
      top: 2px;
      left: 2px;
      transition: transform 0.3s;
    }
    .toggle-switch.active::after {
      transform: translateX(22px);
    }
    
    /* Sprite Name */
    .name-input {
      width: 100%;
      padding: 10px 12px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-size: 0.9rem;
    }
    .name-input:focus {
      border-color: var(--accent);
      outline: none;
    }
    
    /* Size estimate */
    .size-estimate {
      display: flex;
      gap: 16px;
      padding: 10px 12px;
      background: var(--bg-secondary);
      border-radius: 8px;
      margin-top: 8px;
    }
    .size-item {
      display: flex;
      flex-direction: column;
      gap: 2px;
    }
    .size-label {
      font-size: 0.65rem;
      color: var(--text-muted);
      text-transform: uppercase;
    }
    .size-value {
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-primary);
      font-family: 'SF Mono', Monaco, monospace;
    }
    
    /* Action Buttons */
    .action-bar {
      display: flex;
      gap: 12px;
      padding-top: 16px;
      border-top: 1px solid var(--border);
    }
    .action-bar .btn { flex: 1; }
    
    /* Sprite Manager */
    .sprite-manager { margin-top: 24px; }
    .manager-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
    }
    .manager-title {
      font-size: 1rem;
      font-weight: 600;
      color: var(--text-primary);
    }
    .sprite-count {
      font-size: 0.75rem;
      color: var(--text-muted);
      background: var(--bg-secondary);
      padding: 4px 10px;
      border-radius: 12px;
    }
    .sprite-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .sprite-item {
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 12px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 10px;
      transition: all 0.2s;
    }
    .sprite-item:hover { border-color: var(--accent); }
    .sprite-thumb {
      width: 48px;
      height: 24px;
      background: #000;
      border-radius: 4px;
      image-rendering: pixelated;
      flex-shrink: 0;
    }
    .sprite-info { flex: 1; min-width: 0; }
    .sprite-name {
      font-size: 0.9rem;
      font-weight: 500;
      color: var(--text-primary);
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .sprite-meta {
      font-size: 0.7rem;
      color: var(--text-muted);
      margin-top: 2px;
    }
    .sprite-size {
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 0.75rem;
      color: var(--accent);
      padding: 4px 8px;
      background: var(--accent-subtle);
      border-radius: 4px;
    }
    .sprite-actions { display: flex; gap: 6px; }
    .sprite-action-btn {
      width: 32px;
      height: 32px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 0.9rem;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.2s;
    }
    .sprite-action-btn:hover {
      background: var(--accent-subtle);
      border-color: var(--accent);
      color: var(--accent);
    }
    .sprite-action-btn.delete:hover {
      background: rgba(255, 82, 82, 0.1);
      border-color: var(--danger);
      color: var(--danger);
    }
    .no-sprites {
      text-align: center;
      padding: 32px 16px;
      color: var(--text-muted);
      font-size: 0.85rem;
    }
    .no-sprites-icon {
      font-size: 2rem;
      margin-bottom: 8px;
      opacity: 0.5;
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
    }
    .modal-overlay.show { display: flex; }
    .modal-box {
      background: var(--bg-card);
      border-radius: 16px;
      padding: 24px;
      width: 90%;
      max-width: 320px;
    }
    .modal-title {
      font-size: 1.1rem;
      font-weight: 600;
      margin-bottom: 16px;
    }
    .modal-input {
      width: 100%;
      padding: 10px 12px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-size: 0.9rem;
      margin-bottom: 16px;
    }
    .modal-input:focus {
      border-color: var(--accent);
      outline: none;
    }
    .modal-buttons {
      display: flex;
      gap: 8px;
    }
    .modal-buttons .btn { flex: 1; }
    
    /* Processing */
    .processing-overlay {
      position: fixed;
      top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(0,0,0,0.8);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 1000;
    }
    .processing-overlay.show { display: flex; }
    .processing-box {
      background: var(--bg-card);
      border-radius: 16px;
      padding: 32px;
      text-align: center;
    }
    .processing-spinner {
      width: 40px;
      height: 40px;
      border: 4px solid var(--border);
      border-top-color: var(--accent);
      border-radius: 50%;
      animation: spin 1s linear infinite;
      margin: 0 auto 16px;
    }
    @keyframes spin { to { transform: rotate(360deg); } }
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
      
      <!-- Storage Overview -->
      <div class="storage-bar">
        <div class="storage-stat">
          <span class="storage-label">Sprites</span>
          <span class="storage-value" id="sprite-total">--</span>
        </div>
        <div class="storage-stat">
          <span class="storage-label">Used</span>
          <span class="storage-value accent" id="storage-used">--</span>
        </div>
        <div class="storage-progress">
          <div class="storage-fill" id="storage-fill" style="width: 0%;"></div>
        </div>
        <div class="storage-stat">
          <span class="storage-label">Free</span>
          <span class="storage-value" id="storage-free">--</span>
        </div>
      </div>
      
      <div class="card" style="margin-top: 16px;">
        <div class="card-header">
          <h2>Import Sprite</h2>
        </div>
        <div class="card-body">
          <div class="sprite-editor">
            
            <!-- Upload Zone -->
            <div class="upload-zone" id="upload-zone">
              <button class="clear-image-btn" onclick="clearImage(event)" title="Remove image">&times;</button>
              <div class="upload-icon">&#x2B9E;</div>
              <div class="upload-title" id="upload-title">Upload Sprite Image</div>
              <div class="upload-hint desktop-only">Drag and drop an image here</div>
              <div class="upload-hint">PNG, JPG, GIF, BMP supported</div>
              <button class="upload-btn-mobile" onclick="document.getElementById('file-input').click()">
                Select Image
              </button>
              <input type="file" id="file-input" class="upload-input" accept="image/*" onchange="handleFileSelect(event)">
            </div>
            
            <!-- Preview Section - Original + Display Preview -->
            <div class="preview-section" id="preview-section" style="display: none;">
              <div class="preview-grid">
                <!-- Original Image -->
                <div class="preview-panel">
                  <div class="preview-header">
                    <span class="preview-title">Original Image</span>
                    <span class="preview-dims" id="original-dims">-- x --</span>
                  </div>
                  <div class="preview-box original">
                    <img id="original-preview" src="" alt="Original">
                  </div>
                </div>
                
                <!-- Scaled Preview -->
                <div class="preview-panel">
                  <div class="preview-header">
                    <span class="preview-title">Scaled Preview</span>
                    <span class="preview-dims" id="scaled-dims">-- x --</span>
                  </div>
                  <div class="preview-box" id="scaled-preview-box">
                    <canvas id="display-canvas" width="64" height="32"></canvas>
                  </div>
                </div>
              </div>
            </div>
            
            <!-- Controls (shown when image loaded) -->
            <div class="controls-section" id="controls-section" style="display: none;">
              
              <!-- Scale Control -->
              <div class="control-card">
                <div class="control-label">
                  <span>Resolution Scale</span>
                  <span class="control-value" id="output-size">-- x --</span>
                </div>
                <div class="scale-row">
                  <input type="number" class="scale-input" id="scale-input" value="100" step="1" onchange="updateScaleFromInput()">
                  <span class="scale-unit">%</span>
                  <button class="auto-fit-btn" onclick="autoFitScale()">Auto Fit</button>
                </div>
                <div class="slider-container">
                  <span class="slider-label">1%</span>
                  <input type="range" class="slider" id="scale-slider" min="1" max="500" value="100" oninput="updateScale()">
                  <span class="slider-label">500%</span>
                </div>
                <div class="size-estimate">
                  <div class="size-item">
                    <span class="size-label">Sprite Size</span>
                    <span class="size-value" id="sprite-size-kb">-- KB</span>
                  </div>
                  <div class="size-item">
                    <span class="size-label">Pixels</span>
                    <span class="size-value" id="pixel-count">--</span>
                  </div>
                </div>
                <div class="toggle-row">
                  <span class="toggle-label">Invert Colors</span>
                  <div class="toggle-switch" id="invert-toggle" onclick="toggleInvert()"></div>
                </div>
              </div>
              
              <!-- Sprite Name -->
              <div class="control-card">
                <div class="control-label">Sprite Name</div>
                <input type="text" class="name-input" id="sprite-name" placeholder="Enter sprite name..." maxlength="32">
              </div>
              
            </div>
            
            <!-- Action Buttons -->
            <div class="action-bar" id="action-bar" style="display: none;">
              <button class="btn btn-secondary" onclick="clearImage(event)">
                &#x21BA; Clear
              </button>
              <button class="btn btn-primary" onclick="saveSprite()" id="save-btn">
                &#x2713; Save Sprite
              </button>
            </div>
            
            <!-- Sprite Manager -->
            <div class="sprite-manager">
              <div class="manager-header">
                <span class="manager-title">Saved Sprites</span>
                <span class="sprite-count" id="sprite-count">0 sprites</span>
              </div>
              <div class="sprite-list" id="sprite-list">
                <div class="no-sprites" id="no-sprites">
                  <div class="no-sprites-icon">&#x25A2;</div>
                  <div>No sprites saved yet</div>
                  <div style="font-size: 0.75rem; margin-top: 4px;">Import a sprite to use in configurations</div>
                </div>
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
  
  <!-- Processing Overlay -->
  <div class="processing-overlay" id="processing">
    <div class="processing-box">
      <div class="processing-spinner"></div>
      <div>Processing sprite...</div>
    </div>
  </div>
  
  <!-- Rename Modal -->
  <div class="modal-overlay" id="rename-modal">
    <div class="modal-box">
      <div class="modal-title">Rename Sprite</div>
      <input type="text" class="modal-input" id="rename-input" placeholder="Enter new name..." maxlength="32">
      <div class="modal-buttons">
        <button class="btn btn-secondary" onclick="closeRenameModal()">Cancel</button>
        <button class="btn btn-primary" onclick="confirmRename()">Rename</button>
      </div>
    </div>
  </div>
  
  <script>
  // State
  var originalImage = null;
  var originalDataUrl = '';
  var imageLoaded = false;
  var currentScale = 100;
  var invertEnabled = false;
  
  // Canvas - will be resized dynamically
  var canvas = document.getElementById('display-canvas');
  var ctx = canvas.getContext('2d');
  
  // Storage tracking
  var totalStorage = 4 * 1024 * 1024; // 4MB default
  var usedStorage = 0;
  
  // Initialize
  loadStorageInfo();
  loadSpriteList();
  
  // Drag and drop
  var uploadZone = document.getElementById('upload-zone');
  
  uploadZone.addEventListener('dragover', function(e) {
    e.preventDefault();
    uploadZone.classList.add('dragover');
  });
  
  uploadZone.addEventListener('dragleave', function(e) {
    e.preventDefault();
    uploadZone.classList.remove('dragover');
  });
  
  uploadZone.addEventListener('drop', function(e) {
    e.preventDefault();
    uploadZone.classList.remove('dragover');
    if (e.dataTransfer.files.length > 0) {
      processFile(e.dataTransfer.files[0]);
    }
  });
  
  function handleFileSelect(event) {
    if (event.target.files.length > 0) {
      processFile(event.target.files[0]);
    }
  }
  
  function processFile(file) {
    if (!file.type.match(/image.*/)) {
      showToast('Please select an image file', 'error');
      return;
    }
    
    document.getElementById('processing').classList.add('show');
    
    var reader = new FileReader();
    reader.onload = function(e) {
      var img = new Image();
      img.onload = function() {
        originalImage = img;
        originalDataUrl = e.target.result;
        imageLoaded = true;
        
        // Show UI sections
        document.getElementById('preview-section').style.display = 'block';
        document.getElementById('controls-section').style.display = 'flex';
        document.getElementById('action-bar').style.display = 'flex';
        uploadZone.classList.add('has-image');
        document.getElementById('upload-title').textContent = file.name;
        
        // Show original preview
        document.getElementById('original-preview').src = originalDataUrl;
        document.getElementById('original-dims').textContent = img.width + ' x ' + img.height;
        
        // Auto-set name from filename
        if (!document.getElementById('sprite-name').value) {
          document.getElementById('sprite-name').value = file.name.replace(/\.[^/.]+$/, '');
        }
        
        // Auto-fit
        autoFitScale();
        
        document.getElementById('processing').classList.remove('show');
        showToast('Image loaded', 'success');
      };
      img.onerror = function() {
        document.getElementById('processing').classList.remove('show');
        showToast('Failed to load image', 'error');
      };
      img.src = e.target.result;
    };
    reader.onerror = function() {
      document.getElementById('processing').classList.remove('show');
      showToast('Failed to read file', 'error');
    };
    reader.readAsDataURL(file);
  }
  
  function autoFitScale() {
    if (!originalImage) return;
    
    // Calculate scale to fit image to typical 64x32 display
    var targetWidth = 64;
    var targetHeight = 32;
    var scaleX = targetWidth / originalImage.width;
    var scaleY = targetHeight / originalImage.height;
    var fitScale = Math.min(scaleX, scaleY) * 100;
    
    currentScale = Math.round(fitScale);
    document.getElementById('scale-slider').value = Math.min(500, Math.max(1, currentScale));
    document.getElementById('scale-input').value = currentScale;
    updatePreview();
    showToast('Fit to 64x32: ' + currentScale + '%', 'info');
  }
  
  function updateScale() {
    currentScale = parseInt(document.getElementById('scale-slider').value);
    document.getElementById('scale-input').value = currentScale;
    updatePreview();
  }
  
  function updateScaleFromInput() {
    var val = parseInt(document.getElementById('scale-input').value) || 100;
    if (val < 1) val = 1;
    currentScale = val;
    document.getElementById('scale-slider').value = Math.min(500, Math.max(1, val));
    document.getElementById('scale-input').value = val;
    updatePreview();
  }
  
  function updatePreview() {
    if (!imageLoaded || !originalImage) return;
    
    // Calculate scaled dimensions (the actual output resolution)
    var scale = currentScale / 100;
    var scaledWidth = Math.max(1, Math.round(originalImage.width * scale));
    var scaledHeight = Math.max(1, Math.round(originalImage.height * scale));
    
    // Resize canvas to match scaled dimensions
    canvas.width = scaledWidth;
    canvas.height = scaledHeight;
    
    // Scale up display for visibility (max 256px display size)
    var maxDisplaySize = 256;
    var displayScale = Math.min(maxDisplaySize / scaledWidth, maxDisplaySize / scaledHeight, 4);
    displayScale = Math.max(1, displayScale);
    canvas.style.width = Math.round(scaledWidth * displayScale) + 'px';
    canvas.style.height = Math.round(scaledHeight * displayScale) + 'px';
    
    // Draw the image at the scaled resolution
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    ctx.drawImage(originalImage, 0, 0, scaledWidth, scaledHeight);
    
    // Apply invert if enabled
    if (invertEnabled) {
      var imageData = ctx.getImageData(0, 0, scaledWidth, scaledHeight);
      var data = imageData.data;
      for (var i = 0; i < data.length; i += 4) {
        data[i] = 255 - data[i];       // R
        data[i + 1] = 255 - data[i + 1]; // G
        data[i + 2] = 255 - data[i + 2]; // B
        // Alpha stays the same
      }
      ctx.putImageData(imageData, 0, 0);
    }
    
    // Update dimension display
    document.getElementById('scaled-dims').textContent = scaledWidth + ' x ' + scaledHeight;
    document.getElementById('output-size').textContent = scaledWidth + ' x ' + scaledHeight;
    
    // Calculate size (RGB888 = 3 bytes per pixel)
    var totalPixels = scaledWidth * scaledHeight;
    var spriteBytes = totalPixels * 3;
    document.getElementById('sprite-size-kb').textContent = (spriteBytes / 1024).toFixed(1) + ' KB';
    document.getElementById('pixel-count').textContent = totalPixels.toLocaleString();
  }
  
  function toggleInvert() {
    invertEnabled = !invertEnabled;
    var toggle = document.getElementById('invert-toggle');
    if (invertEnabled) {
      toggle.classList.add('active');
    } else {
      toggle.classList.remove('active');
    }
    updatePreview();
  }
  
  function clearImage(event) {
    if (event) event.stopPropagation();
    
    originalImage = null;
    originalDataUrl = '';
    imageLoaded = false;
    currentScale = 100;
    invertEnabled = false;
    document.getElementById('invert-toggle').classList.remove('active');
    
    uploadZone.classList.remove('has-image');
    document.getElementById('upload-title').textContent = 'Upload Sprite Image';
    document.getElementById('file-input').value = '';
    document.getElementById('preview-section').style.display = 'none';
    document.getElementById('controls-section').style.display = 'none';
    document.getElementById('action-bar').style.display = 'none';
    document.getElementById('scale-slider').value = 100;
    document.getElementById('scale-input').value = 100;
    document.getElementById('sprite-name').value = '';
    
    // Reset canvas
    canvas.width = 64;
    canvas.height = 32;
    canvas.style.width = '256px';
    canvas.style.height = '128px';
    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, 64, 32);
    
    showToast('Cleared', 'info');
  }
  
  function loadStorageInfo() {
    fetch('/api/storage')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.total !== undefined) {
        totalStorage = data.total;
        usedStorage = data.used || 0;
        updateStorageDisplay();
      }
    })
    .catch(function() {
      // Use defaults
      updateStorageDisplay();
    });
  }
  
  function updateStorageDisplay() {
    document.getElementById('storage-used').textContent = formatBytes(usedStorage);
    document.getElementById('storage-free').textContent = formatBytes(totalStorage - usedStorage);
    var pct = totalStorage > 0 ? (usedStorage / totalStorage * 100) : 0;
    document.getElementById('storage-fill').style.width = pct + '%';
  }
  
  function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
  }
  
  // Sprites
  var savedSprites = [];
  var renamingSpriteId = null;
  
  function saveSprite() {
    if (!imageLoaded || !originalImage) {
      showToast('Please load an image first', 'warning');
      return;
    }
    
    var name = document.getElementById('sprite-name').value.trim();
    if (!name) {
      showToast('Please enter a sprite name', 'warning');
      document.getElementById('sprite-name').focus();
      return;
    }
    
    document.getElementById('processing').classList.add('show');
    
    // Get current canvas dimensions (the scaled resolution)
    var spriteWidth = canvas.width;
    var spriteHeight = canvas.height;
    
    // Get pixel data from canvas at its current size
    var imageData = ctx.getImageData(0, 0, spriteWidth, spriteHeight);
    var pixels = extractRGB888(imageData, spriteWidth, spriteHeight);
    var pixelsBase64 = uint8ArrayToBase64(pixels);
    
    // Get preview
    var previewB64 = canvas.toDataURL('image/png');
    
    var payload = {
      name: name,
      width: spriteWidth,
      height: spriteHeight,
      scale: currentScale,
      pixels: pixelsBase64,
      preview: previewB64
    };
    
    fetch('/api/sprite/save', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(payload)
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      document.getElementById('processing').classList.remove('show');
      if (data.success) {
        showToast('Sprite "' + name + '" saved', 'success');
        loadSpriteList();
        loadStorageInfo();
      } else {
        showToast('Failed: ' + (data.error || 'Unknown error'), 'error');
      }
    })
    .catch(function(err) {
      document.getElementById('processing').classList.remove('show');
      showToast('Error: ' + err, 'error');
    });
  }
  
  function loadSpriteList() {
    fetch('/api/sprites')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.sprites) {
        savedSprites = data.sprites;
        renderSpriteList();
        document.getElementById('sprite-total').textContent = savedSprites.length;
      }
    })
    .catch(function(err) {
      console.error('Failed to load sprites:', err);
    });
  }
  
  function renderSpriteList() {
    var list = document.getElementById('sprite-list');
    var noSprites = document.getElementById('no-sprites');
    var countEl = document.getElementById('sprite-count');
    
    countEl.textContent = savedSprites.length + ' sprite' + (savedSprites.length !== 1 ? 's' : '');
    
    if (savedSprites.length === 0) {
      noSprites.style.display = 'block';
      var items = list.querySelectorAll('.sprite-item');
      items.forEach(function(item) { item.remove(); });
      return;
    }
    
    noSprites.style.display = 'none';
    list.innerHTML = '';
    
    savedSprites.forEach(function(sprite) {
      var sizeKB = sprite.sizeBytes ? (sprite.sizeBytes / 1024).toFixed(1) : ((64*32*3) / 1024).toFixed(1);
      
      var item = document.createElement('div');
      item.className = 'sprite-item';
      item.innerHTML = 
        '<img class="sprite-thumb" src="' + (sprite.preview || '') + '" alt="">' +
        '<div class="sprite-info">' +
          '<div class="sprite-name">' + escapeHtml(sprite.name) + '</div>' +
          '<div class="sprite-meta">' + sprite.width + 'x' + sprite.height + ' | Scale: ' + sprite.scale + '%</div>' +
        '</div>' +
        '<span class="sprite-size">' + sizeKB + ' KB</span>' +
        '<div class="sprite-actions">' +
          '<button class="sprite-action-btn" onclick="openRenameModal(' + sprite.id + ', \'' + escapeHtml(sprite.name) + '\')" title="Rename">&#x270E;</button>' +
          '<button class="sprite-action-btn delete" onclick="deleteSprite(' + sprite.id + ')" title="Delete">&#x2715;</button>' +
        '</div>';
      list.appendChild(item);
    });
  }
  
  function escapeHtml(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }
  
  function openRenameModal(id, currentName) {
    renamingSpriteId = id;
    document.getElementById('rename-input').value = currentName;
    document.getElementById('rename-modal').classList.add('show');
    document.getElementById('rename-input').focus();
  }
  
  function closeRenameModal() {
    renamingSpriteId = null;
    document.getElementById('rename-modal').classList.remove('show');
  }
  
  function confirmRename() {
    var newName = document.getElementById('rename-input').value.trim();
    if (!newName) {
      showToast('Please enter a name', 'warning');
      return;
    }
    
    fetch('/api/sprite/rename', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ id: renamingSpriteId, name: newName })
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      closeRenameModal();
      if (data.success) {
        showToast('Sprite renamed', 'success');
        loadSpriteList();
      } else {
        showToast('Failed: ' + (data.error || 'Unknown error'), 'error');
      }
    })
    .catch(function(err) {
      closeRenameModal();
      showToast('Error: ' + err, 'error');
    });
  }
  
  function deleteSprite(id) {
    if (!confirm('Delete this sprite?')) return;
    
    fetch('/api/sprite/delete', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ id: id })
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        showToast('Sprite deleted', 'success');
        loadSpriteList();
        loadStorageInfo();
      } else {
        showToast('Failed: ' + (data.error || 'Unknown error'), 'error');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }
  
  function extractRGB888(imageData, width, height) {
    var data = imageData.data;
    var pixels = new Uint8Array(width * height * 3);
    var idx = 0;
    
    for (var i = 0; i < data.length; i += 4) {
      pixels[idx++] = data[i];     // R
      pixels[idx++] = data[i + 1]; // G
      pixels[idx++] = data[i + 2]; // B
    }
    return pixels;
  }
  
  function uint8ArrayToBase64(bytes) {
    var binary = '';
    for (var i = 0; i < bytes.byteLength; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    return btoa(binary);
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
