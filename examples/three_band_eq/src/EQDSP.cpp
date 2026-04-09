/**
 * EQDSP.cpp — 3-band parametric EQ using AgentVST's built-in BiquadFilter modules.
 *
 * Band topology:
 *   Band 1: Low Shelf  @ low_freq_hz  with low_gain_db
 *   Band 2: Peak EQ    @ mid_freq_hz  with mid_gain_db / mid_q
 *   Band 3: High Shelf @ high_freq_hz with high_gain_db
 *
 * Demonstrates using pre-built DSP modules from the AgentVST module library.
 * Each BiquadFilter is a separate object per stereo channel for independent state.
 *
 * REAL-TIME CONSTRAINTS OBEYED: all objects pre-allocated; no allocation in processSample.
 */
#include <AgentDSP.h>

// Include pre-built modules from the framework
// (these headers are on the include path set by agentvst_add_plugin)
#include "modules/BiquadFilter.h"

#include <cmath>
#include <array>

class EQProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        for (int ch = 0; ch < kMaxCh; ++ch) {
            lowShelf_ [ch].prepare(sampleRate, maxBlockSize);
            midPeak_  [ch].prepare(sampleRate, maxBlockSize);
            highShelf_[ch].prepare(sampleRate, maxBlockSize);

            lowShelf_ [ch].setType(AgentVST::BiquadFilter::Type::LowShelf);
            midPeak_  [ch].setType(AgentVST::BiquadFilter::Type::Peak);
            highShelf_[ch].setType(AgentVST::BiquadFilter::Type::HighShelf);
        }
        lastLowFreq_  = -1.0f;
        lastMidFreq_  = -1.0f;
        lastHighFreq_ = -1.0f;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f)
            return input;

        const int ch = std::min(channel, kMaxCh - 1);

        // Read parameters
        const float lowGain  = ctx.getParameter("low_gain_db");
        const float lowFreq  = ctx.getParameter("low_freq_hz");
        const float midGain  = ctx.getParameter("mid_gain_db");
        const float midFreq  = ctx.getParameter("mid_freq_hz");
        const float midQ     = ctx.getParameter("mid_q");
        const float highGain = ctx.getParameter("high_gain_db");
        const float highFreq = ctx.getParameter("high_freq_hz");

        // Update filter parameters only when they change (lazy update, channel 0 drives)
        // This is real-time safe because BiquadFilter recomputes coefficients lazily
        // inside processSample using a dirty flag — no allocation.
        if (channel == 0) {
            if (lowFreq  != lastLowFreq_)  { updateAll(lowShelf_,  lowFreq,  0.707f, lowGain);  lastLowFreq_  = lowFreq; }
            if (midFreq  != lastMidFreq_)  { updateAll(midPeak_,   midFreq,  midQ,   midGain);  lastMidFreq_  = midFreq; }
            if (highFreq != lastHighFreq_) { updateAll(highShelf_, highFreq, 0.707f, highGain); lastHighFreq_ = highFreq; }
            // Also push gain/Q changes every sample (cheap dirty-flag check inside BiquadFilter)
            for (int i = 0; i < kMaxCh; ++i) {
                lowShelf_ [i].setGainDb(lowGain);
                midPeak_  [i].setGainDb(midGain);
                midPeak_  [i].setQ(midQ);
                highShelf_[i].setGainDb(highGain);
            }
        }

        // Process through the three bands in series
        float out = lowShelf_ [ch].processSample(ch, input);
        out       = midPeak_  [ch].processSample(ch, out);
        out       = highShelf_[ch].processSample(ch, out);
        return out;
    }

    void reset() override {
        for (int ch = 0; ch < kMaxCh; ++ch) {
            lowShelf_ [ch].reset();
            midPeak_  [ch].reset();
            highShelf_[ch].reset();
        }
    }

private:
    static constexpr int kMaxCh = 8;

    // One filter object per band per channel — all stack/member allocated, never heap
    std::array<AgentVST::BiquadFilter, kMaxCh> lowShelf_;
    std::array<AgentVST::BiquadFilter, kMaxCh> midPeak_;
    std::array<AgentVST::BiquadFilter, kMaxCh> highShelf_;

    float lastLowFreq_  = -1.0f;
    float lastMidFreq_  = -1.0f;
    float lastHighFreq_ = -1.0f;

    void updateAll(std::array<AgentVST::BiquadFilter, kMaxCh>& band,
                   float freq, float q, float gainDb) {
        for (auto& f : band) {
            f.setFrequency(freq);
            f.setQ(q);
            f.setGainDb(gainDb);
        }
    }
};

AGENTVST_REGISTER_DSP(EQProcessor)
