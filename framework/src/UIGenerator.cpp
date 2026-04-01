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
         << "    <section class=\"meter-strip\">\n"
         << "      <div class=\"meter-panel\">\n"
         << "        <div class=\"meter-panel-title\">Input</div>\n"
         << "        <div class=\"meter-row\"><span>Peak L</span><div class=\"meter-bar\" data-meter-id=\"inputPeakL\"><div class=\"meter-fill\"></div></div></div>\n"
         << "        <div class=\"meter-row\"><span>Peak R</span><div class=\"meter-bar\" data-meter-id=\"inputPeakR\"><div class=\"meter-fill\"></div></div></div>\n"
         << "        <div class=\"meter-row\"><span>RMS L</span><div class=\"meter-bar\" data-meter-id=\"inputRmsL\"><div class=\"meter-fill\"></div></div></div>\n"
         << "        <div class=\"meter-row\"><span>RMS R</span><div class=\"meter-bar\" data-meter-id=\"inputRmsR\"><div class=\"meter-fill\"></div></div></div>\n"
         << "      </div>\n"
         << "      <div class=\"meter-panel\">\n"
         << "        <div class=\"meter-panel-title\">Output</div>\n"
         << "        <div class=\"meter-row\"><span>Peak L</span><div class=\"meter-bar\" data-meter-id=\"outputPeakL\"><div class=\"meter-fill\"></div></div></div>\n"
         << "        <div class=\"meter-row\"><span>Peak R</span><div class=\"meter-bar\" data-meter-id=\"outputPeakR\"><div class=\"meter-fill\"></div></div></div>\n"
         << "        <div class=\"meter-row\"><span>RMS L</span><div class=\"meter-bar\" data-meter-id=\"outputRmsL\"><div class=\"meter-fill\"></div></div></div>\n"
         << "        <div class=\"meter-row\"><span>RMS R</span><div class=\"meter-bar\" data-meter-id=\"outputRmsR\"><div class=\"meter-fill\"></div></div></div>\n"
         << "      </div>\n"
         << "    </section>\n"
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
  --highlight-soft: #ff9b71;
  --text: #eaeaea;
  --text-muted: #888;
  --border: #2d2d4e;
  --knob-track: #0f3460;
  --knob-fill: #e94560;
  --meter-bg: #0d1631;
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

.meter-strip {
  display: grid;
  grid-template-columns: repeat(2, minmax(220px, 1fr));
  gap: 12px;
}

.meter-panel {
  background: linear-gradient(180deg, rgba(24, 46, 85, 0.9), rgba(10, 20, 40, 0.95));
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.meter-panel-title {
  font-size: 11px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--text-muted);
  font-weight: 600;
}

.meter-row {
  display: grid;
  grid-template-columns: 56px 1fr;
  align-items: center;
  gap: 8px;
  font-size: 11px;
  color: var(--text-muted);
}

.meter-bar {
  position: relative;
  height: 8px;
  border-radius: 999px;
  background: var(--meter-bg);
  border: 1px solid rgba(255, 255, 255, 0.08);
  overflow: hidden;
}

.meter-fill {
  position: absolute;
  inset: 0;
  transform: scaleX(0);
  transform-origin: left center;
  background: linear-gradient(90deg, var(--highlight), var(--highlight-soft));
  transition: transform 80ms linear;
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

@media (max-width: 720px) {
  .plugin-container {
    min-width: 0;
    padding: 12px;
  }

  .meter-strip {
    grid-template-columns: 1fr;
  }

  .controls {
    flex-direction: column;
  }

  .param-group {
    min-width: 0;
  }
}
)CSS";
}

