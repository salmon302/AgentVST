Title: impl-2026-04-20-thread-of-memory-mediant-splitter
Date: 2026-04-21T00:26:54.0940652Z
Author: Seth Nenninger; Agent: GitHub Copilot (GPT-5.3-Codex)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Implement ThreadOfMemoryMediantSplitter example plugin from VSTS section 2 concept

Task Reference:
- User request: Develop section 2, "The Thread of Memory Mediant Splitter"

Specification Summary:
- Build a chromatic mediant transition effect that freezes a common-tone layer and shifts remaining material by either 6:5 or 5:4.
- Expose target controls:
  - Common-Tone Freeze
  - Mediant Mode (6:5 or 5:4)
  - Wet/Ghost Mix

Implementation Notes:
- Added new example plugin folder and build files:
  - examples/thread_of_memory_mediant_splitter/ThreadOfMemoryMediantSplitter/CMakeLists.txt
  - examples/thread_of_memory_mediant_splitter/ThreadOfMemoryMediantSplitter/plugin.json
  - examples/thread_of_memory_mediant_splitter/ThreadOfMemoryMediantSplitter/src/ThreadOfMemoryMediantSplitterDSP.cpp
- Registered the example in root build:
  - CMakeLists.txt
- DSP behavior implemented:
  - Harmonic-transition trigger from pitch-jump detection (zero-crossing estimator + envelope gate).
  - Common-tone ghost layer from low-band split and frozen loop playback.
  - Remaining voice layer pitch-shifted by dual-head delay-resampling shifter at 6:5 or 5:4.
  - Wet/Ghost blend between shifted harmony and frozen ghost tone.

Verification & Quality Gates:
- Build: PASS via direct CMake + MSBuild validation.
  - `cmake -S . -B build` -> EXIT:0
  - `cmake --build build --config Debug --target ThreadOfMemoryMediantSplitter` -> EXIT:0
- Editor diagnostics: PASS on modified files (no reported errors).

Evidence links:
- Added: examples/thread_of_memory_mediant_splitter/ThreadOfMemoryMediantSplitter/CMakeLists.txt
- Added: examples/thread_of_memory_mediant_splitter/ThreadOfMemoryMediantSplitter/plugin.json
- Added: examples/thread_of_memory_mediant_splitter/ThreadOfMemoryMediantSplitter/src/ThreadOfMemoryMediantSplitterDSP.cpp
- Modified: CMakeLists.txt
