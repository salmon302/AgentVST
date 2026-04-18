Title: impl-sensory-dissonance-smart-fuzz
Date: 2026-04-18T12:30:00Z
Author: Seth Nenninger (GitHub Copilot Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Developing Sensory Dissonance Smart-Fuzz (Exciter)

Task Reference:
Ad-hoc user request "Next, 4. (Roughness) Exciter / Sensory Dissonance Smart-Fuzz"

Specification Summary:
Implement single-sideband modulation targeting highly dissonant intervals, generating sidebands at (e.g. f * 45/32) to maximize roughness/beating (around 30Hz).
Parameters: Roughness Hz, Smart Threshold, Dissonance Injection, and optional Fuzz Drive.

Implementation Notes:
- Defined JSON schema `sensory_dissonance_fuzz.json` for Roughness Hz, Smart Threshold, Dissonance Injection, Fuzz Drive.
- Scaffolded standard C++ DSP logic using AgentDSP.
- Used a simple pitch tracker / amplitude threshold approach mixed with wave-folding/ring-modulation for the sideband generation, as full polyphonic interval detection without FFT might exceed real-time limits for this MVP. We approximate the roughness by creating an amplitude-dependent inter-modulated signal that folds hard when complex signals surpass the `smart_threshold`.

Evidence links:
N/A
