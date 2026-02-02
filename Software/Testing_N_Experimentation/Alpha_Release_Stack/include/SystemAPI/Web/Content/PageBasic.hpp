/*****************************************************************
 * @file PageBasic.hpp
 * @brief Basic tab page content
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char PAGE_BASIC[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Basic</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    .preset-list { display: flex; flex-direction: column; gap: 6px; }
    .preset-item { 
      display: flex; 
      align-items: center; 
      padding: 14px 16px; 
      background: var(--bg-tertiary); 
      border-radius: 10px; 
      border: 2px solid transparent;
      transition: all 0.2s; 
    }
    .preset-item:hover { background: var(--bg-secondary); }
    .preset-item.active { background: rgba(0, 204, 102, 0.08); border-color: rgba(0, 204, 102, 0.3); }
    .preset-name { 
      flex: 1;
      font-weight: 600; 
      font-size: 1.1rem; 
      color: var(--text-primary);
    }
    .preset-indicators {
      display: flex;
      gap: 8px;
      margin-right: 16px;
    }
    .indicator {
      font-size: 0.7rem;
      font-weight: 700;
      text-transform: uppercase;
      padding: 4px 10px;
      border-radius: 4px;
      letter-spacing: 0.5px;
      transition: all 0.2s;
    }
    .indicator.display { color: var(--success); background: rgba(0, 204, 102, 0.15); }
    .indicator.display.off { color: var(--text-muted); background: rgba(128, 128, 128, 0.1); opacity: 0.5; }
    .indicator.hidden { display: none; }
    .apply-btn { 
      padding: 8px 16px; 
      background: var(--accent); 
      color: var(--bg-primary); 
      border: none; 
      border-radius: 6px; 
      font-size: 0.8rem; 
      font-weight: 600; 
      cursor: pointer; 
      transition: all 0.2s; 
      min-width: 70px;
    }
    .apply-btn:hover { background: var(--accent-hover); transform: scale(1.02); }
    .apply-btn:active { transform: scale(0.98); }
    .legend { 
      display: flex; 
      gap: 20px; 
      margin-bottom: 16px; 
      padding: 12px 16px; 
      background: var(--bg-tertiary); 
      border-radius: 8px; 
      font-size: 0.75rem; 
    }
    .legend-item { display: flex; align-items: center; gap: 8px; color: var(--text-secondary); }
    .legend-dot { width: 10px; height: 10px; border-radius: 50%; }
    .legend-dot.display { background: var(--success); }
    @media (max-width: 600px) {
      .preset-item { padding: 12px; flex-wrap: wrap; }
      .preset-name { font-size: 1rem; width: 100%; margin-bottom: 8px; }
      .preset-indicators { margin-right: 10px; }
      .apply-btn { min-width: 60px; padding: 8px 12px; }
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
      <a href="/" class="tab active">Basic</a>
      <a href="/advanced" class="tab">Advanced</a>
      <a href="/system" class="tab">System</a>
      <a href="/settings" class="tab">Settings</a>
    </nav>
    
    <section class="tab-content active">
      <div class="card" style="max-width: 100%;">
        <div class="card-header">
          <h2>Animation Presets</h2>
        </div>
        <div class="card-body">
          <div class="legend">
            <div class="legend-item">
              <div class="legend-dot display"></div>
              <span>Active</span>
            </div>
          </div>
          <div class="preset-list" id="preset-list">
            <div class="preset-item">
              <span class="preset-name">Loading...</span>
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
  
  <script>
  var scenes = [];
  var activeSceneId = -1;
  
  function fetchScenes() {
    fetch('/api/scenes')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        scenes = data.scenes || [];
        // Find active scene
        for (var i = 0; i < scenes.length; i++) {
          if (scenes[i].active) {
            activeSceneId = scenes[i].id;
            break;
          }
        }
        renderPresets();
      })
      .catch(function(err) {
        console.error('Scene fetch error:', err);
        // Fallback demo data
        scenes = [
          {id: 1, name: 'Gyro Eyes Default'},
          {id: 2, name: 'Static Image'},
          {id: 3, name: 'Demo Scene'}
        ];
        renderPresets();
      });
  }
  
  function renderPresets() {
    var list = document.getElementById('preset-list');
    list.innerHTML = '';
    
    if (scenes.length === 0) {
      list.innerHTML = '<div class="preset-item"><span class="preset-name" style="color:var(--text-muted)">No scenes configured. Create scenes in Advanced tab.</span></div>';
      return;
    }
    
    scenes.forEach(function(scene) {
      var isActive = (scene.id === activeSceneId);
      
      // Highlight class based on active state
      var activeClass = isActive ? 'active' : '';
      
      // Build indicator HTML (just show active state)
      var indicatorsHtml = '';
      if (isActive) {
        indicatorsHtml = '<span class="indicator display">Active</span>';
      }
      
      // Effects-only indicator
      if (scene.effectsOnly) {
        indicatorsHtml += '<span class="indicator" style="color:var(--accent);background:rgba(255,107,0,0.15);">Effect</span>';
      }
      
      var html = '<div class="preset-item ' + activeClass + '" data-id="' + scene.id + '">' +
        '<span class="preset-name">' + escapeHtml(scene.name) + '</span>' +
        '<div class="preset-indicators">' + indicatorsHtml + '</div>' +
        '<button class="apply-btn" onclick="applyScene(' + scene.id + ')">Apply</button>' +
      '</div>';
      
      list.innerHTML += html;
    });
  }
  
  function escapeHtml(text) {
    var div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }
  
  function applyScene(id) {
    fetch('/api/scene/activate', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({id: id})
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        showToast('Scene applied', 'success');
        activeSceneId = id;
        renderPresets();
        // Re-fetch to ensure sync with server
        setTimeout(fetchScenes, 500);
      } else {
        showToast('Failed to apply scene', 'error');
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
    setTimeout(function() {
      toast.className = 'toast';
    }, 3000);
  }
  
  fetchScenes();
  </script>
</body>
</html>
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
