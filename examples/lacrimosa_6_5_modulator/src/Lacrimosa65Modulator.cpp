#include "Lacrimosa65Modulator.h"
#include "PitchDetector.h"
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace lacrimosa;

Lacrimosa65Modulator::Lacrimosa65Modulator(double sampleRate)
    : sr_(sampleRate)
{
}

Lacrimosa65Modulator::~Lacrimosa65Modulator()
{
    delete detector_;
}

void Lacrimosa65Modulator::prepare(double sampleRate, int maxBlockSize)
{
    sr_ = sampleRate;
    maxBlock_ = maxBlockSize;
    delete detector_;
    detector_ = new PitchDetector(sr_, maxBlock_);
    phase_ = 0.0;
    amPhase_ = 0.0;
    currentFreq_ = 0.0;
    targetFreq_ = 0.0;
}

void Lacrimosa65Modulator::setIntensity(float v) { intensity_ = v; }
void Lacrimosa65Modulator::setSobDepth(float v) { sobDepth_ = v; }
void Lacrimosa65Modulator::setTrackingSpeed(float ms) { trackingSpeedMs_ = ms; }

void Lacrimosa65Modulator::updateTracking(double dt)
{
    if (targetFreq_ <= 0.0) {
        currentFreq_ = 0.0;
        return;
    }
    double alpha = (dt > 0.0) ? 1.0 - std::exp(-dt / (0.001 * trackingSpeedMs_)) : 1.0;
    currentFreq_ = currentFreq_ + alpha * (targetFreq_ - currentFreq_);
}

void Lacrimosa65Modulator::process(float* input, float* output, int numSamples)
{
    if (!detector_) {
        std::memcpy(output, input, numSamples * sizeof(float));
        return;
    }

    double dt = 1.0 / sr_;
    double ratio65 = 6.0 / 5.0;

    // detect pitch on block
    double f = detector_->detectPitch(input, numSamples);
    targetFreq_ = (f > 0.0) ? f : 0.0;
    updateTracking(dt * numSamples);

    float intensity = intensity_;
    float sobDepth = sobDepth_;

    for (int i = 0; i < numSamples; ++i) {
        float dry = input[i];
        float wet = 0.0f;

        if (currentFreq_ > 20.0) {
            // micro-pitch shift to 6:5 for the third (approx)
            double shiftedFreq = currentFreq_ * ratio65;
            // AM at beat frequency between tempered minor third and just 6:5
            // equal-tempered minor third ratio = 2^(3/12) ≈ 1.1892
            // just 6:5 = 1.2
            // beat freq = |shiftedFreq * (1.2 - 1.1892)| approx
            double temperedFreq = currentFreq_ * 1.189207;
            double beatFreq = std::fabs(shiftedFreq - temperedFreq);
            // clamp beat freq to audible range for AM effect
            if (beatFreq < 1.0) beatFreq = 1.0;
            if (beatFreq > 20.0) beatFreq = 20.0;

            // update phases
            phase_ += 2.0 * M_PI * shiftedFreq / sr_;
            amPhase_ += 2.0 * M_PI * beatFreq / sr_;
            if (phase_ > 2.0 * M_PI) phase_ -= 2.0 * M_PI;
            if (amPhase_ > 2.0 * M_PI) amPhase_ -= 2.0 * M_PI;

            // AM tremolo at beat frequency
            double am = 1.0 - sobDepth * 0.5 * (1.0 - std::cos(amPhase_));
            wet = float(std::sin(phase_) * am);
        }

        output[i] = dry * (1.0f - intensity) + wet * intensity;
    }
}
