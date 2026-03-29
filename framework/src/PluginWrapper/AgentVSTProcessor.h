/**
 * AgentVSTProcessor.h
 *
 * The concrete juce::AudioProcessor for AgentVST plugins.
 * This file is compiled per-plugin (added to each juce_add_plugin target
 * by agentvst_add_plugin()) — NOT compiled into the shared framework library.
 *
 * It performs the following at construction time:
 *   1. Reads AGENTVST_SCHEMA_PATH (compile-time constant) and parses the JSON schema.
 *   2. Builds the APVTS ParameterLayout from the schema.
 *   3. Caches all std::atomic<float>* pointers into a ParameterCache.
 *   4. Instantiates the agent's IAgentDSP via the registered factory.
 *
 * At prepareToPlay():
 *   5. Calls agentDSP->prepare() — agent allocates all memory here.
 *   6. Configures ProcessBlockHandler.
 *
 * At processBlock():
 *   7. Delegates to ProcessBlockHandler which calls processSample() with a
 *      watchdog.
 */
#pragma once

#include "AgentDSP.h"
#include "SchemaParser.h"
#include "ParameterCache.h"
#include "ProcessBlockHandler.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string>

namespace AgentVST {

class AgentVSTProcessor : public juce::AudioProcessor {
public:
    AgentVSTProcessor();
    ~AgentVSTProcessor() override;

    // ── juce::AudioProcessor interface ───────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()                              override { return 1; }
    int  getCurrentProgram()                           override { return 0; }
    void setCurrentProgram(int)                        override {}
    const juce::String getProgramName(int)             override { return {}; }
    void changeProgramName(int, const juce::String&)   override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── AgentVST accessors (used by editor) ──────────────────────────────────
    juce::AudioProcessorValueTreeState& getAPVTS()        { return *apvts_; }
    const PluginSchema&                 getSchema() const { return schema_; }
    juce::ValueTree&                     getNonParameterState() { return nonParamState_; }

private:
    PluginSchema                                         schema_;
    std::unique_ptr<juce::AudioProcessorValueTreeState>  apvts_;
    juce::ValueTree                                       nonParamState_;
    ParameterCache                                       paramCache_;
    std::unique_ptr<IAgentDSP>                           agentDSP_;
    ProcessBlockHandler                                  blockHandler_;

    juce::AudioProcessorValueTreeState::ParameterLayout buildParameterLayout() const;
    void initParameterCache();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AgentVSTProcessor)
};

} // namespace AgentVST
