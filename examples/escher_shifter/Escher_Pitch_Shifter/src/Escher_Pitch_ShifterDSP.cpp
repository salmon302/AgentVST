/**
 * Escher Pitch Shifter (The Endless Tritone) DSP
 *
 * Keep processSample real-time safe:
 * - no allocations
 * - no locks
 * - no I/O
 */
#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Escher_Pitch_ShifterProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        bufferSize_ = static_cast<int>(sampleRate * 2.0); // 2 seconds max
        
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            delayBuffer_[ch].assign(bufferSize_, 0.0f);
            writePos_[ch] = 0;
        }
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= kMaxChannels) return input;

        // 1. Read parameters
        float shiftHz = ctx.getParameter("shift_cycle"); // default 0.5 Hz
        float lock = ctx.getParameter("tension_lock");
        float asymmetry = ctx.getParameter("asymmetry");
        float mix = ctx.getParameter("mix");

        // 2. Update phase
        if (channel == 0 && channel0Processed_ != ctx.currentSample) {
            double phaseInc = shiftHz / sampleRate_;
            phase_ += phaseInc;
            if (phase_ >= 1.0) phase_ -= 1.0;
            else if (phase_ < 0.0) phase_ += 1.0;
            channel0Processed_ = ctx.currentSample;
        }

        // Apply tension lock (semitone quantization vs smooth glide)
        float currentPhase = static_cast<float>(phase_); // [0, 1)
        if (lock > 0.5f) {
            // Quantize to 6 steps (tritone)
            currentPhase = std::floor(currentPhase * 6.0f) / 6.0f;
        }

        // Shepard envelope weight with asymmetry
        float weightUp = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * currentPhase));
        float weightDown = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * (1.0f - currentPhase)));

        // Asymmetry skew [-1, 1], < 0 favors fall, > 0 favors rise
        float skewUp = std::clamp(1.0f + asymmetry, 0.0f, 1.0f);
        float skewDown = std::clamp(1.0f - asymmetry, 0.0f, 1.0f);

        weightUp *= skewUp;
        weightDown *= skewDown;
        
        // Normalize weights
        float totalWeight = weightUp + weightDown;
        if (totalWeight > 0.0f) {
            weightUp /= totalWeight;
            weightDown /= totalWeight;
        }

        float output = 0.0f;
        
        // Granular read for upward shifting
        float maxDelaySamples = sampleRate_ * 0.1f; // 100ms window
        
        auto getSample = [&](float delaySamples) {
            float readPos = static_cast<float>(writePos_[channel]) - 1.0f - delaySamples;
            while (readPos < 0) readPos += (float)bufferSize_;
            while (readPos >= (float)bufferSize_) readPos -= (float)bufferSize_;
            int i0 = (int)readPos;
            int i1 = (i0 + 1) % bufferSize_;
            float f = readPos - (float)i0;
            return delayBuffer_[channel][i0] * (1.0f - f) + delayBuffer_[channel][i1] * f;
        };

        // Rising path (delay decreases)
        float ramp1Up = std::max(1.0f, (1.0f - currentPhase) * maxDelaySamples);
        float ramp2Up = std::max(1.0f, std::fmod(ramp1Up + (maxDelaySamples * 0.5f), maxDelaySamples));
        float xfadeUp = std::abs((ramp1Up / maxDelaySamples) - 0.5f) * 2.0f;
        float upOut = (getSample(ramp1Up) * (1.0f - xfadeUp)) + (getSample(ramp2Up) * xfadeUp);

        // Falling path (delay increases)
        float ramp1Down = std::max(1.0f, currentPhase * maxDelaySamples);
        float ramp2Down = std::max(1.0f, std::fmod(ramp1Down + (maxDelaySamples * 0.5f), maxDelaySamples));
        float xfadeDown = std::abs((ramp1Down / maxDelaySamples) - 0.5f) * 2.0f;
        float downOut = (getSample(ramp1Down) * (1.0f - xfadeDown)) + (getSample(ramp2Down) * xfadeDown);

        output = (upOut * weightUp) + (downOut * weightDown);

        // Write to delay buffer
        delayBuffer_[channel][writePos_[channel]] = input;
        writePos_[channel] = (writePos_[channel] + 1) % bufferSize_;

        return (input * (1.0f - mix)) + (output * mix);
    }

    void reset() override {
        phase_ = 0.0;
        channel0Processed_ = -1;
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(delayBuffer_[ch].begin(), delayBuffer_[ch].end(), 0.0f);
            writePos_[ch] = 0;
        }
    }

private:
    static constexpr int kMaxChannels = 2;
    std::vector<float> delayBuffer_[kMaxChannels];
    int writePos_[kMaxChannels] = {0, 0};
    int bufferSize_ = 0;
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    int channel0Processed_ = -1;
};

AGENTVST_REGISTER_DSP(Escher_Pitch_ShifterProcessor)
