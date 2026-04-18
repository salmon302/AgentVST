Title: impl-2026-04-18-dechronicization-engine
Date: 2026-04-18T16:00:00Z
Author: GitHub Copilot
Contribution Type: Implementation
Ticket/Context: VSTS_SRS.md - Item 6
Summary: Implementation of the 'Dechronicization' Pre-Echo Engine
Task Reference: Item 6 in VSTS_SRS.md (Dechronicization Pre-Echo Engine)

Specification Summary:
Requires a reverse delay/pre-echo engine that functions as a "reverse polyrhythmic pre-echo", causing the listener to hear the backwards "ghost" of a note *before* the dry transient hits. Target parameters: Pre-Cognition (lookahead/delay latency), Temporal Conflict (ratio of reverse playback speed), and Ghost Mix.

Implementation Notes:
- Added `schema/dechronicization_engine.json` to define parameters.
- Implemented `Dechronicization_EngineDSP.cpp` in `examples/dechronicization/Dechronicization_Engine/src/`.
- Designed DSP to use a 5-second circular delay buffer.
- The "dry" signal is artificially delayed by `Pre-Cognition`.
- The "ghost" reverse signal plays backward using dual overlapping Hann-windowed grains that read backward from the live input pointer to exactly meet the dry signal timeline.
- Wired into root `/CMakeLists.txt`.