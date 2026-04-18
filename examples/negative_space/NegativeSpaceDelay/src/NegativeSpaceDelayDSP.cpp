#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <algorithm>

constexpr double PI = 3.14159265358979323846;

struct Biquad {
    float a0, a1, a2, b1, b2;
    float z1[2], z2[2];
    Biquad() { reset(); }
    void reset() { z1[0]=z1[1]=z2[0]=z2[1]=0.0f; }
    void setBandpass(double sampleRate, double freq, double q) {
        double w0 = 2.0 * PI * freq / sampleRate;
        double alpha = std::sin(w0) / (2.0 * q);
        double a0_p = 1.0 + alpha;
        a0 = (float)(alpha / a0_p);
        a1 = 0.0f;
        a2 = (float)(-alpha / a0_p);
        b1 = (float)(-2.0 * std::cos(w0) / a0_p);
        b2 = (float)((1.0 - alpha) / a0_p);
    }
    float process(int ch, float in) {
        float out = in * a0 + z1[ch];
        z1[ch] = in * a1 + out * (-b1) + z2[ch];
        z2[ch] = in * a2 + out * (-b2);
        return out;
    }
};

class NegativeSpaceDelayProcessor : public AgentVST::IAgentDSP {
    double mSampleRate = 44100.0;
    
    std::vector<std::vector<float>> delayLines;
    std::vector<int> writePos;
    
    // For dry signal analysis
    Biquad bpLow_dry;
    Biquad bpMid_dry;
    Biquad bpHigh_dry;

    // For wet signal filtering
    Biquad bpLow_wet;
    Biquad bpMid_wet;
    Biquad bpHigh_wet;
    
    float envLow[2] = {0.f, 0.f};
    float envMid[2] = {0.f, 0.f};
    float envHigh[2] = {0.f, 0.f};
    
    float attackFast = 0.f;
    float releaseMed = 0.f;

public:
    NegativeSpaceDelayProcessor() {
        delayLines.resize(2, std::vector<float>(44100 * 3, 0.0f)); 
        writePos.resize(2, 0);
    }

    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        mSampleRate = sampleRate;
        bpLow_dry.setBandpass(mSampleRate, 150.0, 0.707);
        bpMid_dry.setBandpass(mSampleRate, 1000.0, 0.707);
        bpHigh_dry.setBandpass(mSampleRate, 5000.0, 0.707);

        bpLow_wet.setBandpass(mSampleRate, 150.0, 0.707);
        bpMid_wet.setBandpass(mSampleRate, 1000.0, 0.707);
        bpHigh_wet.setBandpass(mSampleRate, 5000.0, 0.707);
        
        attackFast = std::exp(-1.0f / (0.01f * float(sampleRate))); 
        releaseMed = std::exp(-1.0f / (0.3f * float(sampleRate))); 
        
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f || channel >= 2)
            return input;

        float evasion = ctx.getParameter("spectral_evasion") / 100.0f; 
        float echoTimeMs = ctx.getParameter("echo_time"); 
        float feedback = ctx.getParameter("feedback") / 100.0f;
        
        int delaySamples = (int)(mSampleRate * (echoTimeMs / 1000.0f));
        delaySamples = std::max(1, std::min(delaySamples, (int)delayLines[channel].size() - 1));
        
        float lowDry = bpLow_dry.process(channel, input);
        float midDry = bpMid_dry.process(channel, input);
        float highDry = bpHigh_dry.process(channel, input);
        
        auto envStep = [&](float sig, float& env) {
            float absSig = std::abs(sig);
            if (absSig > env) {
                env = attackFast * env + (1.0f - attackFast) * absSig;
            } else {
                env = releaseMed * env + (1.0f - releaseMed) * absSig;
            }
        };
        
        envStep(lowDry, envLow[channel]);
        envStep(midDry, envMid[channel]);
        envStep(highDry, envHigh[channel]);
        
        float dL = std::max(0.0f, 1.0f - (envLow[channel] * 15.0f * evasion));
        float dM = std::max(0.0f, 1.0f - (envMid[channel] * 15.0f * evasion));
        float dH = std::max(0.0f, 1.0f - (envHigh[channel] * 15.0f * evasion));

        int rp = writePos[channel] - delaySamples;
        if (rp < 0) rp += delayLines[channel].size();
        
        float delayed = delayLines[channel][rp];
        
        float lowWet = bpLow_wet.process(channel, delayed);
        float midWet = bpMid_wet.process(channel, delayed);
        float highWet = bpHigh_wet.process(channel, delayed);
        
        float remainder = delayed - (lowWet + midWet + highWet);
        
        float duckedDelay = (lowWet * dL) + (midWet * dM) + (highWet * dH) + remainder;
        
        delayLines[channel][writePos[channel]] = input + duckedDelay * feedback;
        writePos[channel] = (writePos[channel] + 1) % delayLines[channel].size();
        
        return input + duckedDelay * 0.5f;
    }

    void reset() override {
        for(auto& b : delayLines) std::fill(b.begin(), b.end(), 0.0f);
        bpLow_dry.reset();
        bpMid_dry.reset();
        bpHigh_dry.reset();
        bpLow_wet.reset();
        bpMid_wet.reset();
        bpHigh_wet.reset();
        for(int i=0; i<2; ++i) {
            envLow[i] = envMid[i] = envHigh[i] = 0.0f;
            writePos[i] = 0;
        }
    }
};

AGENTVST_REGISTER_DSP(NegativeSpaceDelayProcessor)
