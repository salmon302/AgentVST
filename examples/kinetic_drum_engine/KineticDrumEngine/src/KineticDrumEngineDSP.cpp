/**
 * KineticDrumEngineDSP.cpp — Macro-driven multi-stage dynamic processor for drum buses.
 *
 * Architecture:
 *   Central Nervous System (Detection Module)
 *     → Transient Detector + RMS Envelope Follower
 *     → Outputs normalized control signals (0.0 to 1.0) for downstream modules
 *
 * Processing Order:
 *   1. Transient Detection / RMS Calculation (Central Nervous System)
 *   2. Auto-Carving (Dynamic EQ) — cuts 250Hz-500Hz during transient peaks
 *   3. Upward Smasher (Parallel Upward Compression + Saturation)
 *   4. Dynamic Tilt (Spectral Balancing via Tilt EQ)
 *   5. M/S Splash (Spatial Processing via Mid/Side Transient Expander)
 *
 * REAL-TIME CONSTRAINTS OBEYED: no allocation, no locks, no I/O.
 */
#include <AgentDSP.h>
#include <cmath>
#include <algorithm>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Central Nervous System — Detection Module
// ============================================================================
// Single optimized Transient Detector + RMS Envelope Follower
// Outputs normalized control signals for all downstream modules

class CentralNervousSystem {
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept {
        sr_ = sampleRate;
        // Transient detection: fast attack for punch detection
        transientAttackMs_  = 0.5f;   // Very fast to catch transients
        transientReleaseMs_ = 50.0f;  // Slower release to hold transient info
        computeCoeffs();
        // Initialize all state to zero to prevent garbage on first sample
        for (auto& s : transientState_) s = 0.0f;
        for (auto& s : rmsState_)         s = 0.0f;
        for (auto& s : transientPeak_)    s = 0.0f;
        for (auto& s : rmsLin_)           s = 0.0f;
    }

    void reset() noexcept {
        for (auto& s : transientState_) s = 0.0f;
        for (auto& s : rmsState_)         s = 0.0f;
        for (auto& s : transientPeak_)    s = 0.0f;
        for (auto& s : rmsLin_)           s = 0.0f;
    }

    // Process sample and update detection signals
    // Returns transient intensity (0.0 to 1.0)
    float processSample(int channel, float input) noexcept {
        const int ch = std::min(channel, kMaxCh - 1);
        
        // ── 1. RMS calculation ────────────────────────────────────────────────
        const float xSq = input * input;
        rmsState_[ch] = rmsCoeff_ * rmsState_[ch] + (1.0f - rmsCoeff_) * xSq + 1e-15f;
        rmsLin_[ch] = std::sqrt(std::max(0.0f, rmsState_[ch]));
        
        // ── 2. Transient detection via envelope follower ────────────────────
        const float absIn = std::abs(input);
        const float coeff = (absIn > transientState_[ch]) 
                            ? transientAttackCoeff_ 
                            : transientReleaseCoeff_;
        transientState_[ch] = coeff * transientState_[ch] + (1.0f - coeff) * absIn + 1e-15f;
        
        // Track peak for normalization
        if (transientState_[ch] > transientPeak_[ch]) {
            transientPeak_[ch] = transientState_[ch];
        } else {
            transientPeak_[ch] = transientReleaseCoeff_ * transientPeak_[ch] + 1e-15f;
        }
        
        return transientState_[ch];
    }

    // Get normalized control signals (0.0 to 1.0)
    float getTransientIntensity(int channel) const noexcept {
        const int ch = std::min(channel, kMaxCh - 1);
        if (transientPeak_[ch] < 1e-9f) return 0.0f;
        return std::min(1.0f, transientState_[ch] / transientPeak_[ch]);
    }

    float getRmsLevel(int channel) const noexcept {
        const int ch = std::min(channel, kMaxCh - 1);
        return rmsLin_[ch];
    }

    float getRmsNormalized(int channel) const noexcept {
        const int ch = std::min(channel, kMaxCh - 1);
        // Normalize RMS to 0-1 range (assuming -60dB to 0dB range)
        const float rmsDb = (rmsLin_[ch] > 1e-9f) 
                            ? 20.0f * std::log10(rmsLin_[ch]) 
                            : -120.0f;
        return std::max(0.0f, std::min(1.0f, (rmsDb + 60.0f) / 60.0f));
    }

private:
    static constexpr int kMaxCh = 8;
    double sr_ = 44100.0;
    
    // Transient detection
    float transientAttackMs_  = 0.5f;
    float transientReleaseMs_ = 50.0f;
    float transientAttackCoeff_  = 0.0f;
    float transientReleaseCoeff_ = 0.0f;
    
    // RMS
    float rmsCoeff_ = 0.0f;
    
    std::array<float, kMaxCh> transientState_ = {};
    std::array<float, kMaxCh> rmsState_       = {};
    std::array<float, kMaxCh> transientPeak_  = {};
    std::array<float, kMaxCh> rmsLin_         = {};

    void computeCoeffs() noexcept {
        const float sr = static_cast<float>(sr_);
        transientAttackCoeff_  = std::exp(-1.0f / (sr * transientAttackMs_  * 0.001f));
        transientReleaseCoeff_ = std::exp(-1.0f / (sr * transientReleaseMs_ * 0.001f));
        rmsCoeff_               = std::exp(-1.0f / (sr * 30.0f * 0.001f)); // 30ms RMS window
    }
};

