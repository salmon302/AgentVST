agentvst_add_plugin(
    NAME     StrapworkCounterpointWeaver
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "Strp"
    SOURCES
        src/StrapworkCounterpointWeaverDSP.cpp
)
target_include_directories(StrapworkCounterpointWeaver PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
