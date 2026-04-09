/**
 * EscherShifter.h — Shepard-tone pitch shifter
 *
 * Implements an auditory illusion of endless pitch shift using multiple
 * parallel pitch shifters and a Shepard-tone style crossfade.
 */
#pragma once

#include "DSPNode.h"
#include <vector>
#include <cmath>

namespace AgentVST {

class EscherShifter : public DSPNode {
public:
    EscherShifter();

    void  prepare(double sampleRate, int maxBlockSize) override;
    float processSample(int channel, float input) override;
    void  reset() override;
    void  setParameter(const std::string& name, float value) override;

    void setRate(float rate) noexcept;
    void setBlend(float blend) noexcept;
    void setFeedback(float feedback) noexcept;
    void setSpread(int spread) noexcept;

private:
    float rate_     = 1.0f;   // semitones per second
    float blend_    = 0.5f;   // dry/wet
    float feedback_ = 0.0f;   // recursive shift
    int   spread_   = 2;      // octave spread

    // Phase accumulator for the Shepard cycle
    double phase_ = 0.0;
    
    // We'll use a set of delay-based pitch shifters (simple grain-based or phase vocoder)
    // For MVP, we'll implement a simple dual-delay pitch shifter per octave.
    
    struct Voice {
        std::vector<float> delayBuffer;
        int writePos = 0;
        float currentShift = 0.0f;
    };

    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxVoices = 8;
    
    std::vector<Voice> voices_[kMaxChannels];
    int bufferSize_ = 0;

    void updateVoices() noexcept;
};

} // namespace AgentVST
