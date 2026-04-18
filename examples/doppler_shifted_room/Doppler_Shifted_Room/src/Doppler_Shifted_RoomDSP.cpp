#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class DelayLine {
public:
    void resize(int size) {
        if (size <= 0) size = 1;
        buffer.assign(size, 0.0f);
        writeIndex = 0;
    }
    
    void push(float input) {
        buffer[writeIndex] = input;
        writeIndex = (writeIndex + 1) % buffer.size();
    }
    
    float read(float delaySamples) const {
        if (buffer.empty()) return 0.0f;
        
        float readPos = static_cast<float>(writeIndex) - delaySamples;
        while (readPos < 0.0f) readPos += static_cast<float>(buffer.size());
        
        int i0 = static_cast<int>(readPos);
        int i1 = (i0 + 1) % buffer.size();
        float frac = readPos - i0;
        
        return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
    }
    
private:
    std::vector<float> buffer;
    int writeIndex = 0;
};

class ModCombFilter {
public:
    void init(float baseDelayMs, float sampleRate) {
        fs = sampleRate;
        baseDelaySamples = baseDelayMs * sampleRate / 1000.0f;
        // Headroom for mod: up to 20ms extra
        delay.resize(static_cast<int>(baseDelaySamples + 20.0f * sampleRate / 1000.0f + 10.0f));
    }
    
    float process(float input, float feedback, float lfoVal, float modDepthSamples) {
        float currentDelay = baseDelaySamples + lfoVal * modDepthSamples;
        currentDelay = std::max(1.0f, currentDelay);
        float delayed = delay.read(currentDelay);
        
        // Anti-aliasing LP filter on feedback
        lowpassOut = delayed * 0.5f + lowpassOut * 0.5f;
        
        float out = input + lowpassOut * feedback;
        delay.push(out);
        return delayed;
    }
    
private:
    DelayLine delay;
    float baseDelaySamples = 0.0f;
    float fs = 44100.0f;
    float lowpassOut = 0.0f;
};

class AllpassFilter {
public:
    void init(float delayMs, float sampleRate) {
        int size = static_cast<int>(delayMs * sampleRate / 1000.0f);
        delay.resize(std::max(1, size + 10));
        delaySamples = delayMs * sampleRate / 1000.0f;
    }
    
    float process(float input) {
        float delayed = delay.read(delaySamples);
        float out = -input + delayed;
        delay.push(input + delayed * 0.5f); // feedback = 0.5
        return out;
    }

private:
    DelayLine delay;
    float delaySamples = 0.0f;
};

class Doppler_Shifted_RoomProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        fs = sampleRate;
        
        // Schroeder Reverb Setup
        float cBase[4] = {29.7f, 37.1f, 41.1f, 43.7f};
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < 4; ++i) {
                combs[ch][i].init(cBase[i] + ch * 1.5f, fs);
            }
            apf1[ch].init(5.0f + ch * 0.2f, fs);
            apf2[ch].init(1.7f + ch * 0.1f, fs);
        }
        
        lfoPhase = 0.0f;
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        float roomVelocity = ctx.getParameter("room_velocity"); // 0.0 to 100.0
        float expansionCycle = ctx.getParameter("expansion_cycle"); // 0.1 to 10.0
        float acousticMass = ctx.getParameter("acoustic_mass"); // 0.1 to 10.0
        float mix = ctx.getParameter("mix") / 100.0f;
        
        // Update LFO safely per sample (for both channels symmetrically or L channel advances it)
        // Wait, processSample is typically called for Left then Right per sample slice.
        // It's better to update LFO per sample and not per channel wrapper. 
        // We'll update only on channel 0
        if (channel == 0) {
            float phaseInc = 2.0f * M_PI * expansionCycle / fs;
            lfoPhase += phaseInc;
            if (lfoPhase >= 2.0f * M_PI) lfoPhase -= 2.0f * M_PI;
        }
        
        float lfoVal = std::sin(lfoPhase);
        
        // Approx feedback derived from acoustic mass
        float targetFeedback = std::exp(-3.0f * (40.0f / 1000.0f) / acousticMass);
        
        float reverb = 0.0f;
        // Max 15ms depth
        float modDepth = (roomVelocity / 100.0f) * 15.0f * (fs / 1000.0f);
        
        for (int i = 0; i < 4; ++i) {
            reverb += combs[channel][i].process(input, targetFeedback, lfoVal, modDepth);
        }
        reverb *= 0.25f;
        
        reverb = apf1[channel].process(reverb);
        reverb = apf2[channel].process(reverb);
        
        // Mix dry/wet
        return input * (1.0f - mix) + reverb * mix;
    }

    void reset() override {
        prepare(fs, 0);
    }

private:
    float fs = 44100.0f;
    ModCombFilter combs[2][4];
    AllpassFilter apf1[2];
    AllpassFilter apf2[2];
    float lfoPhase = 0.0f;
};

AGENTVST_REGISTER_DSP(Doppler_Shifted_RoomProcessor)
