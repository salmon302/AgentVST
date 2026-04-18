Title: impl-varispeed-shepard-looper
Date: 2026-04-18T01:05:00Z
Author: Seth Nenninger; Agent: GitHub Copilot (Gemini 3.1 Pro (Preview))
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Setup, code-gen, and initial DSP implementation for The Varispeed Shepard Looper

Task Reference:
ad-hoc (from VSTS_SRS.md, Suite 2, Item 10)

Specification Summary:
Create a looper that captures a micro-loop and splits it into three granular paths (static, decelerating, accelerating). Includes complex feedback routing from the accelerating HF stream back into the mix.

Implementation Notes:
- Created the schema representation in `schema/varispeed_shepard_looper.json`. Defined `capture_size` (ms), `warp` (%), and `ouroboros_feedback` (%).
- Scaffolded new JUCE/AgentVST plugin directory using `scripts/generate.py`.
- Hooked `Varispeed_Shepard_Looper` directory into `CMakeLists.txt`.
- Re-wrote `Varispeed_Shepard_LooperDSP.cpp`
    - Implemented a circular `AudioBuffer` for tracking micro-loop recording and high-frequency shard feedback.
    - Created a lightweight `SpeedEngine` to vari-speed read overlapping windowed chunks from the buffer using linear interpolation.
    - Setup parallel engines reading from the same captured audio logic with separate read velocities calculated by the `warp_percent` multiplier, ranging from 1/10th speed up to 4x normal speed continuously.
    - Connected the output of the accelerating up-engine into the `HF_FeedbackBuffer`, mixing it directly back into the input buffer scaling by the `ouroborosFB` constant.
- Build passed on the plugin directory.

Evidence links:
Related conception file: N/A