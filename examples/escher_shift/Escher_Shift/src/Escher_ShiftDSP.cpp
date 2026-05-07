/**
 * Escher Shift DSP - Simplified Pitch Shifter
 * Purpose: Simple dual-delay pitch shifter DSP.
 * Author: Seth Nenninger (GPT-5.2-Codex Agent)
 * Timestamp: 2026-05-06T23:47:00Z
 * Changelog: Smooth parameters further and use Hann-windowed tap crossfades.
 * Parameters: Shift Rate, Tritone Blend, Feedback, Octave Spread, Mix
 *
 * Keep processSample real-time safe:
 * - no allocations
 * - no locks
 * - no I/O
 */
#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Escher_ShiftProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        bufferSize_ = static_cast<int>(sampleRate * 2.0);
        
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            delayBuffer_[ch].assign(bufferSize_, 0.0f);
            writePos_[ch] = 0;
            lastIn_[ch] = 0.0f;
            lastOut_[ch] = 0.0f;
            feedbackOut_[ch] = 0.0f;
        }
        
        // Initialize parameter cache
        cachedShiftRate_ = 0.5f;
        cachedTritoneBlend_ = 0.0f;
        cachedFeedback_ = 0.0f;
        cachedOctaveSpread_ = 0.5f;
        cachedMix_ = 1.0f;
        lastBlockStart_ = -1;
        phase_ = 0.0;
        channel0Processed_ = -1;

        // Smooth phase changes to reduce clicks from quantized steps.
        const float phaseSmoothTimeSec = 0.01f;
        phaseSmoothCoeff_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate_) * phaseSmoothTimeSec));
        smoothedPhase_ = 0.0f;
        currentPhase_ = 0.0f;

        const float paramSmoothTimeSec = 0.02f;
        paramSmoothCoeff_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate_) * paramSmoothTimeSec));
        smoothedShiftRate_ = cachedShiftRate_;
        smoothedTritoneBlend_ = cachedTritoneBlend_;
        smoothedFeedback_ = cachedFeedback_;
        smoothedOctaveSpread_ = cachedOctaveSpread_;
        smoothedMix_ = cachedMix_;
        
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        // Cache parameters at block start
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedShiftRate_ = ctx.getParameter("shift_rate");
            cachedTritoneBlend_ = ctx.getParameter("tritone_blend");
            cachedFeedback_ = ctx.getParameter("feedback");
            cachedOctaveSpread_ = ctx.getParameter("octave_spread");
            cachedMix_ = ctx.getParameter("mix");
        }

        if (channel >= kMaxChannels) return input;

        // Smooth parameter changes to avoid clicks on automation or jumps.
        float targetShiftRate = cachedShiftRate_;
        float targetTritoneBlend = cachedTritoneBlend_;
        float targetFeedback = cachedFeedback_;
        float targetOctaveSpread = cachedOctaveSpread_;
        float targetMix = cachedMix_;

        smoothedShiftRate_ += (targetShiftRate - smoothedShiftRate_) * paramSmoothCoeff_;
        smoothedTritoneBlend_ += (targetTritoneBlend - smoothedTritoneBlend_) * paramSmoothCoeff_;
        smoothedFeedback_ += (targetFeedback - smoothedFeedback_) * paramSmoothCoeff_;
        smoothedOctaveSpread_ += (targetOctaveSpread - smoothedOctaveSpread_) * paramSmoothCoeff_;
        smoothedMix_ += (targetMix - smoothedMix_) * paramSmoothCoeff_;

        float shiftRate = smoothedShiftRate_;
        float tritoneBlend = smoothedTritoneBlend_;
        float feedback = smoothedFeedback_;
        float octaveSpread = smoothedOctaveSpread_;
        float mix = smoothedMix_;

        // Apply feedback to input
        float inputWithFeedback = input + feedbackOut_[channel] * feedback;

        // Update phase and smooth quantized jumps.
        if (channel0Processed_ != ctx.currentSample) {
            double phaseInc = shiftRate / sampleRate_;
            phase_ += phaseInc;
            if (phase_ >= 1.0) phase_ -= 1.0;
            else if (phase_ < 0.0) phase_ += 1.0;

            float targetPhase = static_cast<float>(phase_);
            float quantizedPhase = std::floor(targetPhase * 6.0f) / 6.0f;
            float blendAmount = std::clamp(tritoneBlend, 0.0f, 1.0f);
            targetPhase = targetPhase + (quantizedPhase - targetPhase) * blendAmount;

            float delta = targetPhase - smoothedPhase_;
            if (delta > 0.5f) delta -= 1.0f;
            else if (delta < -0.5f) delta += 1.0f;

            smoothedPhase_ += delta * phaseSmoothCoeff_;
            if (smoothedPhase_ >= 1.0f) smoothedPhase_ -= 1.0f;
            else if (smoothedPhase_ < 0.0f) smoothedPhase_ += 1.0f;

            currentPhase_ = smoothedPhase_;
            channel0Processed_ = ctx.currentSample;
        }

        float currentPhase = currentPhase_;

        // Shepard envelope weight with octave spread
        float weightUp = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * currentPhase));
        float weightDown = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * (currentPhase + 0.5f)));

        // Octave spread skew: < 0.5 favors fall, > 0.5 favors rise
        float skewFactor = (octaveSpread - 0.5f) * 2.0f; // [-1, 1]
        float skewUp = std::clamp(1.0f + skewFactor, 0.0f, 1.0f);
        float skewDown = std::clamp(1.0f - skewFactor, 0.0f, 1.0f);

        weightUp *= skewUp;
        weightDown *= skewDown;
        
        // Normalize weights
        float totalWeight = weightUp + weightDown;
        if (totalWeight > 0.0f) {
            weightUp /= totalWeight;
            weightDown /= totalWeight;
        }

        // Write to delay buffer
        delayBuffer_[channel][writePos_[channel]] = inputWithFeedback;
        writePos_[channel] = (writePos_[channel] + 1) % bufferSize_;

        // Granular read for shifting
        float maxDelaySamples = sampleRate_ * 0.1f; // 100ms window
        
        auto getSample = [&](float delaySamples) {
            float readPos = static_cast<float>(writePos_[channel]) - 1.0f - delaySamples;
            readPos = std::fmod(readPos, (float)bufferSize_);
            if (readPos < 0) readPos += (float)bufferSize_;
            readPos = std::clamp(readPos, 0.0f, (float)bufferSize_ - 1.0f);
            
            int i0 = (int)std::floor(readPos);
            int i1 = i0 + 1;
            if (i1 >= bufferSize_) i1 = 0;
            
            float f = readPos - (float)i0;
            f = std::clamp(f, 0.0f, 1.0f);
            
            return delayBuffer_[channel][i0] * (1.0f - f) + delayBuffer_[channel][i1] * f;
        };

        // Rising path (delay decreases)
        float ramp1Up = std::max(1.0f, (1.0f - currentPhase) * maxDelaySamples);
        float ramp2Up = std::fmod(ramp1Up + (maxDelaySamples * 0.5f), maxDelaySamples);
        if (ramp2Up < 0.0f) ramp2Up += maxDelaySamples;
        ramp2Up = std::max(1.0f, ramp2Up);
        float xfadeUpPhase = std::clamp(ramp1Up / maxDelaySamples, 0.0f, 1.0f);
        float hannUp = 0.5f - 0.5f * std::cos(2.0f * (float)M_PI * xfadeUpPhase);
        float upOut = (getSample(ramp1Up) * hannUp) + (getSample(ramp2Up) * (1.0f - hannUp));
        
        // Falling path (delay increases)
        float ramp1Down = std::max(1.0f, currentPhase * maxDelaySamples);
        float ramp2Down = std::fmod(ramp1Down + (maxDelaySamples * 0.5f), maxDelaySamples);
        if (ramp2Down < 0.0f) ramp2Down += maxDelaySamples;
        ramp2Down = std::max(1.0f, ramp2Down);
        float xfadeDownPhase = std::clamp(ramp1Down / maxDelaySamples, 0.0f, 1.0f);
        float hannDown = 0.5f - 0.5f * std::cos(2.0f * (float)M_PI * xfadeDownPhase);
        float downOut = (getSample(ramp1Down) * hannDown) + (getSample(ramp2Down) * (1.0f - hannDown));

        float shifted = (upOut * weightUp) + (downOut * weightDown);

        // Store feedback for next sample
        feedbackOut_[channel] = shifted;

        // Mix dry and wet
        float output = input * (1.0f - mix) + shifted * mix;

        return output;
    }

    static constexpr int kMaxChannels = 2;

