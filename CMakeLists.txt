cmake_minimum_required(VERSION 3.22)
project(AgentVST VERSION 0.1.0 LANGUAGES CXX)

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
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────
option(AGENTVST_BUILD_TESTS "Build unit and integration tests" ON)
if(AGENTVST_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
