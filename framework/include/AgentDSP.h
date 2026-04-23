/**
 * AgentDSP.h — AgentVST Agent-Facing DSP API
 *
 * This is the ONLY header an AI agent's DSP code needs to include.
 * It provides:
 *   - DSPContext: current plugin state (parameters, transport, sample rate)
 *   - IAgentDSP:  base class the agent inherits from
 *   - AGENTVST_REGISTER_DSP: macro to register the agent's class with the framework
 */
#pragma once

#include <string>
#include <cstddef>
#include <memory>
#include <unordered_map>

namespace AgentVST {

// Forward declaration
class ParameterCache;

// ─────────────────────────────────────────────────────────────────────────────
// DSPContext
//
// Passed to processSample() on every call.  All fields are read-only from the
// agent's perspective.  Parameter reads are lock-free (std::memory_order_relaxed).
// ─────────────────────────────────────────────────────────────────────────────
struct DSPContext {
    // ── Audio configuration ───────────────────────────────────────────────────
    double sampleRate       = 44100.0;
    int    numChannels      = 2;
    int    numSamplesInBlock= 512;
    int    currentSample    = 0;     ///< Sample index within the current block

    // ── Host transport ────────────────────────────────────────────────────────
    bool   isPlaying        = false;
    double bpm              = 120.0;
    double ppqPosition      = 0.0;   ///< Beats since transport start

    // ── Parameter access (real-time safe) ─────────────────────────────────────
    /// Returns the current value of a parameter defined in plugin.json.
    /// Reads from a cached std::atomic<float>* with memory_order_relaxed.
    /// Returns 0.0f if paramId is not found (no allocation, no throw).
    float getParameter(const std::string& paramId) const noexcept;

    // ── Internal — set by the framework, not by the agent ─────────────────────
    const ParameterCache* _paramCache = nullptr;
    const float*          _paramSnapshotValues = nullptr;
    std::size_t           _paramSnapshotCount  = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// IAgentDSP — Base class for agent-written DSP processors
// ─────────────────────────────────────────────────────────────────────────────
class IAgentDSP {
public:
    virtual ~IAgentDSP() = default;

    /**
     * Called by the framework before audio processing begins or when the
     * host sample rate / block size changes.
     *
     * PRE-ALLOCATE all memory here (vectors, history buffers, etc.).
     * After this call returns, processBlock will receive callbacks and
     * NO dynamic memory allocation is permitted.
     */
    virtual void prepare(double /*sampleRate*/, int /*maxBlockSize*/) {}

    /**
     * Process a single audio sample.
     *
     * REAL-TIME CONSTRAINTS — the following operations are PROHIBITED here:
     *   - new / delete / malloc / free
     *   - std::vector::push_back (may reallocate)
     *   - std::cout / printf / DBG (system calls)
     *   - std::mutex / std::unique_lock (blocking)
    *   - Any juce::Component / GUI state updates (message thread only)
     *   - File I/O or network access
     *
     * @param channel  0-based channel index (0 = left, 1 = right, ...)
     * @param input    The incoming audio sample
     * @param ctx      Current plugin state and parameters
     * @return         The processed output sample
     */
    virtual float processSample(int channel, float input, const DSPContext& ctx) = 0;

    /**
     * Called when the host transport rewinds or the plugin is reset.
     * Clear filter histories, envelope state, oscillator phase, etc.
     */
    virtual void reset() {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Registration system
//
// The user's plugin provides this function via the AGENTVST_REGISTER_DSP()
// macro below.  It is called once, from the AgentVSTProcessor constructor.
//
// Rationale: JUCE's juce_add_plugin() places user sources into a STATIC
// library (${target}_SharedCode.lib).  With file-scope static initializers
// in an anonymous namespace, MSVC's linker drops the object file entirely
// because nothing references it — the initializer never runs and the plugin
// silently falls back to pass-through.  Declaring a named function in the
// AgentVST namespace and calling it explicitly from the wrapper forces the
// linker to pull the user's object file into the final DLL.
// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IAgentDSP> createRegisteredDSP();

} // namespace AgentVST

// ─────────────────────────────────────────────────────────────────────────────
// AGENTVST_REGISTER_DSP(ClassName)
//
// Place this macro once in the .cpp file that defines your DSP class.
// It defines AgentVST::createRegisteredDSP() which the framework calls at
// plugin load time.
//
// Example:
//   class GainProcessor : public AgentVST::IAgentDSP { ... };
//   AGENTVST_REGISTER_DSP(GainProcessor)
// ─────────────────────────────────────────────────────────────────────────────
#define AGENTVST_REGISTER_DSP(ClassName)                                         \
    namespace AgentVST {                                                          \
        std::unique_ptr<IAgentDSP> createRegisteredDSP() {                        \
            return std::make_unique<ClassName>();                                 \
        }                                                                         \
    }
