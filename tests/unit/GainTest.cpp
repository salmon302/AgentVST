#include <catch2/catch_test_macros.hpp>
#include "Gain.h"

using AgentVST::Gain;

TEST_CASE("Gain: unity gain passes input", "[dsp][gain]") {
    Gain g;
    g.prepare(48000.0, 256);
    g.setSmoothingMs(0.0f);
    g.setGainLinear(1.0f);

    const float out = g.processSample(0, 0.42f);
    CHECK(out > 0.419f);
    CHECK(out < 0.421f);
}

TEST_CASE("Gain: decibel conversion applies expected factor", "[dsp][gain]") {
    Gain g;
    g.prepare(48000.0, 256);
    g.setSmoothingMs(0.0f);
    g.setGainDb(-6.0f);

    const float out = g.processSample(0, 1.0f);
    CHECK(out > 0.491f);
    CHECK(out < 0.511f);
}

