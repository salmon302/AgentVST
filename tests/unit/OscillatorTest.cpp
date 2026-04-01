#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "Oscillator.h"

using AgentVST::Oscillator;

TEST_CASE("Oscillator: sine output is bounded", "[dsp][oscillator]") {
    Oscillator osc;
    osc.prepare(48000.0, 256);
    osc.setWaveform(Oscillator::Waveform::Sine);
    osc.setFrequency(440.0f);
    osc.setLevel(1.0f);

    float maxAbs = 0.0f;
    for (int i = 0; i < 4800; ++i) {
        const float y = osc.processSample(0, 0.0f);
        REQUIRE(std::isfinite(y));
        maxAbs = std::max(maxAbs, std::abs(y));
    }

    CHECK(maxAbs <= 1.05f);
    CHECK(maxAbs >= 0.8f);
}

TEST_CASE("Oscillator: additive behavior keeps input when level is zero", "[dsp][oscillator]") {
    Oscillator osc;
    osc.prepare(48000.0, 256);
    osc.setWaveform(Oscillator::Waveform::Sawtooth);
    osc.setFrequency(1000.0f);
    osc.setLevel(0.0f);

    const float in = 0.37f;
    const float out = osc.processSample(0, in);
    CHECK(out > (in - 0.0001f));
    CHECK(out < (in + 0.0001f));
}

