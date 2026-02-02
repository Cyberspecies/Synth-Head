/*****************************************************************
 * @file WebContent.hpp
 * @brief Web Content Router - includes all page content
 * 
 * This file serves as a router, including all separate page files.
 * Each tab has its own subpage at a unique URL.
 * 
 * Routes:
 *   /                  -> Basic page (PageBasic.hpp)
 *   /system            -> System page (PageSystem.hpp)
 *   /advanced          -> Advanced menu (PageAdvancedMenu.hpp)
 *   /advanced/sprites  -> Sprite manager (PageSprite.hpp)
 *   /advanced/scenes   -> Scene manager (PageScenes.hpp)
 *   /advanced/scenes/edit -> Scene editor (PageSceneEdit.hpp)
 *   /advanced/ledpresets -> LED preset manager (PageLedPresetList.hpp)
 *   /advanced/ledpresets/edit -> LED preset editor (PageLedPresetEdit.hpp)
 *   /advanced/equations -> Equation editor (PageEquations.hpp)
 *   /settings          -> Settings page (PageSettings.hpp)
 *   /style.css         -> Shared stylesheet
 * 
 * @author ARCOS
 * @version 2.4
 *****************************************************************/

#pragma once

// Include all page content
#include "PageBasic.hpp"
#include "PageSystem.hpp"
#include "PageAdvancedMenu.hpp"
#include "PageSprite.hpp"
#include "PageEquations.hpp"
#include "PageSceneList.hpp"
#include "PageSceneEdit.hpp"
#include "PageLedPresetList.hpp"
#include "PageLedPresetEdit.hpp"
#include "PageSettings.hpp"

namespace SystemAPI {
namespace Web {
namespace Content {

// ============================================================
// Shared CSS Content (used by all pages)
// ============================================================

inline const char STYLE_CSS[] = R"rawliteral(
:root {
  --bg-primary: #0a0a0a;
  --bg-secondary: #111111;
  --bg-tertiary: #1a1a1a;
  --bg-card: #141414;
  --text-primary: #ffffff;
  --text-secondary: #888888;
  --text-muted: #555555;
  --accent: #ff6b00;
  --accent-hover: #ff8533;
  --accent-glow: rgba(255, 107, 0, 0.3);
  --accent-subtle: rgba(255, 107, 0, 0.1);
  --success: #00cc66;
  --warning: #ffaa00;
  --danger: #ff3333;
  --border: #2a2a2a;
  --border-accent: #ff6b00;
}

* { margin: 0; padding: 0; box-sizing: border-box; }

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: var(--bg-primary);
  color: var(--text-primary);
  min-height: 100vh;
  line-height: 1.6;
}

.container { max-width: 1200px; margin: 0 auto; padding: 16px; }

.tab-content { display: block; animation: fadeIn 0.3s ease; }
.card-grid { display: grid; grid-template-columns: 1fr; gap: 16px; }
@media (min-width: 600px) { .card-grid { grid-template-columns: repeat(2, 1fr); } }
@media (min-width: 900px) { .card-grid { grid-template-columns: repeat(3, 1fr); } }

header { padding: 20px 0; margin-bottom: 20px; border-bottom: 1px solid var(--border); }
.header-content { display: flex; justify-content: space-between; align-items: center; }
.logo-section { display: flex; align-items: center; gap: 12px; }
.logo-icon { font-size: 2rem; color: var(--accent); text-shadow: 0 0 20px var(--accent-glow); }
.logo-text h1 { font-size: 1.5rem; font-weight: 700; margin: 0; line-height: 1.2; }
.model-tag { font-size: 0.7rem; color: var(--accent); background: var(--accent-subtle); padding: 2px 8px; border-radius: 4px; font-weight: 600; }

.tabs { display: flex; gap: 8px; margin-bottom: 20px; background: var(--bg-secondary); padding: 4px; border-radius: 12px; border: 1px solid var(--border); text-decoration: none; }
.tab { flex: 1; background: transparent; border: none; color: var(--text-secondary); padding: 12px 16px; border-radius: 8px; cursor: pointer; font-size: 0.9rem; font-weight: 500; transition: all 0.2s; text-decoration: none; text-align: center; }
.tab:hover { color: var(--text-primary); background: var(--bg-tertiary); }
.tab.active { background: var(--accent); color: var(--bg-primary); box-shadow: 0 0 15px var(--accent-glow); }

.tab-content { display: block; animation: fadeIn 0.3s ease; }
@keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }

.card { background: var(--bg-card); border: 1px solid var(--border); border-radius: 16px; overflow: hidden; display: flex; flex-direction: column; }
.card-header { padding: 16px 20px; border-bottom: 1px solid var(--border); }
.card-header h2 { font-size: 1rem; font-weight: 600; margin: 0; }
.card-body { padding: 20px; }
.card-body.center { text-align: center; padding: 40px 20px; }
.card-body.compact { padding: 12px 16px; }

.danger-card { border-color: rgba(255, 51, 51, 0.3); }
.danger-card .card-header { border-color: rgba(255, 51, 51, 0.3); }
.danger-card .card-header h2 { color: var(--danger); }

.placeholder-card { border-style: dashed; border-color: var(--border); }
.placeholder-icon { font-size: 3rem; margin-bottom: 12px; opacity: 0.5; }
.placeholder-text { color: var(--text-muted); font-size: 0.9rem; }

.welcome-text { color: var(--text-secondary); margin-bottom: 20px; }
.welcome-text strong { color: var(--accent); }

.info-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 16px; }
.info-item { background: var(--bg-tertiary); padding: 16px; border-radius: 12px; border-left: 3px solid var(--accent); }
.info-label { display: block; font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 4px; }
.info-value { font-size: 1.1rem; font-weight: 600; font-family: 'SF Mono', Monaco, monospace; }

