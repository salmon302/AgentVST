agentvst_add_plugin(
    NAME     Lacrimosa65Modulator
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "Lacr"
    SOURCES
        src/Lacrimosa65ModulatorDSP.cpp
)
