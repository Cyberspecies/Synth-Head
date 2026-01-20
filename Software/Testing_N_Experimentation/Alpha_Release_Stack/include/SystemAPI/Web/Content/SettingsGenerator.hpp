/*****************************************************************
 * @file SettingsGenerator.hpp
 * @brief Auto-generates Web UI Settings from Parameter Definitions
 * 
 * Takes parameter definitions from animation sets and generates
 * HTML/JavaScript code for the settings interface.
 * 
 * Supports:
 * - Sliders (float and int)
 * - Toggles
 * - Color pickers
 * - Dropdowns
 * - Input/Sprite/Equation selectors
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "AnimationSystem/ParameterDef.hpp"
#include <string>
#include <vector>
#include <sstream>

namespace SystemAPI {
namespace Web {

/**
 * @brief Generates HTML/JS for animation settings
 */
class SettingsGenerator {
public:
    /**
     * @brief Generate HTML for a list of parameters
     * @param params List of parameter definitions
     * @param formId ID of the form container
     * @param apiEndpoint API endpoint for saving values
     */
    static std::string generateParametersHtml(
        const std::vector<AnimationSystem::ParameterDef>& params,
        const std::string& animationSetId,
        const std::string& apiEndpoint = "/api/animation/param"
    ) {
        std::stringstream html;
        
        std::string currentCategory;
        
        for (const auto& p : params) {
            if (!p.visible) continue;
            
            // Handle separators
            if (p.type == AnimationSystem::ParameterType::SEPARATOR) {
                if (!p.name.empty()) {
                    html << "<div class='param-separator'>" << p.name << "</div>\n";
                } else {
                    html << "<hr class='param-divider'>\n";
                }
                continue;
            }
            
            // Handle labels
            if (p.type == AnimationSystem::ParameterType::LABEL) {
                html << "<div class='param-label'>" << p.name << "</div>\n";
                continue;
            }
            
            // Generate control based on type
            html << "<div class='param-row' id='param-" << p.id << "'>\n";
            html << "  <label>" << p.name;
            if (!p.description.empty()) {
                html << " <span class='tooltip' title='" << p.description << "'>?</span>";
            }
            html << "</label>\n";
            html << "  <div class='param-control'>\n";
            
            switch (p.type) {
                case AnimationSystem::ParameterType::SLIDER:
                    html << generateSlider(p, animationSetId, apiEndpoint, false);
                    break;
                    
                case AnimationSystem::ParameterType::SLIDER_INT:
                    html << generateSlider(p, animationSetId, apiEndpoint, true);
                    break;
                    
                case AnimationSystem::ParameterType::TOGGLE:
                    html << generateToggle(p, animationSetId, apiEndpoint);
                    break;
                    
                case AnimationSystem::ParameterType::COLOR:
                    html << generateColorPicker(p, animationSetId, apiEndpoint);
                    break;
                    
                case AnimationSystem::ParameterType::DROPDOWN:
                    html << generateDropdown(p, animationSetId, apiEndpoint);
                    break;
                    
                case AnimationSystem::ParameterType::INPUT_SELECT:
                    html << generateInputSelect(p, animationSetId, apiEndpoint);
                    break;
                    
                case AnimationSystem::ParameterType::SPRITE_SELECT:
                    html << generateSpriteSelect(p, animationSetId, apiEndpoint);
                    break;
                    
                case AnimationSystem::ParameterType::BUTTON:
                    html << generateButton(p, animationSetId, apiEndpoint);
                    break;
                    
                default:
                    html << "    <span>Unsupported type</span>\n";
                    break;
            }
            
            html << "  </div>\n";
            html << "</div>\n";
        }
        
        return html.str();
    }
    
