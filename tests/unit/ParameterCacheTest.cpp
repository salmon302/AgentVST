/**
 * ParameterCacheTest.cpp
 * Tests for the lock-free ParameterCache.
 *
 * IMPORTANT: APVTS stores parameters as normalized values (0.0–1.0).
 * The ParameterCache denormalizes them to real-world ranges on read.
 * Tests simulate this by storing normalized values into atomics and
 * registering the real-world min/max/skew ranges.
 */
#include <catch2/catch_test_macros.hpp>
#include "ParameterCache.h"
#include <atomic>
#include <array>
#include <cmath>

using AgentVST::ParameterCache;

TEST_CASE("ParameterCache: register and read normalized value (linear)", "[cache]") {
    // Simulate a gain parameter: -12 dB to +12 dB, default 0 dB (normalized 0.5)
    ParameterCache cache;
    std::atomic<float> val{ 0.5f }; // normalized midpoint

    cache.registerParameter("gain", &val, -12.0f, 12.0f, 0.0f, false);
    cache.finalize();

    // 0.5 normalized → 0.0 dB (midpoint of -12 to +12)
    const float v = cache.getValue("gain");
    CHECK(v > -0.001f);
    CHECK(v < 0.001f);
}

TEST_CASE("ParameterCache: reads updated normalized value", "[cache]") {
    // Simulate a frequency parameter: 20 Hz to 20000 Hz
    ParameterCache cache;
    std::atomic<float> val{ 0.0f }; // normalized minimum

    cache.registerParameter("freq", &val, 20.0f, 20000.0f, 0.4f, false);
    cache.finalize();

    CHECK(cache.getValue("freq") > 19.9f);
    CHECK(cache.getValue("freq") < 20.1f);

    // Store normalized 0.5 → should denormalize to ~midpoint
    val.store(0.5f, std::memory_order_relaxed);
    const float v = cache.getValue("freq");
    // With skew 0.4: 20 + (20000-20) * pow(0.5, 0.4) ≈ 20 + 19980 * 0.7579 ≈ 15153
    CHECK(v > 15000.0f);
    CHECK(v < 15300.0f);
}

TEST_CASE("ParameterCache: unknown id returns 0", "[cache]") {
    ParameterCache cache;
    cache.finalize();
    CHECK(cache.getValue("missing") == 0.0f);
}

TEST_CASE("ParameterCache: hasParameter returns correct result", "[cache]") {
    ParameterCache cache;
    std::atomic<float> v{ 1.0f };
    cache.registerParameter("x", &v, 0.0f, 100.0f);
    cache.finalize();

    CHECK(cache.hasParameter("x")   == true);
    CHECK(cache.hasParameter("y")   == false);
}

TEST_CASE("ParameterCache: multiple parameters with ranges", "[cache]") {
    ParameterCache cache;
    std::atomic<float> a{ 0.0f }, b{ 0.5f }, c{ 1.0f };
    cache.registerParameter("a", &a, -10.0f, 10.0f);   // 0.0 → -10.0
    cache.registerParameter("b", &b, 0.0f, 100.0f);     // 0.5 → 50.0
    cache.registerParameter("c", &c, 20.0f, 200.0f);    // 1.0 → 200.0
    cache.finalize();

    const float va = cache.getValue("a");
    CHECK(va > -10.001f);
    CHECK(va < -9.999f);

    const float vb = cache.getValue("b");
    CHECK(vb > 49.999f);
    CHECK(vb < 50.001f);

    const float vc = cache.getValue("c");
    CHECK(vc > 199.999f);
    CHECK(vc < 200.001f);

    CHECK(cache.size() == 3);
}

TEST_CASE("ParameterCache: boolean parameter", "[cache]") {
    ParameterCache cache;
    std::atomic<float> bypass{ 0.0f };
    cache.registerParameter("bypass", &bypass, 0.0f, 1.0f, 0.0f, true);
    cache.finalize();

    CHECK(cache.getValue("bypass") == 0.0f);

    bypass.store(0.5f, std::memory_order_relaxed);
    CHECK(cache.getValue("bypass") == 1.0f);

    bypass.store(1.0f, std::memory_order_relaxed);
    CHECK(cache.getValue("bypass") == 1.0f);
}

