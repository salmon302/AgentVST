Title: infrasonic-dread-bed
Date: 2026-05-05T04:52:38Z
Author: Seth Nenninger; Agent: Seth Nenninger (GPT-5.2-Codex Agent)
Contribution Type: Conception
Ticket/Context: ad-hoc (QAFeedback Priority 2)
Summary: Rework InfrasonicDreadGenerator into an evolving infrasonic bed driven by impact envelopes.
Problem:

The current InfrasonicDreadGenerator output is a static sine burst, yielding an uninteresting and flat effect that does not convey evolving "dread".
Constraints/Analysis:

Existing parameters must be preserved (dread_frequency, impact_sensitivity, pressure_amount). The generator should remain real-time safe and stable, avoid excessive loudness, and produce a perceptible but controlled infrasonic presence.
Proposed Solution (Conception):

Generate a layered infrasonic bed combining a base oscillator, a subharmonic oscillator, and filtered low-frequency noise. Use the existing envelope follower to drive a pressure envelope with slower release, then modulate amplitude and subtle frequency drift with a low-rate random walk and a slow tremolo. Apply soft saturation and output scaling for stability. This keeps the parameter set unchanged while creating a more organic, evolving dread texture that responds to low-end impacts.
Implementation notes (optional):

Add an LCG-based noise source and random-walk drift updated every N samples. Use precomputed envelope coefficients and a one-pole lowpass for rumble. Clamp drift and frequency to safe ranges and keep generator state shared across channels.
