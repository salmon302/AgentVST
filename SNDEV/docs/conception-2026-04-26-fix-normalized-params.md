Title: Fix normalized parameter values causing distorted audio across all VSTs
Date: 2026-04-26T18:00:00Z
Author: Seth Nenninger (GitHub Copilot Agent)
Contribution Type: Conception
Ticket/Context: ad-hoc — all VSTs have glitchy/distorted audio
Summary: APVTS getRawParameterValue() returns normalized 0.0–1.0 values, but DSP code expects un-normalized (real-world) values.

Problem:

Every VST plugin built with the AgentVST framework produces glitchy, distorted, or incorrect audio. Even the simplest plugins (SimpleGain, AgentEQ) are affected. The root cause is a fundamental mismatch between how JUCE's APVTS stores parameter values and how the agent DSP code reads them.

Constraints/Analysis:

1. JUCE's `AudioProcessorValueTreeState::getRawParameterValue()` returns a pointer to an `std::atomic<float>` that holds the **normalized** value (0.0 to 1.0), regardless of the parameter's actual range.
2. The `ParameterCache` stores these raw pointers and returns the normalized values directly via `getValue()`.
3. The `DSPContext::getParameter()` method returns these normalized values to the agent's DSP code.
4. Agent DSP code (e.g., `EQDSP.cpp`, `CompressorDSP.cpp`, `GainDSP.cpp`) treats these values as if they were in the real-world range defined in `plugin.json`.

Example with AgentEQ:
- `low_gain_db` has range -15.0 to 15.0 dB, default 0.0 dB
- Normalized: 0.0 dB → 0.5, -15.0 dB → 0.0, +15.0 dB → 1.0
- DSP code reads `ctx.getParameter("low_gain_db")` and gets 0.5 (normalized)
- DSP code then uses 0.5 as if it were 0.5 dB — but the intended default was 0.0 dB!
- At minimum slider position, DSP reads 0.0 and applies 0.0 dB gain (should be -15.0 dB)
- At maximum slider position, DSP reads 1.0 and applies 1.0 dB gain (should be +15.0 dB)

This means ALL parameters are compressed into a tiny 0.0–1.0 range, causing:
- EQ gains stuck near 0 dB (no actual EQ effect, just tiny gain changes)
- Compressor thresholds at 0.0–1.0 dB instead of -60 to 0 dB
- Frequency parameters at 0.0–1.0 Hz instead of 20–20000 Hz
- All DSP algorithms operating with completely wrong values

Proposed Solution (Conception):

Add a denormalization layer to the `ParameterCache` that converts normalized 0.0–1.0 values back to their real-world ranges. The solution requires:

1. Store the parameter range (min, max, skew) alongside each pointer in `ParameterCache::Entry`
2. Add a `denormalize()` method that applies the inverse of JUCE's normalization (including skew)
3. Modify `ParameterCache::getValue()` to return denormalized values
4. Modify `ParameterCache::copyValuesTo()` to denormalize before copying
5. Update `AgentVSTProcessor::initParameterCache()` to pass range information

The denormalization formula for a skewed parameter:
- JUCE normalizes: `normalized = pow((value - min) / (max - min), 1/skew)`
- Inverse (denormalize): `value = min + (max - min) * pow(normalized, skew)`

For non-skewed parameters (skew == 0 or skew == 1):
- `value = min + normalized * (max - min)`

For boolean parameters:
- `value = normalized >= 0.5f ? 1.0f : 0.0f`

Implementation notes:

- The `ParameterCache::registerParameter()` signature needs to change to accept min/max/skew/type
- The `AgentVSTProcessor::initParameterCache()` already has access to `schema_.parameters` which contains all range info
- This is a breaking change to the `ParameterCache` API but it's internal to the framework
- No changes needed to agent DSP code — they already call `ctx.getParameter()` which will now return correct values