.current-wifi { display: flex; align-items: center; gap: 12px; padding: 16px; background: var(--bg-tertiary); border-radius: 12px; margin-bottom: 24px; border-left: 3px solid var(--accent); }
.wifi-label { color: var(--text-secondary); font-size: 0.85rem; }
.wifi-value { flex: 1; font-weight: 500; font-family: 'SF Mono', Monaco, monospace; }
.wifi-badge { font-size: 0.7rem; padding: 4px 10px; border-radius: 12px; font-weight: 600; text-transform: uppercase; background: var(--accent-subtle); color: var(--accent); }

.form-group { margin-bottom: 20px; }
.form-group label { display: block; font-size: 0.85rem; color: var(--text-secondary); margin-bottom: 8px; font-weight: 500; }
.input { width: 100%; padding: 14px 16px; background: var(--bg-tertiary); border: 1px solid var(--border); border-radius: 10px; color: var(--text-primary); font-size: 1rem; transition: all 0.2s; }
.input:focus { outline: none; border-color: var(--accent); box-shadow: 0 0 0 3px var(--accent-glow); }
.input::placeholder { color: var(--text-muted); }

.password-input-wrapper { position: relative; }
.password-input-wrapper .input { padding-right: 50px; }
.password-toggle { position: absolute; right: 12px; top: 50%; transform: translateY(-50%); background: none; border: none; color: var(--text-muted); cursor: pointer; font-size: 1.1rem; }
.input-hint { display: block; font-size: 0.75rem; color: var(--text-muted); margin-top: 6px; }

.button-group { display: flex; gap: 12px; margin-top: 24px; }
.btn { flex: 1; padding: 14px 20px; border: none; border-radius: 10px; font-size: 0.95rem; font-weight: 600; cursor: pointer; transition: all 0.2s; }
.btn-primary { background: var(--accent); color: var(--bg-primary); }
.btn-primary:hover { background: var(--accent-hover); box-shadow: 0 0 20px var(--accent-glow); }
.btn-secondary { background: var(--bg-tertiary); color: var(--text-primary); border: 1px solid var(--border); }
.btn-secondary:hover { background: var(--bg-secondary); border-color: var(--accent); }
.btn-danger { background: var(--danger); color: white; width: 100%; }
.btn-danger:hover { background: #ff4d4d; box-shadow: 0 0 20px rgba(255, 51, 51, 0.3); }
.btn-warning { background: #ff9800; color: white; width: 100%; margin-bottom: 10px; }

.warning-box { display: flex; align-items: center; gap: 12px; padding: 14px 16px; background: rgba(255, 170, 0, 0.1); border: 1px solid rgba(255, 170, 0, 0.3); border-radius: 10px; margin-top: 20px; }
.warning-icon { font-size: 1.2rem; }
.warning-text { color: var(--warning); font-size: 0.85rem; font-weight: 500; }

.info-list { display: flex; flex-direction: column; gap: 12px; }
.info-row { display: flex; justify-content: space-between; align-items: center; padding: 12px 16px; background: var(--bg-tertiary); border-radius: 10px; }
.info-row .info-label { margin: 0; font-size: 0.85rem; }
.info-row .info-value { font-size: 0.95rem; }

.sys-row { display: flex; justify-content: space-between; align-items: center; padding: 6px 0; border-bottom: 1px solid var(--border); font-size: 0.85rem; }
.sys-row:last-child { border-bottom: none; }
.sys-row > span:first-child { color: var(--text-secondary); }
.sys-row > span:last-child { font-family: 'SF Mono', Monaco, monospace; }

.status-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--text-muted); margin: 0 8px; }
.status-dot.connected { background: var(--success); box-shadow: 0 0 6px var(--success); }
.status-dot.error { background: var(--danger); }

