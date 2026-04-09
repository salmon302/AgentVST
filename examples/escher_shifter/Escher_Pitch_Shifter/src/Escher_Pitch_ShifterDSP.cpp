/**
 * Generated DSP scaffold.
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
        // 1.5 seconds of delay buffer per voice
        bufferSize_ = static_cast<int>(sampleRate * 1.5);
        
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            voices_[ch].resize(kMaxVoices);
            for (auto& voice : voices_[ch]) {
                voice.delayBuffer.assign(bufferSize_, 0.0f);
                voice.writePos = 0;
            }
        }
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= kMaxChannels) return input;

        // 1. Read parameters (real-time safe)
        float rate = ctx.getParameter("rate"); // semitones/sec
        float blend = ctx.getParameter("tritone_blend");
        float feedback = ctx.getParameter("feedback");
        int spread = std::clamp((int)ctx.getParameter("octave_spread"), 1, kMaxVoices);

        // 2. Update shared phase (only on channel 0)
        if (channel == 0 && channel0Processed_ != ctx.currentSample) {
            double phaseInc = (double(rate) / 12.0) / sampleRate_;
            phase_ += phaseInc;
            if (phase_ >= 1.0) phase_ -= 1.0;
            else if (phase_ < 0.0) phase_ += 1.0;
            channel0Processed_ = ctx.currentSample;
        }

        float output = 0.0f;
        float drySignal = input;

        // 3. Process Shepard voices
        for (int i = 0; i < spread; ++i) {
            auto& voice = voices_[channel][i];
            
            // Octave-offset phase (0..1)
            double voicePhase = phase_ + (double(i) / spread);
            while (voicePhase >= 1.0) voicePhase -= 1.0;
            while (voicePhase < 0.0) voicePhase += 1.0;
            
            // Shepard weight (cosine bell)
            float weight = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * (float)voicePhase));

            // --- Dual-Delay Crossfade Pitch Shifter (Granular-like) ---
            // Two overlapping delay ramps to avoid clicking
            const float maxDelayMs = 100.0f;
            const float windowSize = static_cast<float>(sampleRate_ * (maxDelayMs / 1000.0f));
            
            // Ramp 1 (0 to windowSize)
            float ramp1 = std::max(1.0f, static_cast<float>(voicePhase) * windowSize);
            // Ramp 2 (offset by half window)
            float ramp2 = std::max(1.0f, std::fmod(ramp1 + (windowSize * 0.5f), windowSize));
            
            // Linear crossfade between the two ramps to hide the "jump"
            float xfade = std::abs((ramp1 / windowSize) - 0.5f) * 2.0f; // 1 at edges, 0 at middle

            auto getSample = [&](float delaySamples) {
                // writePos points to the next slot to be written, so read from
                // one-sample history to avoid reading stale or uninitialized data.
                float readPos = static_cast<float>(voice.writePos) - 1.0f - delaySamples;
                while (readPos < 0) readPos += (float)bufferSize_;
                while (readPos >= (float)bufferSize_) readPos -= (float)bufferSize_;
                int i0 = (int)readPos;
                int i1 = (i0 + 1) % bufferSize_;
                float f = readPos - (float)i0;
                return voice.delayBuffer[i0] * (1.0f - f) + voice.delayBuffer[i1] * f;
            };

            float s1 = getSample(ramp1);
            float s2 = getSample(ramp2);
            float voiceOutput = (s1 * (1.0f - xfade)) + (s2 * xfade);

            output += voiceOutput * weight;

            // Write to buffer with feedback
            voice.delayBuffer[voice.writePos] = input + voiceOutput * feedback;
            voice.writePos = (voice.writePos + 1) % bufferSize_;
        }

        // Keep output level stable as spread increases.
        output *= (2.0f / static_cast<float>(spread));

        // 4. Global Dry/Wet Mix
        return (drySignal * (1.0f - blend)) + (output * blend);
    }

    void reset() override {
        phase_ = 0.0;
        channel0Processed_ = -1;
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            for (auto& voice : voices_[ch]) {
                std::fill(voice.delayBuffer.begin(), voice.delayBuffer.end(), 0.0f);
                voice.writePos = 0;
            }
        }
    }

private:
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxVoices = 4;
    
    struct Voice {
        std::vector<float> delayBuffer;
        int writePos = 0;
    };

    std::vector<Voice> voices_[kMaxChannels];
    int bufferSize_ = 0;
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    int channel0Processed_ = -1;
};

AGENTVST_REGISTER_DSP(Escher_Pitch_ShifterProcessor)

