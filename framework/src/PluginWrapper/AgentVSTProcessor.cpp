#include "AgentVSTProcessor.h"
#include "AgentVSTEditor.h"
#include "StateSerializer.h"
#include "SchemaParser.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <stdexcept>

// AGENTVST_SCHEMA_PATH is injected by agentvst_add_plugin() in cmake
#ifndef AGENTVST_SCHEMA_PATH
  #error "AGENTVST_SCHEMA_PATH must be defined. Use agentvst_add_plugin() in CMake."
#endif

namespace AgentVST {

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AgentVSTProcessor::AgentVSTProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // 1. Parse the schema from the compile-time path
    SchemaParser parser;
    try {
        schema_ = parser.parseFile(AGENTVST_SCHEMA_PATH);
    } catch (const std::exception& e) {
        // This is a programming error (bad JSON), not a runtime error.
        // Log and provide a minimal default schema so the plugin can at least load.
        juce::Logger::writeToLog(juce::String("AgentVST: Schema parse error: ") + e.what());
        schema_.metadata.name = "AgentPlugin (error)";
    }

    // 2. Build APVTS from schema
    apvts_ = std::make_unique<juce::AudioProcessorValueTreeState>(
        *this,
        nullptr,               // UndoManager
        "AgentVSTState",       // ValueTree type
        buildParameterLayout()
    );

    // 3. Cache atomic pointers for real-time access
    initParameterCache();

    // 3.5 Initialize non-parameter state (message-thread-only storage)
    nonParamState_ = juce::ValueTree{"NonParameterState"};

    // 4. Instantiate agent's DSP class
    auto& factory = getRegisteredDSPFactory();
    if (factory) {
        agentDSP_ = factory();
    } else {
        // No DSP registered — provide a transparent pass-through
        class PassThrough : public IAgentDSP {
        public:
            float processSample(int, float input, const DSPContext&) override { return input; }
        };
        agentDSP_ = std::make_unique<PassThrough>();
        juce::Logger::writeToLog("AgentVST: No DSP registered. Using pass-through.");
    }

    blockHandler_.setAgentDSP(agentDSP_.get(), paramCache_);
}

AgentVSTProcessor::~AgentVSTProcessor() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Build APVTS ParameterLayout from schema
// ─────────────────────────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
AgentVSTProcessor::buildParameterLayout() const {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    for (const auto& def : schema_.parameters) {
        if (def.type == "boolean") {
            params.push_back(std::make_unique<AudioParameterBool>(
                ParameterID{ def.id, 1 },
                def.name,
                def.defaultValue >= 0.5f
            ));
        } else if (def.type == "enum") {
            StringArray choices;
            for (const auto& opt : def.enumOptions)
                choices.add(juce::String(opt));
            params.push_back(std::make_unique<AudioParameterChoice>(
                ParameterID{ def.id, 1 },
                def.name,
                choices,
                static_cast<int>(def.defaultValue)
            ));
        } else {
            // float or int
            NormalisableRange<float> range(def.minValue, def.maxValue,
                                           def.step, def.skew);

            // Determine number of decimal places for display
            int decimals = (def.type == "int" || def.step >= 1.0f) ? 0 : 2;
            std::string unit = def.unit;

            auto toString = [decimals, unit](float v, int) -> String {
                return String(v, decimals) + (unit.empty() ? "" : " " + String(unit));
            };
            auto fromString = [](const String& s) -> float {
                return s.getFloatValue();
            };

            params.push_back(std::make_unique<AudioParameterFloat>(
                ParameterID{ def.id, 1 },
                def.name,
                range,
                def.defaultValue,
                AudioParameterFloatAttributes()
                    .withStringFromValueFunction(toString)
                    .withValueFromStringFunction(fromString)
            ));
        }
    }

    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
// Cache std::atomic<float>* pointers
// ─────────────────────────────────────────────────────────────────────────────

void AgentVSTProcessor::initParameterCache() {
    for (const auto& def : schema_.parameters) {
        std::atomic<float>* ptr = apvts_->getRawParameterValue(def.id);
        if (ptr)
            paramCache_.registerParameter(def.id, ptr);
        else
            juce::Logger::writeToLog("AgentVST: Warning — could not get raw pointer for: " +
                                     juce::String(def.id));
    }
    paramCache_.finalize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio processing
// ─────────────────────────────────────────────────────────────────────────────

void AgentVSTProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    blockHandler_.prepare(sampleRate, samplesPerBlock);

    juce::AudioPlayHead::CurrentPositionInfo dummyPos;
    dummyPos.isPlaying   = false;
    dummyPos.bpm         = 120.0;
    dummyPos.ppqPosition = 0.0;
    (void)dummyPos;
}

void AgentVSTProcessor::releaseResources() {
    if (agentDSP_)
        agentDSP_->reset();
}

void AgentVSTProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;

    // Get host position info
    juce::AudioPlayHead::CurrentPositionInfo posInfo;
    if (auto* playHead = getPlayHead())
        playHead->getCurrentPosition(posInfo);

    blockHandler_.processBlock(buffer, midi, posInfo);
}

// ─────────────────────────────────────────────────────────────────────────────
// State serialization
// ─────────────────────────────────────────────────────────────────────────────

void AgentVSTProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // Serialise APVTS state + non-parameter state
    StateSerializer::saveState(*apvts_, nonParamState_, destData);
}

void AgentVSTProcessor::setStateInformation(const void* data, int sizeInBytes) {
    StateSerializer::loadState(*apvts_, nonParamState_, data, sizeInBytes);
}

// ─────────────────────────────────────────────────────────────────────────────
// Metadata
// ─────────────────────────────────────────────────────────────────────────────

const juce::String AgentVSTProcessor::getName() const {
    return juce::String(schema_.metadata.name);
}

// ─────────────────────────────────────────────────────────────────────────────
// Editor
// ─────────────────────────────────────────────────────────────────────────────

juce::AudioProcessorEditor* AgentVSTProcessor::createEditor() {
    return new AgentVSTEditor(*this);
}

} // namespace AgentVST