.imu-row { display: flex; justify-content: space-between; align-items: center; padding: 6px 0; border-bottom: 1px solid var(--border); font-size: 0.85rem; gap: 8px; }
.imu-row:last-child { border-bottom: none; }
.imu-lbl { color: var(--text-secondary); min-width: 40px; }
.imu-row span { font-family: 'SF Mono', Monaco, monospace; }

.toast { position: fixed; bottom: 24px; left: 50%; transform: translateX(-50%) translateY(100px); background: var(--bg-tertiary); padding: 14px 24px; border-radius: 12px; box-shadow: 0 4px 24px rgba(0,0,0,0.4); border: 1px solid var(--border); opacity: 0; transition: all 0.3s; z-index: 1000; }
.toast.show { transform: translateX(-50%) translateY(0); opacity: 1; }
.toast.success { border-color: var(--success); border-left: 4px solid var(--success); }
.toast.warning { border-color: var(--warning); border-left: 4px solid var(--warning); }
.toast.error { border-color: var(--danger); border-left: 4px solid var(--danger); }

footer { text-align: center; padding: 24px 0; color: var(--text-muted); font-size: 0.8rem; }

@media (max-width: 400px) {
  .container { padding: 12px; }
  .header-content { flex-direction: column; gap: 12px; text-align: center; }
  .button-group { flex-direction: column; }
  .info-grid { grid-template-columns: 1fr; }
  .tabs { flex-wrap: wrap; }
  .tab { flex: 1 1 45%; min-width: 80px; }
}

/* External WiFi & Authentication Styles */
.warning-card { border-color: rgba(255, 170, 0, 0.3); }
.warning-card .card-header { border-color: rgba(255, 170, 0, 0.3); }
.warning-card .card-header h2 { color: var(--warning); }

.auth-card { border-color: rgba(100, 150, 255, 0.3); }
.auth-card .card-header { border-color: rgba(100, 150, 255, 0.3); }
.auth-card .card-header h2 { color: #6496ff; }

.warning-box.critical { background: rgba(255, 51, 51, 0.1); border-color: rgba(255, 51, 51, 0.4); flex-direction: column; align-items: flex-start; gap: 8px; }
.warning-box.critical .warning-icon { color: var(--danger); font-size: 1.5rem; }
.warning-box.critical .warning-content { }
.warning-box.critical .warning-content strong { color: var(--danger); display: block; margin-bottom: 4px; }
.warning-box.critical .warning-content p { color: var(--text-secondary); font-size: 0.85rem; margin: 0; }

.info-box { display: flex; align-items: flex-start; gap: 12px; padding: 14px 16px; background: rgba(100, 150, 255, 0.1); border: 1px solid rgba(100, 150, 255, 0.3); border-radius: 10px; margin-bottom: 20px; }
.info-box .info-icon { font-size: 1.2rem; }
.info-box p { color: var(--text-secondary); font-size: 0.85rem; margin: 0; }

.toggle-section { margin: 20px 0; }
.toggle-row { display: flex; justify-content: space-between; align-items: center; padding: 16px; background: var(--bg-tertiary); border-radius: 12px; margin-bottom: 12px; }
.toggle-info { flex: 1; margin-right: 16px; }
.toggle-label { display: block; font-weight: 500; margin-bottom: 4px; }
.toggle-hint { display: block; font-size: 0.75rem; color: var(--text-muted); }

.toggle-switch { position: relative; width: 52px; height: 28px; flex-shrink: 0; }
.toggle-switch input { opacity: 0; width: 0; height: 0; }
.toggle-slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: var(--bg-secondary); border: 1px solid var(--border); border-radius: 28px; transition: 0.3s; }
.toggle-slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px; background: var(--text-muted); border-radius: 50%; transition: 0.3s; }
.toggle-switch input:checked + .toggle-slider { background: var(--accent); border-color: var(--accent); }
.toggle-switch input:checked + .toggle-slider:before { transform: translateX(24px); background: white; }
.toggle-switch input:disabled + .toggle-slider { opacity: 0.5; cursor: not-allowed; }

