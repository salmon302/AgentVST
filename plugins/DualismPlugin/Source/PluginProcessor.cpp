#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../../../examples/dualism/src/DualismProcessor.h"
#include "../../../../examples/dualism/src/PitchDetector.h"
#include "../../../../examples/dualism/src/UndertoneBank.h"

class DualismAudioProcessor::DualismImpl {
public:
    dualism::DualismProcessor proc;
    DualismImpl() : proc(44100.0) {}
};

DualismAudioProcessor::DualismAudioProcessor()
     : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo())
                                       .withOutput ("Output", juce::AudioChannelSet::stereo())),
       pImpl(std::make_unique<DualismImpl>())
{
}

DualismAudioProcessor::~DualismAudioProcessor() = default;

void DualismAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    pImpl->proc.prepare(sampleRate, samplesPerBlock);
}

void DualismAudioProcessor::releaseResources()
{
}

bool DualismAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return true;
}

void DualismAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    // mono processing on left channel; copy to right if stereo
    pImpl->proc.process(left, left, numSamples);
    if (right)
        std::memcpy(right, left, numSamples * sizeof(float));
}

bool DualismAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* DualismAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

const juce::String DualismAudioProcessor::getName() const { return "Dualism"; }
bool DualismAudioProcessor::acceptsMidi() const { return false; }
bool DualismAudioProcessor::producesMidi() const { return false; }
bool DualismAudioProcessor::isMidiEffect() const { return false; }
double DualismAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int DualismAudioProcessor::getNumPrograms() { return 1; }
int DualismAudioProcessor::getCurrentProgram() { return 0; }
void DualismAudioProcessor::setCurrentProgram (int) { }
const juce::String DualismAudioProcessor::getProgramName (int) { return {}; }
void DualismAudioProcessor::changeProgramName (int, const juce::String&) { }

void DualismAudioProcessor::getStateInformation (juce::MemoryBlock&) { }
void DualismAudioProcessor::setStateInformation (const void*, int) { }
