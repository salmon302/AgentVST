#include <AgentDSP.h>
#include <cmath>
#include <algorithm>
#include <vector>

class bimodal_weight_fletcher_munson_eqProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        this->sampleRate = sampleRate;
        rmsLevels.assign(2, 0.0f);
        lowPassStates.assign(2, 0.0f);
        highPassStates.assign(2, 0.0f);

        float dt = 1.0f / (float)sampleRate;
        
        float rcLow = 1.0f / (2.0f * 3.14159265f * 150.0f);
        alphaLow = dt / (dt + rcLow);
        
        float rcHigh = 1.0f / (2.0f * 3.14159265f * 6000.0f);
        alphaHigh = dt / (dt + rcHigh);

        alphaRMS = std::exp(-1.0f / (0.05f * (float)sampleRate)); 
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input; 
        
        float refLoudness = ctx.getParameter("reference_loudness");
        float compDepth = ctx.getParameter("compensation_depth");
        float bias = ctx.getParameter("weight_bite_bias");

        float& rms = rmsLevels[channel];
        rms = alphaRMS * rms + (1.0f - alphaRMS) * std::abs(input);
        
        float currentDb = (rms > 0.0001f) ? 20.0f * std::log10(rms) : -80.0f;
            
        float dev = refLoudness - currentDb;
        if (dev < 0.0f) dev = 0.0f;

        float totalBoostBase = dev * compDepth * 0.5f; 
        if (totalBoostBase > 24.0f) totalBoostBase = 24.0f;

        float wLow = std::clamp(1.0f - bias, 0.0f, 1.0f);
        float wHigh = std::clamp(1.0f + bias, 0.0f, 1.0f);

        float gainLow = std::pow(10.0f, (totalBoostBase * wLow) / 20.0f);
        float gainHigh = std::pow(10.0f, (totalBoostBase * wHigh) / 20.0f);

        lowPassStates[channel] += alphaLow * (input - lowPassStates[channel]);
        float lowBand = lowPassStates[channel];
                
        highPassStates[channel] += alphaHigh * (input - highPassStates[channel]);
        float highBand = input - highPassStates[channel]; 

        float midBand = input - lowBand - highBand;

        float outSample = midBand + (lowBand * gainLow) + (highBand * gainHigh);

        return std::tanh(outSample);
    }

    void reset() override {
        std::fill(rmsLevels.begin(), rmsLevels.end(), 0.0f);
        std::fill(lowPassStates.begin(), lowPassStates.end(), 0.0f);
        std::fill(highPassStates.begin(), highPassStates.end(), 0.0f);
    }
private:
    double sampleRate = 44100.0;
    std::vector<float> rmsLevels;
    std::vector<float> lowPassStates;
    std::vector<float> highPassStates;
    float alphaLow = 0.0f;
    float alphaHigh = 0.0f;
    float alphaRMS = 0.0f;
};

AGENTVST_REGISTER_DSP(bimodal_weight_fletcher_munson_eqProcessor)
#include <AgentDSP.h>
#include <cmath>
#include <algorithm>
#include <vector>

