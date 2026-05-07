Title: strapwork-counterpoint-weave
Date: 2026-05-05T04:58:28Z
Author: Seth Nenninger; Agent: Seth Nenninger (GPT-5.2-Codex Agent)
Contribution Type: Conception
Ticket/Context: ad-hoc (QAFeedback Priority 2)
Summary: Rework StrapworkCounterpointWeaver to interlace strands via cross-feedback and fractional taps.
Problem:

StrapworkCounterpointWeaver currently produces modulated delay taps that sound interesting but do not convey an interwoven counterpoint effect.
Constraints/Analysis:

The existing parameters (knot_complexity, contrary_bias, over_under_ducking, mix) must be preserved. The DSP must remain real-time safe and avoid excessive CPU or instability.
Proposed Solution (Conception):

Implement a two-pass strand system: first compute fractional-delay taps for each strand with contrary-motion LFOs, then weave strands using cross-feedback between neighboring taps. Apply light damping on feedback to avoid runaway build-up, and use the existing ducking control to temper overlap. This creates audible interlacing lines while staying within the current parameter set.
Implementation notes (optional):

Use fractional delay reads for smooth motion, compute neighbor cross-gain from contrary_bias, and use a low-pass on per-strand feedback to keep the weave controlled.
