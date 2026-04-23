cmake_minimum_required(VERSION 3.22)
project(AgentVST VERSION 0.1.0 LANGUAGES CXX)

set(AGENTVST_VST3_DEPLOY_DIR "" CACHE PATH
    "Optional folder to copy built VST3 plugins after build (e.g. Ableton custom VST3 folder)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Policy for FetchContent
cmake_policy(SET CMP0135 NEW)

# ─────────────────────────────────────────────────────────────────────────────
# Dependencies
# ─────────────────────────────────────────────────────────────────────────────
include(FetchContent)

# JUCE 8
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# nlohmann/json (header-only)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# Catch2 (for tests)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ─────────────────────────────────────────────────────────────────────────────
# CMake utilities
# ─────────────────────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(agentvst_ids)
include(agentvst_plugin)

# ─────────────────────────────────────────────────────────────────────────────
# Framework library
# ─────────────────────────────────────────────────────────────────────────────
add_subdirectory(framework)

# ─────────────────────────────────────────────────────────────────────────────
# Example plugins
# ─────────────────────────────────────────────────────────────────────────────
option(AGENTVST_BUILD_EXAMPLES "Build example plugins" ON)
if(AGENTVST_BUILD_EXAMPLES)
    add_subdirectory(examples/simple_gain)
    add_subdirectory(examples/compressor)
    add_subdirectory(examples/three_band_eq)
    add_subdirectory(examples/escher_shifter/Escher_Pitch_Shifter)
    add_subdirectory(examples/devils_grid/Devils_Grid_Irrational_Delay)
    add_subdirectory(examples/ring_modulator/Ring_Modulator_of_the_Void)
    add_subdirectory(examples/sensory_dissonance_fuzz/Sensory_Dissonance_Smart_Fuzz)
    add_subdirectory(examples/material_decay/Material_Decay_Resonator)
    add_subdirectory(examples/dechronicization/Dechronicization_Engine)
    add_subdirectory(examples/doppler_shifted_room/Doppler_Shifted_Room)
    add_subdirectory(examples/false_memory_convolution/FalseMemoryConvolution)
    add_subdirectory(examples/fractured_tachyon/FracturedTachyon)
    add_subdirectory(examples/human_error_ensemble/HumanErrorEnsemble)
    add_subdirectory(examples/infrasonic_dread/InfrasonicDreadGenerator)
    add_subdirectory(examples/zone_of_the_unknown/ZoneOfTheUnknown)
    add_subdirectory(examples/girih_asymmetrical_delay/GirihAsymmetricalDelay)
    add_subdirectory(examples/tonnetz_tessellator/TonnetzTessellator)
    add_subdirectory(examples/strapwork_counterpoint/StrapworkCounterpointWeaver)
    add_subdirectory(examples/tawhid_infinite_suspension/TawhidInfiniteSuspension)
    add_subdirectory(examples/dualism_undertone_anchor/DualismUndertoneAnchor)
    add_subdirectory(examples/thread_of_memory_mediant_splitter/ThreadOfMemoryMediantSplitter)
    add_subdirectory(examples/lacrimosa_6_5_modulator/Lacrimosa65Modulator)
    add_subdirectory(examples/reluctant_resolution_temporal_suspension/ReluctantResolutionTemporalSuspension)
    add_subdirectory(examples/transients_to_texture/TransientsToTexture)
    add_subdirectory(examples/anti_loudness_transient_restorer/AntiLoudnessTransientRestorer)
    add_subdirectory(examples/bimodal_weight_fletcher_munson_eq/bimodal_weight_fletcher_munson_eq)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# CLI Tool
# ─────────────────────────────────────────────────────────────────────────────
option(AGENTVST_BUILD_CLI "Build the agentvst CLI tool" ON)
if(AGENTVST_BUILD_CLI)
    add_subdirectory(cli)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────
option(AGENTVST_BUILD_TESTS "Build unit and integration tests" ON)
if(AGENTVST_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
