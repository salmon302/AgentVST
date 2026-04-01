#include <catch2/catch_test_macros.hpp>
#include "BiquadFilter.h"

using AgentVST::BiquadFilter;

TEST_CASE("BiquadFilter: low-pass passes DC", "[dsp][biquad]") {
    BiquadFilter f;
    f.prepare(48000.0, 512);
    f.setType(BiquadFilter::Type::LowPass);
    f.setFrequency(1200.0f);
    f.setQ(0.707f);

    float y = 0.0f;
    for (int i = 0; i < 4096; ++i)
        y = f.processSample(0, 1.0f);

    CHECK(y > 0.90f);
}

TEST_CASE("BiquadFilter: high-pass rejects DC", "[dsp][biquad]") {
    BiquadFilter f;
    f.prepare(48000.0, 512);
    f.setType(BiquadFilter::Type::HighPass);
    f.setFrequency(1200.0f);
    f.setQ(0.707f);

    float y = 0.0f;
    for (int i = 0; i < 4096; ++i)
        y = f.processSample(0, 1.0f);

    CHECK(y > -0.05f);
    CHECK(y < 0.05f);
}

