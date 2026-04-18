import os
import json

base_dir = "examples/zone_of_the_unknown/ZoneOfTheUnknown"
os.makedirs(f"{base_dir}/src", exist_ok=True)

plugin_json = {
  "plugin": {
    "name": "ZoneOfTheUnknown",
    "version": "1.0.0",
    "vendor": "AgentVST",
    "category": "Fx"
  },
  "parameters": [
    {
      "id": "mask_removal",
      "name": "Mask Removal",
      "type": "float",
      "min": 0.0,
      "max": 100.0,
      "default": 50.0,
      "unit": "%"
    },
    {
      "id": "granular_scatter",
      "name": "Granular Scatter",
      "type": "float",
      "min": 10.0,
      "max": 500.0,
      "default": 100.0,
      "unit": "ms"
    },
    {
      "id": "horror_blend",
      "name": "Horror Blend",
      "type": "float",
      "min": 0.0,
      "max": 100.0,
      "default": 50.0,
      "unit": "%"
    }
  ]
}

with open(f"{base_dir}/plugin.json", "w") as f:
    json.dump(plugin_json, f, indent=2)

cmake = """agentvst_add_plugin(
    NAME     ZoneOfTheUnknown
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "ZotU"
    SOURCES
        src/ZoneOfTheUnknownDSP.cpp
)
target_include_directories(ZoneOfTheUnknown PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
"""

with open(f"{base_dir}/CMakeLists.txt", "w") as f:
    f.write(cmake)

dsp = """#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// A simple granular delay buffer implementation instead of full FFT for real-time simplicity,
// mimicking the "masking inversion" by gating quiet sounds into a scattering buffer
// and ducking loud sounds.
class ZoneOfTheUnknownProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        bufferSize_ = (int)sampleRate * 2; // 2 seconds
        
        for (int ch = 0; ch < 2; ++ch) {
            delayBuffer_[ch].assign(bufferSize_, 0.0f);
            writePos_[ch] = 0;
            envelope_[ch] = 0.0f;
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float maskRemoval = ctx.getParameter("mask_removal") / 100.0f;
        float scatterMs = ctx.getParameter("granular_scatter");
        float horrorBlend = ctx.getParameter("horror_blend") / 100.0f;

        // Envelope follower to detect loud vs quiet
        float attack = 0.01f;
        float release = 0.05f;
        float absIn = std::abs(input);
        
        if (absIn > envelope_[channel]) {
            envelope_[channel] += (absIn - envelope_[channel]) * attack;
        } else {
            envelope_[channel] += (absIn - envelope_[channel]) * release;
        }

        // Masking inversion logic:
        // Loud sounds are ducked, quiet sounds are pushed into the delay buffer
        float duckThreshold = 0.1f - (maskRemoval * 0.09f); // lower threshold = more ducking
        float isolatedQuiet = 0.0f;
        
        if (envelope_[channel] < duckThreshold) {
            // Apply gain to hidden sounds
            isolatedQuiet = input * (1.0f + maskRemoval * 10.0f); 
        }

        // Write isolated quiet sounds to the buffer
        delayBuffer_[channel][writePos_[channel]] = isolatedQuiet;

        // Simple granular scatter playback (randomly offset read head based on scatterMs)
        float maxScatterSamples = (scatterMs / 1000.0f) * sampleRate_;
        
        // Use a simple LFO/Noise approach to modulate read position
        scatterPhase_ += 1.0f / (sampleRate_ * 0.1f); // 10Hz variation
        if (scatterPhase_ > 1.0f) {
            scatterPhase_ -= 1.0f;
            targetOffset_ = static_cast<float>(rand()) / RAND_MAX * maxScatterSamples;
        }
        
        currentOffset_ += (targetOffset_ - currentOffset_) * 0.005f;
        
        int readPos = writePos_[channel] - static_cast<int>(currentOffset_);
        if (readPos < 0) readPos += bufferSize_;

        float granularOut = delayBuffer_[channel][readPos];

        writePos_[channel] = (writePos_[channel] + 1) % bufferSize_;

        // Blend the horrifying undercurrent with the dry signal
        // We duck the dry signal slightly when horror blend is high to "invert the mask"
        float dryMix = 1.0f - (horrorBlend * 0.5f);
        float output = (input * dryMix) + (granularOut * horrorBlend);

        return output;
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            std::fill(delayBuffer_[ch].begin(), delayBuffer_[ch].end(), 0.0f);
            writePos_[ch] = 0;
            envelope_[ch] = 0.0f;
        }
        scatterPhase_ = 0.0f;
        currentOffset_ = 0.0f;
        targetOffset_ = 0.0f;
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<float> delayBuffer_[2];
    int bufferSize_ = 0;
    int writePos_[2] = {0, 0};
    float envelope_[2] = {0.0f, 0.0f};
    
    float scatterPhase_ = 0.0f;
    float targetOffset_ = 0.0f;
    float currentOffset_ = 0.0f;
};

AGENTVST_REGISTER_DSP(ZoneOfTheUnknownProcessor)
"""

with open(f"{base_dir}/src/ZoneOfTheUnknownDSP.cpp", "w") as f:
    f.write(dsp)
