Title: tawhid-suspension-grains
Date: 2026-05-05T04:58:28Z
Author: Seth Nenninger; Agent: Seth Nenninger (GPT-5.2-Codex Agent)
Contribution Type: Conception
Ticket/Context: ad-hoc (QAFeedback Priority 2)
Summary: Rework TawhidInfiniteSuspension into a dual-head suspended buffer with glide-controlled crossfades.
Problem:

TawhidInfiniteSuspension currently uses a single random read head and ignores tension_selection and resolution_glide, yielding an effect that is interesting but not aligned with the intended infinite suspension concept.
Constraints/Analysis:

Existing parameters must be preserved. The processor must remain real-time safe while providing a smooth, controllable suspension without clicks.
Proposed Solution (Conception):

Introduce two read heads over the circular buffer and crossfade between them when a new target position is chosen. Use suspension_gravity to slow the read rate and reduce update probability, tension_selection to choose how far back the target head jumps, and resolution_glide to set the crossfade duration. This keeps stereo coherence while producing a clear, continuous suspended texture.
Implementation notes (optional):

Use deterministic LCG randomness, fractional buffer reads, and wrap logic for read positions. Apply crossfade updates only from a single channel to keep state synchronized.
