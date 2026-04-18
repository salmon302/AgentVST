#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Reusable simple delay line
class FC_DelayLine {
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

// Comb Filter
class FC_CombFilter {
public:
    void init(float delayMs, float sampleRate, float feedback) {
        delaySamples = delayMs * sampleRate / 1000.0f;
        delay.resize(static_cast<int>(delaySamples + 10.0f));
        fb = feedback;
        lowpass = 0.0f;
    }
    
    float process(float input) {
        float delayed = delay.read(delaySamples);
        lowpass = delayed * 0.7f + lowpass * 0.3f;
        delay.push(input + lowpass * fb);
        return delayed;
    }
private:
    FC_DelayLine delay;
    float delaySamples = 0.0f;
    float fb = 0.8f;
    float lowpass = 0.0f;
};

// Allpass Filter
class FC_Allpass {
public:
    void init(float delayMs, float sampleRate) {
        delaySamples = delayMs * sampleRate / 1000.0f;
        delay.resize(static_cast<int>(delaySamples + 10.0f));
    }
    
    float process(float input) {
        float delayed = delay.read(delaySamples);
        float out = -input + delayed;
        delay.push(input + delayed * 0.5f);
        return out;
    }
private:
    FC_DelayLine delay;
    float delaySamples = 0.0f;
};

class FalseMemoryConvolutionProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        fs = sampleRate;
        
        // Space A: Massive Cathedral
        float cBaseA[4] = {43.7f, 51.1f, 57.3f, 61.9f};
        for(int ch=0; ch<2; ++ch) {
            for(int i=0; i<4; ++i) combsA[ch][i].init(cBaseA[i] + ch*2.5f, fs, 0.95f); // long decay
            apfA1[ch].init(12.0f + ch*0.5f, fs);
            apfA2[ch].init(8.3f + ch*0.3f, fs);
        }
        
        // Space B: Claustrophobic Tin Pipe
        float cBaseB[4] = {2.1f, 3.3f, 4.7f, 5.0f}; // extremely short resonant metallic intervals
        for(int ch=0; ch<2; ++ch) {
            for(int i=0; i<4; ++i) combsB[ch][i].init(cBaseB[i] + ch*0.1f, fs, 0.8f); // ringing decay
            apfB1[ch].init(1.1f + ch*0.05f, fs);
            apfB2[ch].init(0.7f + ch*0.02f, fs);
        }
        
        lfoPhase = 0.0f;
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;
        
        float morphSpeed = ctx.getParameter("morph_speed_hz");
        float morphMode = ctx.getParameter("morph_mode"); // 0=Linear, 1=Granular
        float mix = ctx.getParameter("mix");
        
        if (channel == 0) {
            lfoPhase += (2.0f * M_PI * morphSpeed) / fs;
            if (lfoPhase >= 2.0f * M_PI) lfoPhase -= 2.0f * M_PI;
        }
        
        // Compute crossfade value (0.0 to 1.0)
        float morphVal = 0.0f;
        if (morphMode < 0.5f) {
            // Linear Sine Crossfade
            morphVal = (std::sin(lfoPhase) + 1.0f) * 0.5f;
        } else {
            // Granular Shattered Crossfade - fast hard-switches 
            // We use a high-frequency derived square wave for rapid, jarring chopping
            float granularPhase = std::fmod(lfoPhase * 16.0f, 2.0f * M_PI);
            morphVal = (granularPhase < M_PI) ? 1.0f : 0.0f;
        }

        // Process Space A
        float revA = 0.0f;
        for (int i=0; i<4; ++i) revA += combsA[channel][i].process(input);
        revA = apfA1[channel].process(revA * 0.25f);
        revA = apfA2[channel].process(revA);
        
        // Process Space B
        float revB = 0.0f;
        for (int i=0; i<4; ++i) revB += combsB[channel][i].process(input);
        revB = apfB1[channel].process(revB * 0.25f);
        revB = apfB2[channel].process(revB);
        
        // Crossfade
        float wet = revA * (1.0f - morphVal) + revB * morphVal;
        
        return input * (1.0f - mix) + wet * mix;
    }

    void reset() override {
        lfoPhase = 0.0f;
        prepare(fs, 0); // lazy reset by re-allocating
    }

private:
        float fs = 44100.0f;
        FC_CombFilter combsA[2][4];
        FC_Allpass apfA1[2], apfA2[2];
        
        FC_CombFilter combsB[2][4];
        FC_Allpass apfB1[2], apfB2[2];
        
        float lfoPhase = 0.0f;
};

AGENTVST_REGISTER_DSP(FalseMemoryConvolutionProcessor)
