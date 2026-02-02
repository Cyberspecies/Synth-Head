/*****************************************************************
 * @file PageLedPresetList.hpp
 * @brief LED Preset Manager - List Page with YAML Configuration
 * 
 * Features:
 * - LED preset list with drag-drop reordering
 * - Preset name with animation type tags
 * - 3-dot menu with Edit/Delete options
 * - New Preset button at top
 * - Delete confirmation popup
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char* PAGE_LED_PRESET_LIST = R"rawpage(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - LED Presets</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    .card, .card-body { overflow: visible !important; }
    
    .scene-header-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
      gap: 16px;
    }
    .scene-header-bar h2 { margin: 0; font-size: 1.1rem; flex: 1; }
    
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
    .btn:active { transform: scale(0.98); }
    .btn-primary { background: var(--accent); color: var(--bg-primary); }
    .btn-secondary { background: var(--bg-tertiary); color: var(--text-primary); border: 1px solid var(--border); }
    .btn-danger { background: var(--danger); color: #fff; }
    .btn-sm { padding: 7px 12px; font-size: 0.75rem; }

    .preset-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .preset-item {
      background: var(--bg-tertiary);
      border-radius: 10px;
      padding: 14px 16px;
      display: flex;
      align-items: center;
      gap: 12px;
      transition: all 0.2s;
      border: 2px solid transparent;
    }
    .preset-item:hover { border-color: var(--border); }
    .preset-item.dragging { opacity: 0.5; border-color: var(--accent); background: var(--accent-subtle); }
    .preset-item.drag-over { border-color: var(--accent); box-shadow: 0 0 10px rgba(255,107,0,0.3); }
    .preset-item.active { border-color: var(--success); background: rgba(0, 204, 102, 0.08); }

    .drag-handle {
      cursor: grab;
      padding: 6px;
      display: flex;
      flex-direction: column;
      gap: 3px;
      opacity: 0.4;
      transition: opacity 0.2s;
    }
    .drag-handle:hover { opacity: 1; }
    .drag-handle:active { cursor: grabbing; }
    .drag-handle-row { display: flex; gap: 3px; }
    .drag-handle-dot { width: 4px; height: 4px; background: var(--text-secondary); border-radius: 50%; }

    .preset-info { flex: 1; display: flex; flex-direction: column; gap: 4px; }
    .preset-name { font-size: 1rem; font-weight: 600; color: var(--text-primary); }
    .preset-tags { display: flex; gap: 6px; }
    .tag {
      font-size: 0.65rem;
      font-weight: 700;
      text-transform: uppercase;
      padding: 3px 8px;
      border-radius: 4px;
      letter-spacing: 0.5px;
    }
    .tag.led { color: #f59e0b; background: rgba(245, 158, 11, 0.15); }
    .tag.anim { color: #8b5cf6; background: rgba(139, 92, 246, 0.15); }

    .color-preview {
      width: 32px;
      height: 32px;
      border-radius: 6px;
      border: 2px solid var(--border);
      flex-shrink: 0;
    }

    .menu-btn {
      width: 36px;
      height: 36px;
      background: transparent;
      border: none;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      border-radius: 8px;
      transition: background 0.2s;
      position: relative;
      z-index: 1;
    }
    .preset-item:has(.dropdown-menu.show) { z-index: 9998; position: relative; }
    .menu-btn:hover { background: var(--bg-secondary); }
    .menu-dots { display: flex; flex-direction: column; gap: 3px; }
    .menu-dot { width: 4px; height: 4px; background: var(--text-secondary); border-radius: 50%; }

    .dropdown-menu {
      position: absolute;
      top: 100%;
      right: 0;
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 8px;
      min-width: 120px;
      box-shadow: 0 8px 24px rgba(0,0,0,0.5);
      z-index: 9999;
      display: none;
      overflow: visible;
    }
    .dropdown-menu.show { display: block; }
    .dropdown-item {
      padding: 10px 14px;
      font-size: 0.85rem;
      cursor: pointer;
      transition: background 0.15s;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .dropdown-item:hover { background: var(--bg-tertiary); }
    .dropdown-item.danger { color: var(--danger); }

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
      background: var(--bg-card);
      border-radius: 12px;
      padding: 24px;
      width: 90%;
      max-width: 380px;
    }
    .modal h3 { font-size: 1.1rem; margin-bottom: 16px; }
    .modal p { font-size: 0.9rem; color: var(--text-secondary); margin-bottom: 16px; }
    .modal-input {
      width: 100%;
      padding: 12px 14px;
      background: var(--bg-tertiary);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-primary);
      font-size: 0.95rem;
      margin-bottom: 16px;
    }
    .modal-input:focus { outline: none; border-color: var(--accent); }
    .modal-actions { display: flex; gap: 10px; justify-content: flex-end; }

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
      transform: translateY(100px);
      opacity: 0;
      transition: all 0.3s;
    }
    .toast.show { transform: translateY(0); opacity: 1; }
    .toast.success { background: var(--success); }
    .toast.error { background: var(--danger); }

    .empty-state {
      text-align: center;
      padding: 50px 20px;
      color: var(--text-secondary);
    }
    .empty-state .icon { font-size: 48px; margin-bottom: 12px; opacity: 0.3; }
    .empty-state p { font-size: 0.9rem; margin-bottom: 16px; }
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
      <a href="/advanced" style="display: inline-flex; align-items: center; gap: 6px; color: var(--text-muted); text-decoration: none; font-size: 0.85rem; margin-bottom: 12px;">&#x2190; Back to Advanced</a>
      
      <div class="card">
        <div class="card-header">
          <div class="scene-header-bar">
            <h2>LED Preset Manager</h2>
            <button class="btn btn-primary" onclick="showNewPresetModal()">+ New Preset</button>
          </div>
        </div>
        <div class="card-body">
          <div class="preset-list" id="presetList">
            <!-- Presets rendered here -->
          </div>
        </div>
      </div>
    </section>
    
    <footer><p>Lucidius - ARCOS Framework</p></footer>
  </div>
  
  <!-- New Preset Modal -->
  <div class="modal-overlay" id="newPresetModal">
    <div class="modal">
      <h3>Create LED Preset</h3>
      <input type="text" class="modal-input" id="newPresetName" placeholder="Preset name...">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideNewPresetModal()">Cancel</button>
        <button class="btn btn-primary" onclick="createPreset()">Create</button>
      </div>
    </div>
  </div>
  
  <!-- Delete Confirmation Modal -->
  <div class="modal-overlay" id="deleteModal">
    <div class="modal">
      <h3>Delete Preset?</h3>
      <p>This action cannot be undone.</p>
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideDeleteModal()">Cancel</button>
        <button class="btn btn-danger" onclick="confirmDelete()">Delete</button>
      </div>
    </div>
  </div>
  
  <div class="toast" id="toast"></div>
  
  <script>
  var presets = [];
  var activePresetId = -1;
  var deleteTargetId = null;

  function fetchPresets() {
    fetch('/api/ledpresets')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        presets = data.presets || [];
        activePresetId = data.activeId || -1;
        renderPresets();
      })
      .catch(function(err) {
        console.error('Preset fetch error:', err);
        presets = [];
        renderPresets();
      });
  }

  function renderPresets() {
    var list = document.getElementById('presetList');
    
    if (presets.length === 0) {
      list.innerHTML = '<div class="empty-state"><div class="icon">&#x1F4A1;</div><p>No LED presets yet</p><button class="btn btn-primary" onclick="showNewPresetModal()">Create First Preset</button></div>';
      return;
    }
    
    list.innerHTML = '';
    presets.forEach(function(preset, index) {
      var isActive = (preset.id === activePresetId);
      var activeClass = isActive ? 'active' : '';
      
      var colorStyle = 'background: rgb(' + preset.r + ',' + preset.g + ',' + preset.b + ');';
      
      var tagsHtml = '<span class="tag led">' + (preset.animation || 'Solid') + '</span>';
      
      var html = '<div class="preset-item ' + activeClass + '" data-id="' + preset.id + '" data-index="' + index + '" draggable="true">' +
        '<div class="drag-handle" title="Drag to reorder">' +
          '<div class="drag-handle-row"><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div></div>' +
          '<div class="drag-handle-row"><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div></div>' +
        '</div>' +
        '<div class="color-preview" style="' + colorStyle + '"></div>' +
        '<div class="preset-info">' +
          '<span class="preset-name">' + escapeHtml(preset.name) + '</span>' +
          '<div class="preset-tags">' + tagsHtml + '</div>' +
        '</div>' +
        '<button class="menu-btn" onclick="toggleMenu(event, ' + preset.id + ')">' +
          '<div class="menu-dots"><div class="menu-dot"></div><div class="menu-dot"></div><div class="menu-dot"></div></div>' +
          '<div class="dropdown-menu" id="menu-' + preset.id + '">' +
            '<div class="dropdown-item" onclick="editPreset(' + preset.id + ')">&#x270E; Edit</div>' +
            '<div class="dropdown-item danger" onclick="deletePreset(' + preset.id + ')">&#x1F5D1; Delete</div>' +
          '</div>' +
        '</button>' +
      '</div>';
      
      list.innerHTML += html;
    });
    
    setupDragDrop();
  }

  function escapeHtml(text) {
    var div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  function toggleMenu(event, id) {
    event.stopPropagation();
    document.querySelectorAll('.dropdown-menu').forEach(function(m) {
      if (m.id !== 'menu-' + id) m.classList.remove('show');
    });
    var menu = document.getElementById('menu-' + id);
    menu.classList.toggle('show');
  }

  document.addEventListener('click', function() {
    document.querySelectorAll('.dropdown-menu').forEach(function(m) { m.classList.remove('show'); });
  });

  function showNewPresetModal() {
    document.getElementById('newPresetName').value = '';
    document.getElementById('newPresetModal').classList.add('show');
    document.getElementById('newPresetName').focus();
  }

  function hideNewPresetModal() {
    document.getElementById('newPresetModal').classList.remove('show');
  }

  function createPreset() {
    var name = document.getElementById('newPresetName').value.trim();
    if (!name) {
      showToast('Please enter a name', 'error');
      return;
    }
    
    fetch('/api/ledpreset/create', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: name })
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        hideNewPresetModal();
        showToast('Preset created', 'success');
        fetchPresets();
        if (data.id) {
          setTimeout(function() { editPreset(data.id); }, 300);
        }
      } else {
        showToast(data.error || 'Failed to create', 'error');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }

  function editPreset(id) {
    window.location.href = '/advanced/ledpresets/edit?id=' + id;
  }

  function deletePreset(id) {
    deleteTargetId = id;
    document.getElementById('deleteModal').classList.add('show');
  }

  function hideDeleteModal() {
    document.getElementById('deleteModal').classList.remove('show');
    deleteTargetId = null;
  }

  function confirmDelete() {
    if (!deleteTargetId) return;
    
    fetch('/api/ledpreset/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: deleteTargetId })
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      hideDeleteModal();
      if (data.success) {
        showToast('Preset deleted', 'success');
        fetchPresets();
      } else {
        showToast('Failed to delete', 'error');
      }
    })
    .catch(function(err) {
      hideDeleteModal();
      showToast('Error: ' + err, 'error');
    });
  }

  function setupDragDrop() {
    var items = document.querySelectorAll('.preset-item');
    items.forEach(function(item) {
      item.addEventListener('dragstart', function(e) {
        item.classList.add('dragging');
        e.dataTransfer.setData('text/plain', item.dataset.index);
      });
      item.addEventListener('dragend', function() {
        item.classList.remove('dragging');
        document.querySelectorAll('.preset-item').forEach(function(i) { i.classList.remove('drag-over'); });
      });
      item.addEventListener('dragover', function(e) {
        e.preventDefault();
        item.classList.add('drag-over');
      });
      item.addEventListener('dragleave', function() {
        item.classList.remove('drag-over');
      });
      item.addEventListener('drop', function(e) {
        e.preventDefault();
        var fromIndex = parseInt(e.dataTransfer.getData('text/plain'));
        var toIndex = parseInt(item.dataset.index);
        if (fromIndex !== toIndex) {
          reorderPresets(fromIndex, toIndex);
        }
      });
    });
  }

  function reorderPresets(fromIndex, toIndex) {
    var ids = presets.map(function(p) { return p.id; });
    var moved = ids.splice(fromIndex, 1)[0];
    ids.splice(toIndex, 0, moved);
    
    fetch('/api/ledpresets/reorder', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ order: ids })
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        fetchPresets();
      }
    })
    .catch(function(err) {
      console.error('Reorder error:', err);
    });
  }

  function showToast(message, type) {
    var toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className = 'toast ' + type + ' show';
    setTimeout(function() { toast.className = 'toast'; }, 3000);
  }

  fetchPresets();
  </script>
</body>
</html>
)rawpage";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
