#pragma once

#include <vector>
#include <cstdint>

namespace thread_of_memory {

class PitchDetector;
class GranularBuffer;

class ThreadOfMemoryProcessor {
public:
    ThreadOfMemoryProcessor(double sampleRate = 44100.0);
    ~ThreadOfMemoryProcessor();

    void prepare(double sampleRate, int maxBlockSize);
    void process(float* input, float* output, int numSamples);

    void setMediantMode(int mode); // 0=6:5 minor, 1=5:4 major
    void setFreezeLengthMs(float ms);
    void setWetMix(float mix); // 0..1

private:
    double sr_ = 44100.0;
    int maxBlock_ = 512;

    PitchDetector* detector_ = nullptr;
    GranularBuffer* granular_ = nullptr;

    int mediantMode_ = 0;
    float freezeLengthMs_ = 500.0f;
    float wetMix_ = 0.5f;

    double lastFundamental_ = 0.0;
    double currentChordRoot_ = 0.0;
    bool chordChanged_ = false;
};

} // namespace thread_of_memory
