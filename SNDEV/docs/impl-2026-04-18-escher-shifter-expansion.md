Title: impl-escher-pitch-shifter-expansion
Date: 2026-04-18T13:26:33Z
Author: Seth Nenninger (Gemini 3 Flash (Preview) Agent)
Contribution Type: Implementation
Ticket/Context: Expanding Escher Pitch Shifter for tritone exploitation
Summary: Added Paradox Filter, Substitution Injection, and Bifurcated Echo parameters.

Task Reference:
- User request to expand Escher Pitch Shifter methods for tritone exploitation.

Specification Summary:
- Added 3 new parameters to `plugin.json`.
- Updated `Escher_Pitch_ShifterDSP.cpp` to read these parameters and apply placeholder/stub logic for the new effects.

Implementation Notes:
- `Paradox Filter`: Added a HPF step to emulate octave ambiguity.
- `Substitution Injection`: Added a logic branch (stub) for spectral shift.
- `Bifurcated Echo`: Added a fixed tritone delay tap (placeholder for zigzagging patterns).
- Verified `plugin.json` includes the new IDs.

Evidence links:
- [plugin.json](../../examples/escher_shifter/Escher_Pitch_Shifter/plugin.json)
- [Escher_Pitch_ShifterDSP.cpp](../../examples/escher_shifter/Escher_Pitch_Shifter/src/Escher_Pitch_ShifterDSP.cpp)
