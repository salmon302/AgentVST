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
        "NAME;SCHEMA;VENDOR;VERSION;CATEGORY;UI_ROOT;DEV_SERVER_URL;PLUGIN_CODE"
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
    if(NOT PLUGIN_DEV_SERVER_URL)
        set(PLUGIN_DEV_SERVER_URL "http://127.0.0.1:5173")
    endif()

    # Optional: copy built VST3 bundles into a custom dev folder (e.g. Ableton)
    set(_VST3_DEPLOY_DIR "")
    if(DEFINED AGENTVST_VST3_DEPLOY_DIR AND NOT "${AGENTVST_VST3_DEPLOY_DIR}" STREQUAL "")
        file(TO_CMAKE_PATH "${AGENTVST_VST3_DEPLOY_DIR}" _VST3_DEPLOY_DIR)
    endif()

    set(_UI_ROOT "")
    if(PLUGIN_UI_ROOT)
        file(TO_CMAKE_PATH "${PLUGIN_UI_ROOT}" _UI_ROOT)
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

    if(PLUGIN_PLUGIN_CODE)
        string(LENGTH "${PLUGIN_PLUGIN_CODE}" _plugin_code_length)
        if(NOT _plugin_code_length EQUAL 4)
            message(FATAL_ERROR "agentvst_add_plugin: PLUGIN_CODE must be exactly 4 ASCII characters.")
        endif()
        string(REGEX MATCH "^[A-Za-z0-9][A-Za-z0-9][A-Za-z0-9][A-Za-z0-9]$" _plugin_code_ascii "${PLUGIN_PLUGIN_CODE}")
        if(NOT _plugin_code_ascii)
            message(FATAL_ERROR "agentvst_add_plugin: PLUGIN_CODE must contain exactly 4 alphanumeric ASCII characters.")
        endif()
        set(PLUGIN_CODE "${PLUGIN_PLUGIN_CODE}")
    endif()

    # Validate uniqueness within this CMake configure to prevent accidental
    # VST3 class-ID collisions when plugin names share prefixes.
    get_property(_existing_plugin_for_code GLOBAL PROPERTY "AGENTVST_PLUGIN_CODE_${PLUGIN_CODE}")
    if(_existing_plugin_for_code AND NOT "${_existing_plugin_for_code}" STREQUAL "${PLUGIN_NAME}")
        message(FATAL_ERROR
            "agentvst_add_plugin: PLUGIN_CODE '${PLUGIN_CODE}' is already used by '${_existing_plugin_for_code}'. "
            "Set a unique PLUGIN_CODE in agentvst_add_plugin(...).")
    endif()
    set_property(GLOBAL PROPERTY "AGENTVST_PLUGIN_CODE_${PLUGIN_CODE}" "${PLUGIN_NAME}")

    message(STATUS "[AgentVST] Plugin '${PLUGIN_NAME}' (v${PLUGIN_VERSION})")
    message(STATUS "  Vendor  : ${PLUGIN_VENDOR}")
    message(STATUS "  JUCE Code: ${PLUGIN_CODE}")
    message(STATUS "  VST3 CID: ${VST3_CID}")
    message(STATUS "  AU Bundle: ${AU_BUNDLE}")

    # ── Determine build formats ───────────────────────────────────────────────
    set(_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND _FORMATS AU)
    endif()

    # ── Create JUCE plugin target ─────────────────────────────────────────────
    set(_COPY_PLUGIN_AFTER_BUILD FALSE)
    if(_VST3_DEPLOY_DIR)
        set(_COPY_PLUGIN_AFTER_BUILD TRUE)
        # JUCE resolves copy paths from inherited directory/target properties.
        # Set the directory property before juce_add_plugin so generated format
        # targets pick up the custom deployment path.
        set_property(DIRECTORY PROPERTY JUCE_VST3_COPY_DIR "${_VST3_DEPLOY_DIR}")
    endif()

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
        COPY_PLUGIN_AFTER_BUILD ${_COPY_PLUGIN_AFTER_BUILD}
    )

    if(_VST3_DEPLOY_DIR)
        file(MAKE_DIRECTORY "${_VST3_DEPLOY_DIR}")
        set_target_properties(${PLUGIN_NAME} PROPERTIES
            JUCE_VST3_COPY_DIR "${_VST3_DEPLOY_DIR}")
        if(TARGET ${PLUGIN_NAME}_VST3)
            set_target_properties(${PLUGIN_NAME}_VST3 PROPERTIES
                JUCE_VST3_COPY_DIR "${_VST3_DEPLOY_DIR}")
        endif()
        message(STATUS "  VST3 Deploy Dir: ${_VST3_DEPLOY_DIR}")
    endif()

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
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        AGENTVST_SCHEMA_PATH="${PLUGIN_SCHEMA}"
        AGENTVST_PLUGIN_NAME="${PLUGIN_NAME}"
        AGENTVST_PLUGIN_VENDOR="${PLUGIN_VENDOR}"
        AGENTVST_PLUGIN_VERSION="${PLUGIN_VERSION}"
        AGENTVST_UI_ROOT="${_UI_ROOT}"
        AGENTVST_DEV_SERVER_URL="${PLUGIN_DEV_SERVER_URL}"
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
