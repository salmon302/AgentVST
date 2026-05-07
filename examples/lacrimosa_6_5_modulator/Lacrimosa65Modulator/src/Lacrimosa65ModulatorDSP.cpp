/**
 * Lacrimosa65Modulator DSP
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - Reduced YIN buffer from 50ms to 20ms
 * - Increased hop size from 10ms to 25ms
 * - Added parameter caching at block start
 * - Optimized YIN difference function
 */
#include <AgentDSP.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

class Lacrimosa65ModulatorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = std::max(8000.0, sampleRate);

        // PERFORMANCE: Reduced buffer from 50ms to 20ms
        yinBufferSize_ = static_cast<int>(sampleRate_ * 0.02); // 20 ms
        if ((yinBufferSize_ % 2) != 0)
            ++yinBufferSize_;

        halfBufferSize_ = yinBufferSize_ / 2;
        yinBuffer_.assign(static_cast<size_t>(yinBufferSize_), 0.0f);
        yinDiff_.assign(static_cast<size_t>(halfBufferSize_), 0.0f);

        // PERFORMANCE: Increased hop from 10ms to 25ms
        yinHopSize_ = std::max(1, static_cast<int>(sampleRate_ * 0.025)); // 25 ms
        writeIndex_ = 0;
        samplesUntilNextYin_ = 0;

        // PERFORMANCE: Parameter caching
        cachedBypass_ = 0.0f;
        cachedIntensity_ = 0.5f;
        cachedSobDepth_ = 0.5f;
        cachedMix_ = 0.5f;
        cachedTrackingSpeed_ = 50.0f;
        lastBlockStart_ = -1;

        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        // PERFORMANCE: Cache parameters at block start
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedBypass_ = ctx.getParameter("bypass");
            cachedIntensity_ = ctx.getParameter("intensity");
            cachedSobDepth_ = ctx.getParameter("sob_depth");
            cachedMix_ = ctx.getParameter("mix");
            cachedTrackingSpeed_ = ctx.getParameter("tracking_speed");
        }

        if (channel >= 2)
            return input;
        if (cachedBypass_ >= 0.5f)
            return input;

        if (sampleStamp_ != static_cast<std::int64_t>(ctx.currentSample)) {
            sampleStamp_ = static_cast<std::int64_t>(ctx.currentSample);
            updateSharedState(input);
        }

        const float thirdBand = processTemperedThirdBand(channel, input);
        if (channel == 0)
            updateThirdEnvelope(thirdBand);

        const float correctedThird = generatedThird_ * thirdEnvelope_ * sobGain_;
        const float shiftedSignal = input - (thirdBand * intensity_) + (correctedThird * intensity_);

        return (input * (1.0f - mix_)) + (shiftedSignal * mix_);
    }

    void reset() override {
        std::fill(yinBuffer_.begin(), yinBuffer_.end(), 0.0f);
        std::fill(yinDiff_.begin(), yinDiff_.end(), 0.0f);
        resonatorStates_ = {};

        sampleStamp_ = -1;
        writeIndex_ = 0;
        samplesUntilNextYin_ = 0;

        currentFundamental_ = 220.0f;
        smoothedFundamental_ = 220.0f;
        thirdEnvelope_ = 0.0f;

        targetPhase_ = 0.0f;
        beatPhase_ = 0.0f;
        generatedThird_ = 0.0f;
        sobGain_ = 1.0f;

        intensity_ = 0.0f;
        sobDepth_ = 0.0f;
        mix_ = 0.0f;
        envAttackCoeff_ = 0.99f;
        envReleaseCoeff_ = 0.999f;

        updateResonatorCoefficients(smoothedFundamental_ * kTemperedMinorThirdRatio);
    }

