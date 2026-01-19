/*****************************************************************
 * File:      PageSdCard.hpp
 * Category:  include/SystemAPI/Web/Content
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    SD Card Browser page for the web interface.
 *    Allows viewing files, raw hex data, and image previews.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_SYSTEMAPI_WEB_CONTENT_PAGESDCARD_HPP_
#define ARCOS_INCLUDE_SYSTEMAPI_WEB_CONTENT_PAGESDCARD_HPP_

namespace SystemAPI {
namespace Web {
namespace Content {

// ============================================================
// SD Card Browser Page HTML
// ============================================================

inline const char* getPageSdCard() {
    return R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius - SD Card</title>
  <link rel="stylesheet" href="/style.css">
  <style>
    .browser-layout {
      display: grid;
      grid-template-columns: 320px 1fr;
      gap: 16px;
      margin-top: 16px;
    }
    @media (max-width: 800px) {
      .browser-layout { grid-template-columns: 1fr; }
    }
    .file-list-card {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 16px;
      overflow: hidden;
      max-height: 65vh;
      display: flex;
      flex-direction: column;
    }
    .path-bar {
      background: var(--bg-tertiary);
      padding: 12px 16px;
      border-bottom: 1px solid var(--border);
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 0.85rem;
      color: var(--accent);
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .path-bar button {
      background: var(--bg-secondary);
      border: 1px solid var(--border);
      color: var(--text-primary);
      padding: 6px 12px;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.2s;
    }
    .path-bar button:hover { border-color: var(--accent); color: var(--accent); }
    .file-list-content {
      flex: 1;
      overflow-y: auto;
    }
    .file-item {
      display: flex;
      align-items: center;
      padding: 12px 16px;
      border-bottom: 1px solid var(--border);
      cursor: pointer;
      transition: background 0.2s;
    }
    .file-item:hover { background: var(--accent-subtle); }
    .file-item.selected { background: var(--accent-subtle); border-left: 3px solid var(--accent); }
    .file-icon {
      font-size: 1.2rem;
      margin-right: 12px;
      color: var(--text-muted);
      width: 24px;
      text-align: center;
    }
    .file-item.directory .file-icon { color: var(--warning); }
    .file-name { flex: 1; word-break: break-all; font-size: 0.9rem; }
    .file-size {
      color: var(--text-muted);
      font-size: 0.8rem;
      font-family: 'SF Mono', Monaco, monospace;
      margin-left: 10px;
    }
    .preview-card {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 16px;
      overflow: hidden;
      display: flex;
      flex-direction: column;
    }
    .preview-header {
      background: var(--bg-tertiary);
      padding: 14px 16px;
      border-bottom: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .preview-title { font-weight: 600; color: var(--accent); font-size: 0.95rem; }
    .preview-actions { display: flex; gap: 8px; }
    .preview-actions .btn { padding: 8px 14px; font-size: 0.8rem; flex: none; }
    .preview-content {
      flex: 1;
      padding: 16px;
      overflow: auto;
      min-height: 200px;
      max-height: 40vh;
    }
    .hex-view {
      font-family: 'SF Mono', Monaco, monospace;
      font-size: 11px;
      line-height: 1.6;
      white-space: pre;
      color: var(--text-secondary);
    }
    .no-preview {
      color: var(--text-muted);
      text-align: center;
      padding: 40px 20px;
      font-size: 0.9rem;
    }
    .file-info-panel {
      background: var(--bg-tertiary);
      padding: 14px 16px;
      border-top: 1px solid var(--border);
    }
    .file-info-row {
      display: flex;
      justify-content: space-between;
      font-size: 0.85rem;
      padding: 4px 0;
    }
    .file-info-row span:first-child { color: var(--text-muted); }
    .file-info-row span:last-child { font-family: 'SF Mono', Monaco, monospace; }
    .loading-msg { text-align: center; padding: 24px; color: var(--text-muted); }
    .error-toast {
      background: rgba(255, 51, 51, 0.15);
      border: 1px solid var(--danger);
      padding: 12px 16px;
      border-radius: 10px;
      color: var(--danger);
      margin-bottom: 16px;
      display: none;
      font-size: 0.9rem;
    }
    .error-toast.active { display: block; }
    .stats-row {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
      gap: 12px;
    }
    .stat-box {
      background: var(--bg-tertiary);
      padding: 14px;
      border-radius: 10px;
      text-align: center;
      border-left: 3px solid var(--accent);
    }
    .stat-box .stat-value {
      font-size: 1.2rem;
      font-weight: 600;
      color: var(--accent);
      font-family: 'SF Mono', Monaco, monospace;
    }
    .stat-box .stat-label {
      font-size: 0.7rem;
      color: var(--text-muted);
      text-transform: uppercase;
      margin-top: 4px;
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
      <div class="card">
        <div class="card-header">
          <h2>&#x2261; SD Card Browser</h2>
        </div>
        <div class="card-body">
          <div class="error-toast" id="error-msg"></div>
          
          <!-- Status Row -->
          <div class="stats-row">
            <div class="stat-box">
              <div class="stat-value" id="stat-ready">--</div>
              <div class="stat-label">Status</div>
            </div>
            <div class="stat-box">
              <div class="stat-value" id="stat-total">--</div>
              <div class="stat-label">Total</div>
            </div>
            <div class="stat-box">
              <div class="stat-value" id="stat-free">--</div>
              <div class="stat-label">Free</div>
            </div>
            <div class="stat-box">
              <div class="stat-value" id="stat-used">--</div>
              <div class="stat-label">Used</div>
            </div>
          </div>
          
          <!-- Browser Layout -->
          <div class="browser-layout">
            <!-- File List -->
            <div class="file-list-card">
              <div class="path-bar">
                <button onclick="goUp()" title="Go up">&#x2191;</button>
                <input type="text" id="path-input" class="input" style="flex:1; padding:6px 10px; font-size:0.85rem; font-family:'SF Mono',monospace;" value="/" onkeydown="if(event.key==='Enter')goToPath()">
                <button onclick="goToPath()" title="Go">&#x2192;</button>
                <button onclick="refreshDir()" title="Refresh">&#x21BB;</button>
              </div>
              <div class="file-list-content" id="file-list-content">
                <div class="loading-msg">Loading...</div>
              </div>
            </div>
            
            <!-- Preview Panel -->
            <div class="preview-card">
              <div class="preview-header">
                <span class="preview-title" id="preview-title">Select a file</span>
                <div class="preview-actions">
                  <button class="btn btn-secondary" onclick="downloadFile()" id="btn-download" style="display:none">Download</button>
                  <button class="btn btn-danger" onclick="deleteFile()" id="btn-delete" style="display:none">Delete</button>
                </div>
              </div>
              <div class="preview-content" id="preview-content">
                <div class="no-preview">Select a file to preview</div>
              </div>
              <div class="file-info-panel" id="info-panel" style="display:none">
                <div class="file-info-row">
                  <span>Path</span>
                  <span id="info-path">-</span>
                </div>
                <div class="file-info-row">
                  <span>Size</span>
                  <span id="info-size">-</span>
                </div>
                <div class="file-info-row">
                  <span>Type</span>
                  <span id="info-type">-</span>
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
    
  <script>
    let currentPath = '/';
    let selectedFile = null;
    
    document.addEventListener('DOMContentLoaded', function() {
      loadStatus();
      loadDirectory('/');
      setInterval(loadStatus, 5000);
    });
    
    function loadStatus() {
      fetch('/api/sdcard/status')
        .then(function(r) { return r.json(); })
        .then(function(data) {
          document.getElementById('stat-ready').textContent = data.mounted ? 'Ready' : 'N/A';
          document.getElementById('stat-ready').style.color = data.mounted ? 'var(--success)' : 'var(--danger)';
          var total = data.total_mb || 0;
          var free = data.free_mb || 0;
          var used = total - free;
          document.getElementById('stat-total').textContent = total.toFixed(0) + ' MB';
          document.getElementById('stat-free').textContent = free.toFixed(0) + ' MB';
          document.getElementById('stat-used').textContent = used.toFixed(0) + ' MB';
        })
        .catch(function(e) { console.error('Status error:', e); });
    }
    
    function loadDirectory(path) {
      currentPath = path;
      document.getElementById('path-input').value = path;
      document.getElementById('file-list-content').innerHTML = '<div class="loading-msg">Loading...</div>';
      
      fetch('/api/sdcard/list?path=' + encodeURIComponent(path))
        .then(function(r) { return r.json(); })
        .then(function(data) {
          console.log('API response:', data);
          if (!data.success) {
            showError('Error: ' + (data.error || 'Unknown error'));
            document.getElementById('file-list-content').innerHTML = '<div class="no-preview">Error loading</div>';
            return;
          }
          renderFileList(data.files || []);
        })
        .catch(function(e) { 
          showError('Failed to load: ' + e.message);
          document.getElementById('file-list-content').innerHTML = '<div class="no-preview">Failed to load</div>';
        });
    }
    
    function goToPath() {
      var path = document.getElementById('path-input').value.trim();
      if (!path) path = '/';
      if (path[0] !== '/') path = '/' + path;
      loadDirectory(path);
    }
    
    function renderFileList(entries) {
      var container = document.getElementById('file-list-content');
      
      if (entries.length === 0) {
        container.innerHTML = '<div class="no-preview">Empty directory</div>';
        return;
      }
      
      entries.sort(function(a, b) {
        if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
        return a.name.localeCompare(b.name);
      });
      
      var html = '';
      for (var i = 0; i < entries.length; i++) {
        var entry = entries[i];
        var icon = entry.isDir ? '&#x25A1;' : '&#x25A0;';
        var sizeStr = entry.isDir ? '' : formatSize(entry.size);
        var cls = entry.isDir ? 'file-item directory' : 'file-item';
        
        html += '<div class="' + cls + '" onclick="selectItem(\'' + escapeAttr(entry.path) + '\', ' + entry.isDir + ', ' + entry.size + ')" ondblclick="openItem(\'' + escapeAttr(entry.path) + '\', ' + entry.isDir + ')">';
        html += '<span class="file-icon">' + icon + '</span>';
        html += '<span class="file-name">' + escapeHtml(entry.name) + '</span>';
        html += '<span class="file-size">' + sizeStr + '</span>';
        html += '</div>';
      }
      
      container.innerHTML = html;
    }
    
    function selectItem(path, isDir, size) {
      selectedFile = { path: path, isDir: isDir, size: size };
      
      var items = document.querySelectorAll('.file-item');
      for (var i = 0; i < items.length; i++) items[i].classList.remove('selected');
      if (event && event.currentTarget) event.currentTarget.classList.add('selected');
      
      document.getElementById('preview-title').textContent = path.split('/').pop();
      document.getElementById('info-panel').style.display = 'block';
      document.getElementById('info-path').textContent = path;
      document.getElementById('info-size').textContent = formatSize(size);
      document.getElementById('info-type').textContent = isDir ? 'Directory' : getFileType(path);
      
      document.getElementById('btn-download').style.display = isDir ? 'none' : 'inline-block';
      document.getElementById('btn-delete').style.display = 'inline-block';
      
      if (!isDir) {
        loadPreview(path);
      } else {
        document.getElementById('preview-content').innerHTML = '<div class="no-preview">Double-click to open directory</div>';
      }
    }
    
    function openItem(path, isDir) {
      if (isDir) {
        loadDirectory(path);
      } else {
        loadPreview(path);
      }
    }
    
    function loadPreview(path) {
      var ext = path.split('.').pop().toLowerCase();
      var container = document.getElementById('preview-content');
      
      container.innerHTML = '<div class="loading-msg">Loading preview...</div>';
      
      var isHex = ['bin', 'dat', 'raw'].indexOf(ext) >= 0;
      var isText = ['json', 'txt', 'log', 'cfg', 'ini'].indexOf(ext) >= 0;
      
      var url = isText ? '/api/sdcard/read?path=' : '/api/sdcard/hex?path=';
      url += encodeURIComponent(path);
      
      fetch(url)
        .then(function(r) { return r.text(); })
        .then(function(text) {
          container.innerHTML = '<div class="hex-view">' + escapeHtml(text) + '</div>';
        })
        .catch(function(e) {
          container.innerHTML = '<div class="no-preview">Failed to load: ' + e.message + '</div>';
        });
    }
    
    function goUp() {
      if (currentPath === '/' || currentPath === '') return;
      var parts = currentPath.split('/').filter(function(p) { return p; });
      parts.pop();
      var newPath = '/' + parts.join('/');
      loadDirectory(newPath || '/');
    }
    
    function refreshDir() {
      loadDirectory(currentPath);
      loadStatus();
    }
    
    function deleteFile() {
      if (!selectedFile) return;
      if (!confirm('Delete ' + selectedFile.path + '?')) return;
      
      fetch('/api/sdcard/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path: selectedFile.path })
      })
        .then(function(r) { return r.json(); })
        .then(function(data) {
          if (data.success) {
            refreshDir();
            selectedFile = null;
            document.getElementById('preview-content').innerHTML = '<div class="no-preview">File deleted</div>';
            document.getElementById('info-panel').style.display = 'none';
            document.getElementById('btn-download').style.display = 'none';
            document.getElementById('btn-delete').style.display = 'none';
          } else {
            showError('Delete failed: ' + (data.error || 'Unknown'));
          }
        })
        .catch(function(e) { showError('Delete failed: ' + e.message); });
    }
    
    function downloadFile() {
      if (!selectedFile || selectedFile.isDir) return;
      window.location.href = '/api/sdcard/download?path=' + encodeURIComponent(selectedFile.path);
    }
    
    function showError(msg) {
      var el = document.getElementById('error-msg');
      el.textContent = msg;
      el.classList.add('active');
      setTimeout(function() { el.classList.remove('active'); }, 5000);
    }
    
    function formatSize(bytes) {
      if (bytes === 0) return '0 B';
      if (bytes < 1024) return bytes + ' B';
      if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
      return (bytes / 1048576).toFixed(1) + ' MB';
    }
    
    function getFileType(path) {
      var ext = path.split('.').pop().toLowerCase();
      var types = {
        'bin': 'Binary', 'dat': 'Data', 'json': 'JSON',
        'txt': 'Text', 'log': 'Log', 'png': 'Image', 'jpg': 'Image'
      };
      return types[ext] || 'File';
    }
    
    function escapeHtml(text) {
      var div = document.createElement('div');
      div.textContent = text;
      return div.innerHTML;
    }
    
    function escapeAttr(str) {
      return str.replace(/'/g, "\\'").replace(/"/g, '\\"');
    }
  </script>
</body>
</html>
)rawhtml";
}

}  // namespace Content
}  // namespace Web
}  // namespace SystemAPI

#endif  // ARCOS_INCLUDE_SYSTEMAPI_WEB_CONTENT_PAGESDCARD_HPP_