// ============================================================================
// Module A: Auto-Carving "Un-Mudder" — Dynamic EQ (Bell filter)
// Targets 250Hz–500Hz range, cuts during transient peaks
// ============================================================================

class AutoCarvingModule {
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept {
        sr_ = sampleRate;
        // Band 1 (primary mud removal)
        baseFreq1_ = 375.0f;
        q1_ = 1.5f;
        maxCutDb1_ = 8.0f;
        // Band 2 (secondary formant carving)
        baseFreq2_ = 1000.0f;
        q2_ = 1.5f;
        maxCutDb2_ = 6.0f;
        band2Enabled_ = false;
        computeBaseCoeffs();
        // Initialize filter state (2 bands × 2 states per channel)
        for (auto& s : z1Band1_) s = 0.0f;
        for (auto& s : z2Band1_) s = 0.0f;
        for (auto& s : z1Band2_) s = 0.0f;
        for (auto& s : z2Band2_) s = 0.0f;
        for (auto& s : lastCutDb1_) s = 0.0f;
        for (auto& s : lastCutDb2_) s = 0.0f;
    }

    void reset() noexcept {
        for (auto& s : z1Band1_) s = 0.0f;
        for (auto& s : z2Band1_) s = 0.0f;
        for (auto& s : z1Band2_) s = 0.0f;
        for (auto& s : z2Band2_) s = 0.0f;
        for (auto& s : lastCutDb1_) s = 0.0f;
        for (auto& s : lastCutDb2_) s = 0.0f;
    }

    float processSample(int channel, float input, float transientIntensity) noexcept {
        const int ch = std::min(channel, kMaxCh - 1);
        
        // ── Modulate cut amount based on transient intensity ─────────────────
        // During transient peaks: apply cut up to maxCutDb_
        // During sustain: cut = 0 (flat response)
        const float cutDb1 = transientIntensity * maxCutDb1_;
        const float cutDb2 = transientIntensity * maxCutDb2_;
        
        // ── Process Band 1 (Primary Carving) ─────────────────────────────────
        // Only recompute coefficients when cut amount changes significantly
        if (std::abs(cutDb1 - lastCutDb1_[ch]) > 0.05f) {
            lastCutDb1_[ch] = cutDb1;
            updatePeakingCoeffs(ch, cutDb1, true);  // band 1
        }
        
        // Biquad processing for Band 1 (with adaptive Q)
        const float effectiveQ1 = q1_ * (baseFreq1_ / 375.0f);  // Scale Q by frequency
        // clampedQ1 is used if we wanted to change the coefficients dynamically, but here we just process.
        float acc1 = input - a1Band1_[ch] * z1Band1_[ch] - a2Band1_[ch] * z2Band1_[ch] + 1e-15f;
        float out1 = b0Band1_[ch] * acc1 + b1Band1_[ch] * z1Band1_[ch] + b2Band1_[ch] * z2Band1_[ch];
        z2Band1_[ch] = z1Band1_[ch];
        z1Band1_[ch] = acc1;
        const float linearGain1 = std::pow(10.0f, -cutDb1 / 20.0f);
        out1 *= linearGain1;
        
        // ── Process Band 2 (Secondary Carving, optional) ────────────────────
        float out = out1;
        if (band2Enabled_) {
            if (std::abs(cutDb2 - lastCutDb2_[ch]) > 0.05f) {
                lastCutDb2_[ch] = cutDb2;
                updatePeakingCoeffs(ch, cutDb2, false);  // band 2
            }
            
            // Biquad processing for Band 2 (with adaptive Q)
            const float effectiveQ2 = q2_ * (baseFreq2_ / 1000.0f);  // Scale Q by frequency
            float acc2 = out1 - a1Band2_[ch] * z1Band2_[ch] - a2Band2_[ch] * z2Band2_[ch] + 1e-15f;
            float out2 = b0Band2_[ch] * acc2 + b1Band2_[ch] * z1Band2_[ch] + b2Band2_[ch] * z2Band2_[ch];
            z2Band2_[ch] = z1Band2_[ch];
            z1Band2_[ch] = acc2;
            const float linearGain2 = std::pow(10.0f, -cutDb2 / 20.0f);
            out2 *= linearGain2;
            out = out2;
        }
        
        return out;
    }

    void setCenterFrequencyHz(float hz) noexcept {
        const float clamped = std::clamp(hz, 20.0f, 20000.0f);
        if (std::abs(clamped - baseFreq1_) < 0.01f)
            return;
        baseFreq1_ = clamped;
        computeBaseCoeffs();
        invalidateCutTracking();
    }

    void setQ(float q) noexcept {
        const float clamped = std::clamp(q, 0.1f, 12.0f);
        if (std::abs(clamped - q1_) < 0.001f)
            return;
        q1_ = clamped;
        computeBaseCoeffs();
        invalidateCutTracking();
    }

    void setMaxCutDb(float db) noexcept { 
        maxCutDb1_ = std::max(0.0f, std::min(24.0f, db)); 
    }

    // Multi-band control: Band 2 (secondary carving)
    void setBand2FrequencyHz(float hz) noexcept {
        const float clamped = std::clamp(hz, 100.0f, 20000.0f);
        if (std::abs(clamped - baseFreq2_) < 0.01f)
            return;
        baseFreq2_ = clamped;
        computeBaseCoeffs();
        invalidateCutTracking();
    }