.ext-wifi-status { background: var(--bg-tertiary); border-radius: 10px; padding: 12px 16px; margin: 16px 0; border-left: 3px solid var(--accent); }
.status-row { display: flex; justify-content: space-between; padding: 4px 0; font-size: 0.85rem; }
.status-label { color: var(--text-secondary); }
.status-value { font-family: 'SF Mono', Monaco, monospace; }
.status-value.connected { color: var(--success); }
.status-value.disconnected { color: var(--text-muted); }

.form-divider { height: 1px; background: var(--border); margin: 24px 0; }
.subsection-title { font-size: 0.9rem; font-weight: 600; margin-bottom: 16px; color: var(--text-secondary); }

.network-tabs { display: flex; gap: 8px; margin-bottom: 16px; }
.net-tab { flex: 1; background: var(--bg-tertiary); border: 1px solid var(--border); color: var(--text-secondary); padding: 10px; border-radius: 8px; cursor: pointer; font-size: 0.85rem; transition: all 0.2s; }
.net-tab:hover { border-color: var(--accent); color: var(--text-primary); }
.net-tab.active { background: var(--accent-subtle); border-color: var(--accent); color: var(--accent); }

.network-content { min-height: 120px; }
.scan-controls { margin-bottom: 12px; }
.network-list { background: var(--bg-tertiary); border-radius: 10px; padding: 8px; max-height: 200px; overflow-y: auto; }
.network-empty { color: var(--text-muted); text-align: center; padding: 24px; font-size: 0.85rem; }
.network-item { display: flex; justify-content: space-between; align-items: center; padding: 12px; background: var(--bg-secondary); border-radius: 8px; margin-bottom: 8px; cursor: pointer; transition: all 0.2s; border: 1px solid transparent; }
.network-item:last-child { margin-bottom: 0; }
.network-item:hover { border-color: var(--accent); }
.network-item.selected { border-color: var(--accent); background: var(--accent-subtle); }
.network-info { display: flex; flex-direction: column; }
.network-ssid { font-weight: 500; font-size: 0.9rem; }
.network-security { font-size: 0.7rem; color: var(--text-muted); }
.network-signal { display: flex; align-items: center; gap: 4px; color: var(--text-secondary); font-size: 0.8rem; }
.signal-bars { display: flex; gap: 2px; align-items: flex-end; }
.signal-bar { width: 4px; background: var(--text-muted); border-radius: 1px; }
.signal-bar.active { background: var(--success); }
.signal-bar:nth-child(1) { height: 6px; }
.signal-bar:nth-child(2) { height: 10px; }
.signal-bar:nth-child(3) { height: 14px; }
.signal-bar:nth-child(4) { height: 18px; }

.scanning { text-align: center; padding: 24px; }
.scan-spinner { display: inline-block; width: 24px; height: 24px; border: 3px solid var(--border); border-top-color: var(--accent); border-radius: 50%; animation: spin 1s linear infinite; margin-bottom: 8px; }
@keyframes spin { to { transform: rotate(360deg); } }

/* Wizard Steps */
.setup-wizard { margin-top: 16px; }
.wizard-step { background: var(--bg-tertiary); border-radius: 12px; margin-bottom: 12px; overflow: hidden; border: 1px solid var(--border); }
.step-header { display: flex; align-items: center; padding: 14px 16px; cursor: pointer; transition: background 0.2s; }
.step-header:hover { background: var(--bg-secondary); }
.step-number { width: 28px; height: 28px; background: var(--accent); color: var(--bg-primary); border-radius: 50%; display: flex; align-items: center; justify-content: center; font-weight: 700; font-size: 0.85rem; margin-right: 12px; flex-shrink: 0; }
.step-title { flex: 1; font-weight: 500; }
.step-status { font-size: 1rem; color: var(--text-muted); }
.step-status.done { color: var(--success); }
.step-status.locked { color: var(--text-muted); font-size: 0.9rem; }
.step-content { padding: 0 16px 16px 16px; border-top: 1px solid var(--border); }
.step-desc { color: var(--text-secondary); font-size: 0.85rem; margin: 12px 0 16px 0; }
.btn-sm { padding: 10px 16px; font-size: 0.85rem; }
.master-toggle { background: var(--bg-tertiary); margin-top: 16px; border-radius: 12px; }
.connection-summary { background: var(--bg-secondary); border-radius: 8px; padding: 12px; }
.summary-row { display: flex; justify-content: space-between; font-size: 0.85rem; padding: 4px 0; }
.summary-row span:first-child { color: var(--text-secondary); }
.summary-row span:last-child { font-family: 'SF Mono', Monaco, monospace; }
)rawliteral";

} // namespace Content
} // namespace Web
} // namespace SystemAPI
