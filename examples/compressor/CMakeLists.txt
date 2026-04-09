agentvst_add_plugin(
    NAME     AgentCompressor
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Dynamics"
    PLUGIN_CODE "ACMP"
    SOURCES
        src/CompressorDSP.cpp
)