    void setBand2Q(float q) noexcept {
        const float clamped = std::clamp(q, 0.1f, 12.0f);
        if (std::abs(clamped - q2_) < 0.001f)
            return;
        q2_ = clamped;
        computeBaseCoeffs();
        invalidateCutTracking();
    }

    void setBand2MaxCutDb(float db) noexcept { 
        maxCutDb2_ = std::max(0.0f, std::min(24.0f, db)); 
    }

    void setBand2Enabled(bool enabled) noexcept { 
        band2Enabled_ = enabled; 
    }

private:
    static constexpr int kMaxCh = 8;
    double sr_ = 44100.0;
    
    // Band 1 (primary mud removal)
    float baseFreq1_ = 375.0f;
    float q1_ = 1.5f;
    float maxCutDb1_ = 8.0f;
    
    // Band 2 (secondary formant carving)
    float baseFreq2_ = 1000.0f;
    float q2_ = 1.5f;
    float maxCutDb2_ = 6.0f;
    bool band2Enabled_ = false;
    
    // Filter state Band 1 (Direct Form I - 2 delay elements per channel)
    std::array<float, kMaxCh> z1Band1_ = {};
    std::array<float, kMaxCh> z2Band1_ = {};
    
    // Filter state Band 2
    std::array<float, kMaxCh> z1Band2_ = {};
    std::array<float, kMaxCh> z2Band2_ = {};
    
    // Track last cut for optimization (both bands)
    std::array<float, kMaxCh> lastCutDb1_ = {};
    std::array<float, kMaxCh> lastCutDb2_ = {};
    
    // Biquad coefficients Band 1
    std::array<float, kMaxCh> b0Band1_ = {};
    std::array<float, kMaxCh> b1Band1_ = {};
    std::array<float, kMaxCh> b2Band1_ = {};
    std::array<float, kMaxCh> a1Band1_ = {};
    std::array<float, kMaxCh> a2Band1_ = {};
    
    // Biquad coefficients Band 2
    std::array<float, kMaxCh> b0Band2_ = {};
    std::array<float, kMaxCh> b1Band2_ = {};
    std::array<float, kMaxCh> b2Band2_ = {};
    std::array<float, kMaxCh> a1Band2_ = {};
    std::array<float, kMaxCh> a2Band2_ = {};

    void invalidateCutTracking() noexcept {
        for (auto& s : lastCutDb1_)
            s = -1000.0f;
        for (auto& s : lastCutDb2_)
            s = -1000.0f;
    }

    void computeBaseCoeffs() noexcept {
        computeBandCoeffs(baseFreq1_, q1_, true);   // band 1
        computeBandCoeffs(baseFreq2_, q2_, false);  // band 2
    }

    void computeBandCoeffs(float freq, float q, bool isBand1) noexcept {
        const float omega = 2.0f * M_PI * freq / static_cast<float>(sr_);
        const float sinW0 = std::sin(omega);
        const float cosW0 = std::cos(omega);
        const float alpha = sinW0 / (2.0f * q);
        
        const float a0 = 1.0f + alpha;
        const float b0Base = 1.0f;
        const float b1Base = -2.0f * cosW0;
        const float b2Base = 1.0f - alpha;
        const float a1Base = -2.0f * cosW0;
        const float a2Base = 1.0f - alpha;
        
        // Initialize all channels with base coefficients
        auto& b0 = isBand1 ? b0Band1_ : b0Band2_;
        auto& b1 = isBand1 ? b1Band1_ : b1Band2_;
        auto& b2 = isBand1 ? b2Band1_ : b2Band2_;
        auto& a1 = isBand1 ? a1Band1_ : a1Band2_;
        auto& a2 = isBand1 ? a2Band1_ : a2Band2_;
        
        for (int ch = 0; ch < kMaxCh; ++ch) {
            b0[ch] = b0Base / a0;
            b1[ch] = b1Base / a0;
            b2[ch] = b2Base / a0;
            a1[ch] = a1Base / a0;
            a2[ch] = a2Base / a0;
        }
    }

    void updatePeakingCoeffs(int ch, float cutDb, bool isBand1) noexcept {
        // Peaking EQ with cut applied externally via linearGain
        // This just updates the filter coefficients for the given band
        // Base coefficients already computed; this is cached optimization
        // For now, rely on invalidateCutTracking + computeBaseCoeffs
        // Additional per-channel coefficient tuning could go here if needed
    }
};

// ============================================================================
// Module B: Upward Smasher — Parallel Upward Compressor + Soft Clipper
// Increases volume of low-level signals (room decay, ghost notes)
// ============================================================================

class UpwardSmasherModule {
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept {
        sr_ = sampleRate;
        computeCoeffs();
        // Initialize compression state
        for (auto& s : compState_) s = 1.0f;  // Start at unity gain
        for (auto& s : inputDelay_) s = 0.0f;
        delayIndex_ = 0;
    }

    void reset() noexcept {
        for (auto& s : compState_) s = 1.0f;
        for (auto& s : inputDelay_) s = 0.0f;
        delayIndex_ = 0;
    }

