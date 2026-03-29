/**
 * APVTSRoundTripTest.cpp
 *
 * Verifies that StateSerializer preserves APVTS state and non-parameter
 * ValueTree across save/load cycles.
 */
#include <catch2/catch_test_macros.hpp>
#include "StateSerializer.h"
#include <juce_audio_processors/juce_audio_processors.h>

using AgentVST::StateSerializer;

// Minimal dummy AudioProcessor to host an APVTS for testing
class DummyProcessor : public juce::AudioProcessor {
public:
    DummyProcessor() : AudioProcessor(BusesProperties()) {}
    const juce::String getName() const override { return "Dummy"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

TEST_CASE("StateSerializer: APVTS + nonParamState roundtrip", "[state][serialization]") {
    DummyProcessor p1;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gain", "Gain", 0.0f, 1.0f, 0.5f));
    juce::AudioProcessorValueTreeState::ParameterLayout layout{params.begin(), params.end()};
    juce::AudioProcessorValueTreeState apvts1(p1, nullptr, "AgentVSTState", layout);

    // Modify parameter and non-parameter state
    if (auto* p = apvts1.getRawParameterValue("gain"))
        p->store(0.75f);

    juce::ValueTree nonParam1{"NonParameterState"};
    nonParam1.setProperty("note", "hello", nullptr);

    juce::MemoryBlock blob;
    StateSerializer::saveState(apvts1, nonParam1, blob);

    // Load into fresh APVTS
    DummyProcessor p2;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params2;
    params2.push_back(std::make_unique<juce::AudioParameterFloat>("gain", "Gain", 0.0f, 1.0f, 0.5f));
    juce::AudioProcessorValueTreeState::ParameterLayout layout2{params2.begin(), params2.end()};
    juce::AudioProcessorValueTreeState apvts2(p2, nullptr, "AgentVSTState", layout2);

    juce::ValueTree nonParam2{"NonParameterState"};
    StateSerializer::loadState(apvts2, nonParam2, blob.getData(), static_cast<int>(blob.getSize()));

    auto* raw = apvts2.getRawParameterValue("gain");
    REQUIRE(raw != nullptr);
    CHECK(raw->load() == Catch::Approx(0.75f));
    CHECK(nonParam2.getProperty("note").toString() == "hello");
}
