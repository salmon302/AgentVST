/**
 * ReluctantResolutionTemporalSuspensionDSP.cpp
 *
 * Captures high-frequency transients, delays them behind the beat,
 * then glides them downward by a small interval to model reluctant resolution.
 *
 * Real-time safety:
 * - All buffers allocated in prepare().
 * - processSample() performs no allocation, locks, or I/O.
 */
#include <AgentDSP.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

class ReluctantResolutionTemporalSuspensionProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = std::max(8000.0, sampleRate);

        maxCaptureSamples_ = std::max(
            256,
            static_cast<int>(std::ceil(sampleRate_ * 0.40)));

        for (int ch = 0; ch < kMaxChannels; ++ch) {
            captureBuffer_[ch].assign(static_cast<size_t>(maxCaptureSamples_), 0.0f);
            lowState_[ch] = 0.0f;
        }

        const float hpCutoffHz = 1800.0f;
        highpassAlpha_ = std::exp(-kTwoPi * hpCutoffHz / static_cast<float>(sampleRate_));

        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= kMaxChannels)
            return input;

        if (sampleStamp_ != static_cast<std::int64_t>(ctx.currentSample)) {
            sampleStamp_ = static_cast<std::int64_t>(ctx.currentSample);
            updateSharedState(input, ctx);
        }

        const float highBand = splitHighBand(channel, input);

        if (captureActive_ && captureWritePos_ >= 0 && captureWritePos_ < maxCaptureSamples_) {
            captureBuffer_[channel][static_cast<size_t>(captureWritePos_)] = highBand;
        }

        float dry = input;
        if (voiceActive_ && voiceAgeSamples_ < lagSamples_) {
            dry -= highBand * 0.70f;
        }

        float wet = 0.0f;
        if (voiceActive_ && voiceGain_ > 0.0f && capturedLengthSamples_ > 1) {
            wet = readCaptured(channel, voiceReadPos_) * voiceGain_;
        }

        return dry + (wet * 0.85f);
    }

    void reset() override {
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(captureBuffer_[ch].begin(), captureBuffer_[ch].end(), 0.0f);
            lowState_[ch] = 0.0f;
        }

        catchThresholdNorm_ = 0.42f;
        lagSamples_ = static_cast<int>(sampleRate_ * 0.170);
        resolutionNorm_ = 0.55f;
        glideSamples_ = static_cast<int>(sampleRate_ * 0.8);
        releaseSamples_ = static_cast<int>(sampleRate_ * 0.35);
        captureDurationSamples_ = static_cast<int>(sampleRate_ * 0.035);

        transientFast_ = 0.0f;
        transientSlow_ = 0.0f;
        previousInput_ = 0.0f;

        captureActive_ = false;
        captureWritePos_ = -1;
        capturedLengthSamples_ = 0;

        voiceActive_ = false;
        voiceAgeSamples_ = 0;
        voiceReadPos_ = 0.0f;
        voiceReadIncrement_ = 1.0f;
        voiceGain_ = 0.0f;

        cooldownSamples_ = 0;
        sampleStamp_ = -1;
    }

