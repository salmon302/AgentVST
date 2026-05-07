/**
 * Escher Pitch Shifter (The Endless Tritone) DSP
 * Purpose: Shepard-tone pitch shifter DSP.
 * Author: Seth Nenninger (GPT-5.2-Codex Agent)
 * Timestamp: 2026-05-06T23:47:00Z
 * Changelog: Smooth parameters further and use Hann-windowed tap crossfades.
 *
 * PERFORMANCE: Added parameter caching at block start
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

class Escher_Pitch_ShifterProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        bufferSize_ = static_cast<int>(sampleRate * 2.0);
        
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            delayBuffer_[ch].assign(bufferSize_, 0.0f);
            writePos_[ch] = 0;
        }
        
        // PERFORMANCE: Initialize parameter cache
        cachedShiftHz_ = 0.5f;
        cachedLock_ = 0.0f;
        cachedAsymmetry_ = 0.0f;
        cachedParadoxAmount_ = 0.0f;
        cachedSubAmount_ = 0.0f;
        cachedBifurcatedEcho_ = 0.0f;
        cachedMix_ = 0.5f;
        lastBlockStart_ = -1;

        // Smooth phase changes to avoid clicks when delay taps jump.
        const float phaseSmoothTimeSec = 0.01f;
        phaseSmoothCoeff_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate_) * phaseSmoothTimeSec));
        smoothedPhase_ = 0.0f;
        currentPhase_ = 0.0f;

        const float paramSmoothTimeSec = 0.02f;
        paramSmoothCoeff_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate_) * paramSmoothTimeSec));
        smoothedShiftHz_ = cachedShiftHz_;
        smoothedLock_ = cachedLock_;
        smoothedAsymmetry_ = cachedAsymmetry_;
        smoothedParadoxAmount_ = cachedParadoxAmount_;
        smoothedSubAmount_ = cachedSubAmount_;
        smoothedBifurcatedEcho_ = cachedBifurcatedEcho_;
        smoothedMix_ = cachedMix_;
        
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        // PERFORMANCE: Cache parameters at block start
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedShiftHz_ = ctx.getParameter("shift_cycle");
            cachedLock_ = ctx.getParameter("tension_lock");
            cachedAsymmetry_ = ctx.getParameter("asymmetry");
            cachedParadoxAmount_ = ctx.getParameter("paradox_filter");
            cachedSubAmount_ = ctx.getParameter("sub_injection");
            cachedBifurcatedEcho_ = ctx.getParameter("bifurcated_echo");
            cachedMix_ = ctx.getParameter("mix");
        }

        if (channel >= kMaxChannels) return input;

        // Smooth parameter changes to avoid clicks on automation or jumps.
        float targetShiftHz = cachedShiftHz_;
        float targetLock = cachedLock_;
        float targetAsymmetry = cachedAsymmetry_;
        float targetParadoxAmount = cachedParadoxAmount_;
        float targetSubAmount = cachedSubAmount_;
        float targetBifurcatedEcho = cachedBifurcatedEcho_;
        float targetMix = cachedMix_;

        smoothedShiftHz_ += (targetShiftHz - smoothedShiftHz_) * paramSmoothCoeff_;
        smoothedLock_ += (targetLock - smoothedLock_) * paramSmoothCoeff_;
        smoothedAsymmetry_ += (targetAsymmetry - smoothedAsymmetry_) * paramSmoothCoeff_;
        smoothedParadoxAmount_ += (targetParadoxAmount - smoothedParadoxAmount_) * paramSmoothCoeff_;
        smoothedSubAmount_ += (targetSubAmount - smoothedSubAmount_) * paramSmoothCoeff_;
        smoothedBifurcatedEcho_ += (targetBifurcatedEcho - smoothedBifurcatedEcho_) * paramSmoothCoeff_;
        smoothedMix_ += (targetMix - smoothedMix_) * paramSmoothCoeff_;

        float shiftHz = smoothedShiftHz_;
        float lock = smoothedLock_;
        float asymmetry = smoothedAsymmetry_;
        float paradoxAmount = smoothedParadoxAmount_;
        float subAmount = smoothedSubAmount_;
        float bifurcatedEcho = smoothedBifurcatedEcho_;
        float mix = smoothedMix_;

        // 2. Update phase and smooth quantized jumps.
        if (channel0Processed_ != ctx.currentSample) {
            double phaseInc = shiftHz / sampleRate_;
            phase_ += phaseInc;
            if (phase_ >= 1.0) phase_ -= 1.0;
            else if (phase_ < 0.0) phase_ += 1.0;

            float targetPhase = static_cast<float>(phase_); // [0, 1)
            float quantizedPhase = std::floor(targetPhase * 6.0f) / 6.0f;
            float lockAmount = std::clamp(lock, 0.0f, 1.0f);
            targetPhase = targetPhase + (quantizedPhase - targetPhase) * lockAmount;

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

        // 3. Substitution Injection & Pitch Detection (Stub for now)
        // In a real implementation, we'd use YIN/FFT to detect fundamental and shift by 6 semitones.
        // For this example, we apply a subtle spectral shift as a placeholder.
        float dryProcessed = input;
        if (subAmount > 0.01f) {
            // Simple frequency domain shift placeholder (mock)
            dryProcessed += std::sin(2.0f * (float)M_PI * 440.0f * (float)ctx.currentSample / (float)sampleRate_) * 0.1f * subAmount;
        }

        // 4. Paradox Filter (Octave Ambiguity)
        // Strips fundamental identification using a comb filter or high-pass/low-pass sandwich
        if (paradoxAmount > 0.01f) {
            // Placeholder: High-pass and resonance to blur the fundamental
            float cutoff = 200.0f + (paradoxAmount * 1000.0f);
            // Simple 1st-order HP
            float alpha = cutoff / (cutoff + (float)sampleRate_);
            float hp = (1.0f - alpha) * (lastOut_[channel] + dryProcessed - lastIn_[channel]);
            lastIn_[channel] = dryProcessed;
            lastOut_[channel] = hp;
            dryProcessed = (dryProcessed * (1.0f - paradoxAmount)) + (hp * paradoxAmount);
        }

        // Shepard envelope weight with asymmetry
        float weightUp = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * currentPhase));
        float weightDown = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * (currentPhase + 0.5f)));

        // Asymmetry skew [-1, 1], < 0 favors fall, > 0 favors rise
        float skewUp = std::clamp(1.0f + asymmetry, 0.0f, 1.0f);
        float skewDown = std::clamp(1.0f - asymmetry, 0.0f, 1.0f);

        weightUp *= skewUp;
        weightDown *= skewDown;
        
        // Normalize weights
        float totalWeight = weightUp + weightDown;
        if (totalWeight > 0.0f) {
            weightUp /= totalWeight;
            weightDown /= totalWeight;
        }

        float output = 0.0f;
        
        // Granular read for upward shifting
        float maxDelaySamples = sampleRate_ * 0.1f; // 100ms window
        
        auto getSample = [&](float delaySamples) {
            float readPos = static_cast<float>(writePos_[channel]) - 1.0f - delaySamples;
            // Wrap using fmod to handle negative values correctly
            readPos = std::fmod(readPos, (float)bufferSize_);
            if (readPos < 0) readPos += (float)bufferSize_;
            
            // Clamp to valid range before floor to prevent negative indices
            readPos = std::clamp(readPos, 0.0f, (float)bufferSize_ - 1.0f);
            
            int i0 = (int)std::floor(readPos);
            int i1 = i0 + 1;
            if (i1 >= bufferSize_) i1 = 0;
            
            float f = readPos - (float)i0;
            f = std::clamp(f, 0.0f, 1.0f); // Ensure interpolation is within bounds
            
            return delayBuffer_[channel][i0] * (1.0f - f) + delayBuffer_[channel][i1] * f;
        };

        // Rising path (delay decreases)
        float ramp1Up = std::max(1.0f, (1.0f - currentPhase) * maxDelaySamples);
        float ramp2Up = std::fmod(ramp1Up + (maxDelaySamples * 0.5f), maxDelaySamples);
        if (ramp2Up < 0) ramp2Up += maxDelaySamples;
        ramp2Up = std::max(1.0f, ramp2Up);
        float xfadeUpPhase = std::clamp(ramp1Up / maxDelaySamples, 0.0f, 1.0f);
        float hannUp = 0.5f - 0.5f * std::cos(2.0f * (float)M_PI * xfadeUpPhase);
        float upOut = (getSample(ramp1Up) * hannUp) + (getSample(ramp2Up) * (1.0f - hannUp));

        // Falling path (delay increases)
        float ramp1Down = std::max(1.0f, currentPhase * maxDelaySamples);
        float ramp2Down = std::fmod(ramp1Down + (maxDelaySamples * 0.5f), maxDelaySamples);
        if (ramp2Down < 0) ramp2Down += maxDelaySamples;
        ramp2Down = std::max(1.0f, ramp2Down);
        float xfadeDownPhase = std::clamp(ramp1Down / maxDelaySamples, 0.0f, 1.0f);
        float hannDown = 0.5f - 0.5f * std::cos(2.0f * (float)M_PI * xfadeDownPhase);
        float downOut = (getSample(ramp1Down) * hannDown) + (getSample(ramp2Down) * (1.0f - hannDown));

        output = (upOut * weightUp) + (downOut * weightDown);

        // 5. Bifurcated Echo
        if (bifurcatedEcho > 0.01f) {
            // Echo delay at a fixed tritone interval (approx 300-500ms for rhythm)
            float tritoneDelay = sampleRate_ * 0.4f; 
            float echo = getSample(tritoneDelay) * 0.5f;
            output = (output * (1.0f - (bifurcatedEcho * 0.5f))) + (echo * bifurcatedEcho);
        }

        // Write to delay buffer
        delayBuffer_[channel][writePos_[channel]] = dryProcessed;
        writePos_[channel] = (writePos_[channel] + 1) % bufferSize_;

        return (input * (1.0f - mix)) + (output * mix);
    }

    void reset() override {
        phase_ = 0.0;
        channel0Processed_ = -1;
        smoothedPhase_ = 0.0f;
        currentPhase_ = 0.0f;
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(delayBuffer_[ch].begin(), delayBuffer_[ch].end(), 0.0f);
            writePos_[ch] = 0;
            lastIn_[ch] = 0.0f;
            lastOut_[ch] = 0.0f;
        }
    }

private:
    static constexpr int kMaxChannels = 2;
    std::vector<float> delayBuffer_[kMaxChannels];
    int writePos_[kMaxChannels] = {0, 0};
    int bufferSize_ = 0;
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    std::int64_t channel0Processed_ = -1;

    float lastIn_[kMaxChannels] = {0.0f, 0.0f};
    float lastOut_[kMaxChannels] = {0.0f, 0.0f};
    
    // PERFORMANCE: Cached parameter values
    float cachedShiftHz_ = 0.5f;
    float cachedLock_ = 0.0f;
    float cachedAsymmetry_ = 0.0f;
    float cachedParadoxAmount_ = 0.0f;
    float cachedSubAmount_ = 0.0f;
    float cachedBifurcatedEcho_ = 0.0f;
    float cachedMix_ = 0.5f;
    std::int64_t lastBlockStart_ = -1;

    float smoothedPhase_ = 0.0f;
    float currentPhase_ = 0.0f;
    float phaseSmoothCoeff_ = 1.0f;
    float paramSmoothCoeff_ = 1.0f;

    float smoothedShiftHz_ = 0.5f;
    float smoothedLock_ = 0.0f;
    float smoothedAsymmetry_ = 0.0f;
    float smoothedParadoxAmount_ = 0.0f;
    float smoothedSubAmount_ = 0.0f;
    float smoothedBifurcatedEcho_ = 0.0f;
    float smoothedMix_ = 0.5f;
};

AGENTVST_REGISTER_DSP(Escher_Pitch_ShifterProcessor)
