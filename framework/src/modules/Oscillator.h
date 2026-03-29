/**
 * Oscillator.h — PolyBLEP anti-aliased oscillator.
 * Generates sine, sawtooth, square, and triangle waveforms.
 * REAL-TIME SAFE.
 */
#pragma once

#include "DSPNode.h"
#include <array>

namespace AgentVST {

class Oscillator : public DSPNode {
public:
    enum class Waveform { Sine, Sawtooth, Square, Triangle };

    Oscillator();

    void  prepare(double sampleRate, int maxBlockSize) override;
    float processSample(int channel, float input) override;
    void  reset() override;
    void  setParameter(const std::string& name, float value) override;

    void setFrequency(float freqHz) noexcept;
    void setWaveform (Waveform w)   noexcept { waveform_ = w; }
    void setLevel    (float level)  noexcept { level_ = level; }
    void setPhaseOffset(float rad)  noexcept { phaseOffset_ = rad; }

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kTwoPi    = 6.28318530717958647692f;

    Waveform waveform_    = Waveform::Sine;
    float    freqHz_      = 440.0f;
    float    level_       = 1.0f;
    float    phaseOffset_ = 0.0f;
    float    phaseInc_    = 0.0f;

    std::array<float, kMaxChannels> phase_{};

    float polyBlep(float phase, float phaseInc) const noexcept;
    float generateSample(float phase) const noexcept;
    void  updatePhaseIncrement() noexcept;
};

inline void Oscillator::setParameter(const std::string& name, float value) {
    if      (name == "frequency") setFrequency(value);
    else if (name == "waveform")  setWaveform(static_cast<Waveform>(static_cast<int>(value)));
    else if (name == "level")     setLevel(value);
}

} // namespace AgentVST
