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
    // Save both APVTS state and non-parameter state into a single binary blob.
    static void saveState(juce::AudioProcessorValueTreeState& apvts,
                          const juce::ValueTree& nonParamState,
                          juce::MemoryBlock& destData)
    {
        juce::ValueTree root("AgentVSTRoot");
        root.addChild(apvts.copyState(), -1, nullptr);
        root.addChild(nonParamState, -1, nullptr);

        if (auto xml = root.createXml())
            copyXmlToBinary(*xml, destData);
    }

    // Load state. Supports both the new 'AgentVSTRoot' format and the
    // legacy APVTS-only XML (tag == apvts.state.getType()).
    static void loadState(juce::AudioProcessorValueTreeState& apvts,
                          juce::ValueTree& nonParamState,
                          const void* data, int sizeInBytes)
    {
        std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
        if (!xml) return;

        const juce::String apvtsType = apvts.state.getType();

        if (xml->hasTagName("AgentVSTRoot")) {
            juce::ValueTree root = juce::ValueTree::fromXml(*xml);
            // Look for child nodes by type
            for (int i = 0; i < root.getNumChildren(); ++i) {
                juce::ValueTree child = root.getChild(i);
                if (child.getType() == apvtsType) {
                    apvts.replaceState(child);
                } else if (child.getType() == "NonParameterState") {
                    nonParamState = child;
                }
            }
        } else if (xml->hasTagName(apvtsType)) {
            // Legacy APVTS-only format
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
        } else {
            // Unknown format — ignore
        }
    }
};

} // namespace AgentVST
