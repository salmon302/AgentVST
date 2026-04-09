/**
 * CompressorDSP.cpp — Feed-forward RMS compressor
 *
 * Demonstrates the AgentVST DSP workflow for a multi-parameter dynamics plugin.
 *
 * Algorithm: feed-forward compressor
 *   1. Compute RMS level via a leaky integrator
 *   2. Compute gain reduction in dB using a soft-knee characteristic
 *   3. Smooth the gain reduction with separate attack/release coefficients
 *   4. Apply smoothed gain + makeup gain to the signal
 *
 * REAL-TIME CONSTRAINTS OBEYED: no allocation, no locks, no I/O.
 */
#include <AgentDSP.h>
#include <cmath>
#include <algorithm>

class CompressorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        // Pre-compute initial coefficients (will be updated each block from params)
        updateCoefficients(10.0f, 100.0f);
        // Reset state
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f)
            return input;

        // Read parameters (lock-free atomic reads, memory_order_relaxed)
        const float threshDb  = ctx.getParameter("threshold_db");
        const float ratio     = std::max(1.0f, ctx.getParameter("ratio"));
        const float attackMs  = ctx.getParameter("attack_ms");
        const float releaseMs = ctx.getParameter("release_ms");
        const float makeupDb  = ctx.getParameter("makeup_db");
        const float kneeDb    = ctx.getParameter("knee_db");

        // Update envelope coefficients from parameters
        // (updateCoefficients is cheap: just two exp() calls)
        updateCoefficients(attackMs, releaseMs);

        // ── 1. RMS level detection (per-channel) ─────────────────────────────
        const int ch = std::min(channel, kMaxCh - 1);
        const float xSq = input * input;
        rmsState_[ch] = rmsCoeff_ * rmsState_[ch] + (1.0f - rmsCoeff_) * xSq;
        const float rmsLin = std::sqrt(std::max(0.0f, rmsState_[ch]));

        // ── 2. Level in dB (guard against log(0)) ────────────────────────────
        const float levelDb = (rmsLin > 1e-9f)
                                  ? 20.0f * std::log10(rmsLin)
                                  : -120.0f;

        // ── 3. Gain computer with soft knee ──────────────────────────────────
        float gainReductionDb = 0.0f;
        const float halfKnee  = kneeDb * 0.5f;
        const float overDb    = levelDb - threshDb;

        if (kneeDb > 0.0f && overDb > -halfKnee && overDb < halfKnee) {
            // Soft-knee region: interpolate between unity and full ratio
            const float t = (overDb + halfKnee) / kneeDb; // 0..1
            gainReductionDb = (1.0f / ratio - 1.0f) * overDb * t * t * 0.5f;
        } else if (overDb >= halfKnee) {
            // Above threshold: full ratio
            gainReductionDb = (1.0f / ratio - 1.0f) * overDb;
        }
        // Below threshold: gainReductionDb = 0

        // ── 4. Ballistics: smooth gain reduction ─────────────────────────────
        const float targetGrDb = gainReductionDb; // negative or zero
        auto& grState = grState_[ch];

        if (targetGrDb < grState) {
            // Attack: gain is decreasing (more compression)
            grState = attackCoeff_ * grState + (1.0f - attackCoeff_) * targetGrDb;
        } else {
            // Release: gain is recovering
            grState = releaseCoeff_ * grState + (1.0f - releaseCoeff_) * targetGrDb;
        }

        // ── 5. Apply gain reduction + makeup ─────────────────────────────────
        const float totalDb  = grState + makeupDb;
        const float gainLin  = std::pow(10.0f, totalDb / 20.0f);
        return input * gainLin;
    }

    void reset() override {
        for (auto& s : rmsState_) s = 0.0f;
        for (auto& s : grState_)  s = 0.0f;
    }

private:
    static constexpr int kMaxCh = 8;

    double sampleRate_   = 44100.0;
    float  attackCoeff_  = 0.0f;
    float  releaseCoeff_ = 0.0f;
    float  rmsCoeff_     = 0.0f;

    float  rmsState_[kMaxCh] = {};
    float  grState_ [kMaxCh] = {};

    void updateCoefficients(float attackMs, float releaseMs) noexcept {
        const float sr = static_cast<float>(sampleRate_);
        attackCoeff_  = std::exp(-1.0f / (sr * attackMs  * 0.001f));
        releaseCoeff_ = std::exp(-1.0f / (sr * releaseMs * 0.001f));
        // RMS window: ~50ms time constant
        rmsCoeff_     = std::exp(-1.0f / (sr * 0.050f));
    }
};

AGENTVST_REGISTER_DSP(CompressorProcessor)
