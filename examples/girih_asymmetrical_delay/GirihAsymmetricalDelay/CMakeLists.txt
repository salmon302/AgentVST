agentvst_add_plugin(
    NAME     GirihAsymmetricalDelay
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Delay"
    PLUGIN_CODE "Giri"
    SOURCES
        src/GirihAsymmetricalDelayDSP.cpp
)
target_include_directories(GirihAsymmetricalDelay PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
