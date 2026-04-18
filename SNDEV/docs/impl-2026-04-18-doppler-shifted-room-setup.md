Title: impl-doppler-shifted-room-setup
Date: 2026-04-18T00:54:00Z
Author: Seth Nenninger; Agent: GitHub Copilot (Gemini 3.1 Pro (Preview))
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Setup, code-gen, and initial DSP implementation for The Doppler-Shifted Room plugin

Task Reference:
ad-hoc (from VSTS_SRS.md, Suite 2, Item 7)

Specification Summary:
Implement a reverb simulator where the boundary walls are moving, causing a continuous Doppler shift on reverberant reflections. Target params include Room Velocity, Expansion Cycle, Acoustic Mass, and Mix. 

Implementation Notes:
- Created the schema representation in `schema/doppler_shifted_room.json` representing all defined target parameters.
- Ran the `generate.py` python script (after resolving python dependencies) to scaffold `examples/doppler_shifted_room/Doppler_Shifted_Room`.
- Hooked the child directory into the root `CMakeLists.txt` build graph.
- Rewrote the generated `Doppler_Shifted_RoomDSP.cpp` inside the plugin's `src` folder. Replaced the pass-through method with a linear interpolation delay line, modulated comb filter logic, series allpass filters (basic Schroeder reverb network structure), and linked the generated delay times to the 'room_velocity' and 'expansion_cycle' parameters using a smoothed low-frequency sine-wave oscillator.
- Verified build and syntax resolution inside the root structure. (Also resolved unrelated duplicate definition issues inadvertently triggered during file rewrites).

Evidence links:
Related conception file: N/A