/**
 * Generated DSP scaffold.
 *
 * Keep processSample real-time safe:
 * - no allocations
 * - no locks
 * - no I/O
 */
#include <AgentDSP.h>

class SchemaExampleToneShaperProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double /*sampleRate*/, int /*maxBlockSize*/) override {}

    float processSample(int /*channel*/, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f)
            return input;

        // TODO: Replace pass-through with audible DSP logic.
        // AgentVST logs a potential no-op warning if output remains
        // effectively identical to input for many consecutive blocks.
        return input;
    }

    void reset() override {}
};

AGENTVST_REGISTER_DSP(SchemaExampleToneShaperProcessor)