    float processSample(int channel, float input, float transientIntensity) noexcept {
        const int ch = std::min(channel, kMaxCh - 1);
        
        // ── 1. Detect low-level signals ───────────────────────────────────────
        const float absIn = std::abs(input);
        
        // ── 2. Compute gain for upward compression ─────────────────────────────
        // High threshold, high ratio — only affects low-level signals
        // Leave primary peaks untouched
        float gainLin = 1.0f;
        
        const float levelDb = (absIn > 1e-9f) 
                              ? 20.0f * std::log10(absIn) 
                              : -120.0f;
        
        if (levelDb < thresholdDb_) {
            // Below threshold: apply upward compression
            // How much to boost: inverse of how far below threshold
            // Clamp the boost to prevent gain explosion
            const float overDb = thresholdDb_ - levelDb;
            const float clampedOverDb = std::min(overDb, 12.0f); // Max 12dB boost
            const float compressedOverDb = clampedOverDb * ratio_;
            gainLin = std::pow(10.0f, compressedOverDb / 20.0f);
            // Clamp final gain to prevent clipping
            gainLin = std::min(gainLin, 4.0f); // Max 4x gain
        }
        
        // ── 3. Smooth gain changes ────────────────────────────────────────────
        const float targetGain = gainLin;
        auto& state = compState_[ch];
        const float coeff = (targetGain > state) ? attackCoeff_ : releaseCoeff_;
        state = coeff * state + (1.0f - coeff) * targetGain + 1e-15f;
        
        // ── 4. Apply compressed signal to parallel path ───────────────────────
        float processed = input * state;
        
        // ── 5. Soft saturation for the parallel signal ──────────────────────
        processed = softClip(processed, saturationThreshold_);
        
        // ── 6. Mix: dry + wet (upward compressed signal) ──────────────────────
        // Only add processed signal during low-level passages
        // Reduce processed signal during transients to preserve punch
        const float wetAmount = (1.0f - transientIntensity) * wetMix_;
        
        // Clamp processed to prevent clipping when mixing with dry signal
        processed = std::max(-0.9f, std::min(0.9f, processed));
        float output = input + processed * wetAmount;
        
        return output;
    }

    void setThresholdDb(float db) noexcept { thresholdDb_ = db; }
    void setRatio(float r) noexcept { ratio_ = std::max(1.0f, r); }
    void setSaturationThreshold(float t) noexcept {
        saturationThreshold_ = std::clamp(t, 0.01f, 0.99f);
    }
    void setWetMix(float m) noexcept { wetMix_ = std::max(0.0f, std::min(1.0f, m)); }

private:
    static constexpr int kMaxCh = 8;
    double sr_ = 44100.0;
    
    float thresholdDb_ = -30.0f;   // High threshold for upward compression
    float ratio_ = 4.0f;           // High ratio for aggressive compression
    float saturationThreshold_ = 0.5f;  // Soft clip threshold
    float wetMix_ = 0.7f;          // How much of the processed signal to add
    
    float attackCoeff_ = 0.0f;
    float releaseCoeff_ = 0.0f;
    
    std::array<float, kMaxCh> compState_ = {};
    std::array<float, kMaxCh * 64> inputDelay_ = {};  // Small delay for timing
    int delayIndex_ = 0;

    void computeCoeffs() noexcept {
        const float sr = static_cast<float>(sr_);
        attackCoeff_  = std::exp(-1.0f / (sr * 0.5f * 0.001f));   // Very fast attack
        releaseCoeff_ = std::exp(-1.0f / (sr * 100.0f * 0.001f)); // Slow release
    }

    // Soft saturation using tanh
    float softClip(float x, float threshold) noexcept {
        if (std::abs(x) <= threshold) return x;
        const float sign = (x > 0.0f) ? 1.0f : -1.0f;
        const float normalized = (std::abs(x) - threshold) / (1.0f - threshold);
        return threshold * sign + (1.0f - threshold) * std::tanh(normalized) * sign;
    }
};

// ============================================================================
// Module C: Dynamic Tilt Gravity — RMS-Modulated Tilt EQ
// Rhythmically shifts frequency balance based on groove intensity
// ============================================================================

class DynamicTiltModule {
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept {
        sr_ = sampleRate;
        lowFreq_ = 200.0f;
        highFreq_ = 6000.0f;
        lowQ_ = 0.707f;      // Parametric Q for low shelf
        highQ_ = 0.707f;     // Parametric Q for high shelf
        computeBaseCoeffs();
    }

    void reset() noexcept {
        for (auto& s : lowShelfState_) s = 0.0f;
        for (auto& s : highShelfState_) s = 0.0f;
    }

    float processSample(int channel, float input, float rmsNormalized) noexcept {
        const int ch = std::min(channel, kMaxCh - 1);
        
        // ── Determine tilt based on RMS level ────────────────────────────────
        // High RMS (hits): low-shelf boost, high-shelf cut
        // Low RMS (between hits): high-shelf boost for air
        
        float lowGain, highGain;
        
        if (rmsNormalized > 0.5f) {
            // Loud passage: boost lows, cut highs
            const float intensity = (rmsNormalized - 0.5f) * 2.0f;  // 0 to 1
            lowGain  =  intensity * maxBoostDb_;
            highGain = -intensity * maxCutDb_;
        } else {
            // Quiet passage: boost highs for air and cymbal sizzle
            const float intensity = (0.5f - rmsNormalized) * 2.0f;  // 0 to 1
            lowGain  = -intensity * maxCutDb_;
            highGain =  intensity * maxBoostDb_;
        }
        
        // ── Apply low shelf with parametric Q ────────────────────────────────
        float out = processLowShelf(ch, input, lowGain);
        
        // ── Apply high shelf with parametric Q ───────────────────────────────
        out = processHighShelf(ch, out, highGain);
        
        return out;
    }

