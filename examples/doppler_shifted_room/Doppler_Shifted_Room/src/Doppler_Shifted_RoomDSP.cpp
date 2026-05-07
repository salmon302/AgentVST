/*
 * Purpose: Doppler-Shifted Room DSP core.
 * Author: Seth Nenninger (GitHub Copilot GPT-5.2-Codex).
 * Date: 2026-05-06T00:00:00Z.
 * Change: Align parameters, fix stereo LFO spread, and block caching.
 */
#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

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
        
        // Handle negative values safely
        float size = static_cast<float>(buffer.size());
        float readPos = std::fmod(static_cast<float>(writeIndex) - delaySamples, size);
        if (readPos < 0.0f) readPos += size;
        
        int i0 = static_cast<int>(std::floor(readPos));
        int i1 = i0 + 1;
        if (i1 >= static_cast<int>(buffer.size())) i1 = 0;
        float frac = readPos - i0;
        frac = std::clamp(frac, 0.0f, 1.0f);
        
        return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
    }
    
private:
    std::vector<float> buffer;
    int writeIndex = 0;
};

class ModCombFilter {
public:
    void init(float baseDelayMs, float sampleRate, int maxModMs = 30) {
        fs = sampleRate;
        baseDelaySamples = baseDelayMs * sampleRate / 1000.0f;
        // Headroom for modulation
        delay.resize(static_cast<int>(baseDelaySamples + maxModMs * sampleRate / 1000.0f + 10.0f));
    }
    
    float process(float input, float feedback, float lfoVal, float modDepthSamples, float damping) {
        // Asymmetric expansion: lfoVal ranges -1 to 1
        // Positive lfoVal = expanding (longer delay), negative = contracting
        float currentDelay = baseDelaySamples + lfoVal * modDepthSamples;
        currentDelay = std::max(1.0f, currentDelay);
        float delayed = delay.read(currentDelay);
        
        // Damping via lowpass on feedback path
        lowpassOut = delayed * damping + lowpassOut * (1.0f - damping);
        
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
    void init(float delayMs, float sampleRate, float fb = 0.5f) {
        int size = static_cast<int>(delayMs * sampleRate / 1000.0f);
        delay.resize(std::max(1, size + 10));
        delaySamples = delayMs * sampleRate / 1000.0f;
        feedback = fb;
    }
    
    float process(float input) {
        float delayed = delay.read(delaySamples);
        float out = -input + delayed;
        delay.push(input + delayed * feedback);
        return out;
    }

private:
    DelayLine delay;
    float delaySamples = 0.0f;
    float feedback = 0.5f;
};

class Doppler_Shifted_RoomProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        fs = sampleRate;
        
