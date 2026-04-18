Title: impl-negative-space-delay
Date: 2026-04-18T12:00:00Z
Author: Seth Nenninger (Gemini 3.1 Pro (Preview) Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Implement The Negative Space (Anti-Masking) Delay from VSTS_SRS.md
Task Reference:
Ad-hoc user request for #14.

Specification Summary:
Implement the #14 The Negative Space (Anti-Masking) Delay from the conceptual suites. The plugin requires notch/ducking whatever frequencies are prominent in the dry signal from the delayed wet signal. 

Implementation Notes:
Created schema schema/negative_space_delay.json.
Generated the plugin scaffolding using the python script.
Implemented a 3-band (Low, Mid, High) STFT-less approach by employing parallel Linkwitz-Riley biquads. Separate biquads track the dry envelopes while corresponding ducking values are applied directly to the parallel wet/delayed signals.
Mixed back appropriately with standard delay timings and feedback controls.
Tested building the VST3 target properly.

Evidence links:
N/A
