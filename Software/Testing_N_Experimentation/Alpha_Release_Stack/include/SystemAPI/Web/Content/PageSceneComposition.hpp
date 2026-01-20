/*****************************************************************
 * @file PageSceneComposition.hpp
 * @brief Scene Composition Web Page with Full CRUD Operations
 * 
 * Features:
 * - List saved scenes from SD card
 * - Create new scenes
 * - Save scenes to SD card
 * - Delete scenes
 * - Rename scenes
 * - Auto-generated settings from parameter definitions
 * - Real-time preview controls
 * 
 * @author ARCOS
 * @version 3.0
 *****************************************************************/

#pragma once

#include <string>

namespace SystemAPI {
namespace Web {
namespace Content {

/**
 * @brief Scene Composition Page HTML with full CRUD operations
 */
inline const char* PAGE_SCENE_COMPOSITION = R"rawpage(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Scene Composition - Synth-Head</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    :root { --primary: #ff6b00; --bg-primary: #0a0a0a; --bg-secondary: #111111; --bg-tertiary: #1a1a1a; --border: #333; --text: #fff; --text-dim: #888; --success: #22c55e; --danger: #ef4444; }
    * { box-sizing: border-box; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: var(--bg-primary); color: var(--text); margin: 0; }
    .navbar { background: var(--bg-secondary); padding: 12px 24px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid var(--border); }
    .navbar .logo { font-weight: 700; font-size: 18px; color: var(--primary); }
    .nav-links { display: flex; gap: 16px; }
    .nav-links a { color: var(--text-dim); text-decoration: none; font-size: 14px; padding: 6px 12px; border-radius: 4px; transition: all 0.15s; }
    .nav-links a:hover { color: var(--text); background: var(--bg-tertiary); }
    .nav-links a.active { color: var(--primary); background: rgba(255,107,0,0.1); }
    .container { max-width: 1400px; margin: 0 auto; padding: 16px; }
    .scene-layout { display: grid; grid-template-columns: 300px 1fr; gap: 16px; height: calc(100vh - 100px); }
    .sidebar { background: var(--bg-secondary); border-radius: 8px; display: flex; flex-direction: column; overflow: hidden; }
    .sidebar-header { padding: 16px; border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; align-items: center; }
    .sidebar-header h3 { margin: 0; font-size: 15px; }
    .scene-list { flex: 1; overflow-y: auto; padding: 8px; }
    .scene-item { padding: 12px; margin-bottom: 4px; border-radius: 6px; cursor: pointer; display: flex; justify-content: space-between; align-items: center; transition: background 0.15s; }
    .scene-item:hover { background: var(--bg-tertiary); }
    .scene-item.active { background: var(--primary); }
    .scene-item.active .scene-meta { color: rgba(255,255,255,0.8); }
    .scene-info { flex: 1; min-width: 0; }
    .scene-name { font-weight: 500; font-size: 14px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .scene-meta { font-size: 11px; color: var(--text-dim); margin-top: 2px; }
    .scene-actions { display: flex; gap: 4px; opacity: 0; transition: opacity 0.15s; }
    .scene-item:hover .scene-actions, .scene-item.active .scene-actions { opacity: 1; }
    .icon-btn { width: 28px; height: 28px; border: none; border-radius: 4px; cursor: pointer; display: flex; align-items: center; justify-content: center; font-size: 12px; transition: background 0.15s; }
    .icon-btn.edit { background: var(--bg-tertiary); color: var(--text); }
    .icon-btn.delete { background: rgba(239,68,68,0.2); color: var(--danger); }
    .icon-btn:hover { filter: brightness(1.2); }
    .sidebar-footer { padding: 12px; border-top: 1px solid var(--border); }
    .main-content { display: flex; flex-direction: column; gap: 16px; }
    .panel { background: var(--bg-secondary); border-radius: 8px; overflow: hidden; }
    .panel-header { padding: 14px 16px; border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; align-items: center; }
    .panel-header h3 { margin: 0; font-size: 15px; }
    .animation-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(140px, 1fr)); gap: 8px; padding: 12px; max-height: 150px; overflow-y: auto; }
    .anim-card { background: var(--bg-tertiary); border: 2px solid transparent; border-radius: 6px; padding: 10px; cursor: pointer; transition: all 0.15s; }
    .anim-card:hover { border-color: var(--border); }
    .anim-card.selected { border-color: var(--primary); background: rgba(255,107,0,0.1); }
    .anim-card .name { font-size: 13px; font-weight: 500; }
    .anim-card .cat { font-size: 10px; color: var(--text-dim); margin-top: 2px; }
    .params-panel { flex: 1; display: flex; flex-direction: column; }
    .params-content { flex: 1; overflow-y: auto; padding: 16px; }
    .param-group { margin-bottom: 16px; }
    .param-group-title { font-size: 12px; font-weight: 600; color: var(--primary); padding-bottom: 8px; margin-bottom: 8px; border-bottom: 1px solid var(--border); text-transform: uppercase; letter-spacing: 0.5px; }
    .param-row { display: flex; align-items: center; padding: 8px 0; }
    .param-row label { flex: 0 0 35%; font-size: 13px; color: var(--text); }
    .param-control { flex: 1; display: flex; align-items: center; gap: 8px; }
    input[type="range"] { flex: 1; accent-color: var(--primary); }
    input[type="color"] { width: 36px; height: 28px; border: none; border-radius: 4px; cursor: pointer; }
    select { flex: 1; padding: 6px 10px; background: var(--bg-tertiary); border: 1px solid var(--border); border-radius: 4px; color: var(--text); font-size: 13px; }
    .value-display { min-width: 45px; text-align: right; font-size: 12px; color: var(--text-dim); font-family: monospace; }
    .toggle-switch { position: relative; width: 40px; height: 22px; }
    .toggle-switch input { display: none; }
    .toggle-slider { position: absolute; inset: 0; background: var(--bg-tertiary); border-radius: 11px; cursor: pointer; transition: background 0.2s; }
    .toggle-slider:before { content: ''; position: absolute; width: 16px; height: 16px; left: 3px; bottom: 3px; background: #fff; border-radius: 50%; transition: transform 0.2s; }
    .toggle-switch input:checked + .toggle-slider { background: var(--primary); }
    .toggle-switch input:checked + .toggle-slider:before { transform: translateX(18px); }
    .no-params { text-align: center; padding: 40px; color: var(--text-dim); font-size: 13px; }
    .preview-bar { display: flex; align-items: center; gap: 12px; padding: 12px 16px; background: var(--bg-tertiary); border-radius: 8px; }
    .preview-bar .status { flex: 1; font-size: 13px; color: var(--text-dim); }
    .preview-bar .status.active { color: var(--success); }
    .btn { padding: 8px 16px; border: none; border-radius: 6px; cursor: pointer; font-size: 13px; font-weight: 500; transition: all 0.15s; display: inline-flex; align-items: center; gap: 6px; }
    .btn:hover { filter: brightness(1.1); }
    .btn:active { transform: scale(0.98); }
    .btn-primary { background: var(--primary); color: #fff; }
    .btn-secondary { background: var(--bg-tertiary); color: var(--text); }
    .btn-success { background: var(--success); color: #fff; }
    .btn-danger { background: var(--danger); color: #fff; }
    .btn-sm { padding: 6px 12px; font-size: 12px; }
    .modal-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.7); display: none; align-items: center; justify-content: center; z-index: 1000; }
    .modal-overlay.show { display: flex; }
    .modal { background: var(--bg-secondary); border-radius: 12px; padding: 24px; width: 90%; max-width: 400px; box-shadow: 0 20px 40px rgba(0,0,0,0.5); }
    .modal h3 { margin: 0 0 16px 0; font-size: 18px; }
    .modal input[type="text"] { width: 100%; padding: 10px 12px; background: var(--bg-tertiary); border: 1px solid var(--border); border-radius: 6px; color: var(--text); font-size: 14px; margin-bottom: 16px; }
    .modal input:focus { outline: none; border-color: var(--primary); }
    .modal-actions { display: flex; gap: 8px; justify-content: flex-end; }
    .toast { position: fixed; bottom: 20px; right: 20px; padding: 12px 20px; border-radius: 8px; color: #fff; font-size: 14px; z-index: 2000; animation: slideIn 0.3s ease; }
    .toast.success { background: var(--success); }
    .toast.error { background: var(--danger); }
    @keyframes slideIn { from { transform: translateY(20px); opacity: 0; } to { transform: translateY(0); opacity: 1; } }
  </style>
</head>
<body>
  <nav class="navbar">
    <div class="logo">Synth-Head</div>
    <div class="nav-links">
      <a href="/basic">Basic</a>
      <a href="/system">System</a>
      <a href="/scene" class="active">Scene</a>
      <a href="/sprites">Sprites</a>
      <a href="/settings">Settings</a>
    </div>
  </nav>

  <main class="container">
    <div class="scene-layout">
      <div class="sidebar">
        <div class="sidebar-header">
          <h3>Saved Scenes</h3>
          <button class="btn btn-primary btn-sm" onclick="showNewSceneModal()">+ New</button>
        </div>
        <div class="scene-list" id="sceneList"><div class="no-params">Loading...</div></div>
        <div class="sidebar-footer">
          <button class="btn btn-secondary btn-sm" style="width:100%" onclick="refreshScenes()">Refresh</button>
        </div>
      </div>

      <div class="main-content">
        <div class="panel">
          <div class="panel-header"><h3>Animation Sets</h3></div>
          <div class="animation-grid" id="animationGrid"><div class="no-params">Loading...</div></div>
        </div>

        <div class="panel params-panel">
          <div class="panel-header">
            <h3 id="paramsTitle">Parameters</h3>
            <button class="btn btn-secondary btn-sm" onclick="resetParams()">Reset</button>
          </div>
          <div class="params-content" id="paramsContent"><div class="no-params">Select an animation</div></div>
        </div>

        <div class="preview-bar">
          <span class="status" id="previewStatus">Stopped</span>
          <button class="btn btn-primary" id="btnPreview" onclick="togglePreview()">Play</button>
          <button class="btn btn-secondary" onclick="stopPreview()">Stop</button>
          <div style="flex:1"></div>
          <button class="btn btn-success" onclick="saveCurrentScene()">Save Scene</button>
        </div>
      </div>
    </div>
  </main>

  <div class="modal-overlay" id="modalNewScene">
    <div class="modal">
      <h3>Create New Scene</h3>
      <input type="text" id="newSceneName" placeholder="Scene name..." maxlength="32">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalNewScene')">Cancel</button>
        <button class="btn btn-primary" onclick="createNewScene()">Create</button>
      </div>
    </div>
  </div>

  <div class="modal-overlay" id="modalRename">
    <div class="modal">
      <h3>Rename Scene</h3>
      <input type="text" id="renameSceneName" placeholder="New name..." maxlength="32">
      <input type="hidden" id="renameSceneId">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalRename')">Cancel</button>
        <button class="btn btn-primary" onclick="confirmRename()">Rename</button>
      </div>
    </div>
  </div>

  <div class="modal-overlay" id="modalDelete">
    <div class="modal">
      <h3>Delete Scene?</h3>
      <p style="color:var(--text-dim);margin:0 0 16px">This cannot be undone.</p>
      <input type="hidden" id="deleteSceneId">
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="hideModal('modalDelete')">Cancel</button>
        <button class="btn btn-danger" onclick="confirmDelete()">Delete</button>
      </div>
    </div>
  </div>

  <script>
    let savedScenes=[], animationSets=[], selectedSceneId=null, selectedAnimSetId=null, currentParams=[], isPlaying=false;
    document.addEventListener('DOMContentLoaded', function() { loadScenes(); loadAnimationSets(); });
    
    async function loadScenes() {
      try {
        const r = await fetch('/api/scenes');
        const d = await r.json();
        savedScenes = (d.scenes || []).map(function(s) { return { id: String(s.id), name: s.name, animSet: s.animSetId || '', type: s.type }; });
        renderSceneList();
      } catch(e) {
        document.getElementById('sceneList').innerHTML = '<div class="no-params">Failed</div>';
      }
    }
    
    function renderSceneList() {
      const c = document.getElementById('sceneList');
      if (!savedScenes.length) { c.innerHTML = '<div class="no-params">No scenes</div>'; return; }
      c.innerHTML = savedScenes.map(function(s) {
        return '<div class="scene-item ' + (s.id === selectedSceneId ? 'active' : '') + '" onclick="selectScene(\'' + s.id + '\')">' +
          '<div class="scene-info"><div class="scene-name">' + esc(s.name) + '</div><div class="scene-meta">' + (s.animSet || 'None') + '</div></div>' +
          '<div class="scene-actions"><button class="icon-btn edit" onclick="event.stopPropagation();showRenameModal(\'' + s.id + '\',\'' + esc(s.name) + '\')">E</button>' +
          '<button class="icon-btn delete" onclick="event.stopPropagation();showDeleteModal(\'' + s.id + '\')">X</button></div></div>';
      }).join('');
    }
    
    async function selectScene(id) {
      selectedSceneId = id;
      renderSceneList();
      try {
        const r = await fetch('/api/scene/get?id=' + id);
        const d = await r.json();
        if (d.scene && d.scene.animSet) {
          selectedAnimSetId = d.scene.animSet;
          renderAnimationGrid();
          await loadParams(selectedAnimSetId);
          if (d.scene.params) applyParams(d.scene.params);
        }
      } catch(e) {}
    }
    
    function applyParams(sp) {
      sp.forEach(function(p) {
        var param = currentParams.find(function(x) { return x.id === p.id; });
        if (param) param.value = p.value;
        var el = document.getElementById('param_' + p.id);
        if (el) { if (el.type === 'checkbox') el.checked = p.value; else el.value = p.value; }
        var v = document.getElementById('val_' + p.id);
        if (v) v.textContent = p.value;
      });
    }
    
    function showNewSceneModal() { document.getElementById('newSceneName').value = ''; showModal('modalNewScene'); document.getElementById('newSceneName').focus(); }
    
    async function createNewScene() {
      var n = document.getElementById('newSceneName').value.trim();
      if (!n) { toast('Enter name', 'error'); return; }
      try {
        var r = await fetch('/api/scene/create', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: n, type: 0 }) });
        var d = await r.json();
        if (d.success) { hideModal('modalNewScene'); toast('Created!', 'success'); await loadScenes(); selectScene(String(d.id)); }
        else toast(d.error || 'Failed', 'error');
      } catch(e) { toast('Error', 'error'); }
    }
    
