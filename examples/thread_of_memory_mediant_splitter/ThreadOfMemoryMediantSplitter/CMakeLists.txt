agentvst_add_plugin(
    NAME     ThreadOfMemoryMediantSplitter
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "TMem"
    SOURCES
        src/ThreadOfMemoryMediantSplitterDSP.cpp
)
target_include_directories(ThreadOfMemoryMediantSplitter PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
