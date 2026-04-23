#include <AgentDSP.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

class DualismUndertoneAnchorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = std::max(8000.0, sampleRate);

        yinBufferSize_ = static_cast<int>(sampleRate_ * 0.05); // 50ms
        if ((yinBufferSize_ % 2) != 0)
            ++yinBufferSize_;

        halfBufferSize_ = yinBufferSize_ / 2;
        yinBuffer_.assign(static_cast<size_t>(yinBufferSize_), 0.0f);
        yinDiff_.assign(static_cast<size_t>(halfBufferSize_), 0.0f);

        writeIndex_ = 0;
        yinHopSize_ = std::max(1, static_cast<int>(sampleRate_ * 0.01)); // 10ms updates
        samplesUntilNextYin_ = 0;

        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2)
            return input;
        if (ctx.getParameter("bypass") >= 0.5f)
            return input;

        const float biasNorm = std::clamp(ctx.getParameter("overtone_undertone_bias") / 100.0f, 0.0f, 1.0f);
        const float gravityNorm = std::clamp(ctx.getParameter("gravity") / 100.0f, 0.0f, 1.0f);
        const float mix = std::clamp(ctx.getParameter("mix") / 100.0f, 0.0f, 1.0f);
        const float trackingMs = std::clamp(ctx.getParameter("tracking_speed"), 10.0f, 500.0f);

        if (channel == 0) {
            pushTrackingSample(input);

            --samplesUntilNextYin_;
            if (samplesUntilNextYin_ <= 0) {
                samplesUntilNextYin_ = yinHopSize_;
                computeYin();
            }

            const float smoothingCoeff = std::exp(-1.0f / (0.001f * trackingMs * static_cast<float>(sampleRate_)));
            smoothedFundamental_ =
                smoothingCoeff * smoothedFundamental_ + (1.0f - smoothingCoeff) * currentFundamental_;

            const float absInput = std::abs(input);
            const float attackCoeff = std::exp(-1.0f / (0.003f * static_cast<float>(sampleRate_)));
            const float releaseMs = 80.0f + (gravityNorm * 1200.0f);
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

            updateOscillators(smoothedFundamental_);
            generatedOvertone_ = renderOvertones(smoothedFundamental_);
            generatedUndertone_ = renderUndertones(smoothedFundamental_);
        }

        const float dualismSeries = ((1.0f - biasNorm) * generatedOvertone_) + (biasNorm * generatedUndertone_);
        const float synth = dualismSeries * synthEnvelope_ * (0.7f * gravityNorm);

        const float wet = input + synth;
        return (input * (1.0f - mix)) + (wet * mix);
    }

    void reset() override {
        std::fill(yinBuffer_.begin(), yinBuffer_.end(), 0.0f);
        std::fill(yinDiff_.begin(), yinDiff_.end(), 0.0f);
        phaseOvertones_.fill(0.0f);
        phaseUndertones_.fill(0.0f);

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
    static constexpr int kPartials = 4;

    void pushTrackingSample(float input) noexcept {
        if (yinBufferSize_ <= 0)
            return;
        yinBuffer_[writeIndex_] = input;
        writeIndex_ = (writeIndex_ + 1) % yinBufferSize_;
    }

    void updateOscillators(float fundamental) noexcept {
        const float safeFund = std::clamp(fundamental, 40.0f, 1200.0f);
        const float nyquist = static_cast<float>(sampleRate_) * 0.5f;

        for (int i = 0; i < kPartials; ++i) {
            const float overtoneFreq = std::min(safeFund * static_cast<float>(i + 2), nyquist * 0.98f);
            const float undertoneFreq = safeFund / static_cast<float>(i + 2);

            phaseOvertones_[i] = wrapPhase(phaseOvertones_[i] + (kTwoPi * overtoneFreq / static_cast<float>(sampleRate_)));
            phaseUndertones_[i] = wrapPhase(phaseUndertones_[i] + (kTwoPi * undertoneFreq / static_cast<float>(sampleRate_)));
        }
    }

    float renderOvertones(float fundamental) const noexcept {
        const float safeFund = std::clamp(fundamental, 40.0f, 1200.0f);
        const float nyquist = static_cast<float>(sampleRate_) * 0.5f;

        float sum = 0.0f;
        float norm = 0.0f;

        for (int i = 0; i < kPartials; ++i) {
            const float freq = safeFund * static_cast<float>(i + 2);
            if (freq >= nyquist)
                continue;

            const float weight = 1.0f / static_cast<float>(i + 1);
            sum += std::sin(phaseOvertones_[i]) * weight;
            norm += weight;
        }

        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }

    float renderUndertones(float fundamental) const noexcept {
        const float safeFund = std::clamp(fundamental, 40.0f, 1200.0f);

        float sum = 0.0f;
        float norm = 0.0f;

        for (int i = 0; i < kPartials; ++i) {
            const float weight = 1.0f / static_cast<float>(i + 1);
            const float freqScale = std::sqrt((safeFund / static_cast<float>(i + 2)) / safeFund);
            sum += std::sin(phaseUndertones_[i]) * weight * freqScale;
            norm += weight;
        }

        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }

    void computeYin() noexcept {
        if (halfBufferSize_ < 4)
            return;

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
            yinDiff_[tau] = (runningSum > 0.0f) ? (yinDiff_[tau] * static_cast<float>(tau) / runningSum) : 1.0f;
        }

        const int minTau = std::max(2, static_cast<int>(sampleRate_ / 1200.0));
        const int maxTau = std::min(halfBufferSize_ - 1, static_cast<int>(sampleRate_ / 40.0));
        if (maxTau <= minTau)
            return;

        const float threshold = 0.15f;
        int tauEstimate = -1;

        for (int tau = minTau; tau <= maxTau; ++tau) {
            if (yinDiff_[tau] < threshold) {
                while ((tau + 1) <= maxTau && yinDiff_[tau + 1] < yinDiff_[tau]) {
                    ++tau;
                }
                tauEstimate = tau;
                break;
            }
        }

        if (tauEstimate < 0) {
            float best = 1.0e9f;
            for (int tau = minTau; tau <= maxTau; ++tau) {
                if (yinDiff_[tau] < best) {
                    best = yinDiff_[tau];
                    tauEstimate = tau;
                }
            }
        }

        if (tauEstimate > 0) {
            const float detected = static_cast<float>(sampleRate_) / static_cast<float>(tauEstimate);
            if (std::isfinite(detected)) {
                currentFundamental_ = std::clamp(detected, 40.0f, 1200.0f);
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

    std::array<float, kPartials> phaseOvertones_{};
    std::array<float, kPartials> phaseUndertones_{};

    float currentFundamental_ = 110.0f;
    float smoothedFundamental_ = 110.0f;
    float inputEnvelope_ = 0.0f;
    float synthEnvelope_ = 0.0f;
    float generatedOvertone_ = 0.0f;
    float generatedUndertone_ = 0.0f;
};

AGENTVST_REGISTER_DSP(DualismUndertoneAnchorProcessor)
