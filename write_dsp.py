import sys

content = """#include <AgentDSP.h>
#include "modules/BiquadFilter.h"
#include <vector>
#include <cmath>
#include <algorithm>

class OceanicHaasWidenerProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        for (int ch = 0; ch < 2; ++ch) {
            delayBuffer_[ch].assign(65536, 0.0f);
            writeIdx_[ch] = 0;
            
            horizonLp_[ch].prepare(sampleRate, maxBlockSize);
            horizonLp_[ch].setType(AgentVST::BiquadFilter::Type::HighShelf);
            horizonLp_[ch].setQ(0.707f);
            
            horizonHp_[ch].prepare(sampleRate, maxBlockSize);
            horizonHp_[ch].setType(AgentVST::BiquadFilter::Type::LowShelf);
            horizonHp_[ch].setQ(0.707f);
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        const float widthMs  = ctx.getParameter("width_array_ms");
        const float horizon  = ctx.getParameter("horizon_filter");
        const float centerPts= ctx.getParameter("center_anchor");
        const float mixPct   = ctx.getParameter("mix");

        if (channel == 0 && horizon != lastHorizon_) {
            for (int c = 0; c < 2; ++c) {
                if (horizon < 0.0f) {
                    horizonLp_[c].setFrequency(2000.0f);
                    horizonLp_[c].setGainDb(horizon * 24.0f); 
                    horizonHp_[c].setGainDb(0.0f);
                } else {
                    horizonHp_[c].setFrequency(400.0f);
                    horizonHp_[c].setGainDb(-horizon * 24.0f);
                    horizonLp_[c].setGainDb(0.0f);
                }
            }
            lastHorizon_ = horizon;
        }

        delayBuffer_[channel][writeIdx_[channel]] = input;

        float delaySamples = static_cast<float>((widthMs / 1000.0) * sampleRate_);
        float readPos = writeIdx_[channel] - delaySamples;
        if (readPos < 0.0f) readPos += 65536.0f;

        int readInt = static_cast<int>(readPos);
        float frac = readPos - static_cast<float>(readInt);
        int readNext = (readInt + 1) & 65535;
        
        float delayed = delayBuffer_[channel][readInt] * (1.0f - frac) + 
                        delayBuffer_[channel][readNext] * frac;

        delayed = horizonLp_[channel].processSample(channel, delayed);
        delayed = horizonHp_[channel].processSample(channel, delayed);

        writeIdx_[channel] = (writeIdx_[channel] + 1) & 65535;

        float effectSignal = input;
        if (channel == 1) {
            effectSignal = delayed;
        }

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
        lastHorizon_ = -2.0f;
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<float> delayBuffer_[2];
    int writeIdx_[2] = {0, 0};
    AgentVST::BiquadFilter horizonLp_[2];
    AgentVST::BiquadFilter horizonHp_[2];
    float lastHorizon_ = -2.0f;
};

AGENTVST_REGISTER_DSP(OceanicHaasWidenerProcessor)
"""

with open('examples/oceanic_haas_widener/OceanicHaasWidener/src/OceanicHaasWidenerDSP.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
