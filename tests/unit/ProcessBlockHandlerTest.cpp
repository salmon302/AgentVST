#include <catch2/catch_test_macros.hpp>
#include "ProcessBlockHandler.h"
#include "DSPRouter.h"
#include "ParameterCache.h"

#include <atomic>

namespace {

class GainDSP : public AgentVST::IAgentDSP {
public:
    explicit GainDSP(float gain)
        : gain_(gain) {}

    float processSample(int /*channel*/, float input, const AgentVST::DSPContext& /*ctx*/) override {
        return input * gain_;
    }

private:
    float gain_ = 1.0f;
};

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

class BusyDSP : public AgentVST::IAgentDSP {
public:
    float processSample(int /*channel*/, float input, const AgentVST::DSPContext& /*ctx*/) override {
        volatile float sink = input;
        for (int i = 0; i < 20000; ++i)
            sink = sink * 1.000001f + 0.000001f;
        return sink;
    }
};

AgentVST::DSPRoute route(const char* src, const char* dst) {
    AgentVST::DSPRoute r;
    r.source = src;
    r.destination = dst;
    return r;
}

} // namespace

TEST_CASE("ProcessBlockHandler: uses agent DSP when no router is configured", "[dsp][processblock]") {
    AgentVST::ProcessBlockHandler handler;

    AgentVST::ParameterCache cache;
    std::atomic<float> dummy{0.0f};
    cache.registerParameter("dummy", &dummy);
    cache.finalize();

    GainDSP dsp(2.0f);
    handler.setAgentDSP(&dsp, cache);
    handler.prepare(48000.0, 64);

    juce::AudioBuffer<float> buffer(1, 4);
    for (int i = 0; i < 4; ++i)
        buffer.setSample(0, i, 1.0f);

    juce::MidiBuffer midi;
    juce::AudioPlayHead::CurrentPositionInfo pos;
    handler.processBlock(buffer, midi, pos);

    for (int i = 0; i < 4; ++i)
        CHECK(buffer.getSample(0, i) > 1.9999f);
    for (int i = 0; i < 4; ++i)
        CHECK(buffer.getSample(0, i) < 2.0001f);
}

TEST_CASE("ProcessBlockHandler: uses router path when configured", "[dsp][processblock]") {
    AgentVST::ProcessBlockHandler handler;

    AgentVST::ParameterCache cache;
    std::atomic<float> gainParam{0.25f};
    cache.registerParameter("drive", &gainParam);
    cache.finalize();

    // Agent DSP would apply 2x if used; router applies 0.25x so output identifies path.
    GainDSP dsp(2.0f);
    handler.setAgentDSP(&dsp, cache);

    AgentVST::DSPRouter router;
    router.addNode("gainNode", std::make_unique<ScaleNode>(1.0f));
    AgentVST::DSPRoute r1 = route("input", "gainNode");
    r1.parameterLinks["drive"] = "gain";
    AgentVST::DSPRoute r2 = route("gainNode", "output");
    router.setRouting({r1, r2});
    router.prepare(48000.0, 64);
    router.bindParameters(cache);

    handler.setDSPRouter(&router);
    handler.prepare(48000.0, 64);

    juce::AudioBuffer<float> buffer(1, 4);
    for (int i = 0; i < 4; ++i)
        buffer.setSample(0, i, 1.0f);

    juce::MidiBuffer midi;
    juce::AudioPlayHead::CurrentPositionInfo pos;
    handler.processBlock(buffer, midi, pos);

    for (int i = 0; i < 4; ++i)
        CHECK(buffer.getSample(0, i) > 0.2499f);
    for (int i = 0; i < 4; ++i)
        CHECK(buffer.getSample(0, i) < 0.2501f);
}

TEST_CASE("ProcessBlockHandler: watchdog flags slow DSP", "[dsp][processblock][watchdog]") {
    AgentVST::ProcessBlockHandler handler;

    AgentVST::ParameterCache cache;
    std::atomic<float> dummy{0.0f};
    cache.registerParameter("dummy", &dummy);
    cache.finalize();

    BusyDSP dsp;
    handler.setAgentDSP(&dsp, cache);
    handler.setWatchdogBudget(0.01);      // 1% of block duration
    handler.setWatchdogCheckInterval(1);  // check every sample
    handler.prepare(48000.0, 64);

    juce::AudioBuffer<float> buffer(1, 64);
    buffer.clear();

    juce::MidiBuffer midi;
    juce::AudioPlayHead::CurrentPositionInfo pos;
    handler.processBlock(buffer, midi, pos);

    CHECK(handler.hadWatchdogViolation());
    CHECK(handler.watchdogViolationCount() >= 1);
}