    function showRenameModal(id, name) { document.getElementById('renameSceneId').value = id; document.getElementById('renameSceneName').value = name; showModal('modalRename'); document.getElementById('renameSceneName').focus(); }
    
    async function confirmRename() {
      var id = document.getElementById('renameSceneId').value;
      var n = document.getElementById('renameSceneName').value.trim();
      if (!n) { toast('Enter name', 'error'); return; }
      try {
        var r = await fetch('/api/scene/rename', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id: parseInt(id), name: n }) });
        var d = await r.json();
        if (d.success) { hideModal('modalRename'); toast('Renamed!', 'success'); loadScenes(); }
        else toast(d.error || 'Failed', 'error');
      } catch(e) { toast('Error', 'error'); }
    }
    
    function showDeleteModal(id) { document.getElementById('deleteSceneId').value = id; showModal('modalDelete'); }
    
    async function confirmDelete() {
      var id = document.getElementById('deleteSceneId').value;
      try {
        var r = await fetch('/api/scene/delete', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id: parseInt(id) }) });
        var d = await r.json();
        if (d.success) { hideModal('modalDelete'); toast('Deleted', 'success'); if (selectedSceneId === id) selectedSceneId = null; loadScenes(); }
        else toast(d.error || 'Failed', 'error');
      } catch(e) { toast('Error', 'error'); }
    }
    
    async function saveCurrentScene() {
      if (!selectedSceneId) { showNewSceneModal(); return; }
      try {
        var r = await fetch('/api/scene/save', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id: parseInt(selectedSceneId), animSetId: selectedAnimSetId, params: currentParams.map(function(p) { return { id: p.id, value: p.value }; }) }) });
        var d = await r.json();
        if (d.success) { toast('Saved!', 'success'); loadScenes(); }
        else toast(d.error || 'Failed', 'error');
      } catch(e) { toast('Error', 'error'); }
    }
    
    function refreshScenes() { loadScenes(); toast('Refreshed', 'success'); }
    
    async function loadAnimationSets() {
      try {
        var r = await fetch('/api/animation/sets');
        var d = await r.json();
        animationSets = d.sets || [];
        renderAnimationGrid();
      } catch(e) {}
    }
    
    function renderAnimationGrid() {
      var c = document.getElementById('animationGrid');
      if (!animationSets.length) { c.innerHTML = '<div class="no-params">None</div>'; return; }
      c.innerHTML = animationSets.map(function(a) {
        return '<div class="anim-card ' + (a.id === selectedAnimSetId ? 'selected' : '') + '" onclick="selectAnimSet(\'' + a.id + '\')">' +
          '<div class="name">' + esc(a.name) + '</div><div class="cat">' + esc(a.category || 'General') + '</div></div>';
      }).join('');
    }
    
    async function selectAnimSet(id) { selectedAnimSetId = id; renderAnimationGrid(); await loadParams(id); }
    
    async function loadParams(setId) {
      try {
        var r = await fetch('/api/animation/params?set=' + setId);
        var d = await r.json();
        currentParams = d.params || [];
        renderParams();
      } catch(e) { document.getElementById('paramsContent').innerHTML = '<div class="no-params">Failed</div>'; }
    }
    
    function renderParams() {
      var c = document.getElementById('paramsContent');
      if (!currentParams.length) { c.innerHTML = '<div class="no-params">No params</div>'; return; }
      var groups = {};
      currentParams.forEach(function(p) { var cat = catName(p.category); if (!groups[cat]) groups[cat] = []; groups[cat].push(p); });
      var h = '';
      for (var cat in groups) {
        h += '<div class="param-group"><div class="param-group-title">' + cat + '</div>';
        groups[cat].forEach(function(p) { h += paramRow(p); });
        h += '</div>';
      }
      c.innerHTML = h;
    }
    
    function catName(c) { var n = ['General', 'Position', 'Size', 'Movement', 'Color', 'Timing', 'Input', 'Advanced']; return n[c] || 'General'; }
    function typeName(t) { var n = ['slider', 'slider_int', 'toggle', 'color', 'dropdown', 'input_select', 'sprite_select', 'equation_select', 'text', 'button', 'separator', 'label']; return n[t] || 'unknown'; }
    
    function paramRow(p) {
      var t = typeName(p.type);
      var ctrl = '';
      if (t === 'slider') {
        ctrl = '<input type="range" id="param_' + p.id + '" min="' + p.min + '" max="' + p.max + '" step="' + (p.step || 0.01) + '" value="' + p.value + '" oninput="upd(\'' + p.id + '\',this.value,\'slider\')"><span class="value-display" id="val_' + p.id + '">' + Number(p.value).toFixed(2) + '</span>';
      } else if (t === 'slider_int') {
        ctrl = '<input type="range" id="param_' + p.id + '" min="' + p.min + '" max="' + p.max + '" step="1" value="' + p.value + '" oninput="upd(\'' + p.id + '\',this.value,\'int\')"><span class="value-display" id="val_' + p.id + '">' + p.value + '</span>';
      } else if (t === 'toggle') {
        ctrl = '<label class="toggle-switch"><input type="checkbox" id="param_' + p.id + '" ' + (p.value ? 'checked' : '') + ' onchange="upd(\'' + p.id + '\',this.checked?1:0,\'toggle\')"><span class="toggle-slider"></span></label>';
      } else if (t === 'color') {
        ctrl = '<input type="color" id="param_' + p.id + '" value="' + rgb2hex(p.r || 255, p.g || 255, p.b || 255) + '" onchange="upd(\'' + p.id + '\',this.value,\'color\')">';
      } else {
        ctrl = '<span>' + (p.value !== undefined ? p.value : '-') + '</span>';
      }
      return '<div class="param-row"><label>' + esc(p.name) + '</label><div class="param-control">' + ctrl + '</div></div>';
    }
    
    async function upd(id, val, type) {
      var p = currentParams.find(function(x) { return x.id === id; });
      if (p) { p.value = (type === 'slider') ? parseFloat(val) : parseInt(val); }
      var v = document.getElementById('val_' + id);
      if (v) v.textContent = (type === 'slider') ? Number(val).toFixed(2) : val;
      try { await fetch('/api/animation/param', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ set: selectedAnimSetId, param: id, value: parseFloat(val) }) }); } catch(e) {}
    }
    
    async function resetParams() {
      if (!selectedAnimSetId) return;
      try { await fetch('/api/animation/reset', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ set: selectedAnimSetId }) }); await loadParams(selectedAnimSetId); toast('Reset', 'success'); } catch(e) {}
    }
    
    async function togglePreview() {
      if (!selectedAnimSetId) { toast('Select animation', 'error'); return; }
      isPlaying = !isPlaying;
      try {
        var ep = isPlaying ? '/api/animation/activate' : '/api/animation/stop';
        await fetch(ep, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ set: selectedAnimSetId }) });
        updUI();
      } catch(e) {}
    }
    
    async function stopPreview() { isPlaying = false; try { await fetch('/api/animation/stop', { method: 'POST' }); } catch(e) {} updUI(); }
    
    function updUI() {
      var btn = document.getElementById('btnPreview');
      var st = document.getElementById('previewStatus');
      btn.textContent = isPlaying ? 'Pause' : 'Play';
      st.textContent = isPlaying ? 'Playing' : 'Stopped';
      st.className = 'status' + (isPlaying ? ' active' : '');
    }
    
    function showModal(id) { document.getElementById(id).classList.add('show'); }
    function hideModal(id) { document.getElementById(id).classList.remove('show'); }
    function toast(m, t) { var e = document.createElement('div'); e.className = 'toast ' + (t || 'success'); e.textContent = m; document.body.appendChild(e); setTimeout(function() { e.remove(); }, 3000); }
    function esc(s) { var d = document.createElement('div'); d.textContent = s; return d.innerHTML; }
    function rgb2hex(r, g, b) { return '#' + [r, g, b].map(function(x) { return x.toString(16).padStart(2, '0'); }).join(''); }
    document.getElementById('newSceneName').addEventListener('keypress', function(e) { if (e.key === 'Enter') createNewScene(); });
    document.getElementById('renameSceneName').addEventListener('keypress', function(e) { if (e.key === 'Enter') confirmRename(); });
  </script>
</body>
</html>
)rawpage";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
