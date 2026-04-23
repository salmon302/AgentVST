agentvst_add_plugin(
    NAME     DualismUndertoneAnchor
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    SOURCES
        src/DualismUndertoneAnchorDSP.cpp
)
