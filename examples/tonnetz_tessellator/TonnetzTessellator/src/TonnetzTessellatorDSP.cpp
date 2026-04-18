#include <AgentDSP.h>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TonnetzTessellatorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) override {
        sampleRate_ = sampleRate;
        for(int i=0; i<6; ++i) phase_[i] = 0.0f;
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (channel >= 2) return input;

        float anchor = ctx.getParameter("geometric_anchor") / 100.0f;
        float transX = ctx.getParameter("transformation_x");
        float transY = ctx.getParameter("transformation_y");
        float spread = ctx.getParameter("tessellation_spread") / 100.0f;
        float mix = ctx.getParameter("mix") / 100.0f;

        // Abstract representation of Tonnetz shifts
        // We use X and Y to define base frequency multipliers to synthesize a chord layer
        float baseFreq = 220.0f * std::pow(2.0f, transX * 2.0f); 
        float freq1 = baseFreq * 1.25f; // Major third approximation
        float freq2 = baseFreq * 1.5f;  // Perfect fifth approximation
        
        // Modulate slightly by Y axis
        freq1 *= std::pow(1.05946f, transY * 4.0f);
        freq2 *= std::pow(1.05946f, -transY * 4.0f);

        int phaseIdx1 = channel * 3;
        int phaseIdx2 = channel * 3 + 1;

        float synth1 = std::sin(phase_[phaseIdx1] * 2.0f * static_cast<float>(M_PI));
        float synth2 = std::sin(phase_[phaseIdx2] * 2.0f * static_cast<float>(M_PI));

        phase_[phaseIdx1] += freq1 / sampleRate_;
        if (phase_[phaseIdx1] > 1.0f) phase_[phaseIdx1] -= 1.0f;
        
        phase_[phaseIdx2] += freq2 / sampleRate_;
        if (phase_[phaseIdx2] > 1.0f) phase_[phaseIdx2] -= 1.0f;

        // We use an envelope of the input to gate the synthesized harmonies
        // For a more complete implementation this would be YIN pitch tracking + Granular shift
        float absInput = std::abs(input);
        float harmony = (synth1 + synth2) * 0.5f * absInput * spread * anchor;
        
        float out = input + harmony;

        return input * (1.0f - mix) + out * mix;
    }

private:
    double sampleRate_ = 44100.0;
    float phase_[6]; // stereo, multiple oscillators
};

AGENTVST_REGISTER_DSP(TonnetzTessellatorProcessor)
