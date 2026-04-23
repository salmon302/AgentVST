agentvst_add_plugin(
    NAME     ReluctantResolutionTemporalSuspension
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "Rlct"
    SOURCES
        src/ReluctantResolutionTemporalSuspensionDSP.cpp
)
