#include "DualismProcessor.h"
#include "PitchDetector.h"
#include "UndertoneBank.h"
#include <cstring>

using namespace dualism;

DualismProcessor::DualismProcessor(double sampleRate)
    : sampleRate_(sampleRate)
{
}

DualismProcessor::~DualismProcessor()
{
    delete detector_;
    delete bank_;
}

void DualismProcessor::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlock_ = maxBlockSize;
    delete detector_;
    delete bank_;
    detector_ = new PitchDetector(sampleRate_, maxBlock_);
    bank_ = new UndertoneBank(sampleRate_);
    bank_->prepare();
}

void DualismProcessor::process(float* input, float* output, int numSamples)
{
    if (!detector_ || !bank_) {
        if (output != input) std::memcpy(output, input, numSamples * sizeof(float));
        return;
    }

    // copy input to output
    if (output != input) std::memcpy(output, input, numSamples * sizeof(float));

    // detect pitch on block (mono assumption)
    double f = detector_->detectPitch(input, numSamples);
    bank_->setFundamentalFrequency(f);
    bank_->setGravity(gravity_);
    bank_->generateUndertones(output, numSamples, bias_);
}

void DualismProcessor::setBias(float v) { bias_ = v; }
void DualismProcessor::setGravity(float v) { gravity_ = v; }