    /**
     * @brief Generate CSS for parameter controls
     */
    static std::string generateParametersCss() {
        return R"CSS(
.param-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 0;
  border-bottom: 1px solid var(--border);
}

.param-row:last-child {
  border-bottom: none;
}

.param-row label {
  flex: 0 0 40%;
  font-size: 14px;
  color: var(--text);
}

.param-control {
  flex: 0 0 55%;
  display: flex;
  align-items: center;
  gap: 8px;
}

.param-separator {
  font-weight: 600;
  font-size: 13px;
  color: var(--primary);
  padding: 12px 0 6px 0;
  margin-top: 8px;
  border-bottom: 1px solid var(--primary);
}

.param-divider {
  border: none;
  border-top: 1px solid var(--border);
  margin: 12px 0;
}

.param-label {
  font-size: 12px;
  color: var(--text-dim);
  padding: 4px 0;
}

.param-slider {
  flex: 1;
  height: 6px;
  -webkit-appearance: none;
  background: var(--bg-tertiary);
  border-radius: 3px;
  outline: none;
}

.param-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 16px;
  height: 16px;
  background: var(--primary);
  border-radius: 50%;
  cursor: pointer;
}

.param-value {
  min-width: 50px;
  text-align: right;
  font-size: 13px;
  font-family: monospace;
  color: var(--text-dim);
}

.param-toggle {
  position: relative;
  width: 44px;
  height: 24px;
}

.param-toggle input {
  opacity: 0;
  width: 0;
  height: 0;
}

.param-toggle .slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: var(--bg-tertiary);
  transition: 0.2s;
  border-radius: 24px;
}

.param-toggle .slider:before {
  position: absolute;
  content: "";
  height: 18px;
  width: 18px;
  left: 3px;
  bottom: 3px;
  background-color: white;
  transition: 0.2s;
  border-radius: 50%;
}

.param-toggle input:checked + .slider {
  background-color: var(--primary);
}

.param-toggle input:checked + .slider:before {
  transform: translateX(20px);
}

.param-color {
  width: 60px;
  height: 30px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  padding: 0;
}

.param-select {
  flex: 1;
  padding: 6px 10px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 4px;
  color: var(--text);
  font-size: 13px;
}

.param-button {
  padding: 6px 16px;
  background: var(--primary);
  border: none;
  border-radius: 4px;
  color: white;
  font-size: 13px;
  cursor: pointer;
}

.param-button:hover {
  opacity: 0.9;
}

.tooltip {
  display: inline-block;
  width: 14px;
  height: 14px;
  background: var(--bg-tertiary);
  border-radius: 50%;
  text-align: center;
  font-size: 10px;
  line-height: 14px;
  cursor: help;
  margin-left: 4px;
}
)CSS";
    }
    
    /**
     * @brief Generate JavaScript for parameter handling
     */
    static std::string generateParametersJs() {
        return R"JS(
function updateParam(setId, paramId, value, type) {
  let body = { setId: setId, paramId: paramId };
  
  if (type === 'float' || type === 'int') {
    body.value = parseFloat(value);
  } else if (type === 'bool') {
    body.value = value === true || value === 'true';
  } else if (type === 'color') {
    // value is {r, g, b}
    body.r = value.r;
    body.g = value.g;
    body.b = value.b;
  } else {
    body.value = value;
  }
  
  fetch('/api/animation/param', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  }).then(r => r.json()).then(data => {
    if (!data.success) {
      console.error('Failed to update param:', data.error);
    }
  }).catch(err => {
    console.error('Error updating param:', err);
  });
}

function hexToRgb(hex) {
  const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return result ? {
    r: parseInt(result[1], 16),
    g: parseInt(result[2], 16),
    b: parseInt(result[3], 16)
  } : null;
}

function rgbToHex(r, g, b) {
  return '#' + [r, g, b].map(x => {
    const hex = x.toString(16);
    return hex.length === 1 ? '0' + hex : hex;
  }).join('');
}
)JS";
    }
    
