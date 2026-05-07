// Purpose: PhantomSubHarmonicExciter DSP processing.
// Author: Seth Nenninger (GPT-5.2-Codex Agent)
// Timestamp: 2026-05-06T18:58:46Z
// Changelog: Refocus harmonic band and stabilize output level.

/**
 * Generated DSP scaffold.
 *
 * Keep processSample real-time safe:
 * - no allocations
 * - no locks
 * - no I/O
 */
#include <AgentDSP.h>
#include "modules/BiquadFilter.h"
#include <cmath>
#include <algorithm>
#include <array>

class PhantomSubHarmonicExciterProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        paramSmoothCoeff_ = std::exp(-1.0f / (0.05f * static_cast<float>(sampleRate_)));
        for (int ch = 0; ch < kMaxCh; ++ch) {
            excisionEq_[ch].prepare(sampleRate, maxBlockSize);
            isolateLp_[ch].prepare(sampleRate, maxBlockSize);
            harmonicsHp_[ch].prepare(sampleRate, maxBlockSize);
            postLp_[ch].prepare(sampleRate, maxBlockSize);

            // Path A: The notch to remove the fundamental
            excisionEq_[ch].setType(AgentVST::BiquadFilter::Type::Peak);
            excisionEq_[ch].setQ(2.5f); // Tight, surgical excision

            // Path B: The low-pass to isolate the sub before distorting
            isolateLp_[ch].setType(AgentVST::BiquadFilter::Type::LowPass);
            isolateLp_[ch].setQ(0.707f); // Butterworth

            // Path B: The high-pass to remove fundamental after clipping, leaving ONLY harmonics
            harmonicsHp_[ch].setType(AgentVST::BiquadFilter::Type::HighPass);
            harmonicsHp_[ch].setQ(0.707f);

            postLp_[ch].setType(AgentVST::BiquadFilter::Type::LowPass);
            postLp_[ch].setQ(0.707f);
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
                        
        const int ch = std::min(channel, kMaxCh - 1);

        // Cache parameters per block
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedRootFreq_ = ctx.getParameter("phantom_root"); // 20 - 300 Hz
            cachedExcision_ = ctx.getParameter("sub_excision"); // 0 - 48 dB
            cachedDrivePct_ = ctx.getParameter("harmonic_drive"); // 0 - 100 %
            cachedMixPct_ = ctx.getParameter("mix"); // 0 - 100 %
        }

        if (smoothedRoot_ <= 0.0f) smoothedRoot_ = cachedRootFreq_;
        if (smoothedExcision_ <= 0.0f) smoothedExcision_ = cachedExcision_;
        if (smoothedDrive_ <= 0.0f) smoothedDrive_ = cachedDrivePct_;
        if (smoothedMix_ <= 0.0f) smoothedMix_ = cachedMixPct_;

        smoothedRoot_ = paramSmoothCoeff_ * smoothedRoot_ + (1.0f - paramSmoothCoeff_) * cachedRootFreq_;
        smoothedExcision_ = paramSmoothCoeff_ * smoothedExcision_ + (1.0f - paramSmoothCoeff_) * cachedExcision_;
        smoothedDrive_ = paramSmoothCoeff_ * smoothedDrive_ + (1.0f - paramSmoothCoeff_) * cachedDrivePct_;
        smoothedMix_ = paramSmoothCoeff_ * smoothedMix_ + (1.0f - paramSmoothCoeff_) * cachedMixPct_;

        const float rootFreq = smoothedRoot_;
        const float excision = smoothedExcision_;

        // Update Filters lazily (channel 0 drives update for both)
        if (channel == 0) {
            if (std::abs(rootFreq - lastRootFreq_) > 0.01f || std::abs(excision - lastExcision_) > 0.01f) {
                for (int c = 0; c < kMaxCh; ++c) {
                    excisionEq_[c].setFrequency(rootFreq);
                    excisionEq_[c].setGainDb(-excision);

                    isolateLp_[c].setFrequency(std::min(rootFreq * 1.4f, 20000.0f));
                    harmonicsHp_[c].setFrequency(std::clamp(rootFreq * 1.1f, 20.0f, 20000.0f));
                    postLp_[c].setFrequency(std::clamp(rootFreq * 18.0f, 1200.0f, 8000.0f));
                }
                lastRootFreq_ = rootFreq;
                lastExcision_ = excision;
            }
        }

        // Path A: Excision of the sub fundamental
        float pathA = excisionEq_[ch].processSample(channel, input);

        // Path B: Phantom Harmonics Generation
        float isolated = isolateLp_[ch].processSample(channel, input);
        
        // Controlled saturation curve for "Harmonic Drive"
        // 0% -> x1 gain, 100% -> ~x13 gain into tanh to shape harmonics
        const float driveNorm = smoothedDrive_ * 0.01f;
        const float driveGain = 1.0f + driveNorm * driveNorm * 10.0f;
        const float driveComp = 1.0f / (1.0f + driveNorm * 2.3f);
        float saturated = std::tanh(isolated * driveGain) * driveComp;

        // Strip the fundamental away from the distorted signal, leaving only the upper harmonics
        float overtones = harmonicsHp_[ch].processSample(channel, saturated);
        overtones = postLp_[ch].processSample(channel, overtones);
        overtones = std::tanh(overtones * (1.0f + driveNorm * 0.6f));

        // Blend
        // The drive control also scales the volume of the injected harmonics to prevent blowing up the mix at 0%
        const float excisionNorm = std::clamp(smoothedExcision_ / 48.0f, 0.0f, 1.0f);
        const float exciteGain = driveNorm * (0.35f + 0.65f * excisionNorm);
        float output = (pathA * 0.95f) + (overtones * exciteGain);

        // Master Dry/Wet
        float wetMix = smoothedMix_ * 0.01f;
        return (input * (1.0f - wetMix)) + (output * wetMix);
    }

    void reset() override {
        for (int ch = 0; ch < kMaxCh; ++ch) {
            excisionEq_[ch].reset();
            isolateLp_[ch].reset();
            harmonicsHp_[ch].reset();
            postLp_[ch].reset();
        }
        lastRootFreq_ = -1.0f;
        lastExcision_ = -1.0f;
        smoothedRoot_ = 0.0f;
        smoothedExcision_ = 0.0f;
        smoothedDrive_ = 0.0f;
        smoothedMix_ = 0.0f;
    }

private:
    static constexpr int kMaxCh = 8; // Support up to 7.1 surround

    std::array<AgentVST::BiquadFilter, kMaxCh> excisionEq_;
    std::array<AgentVST::BiquadFilter, kMaxCh> isolateLp_;
    std::array<AgentVST::BiquadFilter, kMaxCh> harmonicsHp_;
    std::array<AgentVST::BiquadFilter, kMaxCh> postLp_;

    double sampleRate_ = 44100.0;
    float paramSmoothCoeff_ = 0.0f;
    float lastRootFreq_ = -1.0f;
    float lastExcision_ = -1.0f;
    int lastBlockStart_ = -1;
    float cachedRootFreq_ = 110.0f;
    float cachedExcision_ = 0.0f;
    float cachedDrivePct_ = 0.0f;
    float cachedMixPct_ = 0.0f;
    float smoothedRoot_ = 0.0f;
    float smoothedExcision_ = 0.0f;
    float smoothedDrive_ = 0.0f;
    float smoothedMix_ = 0.0f;
};

AGENTVST_REGISTER_DSP(PhantomSubHarmonicExciterProcessor)
