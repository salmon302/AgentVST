# ─────────────────────────────────────────────────────────────────────────────
# AgentVST Framework Core Library
#
# This static library contains all framework logic that does NOT depend on
# JucePlugin_xxx macros (those are only valid inside a juce_add_plugin target).
#
# The plugin-wrapper files (Processor, Editor, PluginEntry) are compiled
# directly into each plugin target by agentvst_add_plugin() in cmake/.
# ─────────────────────────────────────────────────────────────────────────────

add_library(agentvst_framework STATIC
    src/AgentDSP.cpp
    src/SchemaParser.cpp
    src/ParameterCache.cpp
    src/DSPRouter.cpp
    src/UIGenerator.cpp
    src/ProcessBlockHandler.cpp
    src/modules/BiquadFilter.cpp
    src/modules/EnvelopeFollower.cpp
    src/modules/Gain.cpp
    src/modules/Oscillator.cpp
    src/modules/Delay.cpp
)

# Public headers — consumers see these
target_include_directories(agentvst_framework PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Private headers — internal to framework
target_include_directories(agentvst_framework PRIVATE
    src
    src/PluginWrapper
)

target_compile_definitions(agentvst_framework PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(agentvst_framework PUBLIC
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
    nlohmann_json::nlohmann_json
)

# Compiler warnings
if(MSVC)
    target_compile_options(agentvst_framework PRIVATE /W4 /EHsc)
else()
    target_compile_options(agentvst_framework PRIVATE -Wall -Wextra -Wpedantic)
endif()
