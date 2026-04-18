#include <AgentDSP.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

class FracturedTachyonProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        fs = sampleRate;
        bufferSize = static_cast<int>(fs * 1.5); // 1.5 seconds max
        for (int ch = 0; ch < 2; ++ch) {
            freezeBuffer[ch].assign(bufferSize, 0.0f);
        }
        reset();
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float trigger = ctx.getParameter("capture_trigger");
        float entropy = ctx.getParameter("entropy_amount");
        float rot = ctx.getParameter("chronological_rot");
        float mix = ctx.getParameter("mix");

        bool isFrozen = (trigger > 0.5f);
        
        // Edge detector
        if (channel == 0) {
            if (isFrozen && !wasFrozen) {
                // Just triggered freeze
                frozenSamplesCount = 0;
            } else if (!isFrozen && wasFrozen) {
                // Just unfroze
                frozenSamplesCount = 0;
            }
            wasFrozen = isFrozen;
            
            if (isFrozen) {
                frozenSamplesCount++;
            }
        }

        if (!isFrozen) {
            // Recording
            freezeBuffer[channel][writePos] = input;
            
            // Advance write pos
            if (channel == ctx.numChannels - 1) {
                writePos = (writePos + 1) % bufferSize;
            }
            
            return input * (1.0f - mix) + input * mix; // Pass-through
        }

        // --- PLAYBACK OF FROZEN SHARDS ---
        
        if (channel == 0) {
            // Update Granular / Glitch Engine timers
            grainTimer++;
            float grainDurSamples = 50.0f * (fs / 1000.0f); // 50ms base grain
            if (grainTimer > grainDurSamples) {
                grainTimer = 0;
                
                // Jump to a new random location when grain resets
                // Entropy controls how far we jump
                float maxJump = entropy * bufferSize;
                
                // Simple deterministic PRNG based on frozen samples
                unsigned int seed = frozenSamplesCount + writePos;
                seed = (seed ^ 61) ^ (seed >> 16);
                seed = seed + (seed << 3);
                seed = seed ^ (seed >> 4);
                float randVal = static_cast<float>(seed % 10000) / 10000.0f;
                
                readPos = writePos - static_cast<int>(randVal * maxJump);
                while (readPos < 0) readPos += bufferSize;
            }
            
            // Advance read position
            readPos = (readPos + 1) % bufferSize;
        }

        // Read from buffer
        float wet = freezeBuffer[channel][readPos];
        
        // Apply "Chronological Rot"
        // Increases rot depth based on how long it has been frozen, clamped at max
        float severity = std::min(1.0f, (static_cast<float>(frozenSamplesCount) / fs) / 5.0f); // Max severity at 5 seconds
        float activeRot = rot * severity;

        // Sample rate reduction (Simulate holding a sample)
        int holdSamples = static_cast<int>(1.0f + activeRot * 20.0f); // Hold up to 20 samples
        if (channel == 0) srCounter++;
        if (srCounter >= holdSamples) {
            for (int c=0; c<2; ++c) heldSample[c] = freezeBuffer[c][readPos];
            if (channel == 0) srCounter = 0;
        }
        wet = heldSample[channel];

        // Bitcrushing
        if (activeRot > 0.05f) {
            float bits = 16.0f - activeRot * 14.0f; // Down to 2 bits
            float steps = std::pow(2.0f, bits);
            wet = std::floor(wet * steps) / steps;
        }

        return input * (1.0f - mix) + wet * mix;
    }

    void reset() override {
        writePos = 0;
        readPos = 0;
        wasFrozen = false;
        frozenSamplesCount = 0;
        grainTimer = 0;
        srCounter = 0;
        heldSample[0] = 0.0f;
        heldSample[1] = 0.0f;
        for (int ch = 0; ch < 2; ++ch) {
            std::fill(freezeBuffer[ch].begin(), freezeBuffer[ch].end(), 0.0f);
        }
    }

private:
    float fs = 44100.0f;
    std::vector<float> freezeBuffer[2];
    int bufferSize = 0;
    int writePos = 0;
    int readPos = 0;
    
    bool wasFrozen = false;
    unsigned int frozenSamplesCount = 0;
    
    int grainTimer = 0;
    int srCounter = 0;
    float heldSample[2];
};

AGENTVST_REGISTER_DSP(FracturedTachyonProcessor)
