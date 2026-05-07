Title: impl-anti-loudness-slope
Date: 2026-05-06T23:55:00Z
Author: Seth Nenninger; Agent: GitHub Copilot (GPT-5.2-Codex)
Contribution Type: Implementation
Ticket/Context: ad-hoc user request
Summary: Tightened Anti-Loudness dB ranges and added transient slope control.

Task Reference:
ad-hoc user request (2026-05-06)

Specification Summary:
- Narrow dB ranges for more granular control.
- Add configurable transient slope (soft-knee) instead of a hard cut.

Implementation Notes:
- Updated schema and plugin.json to clamp `Sledgehammer` to 0-12 dB and `Tail Crushing` to -12-0 dB with 0.1 dB steps.
- Added `Transient Slope` parameter (1-12 dB, default 6).
- Reworked transient detection to use a soft-knee ramp around the 2x ratio threshold.

Evidence links:
Commits: N/A
PR: N/A
Related conception file: N/A
