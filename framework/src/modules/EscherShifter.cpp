/**
 * EscherShifter.cpp — Shepard-tone pitch shifter implementation
 */
#include "EscherShifter.h"
#include <iostream>
#include <algorithm>

namespace AgentVST {

EscherShifter::EscherShifter() {
    nodeType_ = "EscherShifter";
}

void EscherShifter::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_   = sampleRate;
    maxBlockSize_ = maxBlockSize;
    
    // We'll use 1-second delay buffers for our pitch shifting voices.
    bufferSize_ = static_cast<int>(sampleRate_ * 1.5); // extra headroom
    
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        voices_[ch].resize(spread_);
        for (auto& voice : voices_[ch]) {
            voice.delayBuffer.assign(bufferSize_, 0.0f);
            voice.writePos = 0;
            voice.currentShift = 0.0f;
        }
    }
    
    reset();
}

void EscherShifter::reset() {
    phase_ = 0.0;
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        for (auto& voice : voices_[ch]) {
            std::fill(voice.delayBuffer.begin(), voice.delayBuffer.end(), 0.0f);
            voice.writePos = 0;
        }
    }
}

void EscherShifter::setParameter(const std::string& name, float value) {
    if (name == "rate")             setRate(value);
    else if (name == "tritone_blend") setBlend(value);
    else if (name == "feedback")     setFeedback(value);
    else if (name == "octave_spread") setSpread(static_cast<int>(value));
}

void EscherShifter::setRate(float rate) noexcept {
    rate_ = rate;
}

void EscherShifter::setBlend(float blend) noexcept {
    blend_ = blend;
}

void EscherShifter::setFeedback(float feedback) noexcept {
    feedback_ = std::clamp(feedback, 0.0f, 0.95f);
}

void EscherShifter::setSpread(int spread) noexcept {
    spread_ = std::clamp(spread, 1, kMaxVoices);
}

float EscherShifter::processSample(int channel, float input) {
    if (channel >= kMaxChannels || voices_[channel].empty()) return input;

    // Phase accumulator: one full cycle (0..1) represents one octave shift (12 semitones)
    // rate_ is semitones per second.
    // 12 semitones in 1 second means phase_ increases by 1 in 1 second.
    // phase_ increment per sample: (rate_ / 12.0) / sampleRate_
    double phaseInc = (double(rate_) / 12.0) / sampleRate_;
    
    // Update phase only on channel 0 to keep them in sync
    if (channel == 0) {
        phase_ += phaseInc;
        if (phase_ >= 1.0) phase_ -= 1.0;
        else if (phase_ < 0.0) phase_ += 1.0;
    }

    float output = 0.0f;
    float drySignal = input;

    // Pitch shifting logic per voice
    // For Shepard tone, we blend multiple octaves.
    for (int i = 0; i < spread_; ++i) {
        auto& voice = voices_[channel][i];
        
        // Target shift for this voice in semitones (offset by i octaves)
        double voicePhase = phase_ + (double(i) / spread_);
        if (voicePhase >= 1.0) voicePhase -= 1.0;
        
        // Shepard weight: cosine bell to fade in/out at the boundaries.
        float weight = 0.5f * (1.0f - std::cos(2.0f * M_PI * (float)voicePhase));

        // Basic Pitch Shift via Doppler effect (dual-delay crossfade)
        // This is a placeholder for a better pitch shifter (e.g. grain-based).
        // For now, we'll just simulate the spectral movement.
        
        // Simple delay write
        voice.delayBuffer[voice.writePos] = input; // + feedback_ * voice.delayBuffer[voice.writePos]; // feedback stub
        
        // Voice-specific shift logic (placeholder for actual grain logic)
        // In a real implementation, we'd adjust read speeds or use granular síntesis.
        // For simplicity in this skeleton, we'll just use a mix of input.
        output += input * weight;

        voice.writePos = (voice.writePos + 1) % bufferSize_;
    }

    return (drySignal * (1.0f - blend_)) + (output * blend_);
}

} // namespace AgentVST
