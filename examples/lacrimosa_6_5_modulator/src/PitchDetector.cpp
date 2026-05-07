#include "PitchDetector.h"
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace lacrimosa;

PitchDetector::PitchDetector(double sampleRate, int maxBufferSize)
    : sr_(sampleRate), maxBuf_(maxBufferSize), workBuf_(maxBufferSize), yinBuf_(maxBufferSize)
{
}

PitchDetector::~PitchDetector() = default;

float PitchDetector::parabolicInterpolation(const float* buf, int x)
{
    float a = buf[x - 1];
    float b = buf[x];
    float c = buf[x + 1];
    float delta = 0.5f * (a - c) / (a - 2.0f * b + c + 1e-12f);
    return float(x) + delta;
}

double PitchDetector::detectPitch(const float* buffer, int numSamples)
{
    if (numSamples <= 0) return 0.0;
    int n = std::min(numSamples, maxBuf_);
    std::memcpy(workBuf_.data(), buffer, n * sizeof(float));

    yinBuf_[0] = 0.0f;
    for (int tau = 1; tau < n; ++tau) {
        double sum = 0.0;
        for (int j = 0; j < n - tau; ++j) {
            double d = workBuf_[j] - workBuf_[j + tau];
            sum += d * d;
        }
        yinBuf_[tau] = float(sum);
    }

    yinBuf_[0] = 1.0f;
    double runningSum = 0.0;
    for (int tau = 1; tau < n; ++tau) {
        runningSum += yinBuf_[tau];
        if (runningSum == 0.0) yinBuf_[tau] = 1.0f;
        else yinBuf_[tau] = yinBuf_[tau] * float(tau) / float(runningSum);
    }

    int bestTau = -1;
    for (int tau = 1; tau < n - 1; ++tau) {
        if (yinBuf_[tau] < threshold_ && yinBuf_[tau] < yinBuf_[tau - 1] && yinBuf_[tau] <= yinBuf_[tau + 1]) {
            bestTau = tau;
            break;
        }
    }
    if (bestTau < 0) {
        float minVal = yinBuf_[1];
        bestTau = 1;
        for (int tau = 2; tau < n - 1; ++tau) {
            if (yinBuf_[tau] < minVal) { minVal = yinBuf_[tau]; bestTau = tau; }
        }
    }
    if (bestTau < 2 || bestTau >= n - 1) return 0.0;
    float fractional = parabolicInterpolation(yinBuf_, bestTau);
    double freq = sr_ / double(fractional);
    if (freq < 20.0 || freq > 20000.0) return 0.0;
    return freq;
}
