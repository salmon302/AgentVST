#include "UIGenerator.h"
#include <sstream>
#include <algorithm>

namespace AgentVST {

std::string UIGenerator::generateHTML(const PluginSchema& schema,
                                       const std::string& /*cssPath*/) const {
    std::ostringstream html;

    html << "<!DOCTYPE html>\n"
         << "<html lang=\"en\">\n"
         << "<head>\n"
         << "  <meta charset=\"UTF-8\">\n"
         << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
         << "  <title>" << escapeHtml(schema.metadata.name) << "</title>\n"
         << "  <style>\n" << generateCSS() << "\n  </style>\n"
         << "</head>\n"
         << "<body>\n"
         << "  <div class=\"plugin-container\">\n"
         << "    <header class=\"plugin-header\">\n"
         << "      <h1>" << escapeHtml(schema.metadata.name) << "</h1>\n"
         << "      <span class=\"plugin-version\">v" << escapeHtml(schema.metadata.version) << "</span>\n"
         << "    </header>\n"
         << "    <div class=\"controls\">\n";

    // Render grouped parameters
    for (const auto& group : schema.groups) {
        html << generateGroupSection(group, schema);
    }

    // Render ungrouped parameters
    auto ungrouped = schema.getUngroupedParameters();
    if (!ungrouped.empty()) {
        html << "      <div class=\"param-group\">\n";
        for (const auto* param : ungrouped) {
            html << generateParameterControl(*param);
        }
        html << "      </div>\n";
    }

    html << "    </div>\n"  // .controls
         << "  </div>\n"    // .plugin-container
         << "  <script>\n" << generateJS(schema) << "\n  </script>\n"
         << "</body>\n"
         << "</html>\n";

    return html.str();
}

std::string UIGenerator::generateCSS() const {
    return R"CSS(
:root {
  --bg: #1a1a2e;
  --surface: #16213e;
  --accent: #0f3460;
  --highlight: #e94560;
  --text: #eaeaea;
  --text-muted: #888;
  --border: #2d2d4e;
  --knob-track: #0f3460;
  --knob-fill: #e94560;
}

* { box-sizing: border-box; margin: 0; padding: 0; }

body {
  background: var(--bg);
  color: var(--text);
  font-family: 'Inter', 'Segoe UI', system-ui, sans-serif;
  font-size: 13px;
  min-height: 100vh;
}

.plugin-container {
  display: flex;
  flex-direction: column;
  padding: 16px;
  gap: 12px;
  min-width: 400px;
}

.plugin-header {
  display: flex;
  align-items: baseline;
  gap: 10px;
  border-bottom: 1px solid var(--border);
  padding-bottom: 8px;
}

.plugin-header h1 {
  font-size: 18px;
  font-weight: 700;
  color: var(--highlight);
  letter-spacing: 0.05em;
}

.plugin-version {
  font-size: 11px;
  color: var(--text-muted);
}

.controls {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
}

.param-group {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 12px;
  min-width: 200px;
}

.param-group-title {
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.1em;
  color: var(--text-muted);
  margin-bottom: 10px;
}

.param-row {
  display: flex;
  flex-direction: column;
  gap: 4px;
  margin-bottom: 12px;
}

.param-row:last-child { margin-bottom: 0; }

.param-label {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.param-name {
  font-size: 12px;
  color: var(--text);
}

.param-value {
  font-size: 11px;
  color: var(--highlight);
  font-variant-numeric: tabular-nums;
  min-width: 60px;
  text-align: right;
}

input[type="range"] {
  -webkit-appearance: none;
  width: 100%;
  height: 4px;
  border-radius: 2px;
  background: var(--knob-track);
  outline: none;
  cursor: pointer;
}

input[type="range"]::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 14px;
  height: 14px;
  border-radius: 50%;
  background: var(--knob-fill);
  cursor: pointer;
  border: 2px solid var(--bg);
  transition: transform 0.1s;
}

input[type="range"]::-webkit-slider-thumb:hover {
  transform: scale(1.2);
}

input[type="checkbox"] {
  width: 36px;
  height: 20px;
  cursor: pointer;
  accent-color: var(--highlight);
}

select {
  width: 100%;
  background: var(--accent);
  color: var(--text);
  border: 1px solid var(--border);
  border-radius: 4px;
  padding: 4px 8px;
  font-size: 12px;
  cursor: pointer;
  outline: none;
}
)CSS";
}

std::string UIGenerator::generateJS(const PluginSchema& schema) const {
    std::ostringstream js;
    js << R"JS(
// AgentVST Parameter Bridge (Phase 0: stub — replaced by JUCE 8 WebView bridge in Phase 3)
const AgentVST = {
    params: {},

    init() {
        // In Phase 3, this will call Juce.getSliderState() for live binding.
        // For now, we wire HTML controls to a local state object.
        document.querySelectorAll('[data-param-id]').forEach(el => {
            const id = el.dataset.paramId;
            const display = document.querySelector('[data-param-display="' + id + '"]');

            el.addEventListener('input', () => {
                AgentVST.params[id] = parseFloat(el.value);
                if (display) {
                    display.textContent = parseFloat(el.value).toFixed(2);
                }
                // Phase 3: will call Juce.setNormalisedValue(id, normalised)
            });
        });
    }
};

document.addEventListener('DOMContentLoaded', () => AgentVST.init());
)JS";
    return js.str();
}

std::string UIGenerator::generateParameterControl(const ParameterDef& param) const {
    std::ostringstream html;
    html << "        <div class=\"param-row\">\n"
         << "          <div class=\"param-label\">\n"
         << "            <span class=\"param-name\">" << escapeHtml(param.name) << "</span>\n"
         << "            <span class=\"param-value\" data-param-display=\""
         << escapeHtml(param.id) << "\">"
         << param.defaultValue << "</span>\n"
         << "          </div>\n";

    if (param.type == "boolean") {
        html << "          <input type=\"checkbox\""
             << " data-param-id=\"" << escapeHtml(param.id) << "\""
             << (param.defaultValue >= 0.5f ? " checked" : "") << ">\n";
    } else if (param.type == "enum") {
        html << "          <select data-param-id=\"" << escapeHtml(param.id) << "\">\n";
        for (std::size_t i = 0; i < param.enumOptions.size(); ++i) {
            html << "            <option value=\"" << i << "\""
                 << (static_cast<int>(param.defaultValue) == static_cast<int>(i) ? " selected" : "")
                 << ">" << escapeHtml(param.enumOptions[i]) << "</option>\n";
        }
        html << "          </select>\n";
    } else {
        // float or int
        html << "          <input type=\"range\""
             << " data-param-id=\"" << escapeHtml(param.id) << "\""
             << " min=\"" << param.minValue << "\""
             << " max=\"" << param.maxValue << "\""
             << " step=\"" << (param.step > 0.0f ? param.step : (param.maxValue - param.minValue) / 1000.0f) << "\""
             << " value=\"" << param.defaultValue << "\">\n";
    }

    html << "        </div>\n";
    return html.str();
}

std::string UIGenerator::generateGroupSection(const ParameterGroup& group,
                                               const PluginSchema& schema) const {
    auto params = schema.getParametersInGroup(group.id);
    if (params.empty()) return {};

    std::ostringstream html;
    html << "      <div class=\"param-group\">\n"
         << "        <div class=\"param-group-title\">" << escapeHtml(group.name) << "</div>\n";
    for (const auto* p : params)
        html << generateParameterControl(*p);
    html << "      </div>\n";
    return html.str();
}

std::string UIGenerator::escapeHtml(const std::string& str) const {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}

} // namespace AgentVST
