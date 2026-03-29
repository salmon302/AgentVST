#include "Gain.h"
#include <cmath>
#include <algorithm>

namespace AgentVST {

Gain::Gain() {
    nodeType_ = "Gain";
    smoothedGain_.setCurrentAndTargetValue(1.0f);
}

void Gain::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;
    smoothedGain_.reset(sampleRate, smoothingMs_ / 1000.0);
    smoothedGain_.setCurrentAndTargetValue(targetGain_);
}

void Gain::reset() {
    smoothedGain_.setCurrentAndTargetValue(targetGain_);
}

void Gain::setGainLinear(float gain) noexcept {
    targetGain_ = std::max(0.0f, gain);
    smoothedGain_.setTargetValue(targetGain_);
}

void Gain::setGainDb(float dB) noexcept {
    setGainLinear(std::pow(10.0f, dB / 20.0f));
}

float Gain::getGainDb() const noexcept {
    if (targetGain_ <= 0.0f) return -96.0f;
    return 20.0f * std::log10(targetGain_);
}

void Gain::setSmoothingMs(float ms) noexcept {
    smoothingMs_ = std::max(0.0f, ms);
    if (sampleRate_ > 0.0)
        smoothedGain_.reset(sampleRate_, smoothingMs_ / 1000.0);
}

float Gain::processSample(int /*channel*/, float input) {
    return input * smoothedGain_.getNextValue();
}

} // namespace AgentVST
