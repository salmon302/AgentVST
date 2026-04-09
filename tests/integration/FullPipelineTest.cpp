/**
 * FullPipelineTest.cpp
 *
 * Integration tests exercising the full pipeline:
 *   JSON schema → SchemaParser → DSPRouter → audio processing → verified output
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "SchemaParser.h"
#include "DSPRouter.h"
#include "BiquadFilter.h"
#include "Gain.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using AgentVST::SchemaParser;
using AgentVST::PluginSchema;
using AgentVST::DSPRouter;
using AgentVST::DSPRoute;
using AgentVST::BiquadFilter;
using AgentVST::Gain;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static PluginSchema parseSchema(const std::string& json)
{
    SchemaParser parser;
    return parser.parseString(json);
}

static DSPRoute makeRoute(const std::string& src, const std::string& dst)
{
    DSPRoute r;
    r.source      = src;
    r.destination = dst;
    return r;
}

// 3-band EQ schema used by several tests
static const std::string kEqSchemaJson = R"({
    "plugin": {
        "name": "3BandEQ",
        "version": "1.0.0",
        "vendor": "AgentVST",
        "category": "EQ"
    },
    "parameters": [
        {"id": "low_gain",  "name": "Low Gain",    "type": "float", "min": -12.0, "max": 12.0, "default": 0.0, "unit": "dB", "group": "low"},
        {"id": "low_freq",  "name": "Low Freq",    "type": "float", "min": 20.0,  "max": 500.0, "default": 100.0, "unit": "Hz", "group": "low"},
        {"id": "mid_gain",  "name": "Mid Gain",    "type": "float", "min": -12.0, "max": 12.0, "default": 0.0, "unit": "dB", "group": "mid"},
        {"id": "mid_freq",  "name": "Mid Freq",    "type": "float", "min": 200.0, "max": 5000.0, "default": 1000.0, "unit": "Hz", "group": "mid"},
        {"id": "mid_q",     "name": "Mid Q",       "type": "float", "min": 0.1,   "max": 10.0, "default": 0.707, "unit": "", "group": "mid"},
        {"id": "high_gain", "name": "High Gain",   "type": "float", "min": -12.0, "max": 12.0, "default": 0.0, "unit": "dB", "group": "high"}
    ],
    "groups": [
        {"id": "low",  "name": "Low Band",  "parameters": ["low_gain", "low_freq"]},
        {"id": "mid",  "name": "Mid Band",  "parameters": ["mid_gain", "mid_freq", "mid_q"]},
        {"id": "high", "name": "High Band", "parameters": ["high_gain"]}
    ]
})";

// Schema with dsp_routing used by routing parse test
static const std::string kRoutingSchemaJson = R"({
    "parameters": [
        {"id": "cutoff_freq", "name": "Cutoff", "type": "float", "min": 20.0, "max": 20000.0, "default": 1000.0, "unit": "Hz"},
        {"id": "resonance",   "name": "Q",      "type": "float", "min": 0.1,  "max": 10.0,   "default": 0.707,  "unit": ""}
    ],
    "dsp_routing": [
        {
            "source": "input",
            "destination": "lpf",
            "parameter_links": {
                "cutoff_freq": "frequency",
                "resonance":   "q"
            }
        },
        {
            "source": "lpf",
            "destination": "output"
        }
    ]
})";

// ─────────────────────────────────────────────────────────────────────────────
// Schema parsing tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("FullPipeline: 3-band EQ schema parses all 6 parameters", "[integration][schema][eq]")
{
    const auto schema = parseSchema(kEqSchemaJson);

    REQUIRE(schema.parameters.size() == 6);
    CHECK(schema.metadata.name == "3BandEQ");

    // Verify all 6 IDs are present via findParameter
    CHECK(schema.findParameter("low_gain")  != nullptr);
    CHECK(schema.findParameter("low_freq")  != nullptr);
    CHECK(schema.findParameter("mid_gain")  != nullptr);
    CHECK(schema.findParameter("mid_freq")  != nullptr);
    CHECK(schema.findParameter("mid_q")     != nullptr);
    CHECK(schema.findParameter("high_gain") != nullptr);

    // Spot-check a few field values
    const auto* midFreq = schema.findParameter("mid_freq");
    REQUIRE(midFreq != nullptr);
    CHECK_THAT(midFreq->minValue,     Catch::Matchers::WithinAbs(200.0f, 0.01f));
    CHECK_THAT(midFreq->maxValue,     Catch::Matchers::WithinAbs(5000.0f, 0.01f));
    CHECK_THAT(midFreq->defaultValue, Catch::Matchers::WithinAbs(1000.0f, 0.01f));
    CHECK(midFreq->unit == "Hz");

    const auto* highGain = schema.findParameter("high_gain");
    REQUIRE(highGain != nullptr);
    CHECK_THAT(highGain->minValue, Catch::Matchers::WithinAbs(-12.0f, 0.01f));
    CHECK_THAT(highGain->maxValue, Catch::Matchers::WithinAbs(12.0f, 0.01f));
}

TEST_CASE("FullPipeline: schema groups map parameters to correct group", "[integration][schema][groups]")
{
    const auto schema = parseSchema(kEqSchemaJson);

    REQUIRE(schema.groups.size() == 3);

    // Mid group must have exactly 3 parameters
    const auto midParams = schema.getParametersInGroup("mid");
    REQUIRE(midParams.size() == 3);

    // Verify IDs (order matches declaration order in group)
    CHECK(midParams[0]->id == "mid_gain");
    CHECK(midParams[1]->id == "mid_freq");
    CHECK(midParams[2]->id == "mid_q");

    // Low group has 2 parameters
    const auto lowParams = schema.getParametersInGroup("low");
    CHECK(lowParams.size() == 2);

    // High group has 1 parameter
    const auto highParams = schema.getParametersInGroup("high");
    CHECK(highParams.size() == 1);
    CHECK(highParams[0]->id == "high_gain");
}

TEST_CASE("FullPipeline: schema with dsp_routing parses routes and parameterLinks",
          "[integration][schema][routing]")
{
    const auto schema = parseSchema(kRoutingSchemaJson);

    REQUIRE(schema.dspRouting.size() == 2);

    // First route: input → lpf with two parameter links
    const auto& r0 = schema.dspRouting[0];
    CHECK(r0.source      == "input");
    CHECK(r0.destination == "lpf");
    REQUIRE(r0.parameterLinks.size() == 2);
    REQUIRE(r0.parameterLinks.count("cutoff_freq") == 1);
    CHECK(r0.parameterLinks.at("cutoff_freq") == "frequency");
    REQUIRE(r0.parameterLinks.count("resonance") == 1);
    CHECK(r0.parameterLinks.at("resonance") == "q");

    // Second route: lpf → output, no parameter links
    const auto& r1 = schema.dspRouting[1];
    CHECK(r1.source      == "lpf");
    CHECK(r1.destination == "output");
    CHECK(r1.parameterLinks.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// DSPRouter audio tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("FullPipeline: DSPRouter chains single Gain node (0.5x)", "[integration][dsp][gain]")
{
    DSPRouter router;

    auto gain = std::make_unique<Gain>();
    gain->setGainLinear(0.5f);
    router.addNode("gain", std::move(gain));

    std::vector<DSPRoute> routes{
        makeRoute("input", "gain"),
        makeRoute("gain",  "output")
    };
    REQUIRE_NOTHROW(router.setRouting(routes));
    router.prepare(44100.0, 512);

    // Process enough samples for SmoothedValue to fully ramp to target (20ms = ~882 samples)
    float out = 0.0f;
    for (int i = 0; i < 2048; ++i)
        out = router.processSample(0, 1.0f);

    CHECK_THAT(out, Catch::Matchers::WithinAbs(0.5f, 0.01f));
}

TEST_CASE("FullPipeline: DSPRouter LPF at 1000Hz passes DC, attenuates 10kHz",
          "[integration][dsp][filter]")
{
    DSPRouter router;

    auto lpf = std::make_unique<BiquadFilter>();
    lpf->setType(BiquadFilter::Type::LowPass);
    lpf->setFrequency(1000.0f);
    lpf->setQ(0.707f);
    router.addNode("lpf", std::move(lpf));

    std::vector<DSPRoute> routes{
        makeRoute("input", "lpf"),
        makeRoute("lpf",   "output")
    };
    REQUIRE_NOTHROW(router.setRouting(routes));
    router.prepare(44100.0, 512);

    // ── Test 1: DC signal should pass through close to 1.0 ────────────────
    // Warm up the filter with DC
    float dcOut = 0.0f;
    for (int i = 0; i < 2048; ++i)
        dcOut = router.processSample(0, 1.0f);

    // After the filter has settled, DC output should be ~1.0
    CHECK_THAT(dcOut, Catch::Matchers::WithinAbs(1.0f, 0.05f));

    // ── Test 2: 10 kHz tone should be strongly attenuated ─────────────────
    router.reset();

    const double sampleRate = 44100.0;
    const double freq10k    = 10000.0;
    const double twoPi      = 6.28318530717958647692;

    // Warm-up period
    for (int i = 0; i < 1024; ++i)
    {
        const float sample = static_cast<float>(std::sin(twoPi * freq10k * i / sampleRate));
        router.processSample(0, sample);
    }

    // Measure peak amplitude over one full period at 10kHz
    float peak = 0.0f;
    const int periodSamples = static_cast<int>(sampleRate / freq10k) + 2;
    for (int i = 1024; i < 1024 + periodSamples; ++i)
    {
        const float sample = static_cast<float>(std::sin(twoPi * freq10k * i / sampleRate));
        const float out    = router.processSample(0, sample);
        peak = std::max(peak, std::abs(out));
    }

    // A 1st-order LPF at 1kHz cuts 10kHz by at least ~20 dB; biquad cuts more.
    // We require peak < 0.25 (–12 dB) as a conservative lower bound.
    CHECK(peak < 0.25f);
}

TEST_CASE("FullPipeline: DSPRouter two-node chain (0.5 x 0.5 = 0.25) after SmoothedValue settle",
          "[integration][dsp][chain]")
{
    DSPRouter router;

    auto gain1 = std::make_unique<Gain>();
    gain1->setGainLinear(0.5f);
    auto gain2 = std::make_unique<Gain>();
    gain2->setGainLinear(0.5f);

    router.addNode("gain1", std::move(gain1));
    router.addNode("gain2", std::move(gain2));

    std::vector<DSPRoute> routes{
        makeRoute("input",  "gain1"),
        makeRoute("gain1",  "gain2"),
        makeRoute("gain2",  "output")
    };
    REQUIRE_NOTHROW(router.setRouting(routes));
    router.prepare(44100.0, 512);

    // Process 4096 samples so both SmoothedValues fully ramp to their targets
    float out = 0.0f;
    for (int i = 0; i < 4096; ++i)
        out = router.processSample(0, 1.0f);

    CHECK_THAT(out, Catch::Matchers::WithinAbs(0.25f, 0.01f));
}

TEST_CASE("FullPipeline: after reset(), feeding silence into filter returns silence",
          "[integration][dsp][reset]")
{
    DSPRouter router;

    auto lpf = std::make_unique<BiquadFilter>();
    lpf->setType(BiquadFilter::Type::LowPass);
    lpf->setFrequency(500.0f);
    lpf->setQ(0.707f);
    router.addNode("lpf", std::move(lpf));

    std::vector<DSPRoute> routes{
        makeRoute("input", "lpf"),
        makeRoute("lpf",   "output")
    };
    REQUIRE_NOTHROW(router.setRouting(routes));
    router.prepare(44100.0, 512);

    // Build up state in the filter
    for (int i = 0; i < 512; ++i)
        router.processSample(0, 1.0f);

    // Reset clears all internal state
    router.reset();

    // Feeding zeros after a reset must return zero (no residual energy)
    for (int i = 0; i < 16; ++i)
    {
        const float out = router.processSample(0, 0.0f);
        CHECK_THAT(out, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }
}