std::string UIGenerator::generateJS(const PluginSchema& schema) const {
    std::ostringstream js;

    js << "const AgentVSTParameterDefs = {\n";
    for (const auto& param : schema.parameters) {
        js << "  \"" << escapeJs(param.id) << "\": {\n"
           << "    type: \"" << escapeJs(param.type) << "\",\n"
           << "    unit: \"" << escapeJs(param.unit) << "\",\n"
           << "    min: " << param.minValue << ",\n"
           << "    max: " << param.maxValue << ",\n"
           << "    step: " << (param.step > 0.0f ? param.step : 0.0f) << ",\n"
           << "    enumOptions: [";

        for (std::size_t i = 0; i < param.enumOptions.size(); ++i) {
            if (i > 0)
                js << ", ";
            js << "\"" << escapeJs(param.enumOptions[i]) << "\"";
        }

        js << "]\n"
           << "  },\n";
    }
    js << "};\n\n";

    js << R"JS(
const JuceNative = (() => {
  const pending = new Map();
  let nextPromiseId = 1;

  const hasBackend = () =>
    typeof window.__JUCE__ !== "undefined" &&
    window.__JUCE__ !== null &&
    typeof window.__JUCE__.backend !== "undefined";

  const invoke = (name, ...params) => {
    if (!hasBackend()) {
      return Promise.resolve(undefined);
    }

    return new Promise((resolve) => {
      const promiseId = nextPromiseId++;
      pending.set(promiseId, resolve);

      window.__JUCE__.backend.emitEvent("__juce__invoke", {
        name,
        params,
        resultId: promiseId
      });
    });
  };

  if (hasBackend()) {
    window.__JUCE__.backend.addEventListener("__juce__complete", (payload) => {
      if (!payload || typeof payload.promiseId !== "number") {
        return;
      }

      const completion = pending.get(payload.promiseId);
      if (!completion) {
        return;
      }

      pending.delete(payload.promiseId);
      completion(payload.result);
    });
  }

  return { hasBackend, invoke };
})();

const AgentVST = {
  controls: new Map(),
  displays: new Map(),
  meterFills: new Map(),
  isSyncingFromHost: false,
  pollTimer: null,
  backendEventTokens: [],

  init() {
    document.querySelectorAll("[data-param-id]").forEach((el) => {
      const id = el.dataset.paramId;
      this.controls.set(id, el);

      const display = document.querySelector('[data-param-display="' + id + '"]');
      if (display) {
        this.displays.set(id, display);
      }

      el.addEventListener("input", () => {
        void this.handleControlChanged(id);
      });

      this.updateDisplay(id, this.readControlValue(id));
    });

    document.querySelectorAll("[data-meter-id]").forEach((el) => {
      const id = el.dataset.meterId;
      const fill = el.querySelector(".meter-fill");
      if (fill) {
        this.meterFills.set(id, fill);
      }
    });

    if (JuceNative.hasBackend() &&
        typeof window.__JUCE__.backend.addEventListener === "function") {
      this.backendEventTokens.push(window.__JUCE__.backend.addEventListener(
        "agentParameterSnapshot",
        (values) => {
          this.applySnapshot(values);
        }
      ));

      this.backendEventTokens.push(window.__JUCE__.backend.addEventListener(
        "agentMeterSnapshot",
        (values) => {
          this.applyMeterSnapshot(values);
        }
      ));
    }

    void this.syncFromHost();
    void this.syncMetersFromHost();
    this.pollTimer = window.setInterval(() => {
      void this.syncFromHost();
      void this.syncMetersFromHost();
    }, 750);
  },

  getDef(id) {
    return AgentVSTParameterDefs[id] || { type: "float", unit: "", enumOptions: [] };
  },

  readControlValue(id) {
    const control = this.controls.get(id);
    if (!control) {
      return undefined;
    }

    const def = this.getDef(id);

    if (control.type === "checkbox") {
      return control.checked;
    }

    if (control.tagName === "SELECT") {
      return Number.parseInt(control.value, 10) || 0;
    }

    const parsed = Number.parseFloat(control.value);
    if (!Number.isFinite(parsed)) {
      return def.type === "int" || def.type === "enum" ? 0 : 0.0;
    }

    if (def.type === "int" || def.type === "enum") {
      return Math.round(parsed);
    }

    return parsed;
  },

  writeControlValue(id, value) {
    const control = this.controls.get(id);
    if (!control) {
      return;
    }

    const def = this.getDef(id);

    if (control.type === "checkbox") {
      control.checked = Boolean(value);
      return;
    }

    if (control.tagName === "SELECT") {
      const index = Number.parseInt(value, 10);
      control.value = Number.isFinite(index) ? String(index) : "0";
      return;
    }

    const numeric = Number.parseFloat(value);
    if (!Number.isFinite(numeric)) {
      return;
    }

    control.value = String(def.type === "int" ? Math.round(numeric) : numeric);
  },

  formatValue(id, value) {
    const def = this.getDef(id);

    if (def.type === "boolean") {
      return value ? "On" : "Off";
    }

    if (def.type === "enum") {
      const index = Number.parseInt(value, 10) || 0;
      if (Array.isArray(def.enumOptions) && index >= 0 && index < def.enumOptions.length) {
        return def.enumOptions[index];
      }
      return String(index);
    }

    if (def.type === "int") {
      const intValue = Number.parseInt(value, 10) || 0;
      return def.unit ? String(intValue) + " " + def.unit : String(intValue);
    }

    const floatValue = Number.parseFloat(value);
    if (!Number.isFinite(floatValue)) {
      return "";
    }

    const display = floatValue.toFixed(2);
    return def.unit ? display + " " + def.unit : display;
  },

  updateDisplay(id, value) {
    const display = this.displays.get(id);
    if (!display) {
      return;
    }

    display.textContent = this.formatValue(id, value);
  },

  applySnapshot(values) {
    if (!values || typeof values !== "object") {
      return;
    }

    this.isSyncingFromHost = true;

    this.controls.forEach((_, id) => {
      if (!Object.prototype.hasOwnProperty.call(values, id)) {
        return;
      }

      const value = values[id];
      this.writeControlValue(id, value);
      this.updateDisplay(id, value);
    });

    this.isSyncingFromHost = false;
  },

  applyMeterSnapshot(values) {
    if (!values || typeof values !== "object") {
      return;
    }

    this.meterFills.forEach((fill, id) => {
      if (!Object.prototype.hasOwnProperty.call(values, id)) {
        return;
      }

      const raw = Number.parseFloat(values[id]);
      if (!Number.isFinite(raw)) {
        return;
      }

      const clamped = Math.max(0.0, Math.min(1.0, raw));
      fill.style.transform = "scaleX(" + clamped.toFixed(4) + ")";
    });
  },

  async handleControlChanged(id) {
    if (this.isSyncingFromHost) {
      return;
    }

    const value = this.readControlValue(id);
    this.updateDisplay(id, value);

    if (!JuceNative.hasBackend()) {
      return;
    }

    const result = await JuceNative.invoke("agentSetParameter", id, value);
    if (typeof result !== "undefined") {
      this.writeControlValue(id, result);
      this.updateDisplay(id, result);
    }
  },

  async syncFromHost() {
    if (!JuceNative.hasBackend()) {
      return;
    }

    const values = await JuceNative.invoke("agentGetAllParameters");
    this.applySnapshot(values);
  },

  async syncMetersFromHost() {
    if (!JuceNative.hasBackend()) {
      return;
    }

    const values = await JuceNative.invoke("agentGetMeters");
    this.applyMeterSnapshot(values);
  },

  destroy() {
    if (this.pollTimer !== null) {
      window.clearInterval(this.pollTimer);
      this.pollTimer = null;
    }

    if (this.backendEventTokens.length > 0 &&
        JuceNative.hasBackend() &&
        typeof window.__JUCE__.backend.removeEventListener === "function") {
      this.backendEventTokens.forEach((token) => {
        window.__JUCE__.backend.removeEventListener(token);
      });
      this.backendEventTokens = [];
    }
  }
};

document.addEventListener("DOMContentLoaded", () => AgentVST.init());
window.addEventListener("beforeunload", () => AgentVST.destroy());
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

std::string UIGenerator::escapeJs(const std::string& str) const {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;       break;
        }
    }
    return out;
}

} // namespace AgentVST
