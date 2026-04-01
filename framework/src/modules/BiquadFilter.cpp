#include "BiquadFilter.h"
#include <cmath>
#include <algorithm>

namespace AgentVST {

BiquadFilter::BiquadFilter() {
    nodeType_ = "BiquadFilter";
    state_.fill({});
}

void BiquadFilter::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;
    state_.fill({});
    dirty_ = true;
}

void BiquadFilter::reset() {
    state_.fill({});
}

void BiquadFilter::setType(Type type) noexcept {
    if (type_ != type) { type_ = type; dirty_ = true; }
}

void BiquadFilter::setFrequency(float freqHz) noexcept {
    freqHz = std::clamp(freqHz, 10.0f, static_cast<float>(sampleRate_ * 0.4999));
    if (freqHz_ != freqHz) { freqHz_ = freqHz; dirty_ = true; }
}

void BiquadFilter::setQ(float q) noexcept {
    q = std::max(0.01f, q);
    if (q_ != q) { q_ = q; dirty_ = true; }
}

void BiquadFilter::setGainDb(float gainDb) noexcept {
    if (gainDb_ != gainDb) { gainDb_ = gainDb; dirty_ = true; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio Eq Cookbook (Robert Bristow-Johnson) coefficient formulas
// ─────────────────────────────────────────────────────────────────────────────

void BiquadFilter::computeCoefficients() noexcept {
    const float w0    = 2.0f * kPi * freqHz_ / static_cast<float>(sampleRate_);
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);
    const float alpha = sinw0 / (2.0f * q_);
    const float A     = std::pow(10.0f, gainDb_ / 40.0f); // linear amplitude for shelves/peak

    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a0 = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;

    switch (type_) {
    case Type::LowPass:
        b0 = (1.0f - cosw0) / 2.0f;
        b1 =  1.0f - cosw0;
        b2 = (1.0f - cosw0) / 2.0f;
        a0 =  1.0f + alpha;
        a1 = -2.0f * cosw0;
        a2 =  1.0f - alpha;
        break;

    case Type::HighPass:
        b0 =  (1.0f + cosw0) / 2.0f;
        b1 = -(1.0f + cosw0);
        b2 =  (1.0f + cosw0) / 2.0f;
        a0 =   1.0f + alpha;
        a1 =  -2.0f * cosw0;
        a2 =   1.0f - alpha;
        break;

    case Type::BandPass:
        b0 =  sinw0 / 2.0f;
        b1 =  0.0f;
        b2 = -sinw0 / 2.0f;
        a0 =  1.0f + alpha;
        a1 = -2.0f * cosw0;
        a2 =  1.0f - alpha;
        break;

    case Type::Notch:
        b0 =  1.0f;
        b1 = -2.0f * cosw0;
        b2 =  1.0f;
        a0 =  1.0f + alpha;
        a1 = -2.0f * cosw0;
        a2 =  1.0f - alpha;
        break;

    case Type::Peak:
        b0 =  1.0f + alpha * A;
        b1 = -2.0f * cosw0;
        b2 =  1.0f - alpha * A;
        a0 =  1.0f + alpha / A;
        a1 = -2.0f * cosw0;
        a2 =  1.0f - alpha / A;
        break;

    case Type::LowShelf:
        b0 =        A * ((A+1) - (A-1)*cosw0 + 2.0f*std::sqrt(A)*alpha);
        b1 =  2.0f * A * ((A-1) - (A+1)*cosw0);
        b2 =        A * ((A+1) - (A-1)*cosw0 - 2.0f*std::sqrt(A)*alpha);
        a0 =              (A+1) + (A-1)*cosw0 + 2.0f*std::sqrt(A)*alpha;
        a1 =       -2.0f*((A-1) + (A+1)*cosw0);
        a2 =              (A+1) + (A-1)*cosw0 - 2.0f*std::sqrt(A)*alpha;
        break;

    case Type::HighShelf:
        b0 =        A * ((A+1) + (A-1)*cosw0 + 2.0f*std::sqrt(A)*alpha);
        b1 = -2.0f * A * ((A-1) + (A+1)*cosw0);
        b2 =        A * ((A+1) + (A-1)*cosw0 - 2.0f*std::sqrt(A)*alpha);
        a0 =              (A+1) - (A-1)*cosw0 + 2.0f*std::sqrt(A)*alpha;
        a1 =        2.0f*((A-1) - (A+1)*cosw0);
        a2 =              (A+1) - (A-1)*cosw0 - 2.0f*std::sqrt(A)*alpha;
        break;

    default:
        break;
    }

    // Normalise
    coeffs_.b0 = b0 / a0;
    coeffs_.b1 = b1 / a0;
    coeffs_.b2 = b2 / a0;
    coeffs_.a1 = a1 / a0;
    coeffs_.a2 = a2 / a0;
    dirty_ = false;
}

float BiquadFilter::processSample(int channel, float input) {
    if (dirty_)
        computeCoefficients();

    const int ch = std::min(channel, kMaxChannels - 1);
    auto& st = state_[static_cast<std::size_t>(ch)];

    // Direct Form II Transposed
    const float output = coeffs_.b0 * input + st.s1;
    st.s1 = coeffs_.b1 * input - coeffs_.a1 * output + st.s2;
    st.s2 = coeffs_.b2 * input - coeffs_.a2 * output;

    return output;
}

} // namespace AgentVST
