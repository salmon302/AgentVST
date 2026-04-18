Title: impl-2026-04-18-bimodal-weight-eq
Date: 2026-04-18T12:00:00Z
Author: GitHub Copilot
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Developed "21. The 'Bimodal Weight' Fletcher-Munson EQ" module.

Task Reference:
Implement specific conceptual module.

Specification Summary:
Implement the Bimodal Weight Fletcher-Munson EQ, which acts as a dynamic EQ adjusting frequencies inversely aligned with equal-loudness contours based on input signal decays.

Implementation Notes:
Created schema `schema/bimodal_weight_fletcher_munson_eq.json` with Reference Loudness target, Compensation Depth, and Weight/Bite Bias. Scaffolded out the C++ using `generate.py`. Bypassed a path-limit failure (LNK1104, MAX_PATH limit hit from redundant nested directory paths >260chars) by shortening the target name cleanly back to `BimodalEQ` in CMake. 

DSP details: Engineered a dynamic dual-shelf inverse tracking design responding dynamically to a 50ms-smoothed envelope tracking the RMS levels of the audio. Boost scales up inversely relative to signal devation from the reference LUFS target. Built and verified `BimodalEQ_VST3` MSBuild target natively on Windows.

Evidence links:
N/A
