#include "Oscillator.h"
#include <cmath>
#include <algorithm>

namespace AgentVST {

Oscillator::Oscillator() {
    nodeType_ = "Oscillator";
    phase_.fill(0.0f);
}

void Oscillator::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;
    updatePhaseIncrement();
    phase_.fill(0.0f);
}

void Oscillator::reset() {
    phase_.fill(0.0f);
}

void Oscillator::setFrequency(float freqHz) noexcept {
    freqHz_ = std::clamp(freqHz, 0.01f, static_cast<float>(sampleRate_ * 0.4999));
    updatePhaseIncrement();
}

void Oscillator::updatePhaseIncrement() noexcept {
    if (sampleRate_ > 0.0)
        phaseInc_ = freqHz_ / static_cast<float>(sampleRate_);
}

float Oscillator::polyBlep(float t, float dt) const noexcept {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float Oscillator::generateSample(float phase) const noexcept {
    switch (waveform_) {
    case Waveform::Sine:
        return std::sin(phase * kTwoPi);

    case Waveform::Sawtooth: {
        float saw = 2.0f * phase - 1.0f;
        saw -= polyBlep(phase, phaseInc_);
        return saw;
    }

    case Waveform::Square: {
        float sq = (phase < 0.5f) ? 1.0f : -1.0f;
        sq += polyBlep(phase, phaseInc_);
        sq -= polyBlep(std::fmod(phase + 0.5f, 1.0f), phaseInc_);
        return sq;
    }

    case Waveform::Triangle: {
        // Integrate a square wave for a triangle (no PolyBLEP needed)
        float sq = (phase < 0.5f) ? 1.0f : -1.0f;
        // Simple approximation: triangle from phase
        float tri = 2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f;
        (void)sq;
        return tri;
    }
    }
    return 0.0f;
}

float Oscillator::processSample(int channel, float input) {
    const int ch = std::min(channel, kMaxChannels - 1);
    auto& ph = phase_[static_cast<std::size_t>(ch)];

    const float out = generateSample(ph + phaseOffset_) * level_;

    ph += phaseInc_;
    if (ph >= 1.0f) ph -= 1.0f;

    return input + out; // Additive: adds generated signal to input
}

} // namespace AgentVST
