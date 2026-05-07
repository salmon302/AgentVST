Title: impl-qa-priority2-glitch-fixes
Date: 2026-05-05T04:38:27Z
Author: Seth Nenninger; Agent: Seth Nenninger (GPT-5.2-Codex Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc (QAFeedback Priority 2)
Summary: Reduce glitch artifacts in DifferentialGlideModulator and DynamizationHaasComb.

Task Reference:
- User request: "Let's focus on Priority 2 of the QA Feedback."

Specification Summary:
- Investigate and reduce glitchy audio for Priority 2 plugins.

Implementation Notes:
- DifferentialGlideModulator: clamp delay offsets, normalize band mix, remove flawed denormal subtraction.
- DynamizationHaasComb: smooth delay modulation, clamp delays, adjust Haas delay ordering, reset smoothing state.
- Tests: Not run (not requested).

Evidence links:
- Commits: N/A
- Related conception file: N/A

Verification:
- 2026-05-06T19:14:29Z: Build & Push VST3 (Debug) PASS with warnings (C4244 in InfrasonicDreadGeneratorDSP.cpp, TransientsToTextureDSP.cpp; C4100 in StrapworkCounterpointWeaverDSP.cpp, TawhidInfiniteSuspensionDSP.cpp; existing C4244 in Escher_ShiftDSP.cpp).

Amendments:
- 2026-05-05T04:43:49Z: IntermodulationWallGenerator smoothing and output level adjustments.
- 2026-05-05T04:46:06Z: NegativeSpaceDelay delay smoothing and fractional reads to reduce glitches.
- 2026-05-05T04:48:01Z: OceanicHaasWidener width/horizon smoothing and symmetric delay.
- 2026-05-05T04:52:38Z: InfrasonicDreadGenerator rework with evolving bed and drift (see conception log).
- 2026-05-06T18:58:46Z: ReluctantResolutionTemporalSuspension onset sensitivity and wet gain tuning.
- 2026-05-06T18:58:46Z: PhantomSubHarmonicExciter harmonic focus and output stabilization.
- 2026-05-06T18:58:46Z: StrapworkCounterpointWeaver weave depth and modulation tuning.
- 2026-05-06T19:02:11Z: TawhidInfiniteSuspension sustain tuning and audible suspension mix.
- 2026-05-06T19:02:11Z: ThreadOfMemoryMediantSplitter easier freeze triggers and higher wet presence.
- 2026-05-06T19:02:11Z: TransientsToTexture loudness reduction and gentler texture blend.
- 2026-05-05T04:55:10Z: ReluctantResolutionTemporalSuspension onset sensitivity and wet mix tuning.
- 2026-05-05T04:56:58Z: PhantomSubHarmonicExciter smoothing and harmonic shaping adjustments.
- 2026-05-05T04:58:28Z: StrapworkCounterpointWeaver cross-feedback weave rework (see conception log).
- 2026-05-05T04:58:28Z: TawhidInfiniteSuspension dual-head suspension rework (see conception log).
- 2026-05-05T05:00:47Z: ThreadOfMemoryMediantSplitter wet gain and trigger sensitivity tuning.
- 2026-05-05T05:01:24Z: TransientsToTexture deterministic noise and loudness reduction.
