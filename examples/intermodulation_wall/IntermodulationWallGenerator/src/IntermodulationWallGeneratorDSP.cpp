#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <algorithm>

class IntermodulationWallGeneratorProcessor : public AgentVST::IAgentDSP {
    double sampleRate = 44100.0;
    
    struct CombFilter {
        std::vector<float> buffer;
        int writePos = 0;
        float feedback = 0.5f;
        void prepare(int delaySamples, float fb) {
            buffer.assign(delaySamples, 0.0f);
            writePos = 0;
            feedback = fb;
        }
        float process(float in) {
            int delayLength = (int)buffer.size();
            if (delayLength == 0) return in;
            float out = buffer[writePos];
            buffer[writePos] = in + out * feedback;
            writePos = (writePos + 1) % delayLength;
            return out;
        }
    };
    
    struct AllPassFilter {
        std::vector<float> buffer;
        int writePos = 0;
        float gain = 0.5f;
        void prepare(int delaySamples, float g) {
            buffer.assign(delaySamples, 0.0f);
            writePos = 0;
            gain = g;
        }
        float process(float in) {
            int delayLength = (int)buffer.size();
            if (delayLength == 0) return in;
            float delayedOut = buffer[writePos];
            float out = -in + delayedOut;
            buffer[writePos] = in + delayedOut * gain;
            writePos = (writePos + 1) % delayLength;
            return out;
        }
    };
    
    CombFilter comb[4];
    AllPassFilter allpass[2];
    float envValue = 0.0f;
    float attack = 0.0f;
    float release = 0.0f;
    
public:
    void prepare(double sr, int maxBlockSize) override {
        sampleRate = sr;
        comb[0].prepare((int)(sr * 0.0297), 0.8f);
        comb[1].prepare((int)(sr * 0.0371), 0.8f);
        comb[2].prepare((int)(sr * 0.0411), 0.8f);
        comb[3].prepare((int)(sr * 0.0437), 0.8f);
        
        allpass[0].prepare((int)(sr * 0.005), 0.7f);
        allpass[1].prepare((int)(sr * 0.0017), 0.7f);
        
        attack = std::exp(-1.0f / (0.01f * sampleRate));
        release = std::exp(-1.0f / (0.5f * sampleRate));
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f) {
            return input;
        }

        float swellTime = ctx.getParameter("swell_time"); 
        float wallDensity = ctx.getParameter("wall_density"); 
        float breathingGate = ctx.getParameter("breathing_gate");
        
        float fb = 0.5f + 0.45f * (std::min(swellTime, 5000.0f) / 5000.0f);
        for(int i=0; i<4; i++) comb[i].feedback = fb;

        float combOut = (comb[0].process(input) + comb[1].process(input) + 
                         comb[2].process(input) + comb[3].process(input)) * 0.25f;
                         
        float wet = allpass[1].process(allpass[0].process(combOut));
        
        // Clipping Stage (Wall Density)
        float drive = 1.0f + (wallDensity / 100.0f) * 50.0f; 
        float distorted = std::tanh(wet * drive);
        
        // Sidechain Ducking Envelope follower (Breathing Gate)
        float absIn = std::abs(input);
        if (absIn > envValue) {
            envValue = attack * envValue + (1.0f - attack) * absIn;
        } else {
            envValue = release * envValue + (1.0f - release) * absIn;
        }
        
        float duckDepth = breathingGate / 100.0f;
        float duckGain = std::max(0.0f, 1.0f - (envValue * duckDepth * 5.0f));
        
        float dryMixFactor = 1.0f - ((wallDensity / 100.0f) * 0.8f);
        return (input * dryMixFactor) + (distorted * duckGain);
    }

    void reset() override {
        for(int i=0; i<4; i++) {
            std::fill(comb[i].buffer.begin(), comb[i].buffer.end(), 0.0f);
        }
        for(int i=0; i<2; i++) {
            std::fill(allpass[i].buffer.begin(), allpass[i].buffer.end(), 0.0f);
        }
        envValue = 0.0f;
    }
};

AGENTVST_REGISTER_DSP(IntermodulationWallGeneratorProcessor)
