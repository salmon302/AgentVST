Title: impl-intermodulation-wall
Date: 2026-04-18T12:00:00Z
Author: Seth Nenninger (Gemini 3.1 Pro (Preview) Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Implement The Intermodulation Wall Generator from VSTS_SRS.md
Task Reference:
Ad-hoc user request

Specification Summary:
Implement the #12 Intermodulation Wall Generator from the SRS. This includes a reverse reverb swelling logic, massive clipping/limiting block, and sidechain ducking.

Implementation Notes:
Added schema schema/intermodulation_wall_generator.json.
Generated JUCE plugin.
Included examples/intermodulation_wall/IntermodulationWallGenerator.
Implemented simple Schroeder comb/allpass cascade for reverb/swell.
Added 	anh hard clipper for the wall density.
Added sidechain input envelope follower for breathing gate.

Evidence links:
N/A
