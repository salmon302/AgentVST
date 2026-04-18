#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

class HumanErrorEnsembleProcessor : public AgentVST::IAgentDSP {
public:
    static constexpr int MAX_VOICES = 5;
    static constexpr int MAX_DELAY_SAMPLES = 88200; // 2 seconds

    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        for (int ch = 0; ch < 2; ++ch) {
            delayBuffer_[ch].assign(MAX_DELAY_SAMPLES, 0.0f);
            writeIdx_[ch] = 0;
        }

        // Initialize voices
        for (int i = 0; i < MAX_VOICES; ++i) {
            voices_[i].timeOffset = 0.0f;
            voices_[i].targetTimeOffset = 0.0f;
            voices_[i].pitchOffset = 0.0f;
            voices_[i].targetPitchOffset = 0.0f;
            voices_[i].pan = (i % 2 == 0) ? -1.0f : 1.0f;
            if (i == 0) voices_[i].pan = 0.0f;
            voices_[i].readIdx = 0.0f;
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        // Handle parameters
        float fatigue = ctx.getParameter("fatigue_level") / 100.0f;
        int activeVoices = static_cast<int>(ctx.getParameter("ensemble_size"));
        float scatter = ctx.getParameter("stereo_scatter") / 100.0f;

        if (channel >= 2) return input;

        // Process Markov chain changes
        if (channel == 0) {
            updateMarkov(fatigue);
        }

        // Write to delay buffer
        delayBuffer_[channel][writeIdx_[channel]] = input;

        float output = 0.0f;
        float gainPerVoice = 1.0f / static_cast<float>(activeVoices + 1);

        // Dry signal
        output += input * gainPerVoice;

        // Process each voice
        for (int i = 0; i < activeVoices; ++i) {
             float panVal = voices_[i].pan * scatter;
             float chPan = (channel == 0) ? (1.0f - panVal) : (1.0f + panVal);
             chPan = std::max(0.0f, std::min(chPan, 2.0f));

             if (channel == 0) {
                 voices_[i].timeOffset += (voices_[i].targetTimeOffset - voices_[i].timeOffset) * 0.001f;
                 voices_[i].pitchOffset += (voices_[i].targetPitchOffset - voices_[i].pitchOffset) * 0.001f;
             }

             float offsetSamples = voices_[i].timeOffset * sampleRate_;
             float readPos = static_cast<float>(writeIdx_[channel]) - offsetSamples;
             if (readPos < 0.0f) readPos += static_cast<float>(MAX_DELAY_SAMPLES);

             int readInt = static_cast<int>(readPos);
             float frac = readPos - static_cast<float>(readInt);
             int readNext = (readInt + 1) % MAX_DELAY_SAMPLES;

             float delayed = delayBuffer_[channel][readInt] * (1.0f - frac) + 
                             delayBuffer_[channel][readNext] * frac;

             output += delayed * chPan * gainPerVoice;
        }

        writeIdx_[channel] = (writeIdx_[channel] + 1) % MAX_DELAY_SAMPLES;

        return output;
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            std::fill(delayBuffer_[ch].begin(), delayBuffer_[ch].end(), 0.0f);
            writeIdx_[ch] = 0;
        }
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<float> delayBuffer_[2];
    int writeIdx_[2] = {0, 0};

    struct Voice {
        float timeOffset;
        float targetTimeOffset;
        float pitchOffset;
        float targetPitchOffset;
        float pan;
        float readIdx;
    };
    Voice voices_[MAX_VOICES];

    std::random_device rd_;
    std::mt19937 gen_;
    int tickCount_ = 0;

    void updateMarkov(float fatigue) {
        if (++tickCount_ > 4410) { // Update ~every 100ms at 44.1kHz
            tickCount_ = 0;
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (dist(gen_) < (0.1f + fatigue * 0.5f)) {
                    voices_[i].targetTimeOffset = dist(gen_) * 0.05f + fatigue * 0.1f; 
                    voices_[i].targetPitchOffset = (dist(gen_) * 2.0f - 1.0f) * 0.02f * fatigue; 
                } else if (dist(gen_) < 0.3f) {
                    voices_[i].targetTimeOffset *= 0.5f;
                    voices_[i].targetPitchOffset *= 0.5f;
                }
            }
        }
    }

public:
    HumanErrorEnsembleProcessor() : gen_(rd_()) {}
};

AGENTVST_REGISTER_DSP(HumanErrorEnsembleProcessor)
