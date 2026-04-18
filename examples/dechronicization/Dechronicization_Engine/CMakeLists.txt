# Core framework dependency
agentvst_add_plugin(
    NAME     Dechronicization_Engine
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    SOURCES
        src/Dechronicization_EngineDSP.cpp
)

target_include_directories(Dechronicization_Engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