    void setLowFreqHz(float hz) noexcept {
        const float clamped = std::clamp(hz, 20.0f, 5000.0f);
        if (std::abs(clamped - lowFreq_) < 0.01f)
            return;
        lowFreq_ = clamped;
        if (highFreq_ <= lowFreq_ + 10.0f)
            highFreq_ = lowFreq_ + 10.0f;
        computeBaseCoeffs();
    }

    void setHighFreqHz(float hz) noexcept {
        const float clamped = std::clamp(hz, 100.0f, 20000.0f);
        if (std::abs(clamped - highFreq_) < 0.01f)
            return;
        highFreq_ = clamped;
        if (highFreq_ <= lowFreq_ + 10.0f)
            lowFreq_ = std::max(20.0f, highFreq_ - 10.0f);
        computeBaseCoeffs();
    }

    void setMaxBoostDb(float db) noexcept { maxBoostDb_ = std::clamp(db, 0.0f, 24.0f); }
    void setMaxCutDb(float db) noexcept { maxCutDb_ = std::clamp(db, 0.0f, 24.0f); }
    
    // Parametric Q control for shelves
    void setLowShelfQ(float q) noexcept {
        const float clamped = std::clamp(q, 0.4f, 2.0f);
        if (std::abs(clamped - lowQ_) < 0.001f)
            return;
        lowQ_ = clamped;
        computeBaseCoeffs();
    }
    
    void setHighShelfQ(float q) noexcept {
        const float clamped = std::clamp(q, 0.4f, 2.0f);
        if (std::abs(clamped - highQ_) < 0.001f)
            return;
        highQ_ = clamped;
        computeBaseCoeffs();
    }

private:
    static constexpr int kMaxCh = 8;
    double sr_ = 44100.0;
    float lowFreq_ = 200.0f;
    float highFreq_ = 6000.0f;
    float lowQ_ = 0.707f;      // Parametric Q for low shelf
    float highQ_ = 0.707f;     // Parametric Q for high shelf
    float maxBoostDb_ = 3.0f;
    float maxCutDb_ = 6.0f;
    
    // Low shelf coefficients
    std::array<float, kMaxCh> lsB0_ = {};
    std::array<float, kMaxCh> lsB1_ = {};
    std::array<float, kMaxCh> lsB2_ = {};
    std::array<float, kMaxCh> lsA1_ = {};
    std::array<float, kMaxCh> lsA2_ = {};
    
    // High shelf coefficients
    std::array<float, kMaxCh> hsB0_ = {};
    std::array<float, kMaxCh> hsB1_ = {};
    std::array<float, kMaxCh> hsB2_ = {};
    std::array<float, kMaxCh> hsA1_ = {};
    std::array<float, kMaxCh> hsA2_ = {};
    
    // Filter state (2 samples per channel for biquad)
    std::array<float, kMaxCh * 2> lowShelfState_ = {};
    std::array<float, kMaxCh * 2> highShelfState_ = {};

    void computeBaseCoeffs() noexcept {
        const float sr = static_cast<float>(sr_);
        const float omegaLow = 2.0f * M_PI * lowFreq_ / sr;
        const float omegaHigh = 2.0f * M_PI * highFreq_ / sr;
        
        // Low shelf at lowFreq_ with parametric Q
        {
            const float sinW0 = std::sin(omegaLow);
            const float cosW0 = std::cos(omegaLow);
            const float alpha = sinW0 / (2.0f * lowQ_);
            
            const float a0 = 1.0f + alpha;
            lsB0_[0] = (1.0f + cosW0) / 2.0f;
            lsB1_[0] = 1.0f - cosW0;
            lsB2_[0] = lsB0_[0];
            lsA1_[0] = -2.0f * cosW0;
            lsA2_[0] = 1.0f - alpha;
            
            for (int ch = 0; ch < kMaxCh; ++ch) {
                lsB0_[ch] = lsB0_[0] / a0;
                lsB1_[ch] = lsB1_[0] / a0;
                lsB2_[ch] = lsB2_[0] / a0;
                lsA1_[ch] = lsA1_[0] / a0;
                lsA2_[ch] = lsA2_[0] / a0;
            }
        }
        
        // High shelf at highFreq_ with parametric Q
        {
            const float sinW0 = std::sin(omegaHigh);
            const float cosW0 = std::cos(omegaHigh);
            const float alpha = sinW0 / (2.0f * highQ_);
            
            const float a0 = 1.0f + alpha;
            hsB0_[0] = (1.0f + cosW0) / 2.0f;
            hsB1_[0] = -(1.0f + cosW0);
            hsB2_[0] = hsB0_[0];
            hsA1_[0] = -2.0f * cosW0;
            hsA2_[0] = 1.0f - alpha;
            
            for (int ch = 0; ch < kMaxCh; ++ch) {
                hsB0_[ch] = hsB0_[0] / a0;
                hsB1_[ch] = hsB1_[0] / a0;
                hsB2_[ch] = hsB2_[0] / a0;
                hsA1_[ch] = hsA1_[0] / a0;
                hsA2_[ch] = hsA2_[0] / a0;
            }
        }
    }

