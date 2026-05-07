#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class GirihAsymmetricalDelayProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxDelaySamples_ = static_cast<int>(sampleRate_ * 3.0); // 3 seconds max (reduced for CPU/memory)
        for (int ch = 0; ch < 2; ++ch) {
            delayBuffer_[ch].resize(maxDelaySamples_, 0.0f);
            writePos_[ch] = 0;
            lastFeedback_[ch] = 0.0f;
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        // Cache parameters once per block (ctx.currentSample changes at block boundary)
        if (ctx.currentSample != lastBlockStart_) {
            lastBlockStart_ = ctx.currentSample;
            float rootMs = ctx.getParameter("tessellation_root");
            float nFold = ctx.getParameter("n_fold_geometry"); // 5, 10, or 12
            int taps = static_cast<int>(ctx.getParameter("tile_complexity"));
            cachedTaps_ = std::max(1, std::min(taps, 16)); // clamp taps to reasonable max
            cachedFeedback_ = ctx.getParameter("feedback") / 100.0f;
            cachedMix_ = ctx.getParameter("mix") / 100.0f;
            cachedRootSamples_ = (rootMs / 1000.0f) * static_cast<float>(sampleRate_);

            // determine fold
            if (nFold >= 11.0f) cachedFold_ = 12;
            else if (nFold >= 7.5f) cachedFold_ = 10;
            else cachedFold_ = 5;

            // Precompute small power table for incremental multipliers to avoid per-sample pow()
            cachedMultiplier5_ = (1.618033988749895f - 1.0f);
            cachedMultiplier10_ = cachedMultiplier5_;
            cachedMultiplier12_ = (1.7320508f - 1.0f);
        }

        float out = 0.0f;
        float currentDelaySamples = cachedRootSamples_;

        // attenuation via iterative multiply to avoid pow
        float tapAtt = 1.0f;
        const float decay = 0.8f;

        for (int t = 0; t < cachedTaps_; ++t) {
            float increment = cachedRootSamples_;
            if (cachedFold_ == 5) {
                int r = t % 3;
                if (r == 0) increment = cachedRootSamples_ * 1.0f;
                else if (r == 1) increment = cachedRootSamples_ * cachedMultiplier5_;
                else increment = cachedRootSamples_ * (cachedMultiplier5_ * cachedMultiplier5_);
            } else if (cachedFold_ == 10) {
                int r = t % 4;
                if (r == 0) increment = cachedRootSamples_;
                else if (r == 1) increment = cachedRootSamples_ * cachedMultiplier10_;
                else if (r == 2) increment = cachedRootSamples_ * (cachedMultiplier10_ * cachedMultiplier10_);
                else increment = cachedRootSamples_ * (cachedMultiplier10_ * cachedMultiplier10_ * cachedMultiplier10_);
            } else {
                int r = t % 2;
                if (r == 0) increment = cachedRootSamples_;
                else increment = cachedRootSamples_ * cachedMultiplier12_;
            }

            currentDelaySamples += increment;
            if (currentDelaySamples >= maxDelaySamples_) currentDelaySamples = maxDelaySamples_ - 1;

            int readPos = writePos_[channel] - static_cast<int>(currentDelaySamples);
            if (readPos < 0) readPos += maxDelaySamples_;

            out += delayBuffer_[channel][readPos] * tapAtt;
            tapAtt *= decay;
        }

        // Write to buffer with feedback from the accumulated taps
        float toBuffer = input + out * cachedFeedback_ * 0.5f;
        toBuffer = std::tanh(toBuffer);

        delayBuffer_[channel][writePos_[channel]] = toBuffer;

        if (++writePos_[channel] >= maxDelaySamples_) writePos_[channel] = 0;

        return input * (1.0f - cachedMix_) + out * cachedMix_;
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            std::fill(delayBuffer_[ch].begin(), delayBuffer_[ch].end(), 0.0f);
            writePos_[ch] = 0;
            lastFeedback_[ch] = 0.0f;
        }
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<float> delayBuffer_[2];
    int writePos_[2] = {0, 0};
    int maxDelaySamples_ = 44100 * 3;
    float lastFeedback_[2] = {0.0f, 0.0f};

    // Block-level cache to avoid per-sample parameter lookups
    std::int64_t lastBlockStart_ = -1;
    int cachedTaps_ = 1;
    float cachedFeedback_ = 0.0f;
    float cachedMix_ = 0.0f;
    float cachedRootSamples_ = 0.0f;
    int cachedFold_ = 5;
    float cachedMultiplier5_ = 0.0f;
    float cachedMultiplier10_ = 0.0f;
    float cachedMultiplier12_ = 0.0f;
};

AGENTVST_REGISTER_DSP(GirihAsymmetricalDelayProcessor)
