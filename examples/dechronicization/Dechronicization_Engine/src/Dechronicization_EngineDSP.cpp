#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Dechronicization_EngineDSP : public AgentVST::IAgentDSP {
public:
    Dechronicization_EngineDSP() {
        for (int i = 0; i < kMaxChannels; ++i) {
            delayBuffer_[i].resize(kBufferSize, 0.0f);
        }
    }

    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        reset();
    }

    void reset() override {
        writePos_ = 0;
        phase_ = 0.0f;
        for (int i = 0; i < kMaxChannels; ++i) {
            std::fill(delayBuffer_[i].begin(), delayBuffer_[i].end(), 0.0f);
        }
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        if (channel >= kMaxChannels) return input;
        
        float preCognitionMs_ = ctx.getParameter("pre_cognition");
        float temporalConflict_ = ctx.getParameter("temporal_conflict");
        float ghostMix_ = ctx.getParameter("ghost_mix");
        
        float preCognitionSamples = std::clamp(preCognitionMs_ * (float)sampleRate_ / 1000.0f, 10.0f, (float)(kBufferSize / 2) - 1.0f);
        float conflictRatio = temporalConflict_;
        float ghostMix = ghostMix_;

        if (channel == 0) {
            phase_ += 1.0f / preCognitionSamples;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
        }

        float phase1 = phase_;
        float phase2 = phase_ + 0.5f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        float window1 = 0.5f * (1.0f - std::cos(2.0f * M_PI * phase1));
        float window2 = 0.5f * (1.0f - std::cos(2.0f * M_PI * phase2));

        delayBuffer_[channel][writePos_] = input;

        int dryReadPos = writePos_ - static_cast<int>(preCognitionSamples);
        if (dryReadPos < 0) dryReadPos += kBufferSize;
        float drySignal = delayBuffer_[channel][dryReadPos];

        float readOffset1 = phase1 * preCognitionSamples * conflictRatio;
        float readOffset2 = phase2 * preCognitionSamples * conflictRatio;

        float ghost1 = getInterpolatedSample(channel, static_cast<float>(writePos_) - readOffset1);
        float ghost2 = getInterpolatedSample(channel, static_cast<float>(writePos_) - readOffset2);

        float ghostSignal = (ghost1 * window1) + (ghost2 * window2);

        if (channel == ctx.numChannels - 1) {
            writePos_++;
            if (writePos_ >= kBufferSize) {
                writePos_ = 0;
            }
        }

        return (drySignal * (1.0f - ghostMix)) + (ghostSignal * ghostMix);
    }

private:
    float getInterpolatedSample(int channel, float targetPos) const {
        while (targetPos < 0.0f) targetPos += static_cast<float>(kBufferSize);
        while (targetPos >= static_cast<float>(kBufferSize)) targetPos -= static_cast<float>(kBufferSize);

        int idx1 = static_cast<int>(targetPos);
        int idx2 = (idx1 + 1) % kBufferSize;
        float frac = targetPos - static_cast<float>(idx1);

        return delayBuffer_[channel][idx1] * (1.0f - frac) + delayBuffer_[channel][idx2] * frac;
    }

    static constexpr int kMaxChannels = 2;
    static constexpr int kBufferSize = 384000; 
    
    std::vector<float> delayBuffer_[kMaxChannels];
    int writePos_ = 0;
    double sampleRate_ = 44100.0;
    float phase_ = 0.0f;
};

AGENTVST_REGISTER_DSP(Dechronicization_EngineDSP)
