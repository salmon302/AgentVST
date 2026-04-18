Title: impl-material-decay-resonator
Date: 2026-04-18T13:00:00Z
Author: Seth Nenninger (GitHub Copilot Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Developing Material Decay Resonator

Task Reference:
Ad-hoc user request "### 5. The 'Material Decay' Resonator"

Specification Summary:
Implement physical modeling simulating resonant objects (glass, sheet metal, wood).
An envelope follower drives a "rot" parameter that actively degrades the material state over time (decay rate).
Structural Failure parameter adds chaotic noise/crackle at the tail end of the decay.

Implementation Notes:
- Defined JSON schema `material_decay_resonator.json` for Material Type, Decay Rate (Rot), Structural Failure, Dry/Wet Mix.
- Scaffolded standard C++ DSP logic using short-delay waveguide/comb-filters with decaying feedback. The envelope tracks the input and reduces the feedback / filters high frequencies over time.
- Structural failure dynamically injects chaotic noise based on the decay envelope entering its trailing edge.

Evidence links:
N/A
