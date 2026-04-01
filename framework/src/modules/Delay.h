/**
 * Delay.h — Pre-allocated feedback delay with fractional reads.
 *
 * Implements a per-channel circular buffer delay with linear interpolation.
 * All memory is allocated in prepare(); processSample() is allocation-free.
 */
#pragma once

#include "DSPNode.h"
#include <array>
#include <vector>

namespace AgentVST {

class Delay : public DSPNode {
public:
    Delay();

    void  prepare(double sampleRate, int maxBlockSize) override;
    float processSample(int channel, float input) override;
    void  reset() override;
    void  setParameter(const std::string& name, float value) override;

    void setDelayMs(float delayMs) noexcept;
    void setFeedback(float feedback) noexcept;
    void setMix(float mix) noexcept;
    void setMaxDelayMs(float maxDelayMs) noexcept;

    float getDelayMs() const noexcept { return delayMs_; }
    float getFeedback() const noexcept { return feedback_; }
    float getMix() const noexcept { return mix_; }

private:
    static constexpr int kMaxChannels = 8;

    float delayMs_    = 250.0f;
    float feedback_   = 0.35f;
    float mix_        = 0.35f;
    float maxDelayMs_ = 2000.0f;

    int delayBufferSize_ = 0;
    float delaySamples_  = 0.0f;

    std::vector<float> delayBuffer_;
    std::array<int, kMaxChannels> writePos_{};

    inline int offsetForChannel(int channel) const noexcept {
        return channel * delayBufferSize_;
    }

    void updateDelaySamples() noexcept;
};

inline void Delay::setParameter(const std::string& name, float value) {
    if (name == "delay_ms")       setDelayMs(value);
    else if (name == "feedback")  setFeedback(value);
    else if (name == "mix")       setMix(value);
    else if (name == "max_delay") setMaxDelayMs(value);
}

} // namespace AgentVST
