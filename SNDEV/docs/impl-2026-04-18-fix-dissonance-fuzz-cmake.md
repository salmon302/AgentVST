Title: impl-2026-04-18-fix-dissonance-fuzz-cmake
Date: 2026-04-18T15:10:00Z
Author: Seth Nenninger (Gemini 3 Flash (Preview) Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Fix CMakeLists.txt for Sensory_Dissonance_Smart_Fuzz

Problem:
The plugin's CMakeLists.txt used `find_package(AgentVST REQUIRED)` which is incorrect for internal examples that should use the `agentvst_add_plugin` macro provided by the repository's root CMake context. This caused configuration to fail.

Implementation Notes:
- Updated `examples/sensory_dissonance_fuzz/Sensory_Dissonance_Smart_Fuzz/CMakeLists.txt` to follow the standard pattern used by other examples (e.g., SimpleGain, Devils_Grid).
- Removed `find_package(AgentVST)` as the framework is already available via the root's `add_subdirectory(framework)` and the `agentvst_plugin` inclusion.

Evidence links:
- Modified file: [examples/sensory_dissonance_fuzz/Sensory_Dissonance_Smart_Fuzz/CMakeLists.txt](examples/sensory_dissonance_fuzz/Sensory_Dissonance_Smart_Fuzz/CMakeLists.txt)
