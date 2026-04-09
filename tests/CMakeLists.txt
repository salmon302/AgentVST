# ─────────────────────────────────────────────────────────────────────────────
# AgentVST Test Suite
# ─────────────────────────────────────────────────────────────────────────────

add_executable(agentvst_tests
    unit/SchemaParserTest.cpp
    unit/UIGeneratorTest.cpp
    unit/ParameterCacheTest.cpp
    unit/APVTSRoundTripTest.cpp
    unit/BiquadFilterTest.cpp
    unit/GainTest.cpp
    unit/OscillatorTest.cpp
    unit/EnvelopeFollowerTest.cpp
    unit/DelayTest.cpp
    unit/DSPRouterTest.cpp
    unit/ProcessBlockHandlerTest.cpp
    integration/FullPipelineTest.cpp
    integration/StateRoundTripTest.cpp
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

if(MSVC)
    target_compile_options(agentvst_tests PRIVATE /EHsc)
endif()

# Register tests with CTest
include(CTest)
include(Catch)
catch_discover_tests(agentvst_tests)
