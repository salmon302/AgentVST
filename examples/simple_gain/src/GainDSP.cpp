#include "AgentDSP.h"
#include <cmath>

class SimpleGainDSP : public AgentVST::IAgentDSP {
public:
    void prepare(double /*sampleRate*/, int /*maxBlockSize*/) override {}

    float processSample(int /*channel*/, float input, const AgentVST::DSPContext& ctx) override {
        float bypass = ctx.getParameter("bypass");
        if (bypass >= 0.5f)
            return input;
        float gainDb = ctx.getParameter("gain_db");
        float gainLin = std::pow(10.0f, gainDb / 20.0f);
        return input * gainLin;
    }
};

AGENTVST_REGISTER_DSP(SimpleGainDSP)
