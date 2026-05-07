#include "UndertoneBank.h"
#include <cmath>
#include <algorithm>

using namespace dualism;

UndertoneBank::UndertoneBank(double sampleRate)
    : sr_(sampleRate)
{
}

UndertoneBank::~UndertoneBank() = default;

void UndertoneBank::prepare()
{
    partials_.resize(4);
    for (int i = 0; i < 4; ++i) {
        partials_[i].phase = 0.0;
        partials_[i].phaseInc = 0.0;
        partials_[i].amp = 0.0;
        partials_[i].lastOut = 0.0;
    }
}

void UndertoneBank::setFundamentalFrequency(double f)
{
    fundamental_ = f;
    if (f <= 0.0) {
        for (auto &p : partials_) p.phaseInc = 0.0;
        return;
    }

    for (int i = 0; i < 4; ++i) {
        double targetFreq = f / double(i + 2); // f/2..f/5
        partials_[i].phaseInc = 2.0 * M_PI * targetFreq / sr_;
        partials_[i].amp = 0.2 / double(i + 1);
    }
}

void UndertoneBank::setGravity(float g)
{
    gravity_ = g;
}

double UndertoneBank::polyBlep(double phase, double dt)
{
    // phase in [0,1), dt = phase increment per sample
    if (dt <= 0.0) return 0.0;
    if (phase < dt) {
        phase /= dt;
        return phase + phase - phase * phase - 1.0;
    } else if (phase > 1.0 - dt) {
        phase = (phase - 1.0) / dt;
        return phase * phase + phase + phase + 1.0;
    }
    return 0.0;
}

double UndertoneBank::mapBiasToMix(float bias, float gravity)
{
    float mix = (bias + 1.0f) * 0.5f; // 0..1
    return mix * gravity;
}

void UndertoneBank::generateUndertones(float* output, int numSamples, float bias)
{
    double mix = mapBiasToMix(bias, gravity_);
    if (mix <= 0.0001) return;

    for (int n = 0; n < numSamples; ++n) {
        double s = 0.0;
        for (auto &p : partials_) {
            if (p.phaseInc == 0.0) continue;
            // band-limited saw via polyBLEP
            double dt = p.phaseInc / (2.0 * M_PI);
            double bl = polyBlep(p.phase, dt);
            double sample = (2.0 * p.phase / (2.0 * M_PI)) - 1.0 - bl;
            s += p.amp * sample;
            p.phase += p.phaseInc;
            if (p.phase >= 2.0 * M_PI) p.phase -= 2.0 * M_PI;
        }
        output[n] += float(s * mix);
    }
}
