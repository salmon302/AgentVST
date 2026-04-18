cmake_minimum_required(VERSION 3.20)
project(Sensory_Dissonance_Smart_Fuzz)

# Core framework dependency
agentvst_add_plugin(
    NAME     Sensory_Dissonance_Smart_Fuzz
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    SOURCES
        src/Sensory_Dissonance_Smart_FuzzDSP.cpp
)

target_include_directories(Sensory_Dissonance_Smart_Fuzz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
