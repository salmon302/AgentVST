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
        for (int ch = 0; ch < kMaxCh; ++ch) {
            excisionEq_[ch].prepare(sampleRate, maxBlockSize);
            isolateLp_[ch].prepare(sampleRate, maxBlockSize);
            harmonicsHp_[ch].prepare(sampleRate, maxBlockSize);

            // Path A: The notch to remove the fundamental
            excisionEq_[ch].setType(AgentVST::BiquadFilter::Type::Peak);
            excisionEq_[ch].setQ(2.5f); // Tight, surgical excision

            // Path B: The low-pass to isolate the sub before distorting
            isolateLp_[ch].setType(AgentVST::BiquadFilter::Type::LowPass);
            isolateLp_[ch].setQ(0.707f); // Butterworth

            // Path B: The high-pass to remove fundamental after clipping, leaving ONLY harmonics
            harmonicsHp_[ch].setType(AgentVST::BiquadFilter::Type::HighPass);
            harmonicsHp_[ch].setQ(0.707f);
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
                        
        const int ch = std::min(channel, kMaxCh - 1);

        // Fetch Parameters
        const float rootFreq = ctx.getParameter("phantom_root"); // 20 - 300 Hz
        const float excision = ctx.getParameter("sub_excision"); // 0 - 48 dB
        const float drivePct = ctx.getParameter("harmonic_drive"); // 0 - 100 %
        const float mixPct   = ctx.getParameter("mix"); // 0 - 100 %

        // Update Filters lazily (channel 0 drives update for both)
        if (channel == 0) {
            if (rootFreq != lastRootFreq_ || excision != lastExcision_) {
                for (int c = 0; c < kMaxCh; ++c) {
                    excisionEq_[c].setFrequency(rootFreq);
                    excisionEq_[c].setGainDb(-excision);
                    
                    isolateLp_[c].setFrequency(std::min(rootFreq * 1.5f, 20000.0f));
                    harmonicsHp_[c].setFrequency(std::min(rootFreq * 2.0f, 20000.0f)); 
                }
                lastRootFreq_ = rootFreq;
                lastExcision_ = excision;
            }
        }

        // Path A: Excision of the sub fundamental
        float pathA = excisionEq_[ch].processSample(channel, input);

        // Path B: Phantom Harmonics Generation
        float isolated = isolateLp_[ch].processSample(channel, input);
        
        // Massive saturation curve for "Harmonic Drive"
        // 0% -> x1 gain, 100% -> x50 gain into tanh to smash it into a square/odd/even harmonic shape
        float driveGain = 1.0f + (drivePct * 0.49f); 
        float saturated = std::tanh(isolated * driveGain);

        // Strip the fundamental away from the distorted signal, leaving only the upper harmonics
        float overtones = harmonicsHp_[ch].processSample(channel, saturated);

        // Blend
        // The drive control also scales the volume of the injected harmonics to prevent blowing up the mix at 0%
        float output = pathA + (overtones * (drivePct / 100.0f));

        // Master Dry/Wet
        float wetMix = mixPct / 100.0f;
        return (input * (1.0f - wetMix)) + (output * wetMix);
    }

    void reset() override {
        for (int ch = 0; ch < kMaxCh; ++ch) {
            excisionEq_[ch].reset();
            isolateLp_[ch].reset();
            harmonicsHp_[ch].reset();
        }
        lastRootFreq_ = -1.0f;
        lastExcision_ = -1.0f;
    }

private:
    static constexpr int kMaxCh = 8; // Support up to 7.1 surround

    std::array<AgentVST::BiquadFilter, kMaxCh> excisionEq_;
    std::array<AgentVST::BiquadFilter, kMaxCh> isolateLp_;
    std::array<AgentVST::BiquadFilter, kMaxCh> harmonicsHp_;

    float lastRootFreq_ = -1.0f;
    float lastExcision_ = -1.0f;
};

AGENTVST_REGISTER_DSP(PhantomSubHarmonicExciterProcessor)
