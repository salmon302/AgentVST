# ─────────────────────────────────────────────────────────────────────────────
# AgentVST CLI Tool
#
# Provides:
#   agentvst validate <schema.json>          — Validates a plugin.json schema
#   agentvst validate-dsp <DSP.cpp> [...]    — Scans DSP code for RT violations
#   agentvst build <schema.json> [options]   — Configures + builds the plugin
#   agentvst init <PluginName>               — Scaffolds a new plugin project
# ─────────────────────────────────────────────────────────────────────────────

add_executable(agentvst
    src/main.cpp
    src/ValidateCommand.cpp
    src/ValidateDspCommand.cpp
    src/BuildCommand.cpp
    src/InitCommand.cpp
    src/CommandRegistry.cpp
    # Compile only the JUCE-free framework sources directly (no JUCE dependency)
    "${CMAKE_SOURCE_DIR}/framework/src/SchemaParser.cpp"
    "${CMAKE_SOURCE_DIR}/framework/src/ParameterCache.cpp"
)

target_include_directories(agentvst PRIVATE
    "${CMAKE_SOURCE_DIR}/framework/include"
    src
)

# The CLI does NOT link against agentvst_framework (which pulls in JUCE).
# It only needs SchemaParser + ParameterCache, compiled above directly.
target_link_libraries(agentvst PRIVATE
    nlohmann_json::nlohmann_json
)

target_compile_definitions(agentvst PRIVATE
    AGENTVST_VERSION="${PROJECT_VERSION}"
    AGENTVST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Enable exceptions on MSVC (required for std::filesystem and throw/catch)
if(MSVC)
    target_compile_options(agentvst PRIVATE /W4 /EHsc)
else()
    target_compile_options(agentvst PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install CLI alongside the framework (optional)
install(TARGETS agentvst DESTINATION bin)
