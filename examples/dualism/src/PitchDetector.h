#pragma once

#include <vector>
#include <cmath>

namespace dualism {

class PitchDetector {
public:
    PitchDetector(double sampleRate, int maxBufferSize);
    ~PitchDetector();

    // YIN pitch detection. Returns 0.0 on failure.
    double detectPitch(const float* buffer, int numSamples);

private:
    double sr_;
    int maxBuf_;
    std::vector<float> workBuf_;
    std::vector<float> yinBuf_;
    float threshold_ = 0.15f;

    static float parabolicInterpolation(const float* buf, int x);
};

} // namespace dualism
