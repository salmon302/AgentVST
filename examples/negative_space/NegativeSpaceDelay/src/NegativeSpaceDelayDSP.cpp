// Purpose: NegativeSpaceDelay DSP processing.
// Author: Seth Nenninger (GPT-5.2-Codex Agent)
// Timestamp: 2026-05-05T04:46:06Z
// Changelog: Smooth delay/feedback and add fractional delay reads to reduce glitches.

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
    float smoothedDelay[2] = {0.f, 0.f};
    float smoothedFeedback[2] = {0.f, 0.f};
    
    float attackFast = 0.f;
    float releaseMed = 0.f;
    float delaySmoothCoeff = 0.f;
    float feedbackSmoothCoeff = 0.f;

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
        delaySmoothCoeff = std::exp(-1.0f / (0.02f * float(sampleRate)));
        feedbackSmoothCoeff = std::exp(-1.0f / (0.05f * float(sampleRate)));
        
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f || channel >= 2)
            return input;

        float evasion = ctx.getParameter("spectral_evasion") / 100.0f; 
        float echoTimeMs = ctx.getParameter("echo_time"); 
        float feedback = ctx.getParameter("feedback") / 100.0f;

        float delaySamplesTarget = static_cast<float>(mSampleRate * (echoTimeMs / 1000.0f));
        float maxDelay = static_cast<float>(delayLines[channel].size() - 2);
        delaySamplesTarget = std::clamp(delaySamplesTarget, 1.0f, maxDelay);
        float& delaySmooth = smoothedDelay[channel];
        if (delaySmooth <= 0.0f) delaySmooth = delaySamplesTarget;
        delaySmooth = delaySmoothCoeff * delaySmooth + (1.0f - delaySmoothCoeff) * delaySamplesTarget;

        float feedbackTarget = std::clamp(feedback, 0.0f, 0.95f);
        float& fbSmooth = smoothedFeedback[channel];
        if (fbSmooth <= 0.0f) fbSmooth = feedbackTarget;
        fbSmooth = feedbackSmoothCoeff * fbSmooth + (1.0f - feedbackSmoothCoeff) * feedbackTarget;
        
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

        float rp = static_cast<float>(writePos[channel]) - delaySmooth;
        while (rp < 0.0f) rp += delayLines[channel].size();
        while (rp >= delayLines[channel].size()) rp -= delayLines[channel].size();

        int i1 = static_cast<int>(rp);
        int i2 = (i1 + 1) % delayLines[channel].size();
        float frac = rp - static_cast<float>(i1);
        float delayed = delayLines[channel][i1] * (1.0f - frac) + delayLines[channel][i2] * frac;
        
        float lowWet = bpLow_wet.process(channel, delayed);
        float midWet = bpMid_wet.process(channel, delayed);
        float highWet = bpHigh_wet.process(channel, delayed);
        
        float remainder = delayed - (lowWet + midWet + highWet);
        
        float duckedDelay = (lowWet * dL) + (midWet * dM) + (highWet * dH) + remainder;
        
        delayLines[channel][writePos[channel]] = input + duckedDelay * fbSmooth;
        writePos[channel] = (writePos[channel] + 1) % delayLines[channel].size();

        float output = input + duckedDelay * 0.5f;
        return std::tanh(output);
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
            smoothedDelay[i] = 0.0f;
            smoothedFeedback[i] = 0.0f;
            writePos[i] = 0;
        }
    }
};

AGENTVST_REGISTER_DSP(NegativeSpaceDelayProcessor)
