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

const float MAX_DELAY_MS = 100.0f;
const int MAX_CASCADES = 16;

class FractionalDelay {
private:
    std::vector<float> buffer;
    int writeIdx = 0;
public:
    void init(int maxLength) {
        buffer.assign(maxLength, 0.0f);
        writeIdx = 0;
    }
    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }
    void write(float input) {
        if (buffer.empty()) return;
        buffer[writeIdx] = input;
        writeIdx = (writeIdx + 1) % buffer.size();
    }
    float read(float delaySamples) {
        if (buffer.empty()) return 0.0f;
        float readPos = static_cast<float>(writeIdx) - delaySamples;
        while (readPos < 0) readPos += buffer.size();
        while (readPos >= buffer.size()) readPos -= buffer.size();
        
        int i1 = static_cast<int>(readPos);
        int i2 = (i1 + 1) % buffer.size();
        float frac = readPos - i1;
        
        return buffer[i1] * (1.0f - frac) + buffer[i2] * frac;
    }
};

class SchroederAllPass {
private:
    FractionalDelay delay;
public:
    void init(int maxLength) {
        delay.init(maxLength);
    }
    void reset() {
        delay.reset();
    }
    float process(float input, float delaySamples, float g) {
        float w_delayed = delay.read(delaySamples);
        float w = input + g * w_delayed;
        float output = -g * input + w_delayed;
        delay.write(w);
        return output;
    }
};

class EnvelopeFollower {
private:
    float env = 0.0f;
public:
    float process(float input, float attackTime, float releaseTime, float sampleRate) {
        float attackCoeff = std::exp(-1.0f / (attackTime * sampleRate));
        float releaseCoeff = std::exp(-1.0f / (releaseTime * sampleRate));
        
        float absInput = std::abs(input);
        if (absInput > env) {
            env = attackCoeff * env + (1.0f - attackCoeff) * absInput;
        } else {
            env = releaseCoeff * env + (1.0f - releaseCoeff) * absInput;
        }
        return env;
    }
    void reset() { env = 0.0f; }
};

class DynamizationHaasCombProcessor : public AgentVST::IAgentDSP {
private:
    float sampleRate = 44100.0f;
    std::vector<std::vector<SchroederAllPass>> allPasses;
    EnvelopeFollower envFollower[2]; // Stereo envelope followers
    FractionalDelay haasDelay;
    
public:
    void prepare(double sr, int /*maxBlockSize*/) override {
        sampleRate = static_cast<float>(sr);
        int maxDelaySamples = static_cast<int>(std::ceil(MAX_DELAY_MS * 0.001f * sampleRate)) + 2;
        
        allPasses.resize(2); // stereo
        for (int ch = 0; ch < 2; ++ch) {
            allPasses[ch].resize(MAX_CASCADES);
            for (int i = 0; i < MAX_CASCADES; ++i) {
                allPasses[ch][i].init(maxDelaySamples);
            }
            envFollower[ch].reset();
        }
        haasDelay.init(maxDelaySamples);
    }

    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
        float cascadesParam = ctx.getParameter("all_pass_cascades");
        float viscosityParam = ctx.getParameter("viscosity"); // ms
        float shatterParam = ctx.getParameter("transient_shatter"); // ms
        float mixParam = ctx.getParameter("mix") * 0.01f;
        
        int numCascades = static_cast<int>(std::round(cascadesParam));
        numCascades = std::max(1, std::min(numCascades, MAX_CASCADES));
        
        float env = envFollower[channel].process(input, 0.001f, viscosityParam * 0.001f, sampleRate); // attack = 1ms
        
        float processed = input;
        
        for (int i = 0; i < numCascades; ++i) {
            float baseDelayMs = 1.1f + i * 0.7f;
            float dynDelayMs = baseDelayMs + env * baseDelayMs * 0.5f; 
            float delaySamples = std::max(1.0f, dynDelayMs * 0.001f * sampleRate);
            processed = allPasses[channel][i].process(processed, delaySamples, 0.6f);
        }
        
        if (channel == 1) { // Haas spread purely on right channel
            haasDelay.write(processed);
            float haasMs = env * shatterParam;
            float haasSamples = std::max(1.0f, haasMs * 0.001f * sampleRate);
            processed = haasDelay.read(haasSamples);
        }

        return input * (1.0f - mixParam) + processed * mixParam;
    }

    void reset() override {
        for (auto& ch : allPasses) {
            for (auto& ap : ch) {
                ap.reset();
            }
        }
        haasDelay.reset();
        envFollower[0].reset();
        envFollower[1].reset();
    }
};

AGENTVST_REGISTER_DSP(DynamizationHaasCombProcessor)
