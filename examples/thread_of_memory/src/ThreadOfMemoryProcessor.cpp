#include "ThreadOfMemoryProcessor.h"
#include "PitchDetector.h"
#include "GranularBuffer.h"
#include <cstring>
#include <cmath>

using namespace thread_of_memory;

ThreadOfMemoryProcessor::ThreadOfMemoryProcessor(double sampleRate)
    : sr_(sampleRate)
{
}

ThreadOfMemoryProcessor::~ThreadOfMemoryProcessor()
{
    delete detector_;
    delete granular_;
}

void ThreadOfMemoryProcessor::prepare(double sampleRate, int maxBlockSize)
{
    sr_ = sampleRate;
    maxBlock_ = maxBlockSize;
    delete detector_;
    delete granular_;
    detector_ = new PitchDetector(sr_, maxBlock_);
    granular_ = new GranularBuffer(sr_);
    granular_->setMaxDelay(2.0); // 2s buffer
}

void ThreadOfMemoryProcessor::setMediantMode(int mode) { mediantMode_ = mode; }
void ThreadOfMemoryProcessor::setFreezeLengthMs(float ms) { freezeLengthMs_ = ms; }
void ThreadOfMemoryProcessor::setWetMix(float mix) { wetMix_ = mix; }

void ThreadOfMemoryProcessor::process(float* input, float* output, int numSamples)
{
    if (!detector_ || !granular_) {
        std::memcpy(output, input, numSamples * sizeof(float));
        return;
    }

    // detect pitch on block (mono assumption)
    double f = detector_->detectPitch(input, numSamples);
    bool chordChange = false;
    if (f > 0.0 && std::fabs(f - lastFundamental_) > 0.05 * lastFundamental_) {
        chordChange = true;
        currentChordRoot_ = f;
        chordChanged_ = true;
    }
    lastFundamental_ = f;

    // write input into granular buffer
    granular_->write(input, numSamples);

    // generate shifted version
    double shiftRatio = (mediantMode_ == 0) ? (6.0/5.0) : (5.0/4.0);
    double shiftedFreq = f * shiftRatio;
    if (f <= 0.0) shiftedFreq = 0.0;

    // on chord change, freeze common tone (simplified: freeze current buffer segment)
    if (chordChange) {
        int freezeSamples = static_cast<int>(0.001 * freezeLengthMs_ * sr_);
        granular_->triggerFreeze(freezeSamples);
    }

    // read shifted + frozen mix
    granular_->readShifted(output, numSamples, shiftedFreq, f, wetMix_);
}
