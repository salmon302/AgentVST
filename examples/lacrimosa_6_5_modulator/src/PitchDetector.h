#pragma once

#include <vector>

namespace lacrimosa {

class PitchDetector {
public:
    PitchDetector(double sampleRate, int maxBufferSize);
    ~PitchDetector();
    double detectPitch(const float* buffer, int numSamples);
private:
    double sr_;
    int maxBuf_;
    std::vector<float> workBuf_;
    std::vector<float> yinBuf_;
    float threshold_ = 0.15f;
    static float parabolicInterpolation(const float* buf, int x);
};

} // namespace lacrimosa
