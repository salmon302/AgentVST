agentvst_add_plugin(
    NAME     HumanErrorEnsemble
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Modulation"
    PLUGIN_CODE "HuEr"
    SOURCES
        src/HumanErrorEnsembleDSP.cpp
)
target_include_directories(HumanErrorEnsemble PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
