#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <algorithm>

class TawhidInfiniteSuspensionProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        buffer_.assign(2, std::vector<float>(static_cast<size_t>(sampleRate_ * 5.0), 0.0f)); // 5s freeze buffer
        writeIdx_ = 0;
        readIdx_ = 0;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float gravity = ctx.getParameter("suspension_gravity") / 100.0f;
        float glideMs = ctx.getParameter("resolution_glide");
        float mix = ctx.getParameter("mix") / 100.0f;

        // Simplified freeze buffer/delay to represent the catching of notes
        int bSize = static_cast<int>(buffer_[channel].size());
        
        // Write to buffer
        buffer_[channel][writeIdx_] = input;
        
        // "Catch" the note by slowing down the read index advancement based on gravity (simulating holding)
        // A true implementation would decouple pitch and time using granular freeze and portamento.
        
        float freezePlayback = buffer_[channel][readIdx_];

        if (channel == 1) { // update indices once per frame (after right channel)
            writeIdx_ = (writeIdx_ + 1) % bSize;
            
            // Advance read index. If gravity is high, we occasionally freeze the read index to sustain the note
            if (static_cast<float>(rand()) / RAND_MAX > (gravity * 0.9f)) {
                readIdx_ = (readIdx_ + 1) % bSize;
            }
        }

        // Output mix
        return input * (1.0f - mix) + freezePlayback * mix;
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<std::vector<float>> buffer_;
    int writeIdx_ = 0;
    int readIdx_ = 0;
};

AGENTVST_REGISTER_DSP(TawhidInfiniteSuspensionProcessor)
