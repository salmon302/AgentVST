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
#include "DSPRouter.h"
#include "SchemaParser.h"
#include "ParameterCache.h"
#include "ProcessBlockHandler.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace AgentVST {

class AgentVSTProcessor : public juce::AudioProcessor {
public:
    struct MeterSnapshot {
        float inputPeakL  = 0.0f;
        float inputPeakR  = 0.0f;
        float inputRmsL   = 0.0f;
        float inputRmsR   = 0.0f;
        float outputPeakL = 0.0f;
        float outputPeakR = 0.0f;
        float outputRmsL  = 0.0f;
        float outputRmsR  = 0.0f;
    };

    AgentVSTProcessor();
    ~AgentVSTProcessor() override;

    // ── juce::AudioProcessor interface ───────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override {}
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int  getNumPrograms()                              override { return 1; }
    int  getCurrentProgram()                           override { return 0; }
    void setCurrentProgram(int)                        override {}
    const juce::String getProgramName(int)             override { return {}; }
    void changeProgramName(int, const juce::String&)   override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── AgentVST accessors (used by editor) ──────────────────────────────────
    juce::AudioProcessorValueTreeState& getAPVTS()        { return *apvts_; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const { return *apvts_; }
    const PluginSchema&                 getSchema() const { return schema_; }
    juce::ValueTree&                     getNonParameterState() { return nonParamState_; }
    MeterSnapshot                        getMeterSnapshot() const noexcept;
    bool                                 hasSeenAnyProcessCallback() const noexcept;
    std::uint64_t                        processCallbackCount() const noexcept;

private:
    PluginSchema                                         schema_;
    std::unique_ptr<juce::AudioProcessorValueTreeState>  apvts_;
    juce::ValueTree                                       nonParamState_;
    ParameterCache                                       paramCache_;
    std::unique_ptr<IAgentDSP>                           agentDSP_;
    DSPRouter                                            dspRouter_;
    ProcessBlockHandler                                  blockHandler_;
    std::atomic<float>                                   inputPeakL_ { 0.0f };
    std::atomic<float>                                   inputPeakR_ { 0.0f };
    std::atomic<float>                                   inputRmsL_  { 0.0f };
    std::atomic<float>                                   inputRmsR_  { 0.0f };
    std::atomic<float>                                   outputPeakL_{ 0.0f };
    std::atomic<float>                                   outputPeakR_{ 0.0f };
    std::atomic<float>                                   outputRmsL_ { 0.0f };
    std::atomic<float>                                   outputRmsR_ { 0.0f };
    std::atomic<bool>                                    sawProcessBlock_{ false };
    std::atomic<bool>                                    sawProcessBlockBypassed_{ false };
    std::atomic<std::uint64_t>                           processCallbackCount_{ 0 };
    double                                               preparedSampleRate_ = 44100.0;
    int                                                  preparedBlockSize_  = 512;

    juce::AudioProcessorValueTreeState::ParameterLayout buildParameterLayout() const;
    void initParameterCache();
    void initDSPRouterFromSchema();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AgentVSTProcessor)
};

} // namespace AgentVST
