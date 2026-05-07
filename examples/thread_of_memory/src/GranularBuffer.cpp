#include "GranularBuffer.h"
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace thread_of_memory;

GranularBuffer::GranularBuffer(double sampleRate)
    : sr_(sampleRate), writePos_(0)
{
    setMaxDelay(2.0);
}

void GranularBuffer::setMaxDelay(double seconds)
{
    maxDelaySamples_ = static_cast<int>(seconds * sr_) + 1;
    buffer_.assign(maxDelaySamples_, 0.0f);
    writePos_ = 0;
}

void GranularBuffer::write(const float* input, int numSamples)
{
    for (int i = 0; i < numSamples; ++i) {
        buffer_[writePos_] = input[i];
        writePos_ = (writePos_ + 1) % maxDelaySamples_;
    }
}

void GranularBuffer::triggerFreeze(int freezeSamples)
{
    freezeLength_ = std::min(freezeSamples, maxDelaySamples_ - 1);
    freezeStart_ = (writePos_ - freezeLength_ + maxDelaySamples_) % maxDelaySamples_;
    frozen_ = true;
}

void GranularBuffer::readShifted(float* output, int numSamples, double shiftedFreq, double originalFreq, float wetMix)
{
    if (originalFreq <= 0.0 || shiftedFreq <= 0.0) {
        // no pitch shift, just mix dry/wet
        for (int i = 0; i < numSamples; ++i) {
            int rp = (writePos_ - 1 - i + maxDelaySamples_) % maxDelaySamples_;
            float dry = buffer_[rp];
            output[i] = dry * (1.0f - wetMix) + dry * wetMix;
        }
        return;
    }

    double ratio = shiftedFreq / originalFreq;
    double readPos = 0.0;
    int bufSize = maxDelaySamples_;

    for (int i = 0; i < numSamples; ++i) {
        int wp = (writePos_ - 1 - i + bufSize) % bufSize;
        float dry = buffer_[wp];

        // simple pitch shift via resampling from recent past
        double srcPos = fmod(readPos, double(bufSize));
        int i0 = static_cast<int>(srcPos) % bufSize;
        int i1 = (i0 + 1) % bufSize;
        float frac = static_cast<float>(srcPos - std::floor(srcPos));
        float shifted = buffer_[i0] * (1.0f - frac) + buffer_[i1] * frac;

        output[i] = dry * (1.0f - wetMix) + shifted * wetMix;
        readPos += ratio;
    }
}
