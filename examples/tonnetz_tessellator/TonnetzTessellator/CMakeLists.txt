agentvst_add_plugin(
    NAME     TonnetzTessellator
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "Tonn"
    SOURCES
        src/TonnetzTessellatorDSP.cpp
)
target_include_directories(TonnetzTessellator PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
