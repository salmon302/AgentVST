#include "AgentVSTProcessor.h"
#include "AgentVSTEditor.h"
#include "StateSerializer.h"
#include "SchemaParser.h"

#include "../modules/BiquadFilter.h"
#include "../modules/Delay.h"
#include "../modules/EnvelopeFollower.h"
#include "../modules/Gain.h"
#include "../modules/Oscillator.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <vector>

// AGENTVST_SCHEMA_PATH is injected by agentvst_add_plugin() in cmake
#ifndef AGENTVST_SCHEMA_PATH
  #error "AGENTVST_SCHEMA_PATH must be defined. Use agentvst_add_plugin() in CMake."
#endif

namespace AgentVST {

namespace {

struct ChannelMeters {
    float peak = 0.0f;
    float rms  = 0.0f;
};

ChannelMeters computeChannelMeters(const juce::AudioBuffer<float>& buffer, int channel) {
    ChannelMeters meters;

    if (channel < 0 || channel >= buffer.getNumChannels())
        return meters;

    const auto* data = buffer.getReadPointer(channel);
    const int numSamples = buffer.getNumSamples();
    if (data == nullptr || numSamples <= 0)
        return meters;

    double sumSquares = 0.0;
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        const float sample = data[i];
        const float absSample = std::abs(sample);
        if (absSample > peak)
            peak = absSample;

        sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    }

    meters.peak = peak;
    meters.rms = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(numSamples)));
    return meters;
}

std::string toLower(std::string text) {
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::unique_ptr<DSPNode> createNodeForId(const std::string& nodeId) {
    const auto lower = toLower(nodeId);

    if (lower.find("filter") != std::string::npos ||
        lower.find("biquad") != std::string::npos) {
        return std::make_unique<BiquadFilter>();
    }
    if (lower.find("osc") != std::string::npos ||
        lower.find("lfo") != std::string::npos) {
        return std::make_unique<Oscillator>();
    }
    if (lower.find("env") != std::string::npos ||
        lower.find("follow") != std::string::npos) {
        return std::make_unique<EnvelopeFollower>();
    }
    if (lower.find("delay") != std::string::npos ||
        lower.find("echo") != std::string::npos) {
        return std::make_unique<Delay>();
    }
    if (lower.find("gain") != std::string::npos ||
        lower.find("amp") != std::string::npos ||
        lower.find("volume") != std::string::npos ||
        lower.find("level") != std::string::npos) {
        return std::make_unique<Gain>();
    }

    return {};
}

} // namespace

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

    // 4. Instantiate agent's DSP class.
    //    createRegisteredDSP() is defined by the user's AGENTVST_REGISTER_DSP()
    //    macro.  Calling it directly (rather than going through a static-init
    //    factory) forces the linker to pull the user's DSP translation unit
    //    into the plugin DLL even though JUCE routes user sources through a
    //    static library — see AgentDSP.h for the full rationale.
    agentDSP_ = createRegisteredDSP();
    if (agentDSP_ == nullptr) {
        class PassThrough : public IAgentDSP {
        public:
            float processSample(int, float input, const DSPContext&) override { return input; }
        };
        agentDSP_ = std::make_unique<PassThrough>();
        juce::Logger::writeToLog("AgentVST: createRegisteredDSP() returned null. Using pass-through.");
    }

    initDSPRouterFromSchema();

    blockHandler_.onPotentialNoOp = [pluginName = schema_.metadata.name](double relDiff, int blocks) {
        juce::Logger::writeToLog(
            "AgentVST: Potential no-op DSP detected for plugin '" +
            juce::String(pluginName) +
            "' (relative diff energy=" + juce::String(relDiff, 12) +
            ", consecutive blocks=" + juce::String(blocks) + "). "
            "Check that processSample modifies input under non-bypass settings, "
            "verify wet/mix defaults are audible, and ensure the deployed .vst3 was "
            "updated (DAW lock can prevent copy)."
        );
    };

    blockHandler_.setAgentDSP(agentDSP_.get(), paramCache_);
    blockHandler_.setDSPRouter(&dspRouter_);
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