class bimodal_weight_fletcher_munson_eqProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        this->sampleRate = sampleRate;
        rmsLevels.assign(2, 0.0f);
        lowPassStates.assign(2, 0.0f);
        highPassStates.assign(2, 0.0f);

        float dt = 1.0f / sampleRate;
        
        float rcLow = 1.0f / (2.0f * 3.14159265f * 150.0f);
        alphaLow = dt / (dt + rcLow);
        
        float rcHigh = 1.0f / (2.0f * 3.14159265f * 6000.0f);
        alphaHigh = dt / (dt + rcHigh);

        alphaRMS = std::exp(-1.0f / (0.05f * sampleRate)); 
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input; 
        
        float refLoudness = ctx.getParameter("reference_loudness");
        float compDepth = ctx.getParameter("compensation_depth");
        float bias = ctx.getParameter("weight_bite_bias");

        float& rms = rmsLevels[channel];
        rms = alphaRMS * rms + (1.0f - alphaRMS) * std::abs(input);
        
        float currentDb = (rms > 0.0001f) ? 20.0f * std::log10(rms) : -80.0f;
            
        float dev = refLoudness - currentDb;
        if (dev < 0.0f) dev = 0.0f;

        float totalBoostBase = dev * compDepth * 0.5f; 
        if (totalBoostBase > 24.0f) totalBoostBase = 24.0f;

        float wLow = std::clamp(1.0f - bias, 0.0f, 1.0f);
        float wHigh = std::clamp(1.0f + bias, 0.0f, 1.0f);

        float gainLow = std::pow(10.0f, (totalBoostBase * wLow) / 20.0f);
        float gainHigh = std::pow(10.0f, (totalBoostBase * wHigh) / 20.0f);

        lowPassStates[channel] += alphaLow * (input - lowPassStates[channel]);
        float lowBand = lowPassStates[channel];
                
        highPassStates[channel] += alphaHigh * (input - highPassStates[channel]);
        float highBand = input - highPassStates[channel]; 

        float midBand = input - lowBand - highBand;

        float outSample = midBand + (lowBand * gainLow) + (highBand * gainHigh);

        return std::tanh(outSample);
    }

    void reset() override {
        std::fill(rmsLevels.begin(), rmsLevels.end(), 0.0f);
        std::fill(lowPassStates.begin(), lowPassStates.end(), 0.0f);
        std::fill(highPassStates.begin(), highPassStates.end(), 0.0f);
    }
private:
    double sampleRate = 44100.0;
    std::vector<float> rmsLevels;
    std::vector<float> lowPassStates;
    std::vector<float> highPassStates;
    float alphaLow = 0.0f;
    float alphaHigh = 0.0f;
    float alphaRMS = 0.0f;
};

AGENTVST_REGISTER_DSP(bimodal_weight_fletcher_munson_eqProcessor)
#include <AgentDSP.h>
#include <cmath>
#include <algorithm>
#include <vector>

class bimodal_weight_fletcher_munson_eqProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        this->sampleRate = sampleRate;
        rmsLevels.assign(2, 0.0f);
        lowPassStates.assign(2, 0.0f);
        highPassStates.assign(2, 0.0f);

        float dt = 1.0f / sampleRate;
        
        float rcLow = 1.0f / (2.0f * 3.14159265f * 150.0f);
        alphaLow = dt / (dt + rcLow);
        
        float rcHigh = 1.0f / (2.0f * 3.14159265f * 6000.0f);
        alphaHigh = dt / (dt + rcHigh);

        alphaRMS = std::exp(-1.0f / (0.05f * sampleRate)); 
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input; 
        
        float refLoudness = ctx.getParameter("reference_loudness");
        float compDepth = ctx.getParameter("compensation_depth");
        float bias = ctx.getParameter("weight_bite_bias");

        float& rms = rmsLevels[channel];
        rms = alphaRMS * rms + (1.0f - alphaRMS) * std::abs(input);
        
        float currentDb = (rms > 0.0001f) ? 20.0f * std::log10(rms) : -80.0f;
            
        float dev = refLoudness - currentDb;
        if (dev < 0.0f) dev = 0.0f;

        float totalBoostBase = dev * compDepth * 0.5f; 
        if (totalBoostBase > 24.0f) totalBoostBase = 24.0f;

        float wLow = std::clamp(1.0f - bias, 0.0f, 1.0f);
        float wHigh = std::clamp(1.0f + bias, 0.0f, 1.0f);

        float gainLow = std::pow(10.0f, (totalBoostBase * wLow) / 20.0f);
        float gainHigh = std::pow(10.0f, (totalBoostBase * wHigh) / 20.0f);

        lowPassStates[channel] += alphaLow * (input - lowPassStates[channel]);
        float lowBand = lowPassStates[channel];
                
        highPassStates[channel] += alphaHigh * (input - highPassStates[channel]);
        float highBand = input - highPassStates[channel]; 

        float midBand = input - lowBand - highBand;

        float outSample = midBand + (lowBand * gainLow) + (highBand * gainHigh);

        return std::tanh(outSample);
    }

    void reset() override {
        std::fill(rmsLevels.begin(), rmsLevels.end(), 0.0f);
        std::fill(lowPassStates.begin(), lowPassStates.end(), 0.0f);
        std::fill(highPassStates.begin(), highPassStates.end(), 0.0f);
    }
