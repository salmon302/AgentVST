/**
 * ParameterCacheTest.cpp
 * Tests for the lock-free ParameterCache.
 */
#include <catch2/catch_test_macros.hpp>
#include "ParameterCache.h"
#include <atomic>

using AgentVST::ParameterCache;

TEST_CASE("ParameterCache: register and read value", "[cache]") {
    ParameterCache cache;
    std::atomic<float> val{ 0.5f };

    cache.registerParameter("gain", &val);
    cache.finalize();

    CHECK(cache.getValue("gain") > 0.4999f);
    CHECK(cache.getValue("gain") < 0.5001f);
}

TEST_CASE("ParameterCache: reads updated atomic value", "[cache]") {
    ParameterCache cache;
    std::atomic<float> val{ 0.0f };

    cache.registerParameter("freq", &val);
    cache.finalize();

    CHECK(cache.getValue("freq") > -0.0001f);
    CHECK(cache.getValue("freq") < 0.0001f);
    val.store(440.0f, std::memory_order_relaxed);
    CHECK(cache.getValue("freq") > 439.999f);
    CHECK(cache.getValue("freq") < 440.001f);
}

TEST_CASE("ParameterCache: unknown id returns 0", "[cache]") {
    ParameterCache cache;
    cache.finalize();
    CHECK(cache.getValue("missing") == 0.0f);
}

TEST_CASE("ParameterCache: hasParameter returns correct result", "[cache]") {
    ParameterCache cache;
    std::atomic<float> v{ 1.0f };
    cache.registerParameter("x", &v);
    cache.finalize();

    CHECK(cache.hasParameter("x")   == true);
    CHECK(cache.hasParameter("y")   == false);
}

TEST_CASE("ParameterCache: multiple parameters", "[cache]") {
    ParameterCache cache;
    std::atomic<float> a{ 1.0f }, b{ 2.0f }, c{ 3.0f };
    cache.registerParameter("a", &a);
    cache.registerParameter("b", &b);
    cache.registerParameter("c", &c);
    cache.finalize();

    CHECK(cache.getValue("a") > 0.9999f);
    CHECK(cache.getValue("a") < 1.0001f);
    CHECK(cache.getValue("b") > 1.9999f);
    CHECK(cache.getValue("b") < 2.0001f);
    CHECK(cache.getValue("c") > 2.9999f);
    CHECK(cache.getValue("c") < 3.0001f);
    CHECK(cache.size() == 3);
}

TEST_CASE("ParameterCache: duplicate registration throws", "[cache]") {
    ParameterCache cache;
    std::atomic<float> v{ 0.0f };
    cache.registerParameter("x", &v);
    CHECK_THROWS(cache.registerParameter("x", &v));
}

