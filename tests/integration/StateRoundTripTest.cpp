/**
 * StateRoundTripTest.cpp
 *
 * Integration tests for StateSerializer: verifies that APVTS parameter state
 * and non-parameter ValueTree data survive a full save/load round-trip, and
 * that legacy APVTS-only XML is also handled correctly.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "StateSerializer.h"
#include <juce_audio_processors/juce_audio_processors.h>

using AgentVST::StateSerializer;

// ─────────────────────────────────────────────────────────────────────────────
// DummyProcessor — minimal AudioProcessor to host an APVTS
// ─────────────────────────────────────────────────────────────────────────────

class DummyProcessor : public juce::AudioProcessor
{
public:
    DummyProcessor() : AudioProcessor(BusesProperties()) {}

    const juce::String getName() const override { return "DummyProcessor"; }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    double getTailLengthSeconds() const override { return 0.0; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    int  getNumPrograms()                          override { return 1; }
    int  getCurrentProgram()                       override { return 0; }
    void setCurrentProgram(int)                    override {}
    const juce::String getProgramName(int)         override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&)   override {}
    void setStateInformation(const void*, int)     override {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build a fresh APVTS with gain_db (float, -60..12, default 0)
//         and bypass (bool, default false)
// ─────────────────────────────────────────────────────────────────────────────

static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain_db", 1},
        "Gain dB",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1},
        "Bypass",
        false));

    return {params.begin(), params.end()};
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: gain_db round-trip (-6 dB → save → reset → load → -6 dB)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StateRoundTrip: save/reload round-trip preserves gain_db value",
          "[integration][state][roundtrip]")
{
    // ── Setup processor + APVTS ────────────────────────────────────────────
    DummyProcessor proc1;
    juce::AudioProcessorValueTreeState apvts1(proc1, nullptr, "AgentVSTState", makeLayout());

    // Set gain_db to -6.0
    const float targetDb = -6.0f;
    const float norm1    = apvts1.getParameterRange("gain_db").convertTo0to1(targetDb);
    apvts1.getParameter("gain_db")->setValueNotifyingHost(norm1);

    // Verify the value was applied before serialising
    const float readback1 = apvts1.getParameterRange("gain_db")
        .convertFrom0to1(apvts1.getParameter("gain_db")->getValue());
    CHECK_THAT(readback1, Catch::Matchers::WithinAbs(targetDb, 0.1f));

    // Non-parameter state (empty for this test)
    juce::ValueTree nonParam1{"NonParameterState"};

    // Serialise
    auto xml = StateSerializer::createStateXml(apvts1, nonParam1);
    REQUIRE(xml != nullptr);

    // ── Fresh processor — default gain (0 dB) ─────────────────────────────
    DummyProcessor proc2;
    juce::AudioProcessorValueTreeState apvts2(proc2, nullptr, "AgentVSTState", makeLayout());

    // Confirm default before load
    const float defaultDb = apvts2.getParameterRange("gain_db")
        .convertFrom0to1(apvts2.getParameter("gain_db")->getValue());
    CHECK_THAT(defaultDb, Catch::Matchers::WithinAbs(0.0f, 0.1f));

    // Load saved state
    juce::ValueTree nonParam2{"NonParameterState"};
    StateSerializer::loadStateFromXml(apvts2, nonParam2, *xml);

    // Verify gain_db was restored
    const float restoredDb = apvts2.getParameterRange("gain_db")
        .convertFrom0to1(apvts2.getParameter("gain_db")->getValue());
    CHECK_THAT(restoredDb, Catch::Matchers::WithinAbs(targetDb, 0.1f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: non-parameter state (string + int) survives round-trip
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StateRoundTrip: non-parameter state is preserved through save/load",
          "[integration][state][nonparam]")
{
    DummyProcessor proc1;
    juce::AudioProcessorValueTreeState apvts1(proc1, nullptr, "AgentVSTState", makeLayout());

    // Write preset_name (String) and ui_width (int) into non-parameter state
    juce::ValueTree nonParam1{"NonParameterState"};
    nonParam1.setProperty("preset_name", "My Cool Preset", nullptr);
    nonParam1.setProperty("ui_width",    800,              nullptr);

    auto xml = StateSerializer::createStateXml(apvts1, nonParam1);
    REQUIRE(xml != nullptr);

    // Load into a fresh instance
    DummyProcessor proc2;
    juce::AudioProcessorValueTreeState apvts2(proc2, nullptr, "AgentVSTState", makeLayout());

    juce::ValueTree nonParam2{"NonParameterState"};
    StateSerializer::loadStateFromXml(apvts2, nonParam2, *xml);

    // Verify both properties were restored
    CHECK(nonParam2.getProperty("preset_name").toString() == "My Cool Preset");
    CHECK(static_cast<int>(nonParam2.getProperty("ui_width")) == 800);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: legacy APVTS-only XML (no AgentVSTRoot wrapper) loads correctly
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StateRoundTrip: legacy APVTS-only XML format loads correctly",
          "[integration][state][legacy]")
{
    // Build a legacy XML blob: just the raw APVTS state, no AgentVSTRoot wrapper
    DummyProcessor proc1;
    juce::AudioProcessorValueTreeState apvts1(proc1, nullptr, "AgentVSTState", makeLayout());

    const float targetDb = -3.0f;
    const float norm     = apvts1.getParameterRange("gain_db").convertTo0to1(targetDb);
    apvts1.getParameter("gain_db")->setValueNotifyingHost(norm);

    // Export raw APVTS state (no AgentVSTRoot)
    const juce::ValueTree apvtsState = apvts1.copyState();
    const auto legacyXml             = apvtsState.createXml();
    REQUIRE(legacyXml != nullptr);

    // The legacy XML tag must NOT be "AgentVSTRoot"
    REQUIRE_FALSE(legacyXml->hasTagName("AgentVSTRoot"));

    // Load into a fresh instance using the legacy path
    DummyProcessor proc2;
    juce::AudioProcessorValueTreeState apvts2(proc2, nullptr, "AgentVSTState", makeLayout());

    juce::ValueTree nonParam2{"NonParameterState"};
    REQUIRE_NOTHROW(StateSerializer::loadStateFromXml(apvts2, nonParam2, *legacyXml));

    // Verify gain_db was restored from the legacy blob
    const float restoredDb = apvts2.getParameterRange("gain_db")
        .convertFrom0to1(apvts2.getParameter("gain_db")->getValue());
    CHECK_THAT(restoredDb, Catch::Matchers::WithinAbs(targetDb, 0.1f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: output XML has correct "AgentVSTRoot" tag name
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StateRoundTrip: output XML has correct AgentVSTRoot tag name",
          "[integration][state][xml]")
{
    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "AgentVSTState", makeLayout());

    juce::ValueTree nonParam{"NonParameterState"};
    nonParam.setProperty("dummy", 42, nullptr);

    auto xml = StateSerializer::createStateXml(apvts, nonParam);
    REQUIRE(xml != nullptr);

    // Root tag must be "AgentVSTRoot"
    CHECK(xml->hasTagName("AgentVSTRoot"));

    // Must have two child elements: APVTS state + NonParameterState
    CHECK(xml->getNumChildElements() == 2);

    // One child should be the APVTS state (tag matches apvts.state type)
    bool foundApvts     = false;
    bool foundNonParam  = false;

    for (auto* child : xml->getChildIterator())
    {
        if (child->hasTagName("AgentVSTState"))
            foundApvts = true;
        if (child->hasTagName("NonParameterState"))
            foundNonParam = true;
    }

    CHECK(foundApvts);
    CHECK(foundNonParam);
}
