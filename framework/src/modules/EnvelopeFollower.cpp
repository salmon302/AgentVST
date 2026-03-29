#include "EnvelopeFollower.h"
#include <cmath>
#include <algorithm>

namespace AgentVST {

EnvelopeFollower::EnvelopeFollower() {
    nodeType_ = "EnvelopeFollower";
    envState_.fill(0.0f);
    rmsState_.fill(0.0f);
}

void EnvelopeFollower::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;
    computeCoeffs();
    reset();
}

void EnvelopeFollower::reset() {
    envState_.fill(0.0f);
    rmsState_.fill(0.0f);
}

void EnvelopeFollower::setAttackMs(float ms) noexcept {
    attackMs_ = std::max(0.01f, ms);
    computeCoeffs();
}

void EnvelopeFollower::setReleaseMs(float ms) noexcept {
    releaseMs_ = std::max(0.01f, ms);
    computeCoeffs();
}

void EnvelopeFollower::computeCoeffs() noexcept {
    if (sampleRate_ <= 0.0) return;
    attackCoeff_  = std::exp(-1.0f / (static_cast<float>(sampleRate_) * attackMs_  * 0.001f));
    releaseCoeff_ = std::exp(-1.0f / (static_cast<float>(sampleRate_) * releaseMs_ * 0.001f));
}

float EnvelopeFollower::processSample(int channel, float input) {
    const int ch = std::min(channel, kMaxChannels - 1);

    if (mode_ == Mode::RMS) {
        // Leaky integrator of squared signal
        const float coeff = releaseCoeff_;
        rmsState_[static_cast<std::size_t>(ch)] =
            coeff * rmsState_[static_cast<std::size_t>(ch)] + (1.0f - coeff) * input * input;
        envState_[static_cast<std::size_t>(ch)] =
            std::sqrt(rmsState_[static_cast<std::size_t>(ch)]);
    } else {
        // Peak detector with separate attack/release
        const float absIn = std::abs(input);
        auto& env = envState_[static_cast<std::size_t>(ch)];
        const float coeff = (absIn > env) ? attackCoeff_ : releaseCoeff_;
        env = coeff * env + (1.0f - coeff) * absIn;
    }

    return envState_[static_cast<std::size_t>(ch)];
}

float EnvelopeFollower::getEnvelopeLevel(int channel) const noexcept {
    const int ch = std::min(channel, kMaxChannels - 1);
    return envState_[static_cast<std::size_t>(ch)];
}

} // namespace AgentVST
