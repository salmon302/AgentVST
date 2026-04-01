#include "Delay.h"
#include <algorithm>
#include <cmath>

namespace AgentVST {

Delay::Delay() {
    nodeType_ = "Delay";
    writePos_.fill(0);
}

void Delay::setDelayMs(float delayMs) noexcept {
    delayMs_ = std::clamp(delayMs, 0.0f, maxDelayMs_);
    updateDelaySamples();
}

void Delay::setFeedback(float feedback) noexcept {
    feedback_ = std::clamp(feedback, 0.0f, 0.999f);
}

void Delay::setMix(float mix) noexcept {
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

void Delay::setMaxDelayMs(float maxDelayMs) noexcept {
    maxDelayMs_ = std::max(1.0f, maxDelayMs);
    delayMs_ = std::min(delayMs_, maxDelayMs_);
    updateDelaySamples();
}

void Delay::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;

    // +2 for interpolation safety margin.
    delayBufferSize_ = std::max(2,
        static_cast<int>(std::ceil((maxDelayMs_ * 0.001f) * static_cast<float>(sampleRate_))) + 2);

    delayBuffer_.assign(static_cast<std::size_t>(kMaxChannels * delayBufferSize_), 0.0f);
    writePos_.fill(0);
    updateDelaySamples();
}

void Delay::reset() {
    std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
    writePos_.fill(0);
}

void Delay::updateDelaySamples() noexcept {
    if (sampleRate_ <= 0.0) {
        delaySamples_ = 0.0f;
        return;
    }

    const float maxSamples = static_cast<float>(std::max(1, delayBufferSize_ - 2));
    float samples = (delayMs_ * 0.001f) * static_cast<float>(sampleRate_);
    delaySamples_ = std::clamp(samples, 0.0f, maxSamples);
}

float Delay::processSample(int channel, float input) {
    if (delayBuffer_.empty() || delayBufferSize_ <= 1)
        return input;

    const int ch = std::clamp(channel, 0, kMaxChannels - 1);
    const int base = offsetForChannel(ch);
    int& write = writePos_[static_cast<std::size_t>(ch)];

    float readPos = static_cast<float>(write) - delaySamples_;
    while (readPos < 0.0f)
        readPos += static_cast<float>(delayBufferSize_);

    int read0 = static_cast<int>(readPos);
    int read1 = read0 + 1;
    if (read1 >= delayBufferSize_)
        read1 -= delayBufferSize_;

    const float frac = readPos - static_cast<float>(read0);
    const float delayed0 = delayBuffer_[static_cast<std::size_t>(base + read0)];
    const float delayed1 = delayBuffer_[static_cast<std::size_t>(base + read1)];
    const float delayed = delayed0 + frac * (delayed1 - delayed0);

    // Standard feed-back delay write and wet/dry mix.
    delayBuffer_[static_cast<std::size_t>(base + write)] = input + delayed * feedback_;
    const float output = input * (1.0f - mix_) + delayed * mix_;

    ++write;
    if (write >= delayBufferSize_)
        write = 0;

    return output;
}

} // namespace AgentVST
