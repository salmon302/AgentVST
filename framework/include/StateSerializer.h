/**
 * StateSerializer.h
 *
 * Helper for serializing plugin state including APVTS state and
 * an optional non-parameter ValueTree.  Provides compatibility with
 * older APVTS-only state blobs.
 */
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace AgentVST {

class StateSerializer {
public:
    // Build XML containing APVTS state + non-parameter state.
    static std::unique_ptr<juce::XmlElement> createStateXml(
        juce::AudioProcessorValueTreeState& apvts,
        const juce::ValueTree& nonParamState)
    {
        juce::ValueTree root("AgentVSTRoot");
        root.addChild(apvts.copyState(), -1, nullptr);
        root.addChild(nonParamState, -1, nullptr);
        return root.createXml();
    }

    // Load state from XML. Supports both the new 'AgentVSTRoot' format and
    // legacy APVTS-only XML (tag == apvts.state.getType()).
    static void loadStateFromXml(juce::AudioProcessorValueTreeState& apvts,
                                 juce::ValueTree& nonParamState,
                                 const juce::XmlElement& xml)
    {
        const juce::Identifier apvtsType = apvts.state.getType();

        if (xml.hasTagName("AgentVSTRoot")) {
            juce::ValueTree root = juce::ValueTree::fromXml(xml);
            // Look for child nodes by type
            for (int i = 0; i < root.getNumChildren(); ++i) {
                juce::ValueTree child = root.getChild(i);
                if (child.getType() == apvtsType) {
                    apvts.replaceState(child);
                } else if (child.getType() == juce::Identifier("NonParameterState")) {
                    nonParamState = child;
                }
            }
        } else if (xml.hasTagName(apvtsType.toString())) {
            // Legacy APVTS-only format
            apvts.replaceState(juce::ValueTree::fromXml(xml));
        } else {
            // Unknown format — ignore
        }
    }
};

} // namespace AgentVST
