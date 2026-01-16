/*****************************************************************
 * @file PageScenes.hpp
 * @brief Scene management web page - sprite selection and display
 * 
 * Features:
 * - Browse available sprites
 * - Position control (X, Y)
 * - Background color selection
 * - One-click activation to HUB75 display
 * 
 * Pipeline: Web UI -> Core 0 -> Core 1 -> GPU -> HUB75
 *****************************************************************/

#ifndef PAGE_SCENES_HPP
#define PAGE_SCENES_HPP

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_SCENES[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Scenes</title>
  <link rel="stylesheet" href="/style.css">
  <style>
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
    
    /* Scene Layout */
    .scene-layout {
      display: flex;
      flex-direction: column;
      gap: 20px;
    }
    
    /* Sprite Grid */
    .sprite-section {
      background: var(--bg-tertiary);
      border-radius: 12px;
      padding: 16px;
    }
    .section-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
    }
    .section-title {
      font-size: 0.95rem;
      font-weight: 600;
      color: var(--text-secondary);
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .section-icon {
      font-size: 1.1rem;
      color: var(--accent);
    }
    .sprite-count {
      font-size: 0.75rem;
      color: var(--text-muted);
      background: var(--bg-secondary);
      padding: 4px 10px;
      border-radius: 12px;
      font-family: 'SF Mono', Monaco, monospace;
    }
    
    .sprite-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(80px, 1fr));
      gap: 10px;
      max-height: 240px;
      overflow-y: auto;
      padding-right: 4px;
    }
    .sprite-grid::-webkit-scrollbar {
      width: 6px;
    }
    .sprite-grid::-webkit-scrollbar-thumb {
      background: var(--border);
      border-radius: 3px;
    }
    
    .sprite-item {
      background: var(--bg-secondary);
      border: 2px solid var(--border);
      border-radius: 8px;
      padding: 8px;
      cursor: pointer;
      transition: all 0.2s;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 6px;
    }
    .sprite-item:hover {
      border-color: var(--text-muted);
      background: var(--bg-tertiary);
    }
    .sprite-item.selected {
      border-color: var(--accent);
      background: var(--accent-subtle);
    }
    .sprite-preview {
      width: 48px;
      height: 48px;
      background: #000;
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    }
    .sprite-preview img {
      max-width: 100%;
      max-height: 100%;
      image-rendering: pixelated;
      image-rendering: crisp-edges;
    }
    .sprite-name {
      font-size: 0.65rem;
      color: var(--text-muted);
      text-align: center;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      max-width: 100%;
    }
    .sprite-item.selected .sprite-name {
      color: var(--accent);
      font-weight: 600;
    }
    
    .no-sprites {
      text-align: center;
      padding: 32px 16px;
      color: var(--text-muted);
    }
    .no-sprites-icon {
      font-size: 2rem;
      margin-bottom: 8px;
      opacity: 0.5;
    }
    .no-sprites-text {
      font-size: 0.85rem;
    }
    .no-sprites-link {
      color: var(--accent);
      text-decoration: none;
      font-weight: 500;
    }
    .no-sprites-link:hover {
      text-decoration: underline;
    }
    
    /* Position Controls */
    .controls-section {
      background: var(--bg-tertiary);
      border-radius: 12px;
      padding: 16px;
    }
    .control-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 16px;
    }
    @media (max-width: 400px) {
      .control-grid { grid-template-columns: 1fr; }
    }
    
    .control-group {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .control-label {
      font-size: 0.75rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .control-row {
      display: flex;
      gap: 8px;
      align-items: center;
    }
    .control-input {
      flex: 1;
      padding: 10px 12px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 0.9rem;
      text-align: center;
    }
    .control-input:focus {
      border-color: var(--accent);
      outline: none;
    }
    .control-unit {
      font-size: 0.75rem;
      color: var(--text-muted);
      min-width: 24px;
    }
    
    /* Background Color */
    .bg-color-section {
      background: var(--bg-tertiary);
      border-radius: 12px;
      padding: 16px;
    }
    .color-presets {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      margin-top: 12px;
    }
    .color-preset {
      width: 32px;
      height: 32px;
      border-radius: 6px;
      border: 2px solid var(--border);
      cursor: pointer;
      transition: all 0.2s;
    }
    .color-preset:hover {
      transform: scale(1.1);
    }
    .color-preset.selected {
      border-color: var(--accent);
      box-shadow: 0 0 0 2px var(--accent-subtle);
    }
    .color-custom {
      display: flex;
      gap: 8px;
      margin-top: 12px;
      align-items: center;
    }
    .color-input-group {
      display: flex;
      gap: 4px;
      align-items: center;
    }
    .color-input {
      width: 48px;
      padding: 8px 6px;
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--text-primary);
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 0.8rem;
      text-align: center;
    }
    .color-input:focus {
      border-color: var(--accent);
      outline: none;
    }
    .color-label {
      font-size: 0.65rem;
      color: var(--text-muted);
      text-transform: uppercase;
    }
    .color-preview-box {
      width: 40px;
      height: 40px;
      border-radius: 8px;
      border: 2px solid var(--border);
      margin-left: auto;
    }
    
    /* Preview Panel */
    .preview-section {
      background: var(--bg-tertiary);
      border-radius: 12px;
      padding: 16px;
    }
    .preview-display {
      background: #000;
      border: 2px solid var(--border);
      border-radius: 8px;
      aspect-ratio: 4 / 1;
      position: relative;
      overflow: hidden;
      margin-top: 12px;
    }
    .preview-sprite {
      position: absolute;
      image-rendering: pixelated;
      image-rendering: crisp-edges;
    }
    .preview-grid-overlay {
      position: absolute;
      top: 0; left: 0; right: 0; bottom: 0;
      background-image: 
        linear-gradient(rgba(255,255,255,0.03) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255,255,255,0.03) 1px, transparent 1px);
      background-size: 3.125% 12.5%;
      pointer-events: none;
    }
    .preview-info {
      display: flex;
      justify-content: space-between;
      margin-top: 8px;
      font-size: 0.7rem;
      color: var(--text-muted);
      font-family: 'SF Mono', Monaco, monospace;
    }
    
    /* Activate Button */
    .action-section {
      display: flex;
      gap: 12px;
    }
    .activate-btn {
      flex: 1;
      padding: 16px 24px;
      background: var(--accent);
      border: none;
      border-radius: 12px;
      color: var(--bg-primary);
      font-size: 1rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
    }
    .activate-btn:hover:not(:disabled) {
      background: var(--accent-hover);
      transform: translateY(-1px);
    }
    .activate-btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .activate-btn .icon {
      font-size: 1.2rem;
    }
    
    .clear-btn {
      padding: 16px 20px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 12px;
      color: var(--text-secondary);
      font-size: 0.9rem;
      font-weight: 500;
      cursor: pointer;
      transition: all 0.2s;
    }
    .clear-btn:hover {
      background: var(--danger);
      border-color: var(--danger);
      color: white;
    }
    
    /* Status Toast */
    .status-toast {
      position: fixed;
      bottom: 20px;
      left: 50%;
      transform: translateX(-50%) translateY(100px);
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 12px 20px;
      font-size: 0.85rem;
      color: var(--text-primary);
      box-shadow: 0 8px 32px rgba(0,0,0,0.3);
      transition: transform 0.3s ease;
      z-index: 1000;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .status-toast.show {
      transform: translateX(-50%) translateY(0);
    }
    .status-toast.success {
      border-color: var(--success);
    }
    .status-toast.error {
      border-color: var(--danger);
    }
    .toast-icon {
      font-size: 1.1rem;
    }
    .status-toast.success .toast-icon { color: var(--success); }
    .status-toast.error .toast-icon { color: var(--danger); }
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
      <a href="/advanced" class="back-link">&#x2190; Advanced</a>
      
      <div class="scene-layout">
        <!-- Sprite Selection -->
        <div class="sprite-section">
          <div class="section-header">
            <div class="section-title">
              <span class="section-icon">&#x25A2;</span>
              Select Sprite
            </div>
            <span class="sprite-count" id="sprite-count">0 sprites</span>
          </div>
          <div class="sprite-grid" id="sprite-grid">
            <!-- Sprites loaded dynamically -->
          </div>
        </div>
        
        <!-- Position Controls -->
        <div class="controls-section">
          <div class="section-header">
            <div class="section-title">
              <span class="section-icon">&#x2316;</span>
              Position
            </div>
          </div>
          <div class="control-grid">
            <div class="control-group">
              <label class="control-label">X Position</label>
              <div class="control-row">
                <input type="number" class="control-input" id="pos-x" value="0" min="0" max="127">
                <span class="control-unit">px</span>
              </div>
            </div>
            <div class="control-group">
              <label class="control-label">Y Position</label>
              <div class="control-row">
                <input type="number" class="control-input" id="pos-y" value="0" min="0" max="31">
                <span class="control-unit">px</span>
              </div>
            </div>
          </div>
        </div>
        
        <!-- Background Color -->
        <div class="bg-color-section">
          <div class="section-header">
            <div class="section-title">
              <span class="section-icon">&#x25A3;</span>
              Background Color
            </div>
          </div>
          <div class="color-presets">
            <div class="color-preset selected" data-r="0" data-g="0" data-b="0" style="background:#000;"></div>
            <div class="color-preset" data-r="32" data-g="32" data-b="32" style="background:#202020;"></div>
            <div class="color-preset" data-r="0" data-g="0" data-b="64" style="background:#000040;"></div>
            <div class="color-preset" data-r="64" data-g="0" data-b="0" style="background:#400000;"></div>
            <div class="color-preset" data-r="0" data-g="64" data-b="0" style="background:#004000;"></div>
            <div class="color-preset" data-r="64" data-g="32" data-b="0" style="background:#402000;"></div>
            <div class="color-preset" data-r="32" data-g="0" data-b="64" style="background:#200040;"></div>
            <div class="color-preset" data-r="0" data-g="32" data-b="64" style="background:#002040;"></div>
          </div>
          <div class="color-custom">
            <div class="color-input-group">
              <span class="color-label">R</span>
              <input type="number" class="color-input" id="bg-r" value="0" min="0" max="255">
            </div>
            <div class="color-input-group">
              <span class="color-label">G</span>
              <input type="number" class="color-input" id="bg-g" value="0" min="0" max="255">
            </div>
            <div class="color-input-group">
              <span class="color-label">B</span>
              <input type="number" class="color-input" id="bg-b" value="0" min="0" max="255">
            </div>
            <div class="color-preview-box" id="color-preview" style="background:#000;"></div>
          </div>
        </div>
        
        <!-- Preview -->
        <div class="preview-section">
          <div class="section-header">
            <div class="section-title">
              <span class="section-icon">&#x25A1;</span>
              Preview
            </div>
          </div>
          <div class="preview-display" id="preview-display">
            <div class="preview-grid-overlay"></div>
            <img id="preview-sprite" class="preview-sprite" style="display:none;">
          </div>
          <div class="preview-info">
            <span id="preview-pos">Position: 0, 0</span>
            <span id="preview-size">Display: 128x32</span>
          </div>
        </div>
        
        <!-- Actions -->
        <div class="action-section">
          <button class="activate-btn" id="activate-btn" disabled>
            <span class="icon">&#x25B6;</span>
            Display on HUB75
          </button>
          <button class="clear-btn" id="clear-btn">Clear</button>
        </div>
      </div>
    </section>
  </div>
  
  <!-- Status Toast -->
  <div class="status-toast" id="status-toast">
    <span class="toast-icon">&#x2713;</span>
    <span class="toast-text">Status message</span>
  </div>
  
  <script>
    // State
    let sprites = [];
    let selectedSprite = null;
    let bgColor = { r: 0, g: 0, b: 0 };
    
    // Elements
    const spriteGrid = document.getElementById('sprite-grid');
    const spriteCount = document.getElementById('sprite-count');
    const posX = document.getElementById('pos-x');
    const posY = document.getElementById('pos-y');
    const bgR = document.getElementById('bg-r');
    const bgG = document.getElementById('bg-g');
    const bgB = document.getElementById('bg-b');
    const colorPreview = document.getElementById('color-preview');
    const previewDisplay = document.getElementById('preview-display');
    const previewSprite = document.getElementById('preview-sprite');
    const previewPos = document.getElementById('preview-pos');
    const activateBtn = document.getElementById('activate-btn');
    const clearBtn = document.getElementById('clear-btn');
    const statusToast = document.getElementById('status-toast');
    
    // Load sprites
    async function loadSprites() {
      try {
        const res = await fetch('/api/sprites');
        const data = await res.json();
        sprites = data.sprites || [];
        renderSpriteGrid();
      } catch (err) {
        console.error('Failed to load sprites:', err);
        spriteGrid.innerHTML = `
          <div class="no-sprites">
            <div class="no-sprites-icon">&#x26A0;</div>
            <div class="no-sprites-text">Failed to load sprites</div>
          </div>
        `;
      }
    }
    
    // Render sprite grid
    function renderSpriteGrid() {
      spriteCount.textContent = `${sprites.length} sprite${sprites.length !== 1 ? 's' : ''}`;
      
      if (sprites.length === 0) {
        spriteGrid.innerHTML = `
          <div class="no-sprites">
            <div class="no-sprites-icon">&#x25A2;</div>
            <div class="no-sprites-text">
              No sprites yet. <a href="/advanced/sprites" class="no-sprites-link">Upload sprites</a>
            </div>
          </div>
        `;
        return;
      }
      
      spriteGrid.innerHTML = sprites.map(s => `
        <div class="sprite-item" data-id="${s.id}">
          <div class="sprite-preview">
            ${s.preview ? `<img src="${s.preview}" alt="${s.name}">` : '&#x25A2;'}
          </div>
          <div class="sprite-name">${s.name}</div>
        </div>
      `).join('');
      
      // Add click handlers
      spriteGrid.querySelectorAll('.sprite-item').forEach(item => {
        item.addEventListener('click', () => selectSprite(parseInt(item.dataset.id)));
      });
    }
    
    // Select sprite
    function selectSprite(id) {
      selectedSprite = sprites.find(s => s.id === id);
      
      // Update visual selection
      spriteGrid.querySelectorAll('.sprite-item').forEach(item => {
        item.classList.toggle('selected', parseInt(item.dataset.id) === id);
      });
      
      // Update preview
      updatePreview();
      
      // Enable/disable activate button
      activateBtn.disabled = !selectedSprite;
    }
    
    // Update preview
    function updatePreview() {
      const x = parseInt(posX.value) || 0;
      const y = parseInt(posY.value) || 0;
      
      // Update background
      const bgHex = `rgb(${bgColor.r},${bgColor.g},${bgColor.b})`;
      previewDisplay.style.background = bgHex;
      
      // Update position text
      previewPos.textContent = `Position: ${x}, ${y}`;
      
      // Update sprite preview
      if (selectedSprite && selectedSprite.preview) {
        previewSprite.src = selectedSprite.preview;
        previewSprite.style.display = 'block';
        
        // Calculate position in preview (128x32 scaled to container)
        const container = previewDisplay.getBoundingClientRect();
        const scaleX = container.width / 128;
        const scaleY = container.height / 32;
        
        previewSprite.style.left = `${x * scaleX}px`;
        previewSprite.style.top = `${y * scaleY}px`;
        previewSprite.style.width = `${(selectedSprite.width || 32) * scaleX}px`;
        previewSprite.style.height = `${(selectedSprite.height || 32) * scaleY}px`;
      } else {
        previewSprite.style.display = 'none';
      }
    }
    
    // Update background color
    function updateBgColor() {
      bgColor.r = Math.min(255, Math.max(0, parseInt(bgR.value) || 0));
      bgColor.g = Math.min(255, Math.max(0, parseInt(bgG.value) || 0));
      bgColor.b = Math.min(255, Math.max(0, parseInt(bgB.value) || 0));
      
      const hex = `rgb(${bgColor.r},${bgColor.g},${bgColor.b})`;
      colorPreview.style.background = hex;
      
      // Deselect presets
      document.querySelectorAll('.color-preset').forEach(p => {
        const pr = parseInt(p.dataset.r);
        const pg = parseInt(p.dataset.g);
        const pb = parseInt(p.dataset.b);
        p.classList.toggle('selected', pr === bgColor.r && pg === bgColor.g && pb === bgColor.b);
      });
      
      updatePreview();
    }
    
    // Show toast
    function showToast(message, type = 'success') {
      const toast = document.getElementById('status-toast');
      toast.querySelector('.toast-text').textContent = message;
      toast.querySelector('.toast-icon').innerHTML = type === 'success' ? '&#x2713;' : '&#x2717;';
      toast.className = `status-toast ${type} show`;
      
      setTimeout(() => {
        toast.classList.remove('show');
      }, 3000);
    }
    
    // Activate scene
    async function activateScene() {
      if (!selectedSprite) return;
      
      activateBtn.disabled = true;
      activateBtn.innerHTML = '<span class="icon">&#x21BB;</span> Sending...';
      
      try {
        const res = await fetch('/api/scene/display', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            spriteId: selectedSprite.id,
            posX: parseInt(posX.value) || 0,
            posY: parseInt(posY.value) || 0,
            bgR: bgColor.r,
            bgG: bgColor.g,
            bgB: bgColor.b
          })
        });
        
        const data = await res.json();
        
        if (data.success) {
          showToast('Sprite displayed on HUB75!', 'success');
        } else {
          showToast(data.error || 'Failed to display sprite', 'error');
        }
      } catch (err) {
        console.error('Activate error:', err);
        showToast('Connection error', 'error');
      }
      
      activateBtn.disabled = !selectedSprite;
      activateBtn.innerHTML = '<span class="icon">&#x25B6;</span> Display on HUB75';
    }
    
    // Clear display
    async function clearDisplay() {
      try {
        const res = await fetch('/api/scene/clear', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' }
        });
        
        const data = await res.json();
        
        if (data.success) {
          showToast('Display cleared', 'success');
        }
      } catch (err) {
        console.error('Clear error:', err);
      }
    }
    
    // Event listeners
    posX.addEventListener('input', updatePreview);
    posY.addEventListener('input', updatePreview);
    bgR.addEventListener('input', updateBgColor);
    bgG.addEventListener('input', updateBgColor);
    bgB.addEventListener('input', updateBgColor);
    activateBtn.addEventListener('click', activateScene);
    clearBtn.addEventListener('click', clearDisplay);
    
    // Color preset clicks
    document.querySelectorAll('.color-preset').forEach(preset => {
      preset.addEventListener('click', () => {
        bgR.value = preset.dataset.r;
        bgG.value = preset.dataset.g;
        bgB.value = preset.dataset.b;
        updateBgColor();
      });
    });
    
    // Initialize
    loadSprites();
    updateBgColor();
  </script>
</body>
</html>
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI

#endif // PAGE_SCENES_HPP
