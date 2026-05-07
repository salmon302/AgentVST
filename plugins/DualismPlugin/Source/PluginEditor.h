#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class DualismAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    DualismAudioProcessorEditor (DualismAudioProcessor&);
    ~DualismAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DualismAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DualismAudioProcessorEditor)
};
