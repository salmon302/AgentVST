#include <AgentDSP.h>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// A simplified implementation of a "Consonance Enforcer" that uses a comb filter
// tuned to perfect integer multiples of the root frequency to filter out
// inharmonic frequencies, emphasizing the Just Intonation series.
class JustIntonationMuqarnasProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        bufferSize_ = (int)sampleRate; // 1 second buffer
        
        for (int ch = 0; ch < 2; ++ch) {
            delayBuffer_[ch].assign(bufferSize_, 0.0f);
            writePos_[ch] = 0;
            delayBuffer2_[ch].assign(bufferSize_, 0.0f);
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float rootHz = ctx.getParameter("unity_root");
        float division = ctx.getParameter("muqarnas_division"); // acts as feedback/sharpness
        float gravity = ctx.getParameter("consonance_gravity") / 100.0f;

        // Comb filter tuned to root Hz
        float delaySamples = static_cast<float>(sampleRate_) / rootHz;
        float feedback = (division / 1024.0f) * 0.99f; // max feedback 0.99
        
        // Two cascade bands for purity
        float combOut = processComb(channel, input, delaySamples, feedback);
        float combOut2 = processComb2(channel, combOut, delaySamples, feedback);

        float purelyConsonant = combOut2 * (1.0f - feedback); // rudimentary gain compensation
        float output = (input * (1.0f - gravity)) + (purelyConsonant * gravity);

        return output;
    }

    float processComb(int channel, float input, float delaySamples, float feedback) {
        int delayInt = static_cast<int>(delaySamples);
        float frac = delaySamples - delayInt;
        
        int readPos1 = writePos_[channel] - delayInt;
        if (readPos1 < 0) readPos1 += bufferSize_;
        
        int readPos2 = readPos1 - 1;
        if (readPos2 < 0) readPos2 += bufferSize_;
        
        float delayed = delayBuffer_[channel][readPos1] * (1.0f - frac) + delayBuffer_[channel][readPos2] * frac;
        
        float filtered = input + delayed * feedback;
        
        delayBuffer_[channel][writePos_[channel]] = filtered;
        writePos_[channel] = (writePos_[channel] + 1) % bufferSize_;
        
        return filtered;
    }
    
    float processComb2(int channel, float input, float delaySamples, float feedback) {
        int delayInt = static_cast<int>(delaySamples);
        float frac = delaySamples - delayInt;
        
        // Use writePos_[channel] - 1 as previous writePos since writePos was just incremented in processComb
        int writePos = (writePos_[channel] - 1);
        if (writePos < 0) writePos += bufferSize_;
        
        int readPos1 = writePos - delayInt;
        if (readPos1 < 0) readPos1 += bufferSize_;
        
        int readPos2 = readPos1 - 1;
        if (readPos2 < 0) readPos2 += bufferSize_;
        
        float delayed = delayBuffer2_[channel][readPos1] * (1.0f - frac) + delayBuffer2_[channel][readPos2] * frac;
        
        float filtered = input + delayed * feedback;
        
        delayBuffer2_[channel][writePos] = filtered; 
        
        return filtered;
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            std::fill(delayBuffer_[ch].begin(), delayBuffer_[ch].end(), 0.0f);
            std::fill(delayBuffer2_[ch].begin(), delayBuffer2_[ch].end(), 0.0f);
            writePos_[ch] = 0;
        }
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<float> delayBuffer_[2];
    std::vector<float> delayBuffer2_[2];
    int bufferSize_ = 0;
    int writePos_[2] = {0, 0};
};

AGENTVST_REGISTER_DSP(JustIntonationMuqarnasProcessor)
