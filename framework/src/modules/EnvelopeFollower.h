/**
 * EnvelopeFollower.h — RMS/Peak envelope detector.
 * REAL-TIME SAFE.
 */
#pragma once

#include "DSPNode.h"
#include <array>

namespace AgentVST {

class EnvelopeFollower : public DSPNode {
public:
    enum class Mode { Peak, RMS };

    EnvelopeFollower();

    void  prepare(double sampleRate, int maxBlockSize) override;
    float processSample(int channel, float input) override;
    void  reset() override;
    void  setParameter(const std::string& name, float value) override;

    void  setAttackMs(float ms)  noexcept;
    void  setReleaseMs(float ms) noexcept;
    void  setMode(Mode mode)     noexcept { mode_ = mode; }

    float getEnvelopeLevel(int channel) const noexcept;

private:
    static constexpr int kMaxChannels = 8;
    Mode  mode_      = Mode::Peak;
    float attackMs_  = 10.0f;
    float releaseMs_ = 100.0f;
    float attackCoeff_  = 0.0f;
    float releaseCoeff_ = 0.0f;

    std::array<float, kMaxChannels> envState_{};
    std::array<float, kMaxChannels> rmsState_{};

    void computeCoeffs() noexcept;
};

inline void EnvelopeFollower::setParameter(const std::string& name, float value) {
    if      (name == "attack")  setAttackMs(value);
    else if (name == "release") setReleaseMs(value);
    else if (name == "mode")    setMode(static_cast<Mode>(static_cast<int>(value)));
}

} // namespace AgentVST
