// Purpose: TawhidInfiniteSuspension DSP processing.
// Author: Seth Nenninger (GPT-5.2-Codex Agent)
// Timestamp: 2026-05-06T20:10:00Z
// Changelog: Add pitch-tracked suspension capture with glide-to-resolution; add YIN tracking and chord-change gating.

/**
 * TawhidInfiniteSuspension DSP
 * PERFORMANCE: Cached parameters, hop-based YIN tracking, deterministic RNG.
 */
#include <AgentDSP.h>
#include <algorithm>
#include <cmath>
#include <vector>

class TawhidInfiniteSuspensionProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = std::max(8000.0, sampleRate);

        const int bufferSize = static_cast<int>(sampleRate_ * 2.0);
        buffer_.assign(2, std::vector<float>(static_cast<size_t>(bufferSize), 0.0f));
        bufferSize_ = bufferSize;
        writeIdx_ = 0;

        // Pitch tracking (YIN, hop-based)
        yinBufferSize_ = static_cast<int>(sampleRate_ * 0.02);
        if ((yinBufferSize_ % 2) != 0)
            ++yinBufferSize_;
        halfBufferSize_ = std::max(2, yinBufferSize_ / 2);
        yinBuffer_.assign(static_cast<size_t>(yinBufferSize_), 0.0f);
        yinDiff_.assign(static_cast<size_t>(halfBufferSize_), 0.0f);
        trackWriteIdx_ = 0;
        yinHopSize_ = std::max(1, static_cast<int>(sampleRate_ * 0.025));
        samplesUntilNextYin_ = 0;

        currentFundamental_ = 0.0f;
        smoothedFundamental_ = 0.0f;
        lastStableFundamental_ = 0.0f;
        pitchValid_ = false;
        changeCooldownSamples_ = 0;

        // Suspension state
        suspendActive_ = false;
        suspendHoldRemaining_ = 0;
        suspendGlideRemaining_ = 0;
        suspendGlideSamples_ = 1;
        suspendPitchStart_ = 0.0f;
        suspendPitchEnd_ = 0.0f;
        heldPitch_ = 0.0f;
        freezeStart_ = 0;
        freezeLength_ = std::max(256, static_cast<int>(sampleRate_ * 0.12));
        freezePos_ = 0.0f;
        freezePosSnapshot_ = 0.0f;
        suspendMix_ = 0.0f;
        lastPitchRatio_ = 1.0f;

        suspendSmoothCoeff_ = std::exp(-1.0f / (0.01f * static_cast<float>(sampleRate_)));
        trackingSmoothCoeff_ = std::exp(-1.0f / (0.05f * static_cast<float>(sampleRate_)));

        // Parameter cache
        cachedGravity_ = 0.5f;
        cachedTension_ = 0.5f;
        cachedGlideMs_ = 1000.0f;
        cachedMix_ = 0.5f;
        glideSamples_ = static_cast<int>(sampleRate_ * 0.5);
        lastBlockStart_ = -1;

        randState_ = 12345;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedGravity_ = ctx.getParameter("suspension_gravity") / 100.0f;
            cachedTension_ = ctx.getParameter("tension_selection");
            cachedGlideMs_ = ctx.getParameter("resolution_glide");
            cachedMix_ = ctx.getParameter("mix") / 100.0f;
            glideSamples_ = std::clamp(
                static_cast<int>(cachedGlideMs_ * 0.001f * static_cast<float>(sampleRate_)),
                32,
                std::max(64, bufferSize_ - 1));
        }

        if (channel >= 2)
            return input;

        buffer_[channel][writeIdx_] = input;

        if (channel == 0) {
            updatePitchTracking(input);
            freezePosSnapshot_ = updateSuspensionState();
        }

        float frozenSample = 0.0f;
        if (suspendMix_ > 1.0e-4f) {
            frozenSample = readFrozenSample(channel, freezePosSnapshot_);
        }

        const float wetMix = std::clamp(cachedMix_, 0.0f, 1.0f);
        const float dryMix = 1.0f - wetMix;
        const float suspendGain = suspendMix_ * (0.6f + 0.4f * cachedGravity_);
        const float output = std::tanh(input * dryMix + frozenSample * wetMix * suspendGain);

        if (channel == ctx.numChannels - 1) {
            writeIdx_ = (writeIdx_ + 1) % bufferSize_;
        }

        return output;
    }

