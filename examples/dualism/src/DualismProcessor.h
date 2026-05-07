#pragma once

#include <vector>
#include <cstdint>

namespace dualism {

class PitchDetector;
class UndertoneBank;

class DualismProcessor {
public:
    DualismProcessor(double sampleRate = 44100.0);
    ~DualismProcessor();

    void prepare(double sampleRate, int maxBlockSize);
    void process(float* input, float* output, int numSamples);

    // Parameters
    void setBias(float v); // -1..1
    void setGravity(float v); // 0..1

private:
    double sampleRate_ = 44100.0;
    int maxBlock_ = 512;

    PitchDetector* detector_ = nullptr;
    UndertoneBank* bank_ = nullptr;

    float bias_ = 0.0f;
    float gravity_ = 0.6f;
};

} // namespace dualism
