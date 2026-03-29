# ─────────────────────────────────────────────────────────────────────────────
# agentvst_plugin.cmake
#
# Provides the agentvst_add_plugin() helper that creates a fully-configured
# JUCE plugin target from an agent's JSON schema + DSP source(s).
#
# Usage:
#   agentvst_add_plugin(
#       NAME     "SimpleGain"
#       SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
#       VENDOR   "AgentVST"           # optional, default "AgentVST"
#       VERSION  "1.0.0"              # optional, default "1.0.0"
#       CATEGORY "Fx"                 # optional, default "Fx"
#       SOURCES
#           src/GainDSP.cpp
#   )
# ─────────────────────────────────────────────────────────────────────────────

function(agentvst_add_plugin)
    cmake_parse_arguments(PLUGIN ""
        "NAME;SCHEMA;VENDOR;VERSION;CATEGORY"
        "SOURCES"
        ${ARGN})

    # ── Validate required arguments ───────────────────────────────────────────
    if(NOT PLUGIN_NAME)
        message(FATAL_ERROR "agentvst_add_plugin: NAME is required.")
    endif()
    if(NOT PLUGIN_SCHEMA)
        message(FATAL_ERROR "agentvst_add_plugin: SCHEMA is required.")
    endif()
    if(NOT EXISTS "${PLUGIN_SCHEMA}")
        message(FATAL_ERROR "agentvst_add_plugin: SCHEMA file not found: ${PLUGIN_SCHEMA}")
    endif()

    # ── Defaults ─────────────────────────────────────────────────────────────
    if(NOT PLUGIN_VENDOR)
        set(PLUGIN_VENDOR "AgentVST")
    endif()
    if(NOT PLUGIN_VERSION)
        set(PLUGIN_VERSION "1.0.0")
    endif()
    if(NOT PLUGIN_CATEGORY)
        set(PLUGIN_CATEGORY "Fx")
    endif()

    # ── Generate deterministic IDs ────────────────────────────────────────────
    agentvst_generate_ids(
        PLUGIN_NAME   "${PLUGIN_NAME}"
        VENDOR_NAME   "${PLUGIN_VENDOR}"
        OUT_VST3_CID        VST3_CID
        OUT_AU_BUNDLE       AU_BUNDLE
        OUT_MANUFACTURER    MANUFACTURER_CODE
        OUT_PLUGIN_CODE     PLUGIN_CODE
    )

    message(STATUS "[AgentVST] Plugin '${PLUGIN_NAME}' (v${PLUGIN_VERSION})")
    message(STATUS "  Vendor  : ${PLUGIN_VENDOR}")
    message(STATUS "  VST3 CID: ${VST3_CID}")
    message(STATUS "  AU Bundle: ${AU_BUNDLE}")

    # ── Determine build formats ───────────────────────────────────────────────
    set(_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND _FORMATS AU)
    endif()

    # ── Create JUCE plugin target ─────────────────────────────────────────────
    juce_add_plugin(${PLUGIN_NAME}
        COMPANY_NAME            "${PLUGIN_VENDOR}"
        BUNDLE_ID               "${AU_BUNDLE}"
        PLUGIN_MANUFACTURER_CODE "${MANUFACTURER_CODE}"
        PLUGIN_CODE             "${PLUGIN_CODE}"
        FORMATS                 ${_FORMATS}
        PRODUCT_NAME            "${PLUGIN_NAME}"
        VERSION                 "${PLUGIN_VERSION}"
        IS_SYNTH                FALSE
        NEEDS_MIDI_INPUT        FALSE
        NEEDS_MIDI_OUTPUT       FALSE
        IS_MIDI_EFFECT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )

    # ── Add framework plugin-wrapper sources ──────────────────────────────────
    # These are compiled per-plugin (not in the shared framework library)
    # because they may reference JucePlugin_xxx macros.
    set(_WRAPPER_SOURCES
        "${CMAKE_SOURCE_DIR}/framework/src/PluginWrapper/AgentVSTProcessor.cpp"
        "${CMAKE_SOURCE_DIR}/framework/src/PluginWrapper/AgentVSTEditor.cpp"
        "${CMAKE_SOURCE_DIR}/framework/src/PluginWrapper/PluginEntry.cpp"
    )

    target_sources(${PLUGIN_NAME} PRIVATE
        ${_WRAPPER_SOURCES}
        ${PLUGIN_SOURCES}
    )

    # ── Compile definitions ───────────────────────────────────────────────────
    # Convert schema path to a compile-time string constant
    target_compile_definitions(${PLUGIN_NAME} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        AGENTVST_SCHEMA_PATH="${PLUGIN_SCHEMA}"
        AGENTVST_PLUGIN_NAME="${PLUGIN_NAME}"
        AGENTVST_PLUGIN_VENDOR="${PLUGIN_VENDOR}"
        AGENTVST_PLUGIN_VERSION="${PLUGIN_VERSION}"
    )

    # ── Include paths ─────────────────────────────────────────────────────────
    target_include_directories(${PLUGIN_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/framework/include"
        "${CMAKE_SOURCE_DIR}/framework/src/PluginWrapper"
    )

    # ── Link libraries ────────────────────────────────────────────────────────
    target_link_libraries(${PLUGIN_NAME} PRIVATE
        agentvst_framework
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_dsp
        nlohmann_json::nlohmann_json
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

endfunction()
