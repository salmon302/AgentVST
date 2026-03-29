/**
 * ProcessBlockHandler.h
 *
 * Bridges JUCE's block-based processBlock() callback to the agent's
 * per-sample processSample() function.
 *
 * Responsibilities:
 *   1. Iterate over every channel × sample in the host buffer.
 *   2. Build a DSPContext for each sample.
 *   3. Invoke the agent's IAgentDSP::processSample().
 *   4. Run a cooperative watchdog: if execution time exceeds the configured
 *      budget fraction, bypass the agent's DSP and log the violation.
 */
#pragma once

#include "AgentDSP.h"
#include "ParameterCache.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <chrono>
#include <atomic>
#include <functional>

namespace AgentVST {

class ProcessBlockHandler {
public:
    ProcessBlockHandler() = default;

    // ── Configuration ─────────────────────────────────────────────────────────

    /**
     * Called from AudioProcessor::prepareToPlay().
     * @param sampleRate   Current host sample rate
     * @param maxBlockSize Maximum buffer size the host will send
     */
    void prepare(double sampleRate, int maxBlockSize);

    /**
     * @param agentDSP      Pointer to the agent's IAgentDSP instance (owned externally)
     * @param paramCache    Reference to the initialized ParameterCache
     */
    void setAgentDSP(IAgentDSP* agentDSP, const ParameterCache& paramCache);

    /**
     * @param budgetFraction  Fraction of buffer duration to allow for DSP.
     *                        Default: 0.10 (10%). Range: 0.01–0.95.
     */
    void setWatchdogBudget(double budgetFraction) noexcept;

    /**
     * @param checkInterval  Number of samples between watchdog clock checks.
     *                       Smaller = more precise, slightly higher overhead.
     *                       Default: 32.
     */
    void setWatchdogCheckInterval(int checkInterval) noexcept;

    // ── Processing ────────────────────────────────────────────────────────────

    /**
     * Called from AudioProcessor::processBlock().
     * Iterates over the buffer, calls the agent's processSample(),
     * and applies the watchdog.
     */
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer&         midi,
                      const juce::AudioPlayHead::CurrentPositionInfo& posInfo);

    // ── Diagnostics ───────────────────────────────────────────────────────────

    /// True if the watchdog triggered during the last processBlock() call.
    bool hadWatchdogViolation() const noexcept;

    /// Number of watchdog violations since last reset.
    std::uint64_t watchdogViolationCount() const noexcept;

    void resetWatchdogStats() noexcept;

    /// Callback invoked asynchronously when a watchdog violation is detected.
    /// Safe to set from any thread; called from a background thread.
    std::function<void(double elapsedMs, double budgetMs)> onWatchdogViolation;

private:
    IAgentDSP*           agentDSP_    = nullptr;
    const ParameterCache* paramCache_ = nullptr;

    double sampleRate_      = 44100.0;
    int    maxBlockSize_    = 512;
    double budgetFraction_  = 0.10;
    int    checkInterval_   = 32;

    std::atomic<bool>          watchdogTriggered_{ false };
    std::atomic<std::uint64_t> violationCount_  { 0 };

    DSPContext buildContext(int sampleIndex, int numChannels, int numSamples,
                            const juce::AudioPlayHead::CurrentPositionInfo& pos) const noexcept;
};

} // namespace AgentVST
