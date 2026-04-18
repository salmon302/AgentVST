agentvst_add_plugin(
    NAME     PhantomSubHarmonicExciter
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "Phan"
    SOURCES
        src/PhantomSubHarmonicExciterDSP.cpp
)
target_include_directories(PhantomSubHarmonicExciter PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
