#include "SchemaParser.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace AgentVST {

// ─────────────────────────────────────────────────────────────────────────────
// PluginSchema helpers
// ─────────────────────────────────────────────────────────────────────────────

const ParameterDef* PluginSchema::findParameter(const std::string& id) const noexcept {
    for (const auto& p : parameters)
        if (p.id == id)
            return &p;
    return nullptr;
}

std::vector<const ParameterDef*>
PluginSchema::getParametersInGroup(const std::string& groupId) const {
    std::vector<const ParameterDef*> result;
    for (const auto& g : groups) {
        if (g.id == groupId) {
            for (const auto& pid : g.parameterIds) {
                const auto* p = findParameter(pid);
                if (p) result.push_back(p);
            }
            break;
        }
    }
    return result;
}

std::vector<const ParameterDef*> PluginSchema::getUngroupedParameters() const {
    // Collect all IDs that appear in at least one group
    std::unordered_map<std::string, bool> inGroup;
    for (const auto& g : groups)
        for (const auto& pid : g.parameterIds)
            inGroup[pid] = true;

    std::vector<const ParameterDef*> result;
    for (const auto& p : parameters)
        if (inGroup.find(p.id) == inGroup.end())
            result.push_back(&p);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// SchemaParser — public API
// ─────────────────────────────────────────────────────────────────────────────

PluginSchema SchemaParser::parseFile(const std::string& filePath) const {
    std::ifstream f(filePath);
    if (!f.is_open())
        throw ParseError("Cannot open schema file: " + filePath);

    std::ostringstream ss;
    ss << f.rdbuf();
    return parseJson(ss.str(), filePath);
}

PluginSchema SchemaParser::parseString(const std::string& jsonStr) const {
    return parseJson(jsonStr, "<string>");
}

// ─────────────────────────────────────────────────────────────────────────────
// SchemaParser — private implementation
// ─────────────────────────────────────────────────────────────────────────────

PluginSchema SchemaParser::parseJson(const std::string& jsonStr,
                                     const std::string& sourceHint) const {
    json j;
    try {
        j = json::parse(jsonStr);
    } catch (const json::parse_error& e) {
        throw ParseError("JSON parse error in " + sourceHint + ": " +
                         std::string(e.what()));
    }

    PluginSchema schema;

    // ── Metadata ─────────────────────────────────────────────────────────────
    if (j.contains("plugin") && j["plugin"].is_object()) {
        const auto& p = j["plugin"];
        schema.metadata.name     = p.value("name",     "AgentPlugin");
        schema.metadata.version  = p.value("version",  "1.0.0");
        schema.metadata.vendor   = p.value("vendor",   "AgentVST");
        schema.metadata.category = p.value("category", "Fx");
    }

    // ── Parameters ───────────────────────────────────────────────────────────
    if (j.contains("parameters") && j["parameters"].is_array()) {
        for (const auto& paramJson : j["parameters"]) {
            try {
                schema.parameters.push_back(
                    parseParameter(&paramJson, sourceHint));
            } catch (const ParseError& e) {
                throw ParseError(std::string(e.what()) +
                                 " (in file: " + sourceHint + ")");
            }
        }
    }

    // ── Groups ───────────────────────────────────────────────────────────────
    if (j.contains("groups") && j["groups"].is_array()) {
        for (const auto& g : j["groups"]) {
            ParameterGroup grp;
            grp.id   = g.value("id",   "");
            grp.name = g.value("name", grp.id);
            if (grp.id.empty())
                throw ParseError("Group missing required field 'id' in " + sourceHint);
            if (g.contains("parameters") && g["parameters"].is_array())
                for (const auto& pid : g["parameters"])
                    grp.parameterIds.push_back(pid.get<std::string>());
            schema.groups.push_back(std::move(grp));
        }
    }

    // ── DSP Routing ──────────────────────────────────────────────────────────
    if (j.contains("dsp_routing") && j["dsp_routing"].is_array()) {
        for (const auto& route : j["dsp_routing"]) {
            DSPRoute r;
            r.source      = route.value("source",      "");
            r.destination = route.value("destination", "");
            if (r.source.empty() || r.destination.empty())
                throw ParseError("DSP route missing 'source' or 'destination' in " + sourceHint);
            if (route.contains("parameter_link") && route["parameter_link"].is_object()) {
                for (const auto& [k, v] : route["parameter_link"].items())
                    r.parameterLinks[k] = v.get<std::string>();
            }
            schema.dspRouting.push_back(std::move(r));
        }
    }

    validateSchema(schema, sourceHint);
    return schema;
}

ParameterDef SchemaParser::parseParameter(const void* jsonObj,
                                           const std::string& sourceHint) const {
    (void)sourceHint;
    const auto& p = *static_cast<const json*>(jsonObj);

    if (!p.contains("id") || !p["id"].is_string())
        throw ParseError("Parameter missing required field 'id'");
    if (!p.contains("name") || !p["name"].is_string())
        throw ParseError("Parameter '" + p["id"].get<std::string>() + "' missing 'name'");

    ParameterDef def;
    def.id   = p["id"].get<std::string>();
    def.name = p["name"].get<std::string>();
    def.type = p.value("type", "float");
    def.unit = p.value("unit", "");
    def.group= p.value("group","");

    if (def.type == "boolean") {
        def.minValue     = 0.0f;
        def.maxValue     = 1.0f;
        def.defaultValue = p.value("default", false) ? 1.0f : 0.0f;
        def.step         = 1.0f;
    } else if (def.type == "enum") {
        if (!p.contains("options") || !p["options"].is_array() || p["options"].empty())
            throw ParseError("Enum parameter '" + def.id + "' missing 'options' array");
        for (const auto& opt : p["options"])
            def.enumOptions.push_back(opt.get<std::string>());
        def.minValue     = 0.0f;
        def.maxValue     = static_cast<float>(def.enumOptions.size() - 1);
        def.defaultValue = static_cast<float>(p.value("default", 0));
        def.step         = 1.0f;
    } else {
        // float or int
        if (!p.contains("min") || !p.contains("max"))
            throw ParseError("Parameter '" + def.id +
                             "' of type '" + def.type + "' requires 'min' and 'max'");
        def.minValue     = p["min"].get<float>();
        def.maxValue     = p["max"].get<float>();
        def.defaultValue = p.value("default", def.minValue);
        def.step         = p.value("step",    0.0f);
        def.skew         = p.value("skew",    1.0f);

        if (def.minValue >= def.maxValue)
            throw ParseError("Parameter '" + def.id +
                             "': 'min' must be less than 'max'");
        if (def.defaultValue < def.minValue || def.defaultValue > def.maxValue)
            throw ParseError("Parameter '" + def.id +
                             "': 'default' value is outside [min, max] range");
        if (def.skew <= 0.0f)
            throw ParseError("Parameter '" + def.id +
                             "': 'skew' must be > 0");
    }

    return def;
}

void SchemaParser::validateSchema(const PluginSchema& schema,
                                   const std::string& sourceHint) const {
    // Check for duplicate parameter IDs
    std::unordered_map<std::string, int> idCount;
    for (const auto& p : schema.parameters)
        idCount[p.id]++;
    for (const auto& [id, count] : idCount)
        if (count > 1)
            throw ValidationError("Duplicate parameter id '" + id + "' in " + sourceHint);

    // Check group references are valid
    for (const auto& g : schema.groups) {
        for (const auto& pid : g.parameterIds) {
            if (schema.findParameter(pid) == nullptr)
                throw ValidationError("Group '" + g.id + "' references unknown parameter '" +
                                     pid + "' in " + sourceHint);
        }
    }
}

} // namespace AgentVST
