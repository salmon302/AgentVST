cmake_minimum_required(VERSION 3.15)

project(DualismPlugin LANGUAGES CXX)

# Find JUCE (assuming it's available via the parent project or as a submodule)
# If JUCE is not found, this will need adjustment to match the repo's existing setup.
find_package(JUCE REQUIRED CONFIG)

juce_add_plugin(DualismPlugin
    COMPANY_NAME "AgentVST"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Agent
    PLUGIN_CODE Dual
    FORMATS VST3 Standalone
    PRODUCT_NAME "Dualism")

juce_generate_juce_header(DualismPlugin)

target_sources(DualismPlugin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(DualismPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DualismPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