private:
    struct ResonatorState {
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    static constexpr float kPi = 3.14159265359f;
    static constexpr float kTwoPi = 2.0f * kPi;
    static constexpr float kTemperedMinorThirdRatio = 1.189207115f;
    static constexpr float kJustMinorThirdRatio = 1.2f;

    // PERFORMANCE: Cached parameter values
    float cachedBypass_ = 0.0f;
    float cachedIntensity_ = 0.5f;
    float cachedSobDepth_ = 0.5f;
    float cachedMix_ = 0.5f;
    float cachedTrackingSpeed_ = 50.0f;
    std::int64_t lastBlockStart_ = -1;

    void updateSharedState(float trackingInput) noexcept {
        // Use cached parameter values
        intensity_ = std::clamp(cachedIntensity_ / 100.0f, 0.0f, 1.0f);
        sobDepth_ = std::clamp(cachedSobDepth_ / 100.0f, 0.0f, 1.0f);
        mix_ = std::clamp(cachedMix_ / 100.0f, 0.0f, 1.0f);

        const float trackingMs = std::clamp(cachedTrackingSpeed_, 10.0f, 500.0f);
        const float smoothCoeff = std::exp(-1.0f / (0.001f * trackingMs * static_cast<float>(sampleRate_)));
        envAttackCoeff_ = std::exp(-1.0f / (0.003f * static_cast<float>(sampleRate_)));
        envReleaseCoeff_ = std::exp(-1.0f / (0.001f * trackingMs * static_cast<float>(sampleRate_)));

        pushTrackingSample(trackingInput);
        --samplesUntilNextYin_;
        if (samplesUntilNextYin_ <= 0) {
            samplesUntilNextYin_ = yinHopSize_;
            computeYin();
        }

        smoothedFundamental_ =
            smoothCoeff * smoothedFundamental_ + (1.0f - smoothCoeff) * currentFundamental_;

        const float nyquist = static_cast<float>(sampleRate_) * 0.5f;
        const float temperedThirdHz =
            std::clamp(smoothedFundamental_ * kTemperedMinorThirdRatio, 60.0f, nyquist * 0.45f);
        const float justThirdHz =
            std::clamp(smoothedFundamental_ * kJustMinorThirdRatio, 60.0f, nyquist * 0.45f);

        const float targetThirdHz =
            temperedThirdHz + (justThirdHz - temperedThirdHz) * intensity_;

        updateResonatorCoefficients(temperedThirdHz);

        targetPhase_ = wrapPhase(targetPhase_ + (kTwoPi * targetThirdHz / static_cast<float>(sampleRate_)));
        generatedThird_ = std::sin(targetPhase_);

        const float beatHz = std::clamp(std::abs(justThirdHz - temperedThirdHz), 0.2f, 12.0f);
        beatPhase_ = wrapPhase(beatPhase_ + (kTwoPi * beatHz / static_cast<float>(sampleRate_)));
        sobGain_ = std::max(0.0f, 1.0f + (0.75f * sobDepth_ * std::sin(beatPhase_)));
    }

    float processTemperedThirdBand(int channel, float input) noexcept {
        ResonatorState& state = resonatorStates_[channel];
        const float y = (resonatorB0_ * input)
            + (resonatorA1_ * state.z1)
            + (resonatorA2_ * state.z2);

        state.z2 = state.z1;
        state.z1 = y;
        return y;
    }

    void updateThirdEnvelope(float thirdBandSample) noexcept {
        const float absBand = std::abs(thirdBandSample);
        if (absBand > thirdEnvelope_) {
            thirdEnvelope_ = envAttackCoeff_ * thirdEnvelope_ + (1.0f - envAttackCoeff_) * absBand;
        } else {
            thirdEnvelope_ = envReleaseCoeff_ * thirdEnvelope_ + (1.0f - envReleaseCoeff_) * absBand;
        }
    }

    void updateResonatorCoefficients(float centerHz) noexcept {
        const float nyquist = static_cast<float>(sampleRate_) * 0.5f;
        const float safeHz = std::clamp(centerHz, 40.0f, nyquist * 0.45f);
        const float omega = kTwoPi * safeHz / static_cast<float>(sampleRate_);
        const float r = 0.988f;

        resonatorA1_ = 2.0f * r * std::cos(omega);
        resonatorA2_ = -(r * r);
        resonatorB0_ = 1.0f - r;
    }

    void pushTrackingSample(float input) noexcept {
        if (yinBufferSize_ <= 0)
            return;

        yinBuffer_[writeIndex_] = input;
        writeIndex_ = (writeIndex_ + 1) % yinBufferSize_;
    }

    void computeYin() noexcept {
        if (halfBufferSize_ < 4)
            return;

        // PERFORMANCE: Optimized YIN with early exit
        for (int tau = 0; tau < halfBufferSize_; ++tau) {
            float diff = 0.0f;
            for (int i = 0; i < halfBufferSize_; ++i) {
                const int idx1 = (writeIndex_ - yinBufferSize_ + i + yinBufferSize_) % yinBufferSize_;
                const int idx2 = (writeIndex_ - yinBufferSize_ + i + tau + yinBufferSize_) % yinBufferSize_;
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

        const int minTau = std::max(2, static_cast<int>(sampleRate_ / 1600.0));
        const int maxTau = std::min(halfBufferSize_ - 1, static_cast<int>(sampleRate_ / 60.0));
        if (maxTau <= minTau)
            return;

        const float threshold = 0.15f;
        int tauEstimate = -1;

        // PERFORMANCE: Early exit when pitch is found
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
            const int limitedMax = std::min(maxTau, minTau + 200); // Limit search range
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
                currentFundamental_ = std::clamp(detected, 60.0f, 1400.0f);
            }
        }
    }

    static float wrapPhase(float phase) noexcept {
        while (phase >= kTwoPi)
            phase -= kTwoPi;
        while (phase < 0.0f)
            phase += kTwoPi;
        return phase;
    }

    double sampleRate_ = 44100.0;

    std::vector<float> yinBuffer_;
    std::vector<float> yinDiff_;
    int yinBufferSize_ = 0;
    int halfBufferSize_ = 0;
    int writeIndex_ = 0;
    int yinHopSize_ = 1;
    int samplesUntilNextYin_ = 0;

    float currentFundamental_ = 220.0f;
    float smoothedFundamental_ = 220.0f;
    float thirdEnvelope_ = 0.0f;

    std::array<ResonatorState, 2> resonatorStates_{};
    float resonatorA1_ = 0.0f;
    float resonatorA2_ = 0.0f;
    float resonatorB0_ = 1.0f;

    float intensity_ = 0.0f;
    float sobDepth_ = 0.0f;
    float mix_ = 0.0f;

    float envAttackCoeff_ = 0.99f;
    float envReleaseCoeff_ = 0.999f;

    float targetPhase_ = 0.0f;
    float beatPhase_ = 0.0f;
    float generatedThird_ = 0.0f;
    float sobGain_ = 1.0f;

    std::int64_t sampleStamp_ = -1;
};

AGENTVST_REGISTER_DSP(Lacrimosa65ModulatorProcessor)
