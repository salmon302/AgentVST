agentvst_add_plugin(
    NAME     ZoneOfTheUnknown
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "ZotU"
    SOURCES
        src/ZoneOfTheUnknownDSP.cpp
)
target_include_directories(ZoneOfTheUnknown PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
