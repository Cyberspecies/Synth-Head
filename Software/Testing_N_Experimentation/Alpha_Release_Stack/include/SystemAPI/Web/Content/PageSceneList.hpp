/*****************************************************************
 * @file PageSceneList.hpp
 * @brief Scene Manager - List Page with Drag-Drop Reorder
 * 
 * Features:
 * - Scene list with drag-drop reordering (3x3 dot grid icon)
 * - Scene name with tags (LEDs/Display/Both)
 * - 3-dot menu with Edit/Delete options
 * - New Scene button at top
 * - Delete confirmation popup
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

namespace SystemAPI {
namespace Web {
namespace Content {

inline const char* PAGE_SCENE_LIST = R"rawpage(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - Scenes</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    /* Override card overflow to allow dropdown menus to escape */
    .card, .card-body {
      overflow: visible !important;
    }
    
    /* Scene List Container */
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

    /* Scene List */
    .scene-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .scene-item {
      background: var(--bg-tertiary);
      border-radius: 10px;
      padding: 14px 16px;
      display: flex;
      align-items: center;
      gap: 12px;
      transition: all 0.2s;
      border: 2px solid transparent;
    }
    .scene-item:hover { border-color: var(--border); }
    .scene-item.dragging { 
      opacity: 0.5; 
      border-color: var(--accent);
      background: var(--accent-subtle);
    }
    .scene-item.drag-over {
      border-color: var(--accent);
      box-shadow: 0 0 10px rgba(255,107,0,0.3);
    }
    .scene-item.active {
      border-color: var(--success);
      background: rgba(0, 204, 102, 0.08);
    }

    /* Drag Handle */
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
    .drag-handle-row {
      display: flex;
      gap: 3px;
    }
    .drag-handle-dot {
      width: 4px;
      height: 4px;
      background: var(--text-secondary);
      border-radius: 50%;
    }

    /* Scene Info */
    .scene-info {
      flex: 1;
      display: flex;
      flex-direction: column;
      gap: 4px;
    }
    .scene-name {
      font-size: 1rem;
      font-weight: 600;
      color: var(--text-primary);
    }
    .scene-tags {
      display: flex;
      gap: 6px;
    }
    .tag {
      font-size: 0.65rem;
      font-weight: 700;
      text-transform: uppercase;
      padding: 3px 8px;
      border-radius: 4px;
      letter-spacing: 0.5px;
    }
    .tag.display { color: var(--success); background: rgba(0, 204, 102, 0.15); }
    .tag.effects { color: #3b82f6; background: rgba(59, 130, 246, 0.15); }

    /* Menu Button */
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
    .scene-item:has(.dropdown-menu.show) {
      z-index: 9998;
      position: relative;
    }
    .menu-btn:hover { background: var(--bg-secondary); }
    .menu-dots {
      display: flex;
      flex-direction: column;
      gap: 3px;
    }
    .menu-dot {
      width: 4px;
      height: 4px;
      background: var(--text-secondary);
      border-radius: 50%;
    }

    /* Dropdown Menu */
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

    /* Modal */
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
      transform: translateY(100px);
      opacity: 0;
      transition: all 0.3s;
    }
    .toast.show { transform: translateY(0); opacity: 1; }
    .toast.success { background: var(--success); }
    .toast.error { background: var(--danger); }

    /* Empty State */
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
            <h2>Scene Manager</h2>
            <button class="btn btn-primary" onclick="showNewSceneModal()">+ New Scene</button>
          </div>
        </div>
        <div class="card-body">
          <div class="scene-list" id="sceneList">
            <!-- Scenes will be rendered here -->
          </div>
          <div class="empty-state" id="emptyState" style="display:none;">
            <div class="icon">&#x25A1;</div>
            <p>No scenes created yet</p>
            <button class="btn btn-primary" onclick="showNewSceneModal()">Create Your First Scene</button>
          </div>
        </div>
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
      <input type="text" class="modal-input" id="newSceneName" placeholder="Scene name..." maxlength="32">
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
      <input type="text" class="modal-input" id="renameInput" placeholder="New name..." maxlength="32">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalRename')">Cancel</button>
        <button class="btn btn-primary" onclick="renameScene()">Rename</button>
      </div>
    </div>
  </div>

  <!-- Delete Confirmation Modal -->
  <div class="modal-overlay" id="modalDelete">
    <div class="modal">
      <h3>Delete Scene</h3>
      <p>Are you sure you want to delete "<span id="deleteSceneName"></span>"? This action cannot be undone.</p>
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalDelete')">Cancel</button>
        <button class="btn btn-danger" onclick="confirmDeleteScene()">Delete</button>
      </div>
    </div>
  </div>

  <div id="toast" class="toast"></div>

  <script>
  // State
  var scenes = [];
  var activeSceneId = null;
  var selectedSceneId = null;
  var draggedItem = null;

  // Load scenes from server
  function loadScenes() {
    fetch('/api/scenes')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        scenes = data.scenes || [];
        activeSceneId = data.activeId || null;
        renderScenes();
      })
      .catch(function(err) {
        console.error('Failed to load scenes:', err);
        showToast('Failed to load scenes', 'error');
      });
  }

  // Render scene list
  function renderScenes() {
    var list = document.getElementById('sceneList');
    var empty = document.getElementById('emptyState');
    
    if (scenes.length === 0) {
      list.innerHTML = '';
      empty.style.display = 'block';
      return;
    }
    
    empty.style.display = 'none';
    list.innerHTML = scenes.map(function(s, idx) {
      var isActive = s.id === activeSceneId;
      var tags = [];
      if (s.displayEnabled) tags.push('<span class="tag display">Display</span>');
      if (s.effectsOnly) tags.push('<span class="tag effects">Effects</span>');
      if (tags.length === 0) tags.push('<span class="tag" style="opacity:0.5">None</span>');
      
      return '<div class="scene-item' + (isActive ? ' active' : '') + '" draggable="true" data-id="' + s.id + '" data-index="' + idx + '">' +
        '<div class="drag-handle" title="Drag to reorder">' +
          '<div class="drag-handle-row"><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div></div>' +
          '<div class="drag-handle-row"><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div></div>' +
          '<div class="drag-handle-row"><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div><div class="drag-handle-dot"></div></div>' +
        '</div>' +
        '<div class="scene-info">' +
          '<div class="scene-name">' + escapeHtml(s.name) + '</div>' +
          '<div class="scene-tags">' + tags.join('') + '</div>' +
        '</div>' +
        '<button class="menu-btn" onclick="toggleMenu(event, ' + s.id + ')">' +
          '<div class="menu-dots"><div class="menu-dot"></div><div class="menu-dot"></div><div class="menu-dot"></div></div>' +
          '<div class="dropdown-menu" id="menu-' + s.id + '">' +
            '<div class="dropdown-item" onclick="editScene(' + s.id + ')">&#x270E; Edit</div>' +
            '<div class="dropdown-item danger" onclick="showDeleteConfirm(' + s.id + ')">&#x2716; Delete</div>' +
          '</div>' +
        '</button>' +
      '</div>';
    }).join('');
    
    // Setup drag events
    setupDragDrop();
  }

  // Setup drag and drop
  function setupDragDrop() {
    var items = document.querySelectorAll('.scene-item');
    items.forEach(function(item) {
      item.addEventListener('dragstart', onDragStart);
      item.addEventListener('dragend', onDragEnd);
      item.addEventListener('dragover', onDragOver);
      item.addEventListener('dragleave', onDragLeave);
      item.addEventListener('drop', onDrop);
    });
  }

  function onDragStart(e) {
    draggedItem = this;
    this.classList.add('dragging');
    e.dataTransfer.effectAllowed = 'move';
    e.dataTransfer.setData('text/plain', this.dataset.index);
  }

  function onDragEnd(e) {
    this.classList.remove('dragging');
    document.querySelectorAll('.scene-item').forEach(function(el) {
      el.classList.remove('drag-over');
    });
    draggedItem = null;
  }

  function onDragOver(e) {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
    if (this !== draggedItem) {
      this.classList.add('drag-over');
    }
  }

  function onDragLeave(e) {
    this.classList.remove('drag-over');
  }

  function onDrop(e) {
    e.preventDefault();
    this.classList.remove('drag-over');
    
    if (this === draggedItem) return;
    
    var fromIndex = parseInt(e.dataTransfer.getData('text/plain'));
    var toIndex = parseInt(this.dataset.index);
    
    // Reorder locally
    var moved = scenes.splice(fromIndex, 1)[0];
    scenes.splice(toIndex, 0, moved);
    
    // Update indices and re-render
    renderScenes();
    
    // Save new order to server
    saveSceneOrder();
  }

  function saveSceneOrder() {
    var order = scenes.map(function(s) { return s.id; });
    fetch('/api/scenes/reorder', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({order: order})
    }).catch(function(err) {
      console.error('Failed to save order:', err);
    });
  }

  // Menu toggle
  function toggleMenu(e, sceneId) {
    e.stopPropagation();
    // Close all other menus
    document.querySelectorAll('.dropdown-menu').forEach(function(m) {
      if (m.id !== 'menu-' + sceneId) m.classList.remove('show');
    });
    var menu = document.getElementById('menu-' + sceneId);
    menu.classList.toggle('show');
    selectedSceneId = sceneId;
  }

  // Close menus on click outside
  document.addEventListener('click', function() {
    document.querySelectorAll('.dropdown-menu').forEach(function(m) {
      m.classList.remove('show');
    });
  });

  // Edit scene - navigate to new display config page
  function editScene(id) {
    window.location.href = '/display-config?id=' + id;
  }

  // Show delete confirmation
  function showDeleteConfirm(id) {
    selectedSceneId = id;
    var scene = scenes.find(function(s) { return s.id === id; });
    document.getElementById('deleteSceneName').textContent = scene ? scene.name : '';
    showModal('modalDelete');
  }

  // Confirm delete
  function confirmDeleteScene() {
    if (!selectedSceneId) return;
    
    fetch('/api/scene/delete', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({id: selectedSceneId})
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        hideModal('modalDelete');
        showToast('Scene deleted', 'success');
        loadScenes();
      } else {
        showToast(data.error || 'Failed to delete', 'error');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }

  // New scene modal
  function showNewSceneModal() {
    document.getElementById('newSceneName').value = '';
    showModal('modalNew');
    document.getElementById('newSceneName').focus();
  }

  // Create scene
  function createScene() {
    var name = document.getElementById('newSceneName').value.trim();
    if (!name) {
      showToast('Please enter a name', 'error');
      return;
    }
    
    fetch('/api/scene/create', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({name: name})
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        hideModal('modalNew');
        showToast('Scene created', 'success');
        // Navigate to display config page for new scene
        window.location.href = '/display-config?id=' + data.id;
      } else {
        showToast(data.error || 'Failed to create', 'error');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }

  // Rename scene
  function showRenameModal() {
    var scene = scenes.find(function(s) { return s.id === selectedSceneId; });
    document.getElementById('renameInput').value = scene ? scene.name : '';
    showModal('modalRename');
    document.getElementById('renameInput').focus();
  }

  function renameScene() {
    var name = document.getElementById('renameInput').value.trim();
    if (!name) {
      showToast('Please enter a name', 'error');
      return;
    }
    
    fetch('/api/scene/rename', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({id: selectedSceneId, name: name})
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.success) {
        hideModal('modalRename');
        showToast('Scene renamed', 'success');
        loadScenes();
      } else {
        showToast(data.error || 'Failed to rename', 'error');
      }
    })
    .catch(function(err) {
      showToast('Error: ' + err, 'error');
    });
  }

  // Modal helpers
  function showModal(id) { document.getElementById(id).classList.add('show'); }
  function hideModal(id) { document.getElementById(id).classList.remove('show'); }

  // Toast
  function showToast(msg, type) {
    var t = document.getElementById('toast');
    t.textContent = msg;
    t.className = 'toast ' + type + ' show';
    setTimeout(function() { t.classList.remove('show'); }, 3000);
  }

  // Escape HTML
  function escapeHtml(text) {
    var div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  // Enter key handlers
  document.getElementById('newSceneName').addEventListener('keypress', function(e) {
    if (e.key === 'Enter') createScene();
  });
  document.getElementById('renameInput').addEventListener('keypress', function(e) {
    if (e.key === 'Enter') renameScene();
  });

  // Initialize
  loadScenes();
  </script>
</body>
</html>
)rawpage";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