private:
    double sampleRate = 44100.0;
    std::vector<float> rmsLevels;
    std::vector<float> lowPassStates;
    std::vector<float> highPassStates;
    float alphaLow = 0.0f;
    float alphaHigh = 0.0f;
    float alphaRMS = 0.0f;
};

AGENTVST_REGISTER_DSP(bimodal_weight_fletcher_munson_eqProcessor)
#include "bimodal_weight_fletcher_munson_eqDSP.h"
#include <cmath>
#include <algorithm>

namespace AgentVST
{
    void bimodal_weight_fletcher_munson_eqDSP::prepare(double sampleRate, int maximumBlockSize, int numChannels)
    {
        this->sampleRate = sampleRate;
        rmsLevels.assign(numChannels, 0.0f);
        lowPassStates.assign(numChannels, 0.0f);
        highPassStates.assign(numChannels, 0.0f);
    }

    void bimodal_weight_fletcher_munson_eqDSP::process(std::vector<std::vector<float>>& buffer, DSPContext& ctx)
    {
        int numChannels = buffer.size();
        if (numChannels == 0) return;
        int numSamples = buffer[0].size();

        float refLoudness = ctx.getParameter("reference_loudness");
        float compDepth = ctx.getParameter("compensation_depth");
        float bias = ctx.getParameter("weight_bite_bias");

        float alphaRMS = std::exp(-1.0f / (0.05f * sampleRate)); // ~50ms 

        float dt = 1.0f / sampleRate;
        float rcLow = 1.0f / (2.0f * 3.14159265f * 150.0f);
        float alphaLow = dt / (dt + rcLow);

        float rcHigh = 1.0f / (2.0f * 3.14159265f * 6000.0f);
        float alphaHigh = dt / (dt + rcHigh);

        for (int c = 0; c < numChannels; ++c)
        {
            float rms = rmsLevels[c];
            float currentDb = (rms > 0.0001f) ? 20.0f * std::log10(rms) : -80.0f;
            
            float dev = refLoudness - currentDb;
            if (dev < 0.0f) dev = 0.0f;

            float totalBoostBase = dev * compDepth * 0.5f; 
            if (totalBoostBase > 24.0f) totalBoostBase = 24.0f;

            float wLow = std::clamp(1.0f - bias, 0.0f, 1.0f);
            float wHigh = std::clamp(1.0f + bias, 0.0f, 1.0f);

            float gainLow = std::pow(10.0f, (totalBoostBase * wLow) / 20.0f);
            float gainHigh = std::pow(10.0f, (totalBoostBase * wHigh) / 20.0f);

            for (int s = 0; s < numSamples; ++s)
            {
                float sample = buffer[c][s];
                
                rms = alphaRMS * rms + (1.0f - alphaRMS) * std::abs(sample);

                lowPassStates[c] += alphaLow * (sample - lowPassStates[c]);
                float lowBand = lowPassStates[c];
                
                highPassStates[c] += alphaHigh * (sample - highPassStates[c]);
                float highBand = sample - highPassStates[c]; 

                float midBand = sample - lowBand - highBand;

                float outSample = midBand + (lowBand * gainLow) + (highBand * gainHigh);

                outSample = std::tanh(outSample);

                buffer[c][s] = outSample;
            }
            rmsLevels[c] = rms;
        }
    }
}
/**
 * Generated DSP scaffold.
 *
 * Keep processSample real-time safe:
 * - no allocations
 * - no locks
 * - no I/O
 */
#include <AgentDSP.h>

class bimodal_weight_fletcher_munson_eqProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double /*sampleRate*/, int /*maxBlockSize*/) override {}

    float processSample(int /*channel*/, float input,
                        const AgentVST::DSPContext& ctx) override {
        // TODO: Replace pass-through with audible DSP logic.
        // AgentVST logs a potential no-op warning if output remains
        // effectively identical to input for many consecutive blocks.
        return input;
    }

    void reset() override {}
};

AGENTVST_REGISTER_DSP(bimodal_weight_fletcher_munson_eqProcessor)
