agentvst_add_plugin(
    NAME     AgentEQ
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "EQ"
    PLUGIN_CODE "AEQ1"
    SOURCES
        src/EQDSP.cpp
)

# The EQ DSP uses BiquadFilter.h from the modules directory
target_include_directories(AgentEQ PRIVATE
    "${CMAKE_SOURCE_DIR}/framework/src"
)
