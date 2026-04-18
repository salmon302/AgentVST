agentvst_add_plugin(
    NAME     InfrasonicDreadGenerator
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Generator"
    PLUGIN_CODE "Infr"
    SOURCES
        src/InfrasonicDreadGeneratorDSP.cpp
)
target_include_directories(InfrasonicDreadGenerator PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
