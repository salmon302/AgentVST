Title: impl-2026-04-26-audio-glitch-fix
Date: 2026-04-26T12:45:00Z
Author: Seth Nenninger (Gemini 3 Flash Preview Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Fixed audio glitches caused by inefficient buffer handling and static state.

Task Reference:
User reported glitchy/distorted audio across all VSTs.

Specification Summary:
Investigate and fix audio processing issues in the framework and example plugins.

Implementation Notes:
1.  **Framework Optimization**: Refactored `ProcessBlockHandler::processBlock` to pre-fetch channel write pointers outside the sample loop. This eliminates redundant JUCE calls and ensures consistent pointer access across the block.
2.  **Static State Removal**: Identified and removed `static` filter state variables in `Escher_Pitch_ShifterDSP.cpp` which were causing cross-talk between plugin instances. Moved state to class members.
3.  **Build Script Update**: Updated `_build_release_and_deploy.cmd` to use the correct Visual Studio 2022 path and target the actual Ableton deployment directory.
4.  **Release Build Verification**: Successfully built and deployed 25 plugins in Release mode, which is required for stable DAW performance.

Evidence links:
- Modified: `framework/src/ProcessBlockHandler.cpp`
- Modified: `examples/escher_shifter/Escher_Pitch_Shifter/src/Escher_Pitch_ShifterDSP.cpp`
- Modified: `_build_release_and_deploy.cmd`
- Verification: `scripts/verify_deployment.ps1` shows 25 fresh Release builds.
