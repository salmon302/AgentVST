/**
 * Sensory Dissonance Smart Fuzz DSP
 *
 * Implements a target roughness (e.g. 30Hz) beating multiplier triggered
 * by amplitude complexity (approximating polyphony/dissonance threshold)
 * using ring-modulation/wave-folding sidebands.
 */
#include <AgentDSP.h>
#include <cmath>
#include <algorithm>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Sensory_Dissonance_Smart_FuzzProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= kMaxChannels) return input;

        // 1. Read parameters
        float roughnessHz = ctx.getParameter("roughness_hz");
        float threshold = ctx.getParameter("smart_threshold");
        float dissonance = ctx.getParameter("dissonance_injection");
        float fuzzDrive = ctx.getParameter("fuzz_drive"); // dB

        // Convert drive dB to linear
        float driveLin = std::pow(10.0f, fuzzDrive / 20.0f);

        // Update shared LFOs if on first channel
        if (channel == 0 && channel0Processed_ != ctx.currentSample) {
            double phaseInc = static_cast<double>(roughnessHz) / sampleRate_;
            roughnessPhase_ += phaseInc;
            if (roughnessPhase_ >= 1.0) roughnessPhase_ -= 1.0;
            channel0Processed_ = ctx.currentSample;
        }

        // 2. Amplitude tracking for "Smart" threshold
        // We approximate chord complexity/dissonance by detecting rapid amplitude fluctuations
        // using a short-term vs long-term envelope tracker.
        
        // Simple envelopes
        float absIn = std::abs(input);
        envFast_[channel] += 0.05f * (absIn - envFast_[channel]);
        envSlow_[channel] += 0.001f * (absIn - envSlow_[channel]);

        // Difference between fast and slow indicates amplitude beating/complexity
        float complexity = std::abs(envFast_[channel] - envSlow_[channel]);
        
        // Trigger if complexity > threshold (mapped appropriately)
        float scaledThreshold = threshold * 0.5f; // practical range
        float trigger = 0.0f;
        if (complexity > scaledThreshold && scaledThreshold < 0.99f) {
            trigger = std::clamp((complexity - scaledThreshold) * 10.0f, 0.0f, 1.0f);
        }
        
        // Target sideband generation: ring-modulate the input with the roughness oscillator
        // to induce violent physical beating exactly at the specified frequency.
        float lfo = static_cast<float>(std::sin(2.0 * M_PI * roughnessPhase_));
        
        // 3. Distortion / Fuzz (wavefolding + hard clipping)
        float driven = input * driveLin;
        
        // Wavefolder
        float folded = std::sin(driven * (float)M_PI * 0.5f);
        
        // Ring modulation sidebands
        float ruined = folded * lfo;

        // 4. Blend
        // We apply the dissonance injection *only* based on the trigger amount
        float targetWet = folded + (ruined * dissonance * trigger);
        
        // Soft clip output
        targetWet = std::tanh(targetWet);
        
        // Makeup gain to counteract the drive
        targetWet /= std::max(1.0f, std::log10(driveLin + 1.0f));

        return targetWet;
    }

    void reset() override {
        roughnessPhase_ = 0.0;
        channel0Processed_ = -1;
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            envFast_[ch] = 0.0f;
            envSlow_[ch] = 0.0f;
        }
    }

private:
    static constexpr int kMaxChannels = 2;
    double sampleRate_ = 44100.0;
    
    double roughnessPhase_ = 0.0;
    std::int64_t channel0Processed_ = -1;
    
    float envFast_[kMaxChannels] = {0.0f, 0.0f};
    float envSlow_[kMaxChannels] = {0.0f, 0.0f};
};

AGENTVST_REGISTER_DSP(Sensory_Dissonance_Smart_FuzzProcessor)