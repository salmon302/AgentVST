/**
 * Generated DSP scaffold for Ring Modulator of the Void
 *
 * Requirements:
 * - Monophonic Pitch tracking (YIN)
 * - Synthesize Sine/Triangle carrier locked at sqrt(2) * detected frequency.
 */
#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>

class Ring_Modulator_of_the_VoidProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = std::max(8000.0, sampleRate);
        
        // Setup YIN buffers
        yinBufferSize_ = static_cast<int>(sampleRate_ * 0.05); // 50ms buffer for ~20Hz lowest pitch
        if (yinBufferSize_ % 2 != 0) yinBufferSize_++;
        halfBufferSize_ = yinBufferSize_ / 2;
        
        yinBuffer_.assign(yinBufferSize_, 0.0f);
        yinDiff_.assign(halfBufferSize_, 0.0f);
        writeIndex_ = 0;
        
        currentFundamental_ = 440.0f;
        smoothedFundamental_ = 440.0f;
        carrierPhase_ = 0.0f;
        lfoPhase_ = 0.0f;
        
        samplesUntilNextYin_ = 0;
        yinHopSize_ = static_cast<int>(sampleRate_ * 0.01); // Update every 10ms
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f)
            return input;

        // Only track pitch on channel 0 to save CPU
        if (channel == 0) {
            yinBuffer_[writeIndex_] = input;
            writeIndex_ = (writeIndex_ + 1) % yinBufferSize_;
            
            samplesUntilNextYin_--;
            if (samplesUntilNextYin_ <= 0) {
                samplesUntilNextYin_ = yinHopSize_;
                computeYin();
            }
            
            const float trackingSpeedMs = std::clamp(ctx.getParameter("tracking_speed"), 10.0f, 500.0f);
            const float smoothCoeff = std::exp(-1.0f / (0.001f * trackingSpeedMs * static_cast<float>(sampleRate_)));
            smoothedFundamental_ = smoothCoeff * smoothedFundamental_ + (1.0f - smoothCoeff) * currentFundamental_;
            
            const float driftAmount = std::clamp(ctx.getParameter("carrier_drift"), 0.0f, 1.0f);
            lfoPhase_ += (2.0f * kPi * 5.0f) / static_cast<float>(sampleRate_); // 5Hz LFO
            if (lfoPhase_ > 2.0f * kPi) lfoPhase_ -= 2.0f * kPi;
            float drift = 1.0f + (driftAmount * 0.05f * std::sin(lfoPhase_)); // +/- 5% max drift
            
            const float carrierFreq = smoothedFundamental_ * kSqrt2 * drift;
            carrierPhase_ += (2.0f * kPi * carrierFreq) / static_cast<float>(sampleRate_);
            if (carrierPhase_ > 2.0f * kPi) carrierPhase_ -= 2.0f * kPi;
        }

        const float carrier = std::sin(carrierPhase_);
        const float inharmonicity = std::clamp(ctx.getParameter("inharmonicity"), 0.0f, 1.0f);
        
        return input * (1.0f - inharmonicity) + (input * carrier) * inharmonicity;
    }

    void reset() override {
        std::fill(yinBuffer_.begin(), yinBuffer_.end(), 0.0f);
        writeIndex_ = 0;
        currentFundamental_ = 440.0f;
        smoothedFundamental_ = 440.0f;
        carrierPhase_ = 0.0f;
        lfoPhase_ = 0.0f;
        samplesUntilNextYin_ = 0;
    }

private:
    static constexpr float kSqrt2 = 1.41421356237f;
    static constexpr float kPi = 3.14159265359f;

    double sampleRate_ = 44100.0;

    std::vector<float> yinBuffer_;
    std::vector<float> yinDiff_;
    int yinBufferSize_ = 0;
    int halfBufferSize_ = 0;
    int writeIndex_ = 0;
    int samplesUntilNextYin_ = 0;
    int yinHopSize_ = 0;

    float currentFundamental_ = 440.0f;
    float smoothedFundamental_ = 440.0f;
    float carrierPhase_ = 0.0f;
    float lfoPhase_ = 0.0f;

    void computeYin() noexcept {
        // 1. Difference function
        for (int tau = 0; tau < halfBufferSize_; tau++) {
            float diff = 0.0f;
            for (int i = 0; i < halfBufferSize_; i++) {
                int idx1 = (writeIndex_ - yinBufferSize_ + i + yinBufferSize_) % yinBufferSize_;
                int idx2 = (writeIndex_ - yinBufferSize_ + i + tau + yinBufferSize_) % yinBufferSize_;
                float delta = yinBuffer_[idx1] - yinBuffer_[idx2];
                diff += delta * delta;
            }
            yinDiff_[tau] = diff;
        }

        // 2. Cumulative mean normalized difference
        yinDiff_[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau < halfBufferSize_; tau++) {
            runningSum += yinDiff_[tau];
            if (runningSum > 0.0f) {
                yinDiff_[tau] = yinDiff_[tau] * tau / runningSum;
            } else {
                yinDiff_[tau] = 1.0f;
            }
        }

        // 3. Absolute threshold
        const float threshold = 0.15f;
        int tauEstimate = -1;
        for (int tau = 2; tau < halfBufferSize_; tau++) {
            if (yinDiff_[tau] < threshold) {
                while (tau + 1 < halfBufferSize_ && yinDiff_[tau + 1] < yinDiff_[tau]) {
                    tau++;
                }
                tauEstimate = tau;
                break;
            }
        }

        if (tauEstimate == -1) {
            // Fallback: global minimum
            float minVal = 1e9f;
            for (int tau = 2; tau < halfBufferSize_; tau++) {
                if (yinDiff_[tau] < minVal) {
                    minVal = yinDiff_[tau];
                    tauEstimate = tau;
                }
            }
        }

        if (tauEstimate > 0) {
            currentFundamental_ = static_cast<float>(sampleRate_) / static_cast<float>(tauEstimate);
            // Clamp to reasonable pitch range
            currentFundamental_ = std::clamp(currentFundamental_, 20.0f, 2000.0f);
        }
    }
};

AGENTVST_REGISTER_DSP(Ring_Modulator_of_the_VoidProcessor)
