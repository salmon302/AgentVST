agentvst_add_plugin(
    NAME     JustIntonationMuqarnas
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "JIMq"
    SOURCES
        src/JustIntonationMuqarnasDSP.cpp
)
target_include_directories(JustIntonationMuqarnas PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/framework/src")
