/**
 * PluginEntry.cpp
 *
 * Defines the JUCE plugin entry point.
 *
 * This file is compiled per-plugin (added by agentvst_add_plugin() in CMake),
 * not into the shared framework library.  JUCE calls createPluginFilter() to
 * instantiate the AudioProcessor when the plugin is loaded by a DAW.
 */
#include "AgentVSTProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AgentVST::AgentVSTProcessor();
}
