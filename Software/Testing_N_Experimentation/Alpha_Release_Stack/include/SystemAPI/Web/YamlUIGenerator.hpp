/*****************************************************************
 * @file YamlUIGenerator.hpp
 * @brief Auto-generates Web UI from YAML Schema Definitions
 * 
 * Parses YAML files with embedded _ui metadata and generates
 * HTML/JavaScript forms automatically based on field types.
 * 
 * Supports:
 * - dropdown  : Select from predefined options
 * - slider    : Numeric slider (float/int)
 * - toggle    : Boolean on/off switch  
 * - color     : RGB color picker
 * - text      : Text input field
 * - number    : Numeric input (no slider)
 * - readonly  : Display only, not editable
 * - file      : File/sprite selector
 * - group     : Contains nested fields (collapsible card)
 * - list      : Dynamic list of items
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <functional>

namespace SystemAPI {
namespace Web {

// ============================================================
// UI Field Types
// ============================================================

enum class YamlUIType : uint8_t {
    UNKNOWN = 0,
    GROUP,          // Container for nested fields
    LIST,           // Dynamic array of items
    TEXT,           // Text input
    NUMBER,         // Numeric input
    SLIDER,         // Slider input
    TOGGLE,         // Boolean toggle
    DROPDOWN,       // Select dropdown
    COLOR,          // Color picker
    FILE,           // File selector
    READONLY        // Display only
};

// ============================================================
// Dropdown Option
// ============================================================

struct YamlOption {
    std::string label;
    std::string value;
    std::string desc;
    
    YamlOption() = default;
    YamlOption(const char* l, const char* v, const char* d = "") 
        : label(l), value(v), desc(d) {}
};

// ============================================================
// Color Value
// ============================================================

struct YamlColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    
    std::string toHex() const {
        char buf[8];
        snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
        return buf;
    }
};

// ============================================================
// UI Field Definition
// ============================================================

struct YamlUIField {
    // Identification
    std::string key;            // YAML key
    std::string path;           // Full path (e.g., "Display.position.x")
    std::string label;          // Display label
    std::string description;    // Tooltip/help text
    std::string icon;           // Icon name (for groups)
    std::string category;       // Grouping category
    
    // Type info
    YamlUIType type = YamlUIType::UNKNOWN;
    std::string fileType;       // For FILE type: "sprite", "bin", etc.
    bool multiline = false;     // For TEXT type
    
    // Constraints
    float minValue = 0.0f;
    float maxValue = 100.0f;
    float step = 1.0f;
    int maxLength = 256;
    std::string unit;
    
    // Options (for DROPDOWN)
    std::vector<YamlOption> options;
    
    // Current value (polymorphic storage)
    std::string stringValue;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
    YamlColor colorValue;
    
    // Display options
    bool visible = true;
    bool readonly = false;
    bool collapsed = false;     // For groups
    bool optional = false;
    bool allowNone = false;     // For file selectors
    std::string visibleIf;      // Conditional visibility
    
    // Children (for GROUP and LIST types)
    std::vector<YamlUIField> children;
    
    // List item template (for LIST type)
    YamlUIField* itemTemplate = nullptr;
};

// ============================================================
// YAML UI Generator Class
// ============================================================

class YamlUIGenerator {
public:
    
    // ========================================================
    // Main API
    // ========================================================
    
    /**
     * @brief Generate complete HTML form from schema
     * @param schema Root schema field
     * @param sceneId Scene identifier for API calls
     * @param apiEndpoint Base API endpoint for saving
     * @return HTML string
     */
    static std::string generateFormHtml(
        const std::vector<YamlUIField>& sections,
        const std::string& sceneId,
        const std::string& apiEndpoint = "/api/scene/update"
    ) {
        std::stringstream html;
        
        html << "<form class='yaml-form' id='scene-form-" << sceneId << "' data-scene='" << sceneId << "'>\n";
        
        for (const auto& section : sections) {
            if (section.type == YamlUIType::GROUP) {
                html << generateGroupHtml(section, sceneId, apiEndpoint, 0);
            } else if (section.type == YamlUIType::LIST) {
                html << generateListHtml(section, sceneId, apiEndpoint);
            } else {
                html << generateFieldHtml(section, sceneId, apiEndpoint);
            }
        }
        
        html << "</form>\n";
        
        return html.str();
    }
    
    /**
     * @brief Generate CSS for YAML UI forms
     */
    static std::string generateCss() {
        return R"CSS(
/* ============================================
   YAML UI Generator Styles
   ============================================ */

.yaml-form {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

/* Section Cards */
.yaml-section {
  background: var(--bg-secondary, #1a1a2e);
  border-radius: 8px;
  border: 1px solid var(--border, #2a2a4e);
  overflow: hidden;
}

.yaml-section-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 16px;
  background: var(--bg-tertiary, #12121f);
  cursor: pointer;
  user-select: none;
}

.yaml-section-header:hover {
  background: var(--bg-hover, #1e1e35);
}

.yaml-section-title {
  display: flex;
  align-items: center;
  gap: 10px;
  font-weight: 600;
  font-size: 14px;
  color: var(--text, #e0e0e0);
}

.yaml-section-icon {
  width: 20px;
  height: 20px;
  opacity: 0.7;
}

.yaml-section-chevron {
  transition: transform 0.2s ease;
  opacity: 0.5;
}

.yaml-section.collapsed .yaml-section-chevron {
  transform: rotate(-90deg);
}

.yaml-section-body {
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.yaml-section.collapsed .yaml-section-body {
  display: none;
}

/* Field Rows */
.yaml-field-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 0;
  border-bottom: 1px solid var(--border-light, #2a2a4e);
}

.yaml-field-row:last-child {
  border-bottom: none;
}

.yaml-field-label {
  flex: 0 0 40%;
  font-size: 13px;
  color: var(--text, #e0e0e0);
  display: flex;
  align-items: center;
  gap: 6px;
}

.yaml-field-label .help-icon {
  width: 14px;
  height: 14px;
  opacity: 0.4;
  cursor: help;
}

.yaml-field-control {
  flex: 0 0 55%;
  display: flex;
  align-items: center;
  gap: 8px;
}

/* Text Input */
.yaml-input-text {
  width: 100%;
  padding: 8px 12px;
  background: var(--bg-tertiary, #12121f);
  border: 1px solid var(--border, #2a2a4e);
  border-radius: 4px;
  color: var(--text, #e0e0e0);
  font-size: 13px;
  transition: border-color 0.2s;
}

.yaml-input-text:focus {
  outline: none;
  border-color: var(--primary, #6366f1);
}

.yaml-input-text:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

/* Number Input */
.yaml-input-number {
  width: 80px;
  padding: 8px 12px;
  background: var(--bg-tertiary, #12121f);
  border: 1px solid var(--border, #2a2a4e);
  border-radius: 4px;
  color: var(--text, #e0e0e0);
  font-size: 13px;
  font-family: monospace;
  text-align: right;
}

.yaml-input-unit {
  font-size: 12px;
  color: var(--text-dim, #888);
  min-width: 24px;
}

/* Slider */
.yaml-slider-container {
  display: flex;
  align-items: center;
  gap: 10px;
  width: 100%;
}

.yaml-slider {
  flex: 1;
  height: 6px;
  -webkit-appearance: none;
  background: var(--bg-tertiary, #12121f);
  border-radius: 3px;
  outline: none;
}

.yaml-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 16px;
  height: 16px;
  background: var(--primary, #6366f1);
  border-radius: 50%;
  cursor: pointer;
  transition: transform 0.1s;
}

.yaml-slider::-webkit-slider-thumb:hover {
  transform: scale(1.1);
}

.yaml-slider-value {
  min-width: 50px;
  text-align: right;
  font-size: 12px;
  font-family: monospace;
  color: var(--text-dim, #888);
}

/* Toggle Switch */
.yaml-toggle {
  position: relative;
  width: 44px;
  height: 24px;
  flex-shrink: 0;
}

.yaml-toggle input {
  opacity: 0;
  width: 0;
  height: 0;
}

.yaml-toggle-slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: var(--bg-tertiary, #12121f);
  border: 1px solid var(--border, #2a2a4e);
  transition: 0.2s;
  border-radius: 24px;
}

.yaml-toggle-slider:before {
  position: absolute;
  content: "";
  height: 18px;
  width: 18px;
  left: 2px;
  bottom: 2px;
  background-color: var(--text-dim, #888);
  transition: 0.2s;
  border-radius: 50%;
}

.yaml-toggle input:checked + .yaml-toggle-slider {
  background-color: var(--primary, #6366f1);
  border-color: var(--primary, #6366f1);
}

.yaml-toggle input:checked + .yaml-toggle-slider:before {
  transform: translateX(20px);
  background-color: white;
}

/* Dropdown Select */
.yaml-select {
  width: 100%;
  padding: 8px 12px;
  background: var(--bg-tertiary, #12121f);
  border: 1px solid var(--border, #2a2a4e);
  border-radius: 4px;
  color: var(--text, #e0e0e0);
  font-size: 13px;
  cursor: pointer;
}

.yaml-select:focus {
  outline: none;
  border-color: var(--primary, #6366f1);
}

/* Color Picker */
.yaml-color-container {
  display: flex;
  align-items: center;
  gap: 10px;
}

.yaml-color-picker {
  width: 40px;
  height: 30px;
  padding: 0;
  border: 1px solid var(--border, #2a2a4e);
  border-radius: 4px;
  cursor: pointer;
  background: transparent;
}

.yaml-color-picker::-webkit-color-swatch-wrapper {
  padding: 2px;
}

.yaml-color-picker::-webkit-color-swatch {
  border-radius: 2px;
  border: none;
}

.yaml-color-value {
  font-size: 12px;
  font-family: monospace;
  color: var(--text-dim, #888);
}

/* File/Sprite Selector */
.yaml-file-select {
  display: flex;
  align-items: center;
  gap: 8px;
  width: 100%;
}

.yaml-file-select select {
  flex: 1;
}

.yaml-file-browse {
  padding: 6px 12px;
  background: var(--bg-tertiary, #12121f);
  border: 1px solid var(--border, #2a2a4e);
  border-radius: 4px;
  color: var(--text, #e0e0e0);
  font-size: 12px;
  cursor: pointer;
  white-space: nowrap;
}

.yaml-file-browse:hover {
  background: var(--bg-hover, #1e1e35);
}

/* Readonly Display */
.yaml-readonly {
  font-size: 13px;
  color: var(--text-dim, #888);
  font-family: monospace;
}

/* Nested Groups */
.yaml-nested-group {
  background: var(--bg-tertiary, #12121f);
  border-radius: 6px;
  padding: 12px;
  margin-top: 8px;
}

.yaml-nested-title {
  font-size: 12px;
  font-weight: 600;
  color: var(--text-dim, #888);
  margin-bottom: 10px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

/* List Items */
.yaml-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.yaml-list-item {
  background: var(--bg-tertiary, #12121f);
  border-radius: 6px;
  padding: 12px;
  position: relative;
}

.yaml-list-item-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
}

.yaml-list-item-title {
  font-size: 13px;
  font-weight: 500;
}

.yaml-list-item-remove {
  width: 24px;
  height: 24px;
  border: none;
  background: var(--danger, #ef4444);
  border-radius: 4px;
  color: white;
  cursor: pointer;
  font-size: 14px;
  line-height: 1;
}

.yaml-list-add {
  padding: 8px 16px;
  background: var(--primary, #6366f1);
  border: none;
  border-radius: 4px;
  color: white;
  font-size: 13px;
  cursor: pointer;
  align-self: flex-start;
}

.yaml-list-add:hover {
  opacity: 0.9;
}

/* Section Description */
.yaml-section-desc {
  font-size: 12px;
  color: var(--text-dim, #888);
  margin-bottom: 12px;
  padding-bottom: 12px;
  border-bottom: 1px solid var(--border-light, #2a2a4e);
}

/* Responsive */
@media (max-width: 600px) {
  .yaml-field-row {
    flex-direction: column;
    align-items: flex-start;
    gap: 8px;
  }
  
  .yaml-field-label,
  .yaml-field-control {
    flex: 1;
    width: 100%;
  }
}
)CSS";
    }
    
    /**
     * @brief Generate JavaScript for YAML UI handling
     */
    static std::string generateJs() {
        return R"JS(
/* ============================================
   YAML UI Generator JavaScript
   ============================================ */

const YamlUI = {
  // API endpoint for updates
  apiEndpoint: '/api/scene/update',
  
  // Current scene ID
  currentSceneId: null,
  
  // Initialize form handlers
  init: function(sceneId, apiEndpoint) {
    this.currentSceneId = sceneId;
    if (apiEndpoint) this.apiEndpoint = apiEndpoint;
    
    // Setup section toggle handlers
    document.querySelectorAll('.yaml-section-header').forEach(header => {
      header.addEventListener('click', () => {
        const section = header.parentElement;
        section.classList.toggle('collapsed');
      });
    });
    
    // Setup input change handlers
    this.setupChangeHandlers();
    
    // Setup list add/remove handlers
    this.setupListHandlers();
    
    // Populate dynamic dropdowns (sprites, files, etc.)
    this.populateDynamicSelects();
  },
  
  // Setup change handlers for all inputs
  setupChangeHandlers: function() {
    const form = document.querySelector('.yaml-form');
    if (!form) return;
    
    // Text inputs (with debounce)
    form.querySelectorAll('.yaml-input-text, .yaml-input-number').forEach(input => {
      let timeout;
      input.addEventListener('input', (e) => {
        clearTimeout(timeout);
        timeout = setTimeout(() => {
          this.updateField(input.dataset.path, input.value, input.dataset.type);
        }, 300);
      });
    });
    
    // Sliders (real-time update display, debounce save)
    form.querySelectorAll('.yaml-slider').forEach(slider => {
      const valueSpan = document.getElementById('val-' + slider.dataset.path.replace(/\./g, '-'));
      let timeout;
      
      slider.addEventListener('input', (e) => {
        // Update display immediately
        if (valueSpan) {
          const unit = slider.dataset.unit || '';
          const isInt = slider.dataset.int === 'true';
          valueSpan.textContent = isInt ? slider.value : parseFloat(slider.value).toFixed(2) + (unit ? ' ' + unit : '');
        }
        
        // Debounce save
        clearTimeout(timeout);
        timeout = setTimeout(() => {
          this.updateField(slider.dataset.path, parseFloat(slider.value), 'number');
        }, 100);
      });
    });
    
    // Toggles
    form.querySelectorAll('.yaml-toggle input').forEach(toggle => {
      toggle.addEventListener('change', (e) => {
        this.updateField(toggle.dataset.path, toggle.checked, 'boolean');
        this.handleConditionalVisibility(toggle.dataset.path, toggle.checked);
      });
    });
    
    // Dropdowns
    form.querySelectorAll('.yaml-select').forEach(select => {
      select.addEventListener('change', (e) => {
        this.updateField(select.dataset.path, select.value, 'string');
      });
    });
    
    // Color pickers
    form.querySelectorAll('.yaml-color-picker').forEach(picker => {
      picker.addEventListener('input', (e) => {
        const rgb = this.hexToRgb(picker.value);
        const valueSpan = document.getElementById('val-' + picker.dataset.path.replace(/\./g, '-'));
        if (valueSpan) {
          valueSpan.textContent = picker.value.toUpperCase();
        }
        this.updateField(picker.dataset.path, rgb, 'color');
      });
    });
    
    // File selectors
    form.querySelectorAll('.yaml-file-select select').forEach(select => {
      select.addEventListener('change', (e) => {
        const type = select.dataset.fileType === 'sprite' ? 'number' : 'string';
        const value = type === 'number' ? parseInt(select.value) : select.value;
        this.updateField(select.dataset.path, value, type);
      });
    });
  },
  
  // Setup list add/remove handlers
  setupListHandlers: function() {
    document.querySelectorAll('.yaml-list-add').forEach(btn => {
      btn.addEventListener('click', (e) => {
        const listPath = btn.dataset.list;
        this.addListItem(listPath);
      });
    });
    
    document.querySelectorAll('.yaml-list-item-remove').forEach(btn => {
      btn.addEventListener('click', (e) => {
        const itemPath = btn.dataset.item;
        this.removeListItem(itemPath);
      });
    });
  },
  
  // Send field update to server
  updateField: function(path, value, type) {
    const body = {
      sceneId: this.currentSceneId,
      path: path,
      value: value,
      type: type
    };
    
    fetch(this.apiEndpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    })
    .then(r => r.json())
    .then(data => {
      if (data.success) {
        this.showSaveIndicator(path, true);
      } else {
        console.error('Update failed:', data.error);
        this.showSaveIndicator(path, false);
      }
    })
    .catch(err => {
      console.error('Update error:', err);
      this.showSaveIndicator(path, false);
    });
  },
  
  // Handle conditional visibility
  handleConditionalVisibility: function(togglePath, isChecked) {
    // Find fields that depend on this toggle
    document.querySelectorAll('[data-visible-if]').forEach(el => {
      const condition = el.dataset.visibleIf;
      if (condition.includes(togglePath)) {
        const shouldShow = condition.includes('== true') ? isChecked : !isChecked;
        el.style.display = shouldShow ? '' : 'none';
      }
    });
  },
  
  // Populate sprite/file selectors dynamically
  populateDynamicSelects: function() {
    // Fetch available sprites
    fetch('/api/sprites')
      .then(r => r.json())
      .then(data => {
        if (data.sprites) {
          document.querySelectorAll('.yaml-file-select select[data-file-type="sprite"]').forEach(select => {
            const currentValue = select.value;
            // Keep first option (None/default)
            while (select.options.length > 1) {
              select.remove(1);
            }
            // Add sprites
            data.sprites.forEach(sprite => {
              const opt = document.createElement('option');
              opt.value = sprite.id;
              opt.textContent = sprite.name || ('Sprite ' + sprite.id);
              if (sprite.id == currentValue) opt.selected = true;
              select.appendChild(opt);
            });
          });
        }
      })
      .catch(err => console.log('Could not load sprites:', err));
  },
  
  // Show save indicator feedback
  showSaveIndicator: function(path, success) {
    const fieldRow = document.querySelector(`[data-field-path="${path}"]`);
    if (fieldRow) {
      const indicator = fieldRow.querySelector('.save-indicator') || document.createElement('span');
      indicator.className = 'save-indicator ' + (success ? 'success' : 'error');
      indicator.textContent = success ? '✓' : '✗';
      if (!fieldRow.querySelector('.save-indicator')) {
        fieldRow.appendChild(indicator);
      }
      setTimeout(() => indicator.remove(), 1500);
    }
  },
  
  // Utility: hex to RGB
  hexToRgb: function(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : { r: 0, g: 0, b: 0 };
  },
  
  // Utility: RGB to hex
  rgbToHex: function(r, g, b) {
    return '#' + [r, g, b].map(x => {
      const hex = x.toString(16);
      return hex.length === 1 ? '0' + hex : hex;
    }).join('');
  },
  
  // Add item to list
  addListItem: function(listPath) {
    fetch('/api/scene/list/add', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        sceneId: this.currentSceneId,
        listPath: listPath
      })
    })
    .then(r => r.json())
    .then(data => {
      if (data.success) {
        location.reload(); // Refresh to show new item
      }
    });
  },
  
  // Remove item from list
  removeListItem: function(itemPath) {
    if (!confirm('Remove this item?')) return;
    
    fetch('/api/scene/list/remove', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        sceneId: this.currentSceneId,
        itemPath: itemPath
      })
    })
    .then(r => r.json())
    .then(data => {
      if (data.success) {
        location.reload();
      }
    });
  }
};

// Auto-init when DOM ready
document.addEventListener('DOMContentLoaded', () => {
  const form = document.querySelector('.yaml-form');
  if (form) {
    YamlUI.init(form.dataset.scene);
  }
});
)JS";
    }

private:
    
    // ========================================================
    // HTML Generation Helpers
    // ========================================================
    
    static std::string generateGroupHtml(
        const YamlUIField& group,
        const std::string& sceneId,
        const std::string& apiEndpoint,
        int depth
    ) {
        std::stringstream html;
        
        if (depth == 0) {
            // Top-level section card
            html << "<div class='yaml-section" << (group.collapsed ? " collapsed" : "") << "'>\n";
            html << "  <div class='yaml-section-header'>\n";
            html << "    <div class='yaml-section-title'>\n";
            if (!group.icon.empty()) {
                html << "      <span class='yaml-section-icon'>" << getIcon(group.icon) << "</span>\n";
            }
            html << "      <span>" << (group.label.empty() ? group.key : group.label) << "</span>\n";
            html << "    </div>\n";
            html << "    <span class='yaml-section-chevron'>▼</span>\n";
            html << "  </div>\n";
            html << "  <div class='yaml-section-body'>\n";
            
            if (!group.description.empty()) {
                html << "    <div class='yaml-section-desc'>" << group.description << "</div>\n";
            }
        } else {
            // Nested group
            html << "<div class='yaml-nested-group'>\n";
            if (!group.label.empty()) {
                html << "  <div class='yaml-nested-title'>" << group.label << "</div>\n";
            }
        }
        
        // Render children
        for (const auto& child : group.children) {
            if (child.type == YamlUIType::GROUP) {
                html << generateGroupHtml(child, sceneId, apiEndpoint, depth + 1);
            } else if (child.type == YamlUIType::LIST) {
                html << generateListHtml(child, sceneId, apiEndpoint);
            } else {
                html << generateFieldHtml(child, sceneId, apiEndpoint);
            }
        }
        
        if (depth == 0) {
            html << "  </div>\n";
            html << "</div>\n";
        } else {
            html << "</div>\n";
        }
        
        return html.str();
    }
    
    static std::string generateListHtml(
        const YamlUIField& list,
        const std::string& sceneId,
        const std::string& apiEndpoint
    ) {
        std::stringstream html;
        
        html << "<div class='yaml-section'>\n";
        html << "  <div class='yaml-section-header'>\n";
        html << "    <div class='yaml-section-title'>\n";
        if (!list.icon.empty()) {
            html << "      <span class='yaml-section-icon'>" << getIcon(list.icon) << "</span>\n";
        }
        html << "      <span>" << (list.label.empty() ? list.key : list.label) << "</span>\n";
        html << "    </div>\n";
        html << "    <span class='yaml-section-chevron'>▼</span>\n";
        html << "  </div>\n";
        html << "  <div class='yaml-section-body'>\n";
        
        html << "    <div class='yaml-list' data-list-path='" << list.path << "'>\n";
        
        // Render existing items
        int index = 0;
        for (const auto& item : list.children) {
            html << "      <div class='yaml-list-item'>\n";
            html << "        <div class='yaml-list-item-header'>\n";
            html << "          <span class='yaml-list-item-title'>Item " << (index + 1) << "</span>\n";
            html << "          <button class='yaml-list-item-remove' data-item='" << item.path << "'>✕</button>\n";
            html << "        </div>\n";
            
            // Render item fields
            for (const auto& field : item.children) {
                html << generateFieldHtml(field, sceneId, apiEndpoint);
            }
            
            html << "      </div>\n";
            index++;
        }
        
        html << "    </div>\n";
        html << "    <button class='yaml-list-add' data-list='" << list.path << "'>+ Add Item</button>\n";
        html << "  </div>\n";
        html << "</div>\n";
        
        return html.str();
    }
    
    static std::string generateFieldHtml(
        const YamlUIField& field,
        const std::string& sceneId,
        const std::string& apiEndpoint
    ) {
        std::stringstream html;
        
        std::string visibility = "";
        if (!field.visibleIf.empty()) {
            visibility = " data-visible-if='" + field.visibleIf + "'";
            visibility += " style='display:none;'"; // Hidden by default, JS will show
        }
        
        html << "<div class='yaml-field-row' data-field-path='" << field.path << "'" << visibility << ">\n";
        html << "  <label class='yaml-field-label'>\n";
        html << "    " << (field.label.empty() ? field.key : field.label);
        if (!field.description.empty()) {
            html << "    <span class='help-icon' title='" << escapeHtml(field.description) << "'>?</span>";
        }
        html << "\n  </label>\n";
        html << "  <div class='yaml-field-control'>\n";
        
        switch (field.type) {
            case YamlUIType::TEXT:
                html << generateTextInput(field);
                break;
            case YamlUIType::NUMBER:
                html << generateNumberInput(field);
                break;
            case YamlUIType::SLIDER:
                html << generateSliderInput(field);
                break;
            case YamlUIType::TOGGLE:
                html << generateToggleInput(field);
                break;
            case YamlUIType::DROPDOWN:
                html << generateDropdownInput(field);
                break;
            case YamlUIType::COLOR:
                html << generateColorInput(field);
                break;
            case YamlUIType::FILE:
                html << generateFileInput(field);
                break;
            case YamlUIType::READONLY:
                html << generateReadonlyDisplay(field);
                break;
            default:
                html << "    <span class='yaml-readonly'>Unsupported type</span>\n";
                break;
        }
        
        html << "  </div>\n";
        html << "</div>\n";
        
        return html.str();
    }
    
    // ========================================================
    // Individual Control Generators
    // ========================================================
    
    static std::string generateTextInput(const YamlUIField& field) {
        std::stringstream html;
        std::string disabled = field.readonly ? " disabled" : "";
        
        if (field.multiline) {
            html << "    <textarea class='yaml-input-text' data-path='" << field.path << "' ";
            html << "data-type='string' maxlength='" << field.maxLength << "'" << disabled << ">";
            html << escapeHtml(field.stringValue) << "</textarea>\n";
        } else {
            html << "    <input type='text' class='yaml-input-text' data-path='" << field.path << "' ";
            html << "data-type='string' value='" << escapeHtml(field.stringValue) << "' ";
            html << "maxlength='" << field.maxLength << "'" << disabled << ">\n";
        }
        
        return html.str();
    }
    
    static std::string generateNumberInput(const YamlUIField& field) {
        std::stringstream html;
        std::string disabled = field.readonly ? " disabled" : "";
        
        html << "    <input type='number' class='yaml-input-number' data-path='" << field.path << "' ";
        html << "data-type='number' value='" << field.floatValue << "' ";
        html << "min='" << field.minValue << "' max='" << field.maxValue << "' step='" << field.step << "'";
        html << disabled << ">\n";
        
        if (!field.unit.empty()) {
            html << "    <span class='yaml-input-unit'>" << field.unit << "</span>\n";
        }
        
        return html.str();
    }
    
    static std::string generateSliderInput(const YamlUIField& field) {
        std::stringstream html;
        std::string pathId = field.path;
        std::replace(pathId.begin(), pathId.end(), '.', '-');
        
        bool isInt = (field.step == 1.0f);
        std::string value = isInt ? std::to_string(field.intValue) : floatStr(field.floatValue);
        
        html << "    <div class='yaml-slider-container'>\n";
        html << "      <input type='range' class='yaml-slider' data-path='" << field.path << "' ";
        html << "data-int='" << (isInt ? "true" : "false") << "' ";
        html << "data-unit='" << field.unit << "' ";
        html << "min='" << floatStr(field.minValue) << "' max='" << floatStr(field.maxValue) << "' ";
        html << "step='" << floatStr(field.step) << "' value='" << value << "'>\n";
        html << "      <span class='yaml-slider-value' id='val-" << pathId << "'>" << value;
        if (!field.unit.empty()) html << " " << field.unit;
        html << "</span>\n";
        html << "    </div>\n";
        
        return html.str();
    }
    
    static std::string generateToggleInput(const YamlUIField& field) {
        std::stringstream html;
        std::string checked = field.boolValue ? " checked" : "";
        
        html << "    <label class='yaml-toggle'>\n";
        html << "      <input type='checkbox' data-path='" << field.path << "'" << checked << ">\n";
        html << "      <span class='yaml-toggle-slider'></span>\n";
        html << "    </label>\n";
        
        return html.str();
    }
    
    static std::string generateDropdownInput(const YamlUIField& field) {
        std::stringstream html;
        
        html << "    <select class='yaml-select' data-path='" << field.path << "'>\n";
        
        for (const auto& opt : field.options) {
            std::string selected = (opt.value == field.stringValue) ? " selected" : "";
            html << "      <option value='" << escapeHtml(opt.value) << "'" << selected;
            if (!opt.desc.empty()) {
                html << " title='" << escapeHtml(opt.desc) << "'";
            }
            html << ">" << escapeHtml(opt.label) << "</option>\n";
        }
        
        html << "    </select>\n";
        
        return html.str();
    }
    
    static std::string generateColorInput(const YamlUIField& field) {
        std::stringstream html;
        std::string pathId = field.path;
        std::replace(pathId.begin(), pathId.end(), '.', '-');
        std::string hexColor = field.colorValue.toHex();
        
        html << "    <div class='yaml-color-container'>\n";
        html << "      <input type='color' class='yaml-color-picker' data-path='" << field.path << "' ";
        html << "value='" << hexColor << "'>\n";
        html << "      <span class='yaml-color-value' id='val-" << pathId << "'>" << hexColor << "</span>\n";
        html << "    </div>\n";
        
        return html.str();
    }
    
    static std::string generateFileInput(const YamlUIField& field) {
        std::stringstream html;
        
        html << "    <div class='yaml-file-select'>\n";
        html << "      <select class='yaml-select' data-path='" << field.path << "' ";
        html << "data-file-type='" << field.fileType << "'>\n";
        
        if (field.allowNone) {
            std::string selected = (field.intValue < 0) ? " selected" : "";
            html << "        <option value='-1'" << selected << ">None (use default)</option>\n";
        }
        
        // Current value as fallback (will be populated by JS)
        if (field.intValue >= 0) {
            html << "        <option value='" << field.intValue << "' selected>Sprite " << field.intValue << "</option>\n";
        }
        
        html << "      </select>\n";
        html << "    </div>\n";
        
        return html.str();
    }
    
    static std::string generateReadonlyDisplay(const YamlUIField& field) {
        std::stringstream html;
        
        html << "    <span class='yaml-readonly'>" << escapeHtml(field.stringValue) << "</span>\n";
        
        return html.str();
    }
    
    // ========================================================
    // Utility Functions
    // ========================================================
    
    static std::string escapeHtml(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '&': result += "&amp;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&#39;"; break;
                default: result += c; break;
            }
        }
        return result;
    }
    
    static std::string floatStr(float val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4f", val);
        char* p = buf + strlen(buf) - 1;
        while (p > buf && *p == '0') *p-- = '\0';
        if (*p == '.') *p = '\0';
        return buf;
    }
    
    static std::string getIcon(const std::string& name) {
        // Simple SVG icons
        if (name == "info") return "ℹ";
        if (name == "display") return "◐";
        if (name == "lightbulb") return "💡";
        if (name == "image") return "🖼";
        if (name == "microphone") return "🎤";
        if (name == "bolt") return "⚡";
        if (name == "settings") return "⚙";
        if (name == "audio") return "🔊";
        return "○";
    }
};

} // namespace Web
} // namespace SystemAPI