private:
    double sampleRate_ = 44100.0;
    int bufferSize_ = 88200;
    std::vector<float> delayBuffer_[kMaxChannels];
    int writePos_[kMaxChannels] = {0, 0};
    float lastIn_[kMaxChannels] = {0.0f, 0.0f};
    float lastOut_[kMaxChannels] = {0.0f, 0.0f};
    float feedbackOut_[kMaxChannels] = {0.0f, 0.0f};
    
    // Parameter cache
    float cachedShiftRate_ = 0.5f;
    float cachedTritoneBlend_ = 0.0f;
    float cachedFeedback_ = 0.0f;
    float cachedOctaveSpread_ = 0.5f;
    float cachedMix_ = 1.0f;
    int lastBlockStart_ = -1;
    
    double phase_ = 0.0;
    int channel0Processed_ = -1;

    float smoothedPhase_ = 0.0f;
    float currentPhase_ = 0.0f;
    float phaseSmoothCoeff_ = 1.0f;
    float paramSmoothCoeff_ = 1.0f;

    float smoothedShiftRate_ = 0.5f;
    float smoothedTritoneBlend_ = 0.0f;
    float smoothedFeedback_ = 0.0f;
    float smoothedOctaveSpread_ = 0.5f;
    float smoothedMix_ = 1.0f;
};

// Plugin entry point
static Escher_ShiftProcessor gProcessor;

AGENTVST_REGISTER_DSP(Escher_ShiftProcessor)