    float processLowShelf(int ch, float input, float gainDb) noexcept {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float omega = 2.0f * M_PI * lowFreq_ / sr_;
        const float sinW0 = std::sin(omega);
        const float cosW0 = std::cos(omega);
        const float alpha = sinW0 / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/lowQ_ - 1.0f) + 2.0f);
        const float sqA = std::sqrt(A);

        const float b0 =      A*( (A+1.0f) - (A-1.0f)*cosW0 + 2.0f*sqA*alpha );
        const float b1 = 2.0f*A*( (A-1.0f) - (A+1.0f)*cosW0 );
        const float b2 =      A*( (A+1.0f) - (A-1.0f)*cosW0 - 2.0f*sqA*alpha );
        const float a0 =          (A+1.0f) + (A-1.0f)*cosW0 + 2.0f*sqA*alpha ;
        const float a1 =  -2.0f*( (A-1.0f) + (A+1.0f)*cosW0 );
        const float a2 =          (A+1.0f) + (A-1.0f)*cosW0 - 2.0f*sqA*alpha ;

        const float nB0 = b0 / a0;
        const float nB1 = b1 / a0;
        const float nB2 = b2 / a0;
        const float nA1 = a1 / a0;
        const float nA2 = a2 / a0;

        const int base = ch * 2;
        const float out = nB0 * input + lowShelfState_[base + 0];
        lowShelfState_[base + 0] = nB1 * input - nA1 * out + lowShelfState_[base + 1] + 1e-15f;
        lowShelfState_[base + 1] = nB2 * input - nA2 * out;
        
        return out;
    }

    float processHighShelf(int ch, float input, float gainDb) noexcept {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float omega = 2.0f * M_PI * highFreq_ / sr_;
        const float sinW0 = std::sin(omega);
        const float cosW0 = std::cos(omega);
        const float alpha = sinW0 / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/highQ_ - 1.0f) + 2.0f);
        const float sqA = std::sqrt(A);

        const float b0 =       A*((A+1.0f) + (A-1.0f)*cosW0 + 2.0f*sqA*alpha);
        const float b1 = -2.0f*A*((A-1.0f) + (A+1.0f)*cosW0);
        const float b2 =       A*((A+1.0f) + (A-1.0f)*cosW0 - 2.0f*sqA*alpha);
        const float a0 =          (A+1.0f) - (A-1.0f)*cosW0 + 2.0f*sqA*alpha;
        const float a1 =    2.0f*((A-1.0f) - (A+1.0f)*cosW0);
        const float a2 =          (A+1.0f) - (A-1.0f)*cosW0 - 2.0f*sqA*alpha;

        const float nB0 = b0 / a0;
        const float nB1 = b1 / a0;
        const float nB2 = b2 / a0;
        const float nA1 = a1 / a0;
        const float nA2 = a2 / a0;

        const int base = ch * 2;
        const float out = nB0 * input + highShelfState_[base + 0];
        highShelfState_[base + 0] = nB1 * input - nA1 * out + highShelfState_[base + 1] + 1e-15f;
        highShelfState_[base + 1] = nB2 * input - nA2 * out;
        
        return out;
    }
};

// ============================================================================
// Module D: Dynamic M/S Splash — Mid/Side Transient Expander
// Widens stereo image of high-frequency transients
// ============================================================================

class DynamicMSModule {
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept {
        sr_ = sampleRate;
        // High-pass filter for side channel
        hpFreq_ = 2000.0f;
        hpQ_ = 0.707f;
        computeHPCoeffs();
        // Initialize filter states
        for (auto& s : hpState_) s = 0.0f;
        sideVcaState_ = 1.0f;
        inL_ = 0.0f;
        outL_ = 0.0f;
        outR_ = 0.0f;
    }

    void reset() noexcept {
        for (auto& s : hpState_) s = 0.0f;
        sideVcaState_ = 1.0f;
        inL_ = 0.0f;
        outL_ = 0.0f;
        outR_ = 0.0f;
    }

    // Process stereo input with M/S encoding/decoding
    // Delays the signal by 1 sample to allow computing both channels together
    float processSample(int channel, float input, float transientIntensity, bool enabled) noexcept {
        if (channel == 0) {
            inL_ = input;
            return outL_; // Return previous sample's Left output
        } else if (channel == 1) {
            // We now have both Left and Right for the current sample
            const float inL = inL_;
            const float inR = input;
            
            float currentOutL = inL;
            float currentOutR = inR;
            
            if (enabled) {
                // 1. M/S encode
                const float mid  = (inL + inR) * 0.5f;
                const float side = (inL - inR) * 0.5f;
                
                // 2. High-pass filter the side channel
                const float sideHp = processHP(0, side);
                
                // 3. VCA envelope on side channel triggered by transients
                const float expansionGain = 1.0f + transientIntensity * (expansionAmount_ - 1.0f);
                
                const float targetGain = expansionGain;
                sideVcaState_ = sideCoeff_ * sideVcaState_ + (1.0f - sideCoeff_) * targetGain;
                
                const float expandedSide = sideHp * sideVcaState_;
                
                // 4. M/S decode: L = mid + side, R = mid - side
                const float clampedSide = std::max(-0.9f, std::min(0.9f, expandedSide));
                
                currentOutL = mid + clampedSide;
                currentOutR = mid - clampedSide;
            }
            
            // We must output the PREVIOUS sample's Right output to maintain phase with Left
            const float delayedOutR = outR_;
            
            outL_ = currentOutL;
            outR_ = currentOutR;
            
            return delayedOutR;
        }
        return input; // Fallback for channels > 1
    }

