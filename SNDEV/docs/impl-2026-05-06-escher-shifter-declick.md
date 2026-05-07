Title: impl-escher-shifter-declick
Date: 2026-05-06T22:05:42Z
Author: Seth Nenninger; Agent: GitHub Copilot (GPT-5.2-Codex)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Add phase smoothing to reduce click artifacts in both Escher shifters.

Task Reference:
- User request: "Let's work to mitigate the clicking in both shifters"

Specification Summary:
- Reduce clicking in Escher Pitch Shifter and Escher Shift by mitigating phase-step discontinuities.

Implementation Notes:
- Added smoothed phase tracking to soften quantized phase jumps and delay-tap wrap transitions.
- 2026-05-06T23:17:33Z: Added wrap-aware tap crossfades with smoothstep curves to reduce tap-change clicks.
- Updated:
  - examples/escher_shifter/Escher_Pitch_Shifter/src/Escher_Pitch_ShifterDSP.cpp
  - examples/escher_shift/Escher_Shift/src/Escher_ShiftDSP.cpp
- Tests: Not run (not requested).

Evidence links:
- Commits: N/A
- PR: N/A
- Related conception file: N/A
