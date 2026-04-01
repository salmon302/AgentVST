/**
 * SchemaParserTest.cpp
 *
 * Unit tests for the JSON schema parser.
 * Verifies correct parsing, field defaults, error messages, and edge cases.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "SchemaParser.h"

using AgentVST::SchemaParser;
using AgentVST::PluginSchema;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static PluginSchema parse(const std::string& json) {
    SchemaParser p;
    return p.parseString(json);
}

static void expectError(const std::string& json, const std::string& fragment) {
    SchemaParser p;
    try {
        p.parseString(json);
        FAIL("Expected ParseError or ValidationError was not thrown");
    } catch (const SchemaParser::ParseError& e) {
        REQUIRE_THAT(std::string(e.what()),
                     Catch::Matchers::ContainsSubstring(fragment));
    } catch (const SchemaParser::ValidationError& e) {
        REQUIRE_THAT(std::string(e.what()),
                     Catch::Matchers::ContainsSubstring(fragment));
    }
}

// -----------------------------------------------------------------------------
// Metadata
// -----------------------------------------------------------------------------
TEST_CASE("SchemaParser: metadata is parsed correctly", "[schema][metadata]") {
    auto schema = parse(R"({
        "plugin": {
            "name": "TestPlugin",
            "version": "2.1.0",
            "vendor": "AcmeDSP",
            "category": "Filter"
        },
        "parameters": []
    })");

    CHECK(schema.metadata.name     == "TestPlugin");
    CHECK(schema.metadata.version  == "2.1.0");
    CHECK(schema.metadata.vendor   == "AcmeDSP");
    CHECK(schema.metadata.category == "Filter");
}

TEST_CASE("SchemaParser: missing plugin block uses defaults", "[schema][metadata]") {
    auto schema = parse(R"({"parameters": []})");
    CHECK(schema.metadata.name    == "AgentPlugin");
    CHECK(schema.metadata.vendor  == "AgentVST");
    CHECK(schema.metadata.version == "1.0.0");
}

// -----------------------------------------------------------------------------
// Float parameters
// -----------------------------------------------------------------------------
TEST_CASE("SchemaParser: float parameter parsed correctly", "[schema][parameter]") {
    auto schema = parse(R"({
        "parameters": [{
            "id": "cutoff",
            "name": "Cutoff Frequency",
            "type": "float",
            "min": 20.0,
            "max": 20000.0,
            "default": 1000.0,
            "unit": "Hz",
            "skew": 0.3,
            "step": 1.0
        }]
    })");

    REQUIRE(schema.parameters.size() == 1);
    const auto& p = schema.parameters[0];
    CHECK(p.id           == "cutoff");
    CHECK(p.name         == "Cutoff Frequency");
    CHECK(p.type         == "float");
    CHECK(p.minValue     == 20.0f);
    CHECK(p.maxValue     == 20000.0f);
    CHECK(p.defaultValue == 1000.0f);
    CHECK(p.unit         == "Hz");
    CHECK(p.skew > 0.2999f);
    CHECK(p.skew < 0.3001f);
    CHECK(p.step         == 1.0f);
}

// -----------------------------------------------------------------------------
// Boolean parameters
// -----------------------------------------------------------------------------
TEST_CASE("SchemaParser: boolean parameter parsed correctly", "[schema][parameter]") {
    auto schema = parse(R"({
        "parameters": [{
            "id": "bypass",
            "name": "Bypass",
            "type": "boolean",
            "default": true
        }]
    })");

    REQUIRE(schema.parameters.size() == 1);
    const auto& p = schema.parameters[0];
    CHECK(p.type         == "boolean");
    CHECK(p.defaultValue >= 0.5f);  // true maps to 1.0
    CHECK(p.minValue     == 0.0f);
    CHECK(p.maxValue     == 1.0f);
    CHECK(p.step         == 1.0f);
}

// -----------------------------------------------------------------------------
// Enum parameters
// -----------------------------------------------------------------------------
TEST_CASE("SchemaParser: enum parameter parsed correctly", "[schema][parameter]") {
    auto schema = parse(R"({
        "parameters": [{
            "id": "filter_type",
            "name": "Filter Type",
            "type": "enum",
            "options": ["Low-Pass", "High-Pass", "Band-Pass"],
            "default": 1
        }]
    })");

    REQUIRE(schema.parameters.size() == 1);
    const auto& p = schema.parameters[0];
    CHECK(p.type == "enum");
    REQUIRE(p.enumOptions.size() == 3);
    CHECK(p.enumOptions[0] == "Low-Pass");
    CHECK(p.enumOptions[1] == "High-Pass");
    CHECK(p.enumOptions[2] == "Band-Pass");
    CHECK(p.defaultValue > 0.9999f);
    CHECK(p.defaultValue < 1.0001f);
    CHECK(p.maxValue > 1.9999f);
    CHECK(p.maxValue < 2.0001f);
}

// -----------------------------------------------------------------------------
// Groups
// -----------------------------------------------------------------------------
TEST_CASE("SchemaParser: groups parsed correctly", "[schema][groups]") {
    auto schema = parse(R"({
        "parameters": [
            {"id": "gain",    "name": "Gain",    "type": "float", "min": 0, "max": 1, "default": 0.5},
            {"id": "bypass",  "name": "Bypass",  "type": "boolean"}
        ],
        "groups": [
            {"id": "main", "name": "Main Controls", "parameters": ["gain", "bypass"]}
        ]
    })");

    REQUIRE(schema.groups.size() == 1);
    CHECK(schema.groups[0].id   == "main");
    CHECK(schema.groups[0].name == "Main Controls");
    REQUIRE(schema.groups[0].parameterIds.size() == 2);
    CHECK(schema.groups[0].parameterIds[0] == "gain");
    CHECK(schema.groups[0].parameterIds[1] == "bypass");
}

// -----------------------------------------------------------------------------
// Error cases - these should all throw with meaningful messages
// -----------------------------------------------------------------------------
TEST_CASE("SchemaParser: invalid JSON throws ParseError", "[schema][errors]") {
    expectError("{ not valid json }", "parse error");
}

TEST_CASE("SchemaParser: parameter missing 'id' throws ParseError", "[schema][errors]") {
    expectError(R"({"parameters": [{"name": "Gain", "type": "float", "min": 0, "max": 1}]})",
                "id");
}

TEST_CASE("SchemaParser: float parameter missing 'min' throws ParseError", "[schema][errors]") {
    expectError(R"({"parameters": [{"id": "g", "name": "G", "type": "float", "max": 1}]})",
                "min");
}

TEST_CASE("SchemaParser: min >= max throws ParseError", "[schema][errors]") {
    expectError(R"({"parameters": [{"id": "g", "name": "G", "type": "float", "min": 5, "max": 1, "default": 1}]})",
                "min");
}

TEST_CASE("SchemaParser: default out of range throws ParseError", "[schema][errors]") {
    expectError(R"({"parameters": [{"id": "g", "name": "G", "type": "float", "min": 0, "max": 1, "default": 2}]})",
                "default");
}

TEST_CASE("SchemaParser: duplicate parameter IDs throw ValidationError", "[schema][errors]") {
    expectError(R"({
        "parameters": [
            {"id": "gain", "name": "Gain 1", "type": "float", "min": 0, "max": 1, "default": 0.5},
            {"id": "gain", "name": "Gain 2", "type": "float", "min": 0, "max": 1, "default": 0.5}
        ]
    })", "Duplicate");
}

TEST_CASE("SchemaParser: group references unknown parameter throws ValidationError",
          "[schema][errors]") {
    expectError(R"({
        "parameters": [{"id": "gain", "name": "G", "type": "float", "min": 0, "max": 1, "default": 0.5}],
        "groups": [{"id": "g1", "name": "G1", "parameters": ["gain", "nonexistent"]}]
    })", "nonexistent");
}

TEST_CASE("SchemaParser: enum missing options throws ParseError", "[schema][errors]") {
    expectError(R"({"parameters": [{"id": "t", "name": "T", "type": "enum"}]})",
                "options");
}

// -----------------------------------------------------------------------------
// findParameter
// -----------------------------------------------------------------------------
TEST_CASE("PluginSchema::findParameter returns correct pointer", "[schema][lookup]") {
    auto schema = parse(R"({
        "parameters": [
            {"id": "alpha", "name": "Alpha", "type": "float", "min": 0, "max": 1, "default": 0.5},
            {"id": "beta",  "name": "Beta",  "type": "float", "min": 0, "max": 2, "default": 1.0}
        ]
    })");

    CHECK(schema.findParameter("alpha") != nullptr);
    CHECK(schema.findParameter("beta")  != nullptr);
    CHECK(schema.findParameter("gamma") == nullptr);
    CHECK(schema.findParameter("alpha")->maxValue == 1.0f);
    CHECK(schema.findParameter("beta")->maxValue  == 2.0f);
}

