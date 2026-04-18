agentvst_add_plugin(
    NAME     OceanicHaasWidener
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Spatial"
    PLUGIN_CODE "Ocea"
    SOURCES
        src/OceanicHaasWidenerDSP.cpp
)
target_include_directories(OceanicHaasWidener PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
