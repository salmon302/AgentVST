/**
 * TonnetzTessellator DSP
 * PERFORMANCE: Added parameter caching at block start
 */
#include <AgentDSP.h>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TonnetzTessellatorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        for(int i=0; i<6; ++i) phase_[i] = 0.0f;
        
        // PERFORMANCE: Initialize parameter cache
        cachedAnchor_ = 0.5f;
        cachedTransX_ = 0.0f;
        cachedTransY_ = 0.0f;
        cachedSpread_ = 0.5f;
        cachedMix_ = 0.5f;
        lastBlockStart_ = -1;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        // PERFORMANCE: Cache parameters at block start
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedAnchor_ = ctx.getParameter("geometric_anchor") / 100.0f;
            cachedTransX_ = ctx.getParameter("transformation_x");
            cachedTransY_ = ctx.getParameter("transformation_y");
            cachedSpread_ = ctx.getParameter("tessellation_spread") / 100.0f;
            cachedMix_ = ctx.getParameter("mix") / 100.0f;
        }

        if (channel >= 2) return input;

        // Use cached parameter values
        float anchor = cachedAnchor_;
        float transX = cachedTransX_;
        float transY = cachedTransY_;
        float spread = cachedSpread_;
        float mix = cachedMix_;

        // Abstract representation of Tonnetz shifts
        float baseFreq = 220.0f * std::pow(2.0f, transX * 2.0f); 
        float freq1 = baseFreq * 1.25f;
        float freq2 = baseFreq * 1.5f;
        
        freq1 *= std::pow(1.05946f, transY * 4.0f);
        freq2 *= std::pow(1.05946f, -transY * 4.0f);

        int phaseIdx1 = channel * 3;
        int phaseIdx2 = channel * 3 + 1;

        float synth1 = std::sin(phase_[phaseIdx1] * 2.0f * static_cast<float>(M_PI));
        float synth2 = std::sin(phase_[phaseIdx2] * 2.0f * static_cast<float>(M_PI));

        phase_[phaseIdx1] += freq1 / sampleRate_;
        if (phase_[phaseIdx1] > 1.0f) phase_[phaseIdx1] -= 1.0f;
        
        phase_[phaseIdx2] += freq2 / sampleRate_;
        if (phase_[phaseIdx2] > 1.0f) phase_[phaseIdx2] -= 1.0f;

        float absInput = std::abs(input);
        float harmony = (synth1 + synth2) * 0.5f * absInput * spread * anchor;
        
        float out = input + harmony;

        return input * (1.0f - mix) + out * mix;
    }

private:
    double sampleRate_ = 44100.0;
    float phase_[6];
    
    // PERFORMANCE: Cached parameter values
    float cachedAnchor_ = 0.5f;
    float cachedTransX_ = 0.0f;
    float cachedTransY_ = 0.0f;
    float cachedSpread_ = 0.5f;
    float cachedMix_ = 0.5f;
    std::int64_t lastBlockStart_ = -1;
};

AGENTVST_REGISTER_DSP(TonnetzTessellatorProcessor)