TEST_CASE("ParameterCache: duplicate registration throws", "[cache]") {
    ParameterCache cache;
    std::atomic<float> v{ 0.0f };
    cache.registerParameter("x", &v);
    CHECK_THROWS(cache.registerParameter("x", &v));
}

TEST_CASE("ParameterCache: tryGetIndex resolves known ids", "[cache]") {
    ParameterCache cache;
    std::atomic<float> a{ 0.5f }, b{ 0.5f };
    cache.registerParameter("a", &a, 0.0f, 10.0f);
    cache.registerParameter("b", &b, 0.0f, 20.0f);
    cache.finalize();

    std::size_t idxA = 999;
    std::size_t idxB = 999;
    std::size_t idxM = 999;

    CHECK(cache.tryGetIndex("a", idxA));
    CHECK(cache.tryGetIndex("b", idxB));
    CHECK_FALSE(cache.tryGetIndex("missing", idxM));
    CHECK(idxA != idxB);
}

TEST_CASE("ParameterCache: copyValuesTo copies denormalized snapshot", "[cache]") {
    // Simulate: gain (-12 to +12 dB) and mix (0 to 100%)
    ParameterCache cache;
    std::atomic<float> gain{ 0.25f };  // normalized → -6.0 dB
    std::atomic<float> mix{ 0.75f };   // normalized → 75.0%
    cache.registerParameter("gain", &gain, -12.0f, 12.0f);
    cache.registerParameter("mix", &mix, 0.0f, 100.0f);
    cache.finalize();

    std::array<float, 2> snapshot{};
    cache.copyValuesTo(snapshot.data(), snapshot.size());

    std::size_t idxGain = 0;
    std::size_t idxMix = 0;
    REQUIRE(cache.tryGetIndex("gain", idxGain));
    REQUIRE(cache.tryGetIndex("mix", idxMix));

    // 0.25 normalized of [-12, 12] → -12 + 0.25 * 24 = -6.0
    CHECK(snapshot[idxGain] > -6.001f);
    CHECK(snapshot[idxGain] < -5.999f);
    // 0.75 normalized of [0, 100] → 75.0
    CHECK(snapshot[idxMix] > 74.999f);
    CHECK(snapshot[idxMix] < 75.001f);

    // Update normalized values
    gain.store(0.5f, std::memory_order_relaxed);  // → 0.0 dB
    mix.store(0.0f, std::memory_order_relaxed);   // → 0.0%
    cache.copyValuesTo(snapshot.data(), snapshot.size());

    CHECK(snapshot[idxGain] > -0.001f);
    CHECK(snapshot[idxGain] < 0.001f);
    CHECK(snapshot[idxMix] > -0.001f);
    CHECK(snapshot[idxMix] < 0.001f);
}

TEST_CASE("ParameterCache: skewed parameter denormalization", "[cache]") {
    // Simulate a frequency with skew (like AgentEQ's low_freq_hz: 20-500 Hz, skew 0.4)
    ParameterCache cache;
    std::atomic<float> val{ 0.0f };
    cache.registerParameter("freq", &val, 20.0f, 500.0f, 0.4f, false);
    cache.finalize();

    // At normalized 0.0 → 20.0 Hz
    CHECK(cache.getValue("freq") > 19.9f);
    CHECK(cache.getValue("freq") < 20.1f);

    // At normalized 1.0 → 500.0 Hz
    val.store(1.0f, std::memory_order_relaxed);
    CHECK(cache.getValue("freq") > 499.9f);
    CHECK(cache.getValue("freq") < 500.1f);

    // At normalized 0.5 → 20 + 480 * pow(0.5, 0.4) ≈ 20 + 480 * 0.7579 ≈ 383.8
    val.store(0.5f, std::memory_order_relaxed);
    const float v = cache.getValue("freq");
    CHECK(v > 380.0f);
    CHECK(v < 390.0f);
}

