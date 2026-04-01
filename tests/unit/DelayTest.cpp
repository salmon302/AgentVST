#include <catch2/catch_test_macros.hpp>
#include "Delay.h"

using AgentVST::Delay;

TEST_CASE("Delay: impulse appears at configured delay time", "[dsp][delay]") {
    Delay d;
    d.prepare(1000.0, 64);   // 1 sample = 1 ms
    d.setDelayMs(1.0f);
    d.setFeedback(0.0f);
    d.setMix(1.0f);

    float s0 = 0.0f;
    float s1 = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float in = (i == 0) ? 1.0f : 0.0f;
        const float out = d.processSample(0, in);
        if (i == 0) s0 = out;
        if (i == 1) s1 = out;
    }

    CHECK(s0 > -0.001f);
    CHECK(s0 < 0.001f);
    CHECK(s1 > 0.999f);
    CHECK(s1 < 1.001f);
}

TEST_CASE("Delay: feedback repeats are attenuated", "[dsp][delay]") {
    Delay d;
    d.prepare(1000.0, 64);
    d.setDelayMs(1.0f);
    d.setFeedback(0.5f);
    d.setMix(1.0f);

    float s1 = 0.0f, s2 = 0.0f;
    for (int i = 0; i < 4; ++i) {
        const float in = (i == 0) ? 1.0f : 0.0f;
        const float out = d.processSample(0, in);
        if (i == 1) s1 = out;
        if (i == 2) s2 = out;
    }

    CHECK(s1 > 0.999f);
    CHECK(s1 < 1.001f);
    CHECK(s2 > 0.49f);
    CHECK(s2 < 0.51f);
}

