#include "ProcessBlockHandler.h"
#include "DSPRouter.h"
#include <algorithm>
#include <thread>

namespace AgentVST {

void ProcessBlockHandler::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_   = sampleRate;
    maxBlockSize_ = maxBlockSize;

    if (paramCache_)
        blockParamSnapshot_.assign(paramCache_->size(), 0.0f);

    if (agentDSP_)
        agentDSP_->prepare(sampleRate, maxBlockSize);

    watchdogTriggered_.store(false, std::memory_order_relaxed);
    noOpTriggered_.store(false, std::memory_order_relaxed);
    unchangedBlockStreak_ = 0;
}

void ProcessBlockHandler::setAgentDSP(IAgentDSP* agentDSP,
                                       const ParameterCache& paramCache) {
    agentDSP_   = agentDSP;
    paramCache_ = &paramCache;
    blockParamSnapshot_.assign(paramCache.size(), 0.0f);
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

void ProcessBlockHandler::setNoOpDetectionEnabled(bool enabled) noexcept {
    noOpDetectionEnabled_ = enabled;
    if (!enabled) {
        unchangedBlockStreak_ = 0;
        noOpTriggered_.store(false, std::memory_order_relaxed);
    }
}

void ProcessBlockHandler::setNoOpDetectionThreshold(double threshold) noexcept {
    noOpThreshold_ = std::clamp(threshold, 1.0e-12, 1.0e-2);
}

void ProcessBlockHandler::setNoOpDetectionConsecutiveBlocks(int blocks) noexcept {
    noOpConsecutiveBlocks_ = std::max(1, blocks);
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

    if (!blockParamSnapshot_.empty())
        paramCache_->copyValuesTo(blockParamSnapshot_.data(), blockParamSnapshot_.size());

    if (router_ && router_->isConfigured())
        router_->updateParameters(*paramCache_);

    auto startTime = std::chrono::high_resolution_clock::now();
    bool timedOut  = false;
    double inputEnergy = 0.0;
    double diffEnergy  = 0.0;

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
            const float inputSample = channelData[sample];
            float outputSample = inputSample;

            if (!timedOut) {
                if (router_ && router_->isConfigured()) {
                    outputSample = router_->processSample(ch, inputSample);
                } else {
                    outputSample = agentDSP_->processSample(ch, inputSample, ctx);
                }
            }

            if (!timedOut && noOpDetectionEnabled_) {
                const double in = static_cast<double>(inputSample);
                const double diff = static_cast<double>(outputSample - inputSample);
                inputEnergy += in * in;
                diffEnergy += diff * diff;
            }

            channelData[sample] = outputSample;
            // If timed out: pass input through unchanged (silence is wrong for effect plugins)
        }
    }

    bool noOpThisBlock = false;
    if (!timedOut && noOpDetectionEnabled_) {
        constexpr double kMinInputEnergy = 1.0e-12;

        if (inputEnergy > kMinInputEnergy) {
            const double relativeDiffEnergy = diffEnergy / inputEnergy;

            if (relativeDiffEnergy <= noOpThreshold_) {
                ++unchangedBlockStreak_;

                if (unchangedBlockStreak_ >= noOpConsecutiveBlocks_) {
                    noOpThisBlock = true;

                    if (unchangedBlockStreak_ == noOpConsecutiveBlocks_) {
                        noOpCount_.fetch_add(1, std::memory_order_relaxed);
                        if (onPotentialNoOp) {
                            auto cb = onPotentialNoOp;
                            const int streak = unchangedBlockStreak_;
                            std::thread([cb, relativeDiffEnergy, streak]() {
                                cb(relativeDiffEnergy, streak);
                            }).detach();
                        }
                    }
                }
            } else {
                unchangedBlockStreak_ = 0;
            }
        } else {
            unchangedBlockStreak_ = 0;
        }
    }

    noOpTriggered_.store(noOpThisBlock, std::memory_order_relaxed);
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

bool ProcessBlockHandler::hadPotentialNoOp() const noexcept {
    return noOpTriggered_.load(std::memory_order_relaxed);
}

std::uint64_t ProcessBlockHandler::potentialNoOpCount() const noexcept {
    return noOpCount_.load(std::memory_order_relaxed);
}

void ProcessBlockHandler::resetNoOpStats() noexcept {
    noOpTriggered_.store(false, std::memory_order_relaxed);
    noOpCount_.store(0, std::memory_order_relaxed);
    unchangedBlockStreak_ = 0;
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
    if (!blockParamSnapshot_.empty()) {
        ctx._paramSnapshotValues = blockParamSnapshot_.data();
        ctx._paramSnapshotCount  = blockParamSnapshot_.size();
    }
    return ctx;
}

} // namespace AgentVST
