#include "ProcessBlockHandler.h"
#include "DSPRouter.h"
#include <algorithm>
#include <thread>

namespace AgentVST {

void ProcessBlockHandler::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_   = sampleRate;
    maxBlockSize_ = maxBlockSize;

    if (agentDSP_)
        agentDSP_->prepare(sampleRate, maxBlockSize);

    watchdogTriggered_.store(false, std::memory_order_relaxed);
}

void ProcessBlockHandler::setAgentDSP(IAgentDSP* agentDSP,
                                       const ParameterCache& paramCache) {
    agentDSP_   = agentDSP;
    paramCache_ = &paramCache;
}

void ProcessBlockHandler::setDSPRouter(DSPRouter* router) {
    router_ = router;
}

void ProcessBlockHandler::setWatchdogBudget(double budgetFraction) noexcept {
    budgetFraction_ = std::clamp(budgetFraction, 0.01, 0.95);
}

void ProcessBlockHandler::setWatchdogCheckInterval(int checkInterval) noexcept {
    checkInterval_ = std::max(1, checkInterval);
}

void ProcessBlockHandler::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&         /*midi*/,
    const juce::AudioPlayHead::CurrentPositionInfo& posInfo)
{
    if (agentDSP_ == nullptr || paramCache_ == nullptr) {
        buffer.clear();
        return;
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    const double bufferDurationMs = (static_cast<double>(numSamples) / sampleRate_) * 1000.0;
    const double budgetMs         = bufferDurationMs * budgetFraction_;

    watchdogTriggered_.store(false, std::memory_order_relaxed);

    if (router_ && router_->isConfigured())
        router_->updateParameters(*paramCache_);

    auto startTime = std::chrono::high_resolution_clock::now();
    bool timedOut  = false;

    for (int sample = 0; sample < numSamples; ++sample) {
        // Watchdog check (every checkInterval_ samples)
        if (!timedOut && (sample & (checkInterval_ - 1)) == 0 && sample > 0) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(now - startTime).count();
            if (elapsedMs > budgetMs) {
                timedOut = true;
                watchdogTriggered_.store(true, std::memory_order_relaxed);
                violationCount_.fetch_add(1, std::memory_order_relaxed);

                // Fire violation callback asynchronously (never on audio thread)
                if (onWatchdogViolation) {
                    auto cb = onWatchdogViolation;
                    std::thread([cb, elapsedMs, budgetMs]() {
                        cb(elapsedMs, budgetMs);
                    }).detach();
                }
            }
        }

        DSPContext ctx = buildContext(sample, numChannels, numSamples, posInfo);

        for (int ch = 0; ch < numChannels; ++ch) {
            float* channelData = buffer.getWritePointer(ch);

            if (!timedOut) {
                if (router_ && router_->isConfigured()) {
                    channelData[sample] = router_->processSample(ch, channelData[sample]);
                } else {
                    channelData[sample] = agentDSP_->processSample(ch,
                                                                    channelData[sample],
                                                                    ctx);
                }
            }
            // If timed out: pass input through unchanged (silence is wrong for effect plugins)
        }
    }
}

bool ProcessBlockHandler::hadWatchdogViolation() const noexcept {
    return watchdogTriggered_.load(std::memory_order_relaxed);
}

std::uint64_t ProcessBlockHandler::watchdogViolationCount() const noexcept {
    return violationCount_.load(std::memory_order_relaxed);
}

void ProcessBlockHandler::resetWatchdogStats() noexcept {
    watchdogTriggered_.store(false, std::memory_order_relaxed);
    violationCount_.store(0, std::memory_order_relaxed);
}

DSPContext ProcessBlockHandler::buildContext(
    int sampleIndex, int numChannels, int numSamples,
    const juce::AudioPlayHead::CurrentPositionInfo& pos) const noexcept
{
    DSPContext ctx;
    ctx.sampleRate        = sampleRate_;
    ctx.numChannels       = numChannels;
    ctx.numSamplesInBlock = numSamples;
    ctx.currentSample     = sampleIndex;
    ctx.isPlaying         = pos.isPlaying;
    ctx.bpm               = pos.bpm;
    ctx.ppqPosition       = pos.ppqPosition;
    ctx._paramCache       = paramCache_;
    return ctx;
}

} // namespace AgentVST
