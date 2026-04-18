/**
 * Generated DSP scaffold.
 *
 * Keep processSample real-time safe:
 * - no allocations
 * - no locks
 * - no I/O
 */
#include <AgentDSP.h>
#include <vector>
#include <cmath>

class VarispeedShepardLooperProcessor : public AgentVST::IAgentDSP {
private:
    std::vector<float> buffer;
    int writePos = 0;
    
    // The three read heads
    float readAnchor = 0.0f;
    float readDecel = 0.0f;
    float readAccel = 0.0f;
    
    double mSampleRate = 48000.0;
    float ouroborosMem = 0.0f;

public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        mSampleRate = sampleRate;
        // Up to 1 second of buffer
        buffer.assign(static_cast<size_t>(sampleRate + 1), 0.0f);
        writePos = 0;
        readAnchor = 0.0f;
        readDecel = 0.0f;
        readAccel = 0.0f;
        ouroborosMem = 0.0f;
    }

    float processSample(int /*channel*/, float input,
                        const AgentVST::DSPContext& ctx) override {
        
        float captureSizeMs = ctx.getParameter("capture_size_ms");
        if (captureSizeMs < 1.0f) captureSizeMs = 1.0f;
        
        float warp = ctx.getParameter("warp_pitch_drop");
        float feedback = ctx.getParameter("ouroboros_feedback");

        int loopSize = static_cast<int>((captureSizeMs / 1000.0f) * mSampleRate);
        if (loopSize > buffer.size() - 1) {
            loopSize = static_cast<int>(buffer.size() - 1);
        }
        if (loopSize < 1) loopSize = 1;

        // Feedback the accelerating cloud back into the input
        float modulatedInput = input + (ouroborosMem * feedback * 0.5f);

        // Write
        buffer[writePos] = modulatedInput;

        // Read at three different speeds
        // 1. Static anchor
        float outAnchor = buffer[static_cast<int>(readAnchor)];
        
        // 2. Decelerating mud
        float outDecel = buffer[static_cast<int>(readDecel)];
        
        // 3. Accelerating clouds
        float outAccel = buffer[static_cast<int>(readAccel)];

        // Advance pointers based on warp
        // Anchor stays at 1x relative to the loop
        readAnchor += 1.0f;
        if (readAnchor >= loopSize) readAnchor -= loopSize;

        // Decelerating reads slower
        float speedDecel = 1.0f - (warp * 0.5f); 
        if (speedDecel < 0.1f) speedDecel = 0.1f;
        readDecel += speedDecel;
        if (readDecel >= loopSize) readDecel -= loopSize;

        // Accelerating reads faster
        float speedAccel = 1.0f + (warp * 1.5f);
        readAccel += speedAccel;
        if (readAccel >= loopSize) readAccel -= loopSize;

        // Advance write pointer in a mini loop
        writePos++;
        if (writePos >= loopSize) writePos = 0;

        // Save accelerating output for feedback next sample
        ouroborosMem = outAccel;

        // Mix the three granular paths with the dry signal
        float wet = (outAnchor + outDecel + outAccel) * 0.333f;
        return (input * 0.5f) + (wet * 0.5f);
    }

    void reset() override {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        readAnchor = 0.0f;
        readDecel = 0.0f;
        readAccel = 0.0f;
        writePos = 0;
        ouroborosMem = 0.0f;
    }
};

AGENTVST_REGISTER_DSP(VarispeedShepardLooperProcessor)
