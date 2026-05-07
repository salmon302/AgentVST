#pragma once

#include <vector>

namespace thread_of_memory {

class GranularBuffer {
public:
    explicit GranularBuffer(double sampleRate);
    ~GranularBuffer() = default;

    void setMaxDelay(double seconds);
    void write(const float* input, int numSamples);
    void triggerFreeze(int freezeSamples);
    void readShifted(float* output, int numSamples, double shiftedFreq, double originalFreq, float wetMix);

private:
    double sr_ = 44100.0;
    std::vector<float> buffer_;
    int writePos_ = 0;
    int maxDelaySamples_ = 0;
    int freezeStart_ = 0;
    int freezeLength_ = 0;
    bool frozen_ = false;
};

} // namespace thread_of_memory
