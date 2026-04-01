#include <catch2/catch_test_macros.hpp>

#include "SchemaParser.h"
#include "UIGenerator.h"

using AgentVST::PluginSchema;
using AgentVST::SchemaParser;
using AgentVST::UIGenerator;

namespace {

PluginSchema parseSchema(const std::string& json) {
    SchemaParser parser;
    return parser.parseString(json);
}

} // namespace

TEST_CASE("UIGenerator: generated JS includes native bridge and snapshot hooks", "[ui][webview]") {
    auto schema = parseSchema(R"JSON({
        "plugin": {"name": "UiBridgeTest", "version": "1.0.0"},
        "parameters": [
            {"id": "gain", "name": "Gain", "type": "float", "min": -24.0, "max": 24.0, "default": 0.0},
            {"id": "mode", "name": "Mode", "type": "enum", "options": ["A", "B"], "default": 0}
        ]
    })JSON");

    UIGenerator generator;
    const auto js = generator.generateJS(schema);

    CHECK(js.find("agentSetParameter") != std::string::npos);
    CHECK(js.find("agentGetAllParameters") != std::string::npos);
    CHECK(js.find("agentGetMeters") != std::string::npos);
    CHECK(js.find("agentMeterSnapshot") != std::string::npos);
    CHECK(js.find("syncMetersFromHost") != std::string::npos);
    CHECK(js.find("AgentVSTParameterDefs") != std::string::npos);
    CHECK(js.find("document.addEventListener(\"DOMContentLoaded\"") != std::string::npos);
    CHECK(js.find("\"gain\": {") != std::string::npos);
    CHECK(js.find("\"mode\": {") != std::string::npos);
}

TEST_CASE("UIGenerator: generated HTML renders controls for parameter types", "[ui][html]") {
    auto schema = parseSchema(R"JSON({
        "plugin": {"name": "UiHtmlTest", "version": "1.0.0"},
        "parameters": [
            {"id": "cutoff", "name": "Cutoff", "type": "float", "min": 20.0, "max": 20000.0, "default": 1000.0, "unit": "Hz"},
            {"id": "bypass", "name": "Bypass", "type": "boolean", "default": false},
            {"id": "shape", "name": "Shape", "type": "enum", "options": ["Sine", "Square"], "default": 0}
        ]
    })JSON");

    UIGenerator generator;
    const auto html = generator.generateHTML(schema);

    CHECK(html.find("data-param-id=\"cutoff\"") != std::string::npos);
    CHECK(html.find("data-param-id=\"bypass\"") != std::string::npos);
    CHECK(html.find("data-param-id=\"shape\"") != std::string::npos);
    CHECK(html.find("<input type=\"range\"") != std::string::npos);
    CHECK(html.find("<input type=\"checkbox\"") != std::string::npos);
    CHECK(html.find("<select data-param-id=\"shape\"") != std::string::npos);
    CHECK(html.find("data-meter-id=\"inputPeakL\"") != std::string::npos);
    CHECK(html.find("data-meter-id=\"outputRmsR\"") != std::string::npos);
}
