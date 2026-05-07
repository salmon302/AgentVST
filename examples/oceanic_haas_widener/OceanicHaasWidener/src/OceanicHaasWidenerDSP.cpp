// Purpose: OceanicHaasWidener DSP processing.
// Author: Seth Nenninger (GPT-5.2-Codex Agent)
// Timestamp: 2026-05-05T04:48:01Z
// Changelog: Smooth width/horizon changes and apply symmetric delay for widening.

#include <AgentDSP.h>
#include "modules/BiquadFilter.h"
#include <vector>
#include <cmath>
#include <algorithm>

class OceanicHaasWidenerProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        delaySmoothCoeff_ = std::exp(-1.0f / (0.02f * static_cast<float>(sampleRate_)));
        horizonSmoothCoeff_ = std::exp(-1.0f / (0.05f * static_cast<float>(sampleRate_)));
        for (int ch = 0; ch < 2; ++ch) {
            delayBuffer_[ch].assign(65536, 0.0f);
            writeIdx_[ch] = 0;
            smoothedDelaySamples_[ch] = 0.0f;
            
            horizonLp_[ch].prepare(sampleRate, maxBlockSize);
            horizonLp_[ch].setType(AgentVST::BiquadFilter::Type::HighShelf);
            horizonLp_[ch].setQ(0.707f);
            
            horizonHp_[ch].prepare(sampleRate, maxBlockSize);
            horizonHp_[ch].setType(AgentVST::BiquadFilter::Type::LowShelf);
            horizonHp_[ch].setQ(0.707f);
        }
        smoothedHorizon_ = 0.0f;
        appliedHorizon_ = -2.0f;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        const float widthMs  = ctx.getParameter("width_array_ms");
        const float horizon  = std::clamp(ctx.getParameter("horizon_filter"), -1.0f, 1.0f);
        const float centerPts= ctx.getParameter("center_anchor");
        const float mixPct   = ctx.getParameter("mix");

        if (channel == 0) {
            smoothedHorizon_ = horizonSmoothCoeff_ * smoothedHorizon_ + (1.0f - horizonSmoothCoeff_) * horizon;
        }

        if (channel == 0 && std::abs(smoothedHorizon_ - appliedHorizon_) > 0.001f) {
            for (int c = 0; c < 2; ++c) {
                if (smoothedHorizon_ < 0.0f) {
                    horizonLp_[c].setFrequency(2000.0f);
                    horizonLp_[c].setGainDb(smoothedHorizon_ * 24.0f); 
                    horizonHp_[c].setGainDb(0.0f);
                } else {
                    horizonHp_[c].setFrequency(400.0f);
                    horizonHp_[c].setGainDb(-smoothedHorizon_ * 24.0f);
                    horizonLp_[c].setGainDb(0.0f);
                }
            }
            appliedHorizon_ = smoothedHorizon_;
        }

        delayBuffer_[channel][writeIdx_[channel]] = input;

        float widthScale = (channel == 0) ? 0.85f : 1.15f;
        float delayTarget = static_cast<float>((widthMs * widthScale / 1000.0) * sampleRate_);
        delayTarget = std::clamp(delayTarget, 1.0f, 65534.0f);
        float& delaySmooth = smoothedDelaySamples_[channel];
        if (delaySmooth <= 0.0f) delaySmooth = delayTarget;
        delaySmooth = delaySmoothCoeff_ * delaySmooth + (1.0f - delaySmoothCoeff_) * delayTarget;

        float readPos = static_cast<float>(writeIdx_[channel]) - delaySmooth;
        while (readPos < 0.0f) readPos += 65536.0f;

        int readInt = static_cast<int>(readPos);
        float frac = readPos - static_cast<float>(readInt);
        int readNext = (readInt + 1) & 65535;
        
        float delayed = delayBuffer_[channel][readInt] * (1.0f - frac) + 
                        delayBuffer_[channel][readNext] * frac;

        delayed = horizonLp_[channel].processSample(channel, delayed);
        delayed = horizonHp_[channel].processSample(channel, delayed);

        writeIdx_[channel] = (writeIdx_[channel] + 1) & 65535;

        float effectSignal = delayed;

        float anchorGain = centerPts / 100.0f;
        float wetOut = (effectSignal * (1.0f - anchorGain)) + (input * anchorGain);
        float wetMix = mixPct / 100.0f;
        
        return (input * (1.0f - wetMix)) + (wetOut * wetMix);
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            std::fill(delayBuffer_[ch].begin(), delayBuffer_[ch].end(), 0.0f);
            writeIdx_[ch] = 0;
            horizonLp_[ch].reset();
            horizonHp_[ch].reset();
        }
        smoothedHorizon_ = 0.0f;
        appliedHorizon_ = -2.0f;
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<float> delayBuffer_[2];
    int writeIdx_[2] = {0, 0};
    AgentVST::BiquadFilter horizonLp_[2];
    AgentVST::BiquadFilter horizonHp_[2];
    float smoothedDelaySamples_[2] = {0.0f, 0.0f};
    float delaySmoothCoeff_ = 0.0f;
    float horizonSmoothCoeff_ = 0.0f;
    float smoothedHorizon_ = 0.0f;
    float appliedHorizon_ = -2.0f;
};

AGENTVST_REGISTER_DSP(OceanicHaasWidenerProcessor)