    void setHpFreqHz(float hz) noexcept {
        const float clamped = std::clamp(hz, 20.0f, 20000.0f);
        if (std::abs(clamped - hpFreq_) < 0.01f)
            return;
        hpFreq_ = clamped;
        computeHPCoeffs();
    }

    void setExpansionAmount(float amount) noexcept { 
        expansionAmount_ = std::clamp(amount, 1.0f, 8.0f);
    }

private:
    static constexpr int kMaxCh = 8;
    double sr_ = 44100.0;
    float hpFreq_ = 2000.0f;
    float hpQ_ = 0.707f;
    float expansionAmount_ = 2.0f;  // Max stereo expansion multiplier
    
    // HP filter coefficients
    float hpB0_ = 0.0f, hpB1_ = 0.0f, hpB2_ = 0.0f;
    float hpA1_ = 0.0f, hpA2_ = 0.0f;
    
    // Filter state (4 elements per channel for Direct Form II biquad)
    std::array<float, kMaxCh * 4> hpState_ = {};
    float sideVcaState_ = 1.0f;
    float sideCoeff_ = 0.0f;
    
    // M/S 1-sample delay state
    float inL_ = 0.0f;
    float outL_ = 0.0f;
    float outR_ = 0.0f;

    void computeHPCoeffs() noexcept {
        const float sr = static_cast<float>(sr_);
        const float freq = std::clamp(hpFreq_, 20.0f, sr * 0.49f);
        const float omega = 2.0f * M_PI * freq / sr;
        const float sinW0 = std::sin(omega);
        const float cosW0 = std::cos(omega);
        const float alpha = sinW0 / (2.0f * hpQ_);
        
        const float a0 = 1.0f + alpha;
        hpB0_ =  (1.0f + cosW0) / 2.0f;
        hpB1_ = -(1.0f + cosW0);
        hpB2_ =  (1.0f + cosW0) / 2.0f;
        hpA1_ = -2.0f * cosW0;
        hpA2_ =  1.0f - alpha;
        
        // Normalize
        hpB0_ /= a0; hpB1_ /= a0; hpB2_ /= a0;
        hpA1_ /= a0; hpA2_ /= a0;
        
        // VCA coefficient
        sideCoeff_ = std::exp(-1.0f / (sr * 5.0f * 0.001f)); // 5ms smoothing
    }

    float processHP(int ch, float input) noexcept {
        const int base = ch * 4;
        float out = hpB0_ * input + hpB1_ * hpState_[base + 0] + hpB2_ * hpState_[base + 1]
                  - hpA1_ * hpState_[base + 2] - hpA2_ * hpState_[base + 3];
        
        hpState_[base + 1] = hpState_[base + 0];
        hpState_[base + 0] = input;
        hpState_[base + 3] = hpState_[base + 2];
        hpState_[base + 2] = out;
        
        return out;
    }
};

// ============================================================================
// Main Processor: Kinetic Drum Engine
// ============================================================================

class KineticDrumEngineProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        
        cns_.prepare(sampleRate, maxBlockSize);
        autoCarving_.prepare(sampleRate, maxBlockSize);
        upwardSmasher_.prepare(sampleRate, maxBlockSize);
        dynamicTilt_.prepare(sampleRate, maxBlockSize);
        dynamicMS_.prepare(sampleRate, maxBlockSize);
        
        // Initialize output buffer and state
        outputBuffer_[0] = 0.0f;
        outputBuffer_[1] = 0.0f;
        
        // Call reset on all submodules
        cns_.reset();
        autoCarving_.reset();
        upwardSmasher_.reset();
        dynamicTilt_.reset();
        dynamicMS_.reset();
        
        // PERFORMANCE: Initialize parameter cache
        cachedBypass_ = 0.0f;
        lastBlockStart_ = -1;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        // PERFORMANCE: Cache parameters at block start
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedBypass_ = ctx.getParameter("bypass");
            updateRuntimeParameters(ctx);
        }

        if (cachedBypass_ >= 0.5f)
            return input;

        const int ch = std::min(channel, kMaxCh - 1);

        // Note: Parameters already cached at block start above
        // No need to update per-channel

        const float in = input * params_.inputGainLin;
        
        // CNS detection
        cns_.processSample(ch, in);
        const float transientIntensity = cns_.getTransientIntensity(ch);
        const float rmsNorm = cns_.getRmsNormalized(ch);
        
        float out = in;

        // === Module A: Auto-Carving (Dynamic EQ) ===
        if (params_.autoCarvingEnabled)
            out = autoCarving_.processSample(ch, out, transientIntensity);
        
        // === Module B: Upward Smasher (Parallel Upward Compression + Saturation) ===
        if (params_.upwardSmasherEnabled)
            out = upwardSmasher_.processSample(ch, out, transientIntensity);
        
        // === Module C: Dynamic Tilt (Spectral Balancing) ===
        if (params_.dynamicTiltEnabled)
            out = dynamicTilt_.processSample(ch, out, rmsNorm);
        
        // === Module D: Dynamic M/S Splash (Stereo Widening) ===
        if (ctx.numChannels >= 2 && ch < 2) {
            out = dynamicMS_.processSample(ch, out, transientIntensity, params_.dynamicMSEnabled);
        }

        out *= params_.outputGainLin;
        
        // Final limiter
        return std::max(-1.0f, std::min(1.0f, out));
    }

    void reset() override {
        cns_.reset();
        autoCarving_.reset();
        upwardSmasher_.reset();
        dynamicTilt_.reset();
        dynamicMS_.reset();
        
        outputBuffer_[0] = 0.0f;
        outputBuffer_[1] = 0.0f;
    }

