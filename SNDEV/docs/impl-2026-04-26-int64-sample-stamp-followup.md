Title: impl-int64-sample-stamp-followup
Date: 2026-04-26T00:00:00Z
Author: Seth Nenninger; Agent: Seth Nenninger (GPT-5.3-Codex Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Updated example DSP sample-stamp fields to int64 after framework context index widening.

Task Reference:
- Follow-up after framework DSPContext currentSample changed to std::int64_t.

Implementation Notes:
- Updated sample-stamp field types and includes in:
  - examples/escher_shifter/Escher_Pitch_Shifter/src/Escher_Pitch_ShifterDSP.cpp
  - examples/devils_grid/Devils_Grid_Irrational_Delay/src/Devils_Grid_Irrational_DelayDSP.cpp
  - examples/sensory_dissonance_fuzz/Sensory_Dissonance_Smart_Fuzz/src/Sensory_Dissonance_Smart_FuzzDSP.cpp
  - examples/thread_of_memory_mediant_splitter/ThreadOfMemoryMediantSplitter/src/ThreadOfMemoryMediantSplitterDSP.cpp
- Added <cstdint> include where needed and changed int sample stamps to std::int64_t.
- Purpose: remove narrowing warnings and keep once-per-sample guards aligned with framework semantics.

Build Observations:
- Prior build log showed non-code blockers:
  - LNK1104 on a specific VST3 output path (likely file lock by host/DAW).
  - Multiple post-build lines: 'pwsh.exe' is not recognized (environment/tooling issue).
