// Purpose: InfrasonicDreadGenerator DSP processing.
// Author: Seth Nenninger (GPT-5.2-Codex Agent)
// Timestamp: 2026-05-05T04:52:38Z
// Changelog: Replace static sine burst with evolving infrasonic bed and drift.

#include <AgentDSP.h>
#include "modules/BiquadFilter.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class InfrasonicDreadGeneratorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        for (int ch = 0; ch < 2; ++ch) {
            lowpass_[ch].prepare(sampleRate, maxBlockSize);
            lowpass_[ch].setType(AgentVST::BiquadFilter::Type::LowPass);
            lowpass_[ch].setFrequency(80.0f);
            lowpass_[ch].setQ(0.707f);
            
            envelope_[ch] = 0.0f;
        }
        envAttackCoef_ = std::exp(-1.0f / (0.005f * sampleRate_));
        envReleaseCoef_ = std::exp(-1.0f / (0.150f * sampleRate_));
        pressureAttackCoef_ = std::exp(-1.0f / (0.03f * sampleRate_));
        pressureReleaseCoef_ = std::exp(-1.0f / (2.5f * sampleRate_));
        float rumbleCutoff = 35.0f;
        rumbleCoeff_ = 1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * rumbleCutoff / static_cast<float>(sampleRate_));
        phase_ = 0.0f;
        subPhase_ = 0.0f;
        tremoloPhase_ = 0.0f;
        pressureEnv_ = 0.0f;
        rumbleState_ = 0.0f;
        drift_ = 0.0f;
        driftCounter_ = 0;
        generatedWave_ = 0.0f;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float dreadFreq = ctx.getParameter("dread_frequency");
        float sensitivityDb = ctx.getParameter("impact_sensitivity");
        float pressure = ctx.getParameter("pressure_amount") / 100.0f;

        float thresholdLinear = std::pow(10.0f, sensitivityDb / 20.0f);

        // Low-pass filter input to track low-end impacts
        float lpInput = lowpass_[channel].processSample(channel, input);

        // Envelope follower (fast attack, medium release)
        float absInput = std::abs(lpInput);
        if (absInput > envelope_[channel]) {
            envelope_[channel] = envAttackCoef_ * envelope_[channel] + (1.0f - envAttackCoef_) * absInput;
        } else {
            envelope_[channel] = envReleaseCoef_ * envelope_[channel] + (1.0f - envReleaseCoef_) * absInput;
        }

        // Only channel 0 drives the shared generator to keep phase aligned
        if (channel == 0) {
            float gateTarget = (envelope_[0] > thresholdLinear) ? 1.0f : 0.0f;
            float gateCoef = (gateTarget > pressureEnv_) ? pressureAttackCoef_ : pressureReleaseCoef_;
            pressureEnv_ = gateCoef * pressureEnv_ + (1.0f - gateCoef) * gateTarget;

            // Slow random walk for dread drift.
            if (++driftCounter_ >= kDriftUpdateInterval) {
                driftCounter_ = 0;
                drift_ = std::clamp(drift_ + nextNoise() * 0.002f, -0.05f, 0.05f);
            }

            float baseFreq = dreadFreq * (1.0f + drift_);
            baseFreq = std::clamp(baseFreq, 3.0f, 30.0f);
            float subFreq = baseFreq * 0.5f;

            phase_ += baseFreq / sampleRate_;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
            subPhase_ += subFreq / sampleRate_;
            if (subPhase_ >= 1.0f) subPhase_ -= 1.0f;

            float tremoloRate = 0.04f + 0.12f * pressure;
            tremoloPhase_ += tremoloRate / sampleRate_;
            if (tremoloPhase_ >= 1.0f) tremoloPhase_ -= 1.0f;

            float tremolo = 0.75f + 0.25f * std::sin(2.0f * M_PI * tremoloPhase_);
            float baseWave = std::sin(2.0f * M_PI * phase_);
            float subWave = std::sin(2.0f * M_PI * subPhase_);

            float noise = nextNoise();
            rumbleState_ += rumbleCoeff_ * (noise - rumbleState_);

            float bed = baseWave * 0.7f + subWave * 0.3f + rumbleState_ * 0.2f;
            float amplitude = pressure * (0.2f + 0.8f * pressureEnv_) * tremolo;
            float drive = 1.0f + pressure * 2.5f;
            float driveComp = 1.0f / (1.0f + pressure * 1.5f);
            generatedWave_ = std::tanh(bed * drive) * driveComp * amplitude;
        }

        // Output mix
        return input + generatedWave_;
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            lowpass_[ch].reset();
            envelope_[ch] = 0.0f;
        }
        phase_ = 0.0f;
        subPhase_ = 0.0f;
        tremoloPhase_ = 0.0f;
        pressureEnv_ = 0.0f;
        rumbleState_ = 0.0f;
        drift_ = 0.0f;
        driftCounter_ = 0;
        generatedWave_ = 0.0f;
    }

private:
    double sampleRate_ = 44100.0;
    AgentVST::BiquadFilter lowpass_[2];
    float envelope_[2] = {0.0f, 0.0f};
    float envAttackCoef_ = 0.0f;
    float envReleaseCoef_ = 0.0f;
    float pressureAttackCoef_ = 0.0f;
    float pressureReleaseCoef_ = 0.0f;
    float rumbleCoeff_ = 0.0f;
    
    float phase_ = 0.0f;
    float subPhase_ = 0.0f;
    float tremoloPhase_ = 0.0f;
    float pressureEnv_ = 0.0f;
    float rumbleState_ = 0.0f;
    float drift_ = 0.0f;
    int driftCounter_ = 0;
    float generatedWave_ = 0.0f;
    uint32_t rngState_ = 0x12345678u;

    static constexpr int kDriftUpdateInterval = 128;

    float nextNoise() {
        rngState_ = rngState_ * 1664525u + 1013904223u;
        float value = static_cast<float>(rngState_) * (1.0f / 4294967296.0f);
        return value * 2.0f - 1.0f;
    }
};

AGENTVST_REGISTER_DSP(InfrasonicDreadGeneratorProcessor)
