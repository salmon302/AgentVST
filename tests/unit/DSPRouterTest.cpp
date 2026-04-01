#include <catch2/catch_test_macros.hpp>
#include "DSPRouter.h"
#include "ParameterCache.h"

#include <atomic>

namespace {

class ScaleNode : public AgentVST::DSPNode {
public:
    explicit ScaleNode(float gain)
        : gain_(gain) {
        nodeType_ = "ScaleNode";
    }

    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
    }

    float processSample(int /*channel*/, float input) override {
        return input * gain_;
    }

    void reset() override {}

    void setParameter(const std::string& name, float value) override {
        if (name == "gain")
            gain_ = value;
    }

private:
    float gain_ = 1.0f;
};

AgentVST::DSPRoute route(const char* src, const char* dst) {
    AgentVST::DSPRoute r;
    r.source = src;
    r.destination = dst;
    return r;
}

} // namespace

TEST_CASE("DSPRouter: chains nodes in topological order", "[dsp][router]") {
    AgentVST::DSPRouter router;
    router.addNode("a", std::make_unique<ScaleNode>(2.0f));
    router.addNode("b", std::make_unique<ScaleNode>(0.5f));

    std::vector<AgentVST::DSPRoute> routes{
        route("input", "a"),
        route("a", "b"),
        route("b", "output")
    };

    REQUIRE_NOTHROW(router.setRouting(routes));
    router.prepare(48000.0, 256);

    const float out = router.processSample(0, 1.0f);
    CHECK(out > 0.9999f);
    CHECK(out < 1.0001f);
}

TEST_CASE("DSPRouter: rejects cyclic graphs", "[dsp][router]") {
    AgentVST::DSPRouter router;
    router.addNode("a", std::make_unique<ScaleNode>(1.0f));
    router.addNode("b", std::make_unique<ScaleNode>(1.0f));

    std::vector<AgentVST::DSPRoute> routes{
        route("input", "a"),
        route("a", "b"),
        route("b", "a"),
        route("b", "output")
    };

    CHECK_THROWS_AS(router.setRouting(routes), AgentVST::DSPRouter::RoutingError);
}

TEST_CASE("DSPRouter: rejects disconnected node graph", "[dsp][router]") {
    AgentVST::DSPRouter router;
    router.addNode("a", std::make_unique<ScaleNode>(1.0f));
    router.addNode("b", std::make_unique<ScaleNode>(1.0f));

    std::vector<AgentVST::DSPRoute> routes{
        route("input", "a"),
        route("a", "output"),
        route("b", "output")
    };

    CHECK_THROWS_AS(router.setRouting(routes), AgentVST::DSPRouter::RoutingError);
}

TEST_CASE("DSPRouter: parameter links update node parameters", "[dsp][router]") {
    AgentVST::DSPRouter router;
    router.addNode("gainNode", std::make_unique<ScaleNode>(1.0f));

    AgentVST::DSPRoute r1 = route("input", "gainNode");
    r1.parameterLinks["drive"] = "gain";
    AgentVST::DSPRoute r2 = route("gainNode", "output");

    std::vector<AgentVST::DSPRoute> routes{r1, r2};

    REQUIRE_NOTHROW(router.setRouting(routes));
    router.prepare(48000.0, 256);

    AgentVST::ParameterCache cache;
    std::atomic<float> drive{0.25f};
    cache.registerParameter("drive", &drive);
    cache.finalize();

    router.updateParameters(cache);
    const float out = router.processSample(0, 1.0f);
    CHECK(out > 0.2499f);
    CHECK(out < 0.2501f);
}

