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
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SD Card Browser - ARCOS</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #e8e8e8;
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 {
            color: #00ff88;
            margin-bottom: 20px;
            display: flex;
            align-items: center;
            gap: 15px;
        }
        h1 svg { width: 40px; height: 40px; fill: #00ff88; }
        
        .status-card {
            background: rgba(0,0,0,0.3);
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
            border: 1px solid #333;
        }
        .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
        }
        .stat-item {
            text-align: center;
            padding: 10px;
            background: rgba(0,255,136,0.1);
            border-radius: 8px;
        }
        .stat-value {
            font-size: 1.5em;
            color: #00ff88;
            font-weight: bold;
        }
        .stat-label { color: #aaa; font-size: 0.9em; margin-top: 5px; }
        
        .browser-layout {
            display: grid;
            grid-template-columns: 300px 1fr;
            gap: 20px;
        }
        @media (max-width: 800px) {
            .browser-layout { grid-template-columns: 1fr; }
        }
        
        .file-list {
            background: rgba(0,0,0,0.3);
            border-radius: 10px;
            border: 1px solid #333;
            max-height: 70vh;
            overflow-y: auto;
        }
        .path-bar {
            background: #1a1a2e;
            padding: 10px 15px;
            border-bottom: 1px solid #333;
            font-family: monospace;
            color: #00ff88;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .path-bar button {
            background: #333;
            border: none;
            color: #fff;
            padding: 5px 10px;
            border-radius: 5px;
            cursor: pointer;
        }
        .path-bar button:hover { background: #444; }
        
        .file-item {
            display: flex;
            align-items: center;
            padding: 12px 15px;
            border-bottom: 1px solid #222;
            cursor: pointer;
            transition: background 0.2s;
        }
        .file-item:hover { background: rgba(0,255,136,0.1); }
        .file-item.selected { background: rgba(0,255,136,0.2); }
        .file-icon {
            width: 24px;
            height: 24px;
            margin-right: 12px;
            fill: #888;
        }
        .file-item.directory .file-icon { fill: #ffcc00; }
        .file-name { flex: 1; word-break: break-all; }
        .file-size {
            color: #888;
            font-size: 0.9em;
            margin-left: 10px;
        }
        
        .preview-panel {
            background: rgba(0,0,0,0.3);
            border-radius: 10px;
            border: 1px solid #333;
            display: flex;
            flex-direction: column;
        }
        .preview-header {
            background: #1a1a2e;
            padding: 15px;
            border-bottom: 1px solid #333;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .preview-title { font-weight: bold; color: #00ff88; }
        .preview-actions { display: flex; gap: 10px; }
        .preview-actions button {
            background: #333;
            border: none;
            color: #fff;
            padding: 8px 15px;
            border-radius: 5px;
            cursor: pointer;
        }
        .preview-actions button:hover { background: #444; }
        .preview-actions button.danger { background: #ff4444; }
        .preview-actions button.danger:hover { background: #ff6666; }
        
        .preview-content {
            flex: 1;
            padding: 15px;
            overflow: auto;
            min-height: 300px;
        }
        .hex-view {
            font-family: 'Courier New', monospace;
            font-size: 12px;
            line-height: 1.5;
            white-space: pre;
            color: #aaa;
        }
        .image-preview {
            max-width: 100%;
            image-rendering: pixelated;
            border: 1px solid #333;
        }
        .no-preview {
            color: #666;
            text-align: center;
            padding: 50px;
        }
        
        .info-panel {
            background: rgba(0,0,0,0.2);
            padding: 15px;
            border-top: 1px solid #333;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            margin-bottom: 5px;
        }
        .info-label { color: #888; }
        .info-value { color: #fff; }
        
        .loading {
            display: none;
            text-align: center;
            padding: 20px;
            color: #00ff88;
        }
        .loading.active { display: block; }
        
        .back-link {
            display: inline-block;
            color: #00ff88;
            text-decoration: none;
            margin-bottom: 20px;
        }
        .back-link:hover { text-decoration: underline; }
        
        .refresh-btn {
            background: #00ff88;
            color: #000;
            border: none;
            padding: 10px 20px;
            border-radius: 5px;
            cursor: pointer;
            font-weight: bold;
        }
        .refresh-btn:hover { background: #00cc6a; }
        
        .error-msg {
            background: rgba(255,0,0,0.2);
            border: 1px solid #ff4444;
            padding: 15px;
            border-radius: 8px;
            color: #ff8888;
            margin-bottom: 20px;
            display: none;
        }
        .error-msg.active { display: block; }
    </style>
</head>
<body>
    <div class="container">
        <a href="/advanced" class="back-link">&larr; Back to Advanced Menu</a>
        
        <h1>
            <svg viewBox="0 0 24 24"><path d="M20 6H12L10 4H4C2.9 4 2 4.9 2 6V18C2 19.1 2.9 20 4 20H20C21.1 20 22 19.1 22 18V8C22 6.9 21.1 6 20 6M20 18H4V8H20V18Z"/></svg>
            SD Card Browser
        </h1>
        
        <div class="error-msg" id="error-msg"></div>
        
        <!-- Status Card -->
        <div class="status-card">
            <div class="status-grid">
                <div class="stat-item">
                    <div class="stat-value" id="stat-ready">--</div>
                    <div class="stat-label">Status</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value" id="stat-total">--</div>
                    <div class="stat-label">Total</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value" id="stat-free">--</div>
                    <div class="stat-label">Free</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value" id="stat-reads">--</div>
                    <div class="stat-label">Reads</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value" id="stat-writes">--</div>
                    <div class="stat-label">Writes</div>
                </div>
                <div class="stat-item">
                    <div class="stat-value" id="stat-errors">--</div>
                    <div class="stat-label">Errors</div>
                </div>
            </div>
        </div>
        
        <!-- Browser Layout -->
        <div class="browser-layout">
            <!-- File List -->
            <div class="file-list">
                <div class="path-bar">
                    <button onclick="goUp()" title="Go up">&uarr;</button>
                    <span id="current-path">/</span>
                    <button onclick="refreshDir()" title="Refresh">&#8635;</button>
                </div>
                <div id="file-list-content">
                    <div class="loading active">Loading...</div>
                </div>
            </div>
            
            <!-- Preview Panel -->
            <div class="preview-panel">
                <div class="preview-header">
                    <span class="preview-title" id="preview-title">Select a file</span>
                    <div class="preview-actions">
                        <button onclick="downloadFile()" id="btn-download" style="display:none">Download</button>
                        <button onclick="deleteFile()" id="btn-delete" class="danger" style="display:none">Delete</button>
                    </div>
                </div>
                <div class="preview-content" id="preview-content">
                    <div class="no-preview">Select a file to preview</div>
                </div>
                <div class="info-panel" id="info-panel" style="display:none">
                    <div class="info-row">
                        <span class="info-label">Path:</span>
                        <span class="info-value" id="info-path">-</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Size:</span>
                        <span class="info-value" id="info-size">-</span>
                    </div>
                    <div class="info-row">
                        <span class="info-label">Type:</span>
                        <span class="info-value" id="info-type">-</span>
                    </div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        let currentPath = '/';
        let selectedFile = null;
        
        // Initialize
        document.addEventListener('DOMContentLoaded', () => {
            loadStatus();
            loadDirectory('/');
            setInterval(loadStatus, 5000);
        });
        
        async function loadStatus() {
            try {
                const resp = await fetch('/api/sdcard/status');
                const data = await resp.json();
                
                document.getElementById('stat-ready').textContent = data.mounted ? 'Ready' : 'Not Ready';
                document.getElementById('stat-ready').style.color = data.mounted ? '#00ff88' : '#ff4444';
                document.getElementById('stat-total').textContent = (data.total_mb || 0).toFixed(1) + ' MB';
                document.getElementById('stat-free').textContent = (data.free_mb || 0).toFixed(1) + ' MB';
                document.getElementById('stat-reads').textContent = '--';
                document.getElementById('stat-writes').textContent = '--';
                document.getElementById('stat-errors').textContent = '--';
            } catch (e) {
                console.error('Failed to load status:', e);
            }
        }
        
        async function loadDirectory(path) {
            currentPath = path;
            document.getElementById('current-path').textContent = path;
            document.getElementById('file-list-content').innerHTML = '<div class="loading active">Loading...</div>';
            
            try {
                const resp = await fetch('/api/sdcard/list?path=' + encodeURIComponent(path));
                const data = await resp.json();
                
                if (!data.success) {
                    showError('Error: ' + (data.error || 'Unknown error'));
                    return;
                }
                
                renderFileList(data.files || []);
            } catch (e) {
                showError('Failed to load directory: ' + e.message);
            }
        }
        
        function renderFileList(entries) {
            const container = document.getElementById('file-list-content');
            
            if (entries.length === 0) {
                container.innerHTML = '<div class="no-preview">Empty directory</div>';
                return;
            }
            
            // Sort: directories first, then files
            entries.sort((a, b) => {
                if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
                return a.name.localeCompare(b.name);
            });
            
            let html = '';
            for (const entry of entries) {
                const icon = entry.isDir ? getFolderIcon() : getFileIcon(entry.name);
                const sizeStr = entry.isDir ? '' : formatSize(entry.size);
                const cls = entry.isDir ? 'file-item directory' : 'file-item';
                
                html += `<div class="${cls}" onclick="selectItem('${entry.path}', ${entry.isDir}, ${entry.size})" ondblclick="openItem('${entry.path}', ${entry.isDir})">
                    ${icon}
                    <span class="file-name">${escapeHtml(entry.name)}</span>
                    <span class="file-size">${sizeStr}</span>
                </div>`;
            }
            
            container.innerHTML = html;
        }
        
        function selectItem(path, isDir, size) {
            selectedFile = { path, isDir, size };
            
            // Update selection UI
            document.querySelectorAll('.file-item').forEach(el => el.classList.remove('selected'));
            event.currentTarget.classList.add('selected');
            
            // Update preview
            document.getElementById('preview-title').textContent = path.split('/').pop();
            document.getElementById('info-panel').style.display = 'block';
            document.getElementById('info-path').textContent = path;
            document.getElementById('info-size').textContent = formatSize(size);
            document.getElementById('info-type').textContent = isDir ? 'Directory' : getFileType(path);
            
            // Show/hide buttons
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
        
        async function loadPreview(path) {
            const ext = path.split('.').pop().toLowerCase();
            const container = document.getElementById('preview-content');
            
            container.innerHTML = '<div class="loading active">Loading preview...</div>';
            
            try {
                if (['bin', 'dat', 'raw'].includes(ext)) {
                    // Hex preview
                    const resp = await fetch('/api/sdcard/hex?path=' + encodeURIComponent(path));
                    const text = await resp.text();
                    container.innerHTML = '<div class="hex-view">' + escapeHtml(text) + '</div>';
                } else if (['json', 'txt', 'log', 'cfg', 'ini'].includes(ext)) {
                    // Text preview
                    const resp = await fetch('/api/sdcard/read?path=' + encodeURIComponent(path));
                    const text = await resp.text();
                    container.innerHTML = '<div class="hex-view">' + escapeHtml(text) + '</div>';
                } else if (['png', 'jpg', 'jpeg', 'gif', 'bmp'].includes(ext)) {
                    // Image preview (if supported)
                    container.innerHTML = '<div class="no-preview">Image preview not available for raw files</div>';
                } else {
                    // Default hex preview
                    const resp = await fetch('/api/sdcard/hex?path=' + encodeURIComponent(path));
                    const text = await resp.text();
                    container.innerHTML = '<div class="hex-view">' + escapeHtml(text) + '</div>';
                }
            } catch (e) {
                container.innerHTML = '<div class="no-preview">Failed to load preview: ' + e.message + '</div>';
            }
        }
        
        function goUp() {
            if (currentPath === '/' || currentPath === '') return;
            const parts = currentPath.split('/').filter(p => p);
            parts.pop();
            const newPath = '/' + parts.join('/');
            loadDirectory(newPath || '/');
        }
        
        function refreshDir() {
            loadDirectory(currentPath);
            loadStatus();
        }
        
        async function deleteFile() {
            if (!selectedFile) return;
            if (!confirm('Delete ' + selectedFile.path + '?')) return;
            
            try {
                const resp = await fetch('/api/sdcard/delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ path: selectedFile.path })
                });
                const data = await resp.json();
                
                if (data.success) {
                    refreshDir();
                    selectedFile = null;
                    document.getElementById('preview-content').innerHTML = '<div class="no-preview">File deleted</div>';
                } else {
                    showError('Delete failed: ' + (data.error || 'Unknown error'));
                }
            } catch (e) {
                showError('Delete failed: ' + e.message);
            }
        }
        
        function downloadFile() {
            if (!selectedFile || selectedFile.isDir) return;
            window.location.href = '/api/sdcard/download?path=' + encodeURIComponent(selectedFile.path);
        }
        
        function showError(msg) {
            const el = document.getElementById('error-msg');
            el.textContent = msg;
            el.classList.add('active');
            setTimeout(() => el.classList.remove('active'), 5000);
        }
        
        function formatSize(bytes) {
            if (bytes === 0) return '0 B';
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / 1048576).toFixed(1) + ' MB';
        }
        
        function getFileType(path) {
            const ext = path.split('.').pop().toLowerCase();
            const types = {
                'bin': 'Binary Data',
                'dat': 'Data File',
                'json': 'JSON Document',
                'txt': 'Text File',
                'log': 'Log File',
                'png': 'PNG Image',
                'jpg': 'JPEG Image',
                'jpeg': 'JPEG Image',
                'bmp': 'Bitmap Image'
            };
            return types[ext] || 'Unknown';
        }
        
        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }
        
        function getFolderIcon() {
            return '<svg class="file-icon" viewBox="0 0 24 24"><path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/></svg>';
        }
        
        function getFileIcon(name) {
            const ext = name.split('.').pop().toLowerCase();
            if (['bin', 'dat', 'raw'].includes(ext)) {
                return '<svg class="file-icon" viewBox="0 0 24 24"><path d="M14 2H6c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6zm-1 9h-2v2H9v-2H7v-2h2V7h2v2h2v2zm3 4H8v-2h8v2z"/></svg>';
            }
            if (['json', 'txt', 'log'].includes(ext)) {
                return '<svg class="file-icon" viewBox="0 0 24 24"><path d="M14 2H6c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/></svg>';
            }
            return '<svg class="file-icon" viewBox="0 0 24 24"><path d="M14 2H6c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6zm4 18H6V4h7v5h5v11z"/></svg>';
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
