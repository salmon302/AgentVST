Title: impl-universal-bitcrush-context-fix
Date: 2026-04-26T00:00:00Z
Author: Seth Nenninger; Agent: Seth Nenninger (GPT-5.3-Codex Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Fixed framework DSP context sample indexing to be monotonic across blocks to prevent phase discontinuities.

Task Reference:
- User request: investigate and implement fix for unintended universal bitcrush-like artifacts across VSTs.

Specification Summary:
- Continue investigation in shared framework path.
- Implement a framework-level correction when a root-cause pattern is confirmed.

Implementation Notes:
- Updated framework/include/AgentDSP.h:
  - Changed DSPContext.currentSample type to std::int64_t.
  - Updated semantics comment to monotonic sample index since prepare().
- Updated framework/include/ProcessBlockHandler.h:
  - Added std::int64_t absoluteSampleCounter_ member.
  - Updated buildContext signature to accept std::int64_t sample index.
- Updated framework/src/ProcessBlockHandler.cpp:
  - Reset absoluteSampleCounter_ in prepare().
  - Switched to per-sample context construction in processBlock().
  - Populated currentSample with absolute (monotonic) sample index.
  - Advanced absoluteSampleCounter_ by numSamples after each processed block.
- Rationale:
  - Multiple DSPs use ctx.currentSample to run once-per-sample shared logic and/or phase math.
  - Prior block-local indexing caused repeated 0..N-1 ramps each callback, which can induce block-boundary phase discontinuities perceived as grain/bitcrush artifacts.

Evidence links:
- Files changed:
  - framework/include/AgentDSP.h
  - framework/include/ProcessBlockHandler.h
  - framework/src/ProcessBlockHandler.cpp
- Validation:
  - VS Code diagnostics on changed files: no errors.
  - Debug build script executed; no compiler/linker errors observed in captured output (warnings present, build output still streaming during capture window).
