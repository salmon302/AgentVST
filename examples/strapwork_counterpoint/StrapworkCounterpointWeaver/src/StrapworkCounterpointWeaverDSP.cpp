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
        delayBuffers_.assign(6, std::vector<float>(static_cast<size_t>(sampleRate_ * 3.0), 0.0f)); // up to 3 sec max
        writeIdx_.assign(6, 0);
        
        // Initialize phases for LFO modulated delay
        for (int i = 0; i < 6; ++i) {
            lfoPhase_[i] = static_cast<float>(i) / 6.0f; 
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input; // Only process L/R

        int maxStrands = static_cast<int>(std::clamp(ctx.getParameter("knot_complexity"), 1.0f, 6.0f));
        float contraryBias = ctx.getParameter("contrary_bias") / 100.0f;
        float ducking = ctx.getParameter("over_under_ducking") / 100.0f;
        float mix = ctx.getParameter("mix") / 100.0f;

        float output = 0.0f;

        // Frequencies for the strands (the "weaving" motion)
        float baseFreq = 0.5f;

        // Process each strand
        std::vector<float> tapVals(maxStrands, 0.0f);
        std::vector<float> readOffsets(maxStrands, 0.0f);
        
        for (int i = 0; i < maxStrands; ++i) {
            float phase = lfoPhase_[i];
            
            // Calculate delay motion (weaving effect)
            float lfo = std::sin(phase * 2.0f * static_cast<float>(M_PI));
            
            // Mix in contrary motion
            if (i % 2 != 0) {
                lfo = (lfo * (1.0f - contraryBias)) + (-lfo * contraryBias);
            }
            
            // Map LFO to delay time (e.g., 50ms to 400ms)
            float delayMs = 225.0f + lfo * 175.0f;
            float delaySamples = delayMs * (static_cast<float>(sampleRate_) / 1000.0f);
            readOffsets[i] = delaySamples;
            
            int bSize = static_cast<int>(delayBuffers_[i].size());
            int rIdx = writeIdx_[i] - static_cast<int>(delaySamples);
            if (rIdx < 0) rIdx += bSize;
            
            float tap = delayBuffers_[i][rIdx];
            tapVals[i] = tap;
            
            // Advance phase 
            // In a true implementation this would track pitch of the incoming signal
            lfoPhase_[i] += (baseFreq + (i * 0.1f)) / static_cast<float>(sampleRate_);
            if (lfoPhase_[i] > 1.0f) lfoPhase_[i] -= 1.0f;
            
            // Advance Write index and push new sample to buffer
            delayBuffers_[i][writeIdx_[i]] = input;
            writeIdx_[i] = (writeIdx_[i] + 1) % bSize;
        }
        
        // Emulate ducking when strands "cross" each other
        for (int i = 0; i < maxStrands; ++i) {
            float interactionMult = 1.0f;
            for (int j = 0; j < maxStrands; ++j) {
                if (i != j) {
                    // Check if delay lines are crossing
                    float diff = std::abs(readOffsets[i] - readOffsets[j]);
                    if (diff < 500.0f) { // Within 500 samples of each other
                        // Apply dynamic ducking/phase shift to represent physical overlap
                        interactionMult -= ducking * (1.0f - (diff / 500.0f)) * 0.5f;
                    }
                }
            }
            output += tapVals[i] * std::clamp(interactionMult, 0.0f, 1.0f);
        }
        
        // Normalize
        output /= static_cast<float>(maxStrands);

        return input * (1.0f - mix) + output * mix;
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<std::vector<float>> delayBuffers_;
    std::vector<int> writeIdx_;
    float lfoPhase_[6];
};

AGENTVST_REGISTER_DSP(StrapworkCounterpointWeaverProcessor)
