/*****************************************************************
 * @file PageAdvancedMenu.hpp
 * @brief Advanced menu page - sub-navigation for Sprites & Configs
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_ADVANCED_MENU[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Advanced</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    .submenu-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 16px;
    }
    .submenu-card {
      background: var(--bg-tertiary);
      border: 2px solid var(--border);
      border-radius: 16px;
      padding: 24px;
      text-decoration: none;
      color: inherit;
      transition: all 0.3s;
      display: block;
    }
    .submenu-card:hover {
      border-color: var(--accent);
      background: var(--accent-subtle);
      transform: translateY(-2px);
    }
    .submenu-icon {
      font-size: 2.5rem;
      margin-bottom: 12px;
      display: block;
    }
    .submenu-title {
      font-size: 1.2rem;
      font-weight: 600;
      margin-bottom: 8px;
      color: var(--text-primary);
    }
    .submenu-desc {
      font-size: 0.85rem;
      color: var(--text-muted);
      line-height: 1.5;
    }
    .submenu-stats {
      display: flex;
      gap: 16px;
      margin-top: 16px;
      padding-top: 12px;
      border-top: 1px solid var(--border);
    }
    .stat-item {
      display: flex;
      flex-direction: column;
      gap: 2px;
    }
    .stat-value {
      font-size: 1.1rem;
      font-weight: 600;
      color: var(--accent);
      font-family: 'SF Mono', Monaco, monospace;
    }
    .stat-label {
      font-size: 0.7rem;
      color: var(--text-muted);
      text-transform: uppercase;
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
      <div class="card">
        <div class="card-header">
          <h2>Advanced Settings</h2>
        </div>
        <div class="card-body">
          <div class="submenu-grid">
            
            <a href="/advanced/sprites" class="submenu-card">
              <span class="submenu-icon">&#x25A2;</span>
              <div class="submenu-title">Sprite Manager</div>
              <div class="submenu-desc">Import, resize, and manage sprite images for display configurations. Optimize storage by adjusting resolution.</div>
              <div class="submenu-stats">
                <div class="stat-item">
                  <span class="stat-value" id="sprite-count">--</span>
                  <span class="stat-label">Sprites</span>
                </div>
                <div class="stat-item">
                  <span class="stat-value" id="storage-used">--</span>
                  <span class="stat-label">Used</span>
                </div>
                <div class="stat-item">
                  <span class="stat-value" id="storage-free">--</span>
                  <span class="stat-label">Free</span>
                </div>
              </div>
            </a>
            
            <a href="/advanced/scenes" class="submenu-card">
              <span class="submenu-icon">&#x25A1;</span>
              <div class="submenu-title">Scenes</div>
              <div class="submenu-desc">Display sprites on HUB75 panels. Select, position, and activate sprites with custom backgrounds.</div>
              <div class="submenu-stats">
                <div class="stat-item">
                  <span class="stat-value" id="scene-sprite-count">--</span>
                  <span class="stat-label">Sprites</span>
                </div>
                <div class="stat-item">
                  <span class="stat-value" id="scene-status">Ready</span>
                  <span class="stat-label">Status</span>
                </div>
              </div>
            </a>
            
            <a href="/advanced/ledpresets" class="submenu-card">
              <span class="submenu-icon">&#x2299;</span>
              <div class="submenu-title">LED Presets</div>
              <div class="submenu-desc">YAML-based LED configuration. Create animations like Breathe, Rainbow, Pulse with color and speed controls.</div>
              <div class="submenu-stats">
                <div class="stat-item">
                  <span class="stat-value" id="led-preset-count">--</span>
                  <span class="stat-label">Presets</span>
                </div>
                <div class="stat-item">
                  <span class="stat-value" id="led-status">Ready</span>
                  <span class="stat-label">Status</span>
                </div>
              </div>
            </a>
            
            <a href="/advanced/equations" class="submenu-card">
              <span class="submenu-icon">&#x222B;</span>
              <div class="submenu-title">Equation Editor</div>
              <div class="submenu-desc">Create equations using sensor inputs for dynamic animation parameters. Build complex behaviors from simple math.</div>
              <div class="submenu-stats">
                <div class="stat-item">
                  <span class="stat-value" id="equation-count">--</span>
                  <span class="stat-label">Equations</span>
                </div>
                <div class="stat-item">
                  <span class="stat-value" id="sensor-count">27</span>
                  <span class="stat-label">Inputs</span>
                </div>
              </div>
            </a>
            
            <a href="/sdcard" class="submenu-card">
              <span class="submenu-icon">&#x2261;</span>
              <div class="submenu-title">SD Card Browser</div>
              <div class="submenu-desc">Browse SD card filesystem. View files, check storage status, and manage saved data.</div>
              <div class="submenu-stats">
                <div class="stat-item">
                  <span class="stat-value" id="sdcard-status">--</span>
                  <span class="stat-label">Status</span>
                </div>
                <div class="stat-item">
                  <span class="stat-value" id="sdcard-free">--</span>
                  <span class="stat-label">Free</span>
                </div>
              </div>
            </a>
            
          </div>
        </div>
      </div>
    </section>
    
    <footer>
      <p>Lucidius - ARCOS Framework</p>
    </footer>
  </div>
  
  <script>
  function loadStats() {
    // Load sprite stats
    fetch('/api/sprites')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.sprites) {
        document.getElementById('sprite-count').textContent = data.sprites.length;
      }
    }).catch(function() {});
    
    // Load storage stats
    fetch('/api/storage')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.used !== undefined) {
        document.getElementById('storage-used').textContent = formatBytes(data.used);
        document.getElementById('storage-free').textContent = formatBytes(data.free);
      }
    }).catch(function() {
      document.getElementById('storage-used').textContent = '0 KB';
      document.getElementById('storage-free').textContent = '4 MB';
    });
    
    // Load scene stats (uses sprites count)
    fetch('/api/sprites')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.sprites) {
        document.getElementById('scene-sprite-count').textContent = data.sprites.length;
      }
    }).catch(function() {
      document.getElementById('scene-sprite-count').textContent = '0';
    });
    
    // Load LED preset stats
    fetch('/api/ledpresets')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.presets) {
        document.getElementById('led-preset-count').textContent = data.presets.length;
        var active = data.presets.find(function(p) { return p.active; });
        document.getElementById('led-status').textContent = active ? 'Active' : 'Ready';
      }
    }).catch(function() {
      document.getElementById('led-preset-count').textContent = '0';
      document.getElementById('led-status').textContent = 'Ready';
    });
    
    // Load equation stats
    fetch('/api/equations')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.equations) {
        document.getElementById('equation-count').textContent = data.equations.length;
      }
    }).catch(function() {
      document.getElementById('equation-count').textContent = '0';
    });
    
    // Load SD card stats
    fetch('/api/sdcard/status')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      document.getElementById('sdcard-status').textContent = data.ready ? 'Ready' : 'Not Ready';
      if (data.free !== undefined) {
        document.getElementById('sdcard-free').textContent = formatBytes(data.free);
      }
    }).catch(function() {
      document.getElementById('sdcard-status').textContent = 'Error';
      document.getElementById('sdcard-free').textContent = '--';
    });
  }
  
  function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
  }
  
  loadStats();
  </script>
</body>
</html>
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
