#include <AgentDSP.h>
#include "modules/BiquadFilter.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class InfrasonicDreadGeneratorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        for (int ch = 0; ch < 2; ++ch) {
            lowpass_[ch].prepare(sampleRate, maxBlockSize);
            lowpass_[ch].setType(AgentVST::BiquadFilter::Type::LowPass);
            lowpass_[ch].setFrequency(80.0f);
            lowpass_[ch].setQ(0.707f);
            
            envelope_[ch] = 0.0f;
        }
        phase_ = 0.0f;
        burstEnvelope_ = 0.0f;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float dreadFreq = ctx.getParameter("dread_frequency");
        float sensitivityDb = ctx.getParameter("impact_sensitivity");
        float pressure = ctx.getParameter("pressure_amount") / 100.0f;

        float thresholdLinear = std::pow(10.0f, sensitivityDb / 20.0f);

        // Low-pass filter input to track low-end impacts
        float lpInput = lowpass_[channel].processSample(channel, input);

        // Envelope follower (fast attack, medium release)
        float attackCoef = std::exp(-1.0f / (0.005f * sampleRate_));
        float releaseCoef = std::exp(-1.0f / (0.100f * sampleRate_));
        
        float absInput = std::abs(lpInput);
        if (absInput > envelope_[channel]) {
            envelope_[channel] = attackCoef * envelope_[channel] + (1.0f - attackCoef) * absInput;
        } else {
            envelope_[channel] = releaseCoef * envelope_[channel] + (1.0f - releaseCoef) * absInput;
        }

        // Only channel 0 drives the shared generator to keep phase aligned
        if (channel == 0) {
            if (envelope_[0] > thresholdLinear) {
                // Attack the burst envelope
                burstEnvelope_ += (1.0f - burstEnvelope_) * 0.01f;
            } else {
                // Release the burst envelope
                burstEnvelope_ *= 0.999f;
            }
            
            // Generate infrasonic wave
            phase_ += dreadFreq / sampleRate_;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
            
            generatedWave_ = std::sin(2.0f * M_PI * phase_);
        }

        // Output mix
        float output = input + (generatedWave_ * burstEnvelope_ * pressure);

        return output;
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            lowpass_[ch].reset();
            envelope_[ch] = 0.0f;
        }
        phase_ = 0.0f;
        burstEnvelope_ = 0.0f;
        generatedWave_ = 0.0f;
    }

private:
    double sampleRate_ = 44100.0;
    AgentVST::BiquadFilter lowpass_[2];
    float envelope_[2] = {0.0f, 0.0f};
    
    float phase_ = 0.0f;
    float burstEnvelope_ = 0.0f;
    float generatedWave_ = 0.0f;
};

AGENTVST_REGISTER_DSP(InfrasonicDreadGeneratorProcessor)
