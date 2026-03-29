agentvst_add_plugin(
    NAME "SimpleGain"
    SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR "AgentVST"
    VERSION "1.0.0"
    CATEGORY "Fx"
    SOURCES
        src/GainDSP.cpp
)
