import os
import json

def generate_tawhid():
    base_dir = "examples/tawhid_infinite_suspension/TawhidInfiniteSuspension"
    os.makedirs(os.path.join(base_dir, "src"), exist_ok=True)

    # 1. plugin.json
    plugin_json = {
      "plugin": {
        "name": "TawhidInfiniteSuspension",
        "version": "1.0.0",
        "vendor": "AgentVST",
        "category": "Fx"
      },
      "parameters": [
        {
          "id": "suspension_gravity",
          "name": "Suspension Gravity",
          "type": "float",
          "min": 0.0,
          "max": 100.0,
          "default": 50.0,
          "unit": "%"
        },
        {
          "id": "tension_selection",
          "name": "Tension Selection",
          "type": "float",
          "min": 0.0,
          "max": 1.0,
          "default": 0.5,
          "unit": ""
        },
        {
          "id": "resolution_glide",
          "name": "Resolution Glide",
          "type": "float",
          "min": 10.0,
          "max": 5000.0,
          "default": 1000.0,
          "unit": "ms"
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
    NAME     TawhidInfiniteSuspension
    SCHEMA   "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    VENDOR   "AgentVST"
    VERSION  "1.0.0"
    CATEGORY "Fx"
    PLUGIN_CODE "Tawh"
    SOURCES
        src/TawhidInfiniteSuspensionDSP.cpp
)
target_include_directories(TawhidInfiniteSuspension PRIVATE "${CMAKE_SOURCE_DIR}/framework/src")
"""
    with open(os.path.join(base_dir, "CMakeLists.txt"), "w") as f:
        f.write(cmake_content)

    # 3. DSP Source
    dsp_content = """#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <algorithm>

class TawhidInfiniteSuspensionProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        buffer_.assign(2, std::vector<float>(static_cast<size_t>(sampleRate_ * 5.0), 0.0f)); // 5s freeze buffer
        writeIdx_ = 0;
        readIdx_ = 0;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float gravity = ctx.getParameter("suspension_gravity") / 100.0f;
        float glideMs = ctx.getParameter("resolution_glide");
        float mix = ctx.getParameter("mix") / 100.0f;

        // Simplified freeze buffer/delay to represent the catching of notes
        int bSize = static_cast<int>(buffer_[channel].size());
        
        // Write to buffer
        buffer_[channel][writeIdx_] = input;
        
        // "Catch" the note by slowing down the read index advancement based on gravity (simulating holding)
        // A true implementation would decouple pitch and time using granular freeze and portamento.
        
        float freezePlayback = buffer_[channel][readIdx_];

        if (channel == 1) { // update indices once per frame (after right channel)
            writeIdx_ = (writeIdx_ + 1) % bSize;
            
            // Advance read index. If gravity is high, we occasionally freeze the read index to sustain the note
            if (static_cast<float>(rand()) / RAND_MAX > (gravity * 0.9f)) {
                readIdx_ = (readIdx_ + 1) % bSize;
            }
        }

        // Output mix
        return input * (1.0f - mix) + freezePlayback * mix;
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<std::vector<float>> buffer_;
    int writeIdx_ = 0;
    int readIdx_ = 0;
};

AGENTVST_REGISTER_DSP(TawhidInfiniteSuspensionProcessor)
"""
    with open(os.path.join(base_dir, "src", "TawhidInfiniteSuspensionDSP.cpp"), "w") as f:
        f.write(dsp_content)

    print("Created TawhidInfiniteSuspension files!")

if __name__ == "__main__":
    generate_tawhid()
