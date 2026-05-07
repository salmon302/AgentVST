agentvst_add_plugin(
    NAME     AgentKineticDrumEngine
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Dynamics"
    PLUGIN_CODE "AKDE"
    SOURCES
        src/KineticDrumEngineDSP.cpp
)
