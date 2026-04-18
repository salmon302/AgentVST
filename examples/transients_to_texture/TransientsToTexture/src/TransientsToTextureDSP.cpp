#include <AgentDSP.h>
#include <cmath>
#include <cstdlib>

class TransientsToTextureProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        for (int ch = 0; ch < 2; ++ch) {
            ch_state[ch].peak_env = 0.0f;
            ch_state[ch].slow_env = 0.0f;
            ch_state[ch].texture_env = 0.0f;
            ch_state[ch].texture_phase = 0.0f;
            ch_state[ch].last_out = 0.0f;
            ch_state[ch].trigger_holdoff = 0;
        }

        // attack 1ms, release 10ms for fast
        a1_ = 1.0f - std::exp(-1.0f / (0.001f * sampleRate_));
        r1_ = 1.0f - std::exp(-1.0f / (0.010f * sampleRate_));
        
        // attack 10ms, release 50ms for slow
        a2_ = 1.0f - std::exp(-1.0f / (0.010f * sampleRate_));
        r2_ = 1.0f - std::exp(-1.0f / (0.050f * sampleRate_));
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;
        
        float thresh_db = ctx.getParameter("transient_threshold");
        int palette = static_cast<int>(ctx.getParameter("texture_palette"));
        float decay_ms = ctx.getParameter("decay_envelope");
        float mix = ctx.getParameter("texture_mix") / 100.0f;

        float thresh_mag = std::pow(10.0f, thresh_db / 20.0f);
        float decay_coeff = std::exp(-1.0f / (decay_ms * 0.001f * sampleRate_));

        auto& state = ch_state[channel];
        float env_in = std::abs(input);

        // Fast envelope
        if (env_in > state.peak_env) state.peak_env += a1_ * (env_in - state.peak_env);
        else state.peak_env += r1_ * (env_in - state.peak_env);

        // Slow envelope
        if (env_in > state.slow_env) state.slow_env += a2_ * (env_in - state.slow_env);
        else state.slow_env += r2_ * (env_in - state.slow_env);

        if (state.trigger_holdoff > 0) {
            state.trigger_holdoff--;
        }

        // Transient detection: Peak jumps significantly above slow average and passes threshold
        bool is_transient = (state.peak_env > state.slow_env * 2.5f) && (state.peak_env > thresh_mag);
        
        if (is_transient && state.trigger_holdoff <= 0) {
            state.texture_env = 1.0f;
            state.trigger_holdoff = static_cast<int>(0.080f * sampleRate_); // 80ms holdoff
        }

        // Apply decay
        float env_out = state.texture_env;
        state.texture_env *= decay_coeff;

        // Ensure duck is smooth: mapping it to 1.0 - texture_env
        float duck = 1.0f - env_out;
        
        float noise = ((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        float texture = 0.0f;

        if (palette == 0) { // Crackle
            if (((float)std::rand() / (float)RAND_MAX) < 0.03f) {
                texture = noise * 2.0f;
            }
        }
        else if (palette == 1) { // Scrape (Highpassed noise)
            state.last_out += 0.5f * (noise - state.last_out);
            texture = (noise - state.last_out) * 1.5f;
        }
        else if (palette == 2) { // Wind (Lowpassed noise)
            state.last_out += 0.01f * (noise - state.last_out);
            texture = state.last_out * 6.0f;
        }
        else if (palette == 3) { // Digital Glitch (Sample and hold noise)
            state.texture_phase += 0.05f; // S&H clock
            if (state.texture_phase > 1.0f) {
                state.texture_phase -= 1.0f;
                state.last_out = noise;
            }
            texture = state.last_out * 1.5f;
        }
        
        // Clip texture slightly to avoid blowing up
        if (texture > 1.0f) texture = 1.0f;
        if (texture < -1.0f) texture = -1.0f;

        float dry_portion = input * (1.0f - mix) + input * duck * mix;
        float wet_portion = texture * env_out * mix;

        return dry_portion + wet_portion;
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            ch_state[ch].peak_env = 0.0f;
            ch_state[ch].slow_env = 0.0f;
            ch_state[ch].texture_env = 0.0f;
            ch_state[ch].texture_phase = 0.0f;
            ch_state[ch].last_out = 0.0f;
            ch_state[ch].trigger_holdoff = 0;
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
        float texture_env;
        float texture_phase;
        float last_out;
        int trigger_holdoff;
    };
    ChannelState ch_state[2];
};

AGENTVST_REGISTER_DSP(TransientsToTextureProcessor)
