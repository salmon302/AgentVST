Title: impl-anti-loudness-transient-restorer
Date: 2026-04-18T10:30:00Z
Author: GitHub Copilot
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Developed Anti-Loudness Transient Restorer Suite 4 module.

Task Reference:
ad-hoc user request

Specification Summary:
Implement "20. The Anti-Loudness Transient Restorer".
Concept: An aggressive upward expander specifically tuned to attack transients, undoing the damage of flat, over-compressed heavy tones from the Loudness War.

Implementation Notes:
Created schema `schema/anti_loudness_transient_restorer.json` defining sledgehammer gain, attack milliseconds, and tail crushing. Generated via `generate.py` into `examples/anti_loudness_transient_restorer`.
Implemented DSP processing utilizing dual envelope following for robust transient detection, mapping sledgehammer gain selectively on sharp transients while applying tail crushing inversely, ensuring heavily compressed elements punch through seamlessly without blowing out audio. Integrated into `CMakeLists.txt` examples configuration. Verified correct compilation logic under correct targets.

Evidence links:
N/A
