/**
 * BiquadFilter.h — Pre-built biquad IIR filter module
 *
 * Implements a second-order IIR (biquad) filter using Direct Form II Transposed.
 * Supports: Low-Pass, High-Pass, Band-Pass, Notch, Peak EQ, Low-Shelf, High-Shelf.
 *
 * Coefficient smoothing prevents zipper noise on parameter changes.
 * All memory is pre-allocated; no dynamic allocation in processSample().
 *
 * REAL-TIME SAFE: processSample() contains zero allocations, no locks, no I/O.
 */
#pragma once

#include "DSPNode.h"
#include <array>
#include <cmath>
#include <string>

namespace AgentVST {

class BiquadFilter : public DSPNode {
public:
    enum class Type { LowPass, HighPass, BandPass, Notch, Peak, LowShelf, HighShelf };

    BiquadFilter();

    // ── DSPNode interface ─────────────────────────────────────────────────────
    void  prepare(double sampleRate, int maxBlockSize) override;
    float processSample(int channel, float input) override;
    void  reset() override;
    void  setParameter(const std::string& name, float value) override;

    // ── Direct API ────────────────────────────────────────────────────────────
    void setType     (Type type)         noexcept;
    void setFrequency(float freqHz)      noexcept;
    void setQ        (float q)           noexcept;
    void setGainDb   (float gainDb)      noexcept; ///< For Peak / Shelf only

    Type  getType()      const noexcept { return type_; }
    float getFrequency() const noexcept { return freqHz_; }
    float getQ()         const noexcept { return q_; }

private:
    static constexpr int kMaxChannels = 8;

    // Filter type and parameters
    Type  type_   = Type::LowPass;
    float freqHz_ = 1000.0f;
    float q_      = 0.707f;
    float gainDb_ = 0.0f;

    // Biquad coefficients: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    // Direct Form II Transposed state: s1, s2 per channel
    struct Coeffs { float b0, b1, b2, a1, a2; };
    Coeffs coeffs_{};

    // Per-channel state (Direct Form II Transposed)
    struct State { float s1 = 0.0f, s2 = 0.0f; };
    std::array<State, kMaxChannels> state_{};

    bool dirty_ = true; ///< Recompute coefficients on next processSample

    void computeCoefficients() noexcept;

    static constexpr float kPi = 3.14159265358979323846f;
};

// ─── Inline DSPNode::setParameter dispatch ───────────────────────────────────
inline void BiquadFilter::setParameter(const std::string& name, float value) {
    if (name == "frequency") setFrequency(value);
    else if (name == "q")    setQ(value);
    else if (name == "gain") setGainDb(value);
    else if (name == "type") setType(static_cast<Type>(static_cast<int>(value)));
}

} // namespace AgentVST
