#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Dechronicization_EngineDSP : public AgentVST::IAgentDSP {
public:
    Dechronicization_EngineDSP() {
        for (int i = 0; i < kMaxChannels; ++i) {
            delayBuffer_[i].resize(kBufferSize, 0.0f);
        }
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        sampleRate_ = sampleRate;
        writePos_ = 0;
        phase_ = 0.0;
    }

    void processBlock(const std::vector<std::vector<float>>& input, std::vector<std::vector<float>>& output) override {
        // Assume non-interleaved channels
        int numChannels = std::min(input.size(), output.size());
        int numSamples = input.empty() ? 0 : input[0].size();

        for (int s = 0; s < numSamples; ++s) {
            std::vector<float> inSample(numChannels);
            std::vector<float> outSample(numChannels);
            for (int c = 0; c < numChannels; ++c) {
                inSample[c] = input[c][s];
            }
            
            processSample(inSample, outSample);
            
            for (int c = 0; c < numChannels; ++c) {
                output[c][s] = outSample[c];
            }
        }
    }

    void processSample(const std::vector<float>& input, std::vector<float>& output) override {
        int numChannels = std::min(static_cast<int>(input.size()), kMaxChannels);
        
        float preCognitionSamples = std::clamp(preCognitionMs_ * (float)sampleRate_ / 1000.0f, 10.0f, (float)(kBufferSize / 2));
        float conflictRatio = temporalConflict_;
        float ghostMix = ghostMix_;

        // Advance phase to control overlapping grains
        phase_ += 1.0f / preCognitionSamples;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        float phase1 = phase_;
        float phase2 = phase_ + 0.5f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        // Windowing function (Hann) to fade reversing grains in/out seamlessly
        float window1 = 0.5f * (1.0f - std::cos(2.0f * M_PI * phase1));
        float window2 = 0.5f * (1.0f - std::cos(2.0f * M_PI * phase2));

        for (int c = 0; c < numChannels; ++c) {
            // Write live input to the head of the buffer
            delayBuffer_[c][writePos_] = input[c];

            // Dry signal is delayed by precisely `preCognitionSamples`
            int dryReadPos = writePos_ - static_cast<int>(preCognitionSamples);
            if (dryReadPos < 0) dryReadPos += kBufferSize;
            float drySignal = delayBuffer_[c][dryReadPos];

            // Ghost grain read offsets (reversing backward from exactly `writePos_`)
            float readOffset1 = phase1 * preCognitionSamples * conflictRatio;
            float readOffset2 = phase2 * preCognitionSamples * conflictRatio;

            float ghost1 = getInterpolatedSample(c, static_cast<float>(writePos_) - readOffset1);
            float ghost2 = getInterpolatedSample(c, static_cast<float>(writePos_) - readOffset2);

            // Reconstruct the overlapping ghost backward pre-echo
            float ghostSignal = (ghost1 * window1) + (ghost2 * window2);

            // Output the mix
            output[c] = (drySignal * (1.0f - ghostMix)) + (ghostSignal * ghostMix);
        }

        writePos_++;
        if (writePos_ >= kBufferSize) {
            writePos_ = 0;
        }
    }

    void setParameter(const std::string& id, float value) override {
        if (id == "pre_cognition") preCognitionMs_ = value;
        else if (id == "temporal_conflict") temporalConflict_ = value;
        else if (id == "ghost_mix") ghostMix_ = value;
    }

private:
    float getInterpolatedSample(int channel, float targetPos) const {
        // Bounds checking
        while (targetPos < 0) targetPos += kBufferSize;
        while (targetPos >= kBufferSize) targetPos -= kBufferSize;

        int idx1 = static_cast<int>(targetPos);
        int idx2 = (idx1 + 1) % kBufferSize;
        float frac = targetPos - static_cast<float>(idx1);

        return delayBuffer_[channel][idx1] * (1.0f - frac) + delayBuffer_[channel][idx2] * frac;
    }

    static constexpr int kMaxChannels = 2;
    static constexpr int kBufferSize = 44100 * 5; // 5 seconds max (fixed to cover 2000.0ms max param range * max temp ratio)
    
    std::vector<float> delayBuffer_[kMaxChannels];
    int writePos_ = 0;
    double sampleRate_ = 44100.0;
    float phase_ = 0.0f;

    // Default target parameters
    float preCognitionMs_ = 500.0f;
    float temporalConflict_ = 1.0f;
    float ghostMix_ = 0.5f;
};

extern "C" {
#ifdef _WIN32
    __declspec(dllexport)
#else
    __attribute__((visibility("default")))
#endif
    AgentVST::IAgentDSP* createDSP() {
        return new Dechronicization_EngineDSP();
    }
#ifdef _WIN32
    __declspec(dllexport)
#else
    __attribute__((visibility("default")))
#endif
    void destroyDSP(AgentVST::IAgentDSP* dsp) {
        delete dsp;
    }
}