private:
    static std::string generateSlider(
        const AnimationSystem::ParameterDef& p,
        const std::string& setId,
        const std::string& apiEndpoint,
        bool isInt
    ) {
        std::stringstream html;
        std::string value = isInt ? std::to_string(p.intValue) : floatStr(p.floatValue);
        std::string step = isInt ? "1" : floatStr(p.step);
        std::string type = isInt ? "int" : "float";
        
        html << "    <input type='range' class='param-slider' ";
        html << "min='" << floatStr(p.minValue) << "' ";
        html << "max='" << floatStr(p.maxValue) << "' ";
        html << "step='" << step << "' ";
        html << "value='" << value << "' ";
        html << "oninput=\"document.getElementById('val-" << p.id << "').textContent = ";
        if (isInt) {
            html << "this.value; ";
        } else {
            html << "parseFloat(this.value).toFixed(2); ";
        }
        html << "updateParam('" << setId << "', '" << p.id << "', this.value, '" << type << "');\">\n";
        html << "    <span class='param-value' id='val-" << p.id << "'>" << value;
        if (!p.unit.empty()) {
            html << " " << p.unit;
        }
        html << "</span>\n";
        
        return html.str();
    }
    
    static std::string generateToggle(
        const AnimationSystem::ParameterDef& p,
        const std::string& setId,
        const std::string& apiEndpoint
    ) {
        std::stringstream html;
        
        html << "    <label class='param-toggle'>\n";
        html << "      <input type='checkbox' ";
        if (p.boolValue) html << "checked ";
        html << "onchange=\"updateParam('" << setId << "', '" << p.id << "', this.checked, 'bool');\">\n";
        html << "      <span class='slider'></span>\n";
        html << "    </label>\n";
        
        return html.str();
    }
    
    static std::string generateColorPicker(
        const AnimationSystem::ParameterDef& p,
        const std::string& setId,
        const std::string& apiEndpoint
    ) {
        std::stringstream html;
        char hexColor[8];
        snprintf(hexColor, sizeof(hexColor), "#%02x%02x%02x", p.colorR, p.colorG, p.colorB);
        
        html << "    <input type='color' class='param-color' ";
        html << "value='" << hexColor << "' ";
        html << "onchange=\"var rgb = hexToRgb(this.value); ";
        html << "updateParam('" << setId << "', '" << p.id << "', rgb, 'color');\">\n";
        
        return html.str();
    }
    
    static std::string generateDropdown(
        const AnimationSystem::ParameterDef& p,
        const std::string& setId,
        const std::string& apiEndpoint
    ) {
        std::stringstream html;
        
        html << "    <select class='param-select' ";
        html << "onchange=\"updateParam('" << setId << "', '" << p.id << "', this.value, 'int');\">\n";
        
        for (const auto& opt : p.options) {
            html << "      <option value='" << opt.value << "'";
            if (opt.value == p.intValue) html << " selected";
            html << ">" << opt.label << "</option>\n";
        }
        
        html << "    </select>\n";
        
        return html.str();
    }
    
    static std::string generateInputSelect(
        const AnimationSystem::ParameterDef& p,
        const std::string& setId,
        const std::string& apiEndpoint
    ) {
        std::stringstream html;
        
        // This will be populated dynamically from available inputs
        html << "    <select class='param-select input-select' data-param='" << p.id << "' ";
        html << "onchange=\"updateParam('" << setId << "', '" << p.id << "', this.value, 'string');\">\n";
        html << "      <option value='" << p.stringValue << "' selected>" << p.stringValue << "</option>\n";
        html << "    </select>\n";
        
        return html.str();
    }
    
    static std::string generateSpriteSelect(
        const AnimationSystem::ParameterDef& p,
        const std::string& setId,
        const std::string& apiEndpoint
    ) {
        std::stringstream html;
        
        // This will be populated dynamically from available sprites
        html << "    <select class='param-select sprite-select' data-param='" << p.id << "' ";
        html << "onchange=\"updateParam('" << setId << "', '" << p.id << "', this.value, 'int');\">\n";
        html << "      <option value='-1'>None (use default)</option>\n";
        if (p.intValue >= 0) {
            html << "      <option value='" << p.intValue << "' selected>Sprite " << p.intValue << "</option>\n";
        }
        html << "    </select>\n";
        
        return html.str();
    }
    
    static std::string generateButton(
        const AnimationSystem::ParameterDef& p,
        const std::string& setId,
        const std::string& apiEndpoint
    ) {
        std::stringstream html;
        
        html << "    <button class='param-button' ";
        html << "onclick=\"updateParam('" << setId << "', '" << p.id << "', true, 'button');\">";
        html << p.name << "</button>\n";
        
        return html.str();
    }
    
    static std::string floatStr(float val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4f", val);
        // Remove trailing zeros
        char* p = buf + strlen(buf) - 1;
        while (p > buf && *p == '0') *p-- = '\0';
        if (*p == '.') *p = '\0';
        return buf;
    }
};

} // namespace Web
} // namespace SystemAPI
