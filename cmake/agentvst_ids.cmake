# ─────────────────────────────────────────────────────────────────────────────
# agentvst_ids.cmake
#
# Generates STABLE, deterministic plugin identifiers from the plugin name and
# vendor string.  These IDs are embedded into the compiled binary and MUST NOT
# change between plugin versions — if they do, existing DAW project files will
# fail to load the plugin.
#
# Usage:
#   agentvst_generate_ids(
#       PLUGIN_NAME   "SimpleGain"
#       VENDOR_NAME   "AgentVST"
#       OUT_VST3_CID        <var>   # 32-char hex string
#       OUT_AU_BUNDLE       <var>   # com.vendor.name
#       OUT_MANUFACTURER    <var>   # 4-char code (JUCE)
#       OUT_PLUGIN_CODE     <var>   # 4-char code (JUCE)
#   )
# ─────────────────────────────────────────────────────────────────────────────

function(agentvst_generate_ids)
    cmake_parse_arguments(ARG ""
        "PLUGIN_NAME;VENDOR_NAME;OUT_VST3_CID;OUT_AU_BUNDLE;OUT_MANUFACTURER;OUT_PLUGIN_CODE"
        "" ${ARGN})

    if(NOT ARG_PLUGIN_NAME OR NOT ARG_VENDOR_NAME)
        message(FATAL_ERROR "agentvst_generate_ids: PLUGIN_NAME and VENDOR_NAME are required.")
    endif()

    # ── VST3 CID ──────────────────────────────────────────────────────────────
    # A 128-bit value derived from a deterministic MD5 hash of
    # "<VendorName>/<PluginName>".  Using a namespaced key prevents accidental
    # collisions with other frameworks that also hash on name alone.
    set(_seed "${ARG_VENDOR_NAME}/${ARG_PLUGIN_NAME}")
    string(MD5 _hash "${_seed}")
    string(TOUPPER "${_hash}" _hash_upper)
    set(${ARG_OUT_VST3_CID} "${_hash_upper}" PARENT_SCOPE)

    # ── AU Bundle ID ──────────────────────────────────────────────────────────
    # com.<vendor_lower_alphanum>.<plugin_lower_alphanum>
    string(TOLOWER "${ARG_VENDOR_NAME}" _v)
    string(TOLOWER "${ARG_PLUGIN_NAME}" _p)
    string(REGEX REPLACE "[^a-z0-9]" "" _v "${_v}")
    string(REGEX REPLACE "[^a-z0-9]" "" _p "${_p}")
    if("${_v}" STREQUAL "")
        set(_v "vendor")
    endif()
    if("${_p}" STREQUAL "")
        set(_p "plugin")
    endif()
    set(${ARG_OUT_AU_BUNDLE} "com.${_v}.${_p}" PARENT_SCOPE)

    # ── JUCE 4-char manufacturer code ─────────────────────────────────────────
    # Must be exactly 4 ASCII chars.  Pad with 'x' if vendor name is short.
    set(_mfr "${ARG_VENDOR_NAME}xxxx")
    string(SUBSTRING "${_mfr}" 0 4 _mfr4)
    set(${ARG_OUT_MANUFACTURER} "${_mfr4}" PARENT_SCOPE)

    # ── JUCE 4-char plugin code ───────────────────────────────────────────────
    set(_plg "${ARG_PLUGIN_NAME}xxxx")
    string(SUBSTRING "${_plg}" 0 4 _plg4)
    set(${ARG_OUT_PLUGIN_CODE} "${_plg4}" PARENT_SCOPE)

endfunction()
