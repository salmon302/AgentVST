/**
 * Devils_Grid_Irrational_DelayDSP.cpp
 *
 * Tempo-synced feedback delay where delay time is multiplied by either
 * sqrt(2) or 1/sqrt(2), producing repeats that do not periodically realign
 * with rational beat divisions.
 *
 * Real-time safety:
 * - All buffers are allocated in prepare().
 * - processSample() performs no allocation, no locks, and no I/O.
 */
#include <AgentDSP.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

class Devils_Grid_Irrational_DelayProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = std::max(8000.0, sampleRate);
        maxDelaySamples_ = std::max(2,
            static_cast<int>(std::ceil(sampleRate_ * kMaxDelaySeconds)) + 4);

        delayBuffer_.assign(static_cast<std::size_t>(kMaxChannels * maxDelaySamples_), 0.0f);
        writePos_.fill(0);
        delaySamples_.fill(1.0f);

        sampleStamp_ = -1;
        baseDelaySamples_ = static_cast<float>(sampleRate_ * 0.35);
        feedback_ = 0.45f;
        mix_ = 0.35f;
        stereoOffset_ = 0.06f;
        driftAmount_ = 0.02f;
        driftRateHz_ = 0.25f;
        lfoPhase_ = 0.0f;
        updateDelaySmoothing();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f)
            return input;

        if (delayBuffer_.empty() || maxDelaySamples_ <= 2)
            return input;

        const int ch = std::clamp(channel, 0, kMaxChannels - 1);

        if (channel == 0 && sampleStamp_ != ctx.currentSample) {
            sampleStamp_ = ctx.currentSample;
            updateSharedTiming(ctx);
            advanceLfo();
        }

        const float targetDelay = computeTargetDelaySamples(ch);
        float& currentDelay = delaySamples_[static_cast<std::size_t>(ch)];
        currentDelay = delaySmoothingCoeff_ * currentDelay
                     + (1.0f - delaySmoothingCoeff_) * targetDelay;

        const float delayed = readDelayedSample(ch, currentDelay);
        writeInputWithFeedback(ch, input, delayed);

        const float dry = input * (1.0f - mix_);
        const float wet = delayed * mix_;
        return dry + wet;
    }

    void reset() override {
        std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
        writePos_.fill(0);

        const float initDelay = std::clamp(
            baseDelaySamples_,
            1.0f,
            static_cast<float>(std::max(2, maxDelaySamples_ - 2)));
        delaySamples_.fill(initDelay);

        lfoPhase_ = 0.0f;
        sampleStamp_ = -1;
    }

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kMaxDelaySeconds = 8.0f;
    static constexpr float kSqrt2 = 1.41421356237f;
    static constexpr float kPhi = 1.61803398875f;
    static constexpr float kPi = 3.14159265359f;
    static constexpr float kTwoPi = 6.28318530718f;

    static constexpr std::array<float, 6> kQuarterNoteMultipliers {
        4.0f,
        2.0f,
        1.0f,
        0.5f,
        0.25f,
        1.0f / 3.0f
    };

    std::vector<float> delayBuffer_;
    std::array<int, kMaxChannels> writePos_{};
    std::array<float, kMaxChannels> delaySamples_{};

    double sampleRate_ = 44100.0;
    int maxDelaySamples_ = 0;
    int sampleStamp_ = -1;

    float baseDelaySamples_ = 22050.0f;
    float feedback_ = 0.45f;
    float mix_ = 0.35f;
    float stereoOffset_ = 0.06f;
    float driftAmount_ = 0.02f;
    float driftRateHz_ = 0.25f;
    float delaySmoothingCoeff_ = 0.0f;
    float lfoPhase_ = 0.0f;

    int channelOffset(int channel) const noexcept {
        return channel * maxDelaySamples_;
    }

    void updateDelaySmoothing() noexcept {
        const float sr = static_cast<float>(sampleRate_);
        delaySmoothingCoeff_ = std::exp(-1.0f / (0.03f * sr));
    }

    void updateSharedTiming(const AgentVST::DSPContext& ctx) noexcept {
        const int noteIdx = std::clamp(
            static_cast<int>(ctx.getParameter("note_division")),
            0,
            static_cast<int>(kQuarterNoteMultipliers.size()) - 1);

        const int modeIdx = std::clamp(
            static_cast<int>(ctx.getParameter("irrational_mode")),
            0,
            2);

        const float bpmRaw = static_cast<float>(ctx.bpm);
        const float bpm = std::clamp((bpmRaw > 1.0f ? bpmRaw : 120.0f), 20.0f, 320.0f);
        const float quarterSeconds = 60.0f / bpm;
        
        float irrational = kSqrt2;
        if (modeIdx == 1) irrational = kPhi;
        else if (modeIdx == 2) irrational = kPi;
        
        const float timeScale = std::clamp(ctx.getParameter("time_scale"), 0.25f, 4.0f);

        float delaySeconds = quarterSeconds
                           * kQuarterNoteMultipliers[static_cast<std::size_t>(noteIdx)]
                           * irrational
                           * timeScale;

        const float minSeconds = 1.0f / static_cast<float>(sampleRate_);
        const float maxSeconds = static_cast<float>(maxDelaySamples_ - 2)
                               / static_cast<float>(sampleRate_);
        delaySeconds = std::clamp(delaySeconds, minSeconds, maxSeconds);

        baseDelaySamples_ = delaySeconds * static_cast<float>(sampleRate_);
        feedback_ = std::clamp(ctx.getParameter("feedback"), 0.0f, 0.95f);
        mix_ = std::clamp(ctx.getParameter("mix"), 0.0f, 1.0f);
        stereoOffset_ = std::clamp(ctx.getParameter("stereo_offset"), 0.0f, 0.25f);
        driftAmount_ = std::clamp(ctx.getParameter("drift_amount"), 0.0f, 0.1f);
        driftRateHz_ = std::clamp(ctx.getParameter("drift_rate_hz"), 0.02f, 2.0f);
    }

    void advanceLfo() noexcept {
        const float increment = kTwoPi * driftRateHz_ / static_cast<float>(sampleRate_);
        lfoPhase_ += increment;
        if (lfoPhase_ >= kTwoPi)
            lfoPhase_ -= kTwoPi;
    }

    float computeTargetDelaySamples(int channel) const noexcept {
        const float side = (channel & 1) ? 1.0f : -1.0f;
        float target = baseDelaySamples_ * (1.0f + side * stereoOffset_);

        const float phaseOffset = (channel & 1) ? (0.5f * kTwoPi) : 0.0f;
        const float drift = std::sin(lfoPhase_ + phaseOffset);
        target *= (1.0f + driftAmount_ * drift);

        return std::clamp(
            target,
            1.0f,
            static_cast<float>(std::max(2, maxDelaySamples_ - 2)));
    }

    float readDelayedSample(int channel, float delayInSamples) const noexcept {
        const int base = channelOffset(channel);
        const int write = writePos_[static_cast<std::size_t>(channel)];

        float readPos = static_cast<float>(write) - delayInSamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(maxDelaySamples_);
        while (readPos >= static_cast<float>(maxDelaySamples_))
            readPos -= static_cast<float>(maxDelaySamples_);

        int i0 = static_cast<int>(readPos);
        const float frac = readPos - static_cast<float>(i0);

        int im1 = i0 - 1;
        if (im1 < 0) im1 += maxDelaySamples_;

        int i1 = i0 + 1;
        if (i1 >= maxDelaySamples_) i1 -= maxDelaySamples_;

        int i2 = i0 + 2;
        if (i2 >= maxDelaySamples_) i2 -= maxDelaySamples_;

        const float y_m1 = delayBuffer_[static_cast<std::size_t>(base + im1)];
        const float y_0  = delayBuffer_[static_cast<std::size_t>(base + i0)];
        const float y_1  = delayBuffer_[static_cast<std::size_t>(base + i1)];
        const float y_2  = delayBuffer_[static_cast<std::size_t>(base + i2)];

        const float c0 = y_0;
        const float c1 = 0.5f * (y_1 - y_m1);
        const float c2 = y_m1 - 2.5f * y_0 + 2.0f * y_1 - 0.5f * y_2;
        const float c3 = 1.5f * (y_0 - y_1) + 0.5f * (y_2 - y_m1);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    void writeInputWithFeedback(int channel, float input, float delayed) noexcept {
        const int base = channelOffset(channel);
        int& write = writePos_[static_cast<std::size_t>(channel)];

        delayBuffer_[static_cast<std::size_t>(base + write)] = input + delayed * feedback_;

        ++write;
        if (write >= maxDelaySamples_)
            write = 0;
    }
};

AGENTVST_REGISTER_DSP(Devils_Grid_Irrational_DelayProcessor)