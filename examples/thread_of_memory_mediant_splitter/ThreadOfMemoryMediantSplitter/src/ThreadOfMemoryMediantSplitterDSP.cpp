/**
 * ThreadOfMemoryMediantSplitterDSP.cpp
 *
 * "Thread of Memory" mediant splitter:
 * - Detects significant pitch jumps as harmonic transitions.
 * - Freezes a low-frequency common-tone layer in a short loop.
 * - Pitch-shifts the remaining layer by either 6:5 or 5:4.
 *
 * Real-time safety:
 * - All memory allocated in prepare().
 * - processSample() performs no allocation, locks, or I/O.
 */
#include <AgentDSP.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

class ThreadOfMemoryMediantSplitterProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = std::max(8000.0, sampleRate);
        bufferSize_ = std::max(512,
            static_cast<int>(std::ceil(sampleRate_ * kMaxBufferSeconds)) + 8);

        for (int ch = 0; ch < kMaxChannels; ++ch) {
            lowBuffer_[ch].assign(static_cast<std::size_t>(bufferSize_), 0.0f);
            highBuffer_[ch].assign(static_cast<std::size_t>(bufferSize_), 0.0f);
            writePos_[ch] = 0;
            freezeStartPos_[ch] = 0;
            lowState_[ch] = 0.0f;
            lastGhost_[ch] = 0.0f;
        }

        analysisBuffer_.assign(static_cast<std::size_t>(bufferSize_), 0.0f);
        analysisWritePos_ = 0;

        sampleStamp_ = -1;
        analysisCounter_ = 0;
        envelope_ = 0.0f;
        trackedHz_ = 0.0f;
        previousTrackedHz_ = 0.0f;

        freezeActive_ = false;
        freezePhase_ = 0.0f;
        freezeLengthSamples_ = std::clamp(
            static_cast<int>(sampleRate_ * 0.35),
            64,
            bufferSize_ - 4);
        freezeRemainingSamples_ = 0;

        pitchPhase_ = 0.0f;
        lowpassAlpha_ = 0.95f;

        updateDerivedState();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel < 0 || channel >= kMaxChannels)
            return input;

        if (channel == 0 && sampleStamp_ != ctx.currentSample) {
            sampleStamp_ = ctx.currentSample;
            analysisBuffer_[static_cast<std::size_t>(analysisWritePos_)] = input;
            ++analysisWritePos_;
            if (analysisWritePos_ >= bufferSize_)
                analysisWritePos_ = 0;
            runSharedFrame(ctx, input);
        }

        const int ch = channel;
        const int write = writePos_[static_cast<std::size_t>(ch)];

        const float low = lowSplit(ch, input);
        const float high = input - low;

        lowBuffer_[static_cast<std::size_t>(ch)][static_cast<std::size_t>(write)] = low;
        highBuffer_[static_cast<std::size_t>(ch)][static_cast<std::size_t>(write)] = high;

        const float ghostTone = computeGhost(ch);
        const float shiftedHarmony = computeShifted(ch);

        const float wet = shiftedHarmony * (1.0f - ghostBlend_)
                        + ghostTone * ghostBlend_;
        const float output = input * (1.0f - outputWet_)
                           + wet * outputWet_;

        int nextWrite = write + 1;
        if (nextWrite >= bufferSize_)
            nextWrite = 0;
        writePos_[static_cast<std::size_t>(ch)] = nextWrite;

        return output;
    }

    void reset() override {
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(lowBuffer_[static_cast<std::size_t>(ch)].begin(),
                      lowBuffer_[static_cast<std::size_t>(ch)].end(),
                      0.0f);
            std::fill(highBuffer_[static_cast<std::size_t>(ch)].begin(),
                      highBuffer_[static_cast<std::size_t>(ch)].end(),
                      0.0f);
            writePos_[static_cast<std::size_t>(ch)] = 0;
            freezeStartPos_[static_cast<std::size_t>(ch)] = 0;
            lowState_[static_cast<std::size_t>(ch)] = 0.0f;
            lastGhost_[static_cast<std::size_t>(ch)] = 0.0f;
        }

        std::fill(analysisBuffer_.begin(), analysisBuffer_.end(), 0.0f);
        analysisWritePos_ = 0;

        sampleStamp_ = -1;
        analysisCounter_ = 0;
        envelope_ = 0.0f;
        trackedHz_ = 0.0f;
        previousTrackedHz_ = 0.0f;

        freezeActive_ = false;
        freezePhase_ = 0.0f;
        freezeRemainingSamples_ = 0;

        pitchPhase_ = 0.0f;
    }

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kTwoPi = 6.28318530718f;
    static constexpr float kMaxBufferSeconds = 2.0f;
    static constexpr int kAnalysisHop = 128;

    double sampleRate_ = 44100.0;
    int bufferSize_ = 0;

    std::array<std::vector<float>, kMaxChannels> lowBuffer_;
    std::array<std::vector<float>, kMaxChannels> highBuffer_;
    std::array<int, kMaxChannels> writePos_{};

    std::vector<float> analysisBuffer_;
    int analysisWritePos_ = 0;

    std::array<int, kMaxChannels> freezeStartPos_{};
    std::array<float, kMaxChannels> lowState_{};
    std::array<float, kMaxChannels> lastGhost_{};

    std::int64_t sampleStamp_ = -1;
    int analysisCounter_ = 0;

    float envelope_ = 0.0f;
    float trackedHz_ = 0.0f;
    float previousTrackedHz_ = 0.0f;

    bool freezeActive_ = false;
    float freezePhase_ = 0.0f;
    int freezeLengthSamples_ = 256;
    int freezeRemainingSamples_ = 0;

    float freezeAmount_ = 0.55f;
    float ghostBlend_ = 0.45f;
    float outputWet_ = 0.6f;

    float shiftRatio_ = 6.0f / 5.0f;
    float grainSizeSamples_ = 2048.0f;
    float pitchPhase_ = 0.0f;
    float pitchPhaseInc_ = 0.0001f;
    float freezePlaybackRate_ = 0.2f;

    float lowpassAlpha_ = 0.95f;

    void updateDerivedState() noexcept {
        freezeLengthSamples_ = std::clamp(
            static_cast<int>((0.08f + freezeAmount_ * 0.82f) * static_cast<float>(sampleRate_)),
            64,
            std::max(64, bufferSize_ - 4));

        grainSizeSamples_ = std::clamp(
            static_cast<float>(sampleRate_) * (0.03f + freezeAmount_ * 0.09f),
            192.0f,
            static_cast<float>(std::max(196, bufferSize_ - 4)));

        pitchPhaseInc_ = std::max(
            (shiftRatio_ - 1.0f) / std::max(1.0f, grainSizeSamples_),
            1.0e-6f);

        outputWet_ = std::clamp(0.40f + freezeAmount_ * 0.50f, 0.0f, 1.0f);
        freezePlaybackRate_ = 0.10f + (1.0f - freezeAmount_) * 0.45f;
    }

    void runSharedFrame(const AgentVST::DSPContext& ctx, float monoIn) noexcept {
        freezeAmount_ = std::clamp(ctx.getParameter("common_tone_freeze") * 0.01f, 0.0f, 1.0f);
        ghostBlend_ = std::clamp(ctx.getParameter("wet_ghost_mix") * 0.01f, 0.0f, 1.0f);
        shiftRatio_ = (ctx.getParameter("mediant_mode") < 0.5f) ? (6.0f / 5.0f) : (5.0f / 4.0f);
        updateDerivedState();

        pitchPhase_ += pitchPhaseInc_;
        if (pitchPhase_ >= 1.0f)
            pitchPhase_ -= std::floor(pitchPhase_);

        if (freezeActive_) {
            freezePhase_ += freezePlaybackRate_;
            while (freezePhase_ >= static_cast<float>(freezeLengthSamples_))
                freezePhase_ -= static_cast<float>(freezeLengthSamples_);

            --freezeRemainingSamples_;
            if (freezeRemainingSamples_ <= 0)
                freezeActive_ = false;
        }

        const float absIn = std::abs(monoIn);
        const float attack = std::exp(-1.0f / (0.01f * static_cast<float>(sampleRate_)));
        const float release = std::exp(-1.0f / (0.12f * static_cast<float>(sampleRate_)));

        if (absIn > envelope_)
            envelope_ = attack * envelope_ + (1.0f - attack) * absIn;
        else
            envelope_ = release * envelope_ + (1.0f - release) * absIn;

        ++analysisCounter_;
        if (analysisCounter_ >= kAnalysisHop) {
            analysisCounter_ = 0;

            const float measuredHz = estimateFrequencyHz();
            if (measuredHz > 35.0f && measuredHz < 2200.0f) {
                if (trackedHz_ <= 1.0f)
                    trackedHz_ = measuredHz;
                else
                    trackedHz_ = 0.86f * trackedHz_ + 0.14f * measuredHz;

                if (previousTrackedHz_ > 20.0f) {
                    const float ratio = std::max(0.01f, trackedHz_ / previousTrackedHz_);
                    const float semitoneJump = 12.0f * std::abs(std::log2(ratio));
                    const float gate = 0.01f + (1.0f - freezeAmount_) * 0.015f;

                    if (envelope_ > gate && semitoneJump > 2.25f)
                        triggerFreeze();
                }

                previousTrackedHz_ = trackedHz_;
            }
        }

        float commonHz = trackedHz_ * ((shiftRatio_ > 1.23f) ? 1.1f : 1.25f);
        commonHz = std::clamp(commonHz, 140.0f, 1800.0f);
        commonHz *= std::clamp(1.1f - freezeAmount_ * 0.35f, 0.6f, 1.1f);
        lowpassAlpha_ = std::exp(-kTwoPi * commonHz / static_cast<float>(sampleRate_));
    }

    float estimateFrequencyHz() const noexcept {
        if (analysisBuffer_.empty() || bufferSize_ < 4)
            return 0.0f;

        const int window = std::clamp(
            static_cast<int>(sampleRate_ * 0.035),
            256,
            std::max(256, bufferSize_ - 2));

        int idx = analysisWritePos_ - window;
        while (idx < 0)
            idx += bufferSize_;

        const float gate = std::max(0.0005f, envelope_ * 0.10f);
        float prev = analysisBuffer_[static_cast<std::size_t>(idx)];
        int crossings = 0;

        for (int i = 1; i < window; ++i) {
            ++idx;
            if (idx >= bufferSize_)
                idx = 0;

            const float curr = analysisBuffer_[static_cast<std::size_t>(idx)];
            const bool crossed = (prev <= 0.0f && curr > 0.0f)
                              || (prev >= 0.0f && curr < 0.0f);
            const bool strong = (std::abs(prev) > gate) || (std::abs(curr) > gate);

            if (crossed && strong)
                ++crossings;

            prev = curr;
        }

        if (crossings < 4)
            return 0.0f;

        const float hz = 0.5f * static_cast<float>(crossings)
                       * static_cast<float>(sampleRate_)
                       / static_cast<float>(window);
        if (hz < 30.0f || hz > 2500.0f)
            return 0.0f;

        return hz;
    }

    void triggerFreeze() noexcept {
        if (freezeAmount_ <= 0.01f)
            return;

        freezeActive_ = true;
        freezePhase_ = 0.0f;
        freezeRemainingSamples_ = std::max(
            freezeLengthSamples_,
            static_cast<int>(freezeLengthSamples_ * (0.75f + freezeAmount_)));

        for (int ch = 0; ch < kMaxChannels; ++ch) {
            int start = writePos_[static_cast<std::size_t>(ch)] - freezeLengthSamples_;
            while (start < 0)
                start += bufferSize_;
            freezeStartPos_[static_cast<std::size_t>(ch)] = start;
            lastGhost_[static_cast<std::size_t>(ch)] = 0.0f;
        }
    }

    float lowSplit(int channel, float input) noexcept {
        float& state = lowState_[static_cast<std::size_t>(channel)];
        state = (1.0f - lowpassAlpha_) * input + lowpassAlpha_ * state;
        return state;
    }

    float readDelayed(const std::vector<float>& buffer,
                      int write,
                      float delaySamples) const noexcept {
        float readPos = static_cast<float>(write) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(bufferSize_);
        while (readPos >= static_cast<float>(bufferSize_))
            readPos -= static_cast<float>(bufferSize_);

        int i0 = static_cast<int>(readPos);
        int i1 = i0 + 1;
        if (i1 >= bufferSize_)
            i1 = 0;

        const float frac = readPos - static_cast<float>(i0);
        const float s0 = buffer[static_cast<std::size_t>(i0)];
        const float s1 = buffer[static_cast<std::size_t>(i1)];
        return s0 + frac * (s1 - s0);
    }

    float readAbsolute(const std::vector<float>& buffer,
                       float absolutePos) const noexcept {
        float pos = absolutePos;
        while (pos < 0.0f)
            pos += static_cast<float>(bufferSize_);
        while (pos >= static_cast<float>(bufferSize_))
            pos -= static_cast<float>(bufferSize_);

        int i0 = static_cast<int>(pos);
        int i1 = i0 + 1;
        if (i1 >= bufferSize_)
            i1 = 0;

        const float frac = pos - static_cast<float>(i0);
        const float s0 = buffer[static_cast<std::size_t>(i0)];
        const float s1 = buffer[static_cast<std::size_t>(i1)];
        return s0 + frac * (s1 - s0);
    }

    float computeShifted(int channel) const noexcept {
        const int ch = channel;
        const int write = writePos_[static_cast<std::size_t>(ch)];
        const auto& ring = highBuffer_[static_cast<std::size_t>(ch)];

        const float phaseA = pitchPhase_;
        float phaseB = phaseA + 0.5f;
        if (phaseB >= 1.0f)
            phaseB -= 1.0f;

        const float delayA = std::max(1.0f, (1.0f - phaseA) * grainSizeSamples_);
        const float delayB = std::max(1.0f, (1.0f - phaseB) * grainSizeSamples_);

        const float a = readDelayed(ring, write, delayA);
        const float b = readDelayed(ring, write, delayB);

        const float windowA = 0.5f - 0.5f * std::cos(kTwoPi * phaseA);
        const float windowB = 0.5f - 0.5f * std::cos(kTwoPi * phaseB);

        const float denom = std::max(1.0e-6f, windowA + windowB);
        return (a * windowA + b * windowB) / denom;
    }

    float computeGhost(int channel) noexcept {
        const int ch = channel;
        if (!freezeActive_) {
            return lowState_[static_cast<std::size_t>(ch)]
                 * (0.20f + 0.35f * freezeAmount_);
        }

        const float position = static_cast<float>(freezeStartPos_[static_cast<std::size_t>(ch)])
                             + freezePhase_;
        const float rawGhost = readAbsolute(lowBuffer_[static_cast<std::size_t>(ch)], position);

        float& smoothed = lastGhost_[static_cast<std::size_t>(ch)];
        smoothed += 0.06f * (rawGhost - smoothed);
        return smoothed;
    }
};

AGENTVST_REGISTER_DSP(ThreadOfMemoryMediantSplitterProcessor)
