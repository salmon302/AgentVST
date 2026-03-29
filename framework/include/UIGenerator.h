/**
 * UIGenerator.h
 *
 * Generates a default HTML/CSS/JS user interface from the PluginSchema.
 * The generated HTML is served by the WebBrowserComponent in AgentVSTEditor.
 *
 * Phase 0/1: Outputs a functional but plain HTML page with labeled sliders,
 *            toggles, and dropdowns for all parameters.
 * Phase 3:   Upgraded to use JUCE 8 WebView bridge with live parameter binding.
 */
#pragma once

#include "SchemaParser.h"
#include <string>

namespace AgentVST {

class UIGenerator {
public:
    UIGenerator() = default;

    /**
     * Generate a complete HTML page for the given schema.
     * The page includes embedded CSS and JavaScript stubs.
     *
     * @param schema   The parsed plugin schema
     * @param cssPath  Optional path to an external stylesheet (empty = inline defaults)
     * @return         Complete HTML string ready to serve in WebBrowserComponent
     */
    std::string generateHTML(const PluginSchema& schema,
                             const std::string& cssPath = "") const;

    /**
     * Generate just the CSS stylesheet.
     */
    std::string generateCSS() const;

    /**
     * Generate the JavaScript that wires the HTML controls to JUCE's
     * WebView parameter bridge (JUCE 8 API).
     */
    std::string generateJS(const PluginSchema& schema) const;

private:
    std::string generateParameterControl(const ParameterDef& param) const;
    std::string generateGroupSection(const ParameterGroup& group,
                                     const PluginSchema& schema) const;
    std::string escapeHtml(const std::string& str) const;
};

} // namespace AgentVST