private:
    static constexpr int kMaxChannels = 2;
    static constexpr float kTwoPi = 6.28318530718f;

    void updateSharedState(float input, const AgentVST::DSPContext& ctx) noexcept {
        catchThresholdNorm_ = std::clamp(ctx.getParameter("catch_threshold") / 100.0f, 0.0f, 1.0f);
        const float lagMs = std::clamp(ctx.getParameter("lag_amount"), 0.0f, 600.0f);
        resolutionNorm_ = std::clamp(ctx.getParameter("resolution_glide") / 100.0f, 0.0f, 1.0f);

        lagSamples_ = std::clamp(
            static_cast<int>((lagMs * 0.001f) * static_cast<float>(sampleRate_)),
            0,
            std::max(0, static_cast<int>(sampleRate_ * 0.75)));

        glideSamples_ = std::clamp(
            static_cast<int>((0.08f + resolutionNorm_ * 1.60f) * static_cast<float>(sampleRate_)),
            8,
            std::max(8, static_cast<int>(sampleRate_ * 2.0)));

        releaseSamples_ = std::clamp(
            static_cast<int>((0.08f + resolutionNorm_ * 0.65f) * static_cast<float>(sampleRate_)),
            8,
            std::max(8, static_cast<int>(sampleRate_ * 1.0)));

        const float lagNorm = std::clamp(lagMs / 600.0f, 0.0f, 1.0f);
        captureDurationSamples_ = std::clamp(
            static_cast<int>((0.015f + lagNorm * 0.055f) * static_cast<float>(sampleRate_)),
            16,
            std::max(16, maxCaptureSamples_ - 1));

        const float transient = std::abs(input - previousInput_);
        previousInput_ = input;

        const float fastAttack = std::exp(-1.0f / (0.0015f * static_cast<float>(sampleRate_)));
        const float fastRelease = std::exp(-1.0f / (0.014f * static_cast<float>(sampleRate_)));
        const float slowAttack = std::exp(-1.0f / (0.030f * static_cast<float>(sampleRate_)));
        const float slowRelease = std::exp(-1.0f / (0.240f * static_cast<float>(sampleRate_)));

        if (transient > transientFast_)
            transientFast_ = fastAttack * transientFast_ + (1.0f - fastAttack) * transient;
        else
            transientFast_ = fastRelease * transientFast_ + (1.0f - fastRelease) * transient;

        if (transient > transientSlow_)
            transientSlow_ = slowAttack * transientSlow_ + (1.0f - slowAttack) * transient;
        else
            transientSlow_ = slowRelease * transientSlow_ + (1.0f - slowRelease) * transient;

        const float absoluteGate = 0.002f + (catchThresholdNorm_ * 0.08f);
        const float relativeGate = transientSlow_ * (1.35f + (catchThresholdNorm_ * 1.9f));
        const bool onset = (transientFast_ > absoluteGate)
                        && (transientFast_ > relativeGate);

        if (cooldownSamples_ > 0)
            --cooldownSamples_;

        if (!captureActive_ && !voiceActive_ && cooldownSamples_ <= 0 && onset)
            beginCapture();

        if (captureActive_) {
            ++captureWritePos_;
            if (captureWritePos_ >= captureDurationSamples_ || captureWritePos_ >= maxCaptureSamples_)
                beginVoice();
        }

        if (voiceActive_) {
            updateVoiceMotion();
        }
    }

    void beginCapture() noexcept {
        captureActive_ = true;
        captureWritePos_ = 0;
        capturedLengthSamples_ = 0;

        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(captureBuffer_[ch].begin(), captureBuffer_[ch].end(), 0.0f);
        }
    }

    void beginVoice() noexcept {
        captureActive_ = false;
        capturedLengthSamples_ = std::clamp(captureWritePos_, 1, maxCaptureSamples_);

        voiceActive_ = true;
        voiceAgeSamples_ = 0;
        voiceReadPos_ = 0.0f;
        voiceReadIncrement_ = 1.0f;
        voiceGain_ = 0.0f;

        cooldownSamples_ = static_cast<int>(sampleRate_ * 0.08);
    }

    void updateVoiceMotion() noexcept {
        ++voiceAgeSamples_;

        const int activeSpan = lagSamples_ + glideSamples_ + releaseSamples_;
        if (voiceAgeSamples_ >= activeSpan) {
            voiceActive_ = false;
            voiceGain_ = 0.0f;
            return;
        }

        if (voiceAgeSamples_ < lagSamples_) {
            voiceGain_ = 0.0f;
            return;
        }

        const int afterLag = voiceAgeSamples_ - lagSamples_;
        const float glideT = std::clamp(
            static_cast<float>(afterLag) / static_cast<float>(std::max(1, glideSamples_)),
            0.0f,
            1.0f);

        const float smooth = glideT * glideT * (3.0f - 2.0f * glideT);
        const float targetSemitones = -(1.0f + resolutionNorm_); // minor to major second down
        const float currentSemitones = targetSemitones * smooth;
        voiceReadIncrement_ = std::pow(2.0f, currentSemitones / 12.0f);

        voiceReadPos_ += voiceReadIncrement_;
        while (voiceReadPos_ >= static_cast<float>(capturedLengthSamples_))
            voiceReadPos_ -= static_cast<float>(capturedLengthSamples_);

        if (afterLag <= glideSamples_) {
            voiceGain_ = 0.85f;
        } else {
            const int releaseAge = afterLag - glideSamples_;
            const float releaseT = std::clamp(
                static_cast<float>(releaseAge) / static_cast<float>(std::max(1, releaseSamples_)),
                0.0f,
                1.0f);
            voiceGain_ = 0.85f * (1.0f - releaseT);
        }
    }

    float splitHighBand(int channel, float input) noexcept {
        float& low = lowState_[channel];
        low = (1.0f - highpassAlpha_) * input + highpassAlpha_ * low;
        return input - low;
    }

    float readCaptured(int channel, float pos) const noexcept {
        if (capturedLengthSamples_ <= 1)
            return 0.0f;

        float wrapped = pos;
        while (wrapped < 0.0f)
            wrapped += static_cast<float>(capturedLengthSamples_);
        while (wrapped >= static_cast<float>(capturedLengthSamples_))
            wrapped -= static_cast<float>(capturedLengthSamples_);

        int i0 = static_cast<int>(wrapped);
        int i1 = i0 + 1;
        if (i1 >= capturedLengthSamples_)
            i1 = 0;

        const float frac = wrapped - static_cast<float>(i0);
        const float s0 = captureBuffer_[channel][static_cast<size_t>(i0)];
        const float s1 = captureBuffer_[channel][static_cast<size_t>(i1)];
        return s0 + frac * (s1 - s0);
    }

    double sampleRate_ = 44100.0;
    int maxCaptureSamples_ = 0;

    std::array<std::vector<float>, kMaxChannels> captureBuffer_;
    std::array<float, kMaxChannels> lowState_{};
    float highpassAlpha_ = 0.95f;

    float catchThresholdNorm_ = 0.42f;
    int lagSamples_ = 0;
    float resolutionNorm_ = 0.55f;
    int glideSamples_ = 0;
    int releaseSamples_ = 0;
    int captureDurationSamples_ = 0;

    float transientFast_ = 0.0f;
    float transientSlow_ = 0.0f;
    float previousInput_ = 0.0f;

    bool captureActive_ = false;
    int captureWritePos_ = -1;
    int capturedLengthSamples_ = 0;

    bool voiceActive_ = false;
    int voiceAgeSamples_ = 0;
    float voiceReadPos_ = 0.0f;
    float voiceReadIncrement_ = 1.0f;
    float voiceGain_ = 0.0f;

    int cooldownSamples_ = 0;
    std::int64_t sampleStamp_ = -1;
};

AGENTVST_REGISTER_DSP(ReluctantResolutionTemporalSuspensionProcessor)
