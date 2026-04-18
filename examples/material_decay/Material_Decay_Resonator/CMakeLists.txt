cmake_minimum_required(VERSION 3.20)
project(Material_Decay_Resonator)

# Core framework dependency
agentvst_add_plugin(
    NAME     Material_Decay_Resonator
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    SOURCES
        src/Material_Decay_ResonatorDSP.cpp
)

target_include_directories(Material_Decay_Resonator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
