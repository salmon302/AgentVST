# ─────────────────────────────────────────────────────────────────────────────
# AgentVST Test Suite
# ─────────────────────────────────────────────────────────────────────────────

add_executable(agentvst_tests
    unit/SchemaParserTest.cpp
    unit/ParameterCacheTest.cpp
    unit/APVTSRoundTripTest.cpp
    unit/BiquadFilterTest.cpp
    unit/GainTest.cpp
)

target_include_directories(agentvst_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/framework/include"
    "${CMAKE_SOURCE_DIR}/framework/src"
    "${CMAKE_SOURCE_DIR}/framework/src/modules"
)

target_link_libraries(agentvst_tests PRIVATE
    agentvst_framework
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
)

# Add the BiquadFilter and Gain source files directly (they are in framework/src/modules)
target_sources(agentvst_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/framework/src/modules/BiquadFilter.cpp"
    "${CMAKE_SOURCE_DIR}/framework/src/modules/Gain.cpp"
    "${CMAKE_SOURCE_DIR}/framework/src/modules/EnvelopeFollower.cpp"
    "${CMAKE_SOURCE_DIR}/framework/src/modules/Oscillator.cpp"
)

# Register tests with CTest
include(CTest)
include(Catch)
catch_discover_tests(agentvst_tests)
