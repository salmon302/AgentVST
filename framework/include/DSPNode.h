/**
 * DSPNode.h — Base class for all AgentVST pre-built DSP modules
 *
 * Pre-built modules (BiquadFilter, Oscillator, Gain, etc.) all inherit from
 * DSPNode.  The DSPRouter uses this interface to chain modules together.
 *
 * CONTRACT: All memory MUST be allocated in prepare(). No dynamic allocation
 * is permitted in processSample().
 */
#pragma once

#include <string>
#include <stdexcept>

namespace AgentVST {

class DSPNode {
public:
    virtual ~DSPNode() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * Called before audio processing begins.
     * Allocate ALL buffers, history arrays, etc. here.
     * After this call, processSample must not allocate.
     */
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;

    /**
     * Process a single sample on the given channel.
     * Must be real-time safe (no allocations, no locks, no I/O).
     */
    virtual float processSample(int channel, float input) = 0;

    /**
     * Reset all internal state (filter history, envelope state, etc.).
     * Called on transport rewind or plugin reset.
     */
    virtual void reset() = 0;

    // ── Parameter control ─────────────────────────────────────────────────────

    /**
     * Set a module-specific parameter by name.
     * Default implementation does nothing.
     */
    virtual void setParameter(const std::string& /*name*/, float /*value*/) {}

    // ── Identity ─────────────────────────────────────────────────────────────

    const std::string& getNodeId()     const noexcept { return nodeId_; }
    void               setNodeId(const std::string& id) { nodeId_ = id; }

    const std::string& getNodeType()   const noexcept { return nodeType_; }

    // ── Error type ────────────────────────────────────────────────────────────
    class ConfigError : public std::runtime_error {
    public:
        explicit ConfigError(const std::string& msg)
            : std::runtime_error(msg) {}
    };

protected:
    std::string nodeId_;
    std::string nodeType_ = "DSPNode";
    double      sampleRate_    = 44100.0;
    int         maxBlockSize_  = 512;
};

} // namespace AgentVST
