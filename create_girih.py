import os
import json

def generate_girih():
    base_dir = "examples/girih_asymmetrical_delay/GirihAsymmetricalDelay"
    os.makedirs(os.path.join(base_dir, "src"), exist_ok=True)

    # 1. plugin.json
    plugin_json = {
      "plugin": {
        "name": "GirihAsymmetricalDelay",
        "version": "1.0.0",
        "vendor": "AgentVST",
        "category": "Delay"
      },
      "parameters": [
        {
          "id": "tessellation_root",
          "name": "Tessellation Root",
          "type": "float",
          "min": 10.0,
          "max": 2000.0,
          "default": 300.0,
          "unit": "ms"
        },
        {
          "id": "n_fold_geometry",
          "name": "N-Fold Geometry",
          "type": "float",
          "min": 5.0,
          "max": 12.0,
          "default": 5.0,
          "unit": "fold"
        },
        {
          "id": "tile_complexity",
          "name": "Tile Complexity",
          "type": "float",
          "min": 1.0,
          "max": 12.0,
          "default": 5.0,
          "unit": "taps"
        },
        {
          "id": "feedback",
          "name": "Feedback",
          "type": "float",
          "min": 0.0,
          "max": 100.0,
          "default": 40.0,
          "unit": "%"
        },
        {
          "id": "mix",
          "name": "Mix",
          "type": "float",
          "min": 0.0,
          "max": 100.0,
          "default": 50.0,
          "unit": "%"
        }
      ]
    }
    with open(os.path.join(base_dir, "plugin.json"), "w") as f:
        json.dump(plugin_json, f, indent=2)

    # 2. CMakeLists.txt
    cmake_content = """agentvst_add_plugin(
    NAME     GirihAsymmetricalDelay
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Delay"
    PLUGIN_CODE "G_Ad"
    SOURCES
        src/GirihAsymmetricalDelayDSP.cpp
)
target_include_directories(GirihAsymmetricalDelay PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
"""
    with open(os.path.join(base_dir, "CMakeLists.txt"), "w") as f:
        f.write(cmake_content)

    # 3. DSP Source
    dsp_content = """#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class GirihAsymmetricalDelayProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxDelaySamples_ = static_cast<int>(sampleRate_ * 10.0); // 10 seconds max
        for (int ch = 0; ch < 2; ++ch) {
            delayBuffer_[ch].resize(maxDelaySamples_, 0.0f);
            writePos_[ch] = 0;
            lastFeedback_[ch] = 0.0f;
        }
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float rootMs = ctx.getParameter("tessellation_root");
        float nFold = ctx.getParameter("n_fold_geometry"); // 5, 10, or 12
        int taps = static_cast<int>(ctx.getParameter("tile_complexity"));
        float feedback = ctx.getParameter("feedback") / 100.0f;
        float mix = ctx.getParameter("mix") / 100.0f;

        // Snap nFold to nearest valid mathematical geometry
        int fold = 5;
        if (nFold >= 11.0f) fold = 12;
        else if (nFold >= 7.5f) fold = 10;
        
        float rootSamples = (rootMs / 1000.0f) * sampleRate_;
        float out = 0.0f;
        
        float currentDelaySamples = rootSamples;
        float goldenRatio = 1.618033988749895f;

        for (int t = 0; t < taps; ++t) {
            // Girih tile math emulation for delay taps
            // 5-fold: increments by golden ratio
            // 10-fold: increments by inverse golden ratio
            // 12-fold: increments by sqrt(3) / 2
            
            float increment = rootSamples;
            if (fold == 5) {
                increment = rootSamples * std::pow(goldenRatio - 1.0f, (t % 3));
            } else if (fold == 10) {
                increment = rootSamples * std::pow(goldenRatio - 1.0f, (t % 4));
            } else {
                increment = rootSamples * std::pow(1.7320508f - 1.0f, (t % 2));
            }
            
            currentDelaySamples += increment;
            
            if (currentDelaySamples >= maxDelaySamples_) {
                currentDelaySamples = maxDelaySamples_ - 1;
            }

            int readPos = writePos_[channel] - static_cast<int>(currentDelaySamples);
            if (readPos < 0) readPos += maxDelaySamples_;
            
            out += delayBuffer_[channel][readPos] * std::pow(0.8f, t); // Tap attenuation
        }
        
        // Write to buffer with feedback from the *longest* tap
        float toBuffer = input + out * feedback * 0.5f; 
        
        // Soft clipping for stability
        toBuffer = std::tanh(toBuffer);

        delayBuffer_[channel][writePos_[channel]] = toBuffer;
        
        writePos_[channel]++;
        if (writePos_[channel] >= maxDelaySamples_) {
            writePos_[channel] = 0;
        }

        return input * (1.0f - mix) + out * mix;
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
    int maxDelaySamples_ = 44100 * 10;
    float lastFeedback_[2] = {0.0f, 0.0f};
};

AGENTVST_REGISTER_DSP(GirihAsymmetricalDelayProcessor)
"""
    with open(os.path.join(base_dir, "src", "GirihAsymmetricalDelayDSP.cpp"), "w") as f:
        f.write(dsp_content)

    print("Created GirihAsymmetricalDelay files!")

if __name__ == "__main__":
    generate_girih()
