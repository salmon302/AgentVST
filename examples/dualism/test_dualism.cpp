#include "src/DualismProcessor.h"
#include <iostream>
#include <vector>
#include <cmath>

int main() {
    const double sr = 44100.0;
    const int blockSize = 512;
    dualism::DualismProcessor proc(sr);
    proc.prepare(sr, blockSize);

    // generate a 220 Hz sine for 1 second
    std::vector<float> input(blockSize);
    std::vector<float> output(blockSize);
    double phase = 0.0;
    double phaseInc = 2.0 * M_PI * 220.0 / sr;

    proc.setBias(0.8f);  // favor undertones
    proc.setGravity(0.7f);

    std::cout << "Testing Dualism processor with 220 Hz sine...\n";
    for (int i = 0; i < sr; i += blockSize) {
        for (int n = 0; n < blockSize; ++n) {
            input[n] = 0.3f * std::sin(float(phase));
            phase += phaseInc;
            if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
        }
        proc.process(input.data(), output.data(), blockSize);
    }

    // simple check: output should differ from input (undertones added)
    float inRms = 0.0f, outRms = 0.0f;
    for (int n = 0; n < blockSize; ++n) {
        inRms += input[n] * input[n];
        outRms += output[n] * output[n];
    }
    inRms = std::sqrt(inRms / blockSize);
    outRms = std::sqrt(outRms / blockSize);

    std::cout << "Input RMS: " << inRms << "\n";
    std::cout << "Output RMS: " << outRms << "\n";
    if (outRms > inRms * 1.05f) {
        std::cout << "PASS: Undertones appear to be added.\n";
        return 0;
    } else {
        std::cout << "WARNING: Output not significantly different; check DSP.\n";
        return 1;
    }
}
