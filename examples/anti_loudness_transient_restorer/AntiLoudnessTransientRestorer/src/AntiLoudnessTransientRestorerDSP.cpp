#include <AgentDSP.h>
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
        
        float sledge_db = ctx.getParameter("sledgehammer_gain");
        float attack_ms = ctx.getParameter("attack_ms");
        float tail_db = ctx.getParameter("tail_crushing");

        float attack_coeff = std::exp(-1.0f / (attack_ms * 0.001f * sampleRate_));

        auto& state = ch_state[channel];
        float env_in = std::abs(input);

        // Fast envelope
        if (env_in > state.peak_env) state.peak_env += a1_ * (env_in - state.peak_env);
        else state.peak_env += r1_ * (env_in - state.peak_env);

        // Slow envelope
        if (env_in > state.slow_env) state.slow_env += a2_ * (env_in - state.slow_env);
        else state.slow_env += r2_ * (env_in - state.slow_env);

        bool is_transient = (state.peak_env > state.slow_env * 2.0f);
        
        if (is_transient) {
            state.transient_env = 1.0f;
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

    struct ChannelState {
        float peak_env;
        float slow_env;
        float transient_env;
        float tail_env;
    };
    ChannelState ch_state[2];
};

AGENTVST_REGISTER_DSP(AntiLoudnessTransientRestorerProcessor)
