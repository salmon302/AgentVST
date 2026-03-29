/**
 * SchemaParser.h — AgentVST Plugin Schema Parser
 *
 * Parses the agent's plugin.json file into a PluginSchema struct.
 * The framework uses this schema to:
 *   - Build the JUCE APVTS ParameterLayout
 *   - Generate the default UI
 *   - Configure DSP routing
 */
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <optional>

namespace AgentVST {

// ─────────────────────────────────────────────────────────────────────────────
// ParameterDef — describes a single plugin parameter
// ─────────────────────────────────────────────────────────────────────────────
struct ParameterDef {
    std::string id;           ///< Machine ID used in code (e.g. "cutoff_freq")
    std::string name;         ///< Human-readable label  (e.g. "Cutoff Frequency")
    std::string type;         ///< "float" | "int" | "boolean" | "enum"
    std::string unit;         ///< Display unit: "Hz", "dB", "ms", "%" etc.
    std::string group;        ///< Optional group ID this parameter belongs to

    float minValue    = 0.0f;
    float maxValue    = 1.0f;
    float defaultValue= 0.5f;
    float step        = 0.0f; ///< 0 = continuous; >0 = discrete steps
    float skew        = 1.0f; ///< NormalisableRange skew (1.0 = linear)

    std::vector<std::string> enumOptions; ///< Non-empty for type=="enum"
};

// ─────────────────────────────────────────────────────────────────────────────
// ParameterGroup — logical grouping of parameters for UI layout
// ─────────────────────────────────────────────────────────────────────────────
struct ParameterGroup {
    std::string              id;
    std::string              name;
    std::vector<std::string> parameterIds;
};

// ─────────────────────────────────────────────────────────────────────────────
// DSPRoute — one edge in the DSP signal routing graph
// ─────────────────────────────────────────────────────────────────────────────
struct DSPRoute {
    std::string source;       ///< "input" or a module ID
    std::string destination;  ///< a module ID or "output"

    /// Maps schema parameter ID → module parameter name
    /// e.g. { "cutoff_freq": "frequency" }
    std::unordered_map<std::string, std::string> parameterLinks;
};

// ─────────────────────────────────────────────────────────────────────────────
// PluginMetadata — from the "plugin" block in plugin.json
// ─────────────────────────────────────────────────────────────────────────────
struct PluginMetadata {
    std::string name     = "AgentPlugin";
    std::string version  = "1.0.0";
    std::string vendor   = "AgentVST";
    std::string category = "Fx";
};

// ─────────────────────────────────────────────────────────────────────────────
// PluginSchema — complete parsed representation of plugin.json
// ─────────────────────────────────────────────────────────────────────────────
struct PluginSchema {
    PluginMetadata               metadata;
    std::vector<ParameterDef>    parameters;
    std::vector<ParameterGroup>  groups;
    std::vector<DSPRoute>        dspRouting;

    /// Returns pointer to a parameter definition, or nullptr if not found.
    const ParameterDef* findParameter(const std::string& id) const noexcept;

    /// Returns all parameters that belong to a given group ID.
    std::vector<const ParameterDef*> getParametersInGroup(
        const std::string& groupId) const;

    /// Returns parameters NOT assigned to any group.
    std::vector<const ParameterDef*> getUngroupedParameters() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// SchemaParser
// ─────────────────────────────────────────────────────────────────────────────
class SchemaParser {
public:
    SchemaParser() = default;

    /// Parse plugin.json from a file path.
    /// @throws SchemaParser::ParseError with a human-readable message on failure.
    PluginSchema parseFile(const std::string& filePath) const;

    /// Parse plugin.json from an in-memory JSON string.
    /// @throws SchemaParser::ParseError on failure.
    PluginSchema parseString(const std::string& jsonStr) const;

    // ── Errors ───────────────────────────────────────────────────────────────
    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& msg)
            : std::runtime_error(msg) {}
    };

    class ValidationError : public std::runtime_error {
    public:
        explicit ValidationError(const std::string& msg)
            : std::runtime_error(msg) {}
    };

private:
    PluginSchema parseJson(const std::string& jsonStr,
                           const std::string& sourceHint) const;
    void         validateSchema(const PluginSchema& schema,
                                const std::string& sourceHint) const;
    ParameterDef parseParameter(const void* jsonObj,
                                const std::string& sourceHint) const;
};

} // namespace AgentVST
