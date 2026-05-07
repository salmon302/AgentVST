/**
 * DualismUndertoneAnchor DSP
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - Reduced YIN buffer from 50ms to 20ms
 * - Increased hop size from 10ms to 25ms
 * - Added parameter caching at block start
 * - Optimized YIN with early exit
 * - Reduced max partials from 10 to 6 for faster oscillator rendering
 */
#include <AgentDSP.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

class DualismUndertoneAnchorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = std::max(8000.0, sampleRate);

        // PERFORMANCE: Reduced buffer from 50ms to 20ms
        yinBufferSize_ = static_cast<int>(sampleRate_ * 0.02); // 20ms
        if ((yinBufferSize_ % 2) != 0)
            ++yinBufferSize_;

        halfBufferSize_ = yinBufferSize_ / 2;
        yinBuffer_.assign(static_cast<size_t>(yinBufferSize_), 0.0f);
        yinDiff_.assign(static_cast<size_t>(halfBufferSize_), 0.0f);

        writeIndex_ = 0;
        // PERFORMANCE: Increased hop from 10ms to 25ms
        yinHopSize_ = std::max(1, static_cast<int>(sampleRate_ * 0.025)); // 25ms updates
        samplesUntilNextYin_ = 0;

        // PERFORMANCE: Parameter caching
        cachedBypass_ = 0.0f;
        cachedBias_ = 0.5f;
        cachedGravity_ = 0.5f;
        cachedDepth_ = 0.5f;
        cachedViscosity_ = 0.5f;
        cachedSinkingSpeed_ = 50.0f;
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
            cachedBias_ = ctx.getParameter("overtone_undertone_bias");
            cachedGravity_ = ctx.getParameter("gravity");
            cachedDepth_ = ctx.getParameter("depth");
            cachedViscosity_ = ctx.getParameter("viscosity");
            cachedSinkingSpeed_ = ctx.getParameter("sinking_speed");
            cachedMix_ = ctx.getParameter("mix");
            cachedTrackingSpeed_ = ctx.getParameter("tracking_speed");
        }

        if (channel >= 2)
            return input;
        if (cachedBypass_ >= 0.5f)
            return input;

        // Use cached parameter values
        const float biasNorm = std::clamp(cachedBias_ / 100.0f, 0.0f, 1.0f);
        const float gravityNorm = std::clamp(cachedGravity_ / 100.0f, 0.0f, 1.0f);
        const float depthNorm = std::clamp(cachedDepth_ / 10.0f, 0.1f, 1.0f);
        const float viscosityNorm = std::clamp(cachedViscosity_ / 100.0f, 0.0f, 1.0f);
        const float sinkingSpeedMs = std::clamp(cachedSinkingSpeed_, 10.0f, 500.0f);
        const float mix = std::clamp(cachedMix_ / 100.0f, 0.0f, 1.0f);

        // Dynamic partial count based on depth parameter
        const int activePartials = computeActivePartials(depthNorm);

        if (channel == 0) {
            pushTrackingSample(input);

            --samplesUntilNextYin_;
            if (samplesUntilNextYin_ <= 0) {
                samplesUntilNextYin_ = yinHopSize_;
                computeYin();
            }

            // Sinking speed affects the smoothing of fundamental tracking
            const float sinkSmoothingCoeff = std::exp(-1.0f / (0.001f * sinkingSpeedMs * static_cast<float>(sampleRate_)));
            smoothedFundamental_ =
                sinkSmoothingCoeff * smoothedFundamental_ + (1.0f - sinkSmoothingCoeff) * currentFundamental_;

            const float absInput = std::abs(input);
            // Viscosity affects attack coefficient - higher viscosity = slower attack
            const float attackCoeff = std::exp(-1.0f / (0.003f * static_cast<float>(sampleRate_) * (1.0f + viscosityNorm * 2.0f)));
            // Higher viscosity = slower release (sound sinks and lingers)
            const float releaseMs = 80.0f + (gravityNorm * 1200.0f) * (1.0f + viscosityNorm);
            const float releaseCoeff = std::exp(-1.0f / (0.001f * releaseMs * static_cast<float>(sampleRate_)));

            if (absInput > inputEnvelope_) {
                inputEnvelope_ = attackCoeff * inputEnvelope_ + (1.0f - attackCoeff) * absInput;
            } else {
                inputEnvelope_ = releaseCoeff * inputEnvelope_ + (1.0f - releaseCoeff) * absInput;
            }

            if (inputEnvelope_ > synthEnvelope_) {
                synthEnvelope_ = inputEnvelope_;
            } else {
                synthEnvelope_ = releaseCoeff * synthEnvelope_ + (1.0f - releaseCoeff) * inputEnvelope_;
            }

            // Update oscillators based on viscosity-affected fundamental
            const float viscousFundamental = smoothedFundamental_ * (1.0f - viscosityNorm * 0.3f);
            updateOscillators(viscousFundamental, activePartials);
            generatedOvertone_ = renderOvertones(viscousFundamental, activePartials);
            generatedUndertone_ = renderUndertones(viscousFundamental, activePartials, viscosityNorm);
        }

        // Blend overtones and undertones based on bias
        const float dualismSeries = ((1.0f - biasNorm) * generatedOvertone_) + (biasNorm * generatedUndertone_);
        // Gravity affects amplitude; viscosity adds weight/sustain
        const float synth = dualismSeries * synthEnvelope_ * (0.7f * gravityNorm * (1.0f + viscosityNorm * 0.5f));

        const float wet = input + synth;
        return (input * (1.0f - mix)) + (wet * mix);
    }

    void reset() override {
        std::fill(yinBuffer_.begin(), yinBuffer_.end(), 0.0f);
        std::fill(yinDiff_.begin(), yinDiff_.end(), 0.0f);
        std::fill(phaseOvertones_.begin(), phaseOvertones_.end(), 0.0f);
        std::fill(phaseUndertones_.begin(), phaseUndertones_.end(), 0.0f);

        writeIndex_ = 0;
        samplesUntilNextYin_ = 0;

        currentFundamental_ = 110.0f;
        smoothedFundamental_ = 110.0f;
        inputEnvelope_ = 0.0f;
        synthEnvelope_ = 0.0f;
        generatedOvertone_ = 0.0f;
        generatedUndertone_ = 0.0f;
    }