private:
    static constexpr float kChordChangeSemis = 0.7f;

    void pushTrackingSample(float input) noexcept {
        if (yinBufferSize_ <= 0)
            return;
        yinBuffer_[trackWriteIdx_] = input;
        trackWriteIdx_ = (trackWriteIdx_ + 1) % yinBufferSize_;
    }

    void computeYin() noexcept {
        if (halfBufferSize_ < 4) {
            pitchValid_ = false;
            return;
        }

        for (int tau = 0; tau < halfBufferSize_; ++tau) {
            float diff = 0.0f;
            for (int i = 0; i < halfBufferSize_; ++i) {
                const int idx1 = (trackWriteIdx_ - yinBufferSize_ + i + yinBufferSize_) % yinBufferSize_;
                const int idx2 = (trackWriteIdx_ - yinBufferSize_ + i + tau + yinBufferSize_) % yinBufferSize_;
                const float delta = yinBuffer_[idx1] - yinBuffer_[idx2];
                diff += delta * delta;
            }
            yinDiff_[tau] = diff;
        }

        yinDiff_[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau < halfBufferSize_; ++tau) {
            runningSum += yinDiff_[tau];
            yinDiff_[tau] = (runningSum > 0.0f)
                ? (yinDiff_[tau] * static_cast<float>(tau) / runningSum)
                : 1.0f;
        }

        const int minTau = std::max(2, static_cast<int>(sampleRate_ / 1000.0));
        const int maxTau = std::min(halfBufferSize_ - 1, static_cast<int>(sampleRate_ / 80.0));
        if (maxTau <= minTau) {
            pitchValid_ = false;
            return;
        }

        const float threshold = 0.15f;
        int tauEstimate = -1;

        for (int tau = minTau; tau <= maxTau; ++tau) {
            if (yinDiff_[tau] < threshold) {
                if (tau + 1 > maxTau || yinDiff_[tau] <= yinDiff_[tau + 1]) {
                    tauEstimate = tau;
                    break;
                }
            }
        }

        if (tauEstimate < 0) {
            float best = 1.0e9f;
            const int limitedMax = std::min(maxTau, minTau + 160);
            for (int tau = minTau; tau <= limitedMax; ++tau) {
                if (yinDiff_[tau] < best) {
                    best = yinDiff_[tau];
                    tauEstimate = tau;
                }
            }
        }

        if (tauEstimate > 0) {
            const float detected = static_cast<float>(sampleRate_) / static_cast<float>(tauEstimate);
            if (std::isfinite(detected)) {
                currentFundamental_ = std::clamp(detected, 80.0f, 1600.0f);
                pitchValid_ = true;
                return;
            }
        }

        pitchValid_ = false;
    }

    void updatePitchTracking(float input) noexcept {
        pushTrackingSample(input);
        --samplesUntilNextYin_;
        if (samplesUntilNextYin_ <= 0) {
            samplesUntilNextYin_ = yinHopSize_;
            computeYin();
        }

        if (pitchValid_) {
            smoothedFundamental_ =
                trackingSmoothCoeff_ * smoothedFundamental_ + (1.0f - trackingSmoothCoeff_) * currentFundamental_;
        }
    }

    float updateSuspensionState() noexcept {
        if (changeCooldownSamples_ > 0)
            --changeCooldownSamples_;

        bool chordChange = false;
        float oldFundamental = 0.0f;
        float newFundamental = 0.0f;

        if (pitchValid_ && smoothedFundamental_ > 0.0f) {
            if (lastStableFundamental_ <= 0.0f) {
                lastStableFundamental_ = smoothedFundamental_;
            } else if (changeCooldownSamples_ <= 0) {
                const float ratio = std::max(smoothedFundamental_ / lastStableFundamental_, 0.0001f);
                const float semis = 12.0f * std::log2(ratio);
                if (std::abs(semis) >= kChordChangeSemis) {
                    chordChange = true;
                    oldFundamental = lastStableFundamental_;
                    newFundamental = smoothedFundamental_;
                    lastStableFundamental_ = smoothedFundamental_;
                    changeCooldownSamples_ = static_cast<int>(sampleRate_ * 0.08f);
                }
            }
        }

        if (chordChange) {
            const float catchChance = std::clamp(cachedGravity_, 0.0f, 1.0f);
            const float randomVal = nextRand01();
            if (randomVal < catchChance && oldFundamental > 0.0f && newFundamental > 0.0f) {
                startSuspension(oldFundamental, newFundamental, randomVal);
            }
        }

        const float targetMix = suspendActive_ ? 1.0f : 0.0f;
        suspendMix_ = suspendSmoothCoeff_ * suspendMix_ + (1.0f - suspendSmoothCoeff_) * targetMix;

        float readPos = freezePos_;
        float ratio = lastPitchRatio_;

        if (suspendActive_) {
            float currentPitch = suspendPitchStart_;
            if (suspendHoldRemaining_ > 0) {
                --suspendHoldRemaining_;
            } else if (suspendGlideRemaining_ > 0) {
                const float t = 1.0f - (static_cast<float>(suspendGlideRemaining_) /
                    static_cast<float>(std::max(1, suspendGlideSamples_)));
                currentPitch = suspendPitchStart_ + (suspendPitchEnd_ - suspendPitchStart_) * t;
                --suspendGlideRemaining_;
            } else {
                suspendActive_ = false;
            }

            if (suspendActive_) {
                ratio = std::clamp(currentPitch / std::max(heldPitch_, 1.0f), 0.5f, 2.5f);
                lastPitchRatio_ = ratio;
            }
        }

        if (suspendMix_ > 1.0e-4f) {
            readPos = freezePos_;
            freezePos_ += ratio;
            while (freezePos_ >= static_cast<float>(freezeLength_))
                freezePos_ -= static_cast<float>(freezeLength_);
        }

        return readPos;
    }

    void startSuspension(float oldFundamental, float newFundamental, float randomVal) noexcept {
        heldPitch_ = oldFundamental;
        suspendPitchStart_ = newFundamental * pickSuspensionRatio(cachedTension_, randomVal);
        suspendPitchEnd_ = newFundamental;

        suspendHoldRemaining_ = std::max(32, static_cast<int>(sampleRate_ * (0.05f + 0.45f * cachedGravity_)));
        suspendGlideSamples_ = std::max(32, glideSamples_);
        suspendGlideRemaining_ = suspendGlideSamples_;
        suspendActive_ = true;

        const int maxFreeze = std::max(256, bufferSize_ - 1);
        freezeLength_ = std::clamp(
            static_cast<int>(sampleRate_ * (0.08f + 0.28f * cachedGravity_)),
            256,
            maxFreeze);
        freezeStart_ = writeIdx_ - freezeLength_;
        if (freezeStart_ < 0)
            freezeStart_ += bufferSize_;
        freezePos_ = 0.0f;
    }

    float readFrozenSample(int channel, float pos) const noexcept {
        if (bufferSize_ <= 0)
            return 0.0f;

        float readPos = static_cast<float>(freezeStart_) + pos;
        while (readPos < 0.0f)
            readPos += static_cast<float>(bufferSize_);
        while (readPos >= static_cast<float>(bufferSize_))
            readPos -= static_cast<float>(bufferSize_);

        const int i0 = static_cast<int>(readPos);
        const int i1 = (i0 + 1) % bufferSize_;
        const float frac = readPos - static_cast<float>(i0);
        return buffer_[channel][i0] * (1.0f - frac) + buffer_[channel][i1] * frac;
    }

    float nextRand01() noexcept {
        randState_ = (randState_ * 1103515245 + 12345) & 0x7FFFFFFF;
        return static_cast<float>(randState_) / 2147483647.0f;
    }

    static float pickSuspensionRatio(float tension, float randomVal) noexcept {
        const float t = std::clamp(tension, 0.0f, 1.0f);
        const float sus2 = 9.0f / 8.0f;
        const float sus4 = 4.0f / 3.0f;
        const float maj7 = 15.0f / 8.0f;

        if (t < 0.33f) {
            return (randomVal < 0.75f) ? sus2 : sus4;
        }
        if (t < 0.66f) {
            if (randomVal < 0.6f)
                return sus4;
            if (randomVal < 0.85f)
                return sus2;
            return maj7;
        }

        return (randomVal < 0.7f) ? maj7 : sus4;
    }

    double sampleRate_ = 44100.0;
    std::vector<std::vector<float>> buffer_;
    int bufferSize_ = 0;
    int writeIdx_ = 0;

    // Pitch tracking
    std::vector<float> yinBuffer_;
    std::vector<float> yinDiff_;
    int yinBufferSize_ = 0;
    int halfBufferSize_ = 0;
    int trackWriteIdx_ = 0;
    int yinHopSize_ = 0;
    int samplesUntilNextYin_ = 0;
    float currentFundamental_ = 0.0f;
    float smoothedFundamental_ = 0.0f;
    float lastStableFundamental_ = 0.0f;
    bool pitchValid_ = false;
    int changeCooldownSamples_ = 0;

    // Suspension state
    bool suspendActive_ = false;
    int suspendHoldRemaining_ = 0;
    int suspendGlideRemaining_ = 0;
    int suspendGlideSamples_ = 0;
    float suspendPitchStart_ = 0.0f;
    float suspendPitchEnd_ = 0.0f;
    float heldPitch_ = 0.0f;
    int freezeStart_ = 0;
    int freezeLength_ = 0;
    float freezePos_ = 0.0f;
    float freezePosSnapshot_ = 0.0f;
    float suspendMix_ = 0.0f;
    float suspendSmoothCoeff_ = 0.0f;
    float trackingSmoothCoeff_ = 0.0f;
    float lastPitchRatio_ = 1.0f;

    // Cached parameters
    float cachedGravity_ = 0.5f;
    float cachedTension_ = 0.5f;
    float cachedGlideMs_ = 1000.0f;
    float cachedMix_ = 0.5f;
    int glideSamples_ = 0;
    std::int64_t lastBlockStart_ = -1;

    unsigned int randState_ = 0;
};

AGENTVST_REGISTER_DSP(TawhidInfiniteSuspensionProcessor)
