/**
 * Material Decay Resonator DSP
 *
 * Implements comb-filter/waveguide based physical modeling. The "rot" parameter dynamically
 * alters the damping coefficients matching the amplitude envelope, and injects structural 
 * failure (noise) as it degrades.
 */
#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

class Material_Decay_ResonatorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        for (int ch = 0; ch < kMaxChannels; ch++) {
            // Allocate delays (e.g. up to 100ms for resonator)
            delayBuffer_[ch].assign(static_cast<size_t>(sampleRate_ * 0.1), 0.0f);
            writePos_[ch] = 0;
            envSlow_[ch] = 0.0f;
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= kMaxChannels) return input;

        // 1. Parameters
        int materialType = static_cast<int>(ctx.getParameter("material_type"));
        float decayRate = ctx.getParameter("decay_rate");
        float structFailure = ctx.getParameter("structural_failure");
        float mix = ctx.getParameter("mix");

        // 2. Track envelope to simulate "Rot" degradation over time
        float absIn = std::abs(input);
        // Attack is immediate, release is controlled by decay_rate (converted to coefficient)
        float releaseCoef = std::exp(-1.0f / (decayRate * sampleRate_));
        if (absIn > envSlow_[channel]) {
             envSlow_[channel] = absIn;
        } else {
             envSlow_[channel] = envSlow_[channel] * releaseCoef;
        }

        // Degradation state (1.0 = pristine, 0.0 = fully rotted)
        float rotFactor = std::clamp(envSlow_[channel] * 2.0f, 0.0f, 1.0f);

        // 3. Define Base Resonator Properties by Material
        float baseDelayMs_ = 10.0f;
        float baseDamping_ = 0.5f;
        float baseFeedback_ = 0.95f;

        switch (materialType) {
            case 0: // Glass
                baseDelayMs_ = 3.0f;
                baseDamping_ = 0.1f;
                baseFeedback_ = 0.99f;
                break;
            case 1: // Sheet Metal
                baseDelayMs_ = 15.0f;
                baseDamping_ = 0.3f;
                baseFeedback_ = 0.97f;
                break;
            case 2: // Wood
                baseDelayMs_ = 25.0f;
                baseDamping_ = 0.8f;
                baseFeedback_ = 0.80f;
                break;
        }

        // Apply Rot: as rotFactor decreases, damping increases and feedback decreases
        float activeDamping = baseDamping_ + ((1.0f - rotFactor) * (1.0f - baseDamping_));
        float activeFeedback = baseFeedback_ * rotFactor;

        // 4. Delay Line processing
        float delaySamples = (baseDelayMs_ / 1000.0f) * sampleRate_;
        int size = static_cast<int>(delayBuffer_[channel].size());

        float readPos = static_cast<float>(writePos_[channel]) - delaySamples;
        while (readPos < 0) readPos += size;
        int i0 = static_cast<int>(readPos);
        int i1 = (i0 + 1) % size;
        float frac = readPos - static_cast<float>(i0);
        
        float delayed = delayBuffer_[channel][i0] * (1.0f - frac) + delayBuffer_[channel][i1] * frac;

        // Apply low pass (Damping)
        delayed = (delayed * (1.0f - activeDamping)) + (lastFilter_[channel] * activeDamping);
        lastFilter_[channel] = delayed;

        // Inject Structural Failure (crackle/noise) as it rots
        if (structFailure > 0.0f && rotFactor < 0.5f) {
            float noise = dis_(gen_);
            // Noise increases as rotFactor decreases
            float crackleAmount = (0.5f - rotFactor) * 2.0f * structFailure;
            delayed += noise * crackleAmount;
        }

        // Write Feedback
        delayBuffer_[channel][writePos_[channel]] = input + (delayed * activeFeedback);
        writePos_[channel] = (writePos_[channel] + 1) % size;

        // 5. Mix
        return (input * (1.0f - mix)) + (delayed * mix);
    }

    void reset() override {
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(delayBuffer_[ch].begin(), delayBuffer_[ch].end(), 0.0f);
            writePos_[ch] = 0;
            envSlow_[ch] = 0.0f;
            lastFilter_[ch] = 0.0f;
        }
    }

private:
    static constexpr int kMaxChannels = 2;
    std::vector<float> delayBuffer_[kMaxChannels];
    float envSlow_[kMaxChannels] = {0.0f, 0.0f};
    float lastFilter_[kMaxChannels] = {0.0f, 0.0f};
    int writePos_[kMaxChannels] = {0, 0};
    double sampleRate_ = 44100.0;
    
    std::random_device rd_;
    std::mt19937 gen_{rd_()};
    std::uniform_real_distribution<float> dis_{-1.0f, 1.0f};
};

AGENTVST_REGISTER_DSP(Material_Decay_ResonatorProcessor)