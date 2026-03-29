/**
 * Gain.h — Simple gain module with parameter smoothing.
 *
 * Applies a gain factor to the incoming signal.
 * Uses juce::SmoothedValue to prevent zipper noise during rapid gain changes.
 *
 * REAL-TIME SAFE.
 */
#pragma once

#include "DSPNode.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace AgentVST {

class Gain : public DSPNode {
public:
    Gain();

    void  prepare(double sampleRate, int maxBlockSize) override;
    float processSample(int channel, float input) override;
    void  reset() override;
    void  setParameter(const std::string& name, float value) override;

    /// Set gain as a linear multiplier (e.g. 1.0 = unity, 2.0 = +6dB)
    void setGainLinear(float gain) noexcept;

    /// Set gain in decibels
    void setGainDb(float dB) noexcept;

    float getGainLinear() const noexcept { return targetGain_; }
    float getGainDb()     const noexcept;

    /// Ramp time in milliseconds for parameter smoothing (default: 20ms)
    void setSmoothingMs(float ms) noexcept;

private:
    float targetGain_    = 1.0f;
    float smoothingMs_   = 20.0f;

    // Shared SmoothedValue — gain is the same for all channels
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain_;
};

inline void Gain::setParameter(const std::string& name, float value) {
    if (name == "gain_db")     setGainDb(value);
    else if (name == "gain")   setGainLinear(value);
    else if (name == "smooth") setSmoothingMs(value);
}

} // namespace AgentVST