void AgentVSTProcessor::initDSPRouterFromSchema() {
    if (schema_.dspRouting.empty())
        return;

    std::vector<std::string> nodeIds;
    auto addNodeId = [&nodeIds](const std::string& id) {
        if (std::find(nodeIds.begin(), nodeIds.end(), id) == nodeIds.end())
            nodeIds.push_back(id);
    };

    for (const auto& route : schema_.dspRouting) {
        if (route.source != "input" && route.source != "output")
            addNodeId(route.source);
        if (route.destination != "input" && route.destination != "output")
            addNodeId(route.destination);
    }

    if (nodeIds.empty())
        return;

    dspRouter_.clearNodes();

    for (const auto& nodeId : nodeIds) {
        auto node = createNodeForId(nodeId);
        if (!node) {
            juce::Logger::writeToLog("AgentVST: Router disabled, unknown node type for id: " +
                                     juce::String(nodeId));
            dspRouter_.clearNodes();
            return;
        }
        dspRouter_.addNode(nodeId, std::move(node));
    }

    try {
        dspRouter_.setRouting(schema_.dspRouting);
    } catch (const std::exception& e) {
        juce::Logger::writeToLog(juce::String("AgentVST: Router configuration failed: ") + e.what());
        dspRouter_.clearNodes();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio processing
// ─────────────────────────────────────────────────────────────────────────────

void AgentVSTProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sawProcessBlock_.store(false, std::memory_order_relaxed);
    sawProcessBlockBypassed_.store(false, std::memory_order_relaxed);
    processCallbackCount_.store(0, std::memory_order_relaxed);

    if (dspRouter_.isConfigured()) {
        dspRouter_.prepare(sampleRate, samplesPerBlock);
        dspRouter_.bindParameters(paramCache_);
    }

    blockHandler_.prepare(sampleRate, samplesPerBlock);

    juce::AudioPlayHead::CurrentPositionInfo dummyPos;
    dummyPos.isPlaying   = false;
    dummyPos.bpm         = 120.0;
    dummyPos.ppqPosition = 0.0;
    (void)dummyPos;
}

void AgentVSTProcessor::releaseResources() {
    if (dspRouter_.isConfigured())
        dspRouter_.reset();

    if (agentDSP_)
        agentDSP_->reset();

    inputPeakL_.store(0.0f, std::memory_order_relaxed);
    inputPeakR_.store(0.0f, std::memory_order_relaxed);
    inputRmsL_.store(0.0f, std::memory_order_relaxed);
    inputRmsR_.store(0.0f, std::memory_order_relaxed);
    outputPeakL_.store(0.0f, std::memory_order_relaxed);
    outputPeakR_.store(0.0f, std::memory_order_relaxed);
    outputRmsL_.store(0.0f, std::memory_order_relaxed);
    outputRmsR_.store(0.0f, std::memory_order_relaxed);
    sawProcessBlock_.store(false, std::memory_order_relaxed);
    sawProcessBlockBypassed_.store(false, std::memory_order_relaxed);
}

bool AgentVSTProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainInput  = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    const bool inputSupported =
        (mainInput == juce::AudioChannelSet::mono()) ||
        (mainInput == juce::AudioChannelSet::stereo());
    const bool outputSupported =
        (mainOutput == juce::AudioChannelSet::mono()) ||
        (mainOutput == juce::AudioChannelSet::stereo());

    if (!inputSupported || !outputSupported)
        return false;

    // Non-synth effect: keep input/output layouts symmetric.
    return mainInput == mainOutput;
}

void AgentVSTProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;

    sawProcessBlock_.store(true, std::memory_order_relaxed);
    processCallbackCount_.fetch_add(1, std::memory_order_relaxed);

    const int totalInputChannels  = getTotalNumInputChannels();
    const int totalOutputChannels = getTotalNumOutputChannels();
    const int bufferChannels      = buffer.getNumChannels();

    // Explicitly clear channels that have no input source.
    for (int ch = totalInputChannels; ch < totalOutputChannels && ch < bufferChannels; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const int processingChannels = juce::jmin(totalInputChannels, totalOutputChannels, bufferChannels);
    if (processingChannels <= 0) {
        buffer.clear();
        return;
    }

    auto* const* writePointers = buffer.getArrayOfWritePointers();
    juce::AudioBuffer<float> processingView(writePointers,
                                            processingChannels,
                                            buffer.getNumSamples());

    const auto inputL = computeChannelMeters(processingView, 0);
    const auto inputR = processingChannels > 1 ? computeChannelMeters(processingView, 1)
                                                : inputL;

    // Get host position info (JUCE 8: getPosition() replaces deprecated getCurrentPosition())
    juce::AudioPlayHead::CurrentPositionInfo posInfo{};
    if (auto* playHead = getPlayHead()) {
        if (auto pos = playHead->getPosition()) {
            posInfo.isPlaying   = pos->getIsPlaying();
            posInfo.bpm         = pos->getBpm().orFallback(120.0);
            posInfo.ppqPosition = pos->getPpqPosition().orFallback(0.0);
        }
    }

    blockHandler_.processBlock(processingView, midi, posInfo);

    const auto outputL = computeChannelMeters(processingView, 0);
    const auto outputR = processingChannels > 1 ? computeChannelMeters(processingView, 1)
                                                 : outputL;

    inputPeakL_.store(inputL.peak, std::memory_order_relaxed);
    inputPeakR_.store(inputR.peak, std::memory_order_relaxed);
    inputRmsL_.store(inputL.rms, std::memory_order_relaxed);
    inputRmsR_.store(inputR.rms, std::memory_order_relaxed);
    outputPeakL_.store(outputL.peak, std::memory_order_relaxed);
    outputPeakR_.store(outputR.peak, std::memory_order_relaxed);
    outputRmsL_.store(outputL.rms, std::memory_order_relaxed);
    outputRmsR_.store(outputR.rms, std::memory_order_relaxed);
}

void AgentVSTProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midi*/) {
    sawProcessBlockBypassed_.store(true, std::memory_order_relaxed);
    processCallbackCount_.fetch_add(1, std::memory_order_relaxed);

    const int totalInputChannels  = getTotalNumInputChannels();
    const int totalOutputChannels = getTotalNumOutputChannels();
    const int bufferChannels      = buffer.getNumChannels();

    for (int ch = totalInputChannels; ch < totalOutputChannels && ch < bufferChannels; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
}

// ─────────────────────────────────────────────────────────────────────────────
// State serialization
// ─────────────────────────────────────────────────────────────────────────────

void AgentVSTProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // Serialise APVTS state + non-parameter state
    if (auto xml = StateSerializer::createStateXml(*apvts_, nonParamState_))
        copyXmlToBinary(*xml, destData);
}

void AgentVSTProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        StateSerializer::loadStateFromXml(*apvts_, nonParamState_, *xml);
}

AgentVSTProcessor::MeterSnapshot AgentVSTProcessor::getMeterSnapshot() const noexcept {
    MeterSnapshot snapshot;
    snapshot.inputPeakL  = inputPeakL_.load(std::memory_order_relaxed);
    snapshot.inputPeakR  = inputPeakR_.load(std::memory_order_relaxed);
    snapshot.inputRmsL   = inputRmsL_.load(std::memory_order_relaxed);
    snapshot.inputRmsR   = inputRmsR_.load(std::memory_order_relaxed);
    snapshot.outputPeakL = outputPeakL_.load(std::memory_order_relaxed);
    snapshot.outputPeakR = outputPeakR_.load(std::memory_order_relaxed);
    snapshot.outputRmsL  = outputRmsL_.load(std::memory_order_relaxed);
    snapshot.outputRmsR  = outputRmsR_.load(std::memory_order_relaxed);
    return snapshot;
}

bool AgentVSTProcessor::hasSeenAnyProcessCallback() const noexcept {
    return sawProcessBlock_.load(std::memory_order_relaxed) ||
           sawProcessBlockBypassed_.load(std::memory_order_relaxed);
}

std::uint64_t AgentVSTProcessor::processCallbackCount() const noexcept {
    return processCallbackCount_.load(std::memory_order_relaxed);
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
