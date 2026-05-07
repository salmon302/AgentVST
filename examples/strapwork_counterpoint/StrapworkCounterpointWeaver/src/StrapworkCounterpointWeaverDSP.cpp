// Purpose: StrapworkCounterpointWeaver DSP processing.
// Author: Seth Nenninger (GPT-5.2-Codex Agent)
// Timestamp: 2026-05-06T18:58:46Z
// Changelog: Strengthen counterpoint weave and clamp delay modulation.

/**
 * StrapworkCounterpointWeaver DSP
 * PERFORMANCE: Added parameter caching, reduced buffer size, simplified ducking
 */
#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class StrapworkCounterpointWeaverProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        
        // PERFORMANCE: Reduced buffer from 3s to 1s per strand
        delayBuffers_.assign(6, std::vector<float>(static_cast<size_t>(sampleRate_ * 1.0), 0.0f));
        writeIdx_.assign(6, 0);
        
        for (int i = 0; i < 6; ++i) {
            lfoPhase_[i] = static_cast<float>(i) / 6.0f; 
            feedbackState_[i] = 0.0f;
        }
        
        // PERFORMANCE: Initialize parameter cache
        cachedKnotComplexity_ = 3.0f;
        cachedContraryBias_ = 0.0f;
        cachedDucking_ = 0.5f;
        cachedMix_ = 0.5f;
        lastBlockStart_ = -1;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        // PERFORMANCE: Cache parameters at block start
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedKnotComplexity_ = ctx.getParameter("knot_complexity");
            cachedContraryBias_ = ctx.getParameter("contrary_bias") / 100.0f;
            cachedDucking_ = ctx.getParameter("over_under_ducking") / 100.0f;
            cachedMix_ = ctx.getParameter("mix") / 100.0f;
        }

        if (channel >= 2) return input;

        int maxStrands = static_cast<int>(std::clamp(cachedKnotComplexity_, 1.0f, 6.0f));
        float contraryBias = cachedContraryBias_;
        float ducking = cachedDucking_;
        float mix = cachedMix_;

        float output = 0.0f;
        float baseFreq = 0.25f + contraryBias * 0.20f;
        float baseDelayMs = 120.0f + contraryBias * 40.0f;
        float modDepthMs = 120.0f + contraryBias * 80.0f;
        float strideMs = 30.0f + contraryBias * 15.0f;
        float weaveDepth = 0.15f + contraryBias * 0.45f;

        // Process each strand
        float tapVals[6] = {0.0f};
        float readOffsets[6] = {0.0f};
        
        for (int i = 0; i < maxStrands; ++i) {
            float phase = lfoPhase_[i];
            
            float lfo = std::sin(phase * 2.0f * static_cast<float>(M_PI));
            
            if (i % 2 != 0) {
                lfo = (lfo * (1.0f - contraryBias)) + (-lfo * contraryBias);
            }
            
            float delayMs = baseDelayMs + (i * strideMs) + lfo * modDepthMs;
            delayMs = std::max(20.0f, delayMs);
            float delaySamples = delayMs * (static_cast<float>(sampleRate_) / 1000.0f);
            readOffsets[i] = delaySamples;
            
            int bSize = static_cast<int>(delayBuffers_[i].size());
            float rPos = static_cast<float>(writeIdx_[i]) - delaySamples;
            while (rPos < 0.0f) rPos += bSize;
            while (rPos >= bSize) rPos -= bSize;
            int rIdx0 = static_cast<int>(rPos);
            int rIdx1 = (rIdx0 + 1) % bSize;
            float frac = rPos - static_cast<float>(rIdx0);
            tapVals[i] = delayBuffers_[i][rIdx0] * (1.0f - frac) + delayBuffers_[i][rIdx1] * frac;
            
            lfoPhase_[i] += (baseFreq + (i * 0.07f)) / static_cast<float>(sampleRate_);
            if (lfoPhase_[i] > 1.0f) lfoPhase_[i] -= 1.0f;
        }
        
        // PERFORMANCE: Simplified ducking (O(n) instead of O(n²))
        // Only check adjacent strands
        float feedbackGain = 0.15f + ducking * 0.35f;
        float crossGain = 0.07f + contraryBias * 0.25f;
        for (int i = 0; i < maxStrands; ++i) {
            float interactionMult = 1.0f;
            int prevIdx = (i > 0) ? i - 1 : maxStrands - 1;
            int nextIdx = (i < maxStrands - 1) ? i + 1 : 0;
            
            float diff1 = std::abs(readOffsets[i] - readOffsets[prevIdx]);
            float diff2 = std::abs(readOffsets[i] - readOffsets[nextIdx]);
            
            if (diff1 < 500.0f) {
                interactionMult -= ducking * (1.0f - (diff1 / 500.0f)) * 0.5f;
            }
            if (diff2 < 500.0f) {
                interactionMult -= ducking * (1.0f - (diff2 / 500.0f)) * 0.5f;
            }
            
            float opposite = (i % 2 == 0) ? 1.0f : -1.0f;
            float woven = tapVals[i] * (1.0f - weaveDepth) + tapVals[nextIdx] * weaveDepth * opposite;
            output += woven * std::clamp(interactionMult, 0.0f, 1.0f);

            int bSize = static_cast<int>(delayBuffers_[i].size());
            int neighborIdx = (i + 1) % maxStrands;
            feedbackState_[i] += (tapVals[i] - feedbackState_[i]) * 0.2f;
            float writeSample = input + (feedbackState_[i] * feedbackGain) + (tapVals[neighborIdx] * crossGain * opposite);
            delayBuffers_[i][writeIdx_[i]] = writeSample;
            writeIdx_[i] = (writeIdx_[i] + 1) % bSize;
        }
        
        output /= static_cast<float>(maxStrands);

        return input * (1.0f - mix) + output * mix;
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<std::vector<float>> delayBuffers_;
    std::vector<int> writeIdx_;
    float lfoPhase_[6];
    float feedbackState_[6];
    
    // PERFORMANCE: Cached parameter values
    float cachedKnotComplexity_ = 3.0f;
    float cachedContraryBias_ = 0.0f;
    float cachedDucking_ = 0.5f;
    float cachedMix_ = 0.5f;
    std::int64_t lastBlockStart_ = -1;
};

AGENTVST_REGISTER_DSP(StrapworkCounterpointWeaverProcessor)