private:
    static constexpr float kPi = 3.14159265359f;
    static constexpr float kTwoPi = 2.0f * kPi;
    // PERFORMANCE: Reduced from 10 to 6 partials
    static constexpr int kMaxPartials = 6;

    // PERFORMANCE: Cached parameter values
    float cachedBypass_ = 0.0f;
    float cachedBias_ = 0.5f;
    float cachedGravity_ = 0.5f;
    float cachedDepth_ = 0.5f;
    float cachedViscosity_ = 0.5f;
    float cachedSinkingSpeed_ = 50.0f;
    float cachedMix_ = 0.5f;
    float cachedTrackingSpeed_ = 50.0f;
    std::int64_t lastBlockStart_ = -1;

    int computeActivePartials(float depthNorm) const noexcept {
        return static_cast<int>(std::round(depthNorm * static_cast<float>(kMaxPartials - 1))) + 1;
    }

    void pushTrackingSample(float input) noexcept {
        if (yinBufferSize_ <= 0)
            return;
        yinBuffer_[writeIndex_] = input;
        writeIndex_ = (writeIndex_ + 1) % yinBufferSize_;
    }

    void updateOscillators(float fundamental, int activePartials) noexcept {
        const float safeFund = std::clamp(fundamental, 20.0f, 800.0f);
        const float nyquist = static_cast<float>(sampleRate_) * 0.5f;

        for (int i = 0; i < kMaxPartials; ++i) {
            if (i >= activePartials) {
                phaseOvertones_[i] = 0.0f;
                phaseUndertones_[i] = 0.0f;
                continue;
            }

            const float overtoneFreq = std::min(safeFund * static_cast<float>(i + 2), nyquist * 0.95f);
            const float undertoneFreq = safeFund / static_cast<float>(i + 2);

            phaseOvertones_[i] = wrapPhase(phaseOvertones_[i] + (kTwoPi * overtoneFreq / static_cast<float>(sampleRate_)));
            phaseUndertones_[i] = wrapPhase(phaseUndertones_[i] + (kTwoPi * undertoneFreq / static_cast<float>(sampleRate_)));
        }
    }

    float renderOvertones(float fundamental, int activePartials) const noexcept {
        const float safeFund = std::clamp(fundamental, 20.0f, 800.0f);
        const float nyquist = static_cast<float>(sampleRate_) * 0.5f;

        float sum = 0.0f;
        float norm = 0.0f;

        for (int i = 0; i < kMaxPartials; ++i) {
            if (i >= activePartials)
                continue;

            const float freq = safeFund * static_cast<float>(i + 2);
            if (freq >= nyquist * 0.95f)
                continue;

            const float weight = 1.0f / static_cast<float>(i + 1);
            sum += std::sin(phaseOvertones_[i]) * weight;
            norm += weight;
        }

        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }

    float renderUndertones(float fundamental, int activePartials, float viscosityNorm) const noexcept {
        const float safeFund = std::clamp(fundamental, 20.0f, 800.0f);

        float sum = 0.0f;
        float norm = 0.0f;

        for (int i = 0; i < kMaxPartials; ++i) {
            if (i >= activePartials)
                continue;

            const float undertoneFreq = safeFund / static_cast<float>(i + 2);
            const float weightDecay = std::pow(1.0f / static_cast<float>(i + 1), 1.0f + viscosityNorm);
            const float weight = weightDecay;

            sum += std::sin(phaseUndertones_[i]) * weight;
            norm += weight;
        }

        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }

    void computeYin() noexcept {
        if (halfBufferSize_ < 4)
            return;

        // YIN pitch detection algorithm
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

        // Cumulative mean normalized difference
        yinDiff_[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau < halfBufferSize_; ++tau) {
            runningSum += yinDiff_[tau];
            yinDiff_[tau] = (runningSum > 0.0f) ? (yinDiff_[tau] * static_cast<float>(tau) / runningSum) : 1.0f;
        }

        // Search for first peak below threshold
        const int minTau = std::max(2, static_cast<int>(sampleRate_ / 1200.0));
        const int maxTau = std::min(halfBufferSize_ - 1, static_cast<int>(sampleRate_ / 20.0));
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

        // Fallback to limited global minimum search
        if (tauEstimate < 0) {
            float best = 1.0e9f;
            const int limitedMax = std::min(maxTau, minTau + 150); // Limit search range
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
                currentFundamental_ = std::clamp(detected, 20.0f, 800.0f);
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
    int yinHopSize_ = 0;
    int samplesUntilNextYin_ = 0;

    std::array<float, kMaxPartials> phaseOvertones_{};
    std::array<float, kMaxPartials> phaseUndertones_{};

    float currentFundamental_ = 110.0f;
    float smoothedFundamental_ = 110.0f;
    float inputEnvelope_ = 0.0f;
    float synthEnvelope_ = 0.0f;
    float generatedOvertone_ = 0.0f;
    float generatedUndertone_ = 0.0f;
};

AGENTVST_REGISTER_DSP(DualismUndertoneAnchorProcessor)
