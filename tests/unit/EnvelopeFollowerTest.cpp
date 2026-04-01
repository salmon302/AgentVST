#include <catch2/catch_test_macros.hpp>

#include "EnvelopeFollower.h"

using AgentVST::EnvelopeFollower;

TEST_CASE("EnvelopeFollower: peak mode rises and decays", "[dsp][envelope]") {
    EnvelopeFollower env;
    env.prepare(1000.0, 128);
    env.setAttackMs(1.0f);
    env.setReleaseMs(10.0f);
    env.setMode(EnvelopeFollower::Mode::Peak);

    float up = 0.0f;
    for (int i = 0; i < 20; ++i)
        up = env.processSample(0, 1.0f);

    float down = 0.0f;
    for (int i = 0; i < 20; ++i)
        down = env.processSample(0, 0.0f);

    CHECK(up > 0.8f);
    CHECK(down < up);
}

TEST_CASE("EnvelopeFollower: rms mode returns positive envelope", "[dsp][envelope]") {
    EnvelopeFollower env;
    env.prepare(48000.0, 256);
    env.setMode(EnvelopeFollower::Mode::RMS);
    env.setReleaseMs(5.0f);

    float last = 0.0f;
    for (int i = 0; i < 2048; ++i)
        last = env.processSample(0, (i % 2 == 0) ? 1.0f : -1.0f);

    CHECK(last > 0.1f);
    CHECK(last <= 1.0f);
}