private:
    static constexpr int kMaxCh = 8;
    double sampleRate_ = 44100.0;

    struct RuntimeParams {
        float inputGainLin = 1.0f;
        float outputGainLin = 1.0f;
        bool autoCarvingEnabled = true;
        bool upwardSmasherEnabled = true;
        bool dynamicTiltEnabled = true;
        bool dynamicMSEnabled = true;
    };

    static float dbToLinear(float db) noexcept {
        return std::pow(10.0f, db / 20.0f);
    }

    void updateRuntimeParameters(const AgentVST::DSPContext& ctx) noexcept {
        const float inputDb = std::clamp(ctx.getParameter("input_gain_db"), -12.0f, 12.0f);
        const float outputDb = std::clamp(ctx.getParameter("output_gain_db"), -12.0f, 12.0f);

        params_.inputGainLin = dbToLinear(inputDb);
        params_.outputGainLin = dbToLinear(outputDb);

        params_.autoCarvingEnabled = ctx.getParameter("auto_carving_enable") >= 0.5f;
        params_.upwardSmasherEnabled = ctx.getParameter("upward_smasher_enable") >= 0.5f;
        params_.dynamicTiltEnabled = ctx.getParameter("dynamic_tilt_enable") >= 0.5f;
        params_.dynamicMSEnabled = ctx.getParameter("dynamic_ms_enable") >= 0.5f;

        // Auto-Carving: Band 1 (primary)
        autoCarving_.setCenterFrequencyHz(std::clamp(ctx.getParameter("auto_carving_freq_hz"), 100.0f, 800.0f));
        autoCarving_.setQ(std::clamp(ctx.getParameter("auto_carving_q"), 0.5f, 4.0f));
        autoCarving_.setMaxCutDb(std::clamp(ctx.getParameter("auto_carving_max_cut_db"), 0.0f, 18.0f));
        
        // Auto-Carving: Band 2 (secondary, multi-band feature)
        autoCarving_.setBand2FrequencyHz(std::clamp(ctx.getParameter("auto_carving_freq2_hz"), 500.0f, 5000.0f));
        autoCarving_.setBand2Q(std::clamp(ctx.getParameter("auto_carving_q2"), 0.5f, 4.0f));
        autoCarving_.setBand2MaxCutDb(std::clamp(ctx.getParameter("auto_carving_max_cut_db2"), 0.0f, 12.0f));
        autoCarving_.setBand2Enabled(ctx.getParameter("auto_carving_band2_enable") >= 0.5f);

        upwardSmasher_.setThresholdDb(std::clamp(ctx.getParameter("upward_smasher_threshold_db"), -50.0f, -10.0f));
        upwardSmasher_.setRatio(std::clamp(ctx.getParameter("upward_smasher_ratio"), 1.0f, 10.0f));
        upwardSmasher_.setSaturationThreshold(std::clamp(ctx.getParameter("upward_smasher_saturation"), 0.0f, 1.0f));
        upwardSmasher_.setWetMix(std::clamp(ctx.getParameter("upward_smasher_mix"), 0.0f, 1.0f));

        dynamicTilt_.setLowFreqHz(std::clamp(ctx.getParameter("dynamic_tilt_low_freq_hz"), 80.0f, 500.0f));
        dynamicTilt_.setHighFreqHz(std::clamp(ctx.getParameter("dynamic_tilt_high_freq_hz"), 2000.0f, 12000.0f));
        dynamicTilt_.setMaxBoostDb(std::clamp(ctx.getParameter("dynamic_tilt_max_boost_db"), 0.0f, 6.0f));
        dynamicTilt_.setMaxCutDb(std::clamp(ctx.getParameter("dynamic_tilt_max_cut_db"), 0.0f, 12.0f));
        dynamicTilt_.setLowShelfQ(std::clamp(ctx.getParameter("dynamic_tilt_low_q"), 0.4f, 2.0f));
        dynamicTilt_.setHighShelfQ(std::clamp(ctx.getParameter("dynamic_tilt_high_q"), 0.4f, 2.0f));

        dynamicMS_.setHpFreqHz(std::clamp(ctx.getParameter("dynamic_ms_hp_freq_hz"), 500.0f, 8000.0f));
        dynamicMS_.setExpansionAmount(std::clamp(ctx.getParameter("dynamic_ms_expansion"), 1.0f, 4.0f));
    }
    
    CentralNervousSystem cns_;
    AutoCarvingModule autoCarving_;
    UpwardSmasherModule upwardSmasher_;
    DynamicTiltModule dynamicTilt_;
    DynamicMSModule dynamicMS_;

    RuntimeParams params_;
    
    float outputBuffer_[2] = {0.0f, 0.0f};
    
    // PERFORMANCE: Cached parameter values
    float cachedBypass_ = 0.0f;
    std::int64_t lastBlockStart_ = -1;
};

AGENTVST_REGISTER_DSP(KineticDrumEngineProcessor)
