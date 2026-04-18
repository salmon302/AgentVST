agentvst_add_plugin(
    NAME     TawhidInfiniteSuspension
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "Tawh"
    SOURCES
        src/TawhidInfiniteSuspensionDSP.cpp
)
target_include_directories(TawhidInfiniteSuspension PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
