Title: impl-2026-04-18-standardize-example-cmake
Date: 2026-04-18T15:25:00Z
Author: Seth Nenninger (Gemini 3 Flash (Preview) Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Standardize CMake configuration across all example plugins

Problem:
Several example plugins (Material_Decay_Resonator, Dechronicization_Engine) had outdated or incorrect CMakeLists.txt files. Material_Decay_Resonator was incorrectly using `find_package(AgentVST)`, and Dechronicization_Engine was using obsolete framework macros like `agentvst_generate_plugin` and `agentvst_add_plugin_target`.

Implementation Notes:
- Updated `examples/material_decay/Material_Decay_Resonator/CMakeLists.txt`:
    - Removed `find_package(AgentVST)`.
    - Switched to standard `agentvst_add_plugin` macro.
    - Added `src` directory to target include directories.
- Updated `examples/dechronicization/Dechronicization_Engine/CMakeLists.txt`:
    - Replaced custom generation/target logic with the standard `agentvst_add_plugin` macro.
    - Corrected schema path to point to the local `plugin.json` instead of a non-existent global one.
    - Added `src` directory to target include directories.

Evidence links:
- Material Decay: [examples/material_decay/Material_Decay_Resonator/CMakeLists.txt](examples/material_decay/Material_Decay_Resonator/CMakeLists.txt)
- Dechronicization: [examples/dechronicization/Dechronicization_Engine/CMakeLists.txt](examples/dechronicization/Dechronicization_Engine/CMakeLists.txt)
