Title: impl-doppler-shifted-room-fix
Date: 2026-05-06T00:00:00Z
Author: Seth Nenninger; Agent: GitHub Copilot (GPT-5.2-Codex)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Align Doppler-Shifted Room schema with DSP and fix LFO spread/caching.

Task Reference:
- User request: improve Doppler-Shifted Room implementation and alignment with concept.

Specification Summary:
- Align plugin schema and DSP parameter naming for room velocity.
- Fix stereo LFO phase offset usage and block-level parameter caching.

Implementation Notes:
- Updated examples/doppler_shifted_room/Doppler_Shifted_Room/plugin.json to include full doppler/reverb parameters.
- Updated schema/doppler_shifted_room.json to rename doppler_depth to room_velocity.
- Updated Doppler_Shifted_RoomDSP.cpp to use room_velocity, correct LFO spread, and block-based caching.

Evidence links:
- Tests: not run (not requested).
