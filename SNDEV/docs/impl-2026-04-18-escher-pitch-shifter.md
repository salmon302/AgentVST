Title: impl-escher-pitch-shifter
Date: 2026-04-18T12:00:00Z
Author: Seth Nenninger (GitHub Copilot Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Developing Escher Pitch Shifter to match SRS spec

Task Reference:
Ad-hoc user request "Begin development for 1. Escher Pitch Shifter (The Endless Tritone)"

Specification Summary:
Implement dual granular pitch shifters driven by counter-rotating sawtooth LFOs.
Parameters: Shift Cycle, Tension/Lock, Asymmetry.

Implementation Notes:
- Updated `schema/escher_pitch_shifter.json` to properly reflect parameters.
- Overwrote/updated `Escher_Pitch_ShifterDSP.cpp` to correctly apply Shepard-tone envelope and tritone shift. Added Tension/Lock (semitone stepping vs smooth glide) and Asymmetry (favor rising/falling).

Evidence links:
N/A
