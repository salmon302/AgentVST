#pragma once

#include <cstdint>

namespace lacrimosa {

class PitchDetector;

class Lacrimosa65Modulator {
public:
    Lacrimosa65Modulator(double sampleRate = 44100.0);
    ~Lacrimosa65Modulator();

    void prepare(double sampleRate, int maxBlockSize);
    void process(float* input, float* output, int numSamples);

    void setIntensity(float v);   // 0..1
    void setSobDepth(float v);    // 0..1
    void setTrackingSpeed(float ms); // 10..500 ms

private:
    double sr_ = 44100.0;
    int maxBlock_ = 512;

    PitchDetector* detector_ = nullptr;

    float intensity_ = 0.5f;
    float sobDepth_ = 0.5f;
    float trackingSpeedMs_ = 100.0f;

    double currentFreq_ = 0.0;
    double targetFreq_ = 0.0;
    double phase_ = 0.0;
    double amPhase_ = 0.0;

    void updateTracking(double dt);
};

} // namespace lacrimosa