        // Schroeder Reverb Setup with more comb filters for richness
        float cBase[6] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f};
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < 6; ++i) {
                combs[ch][i].init(cBase[i] + ch * 2.3f, fs, 40); // 40ms max mod
            }
            apf1[ch].init(5.0f + ch * 0.3f, fs, 0.5f);
            apf2[ch].init(1.7f + ch * 0.15f, fs, 0.5f);
        }
        
        // Pre-delay lines
        preDelay_[0].resize(static_cast<int>(100.0f * fs / 1000.0f) + 10);
        preDelay_[1].resize(static_cast<int>(100.0f * fs / 1000.0f) + 10);
        preDelayWrite_[0] = 0;
        preDelayWrite_[1] = 0;
        
        lfoPhase[0] = 0.0f;
        lfoPhase[1] = 0.0f;
        nextBlockStartSample_ = 0;
        
        // Smoothing filters for parameter changes
        feedbackSmoothed_ = 0.7f;
        dampingSmoothed_ = 0.5f;
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        // Cache parameters once per block to avoid per-sample getParameter calls
        if (channel == 0 && ctx.currentSample >= nextBlockStartSample_) {
            nextBlockStartSample_ = ctx.currentSample + static_cast<std::int64_t>(ctx.numSamplesInBlock);
            cachedExpansionCycle_ = ctx.getParameter("expansion_cycle"); // 0.001 to 2.0
            cachedRoomVelocity_ = ctx.getParameter("room_velocity"); // 0.0 to 100.0
            cachedExpandRatio_ = ctx.getParameter("expand_ratio"); // 0.1 to 10.0
            cachedLfoWaveform_ = static_cast<int>(ctx.getParameter("lfo_waveform")); // 0=sine, 1=tri, 2=half, 3=asym
            cachedStereoSpread_ = ctx.getParameter("stereo_spread"); // 0 to 180 deg
            cachedPreDelayMs_ = ctx.getParameter("pre_delay"); // 0 to 100 ms
            cachedAcousticMass_ = ctx.getParameter("acoustic_mass"); // 0.1 to 10.0
            cachedDamping_ = ctx.getParameter("damping"); // 0 to 100%
            cachedMix_ = ctx.getParameter("mix") / 100.0f;

            float targetFeedback = std::exp(-3.0f * (40.0f / 1000.0f) / cachedAcousticMass_);
            float targetDamping = cachedDamping_ / 100.0f * 0.8f;
            const float smoothingTimeSec = 0.02f;
            const float blockSmooth = 1.0f - std::exp(-static_cast<float>(ctx.numSamplesInBlock) /
                                                     (smoothingTimeSec * static_cast<float>(fs)));
            feedbackSmoothed_ += (targetFeedback - feedbackSmoothed_) * blockSmooth;
            dampingSmoothed_ += (targetDamping - dampingSmoothed_) * blockSmooth;

            // phase offset for stereo
            cachedPhaseOffset_ = (cachedStereoSpread_ / 180.0f) * M_PI;

            // precompute phase increment and mod depth in samples
            cachedPhaseInc_ = 2.0f * M_PI * cachedExpansionCycle_ / static_cast<float>(fs);
            cachedModDepthSamples_ = (cachedRoomVelocity_ / 100.0f) * 40.0f * (static_cast<float>(fs) / 1000.0f);
            cachedPreDelaySamples_ = cachedPreDelayMs_ * static_cast<float>(fs) / 1000.0f;
        }

        // Update LFO per sample (only on channel 0 to keep in sync)
        if (channel == 0) {
            lfoPhase[0] += cachedPhaseInc_;
            lfoPhase[1] = lfoPhase[0] + cachedPhaseOffset_;

            // Wrap both phases
            while (lfoPhase[0] >= 2.0f * M_PI) lfoPhase[0] -= 2.0f * M_PI;
            while (lfoPhase[1] >= 2.0f * M_PI) lfoPhase[1] -= 2.0f * M_PI;
        }

        // Generate LFO value based on waveform (use cached waveform and ratio)
        float lfoVal = generateLFO(lfoPhase[channel], cachedLfoWaveform_, cachedExpandRatio_);

        // Use cached mod depth in samples
        float modDepth = cachedModDepthSamples_;

        // Pre-delay (use cached value)
        int preDelayIdx = (preDelayWrite_[channel] - static_cast<int>(cachedPreDelaySamples_) + static_cast<int>(preDelay_[channel].size())) % static_cast<int>(preDelay_[channel].size());
        float preDelayed = preDelay_[channel][preDelayIdx];
        preDelay_[channel][preDelayWrite_[channel]] = input;
        preDelayWrite_[channel] = (preDelayWrite_[channel] + 1) % static_cast<int>(preDelay_[channel].size());

        float reverb = 0.0f;
        for (int i = 0; i < 6; ++i) {
            reverb += combs[channel][i].process(preDelayed, feedbackSmoothed_, lfoVal, modDepth, dampingSmoothed_);
        }
        reverb *= 0.166f; // 1/6 normalization
        
        reverb = apf1[channel].process(reverb);
        reverb = apf2[channel].process(reverb);
        
        return input * (1.0f - cachedMix_) + reverb * cachedMix_;
    }

private:
    float generateLFO(float phase, int waveform, float expandRatio) {
        switch (waveform) {
            case 0: // Sine
                return std::sin(phase);
            
            case 1: // Triangle
                return 2.0f * std::abs(2.0f * (phase / (2.0f * M_PI)) - 1.0f) - 1.0f;
            
            case 2: // Half-wave (only positive = only expanding)
                return std::max(0.0f, std::sin(phase));
            
            case 3: // Asymmetric (expand slower than contract, or vice versa)
                {
                    float sine = std::sin(phase);
                    // Positive: compressed rise, expanded fall
                    // Negative: expanded rise, compressed fall
                    if (sine >= 0) {
                        return std::pow(sine, expandRatio);
                    } else {
                        return -std::pow(-sine, 2.0f / expandRatio);
                    }
                }
            
            default:
                return std::sin(phase);
        }
    }

    void reset() override {
        prepare(fs, 0);
    }

private:
    float fs = 44100.0f;
    ModCombFilter combs[2][6];
    AllpassFilter apf1[2];
    AllpassFilter apf2[2];
    std::vector<float> preDelay_[2];
    int preDelayWrite_[2] = {0, 0};
    float lfoPhase[2] = {0.0f, 0.0f};
    float feedbackSmoothed_ = 0.7f;
    float dampingSmoothed_ = 0.5f;
    // Block-level cache
    std::int64_t nextBlockStartSample_ = 0;
    float cachedExpansionCycle_ = 0.0f;
    float cachedRoomVelocity_ = 0.0f;
    float cachedExpandRatio_ = 1.0f;
    int cachedLfoWaveform_ = 0;
    float cachedStereoSpread_ = 0.0f;
    float cachedPreDelayMs_ = 0.0f;
    float cachedAcousticMass_ = 1.0f;
    float cachedDamping_ = 0.0f;
    float cachedMix_ = 0.0f;
    float cachedPhaseInc_ = 0.0f;
    float cachedModDepthSamples_ = 0.0f;
    float cachedPreDelaySamples_ = 0.0f;
    float cachedPhaseOffset_ = 0.0f;
};

AGENTVST_REGISTER_DSP(Doppler_Shifted_RoomProcessor)
