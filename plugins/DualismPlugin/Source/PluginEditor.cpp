#include "PluginEditor.h"

DualismAudioProcessorEditor::DualismAudioProcessorEditor (DualismAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (400, 300);
}

DualismAudioProcessorEditor::~DualismAudioProcessorEditor() = default;

void DualismAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.drawText ("Dualism — Undertone Anchor", getLocalBounds(), juce::Justification::centred, true);
}

void DualismAudioProcessorEditor::resized()
{
}
