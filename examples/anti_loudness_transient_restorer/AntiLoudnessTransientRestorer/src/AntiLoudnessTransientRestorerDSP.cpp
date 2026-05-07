/**
 * AntiLoudnessTransientRestorerDSP.cpp
 * Purpose: Restore transient impact with soft-knee transient detection.
 * Author: Seth Nenninger (GPT-5.2-Codex Agent)
 * Timestamp: 2026-05-06T23:55:00Z
 * Changelog: Add transient slope control and tighten dB ranges.
 */
#include <AgentDSP.h>
#include <algorithm>
#include <cmath>

class AntiLoudnessTransientRestorerProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        for (int ch = 0; ch < 2; ++ch) {
            ch_state[ch].peak_env = 0.0f;
            ch_state[ch].slow_env = 0.0f;
            ch_state[ch].transient_env = 0.0f;
            ch_state[ch].tail_env = 0.0f;
        }

        a1_ = 1.0f - std::exp(-1.0f / (0.0005f * sampleRate_)); // 0.5ms fast
        r1_ = 1.0f - std::exp(-1.0f / (0.010f * sampleRate_));  // 10.0ms 
        
        a2_ = 1.0f - std::exp(-1.0f / (0.010f * sampleRate_)); // 10ms slow
        r2_ = 1.0f - std::exp(-1.0f / (0.050f * sampleRate_)); // 50ms slow
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;
        // Cache parameters per block
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            cachedSledgeDb_ = ctx.getParameter("sledgehammer_gain");
            cachedAttackMs_ = ctx.getParameter("attack_ms");
            cachedTailDb_ = ctx.getParameter("tail_crushing");
            cachedSlopeDb_ = ctx.getParameter("transient_slope_db");
            cachedAttackCoeff_ = std::exp(-1.0f / (cachedAttackMs_ * 0.001f * sampleRate_));

            const float slopeDb = std::max(0.1f, cachedSlopeDb_);
            const float halfSlopeDb = 0.5f * slopeDb;
            const float halfSlopeRatio = std::pow(10.0f, halfSlopeDb / 20.0f);
            cachedLowerRatio_ = kTransientRatio / halfSlopeRatio;
            cachedUpperRatio_ = kTransientRatio * halfSlopeRatio;
        }

        float sledge_db = cachedSledgeDb_;
        float attack_ms = cachedAttackMs_;
        float tail_db = cachedTailDb_;

        float attack_coeff = cachedAttackCoeff_;

        auto& state = ch_state[channel];
        float env_in = std::abs(input);

        // Fast envelope
        if (env_in > state.peak_env) state.peak_env += a1_ * (env_in - state.peak_env);
        else state.peak_env += r1_ * (env_in - state.peak_env);

        // Slow envelope
        if (env_in > state.slow_env) state.slow_env += a2_ * (env_in - state.slow_env);
        else state.slow_env += r2_ * (env_in - state.slow_env);

        float ratio = 0.0f;
        if (state.peak_env > 1e-6f) {
            ratio = state.peak_env / std::max(state.slow_env, 1e-6f);
        }

        float transientAmount = 0.0f;
        if (cachedUpperRatio_ > cachedLowerRatio_ + 1e-9f) {
            float x = (ratio - cachedLowerRatio_) / (cachedUpperRatio_ - cachedLowerRatio_);
            x = std::clamp(x, 0.0f, 1.0f);
            transientAmount = x * x * (3.0f - 2.0f * x);
        }

        if (transientAmount > 0.0f) {
            if (transientAmount > state.transient_env)
                state.transient_env = transientAmount;
            state.tail_env = 0.0f; // Reset tail suppression on transient
        }

        // Apply decay to transient mask
        float cur_transient = state.transient_env;
        state.transient_env *= attack_coeff;

        // Tail envelope recovers as transient passes
        if (state.tail_env < 1.0f) state.tail_env += 1.0f - attack_coeff; // Inverse speed
        if (state.tail_env > 1.0f) state.tail_env = 1.0f;

        float sledge_mag = std::pow(10.0f, sledge_db / 20.0f);
        float tail_mag = std::pow(10.0f, tail_db / 20.0f);

        // Interpolate gains based on transient mask vs tail mask
        float effective_gain = (1.0f - cur_transient) * tail_mag + cur_transient * sledge_mag;

        float output = input * effective_gain;

        // Simple hard clip to prevent blowouts
        if (output > 1.0f) output = 1.0f;
        if (output < -1.0f) output = -1.0f;

        return output;
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            ch_state[ch].peak_env = 0.0f;
            ch_state[ch].slow_env = 0.0f;
            ch_state[ch].transient_env = 0.0f;
            ch_state[ch].tail_env = 0.0f;
        }
    }

private:
    double sampleRate_ = 44100.0;
    float a1_ = 0.0f;
    float r1_ = 0.0f;
    float a2_ = 0.0f;
    float r2_ = 0.0f;
    static constexpr float kTransientRatio = 2.0f;

    // Block cache
    int lastBlockStart_ = -1;
    float cachedSledgeDb_ = 0.0f;
    float cachedAttackMs_ = 1.0f;
    float cachedTailDb_ = 0.0f;
    float cachedSlopeDb_ = 6.0f;
    float cachedLowerRatio_ = kTransientRatio;
    float cachedUpperRatio_ = kTransientRatio;
    float cachedAttackCoeff_ = 0.0f;

    struct ChannelState {
        float peak_env;
        float slow_env;
        float transient_env;
        float tail_env;
    };
    ChannelState ch_state[2];
};

AGENTVST_REGISTER_DSP(AntiLoudnessTransientRestorerProcessor)
