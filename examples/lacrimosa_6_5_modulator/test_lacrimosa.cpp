#include "src/Lacrimosa65Modulator.h"
#include <iostream>
#include <vector>
#include <cmath>

int main() {
    const double sr = 44100.0;
    const int blockSize = 512;
    lacrimosa::Lacrimosa65Modulator proc(sr);
    proc.prepare(sr, blockSize);

    proc.setIntensity(0.7f);
    proc.setSobDepth(0.6f);
    proc.setTrackingSpeed(100.0f);

    std::vector<float> input(blockSize);
    std::vector<float> output(blockSize);
    double phase = 0.0;
    double phaseInc = 2.0 * M_PI * 220.0 / sr;

    std::cout << "Testing Lacrimosa65Modulator with 220 Hz sine...\n";
    for (int i = 0; i < sr; i += blockSize) {
        for (int n = 0; n < blockSize; ++n) {
            input[n] = 0.3f * std::sin(float(phase));
            phase += phaseInc;
            if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
        }
        proc.process(input.data(), output.data(), blockSize);
    }

    float inRms = 0.0f, outRms = 0.0f;
    for (int n = 0; n < blockSize; ++n) {
        inRms += input[n] * input[n];
        outRms += output[n] * output[n];
    }
    inRms = std::sqrt(inRms / blockSize);
    outRms = std::sqrt(outRms / blockSize);

    std::cout << "Input RMS: " << inRms << "\n";
    std::cout << "Output RMS: " << outRms << "\n";
    if (outRms > 0.01f) {
        std::cout << "PASS: Output generated.\n";
        return 0;
    } else {
        std::cout << "WARNING: Output near zero.\n";
        return 1;
    }
}
